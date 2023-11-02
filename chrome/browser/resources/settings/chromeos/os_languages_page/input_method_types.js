// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @enum {string}
 */
export const JAPANESE_INPUT_MODE = {
  KANA: 'Kana',
  ROMAJI: 'Romaji',
};


/**
 * @enum {string}
 */
export const JAPANESE_PUNCTUATION_STYLE = {
  KUTEN_TOUTEN: 'KutenTouten',
  COMMA_PERIOD: 'CommaPeriod',
  KUTEN_PERIOD: 'KutenPeriod',
  COMMA_TOUTEN: 'CommaTouten',
};


/**
 * @enum {string}
 */
export const JAPANESE_SYMBOL_STYLE = {
  CORNER_BRACKET_MIDDLE_DOT: 'CornerBracketMiddleDot',
  SQUARE_BRACKET_SLASH: 'SquareBracketSlash',
  CORNER_BRACKET_SLASH: 'CornerBracketSlash',
  SQUARE_BRACKET_MIDDLE_DOT: 'SquareBracketMiddleDot',
};

/**
 * @enum {string}
 */
export const JAPANESE_SPACE_INPUT_STYLE = {
  INPUT_MODE: 'InputMode',
  FULLWIDTH: 'Fullwidth',
  HALFWIDTH: 'Halfwidth',
};


/**
 * @enum {string}
 */
export const JAPANESE_SECTION_SHORTCUT = {
  NO_SHORTCUT: 'NoShortcut',
  DIGITS_123456789: 'Digits123456789',
  ASDFGHJKL: 'ASDFGHJKL',
};


/**
 * @enum {string}
 */
export const JAPANESE_KEYMAP_STYLE = {
  CUSTOM: 'Custom',
  ATOK: 'Atok',
  MS_IME: 'MsIme',
  KOTOERI: 'Kotoeri',
  MOBILE: 'Mobile',
  CHROME_OS: 'ChromeOs',
};

export const JAPANESE_SHIFT_KEY_MODE_STYLE = {
  OFF: 'Off',
  ALPHANUMERIC: 'Alphanumeric',
  KATAKANA: 'Katakana',
};
