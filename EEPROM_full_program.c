//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: EK-TM4C123GXL w/ ENC28J60
// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

// Hardware configuration:
// EEPROM
//   MOSI (SSI2Tx) on PB7
//   MISO (SSI2Rx) on PB6
//   SCLK (SSI2Clk) on PB4
//   ~CS connected to PB1



// SPI_Init
// Initializes SPI communications in order to talk with M95M92 EEPROM
// Inputs: ~CS, PC5 - EEPROM clk, PC6 - EEPROM input
// Outputs: PC7

// Pin
#define PIN_EEPROM_CS (*((volatile uint32_t *)(0x42000000 + (0x400053FC-0x40000000)*32 + 1*4))

// Different LORA registers as defined
#define RegFifoAddrPtr          0x1E			// FIFO SPI pointer
#define RegFifoTxBaseAddr		0x02			// Start Tx Data
#define RegFifoRxBaseAddr		0x0A			// Start Rx Data
#define FifoRxCurrentAddr		0xFF			// Start address of last packet received
#define RegRxTimeout3			0x00			// Payload length
#define RegFifo					0x00			// FIFO data I/O
#define RegBitrateMsb			0x02			// Bit rate MSB
#define RegBitrateLsb			0x03			// Bit rate LSB
#define RegPaRamp				0x0A			// PA Ramp
#define RegOsc					0x24			// Oscillator frequency

void UART_init(){
	// Configure HW to work with 16 MHz XTAL, PLL enabled, system clock of 40 MHz
    SYSCTL_RCC_R = SYSCTL_RCC_XTAL_16MHZ | SYSCTL_RCC_OSCSRC_MAIN | SYSCTL_RCC_USESYSDIV | (4 << SYSCTL_RCC_SYSDIV_S);

    // Set GPIO ports to use APB (not needed since default configuration -- for clarity)
    // Note UART on port A must use APB
    SYSCTL_GPIOHBCTL_R = 0;

    // Enable GPIO port B and E peripherals
    SYSCTL_RCGC2_R = SYSCTL_RCGC2_GPIOB | SYSCTL_RCGC2_GPIOE | SYSCTL_RCGC2_GPIOC | SYSCTL_RCGC2_GPIOA;
    // Configure UART0 pins
    SYSCTL_RCGCUART_R |= SYSCTL_RCGCUART_R0 | SYSCTL_RCGCUART_R1;         // turn-on UART0, leave other uarts in same status
    GPIO_PORTA_DEN_R |= 3;                           // default, added for clarity
    GPIO_PORTA_AFSEL_R |= 3;                         // default, added for clarity
    GPIO_PORTA_PCTL_R = GPIO_PCTL_PA1_U0TX | GPIO_PCTL_PA0_U0RX;

        // Configure UART0 to 115200 baud, 8N1 format (must be 3 clocks from clock enable and config writes)
    UART0_CTL_R = 0;                                 // turn-off UART0 to allow safe programming
    UART0_CC_R = UART_CC_CS_SYSCLK;                  // use system clock (40 MHz)
    UART0_IBRD_R = 21;                               // r = 40 MHz / (Nx115.2kHz), set floor(r)=21, where N=16
    UART0_FBRD_R = 45;                               // round(fract(r)*64)=45
    UART0_LCRH_R = UART_LCRH_WLEN_8 | UART_LCRH_FEN; // configure for 8N1 w/ 16-level FIFO
    UART0_CTL_R = UART_CTL_TXE | UART_CTL_RXE | UART_CTL_UARTEN; // enable TX, RX, and module

}


void SPI_Init(){

    // Configure ~CS for EEPROM
    GPIO_PORTB_DIR_R = 0x02;  // make bit 1 an output
    GPIO_PORTB_DR2R_R = 0x02; // set drive strength to 2mA
    GPIO_PORTB_DEN_R = 0x02;  // enable bits 1 for digital

    //I/O pins for EEPROM
    GPIO_PORTE_DIR_R |= 0x70;
    GPIO_PORTC_DEN_R  |= 0x70; // PC5,PC6 as inputs, PC7 as output
    GPIO_PORTC_DR2R_R |= 0x70;
    GPIO_PORTC_PUR_R  |= 0x70;

    // Configure SSI2 pins for SPI configuration
    SYSCTL_RCGCSSI_R |= SYSCTL_RCGCSSI_R2;           // turn-on SSI2 clocking
    GPIO_PORTB_DIR_R |= 0x90;                        // make bits 4 and 7 outputs
    GPIO_PORTB_DR2R_R |= 0x90;                       // set drive strength to 2mA
	GPIO_PORTB_AFSEL_R |= 0xD0;                      // select alternative functions for MOSI, MISO, SCLK pins
    GPIO_PORTB_PCTL_R = GPIO_PCTL_PB7_SSI2TX | GPIO_PCTL_PB6_SSI2RX | GPIO_PCTL_PB4_SSI2CLK; // map alt fns to SSI2
    GPIO_PORTB_DEN_R |= 0xD0;                        // enable digital operation on TX, RX, CLK pins

    // Configure the SSI2 as a SPI master, mode 3, 8bit operation, 1 MHz bit rate
    SSI2_CR1_R &= ~SSI_CR1_SSE;                      // turn off SSI2 to allow re-configuration
    SSI2_CR1_R = 0;                                  // select master mode
    SSI2_CC_R = 0;                                   // select system clock as the clock source
    SSI2_CPSR_R = 40;                                // set bit rate to 1 MHz (if SR=0 in CR0)
    SSI2_CR0_R = SSI_CR0_FRF_MOTO | SSI_CR0_DSS_8;   // set SR=0, mode 0 (SPH=0, SPO=0), 8-bit
    SSI2_CR1_R |= SSI_CR1_SSE;                       // turn on SSI2
}

void LORA_init(){
    // disable transmission and reception of packets
    LORAClearReg(RegFifo, RegFifoRxBaseAddr);
    LORAClearReg(RegFifo, RegFifoTxBaseAddr);

    RegBitrateMsb = 0x10; // Slowing the bit rate to increase bandwidth. BitRate = FXOSC/((BitRate(15,0)+  (BitrateFrac/16))
    RegBitrateLsb = 0x11;
	RegOsc = 0x07;
	RegRxTimeout3 = 20; //Payload length of 20 bytes


	 txByte = LORA_WriteByte(addr,&data); // Data obtained from EEPROM talked with SPI
	 rxByte = LORA_ReadByte(addr,&data);

}

Unsigned char LORA_WriteByte(unsigned long address, unsigned char txByte){
    EEPROM_CS_On();
    spiWrite(0x02 | address);
    spiRead();
    spiWrite(txByte);
    spiRead();
    EEPROM_CS_OFF();
    return txByte;
}

void PC_commands(){
	// take a character c
	// c = putcUart0("some data");
	// getcUart0(c);

	// Hold the data in some constant variable and use it for EEPROM
}

Unsigned char LORA_ReadByte(unsigned long address, unsigned char* rxByte){
	EEPROM_CS_On();

    spiWrite(0x0A | address);
    spiRead();
    spiWrite(0);
    data = spiRead();
    EEPROM_CS_OFF();
    return rxByte;
};


// SPI_ReadAndWriteByte
// Sends a single byte out on the SPI tx line and waits for a response back on the rx.
// Inputs: The byte to transfer.
// Outputs: The byte returned.
void spiWrite(uint8_t data)
{
	SSI2_DR_R = data;
	while (SSI2_SR_R & SSI_SR_BSY);
}

uint8_t spiRead()
{
    return SSI2_DR_R;
}


// EEPROM_WriteByte
// Writes a byte to a specific address of the EEPROM.
// Inputs: The address to write the byte to and the byte to transfer.
// Outputs: Success or Failure.
Unsigned char EEPROM_WriteByte(unsigned long address, unsigned char txByte){
    EEPROM_CS_On();
    spiWrite(0x02 | address);
    spiRead();
    spiWrite(txByte);
    spiRead();
    EEPROM_CS_OFF();
    return txByte;
}

void EEPROM_CS_On()
{
    PIN_EEPROM_CS = 0;
	__asm (" NOP");                    // allow line to settle
	__asm (" NOP");
	__asm (" NOP");
	__asm (" NOP");
}

void EEPROM_CS_OFF()
{
    PIN_EEPROM_CS = 1;
}


// EEPROM_ReadByte
// Reads a byte from a specific address of the EEPROM.
// Inputs: The address to read from and a pointer to a variable to store the byte read.
// Outputs: Success (0) or Failure (1).
Unsigned char EEPROM_ReadByte(unsigned long address, unsigned char* rxByte){
	EEPROM_CS_On();

    spiWrite(0x03 | address);
    spiRead();
    spiWrite(0);
    data = spiRead();
    EEPROM_CS_OFF();
    return rxByte;
};

// Blocking function that writes a serial character when the UART buffer is not full
void putcUart0(char tempc)
{
	while (UART0_FR_R & UART_FR_TXFF);
	UART0_DR_R = tempc;
}

// Blocking function that writes a string when the UART buffer is not full
void putsUart0(char* tempstr)
{
	uint8_t l;
    for (l = 0; l < strlen(tempstr); l++)
	  putcUart0(tempstr[l]);
}

// Blocking function that returns with serial data once the buffer is not empty
char getcUart0()
{
	while(UART0_FR_R & UART_FR_RXFE)
	return UART0_DR_R & 0xFF;
}

void application(){
	// Interface FT232R to the controller. output of UART0 pins PA0,PA1 will be input for USB.
	char c = putcUart0(str);
	getcUart0();
	// Store the values in a USB register. (Could not configure due to time constraint)
	char USB_reg[] = putsUart0(str);

	// Transmit the commands to the LORA.

	char x = LORA_WriteByte(addr,&USB_reg);
	char y = LORA_ReadByte(addr,&data);
	// LORA receives the data and writes it using SPI

	// call the functions LORA read byte and write byte.

	// Next the .csv files need to be extracted to PC.
	import serial
		with open('names.csv', 'rb') as csvfile:
			c = csv.reader(csvfile, delimiter=' ', quotechar='|')
			for alphabets in c:
			print ' , '.join(alphabets)


}

int main(void) {
	uint8_t addr;
	UART_init();
	char data[128]; // This is the data received from PC through UART0. Talk to SPI with this data
	SPI_Init();
	char txByte = EEPROM_WriteByte(addr,&data);
	char rxByte = EEPROM_ReadByte(addr,&data);
	if(txByte == rxByte)
		printf("\n Valid data received");
	
	else
		printf("\n Invalid data");
	LORA_init();

	application();

	return 0;
}
