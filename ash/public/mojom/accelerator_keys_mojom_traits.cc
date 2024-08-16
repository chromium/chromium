// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/mojom/accelerator_keys_mojom_traits.h"

#include "ash/public/mojom/accelerator_keys.mojom.h"
#include "base/notreached.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace mojo {

using mojom_vkey = ash::mojom::VKey;

mojom_vkey EnumTraits<mojom_vkey, ui::KeyboardCode>::ToMojom(
    ui::KeyboardCode vkey) {
  switch (vkey) {
    case ui::KeyboardCode::VKEY_CANCEL:
      return mojom_vkey::kCancel;
    case ui::KeyboardCode::VKEY_BACK:
      return mojom_vkey::kBack;
    case ui::KeyboardCode::VKEY_TAB:
      return mojom_vkey::kTab;
    case ui::KeyboardCode::VKEY_BACKTAB:
      return mojom_vkey::kBackTab;
    case ui::KeyboardCode::VKEY_CLEAR:
      return mojom_vkey::kClear;
    case ui::KeyboardCode::VKEY_RETURN:
      return mojom_vkey::kReturn;
    case ui::KeyboardCode::VKEY_SHIFT:
      return mojom_vkey::kShift;
    case ui::KeyboardCode::VKEY_CONTROL:
      return mojom_vkey::kControl;
    case ui::KeyboardCode::VKEY_MENU:
      return mojom_vkey::kMenu;
    case ui::KeyboardCode::VKEY_PAUSE:
      return mojom_vkey::kPause;
    case ui::KeyboardCode::VKEY_CAPITAL:
      return mojom_vkey::kCapital;
    case ui::KeyboardCode::VKEY_KANA:  // Also includes VKEY_HANGUL
      return mojom_vkey::kKana;        // Both are 0x15
    case ui::KeyboardCode::VKEY_PASTE:
      return mojom_vkey::kPaste;
    case ui::KeyboardCode::VKEY_JUNJA:
      return mojom_vkey::kJunja;
    case ui::KeyboardCode::VKEY_FINAL:
      return mojom_vkey::kFinal;
    case ui::KeyboardCode::VKEY_HANJA:  // Also includes VKEY_KANJI
      return mojom_vkey::kHanja;        // Both are 0x19
    case ui::KeyboardCode::VKEY_ESCAPE:
      return mojom_vkey::kEscape;
    case ui::KeyboardCode::VKEY_CONVERT:
      return mojom_vkey::kConvert;
    case ui::KeyboardCode::VKEY_NONCONVERT:
      return mojom_vkey::kNonConvert;
    case ui::KeyboardCode::VKEY_ACCEPT:
      return mojom_vkey::kAccept;
    case ui::KeyboardCode::VKEY_MODECHANGE:
      return mojom_vkey::kModeChange;
    case ui::KeyboardCode::VKEY_SPACE:
      return mojom_vkey::kSpace;
    case ui::KeyboardCode::VKEY_PRIOR:
      return mojom_vkey::kPrior;
    case ui::KeyboardCode::VKEY_NEXT:
      return mojom_vkey::kNext;
    case ui::KeyboardCode::VKEY_END:
      return mojom_vkey::kEnd;
    case ui::KeyboardCode::VKEY_HOME:
      return mojom_vkey::kHome;
    case ui::KeyboardCode::VKEY_LEFT:
      return mojom_vkey::kLeft;
    case ui::KeyboardCode::VKEY_UP:
      return mojom_vkey::kUp;
    case ui::KeyboardCode::VKEY_RIGHT:
      return mojom_vkey::kRight;
    case ui::KeyboardCode::VKEY_DOWN:
      return mojom_vkey::kDown;
    case ui::KeyboardCode::VKEY_SELECT:
      return mojom_vkey::kSelect;
    case ui::KeyboardCode::VKEY_PRINT:
      return mojom_vkey::kPrint;
    case ui::KeyboardCode::VKEY_EXECUTE:
      return mojom_vkey::kExecute;
    case ui::KeyboardCode::VKEY_SNAPSHOT:
      return mojom_vkey::kSnapshot;
    case ui::KeyboardCode::VKEY_INSERT:
      return mojom_vkey::kInsert;
    case ui::KeyboardCode::VKEY_DELETE:
      return mojom_vkey::kDelete;
    case ui::KeyboardCode::VKEY_HELP:
      return mojom_vkey::kHelp;
    case ui::KeyboardCode::VKEY_0:
      return mojom_vkey::kNum0;
    case ui::KeyboardCode::VKEY_1:
      return mojom_vkey::kNum1;
    case ui::KeyboardCode::VKEY_2:
      return mojom_vkey::kNum2;
    case ui::KeyboardCode::VKEY_3:
      return mojom_vkey::kNum3;
    case ui::KeyboardCode::VKEY_4:
      return mojom_vkey::kNum4;
    case ui::KeyboardCode::VKEY_5:
      return mojom_vkey::kNum5;
    case ui::KeyboardCode::VKEY_6:
      return mojom_vkey::kNum6;
    case ui::KeyboardCode::VKEY_7:
      return mojom_vkey::kNum7;
    case ui::KeyboardCode::VKEY_8:
      return mojom_vkey::kNum8;
    case ui::KeyboardCode::VKEY_9:
      return mojom_vkey::kNum9;
    case ui::KeyboardCode::VKEY_A:
      return mojom_vkey::kKeyA;
    case ui::KeyboardCode::VKEY_B:
      return mojom_vkey::kKeyB;
    case ui::KeyboardCode::VKEY_C:
      return mojom_vkey::kKeyC;
    case ui::KeyboardCode::VKEY_D:
      return mojom_vkey::kKeyD;
    case ui::KeyboardCode::VKEY_E:
      return mojom_vkey::kKeyE;
    case ui::KeyboardCode::VKEY_F:
      return mojom_vkey::kKeyF;
    case ui::KeyboardCode::VKEY_G:
      return mojom_vkey::kKeyG;
    case ui::KeyboardCode::VKEY_H:
      return mojom_vkey::kKeyH;
    case ui::KeyboardCode::VKEY_I:
      return mojom_vkey::kKeyI;
    case ui::KeyboardCode::VKEY_J:
      return mojom_vkey::kKeyJ;
    case ui::KeyboardCode::VKEY_K:
      return mojom_vkey::kKeyK;
    case ui::KeyboardCode::VKEY_L:
      return mojom_vkey::kKeyL;
    case ui::KeyboardCode::VKEY_M:
      return mojom_vkey::kKeyM;
    case ui::KeyboardCode::VKEY_N:
      return mojom_vkey::kKeyN;
    case ui::KeyboardCode::VKEY_O:
      return mojom_vkey::kKeyO;
    case ui::KeyboardCode::VKEY_P:
      return mojom_vkey::kKeyP;
    case ui::KeyboardCode::VKEY_Q:
      return mojom_vkey::kKeyQ;
    case ui::KeyboardCode::VKEY_R:
      return mojom_vkey::kKeyR;
    case ui::KeyboardCode::VKEY_S:
      return mojom_vkey::kKeyS;
    case ui::KeyboardCode::VKEY_T:
      return mojom_vkey::kKeyT;
    case ui::KeyboardCode::VKEY_U:
      return mojom_vkey::kKeyU;
    case ui::KeyboardCode::VKEY_V:
      return mojom_vkey::kKeyV;
    case ui::KeyboardCode::VKEY_W:
      return mojom_vkey::kKeyW;
    case ui::KeyboardCode::VKEY_X:
      return mojom_vkey::kKeyX;
    case ui::KeyboardCode::VKEY_Y:
      return mojom_vkey::kKeyY;
    case ui::KeyboardCode::VKEY_Z:
      return mojom_vkey::kKeyZ;
    case ui::KeyboardCode::VKEY_LWIN:  // Also includes VKEY_COMMAND
      return mojom_vkey::kLWin;
    case ui::KeyboardCode::VKEY_RWIN:
      return mojom_vkey::kRWin;
    case ui::KeyboardCode::VKEY_APPS:
      return mojom_vkey::kApps;
    case ui::KeyboardCode::VKEY_SLEEP:
      return mojom_vkey::kSleep;
    case ui::KeyboardCode::VKEY_NUMPAD0:
      return mojom_vkey::kNumpad0;
    case ui::KeyboardCode::VKEY_NUMPAD1:
      return mojom_vkey::kNumpad1;
    case ui::KeyboardCode::VKEY_NUMPAD2:
      return mojom_vkey::kNumpad2;
    case ui::KeyboardCode::VKEY_NUMPAD3:
      return mojom_vkey::kNumpad3;
    case ui::KeyboardCode::VKEY_NUMPAD4:
      return mojom_vkey::kNumpad4;
    case ui::KeyboardCode::VKEY_NUMPAD5:
      return mojom_vkey::kNumpad5;
    case ui::KeyboardCode::VKEY_NUMPAD6:
      return mojom_vkey::kNumpad6;
    case ui::KeyboardCode::VKEY_NUMPAD7:
      return mojom_vkey::kNumpad7;
    case ui::KeyboardCode::VKEY_NUMPAD8:
      return mojom_vkey::kNumpad8;
    case ui::KeyboardCode::VKEY_NUMPAD9:
      return mojom_vkey::kNumpad9;
    case ui::KeyboardCode::VKEY_MULTIPLY:
      return mojom_vkey::kMultiply;
    case ui::KeyboardCode::VKEY_ADD:
      return mojom_vkey::kAdd;
    case ui::KeyboardCode::VKEY_SEPARATOR:
      return mojom_vkey::kSeparator;
    case ui::KeyboardCode::VKEY_SUBTRACT:
      return mojom_vkey::kSubtract;
    case ui::KeyboardCode::VKEY_DECIMAL:
      return mojom_vkey::kDecimal;
    case ui::KeyboardCode::VKEY_DIVIDE:
      return mojom_vkey::kDivide;
    case ui::KeyboardCode::VKEY_F1:
      return mojom_vkey::kF1;
    case ui::KeyboardCode::VKEY_F2:
      return mojom_vkey::kF2;
    case ui::KeyboardCode::VKEY_F3:
      return mojom_vkey::kF3;
    case ui::KeyboardCode::VKEY_F4:
      return mojom_vkey::kF4;
    case ui::KeyboardCode::VKEY_F5:
      return mojom_vkey::kF5;
    case ui::KeyboardCode::VKEY_F6:
      return mojom_vkey::kF6;
    case ui::KeyboardCode::VKEY_F7:
      return mojom_vkey::kF7;
    case ui::KeyboardCode::VKEY_F8:
      return mojom_vkey::kF8;
    case ui::KeyboardCode::VKEY_F9:
      return mojom_vkey::kF9;
    case ui::KeyboardCode::VKEY_F10:
      return mojom_vkey::kF10;
    case ui::KeyboardCode::VKEY_F11:
      return mojom_vkey::kF11;
    case ui::KeyboardCode::VKEY_F12:
      return mojom_vkey::kF12;
    case ui::KeyboardCode::VKEY_F13:
      return mojom_vkey::kF13;
    case ui::KeyboardCode::VKEY_F14:
      return mojom_vkey::kF14;
    case ui::KeyboardCode::VKEY_F15:
      return mojom_vkey::kF15;
    case ui::KeyboardCode::VKEY_F16:
      return mojom_vkey::kF16;
    case ui::KeyboardCode::VKEY_F17:
      return mojom_vkey::kF17;
    case ui::KeyboardCode::VKEY_F18:
      return mojom_vkey::kF18;
    case ui::KeyboardCode::VKEY_F19:
      return mojom_vkey::kF19;
    case ui::KeyboardCode::VKEY_F20:
      return mojom_vkey::kF20;
    case ui::KeyboardCode::VKEY_F21:
      return mojom_vkey::kF21;
    case ui::KeyboardCode::VKEY_F22:
      return mojom_vkey::kF22;
    case ui::KeyboardCode::VKEY_F23:
      return mojom_vkey::kF23;
    case ui::KeyboardCode::VKEY_F24:
      return mojom_vkey::kF24;
    case ui::KeyboardCode::VKEY_NUMLOCK:
      return mojom_vkey::kNumLock;
    case ui::KeyboardCode::VKEY_SCROLL:
      return mojom_vkey::kScroll;
    case ui::KeyboardCode::VKEY_LSHIFT:
      return mojom_vkey::kLShift;
    case ui::KeyboardCode::VKEY_RSHIFT:
      return mojom_vkey::kRShift;
    case ui::KeyboardCode::VKEY_LCONTROL:
      return mojom_vkey::kLControl;
    case ui::KeyboardCode::VKEY_RCONTROL:
      return mojom_vkey::kRControl;
    case ui::KeyboardCode::VKEY_LMENU:
      return mojom_vkey::kLMenu;
    case ui::KeyboardCode::VKEY_RMENU:
      return mojom_vkey::kRMenu;
    case ui::KeyboardCode::VKEY_BROWSER_BACK:
      return mojom_vkey::kBrowserBack;
    case ui::KeyboardCode::VKEY_BROWSER_FORWARD:
      return mojom_vkey::kBrowserForward;
    case ui::KeyboardCode::VKEY_BROWSER_REFRESH:
      return mojom_vkey::kBrowserRefresh;
    case ui::KeyboardCode::VKEY_BROWSER_STOP:
      return mojom_vkey::kBrowserStop;
    case ui::KeyboardCode::VKEY_BROWSER_SEARCH:
      return mojom_vkey::kBrowserSearch;
    case ui::KeyboardCode::VKEY_BROWSER_FAVORITES:
      return mojom_vkey::kBrowserFavorites;
    case ui::KeyboardCode::VKEY_BROWSER_HOME:
      return mojom_vkey::kBrowserHome;
    case ui::KeyboardCode::VKEY_VOLUME_MUTE:
      return mojom_vkey::kVolumeMute;
    case ui::KeyboardCode::VKEY_VOLUME_DOWN:
      return mojom_vkey::kVolumeDown;
    case ui::KeyboardCode::VKEY_VOLUME_UP:
      return mojom_vkey::kVolumeUp;
    case ui::KeyboardCode::VKEY_MEDIA_NEXT_TRACK:
      return mojom_vkey::kMediaNextTrack;
    case ui::KeyboardCode::VKEY_MEDIA_PREV_TRACK:
      return mojom_vkey::kMediaPrevTrack;
    case ui::KeyboardCode::VKEY_MEDIA_STOP:
      return mojom_vkey::kMediaStop;
    case ui::KeyboardCode::VKEY_MEDIA_PLAY_PAUSE:
      return mojom_vkey::kMediaPlayPause;
    case ui::KeyboardCode::VKEY_MEDIA_LAUNCH_MAIL:
      return mojom_vkey::kMediaLaunchMail;
    case ui::KeyboardCode::VKEY_MEDIA_LAUNCH_MEDIA_SELECT:
      return mojom_vkey::kMediaLaunchMediaSelect;
    case ui::KeyboardCode::VKEY_MEDIA_LAUNCH_APP1:
      return mojom_vkey::kMediaLaunchApp1;
    case ui::KeyboardCode::VKEY_MEDIA_LAUNCH_APP2:
      return mojom_vkey::kMediaLaunchApp2;
    case ui::KeyboardCode::VKEY_OEM_1:
      return mojom_vkey::kOem1;
    case ui::KeyboardCode::VKEY_OEM_PLUS:
      return mojom_vkey::kOemPlus;
    case ui::KeyboardCode::VKEY_OEM_COMMA:
      return mojom_vkey::kOemComma;
    case ui::KeyboardCode::VKEY_OEM_MINUS:
      return mojom_vkey::kOemMinus;
    case ui::KeyboardCode::VKEY_OEM_PERIOD:
      return mojom_vkey::kOemPeriod;
    case ui::KeyboardCode::VKEY_OEM_2:
      return mojom_vkey::kOem2;
    case ui::KeyboardCode::VKEY_OEM_3:
      return mojom_vkey::kOem3;
    case ui::KeyboardCode::VKEY_OEM_4:
      return mojom_vkey::kOem4;
    case ui::KeyboardCode::VKEY_OEM_5:
      return mojom_vkey::kOem5;
    case ui::KeyboardCode::VKEY_OEM_6:
      return mojom_vkey::kOem6;
    case ui::KeyboardCode::VKEY_OEM_7:
      return mojom_vkey::kOem7;
    case ui::KeyboardCode::VKEY_OEM_8:
      return mojom_vkey::kOem8;
    case ui::KeyboardCode::VKEY_OEM_102:
      return mojom_vkey::kOem102;
    case ui::KeyboardCode::VKEY_OEM_103:
      return mojom_vkey::kOem103;
    case ui::KeyboardCode::VKEY_OEM_104:
      return mojom_vkey::kOem104;
    case ui::KeyboardCode::VKEY_PROCESSKEY:
      return mojom_vkey::kProcessKey;
    case ui::KeyboardCode::VKEY_PACKET:
      return mojom_vkey::kPacket;
    case ui::KeyboardCode::VKEY_OEM_ATTN:
      return mojom_vkey::kOemAttn;
    case ui::KeyboardCode::VKEY_OEM_FINISH:
      return mojom_vkey::kOemFinish;
    case ui::KeyboardCode::VKEY_OEM_COPY:
      return mojom_vkey::kOemCopy;
    case ui::KeyboardCode::VKEY_DBE_SBCSCHAR:
      return mojom_vkey::kDbeSbcsChar;
    case ui::KeyboardCode::VKEY_DBE_DBCSCHAR:
      return mojom_vkey::kDbeDbcsChar;
    case ui::KeyboardCode::VKEY_OEM_BACKTAB:
      return mojom_vkey::kOemBacktab;
    case ui::KeyboardCode::VKEY_ATTN:
      return mojom_vkey::kAttn;
    case ui::KeyboardCode::VKEY_CRSEL:
      return mojom_vkey::kCrsel;
    case ui::KeyboardCode::VKEY_EXSEL:
      return mojom_vkey::kExsel;
    case ui::KeyboardCode::VKEY_EREOF:
      return mojom_vkey::kEreof;
    case ui::KeyboardCode::VKEY_PLAY:
      return mojom_vkey::kPlay;
    case ui::KeyboardCode::VKEY_ZOOM:
      return mojom_vkey::kZoom;
    case ui::KeyboardCode::VKEY_NONAME:
      return mojom_vkey::kNoName;
    case ui::KeyboardCode::VKEY_PA1:
      return mojom_vkey::kPA1;
    case ui::KeyboardCode::VKEY_OEM_CLEAR:
      return mojom_vkey::kOemClear;
    case ui::KeyboardCode::VKEY_UNKNOWN:
      return mojom_vkey::kUnknown;
    case ui::KeyboardCode::VKEY_WLAN:
      return mojom_vkey::kWlan;
    case ui::KeyboardCode::VKEY_POWER:
      return mojom_vkey::kPower;
    case ui::KeyboardCode::VKEY_ASSISTANT:
      return mojom_vkey::kAssistant;
    case ui::KeyboardCode::VKEY_SETTINGS:
      return mojom_vkey::kSettings;
    case ui::KeyboardCode::VKEY_PRIVACY_SCREEN_TOGGLE:
      return mojom_vkey::kPrivacyScreenToggle;
    case ui::KeyboardCode::VKEY_MICROPHONE_MUTE_TOGGLE:
      return mojom_vkey::kMicrophoneMuteToggle;
    case ui::KeyboardCode::VKEY_BRIGHTNESS_DOWN:
      return mojom_vkey::kBrightnessDown;
    case ui::KeyboardCode::VKEY_BRIGHTNESS_UP:
      return mojom_vkey::kBrightnessUp;
    case ui::KeyboardCode::VKEY_KBD_BACKLIGHT_TOGGLE:
      return mojom_vkey::kKbdBrightnessToggle;
    case ui::KeyboardCode::VKEY_KBD_BRIGHTNESS_DOWN:
      return mojom_vkey::kKbdBrightnessDown;
    case ui::KeyboardCode::VKEY_KBD_BRIGHTNESS_UP:
      return mojom_vkey::kKbdBrightnessUp;
    case ui::KeyboardCode::VKEY_ALTGR:
      return mojom_vkey::kAltGr;
    case ui::KeyboardCode::VKEY_COMPOSE:
      return mojom_vkey::kCompose;
    case ui::KeyboardCode::VKEY_MEDIA_PLAY:
      return mojom_vkey::kMediaPlay;
    case ui::KeyboardCode::VKEY_MEDIA_PAUSE:
      return mojom_vkey::kMediaPause;
    case ui::KeyboardCode::VKEY_NEW:
      return mojom_vkey::kNew;
    case ui::KeyboardCode::VKEY_CLOSE:
      return mojom_vkey::kClose;
    case ui::KeyboardCode::VKEY_EMOJI_PICKER:
      return mojom_vkey::kEmojiPicker;
    case ui::KeyboardCode::VKEY_DICTATE:
      return mojom_vkey::kDictate;
    case ui::KeyboardCode::VKEY_ALL_APPLICATIONS:
      return mojom_vkey::kAllApplications;
    case ui::VKEY_FUNCTION:
      return mojom_vkey::kFunction;
    case ui::VKEY_RIGHT_ALT:
      return mojom_vkey::kRightAlt;
    case ui::VKEY_ACCESSIBILITY:
      return mojom_vkey::kAccessibility;
    case ui::VKEY_BUTTON_0:
      return mojom_vkey::kButton0;
    case ui::VKEY_BUTTON_1:
      return mojom_vkey::kButton1;
    case ui::VKEY_BUTTON_2:
      return mojom_vkey::kButton2;
    case ui::VKEY_BUTTON_3:
      return mojom_vkey::kButton3;
    case ui::VKEY_BUTTON_4:
      return mojom_vkey::kButton4;
    case ui::VKEY_BUTTON_5:
      return mojom_vkey::kButton5;
    case ui::VKEY_BUTTON_6:
      return mojom_vkey::kButton6;
    case ui::VKEY_BUTTON_7:
      return mojom_vkey::kButton7;
    case ui::VKEY_BUTTON_8:
      return mojom_vkey::kButton8;
    case ui::VKEY_BUTTON_9:
      return mojom_vkey::kButton9;
    case ui::VKEY_BUTTON_A:
      return mojom_vkey::kButtonA;
    case ui::VKEY_BUTTON_B:
      return mojom_vkey::kButtonB;
    case ui::VKEY_BUTTON_C:
      return mojom_vkey::kButtonC;
    case ui::VKEY_BUTTON_X:
      return mojom_vkey::kButtonX;
    case ui::VKEY_BUTTON_Y:
      return mojom_vkey::kButtonY;
    case ui::VKEY_BUTTON_Z:
      return mojom_vkey::kButtonZ;
  }

  NOTREACHED();
}

bool EnumTraits<mojom_vkey, ui::KeyboardCode>::FromMojom(
    ash::mojom::VKey input,
    ui::KeyboardCode* out) {
  switch (input) {
    case mojom_vkey::kCancel:
      *out = ui::KeyboardCode::VKEY_CANCEL;
      return true;
    case mojom_vkey::kBack:
      *out = ui::KeyboardCode::VKEY_BACK;
      return true;
    case mojom_vkey::kTab:
      *out = ui::KeyboardCode::VKEY_TAB;
      return true;
    case mojom_vkey::kBackTab:
      *out = ui::KeyboardCode::VKEY_BACKTAB;
      return true;
    case mojom_vkey::kClear:
      *out = ui::KeyboardCode::VKEY_CLEAR;
      return true;
    case mojom_vkey::kReturn:
      *out = ui::KeyboardCode::VKEY_RETURN;
      return true;
    case mojom_vkey::kShift:
      *out = ui::KeyboardCode::VKEY_SHIFT;
      return true;
    case mojom_vkey::kControl:
      *out = ui::KeyboardCode::VKEY_CONTROL;
      return true;
    case mojom_vkey::kMenu:
      *out = ui::KeyboardCode::VKEY_MENU;
      return true;
    case mojom_vkey::kPause:
      *out = ui::KeyboardCode::VKEY_PAUSE;
      return true;
    case mojom_vkey::kCapital:
      *out = ui::KeyboardCode::VKEY_CAPITAL;
      return true;
    case mojom_vkey::kKana:  // Also includes VKEY_HANGUL
      *out = ui::KeyboardCode::VKEY_KANA;
      return true;
    case mojom_vkey::kPaste:
      *out = ui::KeyboardCode::VKEY_PASTE;
      return true;
    case mojom_vkey::kJunja:
      *out = ui::KeyboardCode::VKEY_JUNJA;
      return true;
    case mojom_vkey::kFinal:
      *out = ui::KeyboardCode::VKEY_FINAL;
      return true;
    case mojom_vkey::kHanja:  // Also includes VKEY_KANJI
      *out = ui::KeyboardCode::VKEY_HANJA;
      return true;
    case mojom_vkey::kEscape:
      *out = ui::KeyboardCode::VKEY_ESCAPE;
      return true;
    case mojom_vkey::kConvert:
      *out = ui::KeyboardCode::VKEY_CONVERT;
      return true;
    case mojom_vkey::kNonConvert:
      *out = ui::KeyboardCode::VKEY_NONCONVERT;
      return true;
    case mojom_vkey::kAccept:
      *out = ui::KeyboardCode::VKEY_ACCEPT;
      return true;
    case mojom_vkey::kModeChange:
      *out = ui::KeyboardCode::VKEY_MODECHANGE;
      return true;
    case mojom_vkey::kSpace:
      *out = ui::KeyboardCode::VKEY_SPACE;
      return true;
    case mojom_vkey::kPrior:
      *out = ui::KeyboardCode::VKEY_PRIOR;
      return true;
    case mojom_vkey::kNext:
      *out = ui::KeyboardCode::VKEY_NEXT;
      return true;
    case mojom_vkey::kEnd:
      *out = ui::KeyboardCode::VKEY_END;
      return true;
    case mojom_vkey::kHome:
      *out = ui::KeyboardCode::VKEY_HOME;
      return true;
    case mojom_vkey::kLeft:
      *out = ui::KeyboardCode::VKEY_LEFT;
      return true;
    case mojom_vkey::kUp:
      *out = ui::KeyboardCode::VKEY_UP;
      return true;
    case mojom_vkey::kRight:
      *out = ui::KeyboardCode::VKEY_RIGHT;
      return true;
    case mojom_vkey::kDown:
      *out = ui::KeyboardCode::VKEY_DOWN;
      return true;
    case mojom_vkey::kSelect:
      *out = ui::KeyboardCode::VKEY_SELECT;
      return true;
    case mojom_vkey::kPrint:
      *out = ui::KeyboardCode::VKEY_PRINT;
      return true;
    case mojom_vkey::kExecute:
      *out = ui::KeyboardCode::VKEY_EXECUTE;
      return true;
    case mojom_vkey::kSnapshot:
      *out = ui::KeyboardCode::VKEY_SNAPSHOT;
      return true;
    case mojom_vkey::kInsert:
      *out = ui::KeyboardCode::VKEY_INSERT;
      return true;
    case mojom_vkey::kDelete:
      *out = ui::KeyboardCode::VKEY_DELETE;
      return true;
    case mojom_vkey::kHelp:
      *out = ui::KeyboardCode::VKEY_HELP;
      return true;
    case mojom_vkey::kNum0:
      *out = ui::KeyboardCode::VKEY_0;
      return true;
    case mojom_vkey::kNum1:
      *out = ui::KeyboardCode::VKEY_1;
      return true;
    case mojom_vkey::kNum2:
      *out = ui::KeyboardCode::VKEY_2;
      return true;
    case mojom_vkey::kNum3:
      *out = ui::KeyboardCode::VKEY_3;
      return true;
    case mojom_vkey::kNum4:
      *out = ui::KeyboardCode::VKEY_4;
      return true;
    case mojom_vkey::kNum5:
      *out = ui::KeyboardCode::VKEY_5;
      return true;
    case mojom_vkey::kNum6:
      *out = ui::KeyboardCode::VKEY_6;
      return true;
    case mojom_vkey::kNum7:
      *out = ui::KeyboardCode::VKEY_7;
      return true;
    case mojom_vkey::kNum8:
      *out = ui::KeyboardCode::VKEY_8;
      return true;
    case mojom_vkey::kNum9:
      *out = ui::KeyboardCode::VKEY_9;
      return true;
    case mojom_vkey::kKeyA:
      *out = ui::KeyboardCode::VKEY_A;
      return true;
    case mojom_vkey::kKeyB:
      *out = ui::KeyboardCode::VKEY_B;
      return true;
    case mojom_vkey::kKeyC:
      *out = ui::KeyboardCode::VKEY_C;
      return true;
    case mojom_vkey::kKeyD:
      *out = ui::KeyboardCode::VKEY_D;
      return true;
    case mojom_vkey::kKeyE:
      *out = ui::KeyboardCode::VKEY_E;
      return true;
    case mojom_vkey::kKeyF:
      *out = ui::KeyboardCode::VKEY_F;
      return true;
    case mojom_vkey::kKeyG:
      *out = ui::KeyboardCode::VKEY_G;
      return true;
    case mojom_vkey::kKeyH:
      *out = ui::KeyboardCode::VKEY_H;
      return true;
    case mojom_vkey::kKeyI:
      *out = ui::KeyboardCode::VKEY_I;
      return true;
    case mojom_vkey::kKeyJ:
      *out = ui::KeyboardCode::VKEY_J;
      return true;
    case mojom_vkey::kKeyK:
      *out = ui::KeyboardCode::VKEY_K;
      return true;
    case mojom_vkey::kKeyL:
      *out = ui::KeyboardCode::VKEY_L;
      return true;
    case mojom_vkey::kKeyM:
      *out = ui::KeyboardCode::VKEY_M;
      return true;
    case mojom_vkey::kKeyN:
      *out = ui::KeyboardCode::VKEY_N;
      return true;
    case mojom_vkey::kKeyO:
      *out = ui::KeyboardCode::VKEY_O;
      return true;
    case mojom_vkey::kKeyP:
      *out = ui::KeyboardCode::VKEY_P;
      return true;
    case mojom_vkey::kKeyQ:
      *out = ui::KeyboardCode::VKEY_Q;
      return true;
    case mojom_vkey::kKeyR:
      *out = ui::KeyboardCode::VKEY_R;
      return true;
    case mojom_vkey::kKeyS:
      *out = ui::KeyboardCode::VKEY_S;
      return true;
    case mojom_vkey::kKeyT:
      *out = ui::KeyboardCode::VKEY_T;
      return true;
    case mojom_vkey::kKeyU:
      *out = ui::KeyboardCode::VKEY_U;
      return true;
    case mojom_vkey::kKeyV:
      *out = ui::KeyboardCode::VKEY_V;
      return true;
    case mojom_vkey::kKeyW:
      *out = ui::KeyboardCode::VKEY_W;
      return true;
    case mojom_vkey::kKeyX:
      *out = ui::KeyboardCode::VKEY_X;
      return true;
    case mojom_vkey::kKeyY:
      *out = ui::KeyboardCode::VKEY_Y;
      return true;
    case mojom_vkey::kKeyZ:
      *out = ui::KeyboardCode::VKEY_Z;
      return true;
    case mojom_vkey::kLWin:  // Also includes VKEY_COMMAND
      *out = ui::KeyboardCode::VKEY_LWIN;
      return true;
    case mojom_vkey::kRWin:
      *out = ui::KeyboardCode::VKEY_RWIN;
      return true;
    case mojom_vkey::kApps:
      *out = ui::KeyboardCode::VKEY_APPS;
      return true;
    case mojom_vkey::kSleep:
      *out = ui::KeyboardCode::VKEY_SLEEP;
      return true;
    case mojom_vkey::kNumpad0:
      *out = ui::KeyboardCode::VKEY_NUMPAD0;
      return true;
    case mojom_vkey::kNumpad1:
      *out = ui::KeyboardCode::VKEY_NUMPAD1;
      return true;
    case mojom_vkey::kNumpad2:
      *out = ui::KeyboardCode::VKEY_NUMPAD2;
      return true;
    case mojom_vkey::kNumpad3:
      *out = ui::KeyboardCode::VKEY_NUMPAD3;
      return true;
    case mojom_vkey::kNumpad4:
      *out = ui::KeyboardCode::VKEY_NUMPAD4;
      return true;
    case mojom_vkey::kNumpad5:
      *out = ui::KeyboardCode::VKEY_NUMPAD5;
      return true;
    case mojom_vkey::kNumpad6:
      *out = ui::KeyboardCode::VKEY_NUMPAD6;
      return true;
    case mojom_vkey::kNumpad7:
      *out = ui::KeyboardCode::VKEY_NUMPAD7;
      return true;
    case mojom_vkey::kNumpad8:
      *out = ui::KeyboardCode::VKEY_NUMPAD8;
      return true;
    case mojom_vkey::kNumpad9:
      *out = ui::KeyboardCode::VKEY_NUMPAD9;
      return true;
    case mojom_vkey::kMultiply:
      *out = ui::KeyboardCode::VKEY_MULTIPLY;
      return true;
    case mojom_vkey::kAdd:
      *out = ui::KeyboardCode::VKEY_ADD;
      return true;
    case mojom_vkey::kSeparator:
      *out = ui::KeyboardCode::VKEY_SEPARATOR;
      return true;
    case mojom_vkey::kSubtract:
      *out = ui::KeyboardCode::VKEY_SUBTRACT;
      return true;
    case mojom_vkey::kDecimal:
      *out = ui::KeyboardCode::VKEY_DECIMAL;
      return true;
    case mojom_vkey::kDivide:
      *out = ui::KeyboardCode::VKEY_DIVIDE;
      return true;
    case mojom_vkey::kF1:
      *out = ui::KeyboardCode::VKEY_F1;
      return true;
    case mojom_vkey::kF2:
      *out = ui::KeyboardCode::VKEY_F2;
      return true;
    case mojom_vkey::kF3:
      *out = ui::KeyboardCode::VKEY_F3;
      return true;
    case mojom_vkey::kF4:
      *out = ui::KeyboardCode::VKEY_F4;
      return true;
    case mojom_vkey::kF5:
      *out = ui::KeyboardCode::VKEY_F5;
      return true;
    case mojom_vkey::kF6:
      *out = ui::KeyboardCode::VKEY_F6;
      return true;
    case mojom_vkey::kF7:
      *out = ui::KeyboardCode::VKEY_F7;
      return true;
    case mojom_vkey::kF8:
      *out = ui::KeyboardCode::VKEY_F8;
      return true;
    case mojom_vkey::kF9:
      *out = ui::KeyboardCode::VKEY_F9;
      return true;
    case mojom_vkey::kF10:
      *out = ui::KeyboardCode::VKEY_F10;
      return true;
    case mojom_vkey::kF11:
      *out = ui::KeyboardCode::VKEY_F11;
      return true;
    case mojom_vkey::kF12:
      *out = ui::KeyboardCode::VKEY_F12;
      return true;
    case mojom_vkey::kF13:
      *out = ui::KeyboardCode::VKEY_F13;
      return true;
    case mojom_vkey::kF14:
      *out = ui::KeyboardCode::VKEY_F14;
      return true;
    case mojom_vkey::kF15:
      *out = ui::KeyboardCode::VKEY_F15;
      return true;
    case mojom_vkey::kF16:
      *out = ui::KeyboardCode::VKEY_F16;
      return true;
    case mojom_vkey::kF17:
      *out = ui::KeyboardCode::VKEY_F17;
      return true;
    case mojom_vkey::kF18:
      *out = ui::KeyboardCode::VKEY_F18;
      return true;
    case mojom_vkey::kF19:
      *out = ui::KeyboardCode::VKEY_F19;
      return true;
    case mojom_vkey::kF20:
      *out = ui::KeyboardCode::VKEY_F20;
      return true;
    case mojom_vkey::kF21:
      *out = ui::KeyboardCode::VKEY_F21;
      return true;
    case mojom_vkey::kF22:
      *out = ui::KeyboardCode::VKEY_F22;
      return true;
    case mojom_vkey::kF23:
      *out = ui::KeyboardCode::VKEY_F23;
      return true;
    case mojom_vkey::kF24:
      *out = ui::KeyboardCode::VKEY_F24;
      return true;
    case mojom_vkey::kNumLock:
      *out = ui::KeyboardCode::VKEY_NUMLOCK;
      return true;
    case mojom_vkey::kScroll:
      *out = ui::KeyboardCode::VKEY_SCROLL;
      return true;
    case mojom_vkey::kLShift:
      *out = ui::KeyboardCode::VKEY_LSHIFT;
      return true;
    case mojom_vkey::kRShift:
      *out = ui::KeyboardCode::VKEY_RSHIFT;
      return true;
    case mojom_vkey::kLControl:
      *out = ui::KeyboardCode::VKEY_LCONTROL;
      return true;
    case mojom_vkey::kRControl:
      *out = ui::KeyboardCode::VKEY_RCONTROL;
      return true;
    case mojom_vkey::kLMenu:
      *out = ui::KeyboardCode::VKEY_LMENU;
      return true;
    case mojom_vkey::kRMenu:
      *out = ui::KeyboardCode::VKEY_RMENU;
      return true;
    case mojom_vkey::kBrowserBack:
      *out = ui::KeyboardCode::VKEY_BROWSER_BACK;
      return true;
    case mojom_vkey::kBrowserForward:
      *out = ui::KeyboardCode::VKEY_BROWSER_FORWARD;
      return true;
    case mojom_vkey::kBrowserRefresh:
      *out = ui::KeyboardCode::VKEY_BROWSER_REFRESH;
      return true;
    case mojom_vkey::kBrowserStop:
      *out = ui::KeyboardCode::VKEY_BROWSER_STOP;
      return true;
    case mojom_vkey::kBrowserSearch:
      *out = ui::KeyboardCode::VKEY_BROWSER_SEARCH;
      return true;
    case mojom_vkey::kBrowserFavorites:
      *out = ui::KeyboardCode::VKEY_BROWSER_FAVORITES;
      return true;
    case mojom_vkey::kBrowserHome:
      *out = ui::KeyboardCode::VKEY_BROWSER_HOME;
      return true;
    case mojom_vkey::kVolumeMute:
      *out = ui::KeyboardCode::VKEY_VOLUME_MUTE;
      return true;
    case mojom_vkey::kVolumeDown:
      *out = ui::KeyboardCode::VKEY_VOLUME_DOWN;
      return true;
    case mojom_vkey::kVolumeUp:
      *out = ui::KeyboardCode::VKEY_VOLUME_UP;
      return true;
    case mojom_vkey::kMediaNextTrack:
      *out = ui::KeyboardCode::VKEY_MEDIA_NEXT_TRACK;
      return true;
    case mojom_vkey::kMediaPrevTrack:
      *out = ui::KeyboardCode::VKEY_MEDIA_PREV_TRACK;
      return true;
    case mojom_vkey::kMediaStop:
      *out = ui::KeyboardCode::VKEY_MEDIA_STOP;
      return true;
    case mojom_vkey::kMediaPlayPause:
      *out = ui::KeyboardCode::VKEY_MEDIA_PLAY_PAUSE;
      return true;
    case mojom_vkey::kMediaLaunchMail:
      *out = ui::KeyboardCode::VKEY_MEDIA_LAUNCH_MAIL;
      return true;
    case mojom_vkey::kMediaLaunchMediaSelect:
      *out = ui::KeyboardCode::VKEY_MEDIA_LAUNCH_MEDIA_SELECT;
      return true;
    case mojom_vkey::kMediaLaunchApp1:
      *out = ui::KeyboardCode::VKEY_MEDIA_LAUNCH_APP1;
      return true;
    case mojom_vkey::kMediaLaunchApp2:
      *out = ui::KeyboardCode::VKEY_MEDIA_LAUNCH_APP2;
      return true;
    case mojom_vkey::kOem1:
      *out = ui::KeyboardCode::VKEY_OEM_1;
      return true;
    case mojom_vkey::kOemPlus:
      *out = ui::KeyboardCode::VKEY_OEM_PLUS;
      return true;
    case mojom_vkey::kOemComma:
      *out = ui::KeyboardCode::VKEY_OEM_COMMA;
      return true;
    case mojom_vkey::kOemMinus:
      *out = ui::KeyboardCode::VKEY_OEM_MINUS;
      return true;
    case mojom_vkey::kOemPeriod:
      *out = ui::KeyboardCode::VKEY_OEM_PERIOD;
      return true;
    case mojom_vkey::kOem2:
      *out = ui::KeyboardCode::VKEY_OEM_2;
      return true;
    case mojom_vkey::kOem3:
      *out = ui::KeyboardCode::VKEY_OEM_3;
      return true;
    case mojom_vkey::kOem4:
      *out = ui::KeyboardCode::VKEY_OEM_4;
      return true;
    case mojom_vkey::kOem5:
      *out = ui::KeyboardCode::VKEY_OEM_5;
      return true;
    case mojom_vkey::kOem6:
      *out = ui::KeyboardCode::VKEY_OEM_6;
      return true;
    case mojom_vkey::kOem7:
      *out = ui::KeyboardCode::VKEY_OEM_7;
      return true;
    case mojom_vkey::kOem8:
      *out = ui::KeyboardCode::VKEY_OEM_8;
      return true;
    case mojom_vkey::kOem102:
      *out = ui::KeyboardCode::VKEY_OEM_102;
      return true;
    case mojom_vkey::kOem103:
      *out = ui::KeyboardCode::VKEY_OEM_103;
      return true;
    case mojom_vkey::kOem104:
      *out = ui::KeyboardCode::VKEY_OEM_104;
      return true;
    case mojom_vkey::kProcessKey:
      *out = ui::KeyboardCode::VKEY_PROCESSKEY;
      return true;
    case mojom_vkey::kPacket:
      *out = ui::KeyboardCode::VKEY_PACKET;
      return true;
    case mojom_vkey::kOemAttn:
      *out = ui::KeyboardCode::VKEY_OEM_ATTN;
      return true;
    case mojom_vkey::kOemFinish:
      *out = ui::KeyboardCode::VKEY_OEM_FINISH;
      return true;
    case mojom_vkey::kOemCopy:
      *out = ui::KeyboardCode::VKEY_OEM_COPY;
      return true;
    case mojom_vkey::kDbeSbcsChar:
      *out = ui::KeyboardCode::VKEY_DBE_SBCSCHAR;
      return true;
    case mojom_vkey::kDbeDbcsChar:
      *out = ui::KeyboardCode::VKEY_DBE_DBCSCHAR;
      return true;
    case mojom_vkey::kOemBacktab:
      *out = ui::KeyboardCode::VKEY_OEM_BACKTAB;
      return true;
    case mojom_vkey::kAttn:
      *out = ui::KeyboardCode::VKEY_ATTN;
      return true;
    case mojom_vkey::kCrsel:
      *out = ui::KeyboardCode::VKEY_CRSEL;
      return true;
    case mojom_vkey::kExsel:
      *out = ui::KeyboardCode::VKEY_EXSEL;
      return true;
    case mojom_vkey::kEreof:
      *out = ui::KeyboardCode::VKEY_EREOF;
      return true;
    case mojom_vkey::kPlay:
      *out = ui::KeyboardCode::VKEY_PLAY;
      return true;
    case mojom_vkey::kZoom:
      *out = ui::KeyboardCode::VKEY_ZOOM;
      return true;
    case mojom_vkey::kNoName:
      *out = ui::KeyboardCode::VKEY_NONAME;
      return true;
    case mojom_vkey::kPA1:
      *out = ui::KeyboardCode::VKEY_PA1;
      return true;
    case mojom_vkey::kOemClear:
      *out = ui::KeyboardCode::VKEY_OEM_CLEAR;
      return true;
    case mojom_vkey::kUnknown:
      *out = ui::KeyboardCode::VKEY_UNKNOWN;
      return true;
    case mojom_vkey::kWlan:
      *out = ui::KeyboardCode::VKEY_WLAN;
      return true;
    case mojom_vkey::kPower:
      *out = ui::KeyboardCode::VKEY_POWER;
      return true;
    case mojom_vkey::kAssistant:
      *out = ui::KeyboardCode::VKEY_ASSISTANT;
      return true;
    case mojom_vkey::kSettings:
      *out = ui::KeyboardCode::VKEY_SETTINGS;
      return true;
    case mojom_vkey::kPrivacyScreenToggle:
      *out = ui::KeyboardCode::VKEY_PRIVACY_SCREEN_TOGGLE;
      return true;
    case mojom_vkey::kMicrophoneMuteToggle:
      *out = ui::KeyboardCode::VKEY_MICROPHONE_MUTE_TOGGLE;
      return true;
    case mojom_vkey::kBrightnessDown:
      *out = ui::KeyboardCode::VKEY_BRIGHTNESS_DOWN;
      return true;
    case mojom_vkey::kBrightnessUp:
      *out = ui::KeyboardCode::VKEY_BRIGHTNESS_UP;
      return true;
    case mojom_vkey::kKbdBrightnessToggle:
      *out = ui::KeyboardCode::VKEY_KBD_BACKLIGHT_TOGGLE;
      return true;
    case mojom_vkey::kKbdBrightnessDown:
      *out = ui::KeyboardCode::VKEY_KBD_BRIGHTNESS_DOWN;
      return true;
    case mojom_vkey::kKbdBrightnessUp:
      *out = ui::KeyboardCode::VKEY_KBD_BRIGHTNESS_UP;
      return true;
    case mojom_vkey::kAltGr:
      *out = ui::KeyboardCode::VKEY_ALTGR;
      return true;
    case mojom_vkey::kCompose:
      *out = ui::KeyboardCode::VKEY_COMPOSE;
      return true;
    case mojom_vkey::kMediaPlay:
      *out = ui::KeyboardCode::VKEY_MEDIA_PLAY;
      return true;
    case mojom_vkey::kMediaPause:
      *out = ui::KeyboardCode::VKEY_MEDIA_PAUSE;
      return true;
    case mojom_vkey::kNew:
      *out = ui::KeyboardCode::VKEY_NEW;
      return true;
    case mojom_vkey::kClose:
      *out = ui::KeyboardCode::VKEY_CLOSE;
      return true;
    case mojom_vkey::kEmojiPicker:
      *out = ui::KeyboardCode::VKEY_EMOJI_PICKER;
      return true;
    case mojom_vkey::kDictate:
      *out = ui::KeyboardCode::VKEY_DICTATE;
      return true;
    case mojom_vkey::kAllApplications:
      *out = ui::KeyboardCode::VKEY_ALL_APPLICATIONS;
      return true;
    case ash::mojom::VKey::kRightAlt:
      *out = ui::KeyboardCode::VKEY_RIGHT_ALT;
      return true;
    case ash::mojom::VKey::kAccessibility:
      *out = ui::KeyboardCode::VKEY_ACCESSIBILITY;
      return true;
    case ash::mojom::VKey::kFunction:
      *out = ui::KeyboardCode::VKEY_FUNCTION;
      return true;
    case mojom_vkey::kButton0:
      *out = ui::KeyboardCode::VKEY_BUTTON_0;
      return true;
    case mojom_vkey::kButton1:
      *out = ui::KeyboardCode::VKEY_BUTTON_1;
      return true;
    case mojom_vkey::kButton2:
      *out = ui::KeyboardCode::VKEY_BUTTON_2;
      return true;
    case mojom_vkey::kButton3:
      *out = ui::KeyboardCode::VKEY_BUTTON_3;
      return true;
    case mojom_vkey::kButton4:
      *out = ui::KeyboardCode::VKEY_BUTTON_4;
      return true;
    case mojom_vkey::kButton5:
      *out = ui::KeyboardCode::VKEY_BUTTON_5;
      return true;
    case mojom_vkey::kButton6:
      *out = ui::KeyboardCode::VKEY_BUTTON_6;
      return true;
    case mojom_vkey::kButton7:
      *out = ui::KeyboardCode::VKEY_BUTTON_7;
      return true;
    case mojom_vkey::kButton8:
      *out = ui::KeyboardCode::VKEY_BUTTON_8;
      return true;
    case mojom_vkey::kButton9:
      *out = ui::KeyboardCode::VKEY_BUTTON_9;
      return true;
    case mojom_vkey::kButtonA:
      *out = ui::KeyboardCode::VKEY_BUTTON_A;
      return true;
    case mojom_vkey::kButtonB:
      *out = ui::KeyboardCode::VKEY_BUTTON_B;
      return true;
    case mojom_vkey::kButtonC:
      *out = ui::KeyboardCode::VKEY_BUTTON_C;
      return true;
    case mojom_vkey::kButtonX:
      *out = ui::KeyboardCode::VKEY_BUTTON_X;
      return true;
    case mojom_vkey::kButtonY:
      *out = ui::KeyboardCode::VKEY_BUTTON_Y;
      return true;
    case mojom_vkey::kButtonZ:
      *out = ui::KeyboardCode::VKEY_BUTTON_Z;
      return true;
  }
  NOTREACHED();
}

}  // namespace mojo
