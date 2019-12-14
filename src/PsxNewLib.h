#ifndef PSXNEWLIB_H_
#define PSXNEWLIB_H_

//~ #define DUMP_COMMS

// us
const byte INTER_CMD_BYTE_DELAY = 15;

// ms
const unsigned long COMMAND_TIMEOUT = 250;

// ms
const unsigned long COMMAND_RETRY_INTERVAL = 10;

// ms
const unsigned long MODE_SWITCH_DELAY = 500;


/** \brief Type that is used to report button presses
 */
enum PsxButton {
	PSB_NONE       = 0x0000,
	PSB_SELECT     = 0x0001,
	PSB_L3         = 0x0002,
	PSB_R3         = 0x0004,
	PSB_START      = 0x0008,
	PSB_PAD_UP     = 0x0010,
	PSB_PAD_RIGHT  = 0x0020,
	PSB_PAD_DOWN   = 0x0040,
	PSB_PAD_LEFT   = 0x0080,
	PSB_L2         = 0x0100,
	PSB_R2         = 0x0200,
	PSB_L1         = 0x0400,
	PSB_R1         = 0x0800,
	PSB_GREEN      = 0x1000,
	PSB_RED        = 0x2000,
	PSB_BLUE       = 0x4000,
	PSB_PINK       = 0x8000,
	PSB_TRIANGLE   = 0x1000,
	PSB_CIRCLE     = 0x2000,
	PSB_CROSS      = 0x4000,
	PSB_SQUARE     = 0x8000
};

typedef uint16_t PsxButtons;

static byte enter_config[] = {0x01, 0x43, 0x00, 0x01, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A};
static byte exit_config[] = {0x01, 0x43, 0x00, 0x00, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A};
/* These shorter versions of enter_ and exit_config are accepted by all
 * controllers I've tested, even in analog mode, EXCEPT SCPH-1200, so let's use
 * the longer ones
 */
//~ static byte enter_config[] = {0x01, 0x43, 0x00, 0x01, 0x00};
//~ static byte exit_config[] = {0x01, 0x43, 0x00, 0x00, 0x00};
static byte type_read[] = {0x01, 0x45, 0x00, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A};
static byte set_mode[] = {0x01, 0x44, 0x00, /* enabled */ 0x01, /* locked */ 0x03, 0x00, 0x00, 0x00, 0x00};
static byte set_pressures[] = {0x01, 0x4F, 0x00, 0xFF, 0xFF, 0x03, 0x00, 0x00, 0x00};
//~ static byte enable_rumble[] = {0x01, 0x4D, 0x00, 0x00, 0x01};
static byte poll[] = {0x01, 0x42, 0x00, 0xFF, 0xFF};

enum PsxControllerType {
	PSCTRL_UNKNOWN = 0,
	PSCTRL_DUALSHOCK,
	PSCTRL_DSWIRELESS,
	PSCTRL_GUITHERO,

	PSCTRL_MAX
};

class PsxController {
protected:
	static const byte BUFFER_SIZE = 32;
	static const byte ANALOG_BTN_DATA_SIZE = 12;

	byte inputBuffer[BUFFER_SIZE];

	PsxButtons buttonWord;

	byte lx;
	byte ly;
	byte rx;
	byte ry;

	boolean analogSticksValid;
	
	byte analogButtonData[ANALOG_BTN_DATA_SIZE];

	boolean analogButtonDataValid;

	virtual void attention () = 0;

	virtual void noAttention () = 0;

	virtual byte shiftInOut (const byte out) = 0;

	void shiftInOut (const byte *out, byte *in, const byte len) {
#ifdef DUMP_COMMS
		byte inbuf[len];
#endif

		for (byte i = 0; i < len; ++i) {
			byte tmp = shiftInOut (out != NULL ? out[i] : 0x5A);
#ifdef DUMP_COMMS
			inbuf[i] = tmp;
#endif
			if (in != NULL) {
				in[i] = tmp;
			}

			delayMicroseconds (INTER_CMD_BYTE_DELAY);   // Very important!
		}

#ifdef DUMP_COMMS
		Serial.print (F("<-- "));
		for (byte i = 0; i < len; ++i) {
			if (out && out[i] < 0x10)
				Serial.print (0);
			Serial.print (out ? out[i]: 0x5A, HEX);
			Serial.print (' ');
		}
		Serial.println ();

		Serial.print (F("--> "));
		for (byte i = 0; i < len; ++i) {
			if (inbuf[i] < 0x10)
				Serial.print (0);
			Serial.print (inbuf[i], HEX);
			Serial.print (' ');
		}
		Serial.println ();
#endif
	}

	byte *autoShift (const byte *out, const byte outlen) {
		byte * ret = nullptr;

		if (outlen >= 3 && outlen <= BUFFER_SIZE) {
			// All commands have at least 3 bytes, so shift out those first
			shiftInOut (out, inputBuffer, 3);
			if (isValidReply (inputBuffer)) {
				// Reply is good, get full length
				byte replyLen = getReplyLength ();

				// Shift out rest of command
				if (outlen > 3) {
					shiftInOut (out + 3, inputBuffer + 3, outlen - 3);
				}

				byte left = replyLen - outlen + 3;
				//~ Serial.print ("len = ");
				//~ Serial.print (replyLen);
				//~ Serial.print (", left = ");
				//~ Serial.println (left);
				if (left == 0) {
					// The whole reply was gathered
					ret = inputBuffer;
				} else if (outlen + left <= BUFFER_SIZE) {
					// Part of reply is still missing and we have space for it
					shiftInOut (NULL, inputBuffer + outlen, left);
					ret = inputBuffer;
				} else {
					// Reply incomplete but not enough space provided
				}
			}
		}

		return ret;
	}

	byte getReplyLength () const {
		return (inputBuffer[1] & 0x0F) * 2;
	}

	inline boolean isValidReply (const byte *status) {
		//~ return status[0] != 0xFF || status[1] != 0xFF || status[2] != 0xFF;
		return status[1] != 0xFF && (status[2] == 0x5A || status[2] == 0x00);
		//~ return /* status[0] == 0xFF && */ status[1] != 0xFF && status[2] == 0x5A;
	}

	// Green Mode controllers
	inline boolean isFlightstickReply (const byte *status) {
		return (status[1] & 0xF0) == 0x50;
	}

	inline boolean isDualShockReply (const byte *status) {
		return (status[1] & 0xF0) == 0x70;
	}

	inline boolean isDualShock2Reply (const byte *status) {
		return status[1] == 0x79;
	}

	inline boolean isDigitalReply (const byte *status) {
		return (status[1] & 0xF0) == 0x40;
	}

	inline boolean isConfigReply (const byte *status) {
		return (status[1] & 0xF0) == 0xF0;
	}

public:
	virtual boolean begin () {
		lx = 0;
		ly = 0;
		rx = 0;
		ry = 0;

		analogSticksValid = false;

		// Some disposable readings to let the controller know we are here
		for (byte i = 0; i < 5; ++i) {
			read ();
			delay (1);
		}

		return read ();
	}

	boolean enterConfigMode () {
		boolean ret = false;

		unsigned long start = millis ();
		do {
			attention ();
			byte *in = autoShift (enter_config, 4);
			noAttention ();

			ret = in != NULL && isConfigReply (in);

			if (!ret) {
				delay (COMMAND_RETRY_INTERVAL);
			}
		} while (!ret && millis () - start <= COMMAND_TIMEOUT);
		delay (MODE_SWITCH_DELAY);

		return ret;
	}

	boolean setAnalogMode (bool enabled = true, bool locked = false) {
		boolean ret = false;
		byte out[sizeof (set_mode)];

		memcpy (out, set_mode, sizeof (set_mode));
		out[3] = enabled ? 0x01 : 0x00;
		out[4] = locked ? 0x03 : 0x00;

		//~ attention ();
		//~ byte *in = autoShift (out, 5);
		//~ noAttention ();

		//~ // Give controller some time to switch to set the requested mode
		//~ delay (MODE_SWITCH_DELAY);

		unsigned long start = millis ();
		byte cnt = 0;
		do {
			attention ();
			byte *in = autoShift (out, 5);
			noAttention ();

			/* We can't know if we have successfully enabled analog mode until
			 * we get out of config mode, so let's just be happy if we get a few
			 * consecutive valid replies
			 */
			if (in != nullptr) {
				++cnt;
			}
			ret = cnt >= 3;

			if (!ret) {
				delay (COMMAND_RETRY_INTERVAL);
			}
		} while (!ret && millis () - start <= COMMAND_TIMEOUT);
		delay (MODE_SWITCH_DELAY);

		return ret;
	}

	bool enablePressures (bool enabled = true) {
		boolean ret = false;
		byte out[sizeof (set_mode)];

		memcpy (out, set_pressures, sizeof (set_pressures));
		if (!enabled) {
			out[3] = 0x00;
			out[4] = 0x00;
			out[5] = 0x00;
		}

		unsigned long start = millis ();
		byte cnt = 0;
		do {
			attention ();
			byte *in = autoShift (out, sizeof (set_pressures));
			noAttention ();

			/* We can't know if we have successfully enabled analog mode until
			 * we get out of config mode, so let's just be happy if we get a few
			 * consecutive valid replies
			 */
			if (in != nullptr) {
				++cnt;
			}
			ret = cnt >= 3;

			if (!ret) {
				delay (COMMAND_RETRY_INTERVAL);
			}
		} while (!ret && millis () - start <= COMMAND_TIMEOUT);
		delay (MODE_SWITCH_DELAY);

		return ret;
	}

	PsxControllerType getControllerType () {
		PsxControllerType ret = PSCTRL_UNKNOWN;

		attention ();
		byte *in = autoShift (type_read, 3);
		noAttention ();

		if (in != nullptr) {
			const byte& controllerType = in[3];
			if (controllerType == 0x03) {
				ret = PSCTRL_DUALSHOCK;
			//~ } else if (controllerType == 0x01 && in[1] == 0x42) {
				//~ return 4;		// ???
			}  else if (controllerType == 0x01 && in[1] != 0x42) {
				ret = PSCTRL_GUITHERO;
			} else if (controllerType == 0x0C) {
				ret = PSCTRL_DSWIRELESS;
			}
		}

		return ret;
	}

	boolean exitConfigMode () {
		boolean ret = false;

		unsigned long start = millis ();
		do {
			attention ();
			//~ shiftInOut (poll, in, sizeof (poll));
			//~ shiftInOut (exit_config, in, sizeof (exit_config));
			byte *in = autoShift (exit_config, 4);
			noAttention ();

			ret = in != nullptr && !isConfigReply (in);

			if (!ret) {
				delay (COMMAND_RETRY_INTERVAL);
			}
		} while (!ret && millis () - start <= COMMAND_TIMEOUT);
		delay (MODE_SWITCH_DELAY);

		return ret;
	}

	boolean read () {
		boolean ret = false;

		analogSticksValid = false;
		analogButtonDataValid = false;

		attention ();
		byte *in = autoShift (poll, 3);
		noAttention ();

		if (in != NULL) {
			if (isConfigReply (in)) {
				// We're stuck in config mode, try to get out
				exitConfigMode ();
			} else {
				// We surely have buttons
				buttonWord = ((PsxButtons) in[4] << 8) | in[3];

				if (isDualShockReply (in) || isFlightstickReply (in)) {
					// We have analog stick data
					analogSticksValid = true;
					rx = in[5];
					ry = in[6];
					lx = in[7];
					ly = in[8];

					if (isDualShock2Reply (in)) {
						// We also have analog button data
						analogButtonDataValid = true;
						for (int i = 0; i < ANALOG_BTN_DATA_SIZE; ++i) {
							analogButtonData[i] = in[i + 9];
						}
					}
				}

				ret = true;
			}
		}

		return ret;
	}

	PsxButtons getButtonWord () const {
		return ~buttonWord;
	}

	boolean getLeftAnalog (byte& x, byte& y) {
		x = lx;
		y = ly;

		return analogSticksValid;
	}

	boolean getRightAnalog (byte& x, byte& y) {
		x = rx;
		y = ry;

		return analogSticksValid;
	}

	boolean buttonPressed (PsxButtons buttonWordx, PsxButton button) {
		return ((buttonWordx & static_cast<PsxButtons> (button)) > 0);
	}

	boolean buttonPressed (PsxButton button) {
		return buttonPressed (~buttonWord, button);
	}

	boolean noButtonPressed (PsxButtons buttons) {
		return buttons == PSB_NONE;
	}

	boolean noButtonPressed (void) {
		return buttonWord == ~PSB_NONE;
	}

	byte getAnalogButton (PsxButton button) {
		byte ret = 0;
		
		if (analogButtonDataValid) {
			ret = analogButtonData[7];
		} else if (buttonPressed (button)) {
			// No analog data, assume fully pressed or fully released
			ret = 0xFF;
		}

		return ret;
	}	
};

#endif
