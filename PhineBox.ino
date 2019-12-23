/*
 * PhineBox 2019  
 *  
 * This project is based on the Tonuino project, downloaded in September 2019
 * 
 * To make it easier on our kids to use, we added the following:
 * (1) Six-button interface: Play/pause, skip forward, skip backward, mute (speakers only), louder, quieter
 * (2) One indicator LED to show the device is on and idle (fading lights) or playing (light on)
 * (3) Automatic (timed) switch-off and a start-button to turn it on (it is battery-powered/backed)
 * 
 * The MP3-player with a PAM8403 amplifier board is giving me a hard time - changing power supplies has not changed the
 * horrible noises coming from it, and playing with capacitors (low-pass) or resistors (a load to the output otherwise 
 * fine for straight-up headphones - which are working great btw - ) has not helped either
 * 
 * My attempt of cabling in USB-interface to the DFplayer failed. I assume that some high frequency issues caused 
 * the connection to not be made, for the computer just never responded to anything attached to D+/D- wires within 
 * the cable while noticing the USB-plug to go in (by an audible bing). Suggestions are very welcome, for now taking
 * the card out when content is added will need to do. According to some sources, the DFplayer works like any USB-drive
 * once the connectors are made to a USB port.
 * 
 * --- Aufgaben: 
 * 1. Lautstärkebegrenzung im Array von Kopfhörer und Lautsprechern abhängig machen
 * 2. Softes Umschalten (Lautstärke leise, äquivalente Lautstärke im Zielausgabegerät?
 * 3. Googlen: Probleme Anschaltung PAM8403 mit DFplayer ...
 * 5. Info: GND zweimal vom PAM8403 abziehen schaltet diesen auch ab.
 * 
 * V 0.3 - 10Nov2019 - h_i
 */

#include <DFMiniMp3.h>
#include <EEPROM.h>
#include <JC_Button.h>
#include <MFRC522.h>
#include <SPI.h>
#include <SoftwareSerial.h>

// DFPlayer Mini
SoftwareSerial mySoftwareSerial(2, 3); // RX, TX
uint16_t numTracksInFolder;
uint16_t currentTrack;

// this object stores nfc tag data
struct nfcTagObject {
  uint32_t cookie;
  uint8_t version;
  uint8_t folder;
  uint8_t mode;
  uint8_t special;
};

nfcTagObject myCard;

static void nextTrack(uint16_t track);
int voiceMenu(int numberOfOptions, int startMessage, int messageOffset,
              bool preview = false, int previewFromFolder = 0);

bool knownCard = false;

// implement a notification class,
// its member methods will get called
//
class Mp3Notify {
public:
  static void OnError(uint16_t errorCode) {
    // see DfMp3_Error for code meaning
    Serial.println();
    Serial.print("Com Error ");
    Serial.println(errorCode);
  }
  static void OnPlayFinished(uint16_t track) {
    Serial.print("Track beendet");
    Serial.println(track);
    delay(100);
    nextTrack(track);
  }
  static void OnCardOnline(uint16_t code) {
    Serial.println(F("SD Karte online "));
  }
  static void OnCardInserted(uint16_t code) {
    Serial.println(F("SD Karte bereit "));
  }
  static void OnCardRemoved(uint16_t code) {
    Serial.println(F("SD Karte entfernt "));
  }
  static void OnUsbOnline(uint16_t code) {
      Serial.println(F("USB online "));
  }
  static void OnUsbInserted(uint16_t code) {
      Serial.println(F("USB bereit "));
  }
  static void OnUsbRemoved(uint16_t code) {
    Serial.println(F("USB entfernt "));
  }
};

static DFMiniMp3<SoftwareSerial, Mp3Notify> mp3(mySoftwareSerial);

// Leider kann das Modul keine Queue abspielen.
static uint16_t _lastTrackFinished;
static void nextTrack(uint16_t track) {
  if (track == _lastTrackFinished) {
    return;
   }
   _lastTrackFinished = track;
   
   if (knownCard == false)
    // Wenn eine neue Karte angelernt wird soll das Ende eines Tracks nicht
    // verarbeitet werden
    return;

  if (myCard.mode == 1) {
    Serial.println(F("Hörspielmodus ist aktiv -> keinen neuen Track spielen"));
//    mp3.sleep(); // Je nach Modul kommt es nicht mehr zurück aus dem Sleep!
  }
  if (myCard.mode == 2) {
    if (currentTrack != numTracksInFolder) {
      currentTrack = currentTrack + 1;
      mp3.playFolderTrack(myCard.folder, currentTrack);
      Serial.print(F("Albummodus ist aktiv -> nächster Track: "));
      Serial.print(currentTrack);
    } else 
//      mp3.sleep();   // Je nach Modul kommt es nicht mehr zurück aus dem Sleep!
    { }
  }
  if (myCard.mode == 3) {
    uint16_t oldTrack = currentTrack;
    currentTrack = random(1, numTracksInFolder + 1);
    if (currentTrack == oldTrack)
      currentTrack = currentTrack == numTracksInFolder ? 1 : currentTrack+1; 
      /* bedeutet eigentlich ("syntatic sugar")
       * if(currentTrack == numTracksInFolder) {currentTrack = 1;}
       * else{currentTrack++;}
       * 
       */
    Serial.print(F("Party Modus ist aktiv -> zufälligen Track spielen: "));
    Serial.println(currentTrack);
    mp3.playFolderTrack(myCard.folder, currentTrack);
  }
  if (myCard.mode == 4) {
    Serial.println(F("Einzel Modus aktiv -> Strom sparen"));
//    mp3.sleep();      // Je nach Modul kommt es nicht mehr zurück aus dem Sleep!
  }
  if (myCard.mode == 5) {
    if (currentTrack != numTracksInFolder) {
      currentTrack = currentTrack + 1;
      Serial.print(F("Hörbuch Modus ist aktiv -> nächster Track und "
                     "Fortschritt speichern"));
      Serial.println(currentTrack);
      mp3.playFolderTrack(myCard.folder, currentTrack);
      // Fortschritt im EEPROM abspeichern
      EEPROM.write(myCard.folder, currentTrack);
    } 
    else {
//      mp3.sleep();  // Je nach Modul kommt es nicht mehr zurück aus dem Sleep!
      // Fortschritt zurück setzen
      EEPROM.write(myCard.folder, 1);
    }
  }
}

static void previousTrack() {
  if (myCard.mode == 1) {
    Serial.println(F("Hörspielmodus ist aktiv -> Track von vorne spielen"));
    mp3.playFolderTrack(myCard.folder, currentTrack);
  }
  if (myCard.mode == 2) {
    Serial.println(F("Albummodus ist aktiv -> vorheriger Track"));
    if (currentTrack != 1) {
      currentTrack = currentTrack - 1;
    }
    mp3.playFolderTrack(myCard.folder, currentTrack);
  }
  if (myCard.mode == 3) {
    Serial.println(F("Party Modus ist aktiv -> Track von vorne spielen"));
    mp3.playFolderTrack(myCard.folder, currentTrack);
  }
  if (myCard.mode == 4) {
    Serial.println(F("Einzel Modus aktiv -> Track von vorne spielen"));
    mp3.playFolderTrack(myCard.folder, currentTrack);
  }
  if (myCard.mode == 5) {
    Serial.println(F("Hörbuch Modus ist aktiv -> vorheriger Track und "
                     "Fortschritt speichern"));
    if (currentTrack != 1) {
      currentTrack = currentTrack - 1;
    }
    mp3.playFolderTrack(myCard.folder, currentTrack);
    // Fortschritt im EEPROM abspeichern
    EEPROM.write(myCard.folder, currentTrack);
  }
}

// MFRC522
#define RST_PIN 9                 // Configurable, see typical pin layout above
#define SS_PIN 10                 // Configurable, see typical pin layout above

MFRC522 mfrc522(SS_PIN, RST_PIN); // Create MFRC522
MFRC522::MIFARE_Key key;
bool successRead;
byte sector = 1;
byte blockAddr = 4;
byte trailerBlock = 7;
MFRC522::StatusCode status;

// buttons
#define buttonPause A0  // blau
#define buttonUp A1     // schwarz
#define buttonDown A2   // weiß
#define buttonNext A3   // grün
#define buttonLast A4   // rot
#define buttonMute A5   // gelb

#define busyPin 4       // liest die blaue LED am MP3-Spieler/zeigt, dass ein Stück gespielt wird
#define SystemOn 6      // Ausgang für das Relais, der das Gerät "an" lässt (grün)
#define MuteRelay 5     // Ausgang für das Relais, das die Lautsprecher stummschaltet (gelb)
#define IndicatorLED 7  // Ausgang für die Anzeige-LED

/*  A6 und A7 können laut arduino.cc nicht für digitale Ports verwendet werden. Als Eingang lassen sie 
 *   sich nur analog lesen, hat zwischendurch gut funktioniert mit 100kOhm-Pullup und Auswertung der
 *   Spannung. Haben die geplanten LEDs nach außen aus optischen Gründen nicht realisiert, dann waren
 *   die Pins wieder frei :-)
 *  
 *  Wichtig: Hier weichen wir von der Skizze auf der TonUINO-Seite ab, was die Anschlüsse angeht!
*/

#define RESET_VOLUME 10     // Lautstärke darauf zurücksetzen bei Start und Umschalten
#define MAXIMUM_VOLUME 30   // Lautstärkebegrenzung, System startet mit RESET_VOLUME (maximal 30 im System)
#define MIMIMUM_VOLUME 5    // untere Lautstärkebegrenzung, so dass ein laufendes Stück niemals "weg" ist

#define LONG_PRESS 600

Button pauseButton(buttonPause);
Button upButton(buttonUp);
Button downButton(buttonDown);
// --- Hier neue Einträge, drei Zeilen ---
Button nextButton(buttonNext);
Button lastButton(buttonLast);
ToggleButton muteButton(buttonMute);

bool ignorePauseButton = false;
bool ignoreUpButton = false;
bool ignoreDownButton = false;
bool SystemOffTimerStarted = false;

// Die sind auch neu
//bool LidIsOpen = true;
unsigned long TimeOut = 900000;     // Abschaltung nach 15 Minuten ohne Benutzung
unsigned long LastUsed = millis();  // Aktuelle Zeit der letzten Benutzung

uint8_t numberOfCards = 0;
uint8_t currentVolume = RESET_VOLUME;

bool isPlaying() { return !digitalRead(busyPin); }

void setup() {
  // Relais zum Eingeschaltet-lassen initialisieren und anziehen
  pinMode(SystemOn, OUTPUT);
  digitalWrite(SystemOn, LOW);

  // Relais für Stummschaltung Lautsprecher initialisieren, aber nicht anziehen
  pinMode(MuteRelay, OUTPUT);
  digitalWrite(MuteRelay, HIGH);

  // Debug-Ausgaben über die serielle Schnittstelle aktivieren (bleibt im Code)
  Serial.begin(115200); 
  randomSeed(analogRead(A6)); // Zufallsgenerator initialisieren

  Serial.println(F("PhineBox 0.1, based on TonUINO Version 2.0"));
  Serial.println(F("built by Hipster_Inside, (c) original code Thorsten Voß"));

  // Knöpfe mit PullUp
  pinMode(buttonPause, INPUT_PULLUP);
  pinMode(buttonUp, INPUT_PULLUP);
  pinMode(buttonDown, INPUT_PULLUP);
  pinMode(buttonNext, INPUT_PULLUP);
  pinMode(buttonLast, INPUT_PULLUP);
  pinMode(buttonMute, INPUT_PULLUP);

  // Busy Pin
  pinMode(busyPin, INPUT);

  // DFPlayer Mini initialisieren
  mp3.begin();
  mp3.setVolume(RESET_VOLUME);
// --- vorherige Zeile: Startlautstärke neu einstellen

  // NFC Leser initialisieren
  SPI.begin();        // Init SPI bus
  mfrc522.PCD_Init(); // Init MFRC522
  mfrc522
      .PCD_DumpVersionToSerial(); // Show details of PCD - MFRC522 Card Reader
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }

  // RESET --- Pause & lauter & leiser beim STARTEN GEDRÜCKT HALTEN -> alle bekannten
  // Karten werden gelöscht
  if (digitalRead(buttonPause) == LOW && digitalRead(buttonUp) == LOW &&
      digitalRead(buttonDown) == LOW) {
    Serial.println(F("Reset -> EEPROM wird gelöscht"));
    for (int i = 0; i < EEPROM.length(); i++) {
      EEPROM.write(i, 0);
    }
  }

}

void loop() {
  do {
    mp3.loop();
    // Buttons werden nun über JS_Button gehandelt, dadurch kann jede Taste
    // doppelt belegt werden
    pauseButton.read();
    upButton.read();
    downButton.read();
    nextButton.read();
    lastButton.read();
    muteButton.read();  // mute = ToggleButton


    if(isPlaying()) {      
      LastUsed = millis();
      if (SystemOffTimerStarted) {
        SystemOffTimerStarted = false;
        Serial.println(F("System-Abschalttimer beendet"));
      }
    }
    else if(!SystemOffTimerStarted) {
      SystemOffTimerStarted = true;
      Serial.println(F("System-Abschalttimer gestartet"));
    }

    if (millis() > LastUsed + TimeOut) {
      Serial.println(F("System abgeschaltet durch TimeOut"));
      digitalWrite(SystemOn, HIGH);       
    }

    // Spiel/Pause-Knopf (Spiel, Pause, ???bei langem Druck Ansage der Titelnummer --- NOCHMAL ANALYSIEREN
    if (pauseButton.wasReleased()) {
      if (ignorePauseButton == false) {
        if (isPlaying()) {
          mp3.pause();
          Serial.println(F("-- Pause --"));
        }  
        else {
          mp3.start();
          Serial.println(F("Spiele MP3 ab ..."));
        }
      }
      ignorePauseButton = false;
    } 
    else if (pauseButton.pressedFor(LONG_PRESS) &&
               ignorePauseButton == false) {
      if (isPlaying()) {
        mp3.playAdvertisement(currentTrack);
        Serial.println(F("... ist ins isPlaying() reingelaufen ... keine Ahnung warum ..."));
      }
      else {
        knownCard = false;
        mp3.playMp3FolderTrack(800);
        Serial.println(F("Karte resetten..."));
        resetCard();
        mfrc522.PICC_HaltA();
        mfrc522.PCD_StopCrypto1();
      }
      ignorePauseButton = true;
    }

    // Lauter- und Leiseknöpfe (lauter, leiser, gemeinsam = mittlere Lautstärke)
    if (upButton.pressedFor(LONG_PRESS) && downButton.pressedFor(LONG_PRESS)) {
      mp3.setVolume(RESET_VOLUME);
      Serial.println(F("Lautstärke zurückgesetzt"));
    }
    else if (upButton.pressedFor(LONG_PRESS) || upButton.wasReleased()) {
      currentVolume = mp3.getVolume();
      if (currentVolume >= MAXIMUM_VOLUME) {
        Serial.print(F("Volume maxed at "));
      } 
      else {
        Serial.print(F("Volume up to "));
        mp3.increaseVolume();
        currentVolume = mp3.getVolume();    
      }
      Serial.println(currentVolume);   // Die Einfassung mit F() musste weg, geht scheinbar mit uint_t nicht.
      ignoreUpButton = true;
    } 
    else if (downButton.pressedFor(LONG_PRESS) || downButton.wasReleased()) {
      currentVolume = mp3.getVolume();
      if (currentVolume == 1) {
        Serial.print(F("Leiser geht es nicht, ist schon "));        
      }
      else {
        Serial.print(F("Volume down to "));
        mp3.decreaseVolume();
        currentVolume = mp3.getVolume();
      }
      Serial.println(currentVolume);
      ignoreDownButton = true;
    } 

    // Nächster- und Letzter-Knopf (grün und rot), gemeinsam = ausschalten
    if (nextButton.pressedFor(LONG_PRESS) && lastButton.pressedFor(LONG_PRESS)) {
      Serial.println(F("System ausgeschaltet durch Tastenkombination rot-grün"));
      digitalWrite(SystemOn, HIGH);
    }
    else if (nextButton.pressedFor(LONG_PRESS) || nextButton.wasPressed()) {
      Serial.println(F("nächstes Stück"));
      nextTrack(random(65536));
    }
    else if (lastButton.pressedFor(LONG_PRESS) || lastButton.wasPressed()) {
      Serial.println(F("zurück/nochmal anfangen"));
      previousTrack();
    }   

    // Mute-Knopf (ist ein Toggle-Button, siehe Beschreibung JC_Button)
    if (muteButton.changed()) {
      if(muteButton.toggleState() == false) {
        digitalWrite(MuteRelay, HIGH);
        Serial.println(F("Die Lautsprecher sind jetzt wieder an"));
      }
      else {
        digitalWrite(MuteRelay, LOW);
        Serial.println(F("Die Lautsprecher sind jetzt stumm"));
      }
    }
       
    // Ende der Buttons
  } while (!mfrc522.PICC_IsNewCardPresent());

  // RFID Karte wurde aufgelegt

  if (!mfrc522.PICC_ReadCardSerial())
    return;

  if (readCard(&myCard) == true) {
    if (myCard.cookie == 322417479 && myCard.folder != 0 && myCard.mode != 0) {

      knownCard = true;
      _lastTrackFinished = 0;
      numTracksInFolder = mp3.getFolderTrackCount(myCard.folder);
      Serial.print(numTracksInFolder);
      Serial.print(F(" Dateien in Ordner "));
      Serial.println(myCard.folder);

      // Hörspielmodus: eine zufällige Datei aus dem Ordner
      if (myCard.mode == 1) {
        Serial.println(F("Hörspielmodus -> zufälligen Track wiedergeben"));
        currentTrack = random(1, numTracksInFolder + 1);
        Serial.println(currentTrack);
        mp3.playFolderTrack(myCard.folder, currentTrack);
      }
      // Album Modus: kompletten Ordner spielen
      if (myCard.mode == 2) {
        Serial.println(F("Album Modus -> kompletten Ordner wiedergeben"));
        currentTrack = 1;
        mp3.playFolderTrack(myCard.folder, currentTrack);
      }
      // Party Modus: Ordner in zufälliger Reihenfolge
      if (myCard.mode == 3) {
        Serial.println(
            F("Party Modus -> Ordner in zufälliger Reihenfolge wiedergeben"));
        currentTrack = random(1, numTracksInFolder + 1);
        mp3.playFolderTrack(myCard.folder, currentTrack);
      }
      // Einzel Modus: eine Datei aus dem Ordner abspielen
      if (myCard.mode == 4) {
        Serial.println(
            F("Einzel Modus -> eine Datei aus dem Ordner abspielen"));
        currentTrack = myCard.special;
        mp3.playFolderTrack(myCard.folder, currentTrack);
      }
      // Hörbuch Modus: kompletten Ordner spielen und Fortschritt merken
      if (myCard.mode == 5) {
        Serial.println(F("Hörbuch Modus -> kompletten Ordner spielen und "
                         "Fortschritt merken"));
        currentTrack = EEPROM.read(myCard.folder);
        mp3.playFolderTrack(myCard.folder, currentTrack);
      }
    }

    // Neue Karte konfigurieren
    else {
      knownCard = false;
      setupCard();
    }
  }
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}

int voiceMenu(int numberOfOptions, int startMessage, int messageOffset,
              bool preview = false, int previewFromFolder = 0) {
  int returnValue = 0;
  if (startMessage != 0)
    mp3.playMp3FolderTrack(startMessage);
  do {
    pauseButton.read();
    upButton.read();
    downButton.read();
    mp3.loop();
    if (pauseButton.wasPressed()) {
      if (returnValue != 0)
        return returnValue;
      delay(1000);
    }

    if (upButton.pressedFor(LONG_PRESS)) {
      returnValue = min(returnValue + 10, numberOfOptions);
      mp3.playMp3FolderTrack(messageOffset + returnValue);
      delay(1000);
      if (preview) {
        do {
          delay(10);
        } while (isPlaying());
        if (previewFromFolder == 0)
          mp3.playFolderTrack(returnValue, 1);
        else
          mp3.playFolderTrack(previewFromFolder, returnValue);
      }
      ignoreUpButton = true;
    } else if (upButton.wasReleased()) {
      if (!ignoreUpButton) {
        returnValue = min(returnValue + 1, numberOfOptions);
        mp3.playMp3FolderTrack(messageOffset + returnValue);
        delay(1000);
        if (preview) {
          do {
            delay(10);
          } while (isPlaying());
          if (previewFromFolder == 0)
            mp3.playFolderTrack(returnValue, 1);
          else
            mp3.playFolderTrack(previewFromFolder, returnValue);
        }
      } else
        ignoreUpButton = false;
    }
    
    if (downButton.pressedFor(LONG_PRESS)) {
      returnValue = max(returnValue - 10, 1);
      mp3.playMp3FolderTrack(messageOffset + returnValue);
      delay(1000);
      if (preview) {
        do {
          delay(10);
        } while (isPlaying());
        if (previewFromFolder == 0)
          mp3.playFolderTrack(returnValue, 1);
        else
          mp3.playFolderTrack(previewFromFolder, returnValue);
      }
      ignoreDownButton = true;
    } else if (downButton.wasReleased()) {
      if (!ignoreDownButton) {
        returnValue = max(returnValue - 1, 1);
        mp3.playMp3FolderTrack(messageOffset + returnValue);
        delay(1000);
        if (preview) {
          do {
            delay(10);
          } while (isPlaying());
          if (previewFromFolder == 0)
            mp3.playFolderTrack(returnValue, 1);
          else
            mp3.playFolderTrack(previewFromFolder, returnValue);
        }
      } else
        ignoreDownButton = false;
    }
  } while (true);
}

void resetCard() {
  do {
    pauseButton.read();
    upButton.read();
    downButton.read();

    if (upButton.wasReleased() || downButton.wasReleased()) {
      Serial.print(F("Abgebrochen!"));
      mp3.playMp3FolderTrack(802);
      return;
    }
  } while (!mfrc522.PICC_IsNewCardPresent());

  if (!mfrc522.PICC_ReadCardSerial())
    return;

  Serial.print(F("Karte wird neu Konfiguriert!"));
  setupCard();
}

void setupCard() {
  mp3.pause();
  Serial.print(F("Neue Karte konfigurieren"));

  // Ordner abfragen
  myCard.folder = voiceMenu(99, 300, 0, true);

  // Wiedergabemodus abfragen
  myCard.mode = voiceMenu(6, 310, 310);

  // Hörbuchmodus -> Fortschritt im EEPROM auf 1 setzen
  EEPROM.write(myCard.folder,1);

  // Einzelmodus -> Datei abfragen
  if (myCard.mode == 4)
    myCard.special = voiceMenu(mp3.getFolderTrackCount(myCard.folder), 320, 0,
                               true, myCard.folder);

  // Admin Funktionen
  if (myCard.mode == 6)
    myCard.special = voiceMenu(3, 320, 320);

  // Karte ist konfiguriert -> speichern
  mp3.pause();
  writeCard(myCard);
}

bool readCard(nfcTagObject *nfcTag) {
  bool returnValue = true;
  // Show some details of the PICC (that is: the tag/card)
  Serial.print(F("Card UID:"));
  dump_byte_array(mfrc522.uid.uidByte, mfrc522.uid.size);
  Serial.println();
  Serial.print(F("PICC type: "));
  MFRC522::PICC_Type piccType = mfrc522.PICC_GetType(mfrc522.uid.sak);
  Serial.println(mfrc522.PICC_GetTypeName(piccType));

  byte buffer[18];
  byte size = sizeof(buffer);

  // Authenticate using key A
  Serial.println(F("Authenticating using key A..."));
  status = (MFRC522::StatusCode)mfrc522.PCD_Authenticate(
      MFRC522::PICC_CMD_MF_AUTH_KEY_A, trailerBlock, &key, &(mfrc522.uid));
  if (status != MFRC522::STATUS_OK) {
    returnValue = false;
    Serial.print(F("PCD_Authenticate() failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    return;
  }

  // Show the whole sector as it currently is
  Serial.println(F("Current data in sector:"));
  mfrc522.PICC_DumpMifareClassicSectorToSerial(&(mfrc522.uid), &key, sector);
  Serial.println();

  // Read data from the block
  Serial.print(F("Reading data from block "));
  Serial.print(blockAddr);
  Serial.println(F(" ..."));
  status = (MFRC522::StatusCode)mfrc522.MIFARE_Read(blockAddr, buffer, &size);
  if (status != MFRC522::STATUS_OK) {
    returnValue = false;
    Serial.print(F("MIFARE_Read() failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
  }
  Serial.print(F("Data in block "));
  Serial.print(blockAddr);
  Serial.println(F(":"));
  dump_byte_array(buffer, 16);
  Serial.println();
  Serial.println();

  uint32_t tempCookie;
  tempCookie = (uint32_t)buffer[0] << 24;
  tempCookie += (uint32_t)buffer[1] << 16;
  tempCookie += (uint32_t)buffer[2] << 8;
  tempCookie += (uint32_t)buffer[3];

  nfcTag->cookie = tempCookie;
  nfcTag->version = buffer[4];
  nfcTag->folder = buffer[5];
  nfcTag->mode = buffer[6];
  nfcTag->special = buffer[7];

  return returnValue;
}

void writeCard(nfcTagObject nfcTag) {
  MFRC522::PICC_Type mifareType;
  byte buffer[16] = {0x13, 0x37, 0xb3, 0x47, // 0x1337 0xb347 magic cookie to
                                             // identify our nfc tags
                     0x01,                   // version 1
                     nfcTag.folder,          // the folder picked by the user
                     nfcTag.mode,    // the playback mode picked by the user
                     nfcTag.special, // track or function for admin cards
                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

  byte size = sizeof(buffer);

  mifareType = mfrc522.PICC_GetType(mfrc522.uid.sak);

  // Authenticate using key B
  Serial.println(F("Authenticating again using key B..."));
  status = (MFRC522::StatusCode)mfrc522.PCD_Authenticate(
      MFRC522::PICC_CMD_MF_AUTH_KEY_B, trailerBlock, &key, &(mfrc522.uid));
  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("PCD_Authenticate() failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    mp3.playMp3FolderTrack(401);
    return;
  }

  // Write data to the block
  Serial.print(F("Writing data into block "));
  Serial.print(blockAddr);
  Serial.println(F(" ..."));
  dump_byte_array(buffer, 16);
  Serial.println();
  status = (MFRC522::StatusCode)mfrc522.MIFARE_Write(blockAddr, buffer, 16);
  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("MIFARE_Write() failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
      mp3.playMp3FolderTrack(401);
  }
  else
    mp3.playMp3FolderTrack(400);
  Serial.println();
  delay(100);
}

/**
 * Helper routine to dump a byte array as hex values to Serial.
 */
void dump_byte_array(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}
