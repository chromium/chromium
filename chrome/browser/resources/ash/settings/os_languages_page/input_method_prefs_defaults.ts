// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {JapaneseInputMode, JapaneseKeymapStyle, JapanesePunctuationStyle, JapaneseSelectionShortcut, JapaneseShiftKeyModeStyle, JapaneseSpaceInputStyle, JapaneseSymbolStyle, KoreanKeyboardLayout, OptionType, PinyinXkbLayout, ZhuyinKeyboardLayout, ZhuyinPageSize, ZhuyinSelectKeys} from './input_method_prefs_consts.js';

/**
 * Default values for each option type.
 *
 * WARNING: Keep this in sync with corresponding Google3 file for extension.
 */
export const OPTION_DEFAULT = {
  [OptionType.ENABLE_COMPLETION]: false,
  [OptionType.ENABLE_DOUBLE_SPACE_PERIOD]: true,
  [OptionType.ENABLE_GESTURE_TYPING]: true,
  [OptionType.ENABLE_PREDICTION]: false,
  [OptionType.ENABLE_SOUND_ON_KEYPRESS]: false,
  [OptionType.PHYSICAL_KEYBOARD_AUTO_CORRECTION_LEVEL]: 0,
  [OptionType.PHYSICAL_KEYBOARD_ENABLE_CAPITALIZATION]: true,
  [OptionType.PHYSICAL_KEYBOARD_ENABLE_PREDICTIVE_WRITING]: true,
  [OptionType.VIRTUAL_KEYBOARD_AUTO_CORRECTION_LEVEL]: 1,
  [OptionType.VIRTUAL_KEYBOARD_ENABLE_CAPITALIZATION]: true,

  // Default option values for Japanese input methods.
  // LINT.IfChange(JpPrefDefaults)
  [OptionType.JAPANESE_AUTOMATICALLY_SWITCH_TO_HALFWIDTH]: true,
  [OptionType.JAPANESE_SHIFT_KEY_MODE_STYLE]:
      JapaneseShiftKeyModeStyle.ALPHANUMERIC,
  [OptionType.JAPANESE_USE_INPUT_HISTORY]: true,
  [OptionType.JAPANESE_USE_SYSTEM_DICTIONARY]: true,
  [OptionType.JAPANESE_NUMBER_OF_SUGGESTIONS]: 3,
  [OptionType.JAPANESE_INPUT_MODE]: JapaneseInputMode.ROMAJI,
  [OptionType.JAPANESE_PUNCTUATION_STYLE]:
      JapanesePunctuationStyle.TOUTEN_KUTEN,
  [OptionType.JAPANESE_SYMBOL_STYLE]:
      JapaneseSymbolStyle.CORNER_BRACKET_MIDDLE_DOT,
  [OptionType.JAPANESE_SPACE_INPUT_STYLE]: JapaneseSpaceInputStyle.INPUT_MODE,
  [OptionType.JAPANESE_SELECTION_SHORTCUT]:
      JapaneseSelectionShortcut.DIGITS_123456789,
  [OptionType.JAPANESE_KEYMAP_STYLE]: JapaneseKeymapStyle.CHROME_OS,
  [OptionType.JAPANESE_DISABLE_PERSONALIZED_SUGGESTIONS]: false,
  // LINT.ThenChange(/chrome/browser/ash/input_method/japanese_settings.cc:JpPrefDefaults)

  // Default option values for Korean input method.
  [OptionType.KOREAN_ENABLE_SYLLABLE_INPUT]: true,
  [OptionType.KOREAN_KEYBOARD_LAYOUT]: KoreanKeyboardLayout.SET2,

  // Default option values for Pinyin input methods.
  [OptionType.PINYIN_XKB_LAYOUT]: PinyinXkbLayout.XKB_US,
  [OptionType.PINYIN_CHINESE_PUNCTUATION]: true,
  [OptionType.PINYIN_DEFAULT_CHINESE]: true,
  [OptionType.PINYIN_ENABLE_FUZZY]: false,
  [OptionType.PINYIN_ENABLE_LOWER_PAGING]: true,
  [OptionType.PINYIN_ENABLE_UPPER_PAGING]: true,
  [OptionType.PINYIN_FULL_WIDTH_CHARACTER]: false,

  // Default option values for Zhuyin input method.
  [OptionType.ZHUYIN_KEYBOARD_LAYOUT]: ZhuyinKeyboardLayout.STANDARD,
  [OptionType.ZHUYIN_PAGE_SIZE]: ZhuyinPageSize.ZHUYIN_PAGE_SIZE_10,
  [OptionType.ZHUYIN_SELECT_KEYS]:
      ZhuyinSelectKeys.ZHUYIN_SELECT_KEYS_1234567890,

  // Default option values for Vietnamese input methods.
  [OptionType.VIETNAMESE_VNI_ALLOW_FLEXIBLE_DIACRITICS]: true,
  [OptionType.VIETNAMESE_VNI_NEW_STYLE_TONE_MARK_PLACEMENT]: false,
  [OptionType.VIETNAMESE_VNI_INSERT_DOUBLE_HORN_ON_UO]: false,
  [OptionType.VIETNAMESE_VNI_SHOW_UNDERLINE]: true,
  [OptionType.VIETNAMESE_TELEX_ALLOW_FLEXIBLE_DIACRITICS]: true,
  [OptionType.VIETNAMESE_TELEX_NEW_STYLE_TONE_MARK_PLACEMENT]: false,
  [OptionType.VIETNAMESE_TELEX_INSERT_DOUBLE_HORN_ON_UO]: false,
  [OptionType.VIETNAMESE_TELEX_INSERT_U_HORN_ON_W]: true,
  [OptionType.VIETNAMESE_TELEX_SHOW_UNDERLINE]: true,
} satisfies Partial<Record<OptionType, unknown>>;
