// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * CrOS-Prefs-backed options on option pages. Where applicable, these literal
 * string values are persisted as entry keys in CrOS-Prefs dictionary value for
 * input-method-specific settings of a particular CrOS 1P input method.
 */
export enum OptionType {
  ENABLE_COMPLETION = 'enableCompletion',
  ENABLE_DOUBLE_SPACE_PERIOD = 'enableDoubleSpacePeriod',
  ENABLE_GESTURE_TYPING = 'enableGestureTyping',
  ENABLE_PREDICTION = 'enablePrediction',
  ENABLE_SOUND_ON_KEYPRESS = 'enableSoundOnKeypress',
  PHYSICAL_KEYBOARD_AUTO_CORRECTION_LEVEL =
      'physicalKeyboardAutoCorrectionLevel',
  PHYSICAL_KEYBOARD_ENABLE_CAPITALIZATION =
      'physicalKeyboardEnableCapitalization',
  PHYSICAL_KEYBOARD_ENABLE_PREDICTIVE_WRITING =
      'physicalKeyboardEnablePredictiveWriting',
  VIRTUAL_KEYBOARD_AUTO_CORRECTION_LEVEL = 'virtualKeyboardAutoCorrectionLevel',
  VIRTUAL_KEYBOARD_ENABLE_CAPITALIZATION =
      'virtualKeyboardEnableCapitalization',

  // Options for Japanese input method.
  // LINT.IfChange(JpOptionCategories)
  JAPANESE_AUTOMATICALLY_SWITCH_TO_HALFWIDTH = 'AutomaticallySwitchToHalfwidth',
  JAPANESE_SHIFT_KEY_MODE_STYLE = 'ShiftKeyModeStyle',
  JAPANESE_USE_INPUT_HISTORY = 'UseInputHistory',
  JAPANESE_USE_SYSTEM_DICTIONARY = 'UseSystemDictionary',
  JAPANESE_NUMBER_OF_SUGGESTIONS = 'numberOfSuggestions',
  JAPANESE_INPUT_MODE = 'JapaneseInputMode',
  JAPANESE_PUNCTUATION_STYLE = 'JapanesePunctuationStyle',
  JAPANESE_SYMBOL_STYLE = 'JapaneseSymbolStyle',
  JAPANESE_SPACE_INPUT_STYLE = 'JapaneseSpaceInputStyle',
  // "...Section..." in the string value below is a typo, but persisted in CrOS
  // Prefs storage so must NOT be fixed unless user data are migrated first.
  JAPANESE_SELECTION_SHORTCUT = 'JapaneseSectionShortcut',
  JAPANESE_KEYMAP_STYLE = 'JapaneseKeymapStyle',
  JAPANESE_DISABLE_PERSONALIZED_SUGGESTIONS = 'JapaneseDisableSuggestions',
  // LINT.ThenChange(/chrome/browser/ash/input_method/input_method_settings_consts.h:JpOptionCategories)

  // Options for Korean input method.
  KOREAN_ENABLE_SYLLABLE_INPUT = 'koreanEnableSyllableInput',
  KOREAN_KEYBOARD_LAYOUT = 'koreanKeyboardLayout',

  // Options for Pinyin input methods.
  PINYIN_XKB_LAYOUT = 'xkbLayout',
  PINYIN_CHINESE_PUNCTUATION = 'pinyinChinesePunctuation',
  PINYIN_DEFAULT_CHINESE = 'pinyinDefaultChinese',
  PINYIN_ENABLE_FUZZY = 'pinyinEnableFuzzy',
  PINYIN_ENABLE_LOWER_PAGING = 'pinyinEnableLowerPaging',
  PINYIN_ENABLE_UPPER_PAGING = 'pinyinEnableUpperPaging',
  PINYIN_FULL_WIDTH_CHARACTER = 'pinyinFullWidthCharacter',
  PINYIN_EN_ENG = 'en:eng',
  PINYIN_AN_ANG = 'an:ang',
  PINYIN_IAN_IANG = 'ian:iang',
  PINYIN_K_G = 'k:g',
  PINYIN_R_L = 'r:l',
  PINYIN_UAN_UANG = 'uan:uang',
  PINYIN_C_CH = 'c:ch',
  PINYIN_F_H = 'f:h',
  PINYIN_IN_ING = 'in:ing',
  PINYIN_L_N = 'l:n',
  PINYIN_S_SH = 's:sh',
  PINYIN_Z_ZH = 'z:zh',

  // Options for Zhuyin input method.
  ZHUYIN_KEYBOARD_LAYOUT = 'zhuyinKeyboardLayout',
  ZHUYIN_PAGE_SIZE = 'zhuyinPageSize',
  ZHUYIN_SELECT_KEYS = 'zhuyinSelectKeys',

  // Options for Vietnamese input methods.
  VIETNAMESE_VNI_ALLOW_FLEXIBLE_DIACRITICS =
      'vietnameseVniAllowFlexibleDiacritics',
  VIETNAMESE_VNI_NEW_STYLE_TONE_MARK_PLACEMENT =
      'vietnameseVniNewStyleToneMarkPlacement',
  VIETNAMESE_VNI_INSERT_DOUBLE_HORN_ON_UO = 'vietnameseVniInsertDoubleHornOnUo',
  VIETNAMESE_VNI_SHOW_UNDERLINE = 'vietnameseVniShowUnderline',
  VIETNAMESE_TELEX_ALLOW_FLEXIBLE_DIACRITICS =
      'vietnameseTelexAllowFlexibleDiacritics',
  VIETNAMESE_TELEX_NEW_STYLE_TONE_MARK_PLACEMENT =
      'vietnameseTelexNewStyleToneMarkPlacement',
  VIETNAMESE_TELEX_INSERT_DOUBLE_HORN_ON_UO =
      'vietnameseTelexInsertDoubleHornOnUo',
  VIETNAMESE_TELEX_INSERT_U_HORN_ON_W = 'vietnameseTelexInsertUHornOnW',
  VIETNAMESE_TELEX_SHOW_UNDERLINE = 'vietnameseTelexShowUnderline',
}

/**
 * The preference string used to indicate a user has autocorrect enabled by
 * default for a particular engine. See the following for more details
 * https://crsrc.org/chrome/browser/ash/input_method/autocorrect_prefs.cc
 */
export const PHYSICAL_KEYBOARD_AUTOCORRECT_ENABLED_BY_DEFAULT =
    'physicalKeyboardAutoCorrectionEnabledByDefault';

/**
 * Values persisted in OptionType.ZHUYIN_KEYBOARD_LAYOUT CrOS-Prefs entry.
 */
export enum ZhuyinKeyboardLayout {
  STANDARD = 'Default',
  ETEN = 'Eten',
  IBM = 'IBM',
}

/**
 * Values persisted in OptionType.KOREAN_KEYBOARD_LAYOUT CrOS-Prefs entry.
 *
 * Although these may look like UI display strings, and they also happen to be
 * used for UI display, they're still critical values persisted in CrOS-Prefs.
 */
export enum KoreanKeyboardLayout {
  SET2 = '2 Set / 두벌식',
  SET2Y = '2 Set (Old Hangul) / 두벌식 (옛글)',
  SET390 = '3 Set (390) / 세벌식 (390)',
  SET3_FINAL = '3 Set (Final) / 세벌식 (최종)',
  SET3_SUN = '3 Set (No Shift) / 세벌식 (순아래)',
  SET3_YET = '3 Set (Old Hangul) / 세벌식 (옛글)',
}

/**
 * Values persisted in OptionType.PINYIN_XKB_LAYOUT CrOS-Prefs entry.
 */
export enum PinyinXkbLayout {
  XKB_US = 'US',
  XKB_DVORAK = 'Dvorak',
  XKB_COLEMAK = 'Colemak',
}

/**
 * Values persisted in OptionType.ZHUYIN_PAGE_SIZE CrOS-Prefs entry.
 */
export enum ZhuyinPageSize {
  ZHUYIN_PAGE_SIZE_10 = '10',
  ZHUYIN_PAGE_SIZE_9 = '9',
  ZHUYIN_PAGE_SIZE_8 = '8',
}

/**
 * Values persisted in OptionType.ZHUYIN_SELECT_KEYS CrOS-Prefs entry.
 */
export enum ZhuyinSelectKeys {
  ZHUYIN_SELECT_KEYS_1234567890 = '1234567890',
  ZHUYIN_SELECT_KEYS_ASDFGHJKL = 'asdfghjkl;',
  ZHUYIN_SELECT_KEYS_ASDFZXCV89 = 'asdfzxcv89',
  ZHUYIN_SELECT_KEYS_ASDFJKL789 = 'asdfjkl789',
  ZHUYIN_SELECT_KEYS_1234QWERAS = '1234qweras',
}

// LINT.IfChange(JpOptionValues)
/**
 * Values persisted in OptionType.JAPANESE_INPUT_MODE CrOS-Prefs entry.
 */
export enum JapaneseInputMode {
  KANA = 'Kana',
  ROMAJI = 'Romaji',
}

/**
 * Values persisted in OptionType.JAPANESE_PUNCTUATION_STYLE CrOS-Prefs entry.
 */
export enum JapanesePunctuationStyle {
  // "KutenTouten" string value is a misnomer originating from Japanese IME Mozc
  // lib (where it's now been fixed), but this string is persisted in CrOS Prefs
  // storage so must NOT be adapted unless user data are migrated first.
  TOUTEN_KUTEN = 'KutenTouten',

  COMMA_PERIOD = 'CommaPeriod',

  // "KutenPeriod" string value is a misnomer originating from Japanese IME Mozc
  // lib (where it's now been fixed), but this string is persisted in CrOS Prefs
  // storage so must NOT be adapted unless user data are migrated first.
  TOUTEN_PERIOD = 'KutenPeriod',

  // "CommaTouten" string value is a misnomer originating from Japanese IME Mozc
  // lib (where it's now been fixed), but this string is persisted in CrOS Prefs
  // storage so must NOT be adapted unless user data are migrated first.
  COMMA_KUTEN = 'CommaTouten',
}

/**
 * Values persisted in OptionType.JAPANESE_SYMBOL_STYLE CrOS-Prefs entry.
 */
export enum JapaneseSymbolStyle {
  CORNER_BRACKET_MIDDLE_DOT = 'CornerBracketMiddleDot',
  SQUARE_BRACKET_SLASH = 'SquareBracketSlash',
  CORNER_BRACKET_SLASH = 'CornerBracketSlash',
  SQUARE_BRACKET_MIDDLE_DOT = 'SquareBracketMiddleDot',
}

/**
 * Values persisted in OptionType.JAPANESE_SPACE_INPUT_STYLE CrOS-Prefs entry.
 */
export enum JapaneseSpaceInputStyle {
  INPUT_MODE = 'InputMode',
  FULLWIDTH = 'Fullwidth',
  HALFWIDTH = 'Halfwidth',
}

/**
 * Values persisted in OptionType.JAPANESE_SELECTION_SHORTCUT CrOS-Prefs entry.
 */
export enum JapaneseSelectionShortcut {
  NO_SHORTCUT = 'NoShortcut',
  DIGITS_123456789 = 'Digits123456789',
  ASDFGHJKL = 'ASDFGHJKL',
}

/**
 * Values persisted in OptionType.JAPANESE_KEYMAP_STYLE CrOS-Prefs entry.
 */
export enum JapaneseKeymapStyle {
  ATOK = 'Atok',
  MS_IME = 'MsIme',
  KOTOERI = 'Kotoeri',
  CHROME_OS = 'ChromeOs',
}

/**
 * Values persisted in OptionType.JAPANESE_SHIFT_KEY_MODE_STYLE CrOS-Prefs entry
 */
export enum JapaneseShiftKeyModeStyle {
  OFF = 'Off',
  ALPHANUMERIC = 'Alphanumeric',
  KATAKANA = 'Katakana',
}
// LINT.ThenChange(/chrome/browser/ash/input_method/input_method_settings_consts.h:JpOptionValues)
