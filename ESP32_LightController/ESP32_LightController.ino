/*
Name:		ESP32_LightController.ino
Created:	13.03.2017 07:04:44
Author:	Patrick Marchhart
*/

#include <WiFiUdp.h>
#include <WiFi.h>

#include "esp32-hal-ledc.h"
#include "esp32-hal-adc.h"


// PWM Channel definition
#define OUTPUT1_PWMchannel_LS	0
#define OUTPUT1_PWMchannel_HS	1
#define OUTPUT2_PWMchannel_LS	2
#define OUTPUT2_PWMchannel_HS	3
#define OUTPUT3_PWMchannel_LS	4
#define OUTPUT3_PWMchannel_HS	5
#define OUTPUT4_PWMchannel_LS	6
#define OUTPUT4_PWMchannel_HS	7

// PWM GPIO definition
#define OUTPUT1_PWMgpio_LS		27
#define OUTPUT1_PWMgpio_HS		12 
#define OUTPUT2_PWMgpio_LS		32
#define OUTPUT2_PWMgpio_HS		33
#define OUTPUT3_PWMgpio_LS		25
#define OUTPUT3_PWMgpio_HS		26
#define OUTPUT4_PWMgpio_LS		16
#define OUTPUT4_PWMgpio_HS		17

// PWM settings
#define PWMfrequency			100
#define PWMresolution			16

// ADC lines
#define current_feedback_1		36
#define current_feedback_2		39 
#define current_feedback_3		34
#define current_feedback_4		13
#define voltage_monitor			35


// WIFI configuration
const char* ssid = "ssid";
const char* password = "password";

uint8_t ip[4] = { 192, 168, 0, 4 };
uint8_t sn[4] = { 255, 255, 255, 0 };
uint8_t gw[4] = { 192, 168, 0, 1 };

IPAddress    node_ip(ip[0], ip[1], ip[2], ip[3]);
IPAddress gateway_ip(gw[0], gw[1], gw[2], gw[3]);
IPAddress     subnet(sn[0], sn[1], sn[2], sn[3]);

// UDP configuration
uint16_t udp_port = 1111;
uint8_t  incomingPacket[255];
WiFiUDP Udp_receive;

// ArtNet configuration
#define ArtNet_Header "Art-Net"
#define ArtNet_Channels 5
#define ArtNet_Net 0
#define ArtNet_SubNet 0
uint8_t ArtNet_data[530];
WiFiUDP Udp_ArtNet;

// HTTP configuration
WiFiClient client;
WiFiServer server(80);
#define timeout_ms 5000

uint8_t pwmchannel[8] =
{
	OUTPUT1_PWMchannel_LS,
	OUTPUT1_PWMchannel_HS,
	OUTPUT2_PWMchannel_LS,
	OUTPUT2_PWMchannel_HS,
	OUTPUT3_PWMchannel_LS,
	OUTPUT3_PWMchannel_HS,
	OUTPUT4_PWMchannel_LS,
	OUTPUT4_PWMchannel_HS
};

uint8_t pwmgpio[8] =
{
	OUTPUT1_PWMgpio_LS,
	OUTPUT1_PWMgpio_HS,
	OUTPUT2_PWMgpio_LS,
	OUTPUT2_PWMgpio_HS,
	OUTPUT3_PWMgpio_LS,
	OUTPUT3_PWMgpio_HS,
	OUTPUT4_PWMgpio_LS,
	OUTPUT4_PWMgpio_HS
};


// the setup function runs once when you press reset or power the board
void setup()
{
	hw_configuration();
	/*
	ledcWrite(OUTPUT1_PWMchannel_LS, 6554);
	ledcWrite(OUTPUT1_PWMchannel_HS, 13107);
	ledcWrite(OUTPUT2_PWMchannel_LS, 19661);
	ledcWrite(OUTPUT2_PWMchannel_HS, 26214);
	ledcWrite(OUTPUT3_PWMchannel_LS, 32768);
	ledcWrite(OUTPUT3_PWMchannel_HS, 39322);
	ledcWrite(OUTPUT4_PWMchannel_LS, 45875);
	ledcWrite(OUTPUT4_PWMchannel_HS, 52429);
	*/
	network_setup();
}

// the loop function runs over and over again until power down or reset
void loop()
{
	adc_processing();
	udp_processing();
	ArtNet_processing();
	http_processing();
}


void adc_processing()
{
	float voltage;

	voltage = (3.2f / 4096) * analogRead(current_feedback_1);
	Serial.print("Spannung CF1: ");
	Serial.println(voltage);

	voltage = (3.2f / 4096) * analogRead(current_feedback_2);
	Serial.print("Spannung CF2: ");
	Serial.println(voltage);

	voltage = (3.2f / 4096) * analogRead(current_feedback_3);
	Serial.print("Spannung CF3: ");
	Serial.println(voltage);

	voltage = (3.2f / 4096) * analogRead(current_feedback_4);
	Serial.print("Spannung CF4: ");
	Serial.println(voltage);

	voltage = (3.2f / 4096) * analogRead(voltage_monitor);
	Serial.print("Betriebsspannung: ");
	Serial.println(voltage);
}

void hw_configuration()
{
	Serial.begin(115200);

	analogSetAttenuation(ADC_11db);		// ADC configuration

	pinMode(2, OUTPUT);

	for (uint8_t i = 0; i < 8; i++)
	{
		ledcSetup(pwmchannel[i], PWMfrequency, PWMresolution);
		ledcAttachPin(pwmgpio[i], pwmchannel[i]);
		ledcWrite(pwmchannel[i], 0);
	}
}

void network_setup()
{
	//WIFI connection
	WiFi.begin(ssid, password);
	WiFi.config(node_ip, gateway_ip, subnet);
	while (WiFi.status() != WL_CONNECTED)
	{
		delay(100);
		Serial.print(".");
	}
	Serial.println("");
	Serial.println("WiFi connected");
	Serial.println("IP address: ");
	Serial.println(WiFi.localIP());

	if (Udp_receive.begin(udp_port) == 1)
	{
		Serial.printf("UDP Port %d is working\n", udp_port);
	}
	else
	{
		Serial.printf("UDP Port %d is not working\n", udp_port);
	}

	if (Udp_ArtNet.begin(6454) == 1)
	{
		Serial.printf("Art-Net Port is working\n");
	}
	else
	{
		Serial.printf("Art-Net Port is not working\n");
	}

	// start HTTP Server
	server.begin();
}

void udp_processing()
{
	uint8_t packetSize = Udp_receive.parsePacket();
	uint8_t len;
	uint32_t outx_duty_temp = 0;

	if (packetSize)
	{
		// receive incoming UDP packets
		Serial.printf("Received %d bytes from %s, port %d\n", packetSize, Udp_receive.remoteIP().toString().c_str(), Udp_receive.remotePort());
		len = Udp_receive.read(incomingPacket, 255);
		if (len > 0)
		{
			incomingPacket[len] = 0;
		}
		Serial.printf("UDP packet contents: %s\n", incomingPacket);

		//example: CH1l:12345  CH1h:12345
		if ((incomingPacket[0] == 'C') && (incomingPacket[1] == 'H') && (incomingPacket[4] == ':'))
		{
			for (uint8_t i = 5; i < len; i++)
			{
				outx_duty_temp = outx_duty_temp * 10;
				outx_duty_temp += (incomingPacket[i] - 48);
			}
			if (outx_duty_temp > 65535)
			{
				outx_duty_temp = 65535;
			}

			////debug
			Serial.printf("Channel: %c\n", incomingPacket[2]);
			Serial.print("Type: ");
			if (incomingPacket[3] == 'h')
			{
				Serial.println("Highside");
			}
			if (incomingPacket[3] == 'l')
			{
				Serial.println("Lowside");
			}
			Serial.print("PWM: ");
			Serial.println(outx_duty_temp);


			if (incomingPacket[2] == '1')
			{
				if (incomingPacket[3] == 'h')
				{
					pwm_out(1, 1, outx_duty_temp);
				}
				if (incomingPacket[3] == 'l')
				{
					pwm_out(1, 0, outx_duty_temp);
				}
			}
			if (incomingPacket[2] == '2')
			{
				if (incomingPacket[3] == 'h')
				{
					pwm_out(2, 1, outx_duty_temp);
				}
				if (incomingPacket[3] == 'l')
				{
					pwm_out(2, 0, outx_duty_temp);
				}
			}
			if (incomingPacket[2] == '3')
			{
				if (incomingPacket[3] == 'h')
				{
					pwm_out(3, 1, outx_duty_temp);
				}
				if (incomingPacket[3] == 'l')
				{
					pwm_out(3, 0, outx_duty_temp);
				}
			}
			if (incomingPacket[2] == '4')
			{
				if (incomingPacket[3] == 'h')
				{
					pwm_out(4, 1, outx_duty_temp);
				}
				if (incomingPacket[3] == 'l')
				{
					pwm_out(4, 0, outx_duty_temp);
				}
			}

		}
	}
}

void ArtNet_processing()
{
	uint16_t packetSize = Udp_ArtNet.parsePacket();
	uint16_t len;

	if (packetSize)
	{
		len = Udp_ArtNet.read(ArtNet_data, 530);
		if (len > 0)
		{
			ArtNet_data[len] = 0;
		}

		if
			(ArtNet_data[0] == 'A' && ArtNet_data[1] == 'r'&& ArtNet_data[2] == 't' && ArtNet_data[3] == '-'&&
				ArtNet_data[4] == 'N' && ArtNet_data[5] == 'e'&& ArtNet_data[6] == 't' && ArtNet_data[7] == 0x00
				)
		{
			Serial.println("Art-Net protocol detected");
			process_ArtNetdata();
		}
	}

}

void process_ArtNetdata()
{
	uint16_t Opcode;
	uint16_t Revision;
	uint8_t	Sequence;
	uint8_t Physical;
	uint8_t SubUni;
	uint8_t Net;
	uint16_t Length;

	Opcode = (ArtNet_data[9] << 8) | ArtNet_data[8];
	Serial.printf("Opcode: %x\n", Opcode);

	Revision = (ArtNet_data[10] << 8) | ArtNet_data[11];
	Serial.printf("Revision: %d\n", Revision);

	Sequence = ArtNet_data[12];
	Serial.printf("Sequence: %d\n", Sequence);

	Physical = ArtNet_data[13];
	Serial.printf("Physical: %d\n", Physical);

	SubUni = ArtNet_data[14];
	Serial.printf("SubUni: %d\n", SubUni);

	Net = ArtNet_data[15];
	Serial.printf("Net: %d\n", Net);

	Length = (ArtNet_data[16] << 8) | ArtNet_data[17];
	Serial.printf("Length: %d\n", Length);

	if (Revision > 14)
	{
		Serial.println("Protocol Revision not supported");
		return;
	}

	for (uint8_t i = 1; i <= 5; i++)
	{
		Serial.printf("Channel %d: %d\n", i, ArtNet_data[17 + i]);
	}

	// ArtNet DMX Data
	if (Opcode == 0x5000)
	{
		if ((Net == ArtNet_Net) && (SubUni == ArtNet_SubNet))
		{
			pwm_out(1, ((ArtNet_data[18] & 0b00000001)), (ArtNet_data[19] * (65535 / 255)));
			Serial.printf("Channel 1: HW config: %d PWM: %d", ((ArtNet_data[18] & 0b00000001)), (ArtNet_data[19] * (65535 / 255)));

			pwm_out(2, ((ArtNet_data[18] & 0b00000010) >> 1), (ArtNet_data[20] * (65535 / 255)));
			Serial.printf("Channel 2: HW config: %d PWM: %d", ((ArtNet_data[18] & 0b00000010) >> 1), (ArtNet_data[20] * (65535 / 255)));

			pwm_out(3, ((ArtNet_data[18] & 0b00000100) >> 2), (ArtNet_data[21] * (65535 / 255)));
			Serial.printf("Channel 3: HW config: %d PWM: %d", ((ArtNet_data[18] & 0b00000100) >> 2), (ArtNet_data[21] * (65535 / 255)));

			pwm_out(4, ((ArtNet_data[18] & 0b00001000) >> 3), (ArtNet_data[22] * (65535 / 255)));
			Serial.printf("Channel 4: HW config: %d PWM: %d", ((ArtNet_data[18] & 0b00001000) >> 3), (ArtNet_data[22] * (65535 / 255)));
		}
	}

}

void pwm_out(uint8_t output, uint8_t type, uint32_t duty)
{
	if (type == 0)
	{
		if (output == 1)
		{
			ledcWrite(OUTPUT1_PWMchannel_HS, 0);
			ledcWrite(OUTPUT1_PWMchannel_LS, duty);
		}
		if (output == 2)
		{
			ledcWrite(OUTPUT2_PWMchannel_HS, 0);
			ledcWrite(OUTPUT2_PWMchannel_LS, duty);
		}
		if (output == 3)
		{
			ledcWrite(OUTPUT3_PWMchannel_HS, 0);
			ledcWrite(OUTPUT3_PWMchannel_LS, duty);
		}
		if (output == 4)
		{
			ledcWrite(OUTPUT4_PWMchannel_HS, 0);
			ledcWrite(OUTPUT4_PWMchannel_LS, duty);
		}
	}
	else
	{
		if (output == 1)
		{
			ledcWrite(OUTPUT1_PWMchannel_LS, 0);
			ledcWrite(OUTPUT1_PWMchannel_HS, duty);
		}
		if (output == 2)
		{
			ledcWrite(OUTPUT2_PWMchannel_LS, 0);
			ledcWrite(OUTPUT2_PWMchannel_HS, duty);
		}
		if (output == 3)
		{
			ledcWrite(OUTPUT3_PWMchannel_LS, 0);
			ledcWrite(OUTPUT3_PWMchannel_HS, duty);
		}
		if (output == 4)
		{
			ledcWrite(OUTPUT4_PWMchannel_LS, 0);
			ledcWrite(OUTPUT4_PWMchannel_HS, duty);
		}
	}

}

void http_processing()
{
	client = server.available();
	uint32_t outx_duty_temp = 0;
	uint16_t timeout_counter = 0;
	uint8_t  htmlrequest = 0;
	if (client)
	{
		// if you get a client,
		Serial.println("new client");           // print a message out the serial port
		String currentLine = "";                // make a String to hold incoming data from the client
		while (client.connected())				// loop while the client's connected
		{
			if (client.available())				// if there's bytes to read from the client,
			{
				char c = client.read();         // read a byte, then
				Serial.write(c);                // print it out the serial monitor
				if (c == '\n')					// if the byte is a newline character
				{

					// if the current line is blank, you got two newline characters in a row.
					// that's the end of the client HTTP request, so send a response:
					if (currentLine.length() == 0)
					{
						// HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
						// and a content-type so the client knows what's coming, then a blank line:
						client.println("HTTP/1.1 200 OK");
						client.println("Content-type:text/html");
						client.println();

						if (htmlrequest == 1)
						{
							//HTML CODE
							transmit_htmlcode();
						}

						break;
					}
					else
					{   // if you got a newline, then clear currentLine:  GET /CH1l:12345
						if
							((currentLine.charAt(0) == 'G') && (currentLine.charAt(1) == 'E') &&
							(currentLine.charAt(2) == 'T') && (currentLine.charAt(3) == ' ') &&
								(currentLine.charAt(4) == '/') && (currentLine.charAt(9) == ':')
								)
						{
							for (uint8_t i = 10; i < currentLine.length(); i++)
							{
								if (currentLine.charAt(i) == 0x20)
								{
									break;
								}
								else
								{
									outx_duty_temp = outx_duty_temp * 10;
									outx_duty_temp += (currentLine.charAt(i) - 48);
								}
							}
							if (outx_duty_temp > 65535)
							{
								outx_duty_temp = 65535;
							}

							////debug
							Serial.print("Channel: ");
							Serial.println(currentLine.charAt(7));
							Serial.print("Type: ");
							if (currentLine.charAt(8) == 'h')
							{
								Serial.println("Highside");
							}
							if (currentLine.charAt(8) == 'l')
							{
								Serial.println("Lowside");
							}
							Serial.print("PWM: ");
							Serial.println(outx_duty_temp);

							if (currentLine.charAt(7) == '1')
							{
								if (currentLine.charAt(8) == 'h')
								{
									pwm_out(1, 1, outx_duty_temp);
								}
								if (currentLine.charAt(8) == 'l')
								{
									pwm_out(1, 0, outx_duty_temp);
								}
							}

							if (currentLine.charAt(7) == '2')
							{
								if (currentLine.charAt(8) == 'h')
								{
									pwm_out(2, 1, outx_duty_temp);
								}
								if (currentLine.charAt(8) == 'l')
								{
									pwm_out(2, 0, outx_duty_temp);
								}
							}

							if (currentLine.charAt(7) == '3')
							{
								if (currentLine.charAt(8) == 'h')
								{
									pwm_out(3, 1, outx_duty_temp);
								}
								if (currentLine.charAt(8) == 'l')
								{
									pwm_out(3, 0, outx_duty_temp);
								}
							}

							if (currentLine.charAt(7) == '4')
							{
								if (currentLine.charAt(8) == 'h')
								{
									pwm_out(4, 1, outx_duty_temp);
								}
								if (currentLine.charAt(8) == 'l')
								{
									pwm_out(4, 0, outx_duty_temp);
								}
							}

						}

						if
							((currentLine.charAt(0) == 'G') && (currentLine.charAt(1) == 'E') &&
							(currentLine.charAt(2) == 'T') && (currentLine.charAt(3) == ' ') &&
								(currentLine.charAt(4) == '/') && (currentLine.charAt(5) == ' ')
								)
						{
							//HTML CODE
							htmlrequest = 1;
						}
						currentLine = "";
					}
				}
				else if (c != '\r')
				{
					// if you got anything else but a carriage return character,
					currentLine += c;					// add it to the end of the currentLine
				}
			}
			delay(1);
			timeout_counter++;
			if (timeout_counter > timeout_ms)
			{
				Serial.println("Connection Timeout");
				break;
			}
		}

		// close the connection:
		client.stop();
		Serial.println("client disonnected");
	}
}

void transmit_htmlcode()
{
	client.println("<!DOCTYPE html>");
	client.println("<html lang=en>");
	client.println("<head>");
	client.println("<title>ESP32 Light Controller</title>");
	client.println("<link rel=\"apple-touch-icon-precomposed\" href=\"http://icons.iconarchive.com/icons/hopstarter/sleek-xp-basic/128/Lamp-icon.png\"/>");
	client.println("<meta charset=\"utf-8\"/>");
	client.println("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no, user-scrallable=no, target-densitydpi=device-dpi\">");
	client.println("<meta name=\"apple-mobile-web-app-capable\" content=\"yes\"/>");
	client.println("<meta name=\"apple-mobile-web-app-status-bar-style\" content=\"default\"/>");
	client.println("<style>");
	client.println("body {background-color: #ECECEC;}");
	client.println("h1 {color: #2497E3;font-family: \"Trebuchet MS\", Helvetica, sans-serif;}");
	client.println("h2 {color: #2497E3;font-family: \"Trebuchet MS\", Helvetica, sans-serif;}");
	client.println("input[type=\"range\"]::-webkit-slider-thumb{-webkit-appearance: none;border: 2px solid #2497E3;height: 30px;width: 40px;border-radius: 10px;background: #FFFFFF;cursor: pointer;}");
	client.println("input[type=\"range\"]{-webkit-appearance: none;width: 100%;height: 5px;cursor: pointer;animate: 0.2s;background: #2497E3;}");
	client.println("</style>");

	client.println("</head>");
	client.println("<body>");
	client.println("<h1 class=\"text-center\">ESP32 Light Controller</h1><br>");
	client.println("<h2>Channel 1</h2> <input type=range value=0 autofocus min=0 max=65535 step=1 onchange=\"ch1val(this.value)\"/><br>");
	client.println("<h2>Channel 2</h2> <input type=range value=0 autofocus min=0 max=65535 step=1 onchange=\"ch2val(this.value)\"/><br>");
	client.println("<h2>Channel 3</h2> <input type=range value=0 autofocus min=0 max=65535 step=1 onchange=\"ch3val(this.value)\"/><br>");
	client.println("<h2>Channel 4</h2> <input type=range value=0 autofocus min=0 max=65535 step=1 onchange=\"ch4val(this.value)\"/>");
	client.println("<script>");
	client.printf("function ch1val(newVal){var request=new XMLHttpRequest();request.open(\"GET\",\"http://%d.%d.%d.%d/CH1l:\"+newVal);request.send();}\r\n", ip[0], ip[1], ip[2], ip[3]);
	client.printf("function ch2val(newVal){var request=new XMLHttpRequest();request.open(\"GET\",\"http://%d.%d.%d.%d/CH2l:\"+newVal);request.send();}\r\n", ip[0], ip[1], ip[2], ip[3]);
	client.printf("function ch3val(newVal){var request=new XMLHttpRequest();request.open(\"GET\",\"http://%d.%d.%d.%d/CH3l:\"+newVal);request.send();}\r\n", ip[0], ip[1], ip[2], ip[3]);
	client.printf("function ch4val(newVal){var request=new XMLHttpRequest();request.open(\"GET\",\"http://%d.%d.%d.%d/CH4l:\"+newVal);request.send();}\r\n", ip[0], ip[1], ip[2], ip[3]);
	client.println("</script>");
	client.println("</body>");
	client.println("</html>");

	//HTML End
	client.println();
}