//===== Libraries and lib-dependant definitions
#include "horus_l2.h"
//#include "horus_l2.cpp"
#include <SPI.h>
#include <TinyGPSPlus.h>
TinyGPSPlus gps;


#define RSM4x4 //new pcb versions
//#define RSM4x2 //old pcb versions, also rsm4x1

#ifdef RSM4x4
  bool rsm4x2 = false;
  bool rsm4x4 = true;
  //===== Pin definitions
  #define RED_LED_PIN PC8    //red led - reversed operation, pin HIGH=led_off, pin LOW=led_on!
  #define GREEN_LED_PIN PC7  //green led - reversed operation, pin HIGH=led_off, pin LOW=led_on!

  #define PSU_SHUTDOWN_PIN PA9  //battery mosfet disable pin
  #define VBAT_PIN PA5          //battery voltage divider
  #define VBTN_PIN PA6          //button state voltage divider, CHECK VALUES AT LOWER SUPPLY VOLTAGES

  #define MOSI_RADIO_SPI PB15
  #define MISO_RADIO_SPI PB14
  #define SCK_RADIO_SPI PB13
  #define CS_RADIO_SPI PC13
  #define HEAT_REF PC6   //reference heating resistors - LOW=disabled, HIGH=enabled - draw about 180mA of current and after a while may burn skin
  bool heaterAvail = true;
  #define REF_THERM PB1  //reference heating thermistor
  #define PULLUP_TM PB12 //ring oscillator activation mosfet for temperature reading
  #define PULLUP_HYG PA2 //ring oscillator activation mosfet for humidity reading
  #define SPST1 PB3 //idk spst1
  #define SPST2 PA3 //idk spst2
  #define SPST3 PC14 //boom hygro heater temperature
  #define SPST4 PC15 //boom main temperature

  #define SI4032_CLOCK 26.0

  //===== Interfaces
  //SPI_2 interface class (radio communication etc.)
  SPIClass SPI_2(PB15, PB14, PB13);  // MOSI, MISO, SCK for SPI2
  // XDATA (2,3)          rx    tx
  HardwareSerial xdataSerial(PB11, PB10);
  // ublox gps              rx    tx
  HardwareSerial gpsSerial(PB7, PB6);
  int gpsBaudRate = 38400;

#elif defined(RSM4x2)
  bool rsm4x2 = true;
  bool rsm4x4 = false;
  //===== Pin definitions
  #define RED_LED_PIN PB8    //red led - reversed operation, pin HIGH=led_off, pin LOW=led_on!
  #define GREEN_LED_PIN PB7  //green led - reversed operation, pin HIGH=led_off, pin LOW=led_on!

  #define PSU_SHUTDOWN_PIN PA12  //battery mosfet disable pin
  #define VBAT_PIN PA5          //battery voltage 
  #define VBTN_PIN PA6          //button state voltage divider, CHECK VALUES AT LOWER SUPPLY VOLTAGES

  #define MOSI_RADIO_SPI PB15
  #define MISO_RADIO_SPI PB14
  #define SCK_RADIO_SPI PB13
  #define CS_RADIO_SPI PC13
  #define HEAT_REF 0   //not available on older versions, switched only by si4032 gpio
  bool heaterAvail = false;
  #define REF_THERM PB1  //reference heating thermistor
  #define PULLUP_TM PB12 //ring oscillator activation mosfet for temperature reading
  #define PULLUP_HYG PA2 //ring oscillator activation mosfet for humidity reading
  #define SPST1 PB3 //idk spst1
  #define SPST2 PA3 //idk spst2
  #define SPST3 PC14 //boom hygro heater temperature
  #define SPST4 PC15 //boom main temperature

  #define SI4032_CLOCK 26.0

  //===== Interfaces
  //SPI_2 interface class (radio communication etc.)
  SPIClass SPI_2(PB15, PB14, PB13);  // MOSI, MISO, SCK for SPI2
  // XDATA (2,3)          rx    tx
  HardwareSerial xdataSerial(PB11, PB10);
  // ublox gps              rx    tx
  HardwareSerial gpsSerial(PA10, PA9);
  int gpsBaudRate = 9600;

#else
  #error "Please define the pcb model!"
#endif



unsigned char tx_buf[64] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 };

//===== Radio signals config
#define CALLSIGN "N0CALLN0CALL"  //max 10 chars long, currently used for rtty
//#define CALLSIGN_SHORT "N0CALL1" //max 6 chars long, currently unused
#define PREAMBLE "AA"            //max 2 long
int radioPwrSetting = 7; //0 = -1dBm (~0.8mW), 1 = 2dBm (~1.6mW), 2 = 5dBm (~3 mW), 3 = 8dBm (~6 mW), 4 = 11dBm (~12 mW), 5 = 14dBm (25 mW), 6 = 17dBm (50 mW), 7 = 20dBm (100 mW)
bool enableAddData = false;      //rtty-only mode, when false, the additional-data space is filled with zeros


bool pipEnable = false; //pip tx mode
float pipFrequencyMhz = 434.5; //pip tx frequency
int pipLengthMs = 1000; //pip signal length in ms
int pipRepeat = 3; //pip signal repeat count in 1 transmit group

bool morseEnable = false; //morse tx mode
float morseFrequencyMhz = 434.65; //morse tx frequency
int morseUnitTime = 40;  //ms

bool rttyEnable = false; //rtty tx mode
bool rttyShortenMsg = false; //false- all data, true - without preamble, time, speed and adddata
float rttyFrequencyMhz = 434.78; //rtty tx frequency
int rttyBitDelay = 22000;  //22000 ~= 45bdrate, 13333 ~= 75bdr
#define RTTY_RADIO_MARK_OFFSET 0x02 //for space offset set to 0x01, the rtty tone spacing will be: 0x02 - 270Hz spacing, 0x03 - 540Hz spacing | SPACING OTHER THAN 270HZ DOES NOT WORK (at lesast on my tests, will check later)
#define RTTY_RADIO_SPACE_OFFSET 0x01 //usually set to 0x01

bool horusEnable = true; //horus v2 tx mode
float horusFrequencyMhz = 434.74;
int horusBdr = 100;

bool bufTxEnable = false; //alpha state! mode
float fskFrequencyMhz = 436; //txBuf tx frequency

bool radioEnablePA = false;  //default tx state
bool radioSleep = true; //lowers power consumption and recalibrates oscillator (drift compensation)
int modeChangeDelay = 0; //0 - disable, 0<delay<2000 - standard delay, 2000<delay - delay + radio sleep mode and recalibration (if radioSleep enabled)


//===== Other operation config
bool ledStatusEnable = true;
bool xdataDebug = true; //if xdataDebug is enabled, debug messages are sent to the xdata header serial port. WARNING! THIS HAS TO BE FALSE WHEN USING XDATA DEVICES (such as OIF411 ozone sensor)
float vBatWarnValue = 2.5; //battery warning voltage
float vBatErrValue = 2.3; //error voltage
float batteryCutOffVoltage = 1.9; //good for nimh cell life, below 0.8V per AA cell the damage could occur
int ovhtWarnValue = 45; //overheating warning
int ovhtErrValue = 55; //overheating error
int gpsSatsWarnValue = 4; 
int refHeatingMode = 1; //0 - off, 1 - auto, 2 - always on
unsigned long heaterWorkingTimeSec = 600; //heater heating time
unsigned long heaterCooldownTimeSec = 3; //heater cooldown time after heating process
int autoHeaterThreshold = 6; //auto heater temperature threshold, WARNING! If rtty used baud is faster than 45bdr, the threshold should be at 14*C to prevent the radio from loosing PPL-lock, best would be even to set the heating to always on. If only horus mode is used, it is not mandatory, altough for standard flights that dont require more than 12h of operation the 6*C is advised for defrosting and keeping the internals slightly above ice temperature.
int refHeaterCriticalDisableTemp = 70; //heater critical temp which disables the heating if exceeded
int refHeaterCriticalReenableTemp = 65; //heater temperature at which heating gets re-enabled after being cut down due to too high temperature
int gpsNmeaMsgWaitTime = 1200; //waiting time for gps message


//===== System internal variables
int btnCounter = 0;
int bufPacketLength = 64;
int txRepeatCounter = 0;
float batVFactor = 1.0;
bool ledsEnable = ledStatusEnable; //internal boolean used for height disable etc.
#define R25 10400  // 10k Ohms at 25°C thermistor 
#define B 4295     // Beta parameter calculated thermistor
String rttyMsg;
String morseMsg;
unsigned long gpsTime;
int gpsHours;
int gpsMinutes;
int gpsSeconds;
float gpsLat;
float gpsLong;
float gpsAlt;
float gpsSpeed;
int gpsSats; //system wide variables, for use in functions that dont read the gps on their own
unsigned long heaterOnTime = 0;   // Stores the time when heater was turned on
unsigned long heaterOffTime = 0;  // Stores the time when heater was turned off
bool isHeaterOn = false;          // Tracks the current state of the heater
bool isHeaterPausedOvht = false;
uint8_t heaterDebugState = 0; //5 - off, 11 - ON in auto, 10- auto but OFF, 21 - forced on, 19 - overheated when in auto, 29 - ovht when manual
int deviceDebugState = 0; //heaterdebugstate + [0 if ok; 100 if warn; 200 if err]
bool err = false; //const red light, status state
bool warn = false; //orange light, status state
bool ok = true;; //green light, status state
bool vBatErr = false;
bool vBatWarn = false;
bool ovhtErr = false;
bool ovhtWarn = false;
bool gpsFixWarn = false;
int horusPacketCount;




//===== Horus mode deifinitions
// Horus v2 Mode 1 (32-byte) Binary Packet
//== FOR MORE INFO SEE int build_horus_binary_packet_v2(char *buffer) function around line 600
struct HorusBinaryPacketV2
{
    uint16_t     PayloadID;
    uint16_t	Counter;
    uint8_t	Hours;
    uint8_t	Minutes;
    uint8_t	Seconds;
    float	Latitude;
    float	Longitude;
    uint16_t  	Altitude;
    uint8_t     Speed;       // Speed in Knots (1-255 knots)
    uint8_t     Sats;
    int8_t      Temp;        // Twos Complement Temp value.
    uint8_t     BattVoltage; // 0 = 0.5v, 255 = 2.0V, linear steps in-between.
    // The following 9 bytes (up to the CRC) are user-customizable. The following just
    // provides an example of how they could be used.
    int16_t     dummy1;      // unsigned int uint8_t
    int16_t     dummy2;       // Float float
    uint8_t     dummy3;     // battery voltage test uint8_t
    uint16_t     dummy4;     // divide by 10 uint8_t
    uint16_t     dummy5;    // divide by 100 uint16_t
    uint16_t    Checksum;    // CRC16-CCITT Checksum.
}  __attribute__ ((packed));

// Buffers and counters.
char rawbuffer [128];   // Buffer to temporarily store a raw binary packet.
char codedbuffer [128]; // Buffer to store an encoded binary packet
char debugbuffer[256]; // Buffer to store debug strings

uint32_t fsk4_base = 0, fsk4_baseHz = 0;
uint32_t fsk4_shift = 0, fsk4_shiftHz = 0;
uint32_t fsk4_bitDuration;
uint32_t fsk4_tones[4];
uint32_t fsk4_tonesHz[4];



//===== Morse mode definitions
// Morse code mapping for letters A-Z, digits 0-9, and space
const char* MorseTable[37] = {
  ".-",     // A
  "-...",   // B
  "-.-.",   // C
  "-..",    // D
  ".",      // E
  "..-.",   // F
  "--.",    // G
  "....",   // H
  "..",     // I
  ".---",   // J
  "-.-",    // K
  ".-..",   // L
  "--",     // M
  "-.",     // N
  "---",    // O
  ".--.",   // P
  "--.-",   // Q
  ".-.",    // R
  "...",    // S
  "-",      // T
  "..-",    // U
  "...-",   // V
  ".--",    // W
  "-..-",   // X
  "-.--",   // Y
  "--..",   // Z
  "-----",  // 0
  ".----",  // 1
  "..---",  // 2
  "...--",  // 3
  "....-",  // 4
  ".....",  // 5
  "-....",  // 6
  "--...",  // 7
  "---..",  // 8
  "----.",  // 9
  " "       // Space (7 dot lengths)
};




//===== Radio config functions 

void init_RFM22(void) { //initial radio config, it is dynamic in code but this is the entry configuration for cw to work
  writeRegister(0x06, 0x00);             // Disable all interrupts
  writeRegister(0x07, 0x01);             // Set READY mode
  writeRegister(0x09, 0x7F);             // Cap = 12.5pF
  writeRegister(0x0A, 0x05);             // Clk output is 2MHz
  writeRegister(0x0B, 0xF4);             // GPIO0 is for RX data output
  writeRegister(0x0C, 0xEF);             // GPIO1 is TX/RX data CLK output
  writeRegister(0x0D, 0x00);             // GPIO2 for MCLK output
  writeRegister(0x0E, 0x00);             // GPIO port use default value
  writeRegister(0x0F, 0x80);             // adc config
  writeRegister(0x10, 0x00);             // offset or smth idk
  writeRegister(0x12, 0x20);             // temp sensor calibration
  writeRegister(0x13, 0x00);             // temp offset
  writeRegister(0x70, 0x20);             // No manchester code, no data whitening, data rate < 30Kbps
  writeRegister(0x1C, 0x1D);             // IF filter bandwidth
  writeRegister(0x1D, 0x40);             // AFC Loop
  writeRegister(0x20, 0xA1);             // Clock recovery
  writeRegister(0x21, 0x20);             // Clock recovery
  writeRegister(0x22, 0x4E);             // Clock recovery
  writeRegister(0x23, 0xA5);             // Clock recovery
  writeRegister(0x24, 0x00);             // Clock recovery timing
  writeRegister(0x25, 0x0A);             // Clock recovery timing
  writeRegister(0x2C, 0x00);             // Unused register
  writeRegister(0x2D, 0x00);             // Unused register
  writeRegister(0x2E, 0x00);             // Unused register
  writeRegister(0x6E, 0x27);             // TX data rate 1
  writeRegister(0x6F, 0x52);             // TX data rate 0
  writeRegister(0x30, 0x8C);             // Data access control
  writeRegister(0x32, 0xFF);             // Header control
  writeRegister(0x33, 0x42);             // Header control
  writeRegister(0x34, 64);               // 64 nibble = 32 byte preamble
  writeRegister(0x35, 0x20);             // Preamble detection
  writeRegister(0x36, 0x2D);             // Synchronize word
  writeRegister(0x37, 0xD4);             // Synchronize word
  writeRegister(0x3A, 's');              // Set TX header 3
  writeRegister(0x3B, 'o');              // Set TX header 2
  writeRegister(0x3C, 'n');              // Set TX header 1
  writeRegister(0x3D, 'g');              // Set TX header 0
  writeRegister(0x3E, bufPacketLength);  // Set packet length to 16 bytes
  writeRegister(0x3F, 's');              // Set RX header
  writeRegister(0x40, 'o');              // Set RX header
  writeRegister(0x41, 'n');              // Set RX header
  writeRegister(0x42, 'g');              // Set RX header
  writeRegister(0x43, 0xFF);             // Check all bits
  writeRegister(0x44, 0xFF);             // Check all bits
  writeRegister(0x45, 0xFF);             // Check all bits
  writeRegister(0x46, 0xFF);             // Check all bits
  writeRegister(0x56, 0x01);             // Unused register
  writeRegister(0x6D, 0x07);             // TX power to min (max = 0x07)
  writeRegister(0x79, 0x00);             // No frequency hopping
  writeRegister(0x7A, 0x00);             // No frequency hopping
  //writeRegister(0x71, 0x22);    // GFSK, fd[8]=0, no invert for TX/RX data, FIFO mode, txclk-->gpio
  //writeRegister(0x71, 0x00);    //CW
  //writeRegister(0x71, 0x10);    //FSK
  //writeRegister(0x72, 0x48);    // Frequency deviation setting to 45K=72*625
  writeRegister(0x72, 0x08);
  writeRegister(0x73, 0x00);  // No frequency offset
  writeRegister(0x74, 0x00);  // No frequency offset
  writeRegister(0x75, 0x21);  // Frequency set to 435MHz 36/40
  writeRegister(0x76, 0x18);  // Frequency set to 435MHz
  writeRegister(0x77, 0x0A);  // Frequency set to 435Mhz
  writeRegister(0x5A, 0x7F);  // Unused register
  writeRegister(0x59, 0x40);  // Unused register
  writeRegister(0x58, 0x80);  // Unused register
  writeRegister(0x6A, 0x0B);  // Unused register
  writeRegister(0x68, 0x04);  // Unused register
  writeRegister(0x1F, 0x03);  // Unused register
}

void radioSoftReset() {
  writeRegister(0x07, 0x80);
}

void radioEnableTx() {
  // Modified to set the PLL and Crystal enable bits to high. Not sure if this makes much difference.
  writeRegister(0x07, 0x4B);
}

void radioInhibitTx() {
  // Sleep mode, but with PLL idle mode enabled, in an attempt to reduce drift on key-up.
  writeRegister(0x07, 0x43);
}

void radioDisableTx() {
  writeRegister(0x07, 0x40);
}



//===== Low-Level SPI Communication for Si4032

void writeRegister(uint8_t address, uint8_t data) {
  digitalWrite(CS_RADIO_SPI, LOW);
  SPI_2.transfer(address | 0x80);  // Write mode using SPI2
  SPI_2.transfer(data);
  digitalWrite(CS_RADIO_SPI, HIGH);
}

uint8_t readRegister(uint8_t address) {
  uint8_t result;
  digitalWrite(CS_RADIO_SPI, LOW);
  SPI_2.transfer(address & 0x7F);  // Read mode using SPI2
  result = SPI_2.transfer(0x00);
  digitalWrite(CS_RADIO_SPI, HIGH);
  return result;
}

void setRadioFrequency(const float frequency_mhz) {
  uint8_t hbsel = (uint8_t)((frequency_mhz * (30.0f / SI4032_CLOCK)) >= 480.0f ? 1 : 0);

  uint8_t fb = (uint8_t)((((uint8_t)((frequency_mhz * (30.0f / SI4032_CLOCK)) / 10) - 24) - (24 * hbsel)) / (1 + hbsel));
  uint8_t gen_div = 3;  // constant - not possible to change!
  uint16_t fc = (uint16_t)(((frequency_mhz / ((SI4032_CLOCK / gen_div) * (hbsel + 1))) - fb - 24) * 64000);
  writeRegister(0x75, (uint8_t)(0b01000000 | (fb & 0b11111) | ((hbsel & 0b1) << 5)));
  writeRegister(0x76, (uint8_t)(((uint16_t)fc >> 8U) & 0xffU));
  writeRegister(0x77, (uint8_t)((uint16_t)fc & 0xff));
}

void setRadioPower(uint8_t power) {
  writeRegister(0x6D, power & 0x7U);
}

void setRadioOffset(uint16_t offset) {
  writeRegister(0x73, offset);
  writeRegister(0x74, 0);
}

void setRadioSmallOffset(uint8_t offset) {
  writeRegister(0x73, offset);
}

void setRadioDeviation(uint8_t deviation) {
  // The frequency deviation can be calculated: Fd = 625 Hz x fd[8:0].
  // Zero disables deviation between 0/1 bits
  writeRegister(0x72, deviation);
}

void setRadioModulation(int modulationNumber) {
  /* modulationNumber:
    = 0 = no modulation (CW)
    = 1 = OOK
    = 2 = FSK
    = 3 = GFSK
    */

  if (modulationNumber == 0) {
    writeRegister(0x71, 0x00);  //cw
  }
  else if (modulationNumber == 1) {
    writeRegister(0x71, 0b00010001);  //OOK
  }
  else if (modulationNumber == 2) {
    writeRegister(0x71, 0b00010010);  //FSK
  }
  else if (modulationNumber == 3) {
    writeRegister(0x71, 0x22);  //GFSK
  }
  else {
    writeRegister(0x71, 0x00);  //error handling
  }
}



//===== Radio modes functions

//unused buffer mode for now, development implementation seems to transmit something out of the FIFO
void txBuf(void) {
  writeRegister(0x07, 0x01);  // To ready mode

  writeRegister(0x08, 0x03);  // FIFO reset
  writeRegister(0x08, 0x00);  // Clear FIFO

  writeRegister(0x34, 64);               // Preamble = 64 nibbles
  writeRegister(0x3E, bufPacketLength);  // Packet length = 16 bytes
  for (unsigned char i = 0; i < bufPacketLength; i++) {
    writeRegister(0x7F, tx_buf[i]);  // Send payload to the FIFO
  }

  writeRegister(0x07, 0x09);  // Start TX

  delay(50);  // Add a small delay to ensure the transmission completes

  writeRegister(0x07, 0x01);  // To ready mode
}

//rtty
void sendRTTYPacket(const char* message) {
  while (*message) {
    sendCharacter(*message++);
  }
}

void sendBit(bool bitValue) {
  if (bitValue) {
    setRadioSmallOffset(RTTY_RADIO_MARK_OFFSET);
  } else {
    setRadioSmallOffset(RTTY_RADIO_SPACE_OFFSET);
  }
  delayMicroseconds(rttyBitDelay); // Fixed delay according to baud rate
  buttonHandler();
}

void sendStartBits() {
  // Send start bits (usually 1 start bit)
  sendBit(0); // Start bit (0)
}

void sendCharacter(char character) {
  // Encode character to RTTY format (assuming default encoding and no special encoding needed for . and -)
  
  // Send start bits
  sendStartBits();

  // Character encoding (use default encoding, 7-bit or 8-bit)
  uint8_t encoding = (uint8_t)character; // Assuming ASCII encoding for characters
  for (int i = 0; i < 8; i++) {
    sendBit((encoding >> i) & 0x01);
  }

  // Send stop bits (1.5 stop bits)
  sendBit(1); // First stop bit
  delayMicroseconds(rttyBitDelay); // Delay between stop bits
  sendBit(1); // Extra stop bit (part of 1.5 stop bits)
}


//morse
void transmitMorseChar(const char* morseChar, int unitTime) {
  while (*morseChar) {
    if (*morseChar == '.') {
      // Dot: 1 unit time on
      radioEnableTx();
      delay(unitTime);
      radioDisableTx();
    } else if (*morseChar == '-') {
      // Dash: 3 unit times on
      radioEnableTx();
      delay(3 * unitTime);
      radioDisableTx();
    }
    // Space between elements: 1 unit time off
    delay(unitTime);

    morseChar++;

    buttonHandler();
  }
}

void transmitMorseString(const char* str, int unitTime) {
  while (*str) {
    char c = *str++;

    // Handle letters (A-Z) and digits (0-9)
    if (c >= 'A' && c <= 'Z') {
      transmitMorseChar(MorseTable[c - 'A'], unitTime);
    } else if (c >= '0' && c <= '9') {
      transmitMorseChar(MorseTable[c - '0' + 26], unitTime);
    } else if (c == ' ') {
      // Word space: 7 unit times off
      delay(7 * unitTime);
    }
    // Space between characters: 3 unit times off
    delay(3 * unitTime);
  }
}


//4fsk implementation for horus
void fsk4_tone(int t) {
  switch (t) {
    case 0:
      setRadioSmallOffset(0x01);
      break;
    case 1:
      setRadioSmallOffset(0x02);
      break;
    case 2:
      setRadioSmallOffset(0x03);
      break;
    case 3:
      setRadioSmallOffset(0x04);
      break;
    default:
      setRadioSmallOffset(0x01);
  }

  delayMicroseconds(fsk4_bitDuration);
}

void fsk4_idle(){
    fsk4_tone(0);
}

void fsk4_preamble(uint8_t len){
    int k;
    for (k=0; k<len; k++){
        fsk4_writebyte(0x1B);
    }
}

size_t fsk4_writebyte(uint8_t b){
    int k;
    // Send symbols MSB first.
    for (k=0;k<4;k++)
    {
        // Extract 4FSK symbol (2 bits)
        uint8_t symbol = (b & 0xC0) >> 6;
        // Modulate
        fsk4_tone(symbol);
        // Shift to next symbol.
        b = b << 2;
    }

  return(1);
}

size_t fsk4_write(char* buff, size_t len){
  size_t n = 0;
  for(size_t i = 0; i < len; i++) {
    n += fsk4_writebyte(buff[i]);
  }
  return(n);
}



//===== Radio payload creation

String createRttyPayload() {
  String payload;
  String addData;

  String formattedLat = String(gpsLat, 5);
  String formattedLong = String(gpsLong, 5);

  if (enableAddData) {
    addData = "ADD DATA 0";  // Example additional data
  } else {
    addData = "0000000000";  // Default additional data
  }

  payload = String(PREAMBLE) + ";" + String(CALLSIGN) + ";" + String(gpsTime) + ";" + String(gpsLat, 5) + ";" + String(gpsLong, 5) + ";" + String(gpsAlt, 1) + ";" + String(gpsSpeed) + ";" + String(gpsSats) + ";" + String(readBatteryVoltage()) + ";" + String(readThermistorTemp(), 1) + ";" + addData;  

  return payload;
}

String createRttyShortPayload() {
  String payload;
  String addData;

  String formattedLat = String(gpsLat, 5);
  String formattedLong = String(gpsLong, 5);

  payload = String(CALLSIGN) + ";" + String(gpsLat, 5) + ";" + String(gpsLong, 5) + ";" + String(gpsAlt, 1) + ";" + String(gpsSats) + ";" + String(readBatteryVoltage()) + ";" + String(readThermistorTemp(), 1);  

  return payload;
}

int build_horus_binary_packet_v2(char *buffer){
  // Generate a Horus Binary v2 packet, and populate it with data.
  // The assignments in this function should be replaced with real data
  horusPacketCount++;
  
  struct HorusBinaryPacketV2 BinaryPacketV2;

  // DUMMY FORMAT FOR FIRST 4 (LAST NOT USED) BYTES OF ADDITIONAL DATA USING 4FSKTEST-V2 CODING (used in rs41ng and others), dummy1-5 definitions were changed based on the 4FSKTEST-V2 definitions, comments at the end of the definition contain orig values

  BinaryPacketV2.PayloadID = 256; // 0256 = 4FSKTEST-V2. Refer https://github.com/projecthorus/horusdemodlib/blob/master/payload_id_list.txt
  BinaryPacketV2.Counter = horusPacketCount;
  BinaryPacketV2.Hours = gpsHours;
  BinaryPacketV2.Minutes = gpsMinutes;
  BinaryPacketV2.Seconds = gpsSeconds;
  BinaryPacketV2.Latitude = gpsLat;
  BinaryPacketV2.Longitude = gpsLong;
  BinaryPacketV2.Altitude = gpsAlt;
  BinaryPacketV2.Speed = gpsSpeed;
  BinaryPacketV2.BattVoltage = map(readBatteryVoltage() * 100, 0, 5 * 100, 0, 255);
  BinaryPacketV2.Sats = gpsSats;
  BinaryPacketV2.Temp = readThermistorTemp();
  // Custom section. This is an example only, and the 9 bytes in this section can be used in other
  // ways. Refer here for details: https://github.com/projecthorus/horusdemodlib/wiki/5-Customising-a-Horus-Binary-v2-Packet
  BinaryPacketV2.dummy1 = 0; //-32768 - 32767 int16_t
  BinaryPacketV2.dummy2 = 0; //-32768 - 32767 int16_t
  BinaryPacketV2.dummy3 = deviceDebugState; //0 - 255 uint8_t
  BinaryPacketV2.dummy4 = 0; //0 - 65535 uint16_t
  BinaryPacketV2.dummy5 = 0; //unused in this decoding scheme

  BinaryPacketV2.Checksum = (uint16_t)crc16((unsigned char*)&BinaryPacketV2,sizeof(BinaryPacketV2)-2);

  memcpy(buffer, &BinaryPacketV2, sizeof(BinaryPacketV2));
	
  return sizeof(struct HorusBinaryPacketV2);
}



//===== Function-only algorythms (for horus modem etc.)

unsigned int _crc_xmodem_update(unsigned int crc, uint8_t data) {
    crc ^= data << 8;
    for (int i = 0; i < 8; i++) {
        if (crc & 0x8000) {
            crc = (crc << 1) ^ 0x1021;
        } else {
            crc <<= 1;
        }
    }
    return crc;
}

// CRC16 Calculation
unsigned int crc16(unsigned char *string, unsigned int len) {
    unsigned int crc = 0xFFFF; // Initial seed
    for (unsigned int i = 0; i < len; i++) {
        crc = _crc_xmodem_update(crc, string[i]);
    }
    return crc;
}

void PrintHex(char *data, uint8_t length, char *tmp){
 // Print char data as hex
 byte first ;
 int j=0;
 for (uint8_t i=0; i<length; i++) 
 {
   first = ((uint8_t)data[i] >> 4) | 48;
   if (first > 57) tmp[j] = first + (byte)39;
   else tmp[j] = first ;
   j++;

   first = ((uint8_t)data[i] & 0x0F) | 48;
   if (first > 57) tmp[j] = first + (byte)39; 
   else tmp[j] = first;
   j++;
 }
 tmp[length*2] = 0;
}


//===== System operation handlers

void buttonHandler() {
  if (analogRead(VBTN_PIN) + 50 > analogRead(VBAT_PIN)) { //new button logic, before VBTN_PIN > 1585 which under lower voltage doesnt work, now reads according to voltage
    if(xdataDebug) {
      xdataSerial.println("Button pressed");
    }
    btnCounter = 0;
    while (btnCounter < 7 && analogRead(VBTN_PIN) + 50 > analogRead(VBAT_PIN)) {
      digitalWrite(RED_LED_PIN, HIGH);
      digitalWrite(GREEN_LED_PIN, LOW);
      delay(250);
      digitalWrite(RED_LED_PIN, LOW);
      digitalWrite(GREEN_LED_PIN, HIGH);
      delay(250);
      digitalWrite(RED_LED_PIN, HIGH);
      digitalWrite(GREEN_LED_PIN, HIGH);
      if(xdataDebug) {
        xdataSerial.print("*");
      }
      btnCounter++;
    }
  }
  if (btnCounter == 1) {

  }
  else if (btnCounter == 2) {
    if (radioEnablePA == true) {
      radioEnablePA = false;
      digitalWrite(RED_LED_PIN, LOW);
      delay(50);
      digitalWrite(RED_LED_PIN, HIGH);
      delay(50);
      digitalWrite(RED_LED_PIN, LOW);
      delay(50);
      digitalWrite(RED_LED_PIN, HIGH);
      if(xdataDebug) {
        xdataSerial.println("Radio PA disabled");
      }
    }
    else {
      radioEnablePA = true;
      digitalWrite(GREEN_LED_PIN, LOW);
      delay(50);
      digitalWrite(GREEN_LED_PIN, HIGH);
      delay(50);
      digitalWrite(GREEN_LED_PIN, LOW);
      delay(50);
      digitalWrite(GREEN_LED_PIN, HIGH);
      if(xdataDebug) {
        xdataSerial.println("Radio PA enabled");
      }
    }
  }
  else if (btnCounter == 3) {
    if (radioPwrSetting != 7) {
      radioPwrSetting = 7;
      setRadioPower(7);

      digitalWrite(GREEN_LED_PIN, LOW);
      delay(50);
      digitalWrite(GREEN_LED_PIN, HIGH);
      delay(50);
      digitalWrite(GREEN_LED_PIN, LOW);
      delay(50);
      digitalWrite(GREEN_LED_PIN, HIGH);
      delay(50);
      digitalWrite(GREEN_LED_PIN, LOW);
      delay(50);
      digitalWrite(GREEN_LED_PIN, HIGH);
      if(xdataDebug) {
        xdataSerial.println("Radio PA power set to 100mW (+20dBm, MAX!)");
      }  
    }
    else {
      radioPwrSetting = 0;
      setRadioPower(0);

      digitalWrite(RED_LED_PIN, LOW);
      delay(50);
      digitalWrite(RED_LED_PIN, HIGH);
      delay(50);
      digitalWrite(RED_LED_PIN, LOW);
      delay(50);
      digitalWrite(RED_LED_PIN, HIGH);
      delay(50);
      digitalWrite(RED_LED_PIN, LOW);
      delay(50);
      digitalWrite(RED_LED_PIN, HIGH);
      if(xdataDebug) {
        xdataSerial.println("Radio PA power set to 2mW (min)");
      }
    }
  }
  else if (btnCounter == 4) {
    if(rttyEnable != true) {
      rttyEnable = true;

      digitalWrite(GREEN_LED_PIN, LOW);
      delay(50);
      digitalWrite(GREEN_LED_PIN, HIGH);
      delay(50);
      digitalWrite(GREEN_LED_PIN, LOW);
      delay(50);
      digitalWrite(GREEN_LED_PIN, HIGH);
      delay(50);
      digitalWrite(GREEN_LED_PIN, LOW);
      delay(50);
      digitalWrite(GREEN_LED_PIN, HIGH);
      delay(50);
      digitalWrite(GREEN_LED_PIN, LOW);
      delay(50);
      digitalWrite(GREEN_LED_PIN, HIGH);
      
      if(xdataDebug) {
        xdataSerial.println("RTTY enabled");
      } 
    }
    else {
      rttyEnable = false;

      digitalWrite(RED_LED_PIN, LOW);
      delay(50);
      digitalWrite(RED_LED_PIN, HIGH);
      delay(50);
      digitalWrite(RED_LED_PIN, LOW);
      delay(50);
      digitalWrite(RED_LED_PIN, HIGH);
      delay(50);
      digitalWrite(RED_LED_PIN, LOW);
      delay(50);
      digitalWrite(RED_LED_PIN, HIGH);
      delay(50);
      digitalWrite(RED_LED_PIN, LOW);
      delay(50);
      digitalWrite(RED_LED_PIN, HIGH);

      if(xdataDebug) {
        xdataSerial.println("RTTY disabled");
      }
    }
  }
  else if (btnCounter == 5) {
    if(rttyShortenMsg != true) {
      rttyShortenMsg = true;

      digitalWrite(RED_LED_PIN, LOW);
      delay(50);
      digitalWrite(RED_LED_PIN, HIGH);
      delay(50);
      digitalWrite(RED_LED_PIN, LOW);
      delay(50);
      digitalWrite(RED_LED_PIN, HIGH);
      delay(50);
      digitalWrite(RED_LED_PIN, LOW);
      delay(50);
      digitalWrite(RED_LED_PIN, HIGH);
      delay(50);
      digitalWrite(RED_LED_PIN, LOW);
      delay(50);
      digitalWrite(RED_LED_PIN, HIGH);
      delay(50);
      digitalWrite(RED_LED_PIN, LOW);
      delay(50);
      digitalWrite(RED_LED_PIN, HIGH);

      if(xdataDebug) {
        xdataSerial.println("RTTY short messages format enabled");
      }
    }
    else {
      rttyShortenMsg = false;

      digitalWrite(GREEN_LED_PIN, LOW);
      delay(50);
      digitalWrite(GREEN_LED_PIN, HIGH);
      delay(50);
      digitalWrite(GREEN_LED_PIN, LOW);
      delay(50);
      digitalWrite(GREEN_LED_PIN, HIGH);
      delay(50);
      digitalWrite(GREEN_LED_PIN, LOW);
      delay(50);
      digitalWrite(GREEN_LED_PIN, HIGH);
      delay(50);
      digitalWrite(GREEN_LED_PIN, LOW);
      delay(50);
      digitalWrite(GREEN_LED_PIN, HIGH);
      delay(50);
      digitalWrite(GREEN_LED_PIN, LOW);
      delay(50);
      digitalWrite(GREEN_LED_PIN, HIGH);
      if(xdataDebug) {
        xdataSerial.println("RTTY standard messages format enabled");
      }
    }
  }
  else if (btnCounter == 6) {
    if(refHeatingMode == 0) { //if it is disabled
      refHeatingMode = 1; //set to auto

      digitalWrite(RED_LED_PIN, LOW);
      digitalWrite(GREEN_LED_PIN, LOW);
      delay(50);
      digitalWrite(RED_LED_PIN, HIGH);
      digitalWrite(GREEN_LED_PIN, HIGH);
      delay(50);
      digitalWrite(RED_LED_PIN, LOW);
      digitalWrite(GREEN_LED_PIN, LOW);
      delay(50);
      digitalWrite(RED_LED_PIN, HIGH);
      digitalWrite(GREEN_LED_PIN, HIGH);
      delay(50);
      digitalWrite(RED_LED_PIN, LOW);
      digitalWrite(GREEN_LED_PIN, LOW);
      delay(50);
      digitalWrite(RED_LED_PIN, HIGH);
      digitalWrite(GREEN_LED_PIN, HIGH);
      delay(50);
      digitalWrite(RED_LED_PIN, LOW);
      digitalWrite(GREEN_LED_PIN, LOW);
      delay(50);
      digitalWrite(RED_LED_PIN, HIGH);
      digitalWrite(GREEN_LED_PIN, HIGH);
      delay(50);
      digitalWrite(RED_LED_PIN, LOW);
      digitalWrite(GREEN_LED_PIN, LOW);
      delay(50);
      digitalWrite(RED_LED_PIN, HIGH);
      digitalWrite(GREEN_LED_PIN, HIGH);
      delay(50);
      digitalWrite(RED_LED_PIN, LOW);
      digitalWrite(GREEN_LED_PIN, LOW);
      delay(50);
      digitalWrite(RED_LED_PIN, HIGH);
      digitalWrite(GREEN_LED_PIN, HIGH);
      delay(50);

      if(xdataDebug) {
        xdataSerial.println("Reference heater - AUTO mode!");
      }
    }
    else if(refHeatingMode == 1) { //if set to auto
      refHeatingMode = 2; //set to always on

      digitalWrite(RED_LED_PIN, LOW);
      delay(50);
      digitalWrite(RED_LED_PIN, HIGH);
      delay(50);
      digitalWrite(RED_LED_PIN, LOW);
      delay(50);
      digitalWrite(RED_LED_PIN, HIGH);
      delay(50);
      digitalWrite(RED_LED_PIN, LOW);
      delay(50);
      digitalWrite(RED_LED_PIN, HIGH);
      delay(50);
      digitalWrite(RED_LED_PIN, LOW);
      delay(50);
      digitalWrite(RED_LED_PIN, HIGH);
      delay(50);
      digitalWrite(RED_LED_PIN, LOW);
      delay(50);
      digitalWrite(RED_LED_PIN, HIGH);
      delay(50);
      digitalWrite(RED_LED_PIN, LOW);
      delay(50);
      digitalWrite(RED_LED_PIN, HIGH);

      if(xdataDebug) {
        xdataSerial.println("Reference heater - ALWAYS ON!");
      }
    }
    else {
      refHeatingMode = 0; //turn heating off
      digitalWrite(GREEN_LED_PIN, LOW);
      delay(50);
      digitalWrite(GREEN_LED_PIN, HIGH);
      delay(50);
      digitalWrite(GREEN_LED_PIN, LOW);
      delay(50);
      digitalWrite(GREEN_LED_PIN, HIGH);
      delay(50);
      digitalWrite(GREEN_LED_PIN, LOW);
      delay(50);
      digitalWrite(GREEN_LED_PIN, HIGH);
      delay(50);
      digitalWrite(GREEN_LED_PIN, LOW);
      delay(50);
      digitalWrite(GREEN_LED_PIN, HIGH);
      delay(50);
      digitalWrite(GREEN_LED_PIN, LOW);
      delay(50);
      digitalWrite(GREEN_LED_PIN, HIGH);
      delay(50);
      digitalWrite(GREEN_LED_PIN, LOW);
      delay(50);
      digitalWrite(GREEN_LED_PIN, HIGH);

      if(xdataDebug) {
        xdataSerial.println("Reference heater - disabled");
      }
    }

    if(!heaterAvail) {
      if(xdataDebug) {
        xdataSerial.println("[WARN] HEATER IS NOT AVAILABLE ON THIS DEVICE!");
      }
    }
  }
  else if (btnCounter == 7) {
    digitalWrite(RED_LED_PIN, LOW);
    radioDisableTx();
    if(xdataDebug) {
      xdataSerial.println("\n PSU_SHUTDOWN_PIN set HIGH, bye!");
    }
    delay(3000);
    digitalWrite(PSU_SHUTDOWN_PIN, HIGH);
  } else {}

  btnCounter = 0;
}

void deviceStatusHandler() {
  // Temporary flags for each check
  vBatErr = false;
  vBatWarn = false;
  ovhtErr = false;
  ovhtWarn = false;
  gpsFixWarn = false;

  err = false;
  warn = false;
  ok = true; // Default to ok until proven otherwise

  // Evaluate battery voltage
  float vBat = readBatteryVoltage();
  if (vBat < vBatErrValue) {
      vBatErr = true;
  } else if (vBat < vBatWarnValue) {
      vBatWarn = true;
  }

  // Evaluate thermistor temperature
  float temp = readThermistorTemp();
  if (temp > ovhtErrValue && refHeatingMode == 0) {
      ovhtErr = true;
  } else if (temp > ovhtWarnValue && refHeatingMode == 0) {
      ovhtWarn = true;
  }

  // Evaluate GPS status
  if (gpsSats < gpsSatsWarnValue) {
      gpsFixWarn = true;
  }

  // Combine the results to determine the final state
  if (vBatErr || ovhtErr) {
      err = true;
      ok = false;
  } else if (vBatWarn || ovhtWarn || gpsFixWarn) {
      warn = true;
      ok = false;
  }

  // If no errors or warnings, set ok to true
  if (!err && !warn) {
      ok = true;
  }

  if(ok) {
    deviceDebugState = 0;
    deviceDebugState += heaterDebugState;
  }
  else if(warn) {
    deviceDebugState = 0;
    deviceDebugState += 100;
    deviceDebugState += heaterDebugState;
  }
  else {
    deviceDebugState = 0;
    deviceDebugState += 200;
    deviceDebugState += heaterDebugState;
  }

  if(deviceDebugState < 0 || deviceDebugState > 255) {
    deviceDebugState = 249;
  }


  if(ledStatusEnable) {
    if(gpsAlt > 3000) { //disable leds after launch
      ledsEnable = false;
    }
    else {
      ledsEnable = true;
    }

    if(ledsEnable) {
      if(err) {
        digitalWrite(GREEN_LED_PIN, HIGH);
        digitalWrite(RED_LED_PIN, LOW);
      }
      else if(warn) {
        digitalWrite(GREEN_LED_PIN, LOW);
        digitalWrite(RED_LED_PIN, LOW);
      }
      else if(ok) {
        digitalWrite(GREEN_LED_PIN, LOW);
        digitalWrite(RED_LED_PIN, HIGH);
      }
      else {
        digitalWrite(GREEN_LED_PIN, HIGH);
        digitalWrite(RED_LED_PIN, HIGH);
      }
    }

  }
  else {
    digitalWrite(GREEN_LED_PIN, HIGH);
    digitalWrite(RED_LED_PIN, HIGH);
  }

}

void serialStatusHandler() {
    if(xdataDebug) {
      if(vBatErr) {
        xdataSerial.println("[ERR]: vBatErr - critically low voltage");
      }

      if(vBatWarn) {
        xdataSerial.println("[WARN]: vBatWarn - low voltage");
      }

      if(ovhtErr) {
        xdataSerial.println("[ERR]: ovhtErr - internal temperature very high");
      }

      if(ovhtWarn) {
        xdataSerial.println("[WARN]: ovhtWarn - internal temperature high");
      }

      if(gpsFixWarn) {
        xdataSerial.println("[WARN]: gpsFix - gps isn't locked on position, give sky clearance, waiting for fix...");
      }

      if(ok) {
        xdataSerial.println("[ok]: Device working properly");
      }
  
  }
    
}


float readBatteryVoltage() {
  float batV;

  if(rsm4x4) { //12bit adc
    batV = ((float)analogRead(VBAT_PIN) / 4095) * 3 * 2 * batVFactor;
  }
  else { //10bit adc
    batV = ((float)analogRead(VBAT_PIN) / 1024) * 3 * 2 * batVFactor;
  }

  return batV;
}

float readThermistorTemp() {
  int adcValue = analogRead(REF_THERM);

  float voltage;

  // Convert ADC value to voltage
  if(rsm4x4) {
    voltage = adcValue * (3.0 / 4095);  // 3.0V reference, 12-bit ADC
  }
  else {
    voltage = adcValue * (3.0 / 1023); //10bit adc
  }


  // Convert voltage to thermistor resistance
  float resistance = (3.0 / voltage - 1) * R25;

  // Convert resistance to temperature using the Beta equation
  // Note: The Beta equation is T = 1 / ( (1 / T0) + (1 / B) * ln(R/R0) ) - 273.15
  // Adjust for correct temperature calculation

  float temperatureK = 1.0 / (1.0 / (25 + 273.15) + (1.0 / B) * log(R25 / resistance));
  float temperatureC = temperatureK - 273.15;

  return temperatureC;
}

uint8_t readRadioTemp() {
  // Read the ADC value from register 0x11
  uint8_t temp = readRegister(0x11);

  // Convert ADC value to signed temperature value
  int16_t temp_2 = -64 + ((int16_t)temp * 5) / 10;

  // Trigger ADC to capture another measurement by writing 0x80 to register 0x0F
  writeRegister(0x0F, 0x80);

  // Cast temperature value to int8_t and return
  return (uint8_t)temp_2;
}

void updateGpsData() {
  unsigned long start = millis();
  do 
  {
    while (gpsSerial.available())
      gps.encode(gpsSerial.read());
  } while (millis() - start < gpsNmeaMsgWaitTime);
  

  gpsTime = gps.time.value() / 100;
  gpsHours = gps.time.hour();
  gpsMinutes = gps.time.minute();
  gpsSeconds = gps.time.second();
  gpsLat = gps.location.lat();
  gpsLong = gps.location.lng();
  gpsAlt = gps.altitude.meters();
  gpsSpeed = gps.speed.mps();
  gpsSats = gps.satellites.value();

  if(xdataDebug) {
    xdataSerial.println("[info]: GPS data obtained and updated");
  }
}


void disableRefHeaterRing() {
  digitalWrite(PULLUP_TM, LOW); //disable temperature ring oscillator power
  digitalWrite(PULLUP_HYG, LOW); //disable humidity ring oscillator 
  if(heaterAvail) {
    digitalWrite(HEAT_REF, LOW); //disable reference heater
  }
}
  

void enableRefHeaterRing() {
  digitalWrite(PULLUP_TM, LOW); //enable temperature ring oscillator power
  digitalWrite(PULLUP_HYG, LOW); //enable humidity ring oscillator power
  if(heaterAvail) {
    digitalWrite(HEAT_REF, HIGH); //enable reference heater
  }
}


void refHeaterHandler() {
    unsigned long currentMillis = millis();
    float currentTemp = readThermistorTemp();

    // Mode 0: Heating Off
    if (refHeatingMode == 0) {
        disableRefHeaterRing(); // Ensure the heater is off

        isHeaterOn = false;
        heaterDebugState = 5;

        if(xdataDebug) {
          xdataSerial.println("[info]: Heating is completely OFF.");
        }
        
        return; // Exit early
    }

    // Mode 1: Auto Mode
    if (refHeatingMode == 1) {
        // Disable heater if temperature exceeds the critical disable threshold
        if (currentTemp > refHeaterCriticalDisableTemp) {
            disableRefHeaterRing(); // Turn off the heater

            if(xdataDebug) {
              xdataSerial.print("[ERR]: Heater has been turned OFF because temp exceeds threshold of (*C): ");
              xdataSerial.println(refHeaterCriticalDisableTemp);
            }
            
            heaterOffTime = currentMillis; // Record the time the heater was turned off

            isHeaterOn = false; // Update the heater state
            isHeaterPausedOvht = true; // Mark that the heater was paused due to overheating
            heaterDebugState = 19;

            return; // Exit early
        }

        // Re-enable the heater if it was paused due to overheating and the temperature drops below the re-enable threshold
        if (isHeaterPausedOvht && currentTemp <= refHeaterCriticalReenableTemp) {
          if(xdataDebug) {
            xdataSerial.print("[info]: Temperature dropped below re-enable threshold of (*C): ");
            xdataSerial.println(refHeaterCriticalReenableTemp);
          }
            
            isHeaterPausedOvht = false; // Clear the overheating flag

            enableRefHeaterRing(); // Turn on the heater

            if(xdataDebug) {
              xdataSerial.println("[info]: HEATER RE-ENABLED!");
            }

            isHeaterOn = true; // Update the heater state
            heaterDebugState = 11;

            return; // Exit early after re-enabling
        }

        // Disable heater if the timer condition is met
        if (isHeaterOn && currentMillis - heaterOnTime >= heaterWorkingTimeSec * 1000) {
            disableRefHeaterRing(); // Turn off the heater
            if(xdataDebug) {
              xdataSerial.print("[info]: Heater OFF due to timer, cooldown for (seconds): ");
              xdataSerial.println(heaterCooldownTimeSec);
            }

            heaterOffTime = currentMillis; // Record the time the heater was turned off
            isHeaterOn = false; // Update the heater state
            heaterDebugState = 10;
        }
        else if (isHeaterOn) {
          if(xdataDebug) {
            xdataSerial.print("[info]: Heater is currently ON, at (*C): ");
            xdataSerial.println(currentTemp);
          }

          heaterDebugState = 11;
        }
        else {
            // If the heater is off, check if cooldown has passed and if it should be re-enabled
            if (currentMillis - heaterOffTime >= heaterCooldownTimeSec * 1000) {
                if (currentTemp < autoHeaterThreshold) {
                  if(xdataDebug) {
                    xdataSerial.print("[info]: Cooldown time passed, thermistor temp is under threshold of (*C): ");
                    xdataSerial.println(autoHeaterThreshold);
                    xdataSerial.println("[info]: HEATER ON!");
                  }

                    enableRefHeaterRing(); // Turn on the heater

                    heaterOnTime = currentMillis; // Record the time the heater was turned on
                    isHeaterOn = true; // Update the heater state
                    heaterDebugState = 11;
                }
                else {
                  if(xdataDebug) {
                    xdataSerial.println("[info]: autoRefHeating is enabled, but the temperature is higher than the threshold.");
                  }
                    
                    heaterDebugState = 10;
                }
            }
        }
        return; // Exit early
    }

    // Mode 2: Heater Always On with Safety Control
    if (refHeatingMode == 2) {
        // Check if the temperature exceeds the critical disable threshold
        if (currentTemp > refHeaterCriticalDisableTemp) {
            disableRefHeaterRing(); // Turn off the heater

            if(xdataDebug) {
              xdataSerial.print("[ERR]: Heater has been turned OFF because temp exceeds threshold of (*C): ");
              xdataSerial.println(refHeaterCriticalDisableTemp);
            }
            
            heaterOffTime = currentMillis; // Record the time the heater was turned off
            isHeaterOn = false; // Update the heater state
            isHeaterPausedOvht = true; // Mark that the heater was paused due to overheating
            heaterDebugState = 29;

            return; // Exit early
        }

        // Re-enable the heater immediately after temperature drops below re-enable threshold
        if (isHeaterPausedOvht && currentTemp <= refHeaterCriticalReenableTemp) {
          if(xdataDebug) {
            xdataSerial.print("[info]: Temperature dropped below re-enable threshold of (*C): ");
            xdataSerial.println(refHeaterCriticalReenableTemp);
          }
            
            isHeaterPausedOvht = false; // Clear the overheating flag

            enableRefHeaterRing(); // Turn on the heater

            if(xdataDebug) {
              xdataSerial.println("[info]: HEATER RE-ENABLED!");
            }

            isHeaterOn = true; // Update the heater state
            heaterOnTime = currentMillis; // Reset the heater's working time
            heaterDebugState = 21;
        }
        else if (!isHeaterOn) {
            // If the heater is not on, ensure it is turned on
            enableRefHeaterRing(); // Turn on the heater

            if(xdataDebug) {
              xdataSerial.println("[info]: Heating is always ON with safety control.");
            }
            
            isHeaterOn = true; // Update the heater state
            heaterOnTime = currentMillis; // Reset the heater's working time
            heaterDebugState = 21;
        }
        return; // Exit early
    }
}


void powerHandler() {
  if(readBatteryVoltage() < batteryCutOffVoltage) {
    digitalWrite(RED_LED_PIN, LOW);
    delay(100);
    digitalWrite(RED_LED_PIN, HIGH);
    delay(100);
    digitalWrite(RED_LED_PIN, LOW);
    delay(100);
    digitalWrite(RED_LED_PIN, HIGH);
    delay(100);
    digitalWrite(RED_LED_PIN, LOW);
    delay(100);
    digitalWrite(RED_LED_PIN, HIGH);
    delay(100);
    digitalWrite(RED_LED_PIN, LOW);
    delay(100);
    digitalWrite(RED_LED_PIN, HIGH);
    delay(100);
    digitalWrite(RED_LED_PIN, LOW);

    setRadioModulation(0);  // CW modulation
    setRadioFrequency(rttyFrequencyMhz);

    if(xdataDebug) {
      xdataSerial.println("[ERR] BATTERY CUT-OFF VOLTAGE, SYSTEM WILL POWER OFF");
    }

    radioEnableTx();
    setRadioSmallOffset(RTTY_RADIO_MARK_OFFSET);
    delay(250); //mark character idle
    sendRTTYPacket("\n\n VBAT-CUTOFF VBAT-CUTOFF VBAT-CUTOFF");
    radioDisableTx();

    if(xdataDebug) {
      xdataSerial.println("\n PSU_SHUTDOWN_PIN set HIGH, bye!");
    }
    
    delay(250);
    digitalWrite(PSU_SHUTDOWN_PIN, HIGH);
  }

}


void sensorBoomHandler() { //for the future if the sensor circuitry will work
  digitalWrite(SPST1, LOW);
  digitalWrite(SPST2, LOW);
  digitalWrite(SPST3, LOW);
  digitalWrite(SPST4, LOW);  
}




void setup() {
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);  
  pinMode(PSU_SHUTDOWN_PIN, OUTPUT);
  pinMode(CS_RADIO_SPI, OUTPUT);
  if(heaterAvail) {
    pinMode(HEAT_REF, OUTPUT);
  }
  pinMode(PULLUP_TM, OUTPUT);
  pinMode(PULLUP_HYG, OUTPUT);
  pinMode(SPST1, OUTPUT);
  pinMode(SPST2, OUTPUT);
  pinMode(SPST3, OUTPUT);
  pinMode(SPST4, OUTPUT);
  
  /*digitalWrite(PULLUP_TM, LOW); //disable temperature ring oscillator power
  digitalWrite(PULLUP_HYG, LOW); //disable humidity ring oscillator power
  digitalWrite(HEAT_REF, LOW); //disable reference heater*/
  disableRefHeaterRing(); //does all three commands above

  digitalWrite(SPST1, LOW);
  digitalWrite(SPST2, LOW);
  digitalWrite(SPST3, LOW);
  digitalWrite(SPST4, LOW);
  
  digitalWrite(GREEN_LED_PIN, HIGH);
  digitalWrite(RED_LED_PIN, LOW);
  delay(100);
  digitalWrite(RED_LED_PIN, HIGH);
  digitalWrite(GREEN_LED_PIN, LOW);
  delay(100);
  digitalWrite(RED_LED_PIN, HIGH);
  digitalWrite(GREEN_LED_PIN, HIGH);

  xdataSerial.begin(115200);
  if(rsm4x4){
    gpsSerial.begin(gpsBaudRate);
  }
  else if(rsm4x2) {
    gpsSerial.begin(gpsBaudRate);
  }
  
  if(xdataDebug) {
    xdataSerial.println("[info]: Serial interfaces initialized");
  }

  if(rsm4x4) {
    analogReadResolution(12);
    if(xdataDebug) {
      xdataSerial.println("[info]: ADC resolution set to 12 bits");
    }
  }
  

  SPI_2.begin();
  digitalWrite(CS_RADIO_SPI, HIGH);  // Deselect the SI4432 CS pin
  if(xdataDebug) {
    xdataSerial.println("[info]: SPI_2 interface initialized");
  }

  digitalWrite(CS_RADIO_SPI, LOW);
  init_RFM22();
  if(xdataDebug) {
    xdataSerial.println("[info]: Si4032 radio register initialization complete");
  }
  //digitalWrite(CS_RADIO_SPI, HIGH); //no need to disable cs because no other spi devices on the bus

  setRadioPower(radioPwrSetting);
  if(xdataDebug) {
    xdataSerial.print("[info]: Si4032 PA power set to (pre-config): ");
    xdataSerial.println(radioPwrSetting);
  }
  
  writeRegister(0x72, 0x07);  //deviation ~12khz, currently no use because of manual cw modes (rtty morse pip)

  if(xdataDebug) {
    xdataSerial.println("[info]: Si4032 deviation set to 0x07, not used for now?...");
  }
  
  fsk4_bitDuration = (uint32_t)1000000/horusBdr; //horus 100baud delay calculation


  digitalWrite(GREEN_LED_PIN, LOW);
  delay(50);
  digitalWrite(GREEN_LED_PIN, HIGH);
  delay(200);
  digitalWrite(GREEN_LED_PIN, LOW);
  delay(50);
  digitalWrite(GREEN_LED_PIN, HIGH);
  delay(200);

  if(xdataDebug) {
    xdataSerial.println("[info]: Exiting setup...");
  }
}


void loop() {
  buttonHandler();
  deviceStatusHandler(); 
  serialStatusHandler();
  refHeaterHandler();
  powerHandler();
  //sensorBoomHandler();

  if (pipEnable) {
    if(xdataDebug) {
      xdataSerial.println("[info]: PIP mode enabled");
    }
    
    if (radioEnablePA) {
      setRadioModulation(0);
      setRadioFrequency(pipFrequencyMhz);

      if(xdataDebug) {
        xdataSerial.print("[info]: PIP on (MHz): ");
        xdataSerial.println(pipFrequencyMhz);
        xdataSerial.println("[info]: Transmitting PIP...");
      }      

      for (txRepeatCounter; txRepeatCounter < pipRepeat; txRepeatCounter++) {
        radioEnableTx();

        if(xdataDebug) {
          xdataSerial.print("pip ");
        }
        
        delay(pipLengthMs);
        buttonHandler();
        deviceStatusHandler();

        radioDisableTx();
        delay(pipLengthMs);
        buttonHandler();
        deviceStatusHandler();
      }

      if(xdataDebug) {
        xdataSerial.println("[info]: PIP TX done");
      }
      
      txRepeatCounter = 0;
    }
    else {
      if(xdataDebug) {
        xdataSerial.println("[info]: radioEnablePA false, won't transmit");
      }
    }

    if(modeChangeDelay == 0) {}
    else if(modeChangeDelay != 0 && modeChangeDelay < 1000) {
      delay(modeChangeDelay);
      if(xdataDebug) {
        xdataSerial.print("[info]: modeChangeDelay enabled, waiting for: ");
        xdataSerial.println(modeChangeDelay);
      }
    }
    else {
      if(xdataDebug) {
        xdataSerial.print("[info]: modeChangeDelay enabled, waiting for: ");
        xdataSerial.println(modeChangeDelay);
      }
      
      if(radioSleep) {
        radioInhibitTx();

        if(xdataDebug) {
          xdataSerial.println("[info]: Radio sleep");
        }

      }

      delay(modeChangeDelay);
    }
  }

  buttonHandler();
  deviceStatusHandler();

  if (morseEnable) {
    if(xdataDebug) {
      xdataSerial.println("[info]: Morse mode enabled");
    }

    if (radioEnablePA) {
      morseMsg = createRttyShortPayload();
      const char* morseMsgCstr = morseMsg.c_str();
      if(xdataDebug) {
        xdataSerial.println("[info]: Morse payload created: ");
        xdataSerial.println(morseMsg);
        xdataSerial.println();
      }
      
      setRadioModulation(0);  // CW modulation
      setRadioFrequency(morseFrequencyMhz);

      if(xdataDebug) {
        xdataSerial.print("[info]: Morse transmitting on (MHz): ");
        xdataSerial.println(morseFrequencyMhz);
      }

      if(xdataDebug) {
        xdataSerial.println("[info]: Transmitting morse...");
      }
      
      transmitMorseString(morseMsgCstr, morseUnitTime);
      radioDisableTx();

      if(xdataDebug) {
      xdataSerial.println("[info]: Morse TX done");
      }
      
    }
    else {
      if(xdataDebug) {
        xdataSerial.println("[info]: radioEnablePA false, won't transmit");
      }
      
    }


    if(modeChangeDelay == 0) {}
      else if(modeChangeDelay != 0 && modeChangeDelay < 1000) {
        delay(modeChangeDelay);
        if(xdataDebug) {
          xdataSerial.print("[info]: modeChangeDelay enabled, waiting for: ");
          xdataSerial.println(modeChangeDelay);
        }
      }
      else {
        if(xdataDebug) {
          xdataSerial.print("[info]: modeChangeDelay enabled, waiting for: ");
          xdataSerial.println(modeChangeDelay);
        }
        
        if(radioSleep) {
          radioInhibitTx();

          if(xdataDebug) {
            xdataSerial.println("[info]: Radio sleep");
          }

        }

        delay(modeChangeDelay);
      }

  }

  buttonHandler();
  deviceStatusHandler();


  if (rttyEnable) {
    if(xdataDebug) {
      xdataSerial.println("[info]: RTTY mode enabled");
    }
    
    if (radioEnablePA) {
      if(rttyShortenMsg) {
        rttyMsg = createRttyShortPayload();
      }
      else {
        rttyMsg = createRttyPayload();
      }
      
      const char* rttyMsgCstr = rttyMsg.c_str();
      if(xdataDebug) {
        xdataSerial.print("[info]: RTTY payload created: ");
        xdataSerial.println(rttyMsg);
        xdataSerial.println("");
      }
      

      setRadioModulation(0);  // CW modulation
      setRadioFrequency(rttyFrequencyMhz);
      if(xdataDebug) {
        xdataSerial.print("[info]: RTTY frequency set to (MHz): ");
        xdataSerial.println(rttyFrequencyMhz);
      }
      

      radioEnableTx();

      if(xdataDebug) {
        xdataSerial.println("[info]: Transmitting RTTY");
      }
      
      setRadioSmallOffset(RTTY_RADIO_MARK_OFFSET);
      delay(250); //mark character idle
      sendRTTYPacket(rttyMsgCstr);
      sendRTTYPacket("\n\n");
      radioDisableTx();

      if(xdataDebug) {
        xdataSerial.println("[info]: RTTY TX done");
      }
      
    }
    else {
      if(xdataDebug) {
        xdataSerial.println("[info]: radioEnablePA false, won' transmit");
      }
    }
  
    if(modeChangeDelay == 0) {}
    else if(modeChangeDelay != 0 && modeChangeDelay < 1000) {
      delay(modeChangeDelay);
      if(xdataDebug) {
        xdataSerial.print("[info]: modeChangeDelay enabled, waiting for: ");
        xdataSerial.println(modeChangeDelay);
      }
    }
    else {
      if(xdataDebug) {
        xdataSerial.print("[info]: modeChangeDelay enabled, waiting for: ");
        xdataSerial.println(modeChangeDelay);
      }
      
      if(radioSleep) {
        radioInhibitTx();

        if(xdataDebug) {
          xdataSerial.println("[info]: Radio sleep");
        }

      }

      delay(modeChangeDelay);
    }
  }

  buttonHandler();
  deviceStatusHandler();

  if (horusEnable) {
    if(xdataDebug) {
      xdataSerial.println("[info]: HORUS mode enabled");
    }
    
    if (radioEnablePA) {
      int pkt_len = build_horus_binary_packet_v2(rawbuffer);
      int coded_len = horus_l2_encode_tx_packet((unsigned char*)codedbuffer,(unsigned char*)rawbuffer,pkt_len);

      if(xdataDebug) {
        xdataSerial.print("[info]: HORUS payload created.");

        xdataSerial.print(F("Uncoded Length (bytes): "));
        xdataSerial.println(pkt_len);
        xdataSerial.print("Uncoded: ");
        PrintHex(rawbuffer, pkt_len, debugbuffer);
        xdataSerial.println(debugbuffer);
        xdataSerial.print(F("Encoded Length (bytes): "));
        xdataSerial.println(coded_len);
        xdataSerial.print("Coded: ");
        PrintHex(codedbuffer, coded_len, debugbuffer);
        xdataSerial.println(debugbuffer);
      }
      

      setRadioModulation(0);  // CW modulation
      setRadioFrequency(horusFrequencyMhz);
      if(xdataDebug) {
        xdataSerial.print("[info]: HORUS frequency set to (MHz): ");
        xdataSerial.println(horusFrequencyMhz);
      }
      
      if(xdataDebug) {
        xdataSerial.println("[info]: Transmitting HORUS");
      }
      
      radioEnableTx();

      fsk4_idle();
      delay(1000);
      fsk4_preamble(8);
      fsk4_write(codedbuffer, coded_len);

      radioDisableTx();

      if(xdataDebug) {
        xdataSerial.println("[info]: HORUS TX done");
      }
      
    }
    else {
      if(xdataDebug) {
        xdataSerial.println("[info]: radioEnablePA false, won' transmit");
      }
    }
    if(modeChangeDelay == 0) {}
    else if(modeChangeDelay != 0 && modeChangeDelay < 1000) {
      delay(modeChangeDelay);
      if(xdataDebug) {
        xdataSerial.print("[info]: modeChangeDelay enabled, waiting for: ");
        xdataSerial.println(modeChangeDelay);
      }
    }
    else {
      if(xdataDebug) {
        xdataSerial.print("[info]: modeChangeDelay enabled, waiting for: ");
        xdataSerial.println(modeChangeDelay);
      }
      
      if(radioSleep) {
        radioInhibitTx();

        if(xdataDebug) {
          xdataSerial.println("[info]: Radio sleep");
        }

      }

      delay(modeChangeDelay);
    }
  }

  buttonHandler();
  deviceStatusHandler();

  if (bufTxEnable) {
    if(xdataDebug) {
      xdataSerial.println("[info]: bufTx mode enabled");
    }
    
    if (radioEnablePA) {
      setRadioModulation(2);
      setRadioFrequency(fskFrequencyMhz);
      if(xdataDebug) {
        xdataSerial.print("[info]: Transmitting on (MHz): ");
        xdataSerial.println(fskFrequencyMhz);
      }
      
      txBuf();
      
      if(xdataDebug) {
        xdataSerial.println("[info]: TX done.");
      }
      
    }
  }

  if(radioSleep) {
    radioInhibitTx();

    if(xdataDebug) {
      xdataSerial.println("[info]: Radio sleep");
    }

    if(modeChangeDelay == 0) {}
    else if(modeChangeDelay != 0 && modeChangeDelay < 1000) {
      delay(modeChangeDelay);
      if(xdataDebug) {
        xdataSerial.print("[info]: modeChangeDelay enabled, waiting for: ");
        xdataSerial.println(modeChangeDelay);
      }
    }
    else {
      if(xdataDebug) {
        xdataSerial.print("[info]: modeChangeDelay enabled, waiting for: ");
        xdataSerial.println(modeChangeDelay);
      }
      
      if(radioSleep) {
        radioInhibitTx();

        if(xdataDebug) {
          xdataSerial.println("[info]: Radio sleep");
        }

      }

      delay(modeChangeDelay);
    }

  }

  buttonHandler();
  deviceStatusHandler();

  updateGpsData();

  if(xdataDebug) { //only for development
    /*xdataSerial.println("[info]: DEBUG DATA: ");
    xdataSerial.print("Lat, long: ");
    xdataSerial.print(String(gpsLat, 6));
    xdataSerial.print(" ,  ");
    xdataSerial.print(String(gpsLong, 6));
    xdataSerial.print("\nSat: ");
    xdataSerial.print(gpsSats);
    xdataSerial.print("\nTime: ");
    xdataSerial.print(gpsTime);
    xdataSerial.print("\nSpeed: ");
    xdataSerial.print(gpsSpeed);
    xdataSerial.print("\nAltitude: ");
    xdataSerial.print(gpsAlt);
    xdataSerial.print("\n");

    xdataSerial.print(readBatteryVoltage());
    xdataSerial.print("\n");
    xdataSerial.print(readThermistorTemp());
    xdataSerial.print("\n");
    xdataSerial.print(readRadioTemp());
    xdataSerial.print("\n\n");

    xdataSerial.println(String(rttyMsg));

    xdataSerial.println(analogRead(VBTN_PIN));
    xdataSerial.println(analogRead(VBAT_PIN));
    xdataSerial.println("\n\n\n");*/
  }
  
  

  if (!radioEnablePA) {
    delay(500);

    if(xdataDebug) {
      xdataSerial.println("[info]: Radio PA not enabled, sleep for 500ms");
    }
  }

  if(xdataDebug) {
    xdataSerial.println("\n\n");
  }
  
}
