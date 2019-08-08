#include <stdint.h>
#include <generated/csr.h>
#include <irq.h>

#define SP_MOSI_PIN 0
#define SP_MISO_PIN 1
#define SP_WP_PIN 2
#define SP_HOLD_PIN 3
#define SP_CLK_PIN 4
#define SP_CS_PIN 5
#define SP_D0_PIN 0
#define SP_D1_PIN 1
#define SP_D2_PIN 2
#define SP_D3_PIN 3

#define PI_OUTPUT 1
#define PI_INPUT 0

__attribute__((section(".ramtext")))
static void gpioWrite(int pin, int val) {
    static uint8_t do_mirror;
    if (val)
        do_mirror |= 1 << pin;
    else
        do_mirror &= ~(1 << pin);
    picorvspi_cfg1_write(do_mirror);
}

__attribute__((section(".ramtext")))
static int gpioRead(int pin) {
    return !!(picorvspi_stat1_read() & (1 << pin));
}

__attribute__((section(".ramtext")))
void spiBegin(void) {
    gpioWrite(SP_WP_PIN, 1);
    gpioWrite(SP_HOLD_PIN, 1);
	gpioWrite(SP_CS_PIN, 0);
}

__attribute__((section(".ramtext")))
void spiEnd(void) {
	gpioWrite(SP_CS_PIN, 1);
}

__attribute__((section(".ramtext")))
void spiPause(void) {
	return;
}

__attribute__((section(".ramtext")))
static uint8_t spiXfer(uint8_t out) {
	int bit;
	uint8_t in = 0;

	for (bit = 7; bit >= 0; bit--) {
		if (out & (1 << bit)) {
			gpioWrite(SP_MOSI_PIN, 1);
		}
		else {
			gpioWrite(SP_MOSI_PIN, 0);
		}
		gpioWrite(SP_CLK_PIN, 1);
		spiPause();
		in |= ((!!gpioRead(SP_MISO_PIN)) << bit);
		gpioWrite(SP_CLK_PIN, 0);
		spiPause();
	}

	return in;
}

__attribute__((section(".ramtext")))
void spiCommand(uint8_t cmd) {
    spiXfer(cmd);
}

__attribute__((section(".ramtext")))
uint8_t spiCommandRx(void) {
    return spiXfer(0xff);
}

__attribute__((section(".ramtext")))
uint8_t spiReadStatus(void) {
	uint8_t val = 0xff;
    spiBegin();
    spiCommand(0x05);
    val = spiCommandRx();
    spiEnd();
    return val;
}

void spiBeginErase4(uint32_t erase_addr) {
	// Enable Write-Enable Latch (WEL)
	spiBegin();
	spiCommand(0x06);
	spiEnd();

	spiBegin();
	spiCommand(0x20);
	spiCommand(erase_addr >> 16);
	spiCommand(erase_addr >> 8);
	spiCommand(erase_addr >> 0);
	spiEnd();
}

void spiBeginErase32(uint32_t erase_addr) {
	// Enable Write-Enable Latch (WEL)
	spiBegin();
	spiCommand(0x06);
	spiEnd();

	spiBegin();
	spiCommand(0x52);
	spiCommand(erase_addr >> 16);
	spiCommand(erase_addr >> 8);
	spiCommand(erase_addr >> 0);
	spiEnd();
}

void spiBeginErase64(uint32_t erase_addr) {
	// Enable Write-Enable Latch (WEL)
	spiBegin();
	spiCommand(0x06);
	spiEnd();

	spiBegin();
	spiCommand(0xD8);
	spiCommand(erase_addr >> 16);
	spiCommand(erase_addr >> 8);
	spiCommand(erase_addr >> 0);
	spiEnd();
}

__attribute__((section(".ramtext")))
int spiIsBusy(void) {
  	return spiReadStatus() & (1 << 0);
}

void spiBeginWrite(uint32_t addr, const void *v_data, unsigned int count) {
	const uint8_t write_cmd = 0x02;
	const uint8_t *data = v_data;
	unsigned int i;

	// Enable Write-Enable Latch (WEL)
	spiBegin();
	spiCommand(0x06);
	spiEnd();

	spiBegin();
	spiCommand(write_cmd);
	spiCommand(addr >> 16);
	spiCommand(addr >> 8);
	spiCommand(addr >> 0);
	for (i = 0; (i < count) && (i < 256); i++)
		spiCommand(*data++);
	spiEnd();
}

__attribute__((section(".ramtext")))
uint32_t spiId(void) {
	uint32_t id = 0;

	spiBegin();
	spiCommand(0x90);                // Read manufacturer ID
	spiCommand(0x00);                // Dummy byte 1
	spiCommand(0x00);                // Dummy byte 2
	spiCommand(0x00);                // Dummy byte 3
	id = (id << 8) | spiCommandRx(); // Manufacturer ID
	id = (id << 8) | spiCommandRx(); // Device ID
	spiEnd();

	spiBegin();
	spiCommand(0x9f);                // Read device id
	(void)spiCommandRx();            // Manufacturer ID (again)
	id = (id << 8) | spiCommandRx(); // Memory Type
	id = (id << 8) | spiCommandRx(); // Memory Size
	spiEnd();

	return id;
}

__attribute__((section(".ramtext")))
void spiStartBB(void) {
    irq_setie(0);
    // Now that everything is copied to RAM, disable memory-mapped SPI mode.
    // This puts the SPI into bit-banged mode, which allows us to write to it.
    picorvspi_cfg1_write(0x20); // CS high
    picorvspi_cfg2_write(0x0d); // IO0, WP, HOLD pins to output
    picorvspi_cfg4_write(0);
}

__attribute__((section(".ramtext")))
void spiEndBB(void) {
    picorvspi_cfg4_write(0x80);
    irq_setie(1);
}