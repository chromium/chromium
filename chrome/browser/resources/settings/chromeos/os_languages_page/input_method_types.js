// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @enum {string}
 */
export const JapaneseInputMode = {
  KANA: 'Kana',
  ROMAJI: 'Romaji',
};

/**
 * @enum {string}
 */
export const JapanesePunctuationStyle = {
  KUTEN_TOUTEN: 'KutenTouten',
  COMMA_PERIOD: 'CommaPeriod',
  KUTEN_PERIOD: 'KutenPeriod',
  COMMA_TOUTEN: 'CommaTouten',
};

/**
 * @enum {string}
 */
export const JapaneseSymbolStyle = {
  CORNER_BRACKET_MIDDLE_DOT: 'CornerBracketMiddleDot',
  SQUARE_BRACKET_SLASH: 'SquareBracketSlash',
  CORNER_BRACKET_SLASH: 'CornerBracketSlash',
  SQUARE_BRACKET_MIDDLE_DOT: 'SquareBracketMiddleDot',
};

/**
 * @enum {string}
 */
export const JapaneseSpaceInputStyle = {
  INPUT_MODE: 'InputMode',
  FULLWIDTH: 'Fullwidth',
  HALFWIDTH: 'Halfwidth',
};

/**
 * @enum {string}
 */
export const JapaneseSectionShortcut = {
  NO_SHORTCUT: 'NoShortcut',
  DIGITS_123456789: 'Digits123456789',
  ASDFGHJKL: 'ASDFGHJKL',
};

/**
 * @enum {string}
 */
export const JapaneseKeymapStyle = {
  CUSTOM: 'Custom',
  ATOK: 'Atok',
  MS_IME: 'MsIme',
  KOTOERI: 'Kotoeri',
  MOBILE: 'Mobile',
  CHROME_OS: 'ChromeOs',
};

export const JapaneseShiftKeyModeStyle = {
  OFF: 'Off',
  ALPHANUMERIC: 'Alphanumeric',
  KATAKANA: 'Katakana',
};
