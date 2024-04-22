// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// LINT.IfChange(JpOptionValues)
export enum JapaneseInputMode {
  KANA = 'Kana',
  ROMAJI = 'Romaji',
}

export enum JapanesePunctuationStyle {
  KUTEN_TOUTEN = 'KutenTouten',
  COMMA_PERIOD = 'CommaPeriod',
  KUTEN_PERIOD = 'KutenPeriod',
  COMMA_TOUTEN = 'CommaTouten',
}

export enum JapaneseSymbolStyle {
  CORNER_BRACKET_MIDDLE_DOT = 'CornerBracketMiddleDot',
  SQUARE_BRACKET_SLASH = 'SquareBracketSlash',
  CORNER_BRACKET_SLASH = 'CornerBracketSlash',
  SQUARE_BRACKET_MIDDLE_DOT = 'SquareBracketMiddleDot',
}

export enum JapaneseSpaceInputStyle {
  INPUT_MODE = 'InputMode',
  FULLWIDTH = 'Fullwidth',
  HALFWIDTH = 'Halfwidth',
}

export enum JapaneseSectionShortcut {
  NO_SHORTCUT = 'NoShortcut',
  DIGITS_123456789 = 'Digits123456789',
  ASDFGHJKL = 'ASDFGHJKL',
}

export enum JapaneseKeymapStyle {
  CUSTOM = 'Custom',
  ATOK = 'Atok',
  MS_IME = 'MsIme',
  KOTOERI = 'Kotoeri',
  MOBILE = 'Mobile',
  CHROME_OS = 'ChromeOs',
}

export enum JapaneseShiftKeyModeStyle {
  OFF = 'Off',
  ALPHANUMERIC = 'Alphanumeric',
  KATAKANA = 'Katakana',
}
// LINT.ThenChange(/chrome/browser/ash/input_method/japanese/japanese_prefs_constants.h:JpOptionValues)

