// license:BSD-3-Clause
// copyright-holders:Miodrag Milanovic
/******************************************************************************

    Iskra Delta produced:
        1984 - Partner series
            model with Winchester disk + floppy
            model with 2 floppy disks
        1987 - Partner G series
            1F/G - model with 1 floppy disk
            2F/G - model with 2 floppy disks
            WF/G - model with Winchester disk + floppy

    Schematics and info at :
        https://github.com/tstih/idp-doc/tree/main/iskra-delta
        http://matejhorvat.si/sl/slorac/delta/partner/index.htm

*******************************************************************************/

#include "emu.h"

#include "cpu/z80/z80.h"
#include "machine/mc14411.h"
#include "machine/mm58167.h"
#include "machine/upd765.h"
#include "machine/z80ctc.h"
#include "machine/z80dma.h"
#include "machine/z80daisy.h"
#include "machine/z80daisy_generic.h"
#include "machine/z80pio.h"
#include "machine/z80sio.h"
#include "bus/idpartner/bus.h"
#include "bus/rs232/rs232.h"

#include "imagedev/floppy.h"

namespace {

class idpartner_state : public driver_device
{
public:
	idpartner_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag)
		, m_maincpu(*this, "maincpu")
		, m_bus(*this, "j2")
		, m_conn(*this, "j2:%d", 0U)
		, m_sio1(*this, "sio1")
		, m_sio2(*this, "sio2")
		, m_ctc(*this, "ctc")
		, m_fdc(*this, "fdc")
		, m_fdc_daisy(*this, "fdc_daisy")
		, m_brg(*this, "brg")
		, m_serial(*this, "serial_%d",0U)
		, m_rom(*this, "maincpu")
		, m_bankr0(*this, "bankr0")
		, m_bankw0(*this, "bankw0")
		, m_bank1(*this, "bank1")
		, m_bank2(*this, "bank2")
	{ }

	void partner_base(machine_config &config);
	void idpartner(machine_config &config);
	void partner1fg(machine_config &config);
	void partnerwfg(machine_config &config);

	void machine_start() override;
	void machine_reset() override;

private:
	void io_map(address_map &map);
	void mem_map(address_map &map);

	void update_bank();

	u8 rom_bank_r() { m_rom_enabled = false; update_bank(); return 0xff; }
	void rom_bank_w(u8 data) { m_rom_enabled = false; update_bank(); }
	u8 bank1_r() { m_bank = 0; update_bank(); return 0xff; }
	void bank1_w(u8 data) { m_bank = 0; update_bank(); }
	u8 bank2_r() { m_bank = 1; update_bank(); return 0xff; }
	void bank2_w(u8 data) { m_bank = 1; update_bank(); }
	void fdc_vector_w(u8 data) { m_fdc_daisy->set_vector(data); }
	void fdc_int_w(int state);
	void write_f1_clock(int state);

	int m_bank;
	bool m_rom_enabled;

	required_device<z80_device> m_maincpu;
	required_device<bus::idpartner::bus_device> m_bus;
	required_device_array<bus::idpartner::bus_connector_device, 2> m_conn;
	required_device<z80sio_device> m_sio1;
	required_device<z80sio_device> m_sio2;
	required_device<z80ctc_device> m_ctc;
	required_device<i8272a_device> m_fdc;
	required_device<z80daisy_generic_device> m_fdc_daisy;
	required_device<mc14411_device> m_brg;
	required_device_array<rs232_port_device,4> m_serial;
	std::unique_ptr<u8[]> m_ram;
	required_region_ptr<u8> m_rom;
	required_memory_bank m_bankr0;
	required_memory_bank m_bankw0;
	required_memory_bank m_bank1;
	required_memory_bank m_bank2;
};

void idpartner_state::fdc_int_w(int state)
{
	m_fdc_daisy->int_w(state);
}

void idpartner_state::machine_start()
{
	// 16 KB actually unused
	m_ram = std::make_unique<u8[]>(0x20000);
	std::fill_n(m_ram.get(), 0x20000, 0xff);
	save_pointer(NAME(m_ram), 0x20000);

	m_bankr0->configure_entry(0, m_ram.get() + 0x00000);
	m_bankr0->configure_entry(1, m_ram.get() + 0x10000);
	m_bankr0->configure_entry(2, m_rom);
	m_bankw0->configure_entry(0, m_ram.get() + 0x00000);
	m_bankw0->configure_entry(1, m_ram.get() + 0x10000);
	m_bankw0->configure_entry(2, m_rom);

	m_bank1->configure_entry(0, m_ram.get() + 0x01000);
	m_bank1->configure_entry(1, m_ram.get() + 0x11000);

	// Last 16KB is always same
	m_bank2->configure_entry(0, m_ram.get() + 0x0c000);
	m_bank2->set_entry(0);
}

void idpartner_state::update_bank()
{
	if (m_rom_enabled)
		m_bankr0->set_entry(2);
	else
	{
		m_bankr0->set_entry(m_bank);
		m_bankw0->set_entry(m_bank);
	}
	m_bank1->set_entry(m_bank);
}

void idpartner_state::machine_reset()
{
	m_bank = 0;
	m_rom_enabled = true;
	update_bank();
}

/* Address maps */
void idpartner_state::mem_map(address_map &map)
{
	map(0x0000, 0x0fff).bankr("bankr0").bankw("bankw0");
	map(0x1000, 0xbfff).bankrw("bank1");
	map(0xc000, 0xffff).bankrw("bank2");
}
void idpartner_state::io_map(address_map &map)
{
	map.global_mask(0xff);
	map.unmap_value_high();
	map(0x80,0x87).rw(FUNC(idpartner_state::rom_bank_r), FUNC(idpartner_state::rom_bank_w)); // ROM bank
	map(0x88,0x8f).rw(FUNC(idpartner_state::bank1_r), FUNC(idpartner_state::bank1_w)); // RAM bank 1
	map(0x90,0x97).rw(FUNC(idpartner_state::bank2_r), FUNC(idpartner_state::bank2_w)); // RAM bank 2
	//map(0x98,0x9f) // floppy motors
	//map(0xa0,0xa7) //
	//map(0xa8,0xaf) //
	//map(0xb0,0xb7) // RTC - mm58167
	//map(0xb8,0xbf) //
	//map(0xc0,0xc7) // DMA
	map(0xc8,0xcb).mirror(0x04).rw("ctc", FUNC(z80ctc_device::read), FUNC(z80ctc_device::write));  // CTC - A2 not connected
	//map(0xd0,0xd7) // PIO - A2 not connected
	map(0xd8,0xdb).mirror(0x04).rw("sio1", FUNC(z80sio_device::ba_cd_r), FUNC(z80sio_device::ba_cd_w)); // SIO1 - A2 not connected
	map(0xe0,0xe3).mirror(0x04).rw("sio2", FUNC(z80sio_device::ba_cd_r), FUNC(z80sio_device::ba_cd_w)); // SIO2 - A2 not connected
	map(0xe8,0xef).w(FUNC(idpartner_state::fdc_vector_w)); // FDC interrupt vector
	map(0xf0,0xf1).mirror(0x06).m(m_fdc, FUNC(i8272a_device::map));
	//map(0xf8,0xff) //
}

/* Input ports */
static INPUT_PORTS_START( idpartner )
INPUT_PORTS_END

static const z80_daisy_config daisy_chain[] =
{
	{ "ctc" },
	{ "sio1" },
	{ "sio2" },
	{ "fdc_daisy" },
	{ nullptr }
};

static void partner_floppies(device_slot_interface &device)
{
	device.option_add("fdd", FLOPPY_525_DD);
}

void idpartner_state::write_f1_clock(int state)
{
	m_sio1->txca_w(state);
	m_sio1->rxca_w(state);

	m_sio1->txcb_w(state);
	m_sio1->rxcb_w(state);

	m_sio2->txca_w(state);
	m_sio2->rxca_w(state);

	m_sio2->txcb_w(state);
	m_sio2->rxcb_w(state);
}

static DEVICE_INPUT_DEFAULTS_START( terminal )
	DEVICE_INPUT_DEFAULTS( "RS232_RXBAUD", 0xff, RS232_BAUD_9600 )
	DEVICE_INPUT_DEFAULTS( "RS232_TXBAUD", 0xff, RS232_BAUD_9600 )
	DEVICE_INPUT_DEFAULTS( "RS232_DATABITS", 0xff, RS232_DATABITS_8 )
	DEVICE_INPUT_DEFAULTS( "RS232_PARITY", 0xff, RS232_PARITY_NONE )
	DEVICE_INPUT_DEFAULTS( "RS232_STOPBITS", 0xff, RS232_STOPBITS_1 )
DEVICE_INPUT_DEFAULTS_END

/* Machine driver */
void idpartner_state::partner_base(machine_config &config)
{
	/* basic machine hardware */
	Z80(config, m_maincpu, XTAL(8'000'000) / 2);
	m_maincpu->set_daisy_config(daisy_chain);
	m_maincpu->set_addrmap(AS_PROGRAM, &idpartner_state::mem_map);
	m_maincpu->set_addrmap(AS_IO, &idpartner_state::io_map);

	Z80SIO(config, m_sio1, XTAL(8'000'000) / 2);
	m_sio1->out_txda_callback().set(m_serial[0], FUNC(rs232_port_device::write_txd));
	m_sio1->out_txdb_callback().set(m_serial[1], FUNC(rs232_port_device::write_txd));
	m_sio1->out_int_callback().set_inputline(m_maincpu, INPUT_LINE_IRQ0);

	MC14411(config, m_brg, XTAL(1'843'200));
	m_brg->rsa_w(0);
	m_brg->rsb_w(1);

	m_brg->out_f<1>().set(FUNC(idpartner_state::write_f1_clock));
	m_brg->out_f<13>().set(m_ctc, FUNC(z80ctc_device::trg0));

	RS232_PORT(config, m_serial[0], default_rs232_devices, nullptr);
	m_serial[0]->rxd_handler().set(m_sio1, FUNC(z80sio_device::rxa_w));

	RS232_PORT(config, m_serial[1], default_rs232_devices, nullptr);
	m_serial[1]->rxd_handler().set(m_sio1, FUNC(z80sio_device::rxb_w));

	Z80SIO(config, m_sio2, XTAL(8'000'000) / 2);
	m_sio2->out_txda_callback().set(m_serial[2], FUNC(rs232_port_device::write_txd));
	m_sio2->out_txdb_callback().set(m_serial[3], FUNC(rs232_port_device::write_txd));
	m_sio2->out_int_callback().set_inputline(m_maincpu, INPUT_LINE_IRQ0);

	RS232_PORT(config, m_serial[2], default_rs232_devices, nullptr);
	m_serial[2]->rxd_handler().set(m_sio2, FUNC(z80sio_device::rxa_w));

	RS232_PORT(config, m_serial[3], default_rs232_devices, nullptr);
	m_serial[3]->rxd_handler().set(m_sio2, FUNC(z80sio_device::rxb_w));

	Z80CTC(config, m_ctc, XTAL(8'000'000) / 2);
	m_ctc->intr_callback().set_inputline(m_maincpu, INPUT_LINE_IRQ0);
	m_ctc->zc_callback<0>().set(m_ctc, FUNC(z80ctc_device::trg1));
	m_ctc->zc_callback<2>().set(m_ctc, FUNC(z80ctc_device::trg3));  // optional

	Z80DAISY_GENERIC(config, m_fdc_daisy, 0xff);
	m_fdc_daisy->int_handler().set_inputline(m_maincpu, INPUT_LINE_IRQ0);

	I8272A(config, m_fdc, 8_MHz_XTAL);
	m_fdc->intrq_wr_callback().set(FUNC(idpartner_state::fdc_int_w));
	FLOPPY_CONNECTOR(config, "fdc:0", partner_floppies, "fdd",   floppy_image_device::default_mfm_floppy_formats).enable_sound(true);
	FLOPPY_CONNECTOR(config, "fdc:1", partner_floppies, nullptr, floppy_image_device::default_mfm_floppy_formats).enable_sound(true);

	// There is one bus connector J2, but cable goes to up to two devices
	IDPARTNER_BUS(config, m_bus, 0);
	m_bus->set_io_space(m_maincpu, AS_IO);
	m_bus->int_handler().set_inputline(m_maincpu, INPUT_LINE_IRQ0);
	m_bus->nmi_handler().set_inputline(m_maincpu, INPUT_LINE_NMI);
	IDPARTNER_BUS_CONNECTOR(config, m_conn[0], m_bus, idpartner_exp_devices, nullptr);
	IDPARTNER_BUS_CONNECTOR(config, m_conn[1], m_bus, idpartner_exp_devices, nullptr);
}

void idpartner_state::idpartner(machine_config &config)
{
	partner_base(config);

	m_serial[0]->set_default_option("terminal");
	m_serial[0]->set_option_device_input_defaults("terminal", DEVICE_INPUT_DEFAULTS_NAME(terminal)); // must be below the DEVICE_INPUT_DEFAULTS_START block
}

void idpartner_state::partner1fg(machine_config &config)
{
	partner_base(config);

	m_serial[0]->set_default_option("keyboard");

	m_conn[0]->set_default_option("gdp");
}

void idpartner_state::partnerwfg(machine_config &config)
{
	partner_base(config);

	m_serial[0]->set_default_option("keyboard");

	m_conn[0]->set_default_option("gdp");
}

/* ROM definition */

ROM_START( idpartner )
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD( "partner.e51",     0x0000, 0x800, CRC(cabcf36e) SHA1(9c391bacb8d1a742cf74803c61cc061707ab23f4) )
	// e50 is empty
ROM_END

ROM_START( partner1fg )
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD( "partner1fg.e51",  0x0000, 0x800, CRC(571e297a) SHA1(05379c75d6ceb338e49958576f3a1c844f202a00) )
	// e50 is empty
ROM_END

ROM_START( partnerwfg )
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD( "partnerwfg.e51",  0x0000, 0x800, CRC(81a2a3db) SHA1(22b23969d38cf2b400be0042dbdd6f8cff2536be) )
	// e50 is empty
ROM_END

} // anonymous namespace


/* Driver */

/*    YEAR  NAME        PARENT      COMPAT  MACHINE        INPUT        CLASS            INIT        COMPANY         FULLNAME         FLAGS */
COMP( 1984, idpartner,  0,          0,      idpartner,     idpartner,   idpartner_state, empty_init, "Iskra Delta",  "Partner",       MACHINE_NOT_WORKING | MACHINE_NO_SOUND_HW )
COMP( 1987, partner1fg, idpartner,  0,      partner1fg,    idpartner,   idpartner_state, empty_init, "Iskra Delta",  "Partner 1F/G",  MACHINE_NOT_WORKING | MACHINE_NO_SOUND_HW )
COMP( 1987, partnerwfg, idpartner,  0,      partnerwfg,    idpartner,   idpartner_state, empty_init, "Iskra Delta",  "Partner WF/G",  MACHINE_NOT_WORKING | MACHINE_NO_SOUND_HW )
