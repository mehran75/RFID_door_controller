/*
   --------------------------------------------------------------------------------------------------------------------
   Example sketch/program showing how to read data from a PICC to serial.
   --------------------------------------------------------------------------------------------------------------------
   This is a MFRC522 library example; for further details and other examples see: https://github.com/miguelbalboa/rfid

   Example sketch/program showing how to read data from a PICC (that is: a RFID Tag or Card) using a MFRC522 based RFID
   Reader on the Arduino SPI interface.

   When the Arduino and the MFRC522 module are connected (see the pin layout below), load this sketch into Arduino IDE
   then verify/compile and upload it. To see the output: use Tools, Serial Monitor of the IDE (hit Ctrl+Shft+M). When
   you present a PICC (that is: a RFID Tag or Card) at reading distance of the MFRC522 Reader/PCD, the serial output
   will show the ID/UID, type and any data blocks it can read. Note: you may see "Timeout in communication" messages
   when removing the PICC from reading distance too early.

   If your reader supports it, this sketch/program will read all the PICCs presented (that is: multiple tag reading).
   So if you stack two or more PICCs on top of each other and present them to the reader, it will first output all
   details of the first and then the next PICC. Note that this may take some time as all data blocks are dumped, so
   keep the PICCs at reading distance until complete.

   @license Released into the public domain.

   Typical pin layout used:
   -----------------------------------------------------------------------------------------
               MFRC522      Arduino       Arduino   Arduino    Arduino          Arduino
               Reader/PCD   Uno/101       Mega      Nano v3    Leonardo/Micro   Pro Micro
   Signal      Pin          Pin           Pin       Pin        Pin              Pin
   -----------------------------------------------------------------------------------------
   RST/Reset   RST          7             5         D9         RESET/ICSP-5     RST
   SPI SS      SDA(SS)      10            53        D10        10               10
   SPI MOSI    MOSI         11 / ICSP-4   51        D11        ICSP-4           16
   SPI MISO    MISO         12 / ICSP-1   50        D12        ICSP-1           14
   SPI SCK     SCK          13 / ICSP-3   52        D13        ICSP-3           15
*/

#include <SPI.h>
#include <MFRC522.h>
#include <EEPROM.h>
#include "pitches.h"


#define RST_PIN         7          // Configurable, see typical pin layout above
#define SS_PIN          10         // Configurable, see typical pin layout above


/// OUTPUTs
#define LED_GRANTED A0
#define LED_DENYED  A1
#define RELAY_TRIGGER 5
#define BUZZER 9

///INPUTs
#define PROGRAM_BTN 4
#define BUZZER_BTN 3

MFRC522 mfrc522(SS_PIN, RST_PIN);  // Create MFRC522 instance


/// EEPROM
int count;          // count of stored tags
byte readCard[4];   // Stores scanned ID read from RFID Module

/// State
int programMode = HIGH;
bool buzzer_toggle = false;


void setup() {

  ///Output pins
  pinMode(LED_GRANTED, OUTPUT);
  pinMode(LED_DENYED, OUTPUT);
  pinMode(RELAY_TRIGGER, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  ///Input pins
  pinMode(PROGRAM_BTN, INPUT_PULLUP);
  pinMode(BUZZER_BTN, INPUT_PULLUP);



  digitalWrite(LED_GRANTED, LOW);
  digitalWrite(LED_DENYED, LOW);
  digitalWrite(RELAY_TRIGGER, HIGH);
  /// start initialization

  Serial.begin(9600);   // Initialize serial communications with the PC
  while (!Serial);    // Do nothing if no serial port is opened (added for Arduinos based on ATMEGA32U4)
  SPI.begin();      // Init SPI bus
  //SPI.setClockDivider(SPI_CLOCK_DIV8);

  mfrc522.PCD_Init();   // Init MFRC522
  delay(50);       // Optional delay. Some board do need more time after init to be ready, see Readme

  mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max);
  ShowReaderDetails();

  Serial.println(F("Scan PICC to see UID, SAK, type, and data blocks..."));

  loadAndCheckEEPROM();



  delay(1000);
  /// end initialization
  digitalWrite(LED_GRANTED, HIGH);
  digitalWrite(LED_DENYED, HIGH);
}

void loop() {

  programMode = digitalRead(PROGRAM_BTN);

  if (programMode == LOW) {

    programModeLED();
    if (getID() == 1) {
      if (storeNewTag()) {
        successLED();
      } else {
        failureLED();
      }
    }


  } else {
    
    if (getID() == 1) {
      if (idExists(readCard)) {
        granted();
        openDoor();
      } else {
        denyed();
      }
    }



    if (digitalRead(BUZZER_BTN) == LOW) {
      if (buzzer_toggle == false) {
        tone(BUZZER, NOTE_F5);
        digitalWrite(LED_GRANTED, LOW);
        buzzer_toggle = true;
      }
    } else {
      buzzer_toggle = false;
      digitalWrite(LED_GRANTED, HIGH);
      noTone(BUZZER);
    }
  }

  delay(100);
}


void loadAndCheckEEPROM() {

  //    for (int i = 0; i < EEPROM.length(); i++) {
  //     EEPROM.write(i, 255);
  //    }

  count = EEPROM.read(0);
  if (count == 255) {
    EEPROM.write(0, 0);
    count = EEPROM.read(0);
  }
  Serial.print("\nStored Tags: ");
  Serial.print(count);


  Serial.println("\n-----------TAGs----------");
  for (int i = 0; i < count; i++) {
    for (int j = 0; j < 4; j++) {
      Serial.print(EEPROM.read((i * 4) + j + 1), HEX);
    }
    Serial.println("");
  }
  Serial.println("---------------------------");

}


uint8_t getID() {

  // Getting ready for Reading PICCs
  if ( ! mfrc522.PICC_IsNewCardPresent()) { //If a new PICC placed to RFID reader continue
    return 0;
  }

  if ( ! mfrc522.PICC_ReadCardSerial()) {   //Since a PICC placed get Serial and continue
    return 0;
  }

  // There are Mifare PICCs which have 4 byte or 7 byte UID care if you use 7 byte PICC
  // I think we should assume every PICC as they have 4 byte UID
  // Until we support 7 byte PICCs
  Serial.println(F("Scanned PICC's UID:"));
  for ( uint8_t i = 0; i < 4; i++) {  //
    readCard[i] = mfrc522.uid.uidByte[i];
    Serial.print(readCard[i], HEX);
  }

  mfrc522.PICC_HaltA(); // Stop reading
  return 1;
}


void ShowReaderDetails() {
  // Get the MFRC522 software version
  byte v = mfrc522.PCD_ReadRegister(mfrc522.VersionReg);
  Serial.print(F("MFRC522 Software Version: 0x"));
  Serial.print(v, HEX);
  if (v == 0x91)
    Serial.print(F(" = v1.0"));
  else if (v == 0x92)
    Serial.print(F(" = v2.0"));
  else
    Serial.print(F(" (unknown),probably a chinese clone?"));
  Serial.println("");
  // When 0x00 or 0xFF is returned, communication probably failed
  if ((v == 0x00) || (v == 0xFF)) {
    Serial.println(F("WARNING: Communication failure, is the MFRC522 properly connected?"));
    Serial.println(F("SYSTEM HALTED: Check connections."));
    // Visualize system is halted
    //    digitalWrite(greenLed, LED_OFF);  // Make sure green LED is off
    //   digitalWrite(blueLed, LED_OFF);   // Make sure blue LED is off
    //    digitalWrite(redLed, LED_ON);   // Turn on red LED
    while (true); // do not go further
  }
}


bool bytesEquals ( byte a[], byte b[] ) {
  for ( uint8_t k = 0; k < 4; k++ ) {   // Loop 4 times
    if ( a[k] != b[k] ) {     // IF a != b then false, because: one fails, all fail
      return false;
    }
  }
  return true;
}


bool idExists(byte tag[]) {

  Serial.println("Checking if Tag exists");

  for (int i = 0; i < count; i++) {
    byte tmpTag[4];

    for (int j = 0; j < 4; j++) {
      tmpTag[j] = EEPROM.read((i * 4) + j + 1);
    }

    if (bytesEquals(tag, tmpTag)) {
      Serial.println("Tag exists");
      return true;
    }
  }

  return false;
}


bool storeNewTag() {
  Serial.println("Saving new tag");

  if (!idExists(readCard)) {     // Before we write to the EEPROM, check to see if we have seen this card before!

    int start = (count) * 4;
    for ( int i = 0; i < 4; i++ ) {
      Serial.println(start + i + 1);
      EEPROM.write(start + i + 1, readCard[i]);
    }

    EEPROM.write(0, count + 1);
    count += 1;

    Serial.println(F("Succesfully added ID record to EEPROM"));

    return true;
  }

  Serial.println(F("Failed! There is something wrong with ID or bad EEPROM"));
  return false;
}


void openDoor() {
  Serial.println("OpenDoor");
  digitalWrite(RELAY_TRIGGER, LOW);
  delay(1500);
  digitalWrite(RELAY_TRIGGER, HIGH);
}

void granted() {
  digitalWrite(LED_GRANTED, LOW);
  delay(150);
  digitalWrite(LED_GRANTED, HIGH);
  delay(150);
  digitalWrite(LED_GRANTED, LOW);
  delay(150);
  digitalWrite(LED_GRANTED, HIGH);
}


void denyed() {
  digitalWrite(LED_DENYED, LOW);
  delay(150);
  digitalWrite(LED_DENYED, HIGH);
  delay(150);
  digitalWrite(LED_DENYED, LOW);
  delay(150);
  digitalWrite(LED_DENYED, HIGH);
}


void programModeLED() {

  digitalWrite(LED_DENYED, LOW);
  digitalWrite(LED_GRANTED, LOW);
  delay(150);
  digitalWrite(LED_DENYED, HIGH);
  digitalWrite(LED_GRANTED, HIGH);
  delay(150);
  digitalWrite(LED_DENYED, LOW);
  digitalWrite(LED_GRANTED, LOW);
  delay(150);
  digitalWrite(LED_DENYED, HIGH);
  digitalWrite(LED_GRANTED, HIGH);
  delay(150);

}

void successLED() {
  digitalWrite(LED_GRANTED, LOW);
  delay(1000);
  digitalWrite(LED_GRANTED, HIGH);
  delay(1000);
}

void failureLED() {
  digitalWrite(LED_DENYED, LOW);
  delay(1000);
  digitalWrite(LED_DENYED, HIGH);
  delay(1000);
}


void RING() {

  if (digitalRead(BUZZER_BTN == LOW)){
  Serial.println("RING");
  digitalWrite(LED_DENYED, LOW);
  digitalWrite(LED_GRANTED, LOW);
  delay(150);
  digitalWrite(LED_DENYED, HIGH);
  digitalWrite(LED_GRANTED, HIGH);


  
  tone(BUZZER, NOTE_F5);
  } else {
    noTone(BUZZER);
  }
}
