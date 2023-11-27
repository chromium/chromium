// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A centralized listing for keycodes used by accessibility
 * component extensions. Largely taken from
 * ui/events/keycodes/keyboard_codes_posix.h, but with specific edits for Chrome
 * OS and accessibility.
 */

/** @type {!Object<{code: !Key.Code, name: !Key.Name}>} */
const DATA = {
  UNKNOWN: {code: 0, name: ''},
  POWER_BUTTON: {code: 0, name: 'Power button'},
  CANCEL: {code: 3, name: ''},
  BACK: {code: 8, name: 'Backspace'},
  TAB: {code: 9, name: 'Tab'},
  BACKTAB: {code: 10, name: ''},
  CLEAR: {code: 12, name: ''},
  RETURN: {code: 13, name: 'Enter'},
  SHIFT: {code: 16, name: 'Shift'},
  CONTROL: {code: 17, name: 'Control'},
  MENU: {code: 18, name: ''},
  ALT: {code: 18, name: 'Alt'},
  PAUSE: {code: 19, name: ''},
  CAPITAL: {code: 20, name: ''},
  KANA: {code: 21, name: ''},
  HANGUL: {code: 21, name: ''},
  PASTE: {code: 22, name: ''},
  JUNJA: {code: 23, name: ''},
  FINAL: {code: 24, name: ''},
  HANJA: {code: 25, name: ''},
  KANJI: {code: 25, name: ''},
  ESCAPE: {code: 27, name: 'Escape'},
  CONVERT: {code: 28, name: ''},
  NONCONVERT: {code: 29, name: ''},
  ACCEPT: {code: 30, name: ''},
  MODECHANGE: {code: 31, name: ''},
  SPACE: {code: 32, name: 'Space'},
  PRIOR: {code: 33, name: ''},
  NEXT: {code: 34, name: ''},
  END: {code: 35, name: 'end'},
  HOME: {code: 36, name: 'home'},
  LEFT: {code: 37, name: 'Left arrow'},
  UP: {code: 38, name: 'Up arrow'},
  RIGHT: {code: 39, name: 'Right arrow'},
  DOWN: {code: 40, name: 'Down arrow'},
  SELECT: {code: 41, name: ''},
  PRINT: {code: 42, name: ''},
  EXECUTE: {code: 43, name: ''},
  SNAPSHOT: {code: 44, name: ''},
  INSERT: {code: 45, name: 'Insert'},
  DELETE: {code: 46, name: 'Delete'},
  HELP: {code: 47, name: ''},
  ZERO: {code: 48, name: '0'},
  ONE: {code: 49, name: '1'},
  TWO: {code: 50, name: '2'},
  THREE: {code: 51, name: '3'},
  FOUR: {code: 52, name: '4'},
  FIVE: {code: 53, name: '5'},
  SIX: {code: 54, name: '6'},
  SEVEN: {code: 55, name: '7'},
  EIGHT: {code: 56, name: '8'},
  NINE: {code: 57, name: '9'},
  A: {code: 65, name: 'A'},
  B: {code: 66, name: 'B'},
  C: {code: 67, name: 'C'},
  D: {code: 68, name: 'D'},
  E: {code: 69, name: 'E'},
  F: {code: 70, name: 'F'},
  G: {code: 71, name: 'G'},
  H: {code: 72, name: 'H'},
  I: {code: 73, name: 'I'},
  J: {code: 74, name: 'J'},
  K: {code: 75, name: 'K'},
  L: {code: 76, name: 'L'},
  M: {code: 77, name: 'M'},
  N: {code: 78, name: 'N'},
  O: {code: 79, name: 'O'},
  P: {code: 80, name: 'P'},
  Q: {code: 81, name: 'Q'},
  R: {code: 82, name: 'R'},
  S: {code: 83, name: 'S'},
  T: {code: 84, name: 'T'},
  U: {code: 85, name: 'U'},
  V: {code: 86, name: 'V'},
  W: {code: 87, name: 'W'},
  X: {code: 88, name: 'X'},
  Y: {code: 89, name: 'Y'},
  Z: {code: 90, name: 'Z'},
  SEARCH: {code: 91, name: 'Search'},
  RWIN: {code: 92, name: ''},
  APPS: {code: 93, name: 'Search'},
  SLEEP: {code: 95, name: ''},
  NUMPAD0: {code: 96, name: '0'},
  NUMPAD1: {code: 97, name: '1'},
  NUMPAD2: {code: 98, name: '2'},
  NUMPAD3: {code: 99, name: '3'},
  NUMPAD4: {code: 100, name: '4'},
  NUMPAD5: {code: 101, name: '5'},
  NUMPAD6: {code: 102, name: '6'},
  NUMPAD7: {code: 103, name: '7'},
  NUMPAD8: {code: 104, name: '8'},
  NUMPAD9: {code: 105, name: '9'},
  MULTIPLY: {code: 106, name: ''},
  ADD: {code: 107, name: ''},
  SEPARATOR: {code: 108, name: ''},
  SUBTRACT: {code: 109, name: ''},
  DECIMAL: {code: 110, name: ''},
  DIVIDE: {code: 111, name: ''},
  F1: {code: 112, name: ''},
  F2: {code: 113, name: ''},
  F3: {code: 114, name: ''},
  F4: {code: 115, name: 'Toggle full screen'},
  F5: {code: 116, name: ''},
  F6: {code: 117, name: ''},
  F7: {code: 118, name: ''},
  F8: {code: 119, name: ''},
  F9: {code: 120, name: ''},
  F10: {code: 121, name: ''},
  F11: {code: 122, name: 'F11'},
  F12: {code: 123, name: 'F12'},
  F13: {code: 124, name: ''},
  F14: {code: 125, name: ''},
  F15: {code: 126, name: ''},
  F16: {code: 127, name: ''},
  F17: {code: 128, name: ''},
  F18: {code: 129, name: ''},
  F19: {code: 130, name: ''},
  F20: {code: 131, name: ''},
  F21: {code: 132, name: ''},
  F22: {code: 133, name: ''},
  F23: {code: 134, name: ''},
  F24: {code: 135, name: ''},
  NUMLOCK: {code: 144, name: ''},
  SCROLL: {code: 145, name: ''},
  WLAN: {code: 151, name: ''},
  POWER: {code: 152, name: ''},
  ASSISTANT: {code: 153, name: ''},
  SETTINGS: {code: 154, name: ''},
  PRIVACY_SCREEN_TOGGLE: {code: 155, name: ''},
  LSHIFT: {code: 160, name: ''},
  RSHIFT: {code: 161, name: ''},
  LCONTROL: {code: 162, name: ''},
  RCONTROL: {code: 163, name: ''},
  LMENU: {code: 164, name: ''},
  RMENU: {code: 165, name: ''},
  BROWSER_BACK: {code: 166, name: ''},
  BROWSER_FORWARD: {code: 167, name: ''},
  BROWSER_REFRESH: {code: 168, name: ''},
  BROWSER_STOP: {code: 169, name: ''},
  BROWSER_SEARCH: {code: 170, name: ''},
  BROWSER_FAVORITES: {code: 171, name: ''},
  BROWSER_HOME: {code: 172, name: ''},
  VOLUME_MUTE: {code: 173, name: ''},
  VOLUME_DOWN: {code: 174, name: ''},
  VOLUME_UP: {code: 175, name: ''},
  MEDIA_NEXT_TRACK: {code: 176, name: ''},
  MEDIA_PREV_TRACK: {code: 177, name: ''},
  MEDIA_STOP: {code: 178, name: ''},
  MEDIA_PLAY_PAUSE: {code: 179, name: ''},
  MEDIA_LAUNCH_MAIL: {code: 180, name: ''},
  MEDIA_LAUNCH_MEDIA_SELECT: {code: 181, name: ''},
  MEDIA_LAUNCH_APP1: {code: 182, name: ''},
  MEDIA_LAUNCH_APP2: {code: 183, name: ''},
  OEM_1: {code: 186, name: 'Semicolon'},
  OEM_PLUS: {code: 187, name: 'Equal sign'},
  OEM_COMMA: {code: 188, name: 'Comma'},
  OEM_MINUS: {code: 189, name: 'Dash'},
  OEM_PERIOD: {code: 190, name: 'Period'},
  OEM_2: {code: 191, name: 'Forward slash'},
  OEM_3: {code: 192, name: 'Grave accent'},
  BRIGHTNESS_DOWN: {code: 216, name: ''},
  BRIGHTNESS_UP: {code: 217, name: ''},
  KBD_BRIGHTNESS_DOWN: {code: 218, name: ''},
  OEM_4: {code: 219, name: 'Open bracket'},
  OEM_5: {code: 220, name: 'Back slash'},
  OEM_6: {code: 221, name: 'Close bracket'},
  OEM_7: {code: 222, name: 'Single quote'},
  OEM_8: {code: 223, name: ''},
  ALTGR: {code: 225, name: ''},
  OEM_102: {code: 226, name: ''},
  OEM_103: {code: 227, name: ''},
  OEM_104: {code: 228, name: ''},
  PROCESSKEY: {code: 229, name: ''},
  COMPOSE: {code: 230, name: ''},
  PACKET: {code: 231, name: ''},
  KBD_BRIGHTNESS_UP: {code: 232, name: ''},
  MEDIA_PLAY: {code: 233, name: ''},
  MEDIA_PAUSE: {code: 234, name: ''},
  OEM_ATTN: {code: 240, name: ''},
  OEM_FINISH: {code: 241, name: ''},
  OEM_COPY: {code: 242, name: ''},
  DBE_SBCSCHAR: {code: 243, name: ''},
  DBE_DBCSCHAR: {code: 244, name: ''},
  OEM_BACKTAB: {code: 245, name: ''},
  ATTN: {code: 246, name: ''},
  CRSEL: {code: 247, name: ''},
  EXSEL: {code: 248, name: ''},
  EREOF: {code: 249, name: ''},
  PLAY: {code: 250, name: ''},
  ZOOM: {code: 251, name: ''},
  NONAME: {code: 252, name: ''},
  PA1: {code: 253, name: ''},
  OEM_CLEAR: {code: 254, name: ''},
};

export const Key = {};

/** @typedef {number} */
Key.Code;

/** @typedef {string} */
Key.Name;

/** @type {!Object<string, Key.Code>} */
export const KeyCode = Object.fromEntries(
    Object.entries(DATA).map(([key, data]) => [key, data.code]));

/** @type {!Object<string, Key.Name>} */
export const KeyName = Object.fromEntries(
    Object.entries(DATA).map(([key, data]) => [key, data.name]));

/**
 * @param {!Key.Code} code
 * @return {!Key.Name}
 */
KeyName.fromCode = function(code) {
  const key = String(Object.entries(KeyCode).find(([k, c]) => c === code)[0]);
  return KeyName[key];
};

/**
 * @param {!Key.Name} name
 * @return {!Key.Code}
 */
KeyCode.fromName = function(name) {
  const key = Object.entries(KeyName).find(([k, n]) => n === name)[0];
  return KeyCode[key];
};
