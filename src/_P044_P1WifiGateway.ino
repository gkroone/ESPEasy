#ifdef USES_P044
//#################################### Plugin 044: P1WifiGateway ########################################
//
//  based on P020 Ser2Net, extended by Ronald Leenes romix/-at-/macuser.nl
//
//  designed for combo
//    Wemos D1 mini (see http://wemos.cc) and
//    P1 wifi gateway shield (see https://circuits.io/circuits/2460082)
//    see http://romix.macuser.nl for kits
//#######################################################################################################

#define PLUGIN_044
#define PLUGIN_ID_044         44
#define PLUGIN_NAME_044       "Communication - P1 Wifi Gateway"
//#define PLUGIN_VALUENAME1_044 "P1WifiGateway"
#define PLUGIN_VALUENAME1_044 "T1"
#define PLUGIN_VALUENAME2_044 "T2"
#define PLUGIN_VALUENAME3_044 "T1d"
#define PLUGIN_VALUENAME4_044 "T2d"
#define PLUGIN_VALUENAME5_044 "Win"
#define PLUGIN_VALUENAME6_044 "Wout"
#define PLUGIN_VALUENAME7_044 "Gas"
#define PLUGIN_VALUENAME8_044 "Volt"

#define STATUS_LED 12
#define P044_BUFFER_SIZE 1024
#define NETBUF_SIZE 600
#define DISABLED 0
#define WAITING 1
#define READING 2
#define CHECKSUM 3
#define DONE 4

boolean Plugin_044_init = false;
boolean serialdebug = false;
char* Plugin_044_serial_buf;
unsigned int bytes_read = 0;
boolean CRCcheck = false;
unsigned int currCRC = 0;
int checkI = 0;
int sendskipcounter=1;
long startupdelay=20;
long startupcounter=0;

WiFiServer *P1GatewayServer;
WiFiClient P1GatewayClient;

struct obis {
 byte a;
 byte b;
 byte c;
 byte d;
 byte e;
 byte f;
};

float wattsin=0;
float wattsout=0;
String volts;
String Value0;
String Value1;
String Value2;
String Value3;
String Value4;
String Value5;
String Value6;
String Value7;

boolean Plugin_044(byte function, struct EventStruct *event, String& string)
{
  boolean success = false;
  static byte connectionState = 0;
  static int state = DISABLED;

  switch (function)
  {

    case PLUGIN_DEVICE_ADD:
      {
        Device[++deviceCount].Number = PLUGIN_ID_044;
        Device[deviceCount].Type = DEVICE_TYPE_SINGLE;
        Device[deviceCount].VType = SENSOR_TYPE_HEXA;
        Device[deviceCount].SendDataOption = true;
        Device[deviceCount].ValueCount = 8;
        Device[deviceCount].Custom = true;
        Device[deviceCount].TimerOption = false;
        Device[deviceCount].GlobalSyncOption = true;
        break;
      }

    case PLUGIN_GET_DEVICENAME:
      {
        string = F(PLUGIN_NAME_044);
        break;
      }

    case PLUGIN_GET_DEVICEVALUENAMES:
      {
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[0], PSTR(PLUGIN_VALUENAME1_044));
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[1], PSTR(PLUGIN_VALUENAME2_044));
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[2], PSTR(PLUGIN_VALUENAME3_044));
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[4], PSTR(PLUGIN_VALUENAME5_044));
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[3], PSTR(PLUGIN_VALUENAME4_044));
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[5], PSTR(PLUGIN_VALUENAME6_044));
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[6], PSTR(PLUGIN_VALUENAME7_044));
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[7], PSTR(PLUGIN_VALUENAME8_044));
        break;
      }

    case PLUGIN_WEBFORM_LOAD:
      {
      	addFormNumericBox(F("TCP Port"), F("plugin_044_port"), ExtraTaskSettings.TaskDevicePluginConfigLong[0]);
      	addFormNumericBox(F("Baud Rate"), F("plugin_044_baud"), ExtraTaskSettings.TaskDevicePluginConfigLong[1]);
      	addFormNumericBox(F("Data bits"), F("plugin_044_data"), ExtraTaskSettings.TaskDevicePluginConfigLong[2]);


        byte choice = ExtraTaskSettings.TaskDevicePluginConfigLong[3];
        String options[3];
        options[0] = F("No parity");
        options[1] = F("Even");
        options[2] = F("Odd");
        int optionValues[3] = { 0, 2, 3 };
        addFormSelector(F("Parity"), F("plugin_044_parity"), 3, options, optionValues, choice);

      	addFormNumericBox(F("Stop bits"), F("plugin_044_stop"), ExtraTaskSettings.TaskDevicePluginConfigLong[4]);

        addFormPinSelect(F("Reset target after boot"), F("taskdevicepin1"), Settings.TaskDevicePin1[event->TaskIndex]);

        addFormNumericBox(F("RX Receive Timeout (mSec)"), F("plugin_044_rxwait"), Settings.TaskDevicePluginConfig[event->TaskIndex][0]);
//P1 meter provides data for multiple domoticz idx devices. currently focus on gas, power and voltage. Amps may also be present, but is ignored.
//      	addFormNumericBox(F("G idx"), F("plugin_044_gas"), ExtraTaskSettings.TaskDevicePluginConfigLong[5]);
//      	addFormNumericBox(F("E idx"), F("plugin_044_elec"), ExtraTaskSettings.TaskDevicePluginConfigLong[6]);
//      	addFormNumericBox(F("V idx"), F("plugin_044_volt"), ExtraTaskSettings.TaskDevicePluginConfigLong[7]);
        success = true;
        break;
      }

    case PLUGIN_WEBFORM_SAVE:
      {
        ExtraTaskSettings.TaskDevicePluginConfigLong[0] = getFormItemInt(F("plugin_044_port"));
        ExtraTaskSettings.TaskDevicePluginConfigLong[1] = getFormItemInt(F("plugin_044_baud"));
        ExtraTaskSettings.TaskDevicePluginConfigLong[2] = getFormItemInt(F("plugin_044_data"));
        ExtraTaskSettings.TaskDevicePluginConfigLong[3] = getFormItemInt(F("plugin_044_parity"));
        ExtraTaskSettings.TaskDevicePluginConfigLong[4] = getFormItemInt(F("plugin_044_stop"));
        //ExtraTaskSettings.TaskDevicePluginConfigLong[5] = getFormItemInt(F("plugin_044_gas"));
        //ExtraTaskSettings.TaskDevicePluginConfigLong[6] = getFormItemInt(F("plugin_044_elec"));
        //ExtraTaskSettings.TaskDevicePluginConfigLong[7] = getFormItemInt(F("plugin_044_volt"));
        Settings.TaskDevicePluginConfig[event->TaskIndex][0] = getFormItemInt(F("plugin_044_rxwait"));

        success = true;
        break;
      }

    case PLUGIN_INIT:
      {
        pinMode(STATUS_LED, OUTPUT);
        digitalWrite(STATUS_LED, 0);

        LoadTaskSettings(event->TaskIndex);
        if ((ExtraTaskSettings.TaskDevicePluginConfigLong[0] != 0) && (ExtraTaskSettings.TaskDevicePluginConfigLong[1] != 0))
        {
          #if defined(ESP8266)
            byte serialconfig = 0x10;
          #endif
          #if defined(ESP32)
            uint32_t serialconfig = 0x8000010;
          #endif
          serialconfig += ExtraTaskSettings.TaskDevicePluginConfigLong[3];
          serialconfig += (ExtraTaskSettings.TaskDevicePluginConfigLong[2] - 5) << 2;
          //serialconfig += ExtraTaskSettings.TaskDevicePluginConfigLong[3];
          if (ExtraTaskSettings.TaskDevicePluginConfigLong[4] == 2)
            serialconfig += 0x20;
          #if defined(ESP8266)
            Serial.begin(ExtraTaskSettings.TaskDevicePluginConfigLong[1], (SerialConfig)serialconfig);
          #endif
          #if defined(ESP32)
            Serial.begin(ExtraTaskSettings.TaskDevicePluginConfigLong[1], serialconfig);
          #endif
          if (P1GatewayServer) P1GatewayServer->close();
          P1GatewayServer = new WiFiServer(ExtraTaskSettings.TaskDevicePluginConfigLong[0]);
          P1GatewayServer->begin();

          if (!Plugin_044_serial_buf)
            Plugin_044_serial_buf = (char *)malloc(P044_BUFFER_SIZE);

          if (Settings.TaskDevicePin1[event->TaskIndex] != -1)
          {
            pinMode(Settings.TaskDevicePin1[event->TaskIndex], OUTPUT);
            digitalWrite(Settings.TaskDevicePin1[event->TaskIndex], LOW);
            delay(500);
            digitalWrite(Settings.TaskDevicePin1[event->TaskIndex], HIGH);
            pinMode(Settings.TaskDevicePin1[event->TaskIndex], INPUT_PULLUP);
          }

          Plugin_044_init = true;
        }

        blinkLED();

        if (ExtraTaskSettings.TaskDevicePluginConfigLong[1] == 115200) {
          addLog(LOG_LEVEL_DEBUG, F("P1   : DSMR version 4 meter, CRC on"));
          CRCcheck = true;
        } else {
          addLog(LOG_LEVEL_DEBUG, F("P1   : DSMR version 4 meter, CRC off"));
          CRCcheck = false;
        }


        state = WAITING;
        success = true;
        break;
      }

    case PLUGIN_EXIT:
      {
        if (P1GatewayServer) {
          P1GatewayServer->close();
          //FIXME: shouldnt P1P1GatewayServer be deleted?
          P1GatewayServer = NULL;
        }
        success = true;
        break;
      }

    case PLUGIN_TEN_PER_SECOND:
      {
        if (Plugin_044_init)
        {
         unsigned int bytes_read;
          if (P1GatewayServer->hasClient())
          {
            if (P1GatewayClient) P1GatewayClient.stop();
            P1GatewayClient = P1GatewayServer->available();
            addLog(LOG_LEVEL_ERROR, F("P1   : Client connected!"));
          }

          if (P1GatewayClient.connected() or ( startupcounter++ > startupdelay ) )
          {
            connectionState = 1;
            startupcounter=startupcounter+2;//ensure we keep running after initial startupdelay
            uint8_t net_buf[P044_BUFFER_SIZE];
            int count = P1GatewayClient.available();
            if (count > 0)
            {
              if (count > P044_BUFFER_SIZE)
                count = P044_BUFFER_SIZE;
              bytes_read = P1GatewayClient.read(net_buf, count);
              Serial.write(net_buf, bytes_read);
              Serial.flush(); // Waits for the transmission of outgoing serial data to complete

              if (count == P044_BUFFER_SIZE) // if we have a full buffer, drop the last position to stuff with string end marker
              {
                count--;
                // and log buffer full situation
                addLog(LOG_LEVEL_ERROR, F("P1   : Error: network buffer full!"));
              }
              net_buf[count] = 0; // before logging as a char array, zero terminate the last position to be safe.
              char log[P044_BUFFER_SIZE + 40];
              sprintf_P(log, PSTR("P1   : Error: N>: %s"), (char*)net_buf);
              addLog(LOG_LEVEL_DEBUG, log);
            }
          }
          else
          {
            if (connectionState == 1) // there was a client connected before...
            {
              connectionState = 0;
              addLog(LOG_LEVEL_ERROR, F("P1   : Client disconnected!"));
            }

            while (Serial.available())
              Serial.read();
          }

          success = true;
        }
        break;
      }

    case PLUGIN_SERIAL_IN:
      {
        if (Plugin_044_init)
        {
          if (P1GatewayClient.connected() or ( startupcounter > startupdelay ) )
          {
            int RXWait = Settings.TaskDevicePluginConfig[event->TaskIndex][0];
            if (RXWait == 0)
              RXWait = 1;
            int timeOut = RXWait;
            while (timeOut > 0)
            {
              while (Serial.available() && state != DONE) {
                if (bytes_read < P044_BUFFER_SIZE - 5) {
                  char  ch = Serial.read();
                  digitalWrite(STATUS_LED, 1);
                  switch (state) {
                    case DISABLED: //ignore incoming data
                      break;
                    case WAITING:
                      if (ch == '/')  {
                        Plugin_044_serial_buf[0] = ch;
                        bytes_read=1;
                        state = READING;
                      } // else ignore data
                      break;
                    case READING:
                      if (ch == '!') {
                        if (CRCcheck) {
                          state = CHECKSUM;
                        } else {
                          state = DONE;
                        }
                      }
                      if (validP1char(ch)) {
                        Plugin_044_serial_buf[bytes_read] = ch;
                        bytes_read++;
                      } else if (ch=='/') {
                        addLog(LOG_LEVEL_DEBUG, F("P1   : Error: Start detected, discarded input."));
                        Plugin_044_serial_buf[0] = ch;
                        bytes_read = 1;
                      } else {              // input is non-ascii
                        addLog(LOG_LEVEL_DEBUG, F("P1   : Error: DATA corrupt, discarded input."));
                        Serial.flush();
                        bytes_read = 0;
                        state = WAITING;
                      }
                      break;
                    case CHECKSUM:
                      checkI ++;
                      if (checkI == 4) {
                        checkI = 0;
                        state = DONE;
                      }
                      Plugin_044_serial_buf[bytes_read] = ch;
                      bytes_read++;
                      break;
                    case DONE:
                      // Plugin_044_serial_buf[bytes_read]= '\n';
                      // bytes_read++;
                      // Plugin_044_serial_buf[bytes_read] = 0;
                      break;
                  }
                }
                else
                {
                  Serial.read();      // when the buffer is full, just read remaining input, but do not store...
                  bytes_read = 0;
                  state = WAITING;    // reset
                }
                digitalWrite(STATUS_LED, 0);
                timeOut = RXWait; // if serial received, reset timeout counter
              }
              delay(1);
              timeOut--;
            }

            if (state == DONE) {
              if (checkDatagram(bytes_read)) {
                Plugin_044_serial_buf[bytes_read] = '\r';
                bytes_read++;
                Plugin_044_serial_buf[bytes_read] = '\n';
                bytes_read++;
                Plugin_044_serial_buf[bytes_read] = 0;

              //if an idx was provided to send data to domoticz for Electricity, then we'll assume we're configured and can go into this section:
              if ( ExtraTaskSettings.TaskDevicePluginConfigLong[6] >0 and sendskipcounter < 1 )
              {
                //Start of additions to send values to domoticz
                //define some local variables:
                //char mlinebuffer[1028];
                struct obis po;
                char Value[10][12];
                char linebuffer[128];
                String log = F("P1Wifi: copying data bytes_read '");

                u_int pc=0;

                for ( u_int lc = 0 ;lc < strlen(Plugin_044_serial_buf); lc++ )
                {
                  if (Plugin_044_serial_buf[lc] !='\n' and  Plugin_044_serial_buf[lc] != '\0' ){
                    linebuffer[pc++]=Plugin_044_serial_buf[lc];
                  }
                  else
                  {
                    pc=0;
                    // exclude line with serial nr where start character is missing.
                    if (linebuffer[0] != '!' and linebuffer[0] != '/' and linebuffer[0] != 'X' and linebuffer[0] != ' ' and linebuffer[0] != '\n' and linebuffer[1] != '\n')
                    {
                      //get the values from the buffered data
                      int i = GetObis(linebuffer, &po);
                      // 1-0:1.8.1(00180.726*kWh) T1
                      if (CompareObis(&po,1,0,1,8,1,255))
                      {
                         i = GetFloat(&linebuffer[i],Value[0]);
                         Value0=(char*)Value[0];
                         log =F("P1Wifi: T1: ");
                         log +=  Value0;
                         addLog(LOG_LEVEL_INFO, log);
                    }
                    //1-0:1.8.2(00001.416*kWh) T2
                    else if (CompareObis(&po,1,0,1,8,2,255))
                    {
                       i = GetFloat(&linebuffer[i],Value[1]);
                       Value1=(char*)Value[1];
                       log =F("P1Wifi: T2: ");
                       log +=  Value1;
                       addLog(LOG_LEVEL_INFO, log);
                    }
                    // 1-0:2.8.1(00000.000*kWh)
                    else if (CompareObis(&po,1,0,2,8,1,255))
                    {
                        i = GetFloat(&linebuffer[i],Value[2]);
                        Value2=(char*)Value[2];
                        //  publish("T1d",Value[2]);
                        log =F("P1Wifi: T1d: ");
                        log +=  Value2;
                        addLog(LOG_LEVEL_INFO, log);
                        //        UserVar[event->BaseVarIndex +2 ] = Value2.toFloat(); //T1d Value2
                    }
                    // 1-0:2.8.2(00000.000*kWh)
                    else if (CompareObis(&po,1,0,2,8,2,255))
                    {
                      i = GetFloat(&linebuffer[i],Value[3]);
                      Value3=(char*)Value[3];
                      log =F("P1Wifi: T2d: ");
                      log +=  Value3;
                      addLog(LOG_LEVEL_INFO, log);
                    }
                    // 1-0:1.7.0(0000.42*kW)
                    else if (CompareObis(&po,1,0,1,7,0,255))
                    {
                       i = GetFloat(&linebuffer[i],Value[4]);
                       Value4=(char*)Value[4];
                       wattsin=Value4.toFloat()*1000;
                       Value4=wattsin;
                       //Serial.print(" Va1ue4='");Serial.print(Value4);Serial.println("' ");
                       log =F("P1Wifi: Win: ");
                       log +=  Value4;
                       addLog(LOG_LEVEL_INFO, log);
                       //         UserVar[event->BaseVarIndex +4 ] = wattsin; //Win Value4
                      // publish("Win",Value[4]);
                    }
                    // 1-0:2.7.0(0000.00*kW)
                    else if (CompareObis(&po,1,0,2,7,0,255))
                    {
                       i = GetFloat(&linebuffer[i],Value[5]);
                       Value5=(char*)Value[5];
                       wattsout=Value5.toFloat()*1000;
                       Value5=wattsout;
                       //Serial.print(" Value5='");Serial.print(Value5);Serial.println("' ");
                       // publish("Wout",Value[5]);
                       log =F("P1Wifi: Wout: ");
                       log +=  Value5;
                       addLog(LOG_LEVEL_INFO, log);
                       //         UserVar[event->BaseVarIndex +5 ] = wattsout; // Wout Value5
                    }
                    //0-1:24.2.1(170530203502S)(00236.270*m3)
                    else if (CompareObis(&po,0,1,24,2,1,255))
                    {
                     i = GetFloat(&linebuffer[i],Value[6]);
                     Value6=(char*)Value[6];
                     log =F("P1Wifi: Gas: ");
                     log +=  Value6;
                     addLog(LOG_LEVEL_INFO, log);
                     //         UserVar[event->BaseVarIndex] = Value6.toFloat(); //Gas Value6
                    }
                    // 1-0:32.7.0(222.0*V)
                    else if (CompareObis(&po,1,0,32,7,0,255))
                    {
                      i = GetFloat(&linebuffer[i],Value[7]);
                      volts=(char*)Value[7];
                      Value7=volts;
                      log =F("P1Wifi: Volt: ");
                      log +=  Value7;
                      addLog(LOG_LEVEL_INFO, log);
                      //    UserVar[event->BaseVarIndex+2] = Value7.toFloat(); //Volt Value7
                    }
                    // Invalid Obis returns with po.d = 0 ...
                    else if (CompareObis(&po,255,255,255,0,255,255))
                    {
                       i = GetFloat(&linebuffer[i],Value[8]);
                       log =F("P1Wifi: invalid OBIS ");
                       //log += linebuffer;
                       addLog(LOG_LEVEL_INFO, log);
                   };
                 }
               }
             }
             if (ExtraTaskSettings.TaskDevicePluginConfigLong[5] != 0)
             {

               if (Value0.toFloat() >0 and Value1.toFloat() >0 and Value2.toFloat() >=0 and Value3.toFloat() >=0 and Value4.toFloat() >=0 and Value5.toFloat() >=0)
               {
                    UserVar[event->BaseVarIndex ]    = Value0.toFloat()*1000;; //T1 Value0
                    UserVar[event->BaseVarIndex +1 ] = Value1.toFloat()*1000; //T2 Value1
                    UserVar[event->BaseVarIndex +2 ] = Value2.toFloat(); //T1d Value2
                    UserVar[event->BaseVarIndex +3 ] = Value3.toFloat(); //T2d Value3
                    UserVar[event->BaseVarIndex +4 ] = wattsin; //Win Value4
                    UserVar[event->BaseVarIndex +5 ] = wattsout; // Wout Value5
                    //addLog(LOG_LEVEL_INFO, F("P1Wifi: sending Electricity data));
                    //sendData(event);
                }

                  if (Value6.toFloat() > 0 )
                  {
                    UserVar[event->BaseVarIndex +6 ] = Value6.toFloat()*1000; //Gas Value6
                  }
                  if (Value7.toFloat() > 10 )
                  {
                    UserVar[event->BaseVarIndex +7 ] = Value7.toFloat(); //Volt Value7
                  }
                }
                sendskipcounter=1;
              }
              sendskipcounter--;
              //end of additions
        //addLog(LOG_LEVEL_DEBUG, F("P1Wifi: sending to client"));

        if (P1GatewayClient.connected() ) {
                P1GatewayClient.write((const uint8_t*)Plugin_044_serial_buf, bytes_read);
                P1GatewayClient.flush();
                addLog(LOG_LEVEL_INFO, F("P1Wifi  : data sent to port 8088!"));
                blinkLED();
        }

                if (Settings.UseRules)
                {
                  LoadTaskSettings(event->TaskIndex);
                  String eventString = ExtraTaskSettings.TaskDeviceName;
                  eventString += F("#Data");
                  rulesProcessing(eventString);
                }

              } else {
                addLog(LOG_LEVEL_DEBUG, F("P1   : Error: Invalid CRC, dropped data"));
              }

              bytes_read = 0;
              state = WAITING;
            }   // state == DONE
          }
          success = true;
        }
        break;
      }

  }
  return success;
}
void blinkLED() {
  digitalWrite(STATUS_LED, 1);
  delay(500);
  digitalWrite(STATUS_LED, 0);
}
/*
   validP1char
       checks whether the incoming character is a valid one for a P1 datagram. Returns false if not, which signals corrupt datagram
*/
bool validP1char(char ch) {
  if ((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch == '.') || (ch == '!') || (ch == ' ') || (ch == 92) || (ch == 13) || (ch == '\n') || (ch == '(') || (ch == ')') || (ch == '-') || (ch == '*') || (ch == ':') )
  {
    return true;
  } else {
    addLog(LOG_LEVEL_DEBUG, F("P1   : Error: invalid char read from P1"));
    if (serialdebug) {
      Serial.print(F("faulty char>"));
      Serial.print(ch);
      Serial.println(F("<"));
    }
    return false;
  }
}

int FindCharInArrayRev(char array[], char c, int len) {
  for (int i = len - 1; i >= 0; i--) {
    if (array[i] == c) {
      return i;
    }
  }
  return -1;
}

/*
   CRC16
      based on code written by Jan ten Hove
     https://github.com/jantenhove/P1-Meter-ESP8266
*/
unsigned int CRC16(unsigned int crc, unsigned char *buf, int len)
{
  for (int pos = 0; pos < len; pos++)
  {
    crc ^= (unsigned int)buf[pos];    // XOR byte into least sig. byte of crc

    for (int i = 8; i != 0; i--) {    // Loop over each bit
      if ((crc & 0x0001) != 0) {      // If the LSB is set
        crc >>= 1;                    // Shift right and XOR 0xA001
        crc ^= 0xA001;
      }
      else                            // Else LSB is not set
        crc >>= 1;                    // Just shift right
    }
  }

  return crc;
}

/*  checkDatagram
      checks whether the checksum of the data received from P1 matches the checksum attached to the
      telegram
     based on code written by Jan ten Hove
     https://github.com/jantenhove/P1-Meter-ESP8266
*/
bool checkDatagram(int len) {
  int startChar = FindCharInArrayRev(Plugin_044_serial_buf, '/', len);
  int endChar = FindCharInArrayRev(Plugin_044_serial_buf, '!', len);
  bool validCRCFound = false;

  if (!CRCcheck) return true;

  if (serialdebug) {
    Serial.print(F("input length: "));
    Serial.println(len);
    Serial.print("Start char \\ : ");
    Serial.println(startChar);
    Serial.print(F("End char ! : "));
    Serial.println(endChar);
  }

  if (endChar >= 0)
  {
    currCRC = CRC16(0x0000, (unsigned char *) Plugin_044_serial_buf, endChar - startChar + 1);

    char messageCRC[5];
    strncpy(messageCRC, Plugin_044_serial_buf + endChar + 1, 4);
    messageCRC[4] = 0;
    if (serialdebug) {
      for (int cnt = 0; cnt < len; cnt++)
        Serial.print(Plugin_044_serial_buf[cnt]);
    }

    validCRCFound = (strtoul(messageCRC, NULL, 16) == currCRC);
    if (!validCRCFound) {
      addLog(LOG_LEVEL_DEBUG, F("P1   : Error: invalid CRC found"));
    }
    else{
      String log = F("P1   : OK: valid CRC found");log += currCRC; addLog(LOG_LEVEL_DEBUG_MORE, log);
  }
    currCRC = 0;
  }
  return validCRCFound;
}



// Additions for the P1 data extraction functions. These probably should be moved to MISC.ino
const char *message[] = {
 " \"svalue1\": \""," \"svalue2\": \""," \"svalue3\": \""," \"svalue4\": \""," \"svalue5\": \""," \"svalue6\": \""," \"svalue1\": \""," \"svalue1\": \"", " invalid:"};
 // "T1","T2","T1d","T2d","Win","Wout","Gas", "\"svalue1\": \""};

byte GetObis (char *p, struct obis *po) {
 // return index to delimeter hit
 byte i,a;
 po->a = po->b = po->c = po->d = po->e = po->f = 0xff; // clear obis struct
 i = a = 0;

 while (isdigit(p[i]) || p[i] == '-' || p[i] == ':' || p[i] == '.' || p[i] == '*') { // Escape on any non valid OBIS character
  if (isdigit(p[i])) {
   if (a) {
    a = a*10+p[i] - 0x30;
   }
   else
    a = p[i] - 0x30;
  }
  if (p[i] == '-') {
   po->a = a;
   a=0;
  }
  else if (p[i] == ':') {
   po->b = a;
   a=0;
  }
  else if (p[i] == '.' && po->c == 0xff) {
   po->c = a;
   a=0;
  }
  else if (p[i] == '.' && po->d == 0xff) {
   po->d = a;
   a=0;
  }
  else if (p[i] == '*' ) {
   po->e = a;
   a=0;
  }
  i++;
 }

 // We bumped into a non valid character, determine the last obis field (C & D are mandatory)
 if (po->d == 0xff) {     // D
  po->d = a;
 }
 else if (po->e == 0xff) {  // E
  po->e = a;
 }
 else if (po->f == 0xff) {   // F
  po->f = a;
 }
 return i;
}


byte CompareObis (obis *po, byte a,byte b,byte c,byte d, byte e,byte f) {
 return (po->a == a && po->b == b && po->c == c && po->d == d && po->e == e && po->f == f);
}

// Extracts string from a single line and return index to the next value (when available) or '0' on EOL
byte GetString (char *s,char *d) {
 byte i;
 i=0;
 while (s[i] != ')' && s[i] != 0) {
  if (s[i] != '(') {
   *d = s[i];
   d++;
  }
  *d=0;
  i++; // Advance to next
 }
 //
 return ( s[i] == ')' && s[i+1] == '(' ? i+1 : 0 ); // 0 or index to next value
}

// Extracts float from a single line and return index to the next value (when available) or '0' on EOL
byte GetFloat (char *s,char *d) {
 byte i;
 i=0;
 char *datastart;
 datastart=d;

 while (s[i] != ')' && s[i] != 0) {
  if (s[i] != '(') {
   if (s[i] == '*')
    *d=0;      // Terminate on unit, but keep for reference
   else
    *d = s[i];
   // if (s[i] != '.')
   d++; //this is to skip the decimal point. turns out domoticz needs that to be there, so disabled this line.

  }
  *d=0;
  i++; // Advance to next
}

//Check for extra data field. The last one is correct for Gas.
if (s[i+1] == '(' && s[i] != 0) {
 i++;
d=datastart;

 while (s[i] != ')' && s[i] != 0) {
   if (s[i] != '(') {
    if (s[i] == '*')
     *d=0;      // Terminate on unit, but keep for reference
    else
     *d = s[i];
    // if (s[i] != '.')
    d++;
   }
   *d=0;
   i++; // Advance to next
 }
}
// test
return ( s[i] == ')' && s[i+1] == '(' ? i+1 : 0 );
// or index to next value
}
#endif // USES_P044
