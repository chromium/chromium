// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview constants related to input method options.
 */

import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {assertExhaustive} from '../assert_extras.js';
import {Route, routes} from '../router.js';

import {getInputMethodSettings, SettingsContext, SettingsType} from './input_method_settings.js';
import {JapaneseInputMode, JapaneseKeymapStyle, JapanesePunctuationStyle, JapaneseSectionShortcut, JapaneseShiftKeyModeStyle, JapaneseSpaceInputStyle, JapaneseSymbolStyle} from './input_method_types.js';

/**
 * The prefix string shared by all first party input method ID.
 */
export const FIRST_PARTY_INPUT_METHOD_ID_PREFIX =
    '_comp_ime_jkghodnilhceideoidjikpgommlajknk';

/**
 * The preference string used to indicate a user has autocorrect enabled by
 * default for a particular engine. See the following for more details
 * https://crsrc.org/chrome/browser/ash/input_method/autocorrect_prefs.cc
 */
export const PHYSICAL_KEYBOARD_AUTOCORRECT_ENABLED_BY_DEFAULT =
    'physicalKeyboardAutoCorrectionEnabledByDefault';

/**
 * All possible keyboard layouts. Should match Google3.
 */
enum KeyboardLayout {
  STANDARD = 'Default',
  GINYIEH = 'Gin Yieh',
  ETEN = 'Eten',
  IBM = 'IBM',
  HSU = 'Hsu',
  ETEN26 = 'Eten 26',
  SET2 = '2 Set / 두벌식',
  SET2Y = '2 Set (Old Hangul) / 두벌식 (옛글)',
  SET390 = '3 Set (390) / 세벌식 (390)',
  SET3_FINAL = '3 Set (Final) / 세벌식 (최종)',
  SET3_SUN = '3 Set (No Shift) / 세벌식 (순아래)',
  SET3_YET = '3 Set (Old Hangul) / 세벌식 (옛글)',
  XKB_US = 'US',
  XKB_DVORAK = 'Dvorak',
  XKB_COLEMAK = 'Colemak',
}

/**
 * All possible options on options pages. Should match Gooogle3.
 */
export enum OptionType {
  EDIT_USER_DICT = 'editUserDict',
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
  XKB_LAYOUT = 'xkbLayout',
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
  JAPANESE_SECTION_SHORTCUT = 'JapaneseSectionShortcut',
  JAPANESE_KEYMAP_STYLE = 'JapaneseKeymapStyle',
  JAPANESE_MANAGE_USER_DICTIONARY = 'JapaneseManageUserDictionary',
  JAPANESE_DELETE_PERSONALIZATION_DATA = 'JapaneseClearPersonalizationData',
  JAPANESE_DISABLE_PERSONALIZED_SUGGESTIONS = 'JapaneseDisableSuggestions',
  JAPANESE_AUTOMATICALLY_SEND_STATISTICS_TO_GOOGLE =
      'AutomaticallySendStatisticsToGoogle',
  // LINT.ThenChange(/chrome/browser/ash/input_method/japanese/japanese_prefs_constants.h:JpOptionCategories)
  // Options for Korean input method.
  KOREAN_ENABLE_SYLLABLE_INPUT = 'koreanEnableSyllableInput',
  KOREAN_KEYBOARD_LAYOUT = 'koreanKeyboardLayout',
  // Options for pinyin input method.
  PINYIN_CHINESE_PUNCTUATION = 'pinyinChinesePunctuation',
  PINYIN_DEFAULT_CHINESE = 'pinyinDefaultChinese',
  PINYIN_ENABLE_FUZZY = 'pinyinEnableFuzzy',
  PINYIN_ENABLE_LOWER_PAGING = 'pinyinEnableLowerPaging',
  PINYIN_ENABLE_UPPER_PAGING = 'pinyinEnableUpperPaging',
  PINYIN_FULL_WIDTH_CHARACTER = 'pinyinFullWidthCharacter',
  PINYIN_FUZZY_CONFIG = 'pinyinFuzzyConfig',
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
  // Options for zhuyin input method.
  ZHUYIN_KEYBOARD_LAYOUT = 'zhuyinKeyboardLayout',
  ZHUYIN_PAGE_SIZE = 'zhuyinPageSize',
  ZHUYIN_SELECT_KEYS = 'zhuyinSelectKeys',
  // Options for Vietnamese VNI input method
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
  [OptionType.JAPANESE_NUMBER_OF_SUGGESTIONS]: 3,
  [OptionType.PHYSICAL_KEYBOARD_AUTO_CORRECTION_LEVEL]: 0,
  [OptionType.PHYSICAL_KEYBOARD_ENABLE_CAPITALIZATION]: true,
  [OptionType.PHYSICAL_KEYBOARD_ENABLE_PREDICTIVE_WRITING]: true,
  [OptionType.VIRTUAL_KEYBOARD_AUTO_CORRECTION_LEVEL]: 1,
  [OptionType.VIRTUAL_KEYBOARD_ENABLE_CAPITALIZATION]: true,
  [OptionType.XKB_LAYOUT]: 'US',
  // Options for Japanese input methods.
  // LINT.IfChange(JpPrefDefaults)
  [OptionType.JAPANESE_AUTOMATICALLY_SWITCH_TO_HALFWIDTH]: true,
  [OptionType.JAPANESE_SHIFT_KEY_MODE_STYLE]:
      JapaneseShiftKeyModeStyle.ALPHANUMERIC,
  [OptionType.JAPANESE_USE_INPUT_HISTORY]: true,
  [OptionType.JAPANESE_USE_SYSTEM_DICTIONARY]: true,
  [OptionType.JAPANESE_INPUT_MODE]: JapaneseInputMode.ROMAJI,
  [OptionType.JAPANESE_PUNCTUATION_STYLE]:
      JapanesePunctuationStyle.KUTEN_TOUTEN,
  [OptionType.JAPANESE_SYMBOL_STYLE]:
      JapaneseSymbolStyle.CORNER_BRACKET_MIDDLE_DOT,
  [OptionType.JAPANESE_SPACE_INPUT_STYLE]: JapaneseSpaceInputStyle.INPUT_MODE,
  [OptionType.JAPANESE_SECTION_SHORTCUT]:
      JapaneseSectionShortcut.DIGITS_123456789,
  [OptionType.JAPANESE_KEYMAP_STYLE]: JapaneseKeymapStyle.CUSTOM,
  [OptionType.JAPANESE_DISABLE_PERSONALIZED_SUGGESTIONS]: true,
  [OptionType.JAPANESE_AUTOMATICALLY_SEND_STATISTICS_TO_GOOGLE]: true,
  // LINT.ThenChange(/chrome/browser/ash/input_method/japanese/japanese_settings.cc:JpPrefDefaults)

  // Options for Korean input method.
  [OptionType.KOREAN_ENABLE_SYLLABLE_INPUT]: true,
  [OptionType.KOREAN_KEYBOARD_LAYOUT]: KeyboardLayout.SET2,
  // Options for pinyin input method.
  [OptionType.PINYIN_CHINESE_PUNCTUATION]: true,
  [OptionType.PINYIN_DEFAULT_CHINESE]: true,
  [OptionType.PINYIN_ENABLE_FUZZY]: false,
  [OptionType.PINYIN_ENABLE_LOWER_PAGING]: true,
  [OptionType.PINYIN_ENABLE_UPPER_PAGING]: true,
  [OptionType.PINYIN_FULL_WIDTH_CHARACTER]: false,
  [OptionType.PINYIN_FUZZY_CONFIG]: {
    an_ang: undefined,
    c_ch: undefined,
    en_eng: undefined,
    f_h: undefined,
    ian_iang: undefined,
    in_ing: undefined,
    k_g: undefined,
    l_n: undefined,
    r_l: undefined,
    s_sh: undefined,
    uan_uang: undefined,
    z_zh: undefined,
  },
  // Options for zhuyin input method.
  [OptionType.ZHUYIN_KEYBOARD_LAYOUT]: KeyboardLayout.STANDARD,
  [OptionType.ZHUYIN_PAGE_SIZE]: '10',
  [OptionType.ZHUYIN_SELECT_KEYS]: '1234567890',
  // Options for Vietnamese inputs.
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

/**
 * @param optionName The option we want the default value for.
 * @param overrides List of values to use instead of the default values.
 * @return The default, or overridden value.
 */
export function getDefaultValue<T extends keyof typeof OPTION_DEFAULT>(
    optionName: T, overrides: {[K in T]?: (typeof OPTION_DEFAULT)[K]}):
    (typeof OPTION_DEFAULT)[T] {
  // Overrides are only coming from the following flag, let's be safe here and
  // only enable this branch if the flag is also enabled.
  if (!loadTimeData.getBoolean('autocorrectEnableByDefault')) {
    return OPTION_DEFAULT[optionName];
  }
  return overrides[optionName] ?? OPTION_DEFAULT[optionName];
}

/**
 * Type conversions functions for reading and writing options.  Use these for
 * reading and writing pref values when we don't want the default mappings. This
 * is only used if allow autocorrect toggle is on.
 */
export const AUTOCORRECT_OPTION_MAP_OVERRIDE = {
  [OptionType.PHYSICAL_KEYBOARD_AUTO_CORRECTION_LEVEL]: {
    mapValueForDisplay: (value: number): boolean => value > 0 ? true : false,
    mapValueForWrite: (value: boolean): 1 | 0 => value ? 1 : 0,
  },
  [OptionType.VIRTUAL_KEYBOARD_AUTO_CORRECTION_LEVEL]: {
    mapValueForDisplay: (value: number): boolean => value > 0 ? true : false,
    mapValueForWrite: (value: boolean): 1 | 0 => value ? 1 : 0,
  },
};

/**
 * All possible UI elements for options.
 */
export enum UiType {
  DROPDOWN = 'dropdown',
  LINK = 'link',
  SUBMENU_BUTTON = 'submenuButton',
  TOGGLE_BUTTON = 'toggleButton',
}

/**
 * All possible submenu button types.
 */
export enum SubmenuButton {
  JAPANESE_DELETE_PERSONALIZATION_DATA = 'SubmenuButtonDeletePersonalizedData',
}

/**
 * All possible Settings headers
 */
export enum SettingsHeaders {
  ADVANCED = 'advanced',
  BASIC = 'basic',
  INPUT_ASSISTANCE = 'inputAssistance',
  PHYSICAL_KEYBOARD = 'physicalKeyboard',
  PRIVACY = 'privacy',
  SUGGESTIONS = 'suggestions',
  USER_DICTIONARIES = 'userDictionaries',
  VIRTUAL_KEYBOARD = 'virtualKeyboard',
  VIETNAMESE_FLEXIBLE_TYPING_EMPTY_HEADER =
      'vietnameseFlexibleTypingEmptyHeader',
  VIETNAMESE_SHORTHAND = 'vietnameseShorthand',
  VIETNAMESE_SHOW_UNDERLINE_EMPTY_HEADER = 'vietnameseShowUnderlineEmptyHeader',
}

/**
 * Contents of the settings page for different settings types.
 * These should be in the order they are expected to appear in
 * the actual settings pages.
 */
const Settings = {
  [SettingsType.LATIN_SETTINGS]: [
    {
      title: SettingsHeaders.VIRTUAL_KEYBOARD,
      optionNames: [
        {name: OptionType.ENABLE_SOUND_ON_KEYPRESS},
        {
          name: OptionType.VIRTUAL_KEYBOARD_AUTO_CORRECTION_LEVEL,
          dependentOptions: [
            OptionType.VIRTUAL_KEYBOARD_ENABLE_CAPITALIZATION,
          ],
        },
        {name: OptionType.ENABLE_GESTURE_TYPING},
        {name: OptionType.ENABLE_DOUBLE_SPACE_PERIOD},
        {name: OptionType.EDIT_USER_DICT},
      ],
    },
  ],
  [SettingsType.JAPANESE_SETTINGS]: [
    {
      title: SettingsHeaders.BASIC,
      optionNames: [
        {name: OptionType.JAPANESE_INPUT_MODE},
        {name: OptionType.JAPANESE_PUNCTUATION_STYLE},
        {name: OptionType.JAPANESE_SYMBOL_STYLE},
        {name: OptionType.JAPANESE_SPACE_INPUT_STYLE},
        {name: OptionType.JAPANESE_SECTION_SHORTCUT},
        {name: OptionType.JAPANESE_KEYMAP_STYLE},
      ],
    },
    {
      title: SettingsHeaders.INPUT_ASSISTANCE,
      optionNames: [
        {name: OptionType.JAPANESE_AUTOMATICALLY_SWITCH_TO_HALFWIDTH},
        {name: OptionType.JAPANESE_SHIFT_KEY_MODE_STYLE},
      ],
    },
    {
      title: SettingsHeaders.SUGGESTIONS,
      optionNames: [
        {name: OptionType.JAPANESE_USE_INPUT_HISTORY},
        {name: OptionType.JAPANESE_USE_SYSTEM_DICTIONARY},
        {name: OptionType.JAPANESE_NUMBER_OF_SUGGESTIONS},
      ],
    },
    {
      title: SettingsHeaders.USER_DICTIONARIES,
      optionNames: [{
        name: OptionType.JAPANESE_MANAGE_USER_DICTIONARY,
      }],
    },
    {
      title: SettingsHeaders.PRIVACY,
      optionNames: [
        {name: OptionType.JAPANESE_DELETE_PERSONALIZATION_DATA},
        {name: OptionType.JAPANESE_DISABLE_PERSONALIZED_SUGGESTIONS},
        {name: OptionType.JAPANESE_AUTOMATICALLY_SEND_STATISTICS_TO_GOOGLE},
      ],
    },
  ],
  [SettingsType.ZHUYIN_SETTINGS]: [{
    title: SettingsHeaders.PHYSICAL_KEYBOARD,
    optionNames: [
      {name: OptionType.ZHUYIN_KEYBOARD_LAYOUT},
      {name: OptionType.ZHUYIN_SELECT_KEYS},
      {name: OptionType.ZHUYIN_PAGE_SIZE},
    ],
  }],
  [SettingsType.KOREAN_SETTINGS]: [{
    title: SettingsHeaders.BASIC,
    optionNames: [
      {name: OptionType.KOREAN_KEYBOARD_LAYOUT},
      {name: OptionType.KOREAN_ENABLE_SYLLABLE_INPUT},
    ],
  }],
  [SettingsType.PINYIN_FUZZY_SETTINGS]: [{
    title: SettingsHeaders.ADVANCED,
    optionNames: [{
      name: OptionType.PINYIN_ENABLE_FUZZY,
      dependentOptions: [
        OptionType.PINYIN_AN_ANG,
        OptionType.PINYIN_EN_ENG,
        OptionType.PINYIN_IAN_IANG,
        OptionType.PINYIN_K_G,
        OptionType.PINYIN_R_L,
        OptionType.PINYIN_UAN_UANG,
        OptionType.PINYIN_C_CH,
        OptionType.PINYIN_F_H,
        OptionType.PINYIN_IN_ING,
        OptionType.PINYIN_L_N,
        OptionType.PINYIN_S_SH,
        OptionType.PINYIN_Z_ZH,
      ],
    }],
  }],
  [SettingsType.PINYIN_SETTINGS]: [
    {
      title: SettingsHeaders.ADVANCED,
      optionNames: [{name: OptionType.EDIT_USER_DICT}],
    },
    {
      title: SettingsHeaders.PHYSICAL_KEYBOARD,
      optionNames: [
        {name: OptionType.XKB_LAYOUT},
        {name: OptionType.PINYIN_ENABLE_UPPER_PAGING},
        {name: OptionType.PINYIN_ENABLE_LOWER_PAGING},
        {name: OptionType.PINYIN_DEFAULT_CHINESE},
        {name: OptionType.PINYIN_FULL_WIDTH_CHARACTER},
        {name: OptionType.PINYIN_CHINESE_PUNCTUATION},
      ],
    },
  ],
  [SettingsType.BASIC_SETTINGS]: [{
    title: SettingsHeaders.VIRTUAL_KEYBOARD,
    optionNames: [
      {name: OptionType.ENABLE_SOUND_ON_KEYPRESS},
    ],
  }],
  [SettingsType.ENGLISH_BASIC_WITH_AUTOSHIFT_SETTINGS]: [{
    title: SettingsHeaders.VIRTUAL_KEYBOARD,
    optionNames: [
      {name: OptionType.ENABLE_SOUND_ON_KEYPRESS},
      {name: OptionType.VIRTUAL_KEYBOARD_ENABLE_CAPITALIZATION},
    ],
  }],
  [SettingsType.SUGGESTION_SETTINGS]: [{
    title: SettingsHeaders.SUGGESTIONS,
    optionNames:
        [{name: OptionType.PHYSICAL_KEYBOARD_ENABLE_PREDICTIVE_WRITING}],
  }],
  [SettingsType.LATIN_PHYSICAL_KEYBOARD_SETTINGS]: [{
    title: SettingsHeaders.PHYSICAL_KEYBOARD,
    optionNames: [{
      name: OptionType.PHYSICAL_KEYBOARD_AUTO_CORRECTION_LEVEL,
    }],
  }],
  [SettingsType.VIETNAMESE_TELEX_SETTINGS]: [
    {
      title: SettingsHeaders.VIETNAMESE_FLEXIBLE_TYPING_EMPTY_HEADER,
      optionNames: [
        {
          name: OptionType.VIETNAMESE_TELEX_ALLOW_FLEXIBLE_DIACRITICS,
          dependentOptions: [
            OptionType.VIETNAMESE_TELEX_NEW_STYLE_TONE_MARK_PLACEMENT,
          ],
        },
      ],
    },
    {
      title: SettingsHeaders.VIETNAMESE_SHORTHAND,
      optionNames: [
        {name: OptionType.VIETNAMESE_TELEX_INSERT_DOUBLE_HORN_ON_UO},
        {name: OptionType.VIETNAMESE_TELEX_INSERT_U_HORN_ON_W},
      ],
    },
    {
      title: SettingsHeaders.VIETNAMESE_SHOW_UNDERLINE_EMPTY_HEADER,
      optionNames: [
        {name: OptionType.VIETNAMESE_TELEX_SHOW_UNDERLINE},
      ],
    },
  ],
  [SettingsType.VIETNAMESE_VNI_SETTINGS]: [
    {
      title: SettingsHeaders.VIETNAMESE_FLEXIBLE_TYPING_EMPTY_HEADER,
      optionNames: [
        {
          name: OptionType.VIETNAMESE_VNI_ALLOW_FLEXIBLE_DIACRITICS,
          dependentOptions: [
            OptionType.VIETNAMESE_VNI_NEW_STYLE_TONE_MARK_PLACEMENT,
          ],
        },
      ],
    },
    {
      title: SettingsHeaders.VIETNAMESE_SHORTHAND,
      optionNames: [
        {name: OptionType.VIETNAMESE_VNI_INSERT_DOUBLE_HORN_ON_UO},
      ],
    },
    {
      title: SettingsHeaders.VIETNAMESE_SHOW_UNDERLINE_EMPTY_HEADER,
      optionNames: [
        {name: OptionType.VIETNAMESE_VNI_SHOW_UNDERLINE},
      ],
    },
  ],
} satisfies Record<SettingsType, Array<{
                     title: SettingsHeaders,
                     optionNames: Array<{
                       name: OptionType,
                       dependentOptions?: OptionType[],
                     }>,
                   }>>;

/**
 * @param id A first party input method ID.
 * @return The corresponding engind ID of the input method.
 */
export function getFirstPartyInputMethodEngineId(id: string): string {
  // Safety: Enforced by documentation.
  assert(isFirstPartyInputMethodId(id));
  return id.substring(FIRST_PARTY_INPUT_METHOD_ID_PREFIX.length);
}

/**
 * @param id Input method ID.
 * @return true if the input method's options page is implemented.
 */
export function hasOptionsPageInSettings(
    id: string, context: SettingsContext): boolean {
  if (!isFirstPartyInputMethodId(id)) {
    return false;
  }
  const engineId = getFirstPartyInputMethodEngineId(id);
  const inputMethodSettings = getInputMethodSettings(context);
  return !!inputMethodSettings[engineId];
}

/**
 * Generates options to be displayed in the options page, grouped by sections.
 * @param engineId Input method engine ID.
 * @return the options to be displayed.
 */
export function generateOptions(
    engineId: string, context: SettingsContext): Array<{
  title: SettingsHeaders,
  optionNames: Array<{name: OptionType, dependentOptions?: OptionType[]}>,
}> {
  const options: Array<{
    title: SettingsHeaders,
    optionNames: Array<{name: OptionType, dependentOptions?: OptionType[]}>,
  }> = [];
  const inputMethodSettings = getInputMethodSettings(context);
  const engineSettings = inputMethodSettings[engineId];
  if (engineSettings) {
    const pushedOptions = new Map<SettingsHeaders, number>();

    engineSettings.forEach((settingType) => {
      const settings = Settings[settingType];
      for (const {title, optionNames} of settings) {
        if (optionNames) {
          const optionsIndex = pushedOptions.get(title);
          if (optionsIndex === undefined) {
            pushedOptions.set(title, options.length);
            options.push({
              title,
              optionNames: [...optionNames],
            });
          } else {
            // Safety: `pushedOptions` values are always set to
            // `options.length`, and `options` is immediately pushed to, so the
            // values of `pushedOptions` must always be valid indices into
            // `options`.
            options[optionsIndex]!.optionNames.push(...optionNames);
          }
        }
      }
    });
  }

  return options;
}

/**
 * @param option The option type.
 * @return The UI type of |option|.
 */
export function getOptionUiType(option: OptionType): UiType {
  switch (option) {
    // TODO(b/191608723): Clean up switch statements.
    case OptionType.ENABLE_COMPLETION:
    case OptionType.ENABLE_DOUBLE_SPACE_PERIOD:
    case OptionType.ENABLE_GESTURE_TYPING:
    case OptionType.ENABLE_PREDICTION:
    case OptionType.ENABLE_SOUND_ON_KEYPRESS:
    case OptionType.JAPANESE_AUTOMATICALLY_SWITCH_TO_HALFWIDTH:
    case OptionType.JAPANESE_USE_SYSTEM_DICTIONARY:
    case OptionType.JAPANESE_USE_INPUT_HISTORY:
    case OptionType.JAPANESE_DISABLE_PERSONALIZED_SUGGESTIONS:
    case OptionType.JAPANESE_AUTOMATICALLY_SEND_STATISTICS_TO_GOOGLE:
    case OptionType.PHYSICAL_KEYBOARD_ENABLE_CAPITALIZATION:
    case OptionType.PHYSICAL_KEYBOARD_ENABLE_PREDICTIVE_WRITING:
    case OptionType.VIRTUAL_KEYBOARD_ENABLE_CAPITALIZATION:
    case OptionType.KOREAN_ENABLE_SYLLABLE_INPUT:
    case OptionType.PINYIN_CHINESE_PUNCTUATION:
    case OptionType.PINYIN_DEFAULT_CHINESE:
    case OptionType.PINYIN_ENABLE_FUZZY:
    case OptionType.PINYIN_ENABLE_LOWER_PAGING:
    case OptionType.PINYIN_ENABLE_UPPER_PAGING:
    case OptionType.PINYIN_FULL_WIDTH_CHARACTER:
    case OptionType.PINYIN_AN_ANG:
    case OptionType.PINYIN_EN_ENG:
    case OptionType.PINYIN_IAN_IANG:
    case OptionType.PINYIN_K_G:
    case OptionType.PINYIN_R_L:
    case OptionType.PINYIN_UAN_UANG:
    case OptionType.PINYIN_C_CH:
    case OptionType.PINYIN_F_H:
    case OptionType.PINYIN_IN_ING:
    case OptionType.PINYIN_L_N:
    case OptionType.PINYIN_S_SH:
    case OptionType.PINYIN_Z_ZH:
    case OptionType.VIETNAMESE_VNI_ALLOW_FLEXIBLE_DIACRITICS:
    case OptionType.VIETNAMESE_VNI_NEW_STYLE_TONE_MARK_PLACEMENT:
    case OptionType.VIETNAMESE_VNI_INSERT_DOUBLE_HORN_ON_UO:
    case OptionType.VIETNAMESE_VNI_SHOW_UNDERLINE:
    case OptionType.VIETNAMESE_TELEX_ALLOW_FLEXIBLE_DIACRITICS:
    case OptionType.VIETNAMESE_TELEX_NEW_STYLE_TONE_MARK_PLACEMENT:
    case OptionType.VIETNAMESE_TELEX_INSERT_DOUBLE_HORN_ON_UO:
    case OptionType.VIETNAMESE_TELEX_INSERT_U_HORN_ON_W:
    case OptionType.VIETNAMESE_TELEX_SHOW_UNDERLINE:
      return UiType.TOGGLE_BUTTON;
    case OptionType.PHYSICAL_KEYBOARD_AUTO_CORRECTION_LEVEL:
    case OptionType.VIRTUAL_KEYBOARD_AUTO_CORRECTION_LEVEL:
      return loadTimeData.getBoolean('allowAutocorrectToggle') ?
          UiType.TOGGLE_BUTTON :
          UiType.DROPDOWN;
    case OptionType.XKB_LAYOUT:
    case OptionType.JAPANESE_INPUT_MODE:
    case OptionType.JAPANESE_PUNCTUATION_STYLE:
    case OptionType.JAPANESE_SYMBOL_STYLE:
    case OptionType.JAPANESE_SPACE_INPUT_STYLE:
    case OptionType.JAPANESE_SECTION_SHORTCUT:
    case OptionType.JAPANESE_KEYMAP_STYLE:
    case OptionType.JAPANESE_SHIFT_KEY_MODE_STYLE:
    case OptionType.JAPANESE_NUMBER_OF_SUGGESTIONS:
    case OptionType.KOREAN_KEYBOARD_LAYOUT:
    case OptionType.ZHUYIN_KEYBOARD_LAYOUT:
    case OptionType.ZHUYIN_SELECT_KEYS:
    case OptionType.ZHUYIN_PAGE_SIZE:
      return UiType.DROPDOWN;
    case OptionType.EDIT_USER_DICT:
    case OptionType.JAPANESE_MANAGE_USER_DICTIONARY:
      return UiType.LINK;
    case OptionType.JAPANESE_DELETE_PERSONALIZATION_DATA:
      return UiType.SUBMENU_BUTTON;
    case OptionType.PINYIN_FUZZY_CONFIG:
      // Not implemented.
      assertNotReached();
    default:
      assertExhaustive(option);
  }
}

export function isOptionLabelTranslated(option: OptionType): option is Exclude<
    OptionType,
    OptionType.PINYIN_AN_ANG|OptionType.PINYIN_EN_ENG|OptionType
        .PINYIN_IAN_IANG|OptionType.PINYIN_K_G|OptionType.PINYIN_R_L|
    OptionType.PINYIN_UAN_UANG|OptionType.PINYIN_C_CH|
    OptionType.PINYIN_F_H|OptionType.PINYIN_IN_ING|
    OptionType.PINYIN_L_N|OptionType.PINYIN_S_SH|OptionType.PINYIN_Z_ZH> {
  switch (option) {
    // TODO(b/191608723): Clean up switch statements.
    case OptionType.PINYIN_AN_ANG:
    case OptionType.PINYIN_EN_ENG:
    case OptionType.PINYIN_IAN_IANG:
    case OptionType.PINYIN_K_G:
    case OptionType.PINYIN_R_L:
    case OptionType.PINYIN_UAN_UANG:
    case OptionType.PINYIN_C_CH:
    case OptionType.PINYIN_F_H:
    case OptionType.PINYIN_IN_ING:
    case OptionType.PINYIN_L_N:
    case OptionType.PINYIN_S_SH:
    case OptionType.PINYIN_Z_ZH:
      return false;
    default:
      return true;
  }
}

/**
 * `isOptionLabelTranslated(option)` must be true.
 * @param option The option type.
 * @return The name of the i18n string for the label of |option|.
 */
export function getOptionLabelName(option: OptionType): string {
  switch (option) {
    case OptionType.ENABLE_DOUBLE_SPACE_PERIOD:
      return 'inputMethodOptionsEnableDoubleSpacePeriod';
    case OptionType.ENABLE_GESTURE_TYPING:
      return 'inputMethodOptionsEnableGestureTyping';
    case OptionType.ENABLE_PREDICTION:
      return 'inputMethodOptionsEnablePrediction';
    case OptionType.ENABLE_SOUND_ON_KEYPRESS:
      return 'inputMethodOptionsEnableSoundOnKeypress';
    case OptionType.PHYSICAL_KEYBOARD_ENABLE_CAPITALIZATION:
    case OptionType.VIRTUAL_KEYBOARD_ENABLE_CAPITALIZATION:
      return 'inputMethodOptionsEnableCapitalization';
    case OptionType.PHYSICAL_KEYBOARD_ENABLE_PREDICTIVE_WRITING:
      return 'inputMethodOptionsPredictiveWriting';
    case OptionType.PINYIN_CHINESE_PUNCTUATION:
      return 'inputMethodOptionsPinyinChinesePunctuation';
    case OptionType.PINYIN_DEFAULT_CHINESE:
      return 'inputMethodOptionsPinyinDefaultChinese';
    case OptionType.PINYIN_ENABLE_FUZZY:
      return 'inputMethodOptionsPinyinEnableFuzzy';
    case OptionType.PINYIN_ENABLE_LOWER_PAGING:
      return 'inputMethodOptionsPinyinEnableLowerPaging';
    case OptionType.PINYIN_ENABLE_UPPER_PAGING:
      return 'inputMethodOptionsPinyinEnableUpperPaging';
    case OptionType.PINYIN_FULL_WIDTH_CHARACTER:
      return 'inputMethodOptionsPinyinFullWidthCharacter';
    case OptionType.PHYSICAL_KEYBOARD_AUTO_CORRECTION_LEVEL:
    case OptionType.VIRTUAL_KEYBOARD_AUTO_CORRECTION_LEVEL:
      return 'inputMethodOptionsAutoCorrection';
    case OptionType.JAPANESE_INPUT_MODE:
      return 'inputMethodOptionsJapaneseInputMode';
    case OptionType.JAPANESE_PUNCTUATION_STYLE:
      return 'inputMethodOptionsJapanesePunctuationStyle';
    case OptionType.JAPANESE_SYMBOL_STYLE:
      return 'inputMethodOptionsJapaneseSymbolStyle';
    case OptionType.JAPANESE_SPACE_INPUT_STYLE:
      return 'inputMethodOptionsJapaneseSpaceInputStyle';
    case OptionType.JAPANESE_SECTION_SHORTCUT:
      return 'inputMethodOptionsJapaneseSectionShortcut';
    case OptionType.JAPANESE_KEYMAP_STYLE:
      return 'inputMethodOptionsJapaneseKeymapStyle';
    case OptionType.JAPANESE_AUTOMATICALLY_SWITCH_TO_HALFWIDTH:
      return 'inputMethodOptionsJapaneseAutomaticallySwitchToHalfwidth';
    case OptionType.JAPANESE_SHIFT_KEY_MODE_STYLE:
      return 'inputMethodOptionsJapaneseShiftKeyModeStyle';
    case OptionType.JAPANESE_USE_INPUT_HISTORY:
      return 'inputMethodOptionsJapaneseUseInputHistory';
    case OptionType.JAPANESE_USE_SYSTEM_DICTIONARY:
      return 'inputMethodOptionsJapaneseUseSystemDictionary';
    case OptionType.JAPANESE_NUMBER_OF_SUGGESTIONS:
      return 'inputMethodOptionsJapaneseNumberOfSuggestions';
    case OptionType.JAPANESE_MANAGE_USER_DICTIONARY:
      return 'inputMethodOptionsJapaneseManageUserDictionary';
    case OptionType.JAPANESE_DELETE_PERSONALIZATION_DATA:
      return 'inputMethodOptionsJapaneseDeletePersonalizationData';
    case OptionType.JAPANESE_DISABLE_PERSONALIZED_SUGGESTIONS:
      return 'inputMethodOptionsJapaneseDisablePersonalizedSuggestions';
    case OptionType.JAPANESE_AUTOMATICALLY_SEND_STATISTICS_TO_GOOGLE:
      return 'inputMethodOptionsJapaneseAutomaticallySendStatisticsToGoogle';
    case OptionType.XKB_LAYOUT:
      return 'inputMethodOptionsXkbLayout';
    case OptionType.EDIT_USER_DICT:
      return 'inputMethodOptionsEditUserDict';
    case OptionType.ZHUYIN_KEYBOARD_LAYOUT:
      return 'inputMethodOptionsZhuyinKeyboardLayout';
    case OptionType.ZHUYIN_SELECT_KEYS:
      return 'inputMethodOptionsZhuyinSelectKeys';
    case OptionType.ZHUYIN_PAGE_SIZE:
      return 'inputMethodOptionsZhuyinPageSize';
    case OptionType.KOREAN_KEYBOARD_LAYOUT:
      return 'inputMethodOptionsKoreanLayout';
    case OptionType.KOREAN_ENABLE_SYLLABLE_INPUT:
      return 'inputMethodOptionsKoreanSyllableInput';
    case OptionType.VIETNAMESE_VNI_ALLOW_FLEXIBLE_DIACRITICS:
      return 'inputMethodOptionsVietnameseFlexibleTyping';
    case OptionType.VIETNAMESE_VNI_NEW_STYLE_TONE_MARK_PLACEMENT:
      return 'inputMethodOptionsVietnameseModernToneMarkPlacement';
    case OptionType.VIETNAMESE_VNI_INSERT_DOUBLE_HORN_ON_UO:
      return 'inputMethodOptionsVietnameseVniUoHookShortcut';
    case OptionType.VIETNAMESE_VNI_SHOW_UNDERLINE:
      return 'inputMethodOptionsVietnameseShowUnderline';
    case OptionType.VIETNAMESE_TELEX_ALLOW_FLEXIBLE_DIACRITICS:
      return 'inputMethodOptionsVietnameseFlexibleTyping';
    case OptionType.VIETNAMESE_TELEX_NEW_STYLE_TONE_MARK_PLACEMENT:
      return 'inputMethodOptionsVietnameseModernToneMarkPlacement';
    case OptionType.VIETNAMESE_TELEX_INSERT_DOUBLE_HORN_ON_UO:
      return 'inputMethodOptionsVietnameseTelexUoHookShortcut';
    case OptionType.VIETNAMESE_TELEX_SHOW_UNDERLINE:
      return 'inputMethodOptionsVietnameseShowUnderline';
    case OptionType.VIETNAMESE_TELEX_INSERT_U_HORN_ON_W:
      return 'inputMethodOptionsVietnameseTelexWShortcut';
    case OptionType.ENABLE_COMPLETION:
    case OptionType.PINYIN_FUZZY_CONFIG:
      // Not implemented.
      assertNotReached();
    default:
      assert(isOptionLabelTranslated(option));
      assertExhaustive(option);
  }
}

/**
 * @param option The option type.
 * @return The name of the string for the subtitle of |option|. Returns empty
 *     string if no subtitle.
 */
export function getOptionSubtitleName(option: OptionType): string {
  switch (option) {
    // TODO(b/234790486): The subtitle is not forced to the next line if it is
    // too short. You end up with something like :
    // https://screenshot.googleplex.com/8xk2BfbBXcGqhvs This likely also
    // affects the diacritics label. This is not currently an issue since both
    // string are long enough and force themself to the next line, but it may be
    // an issue in other languages or with future strings which may be shorter.
    case OptionType.JAPANESE_MANAGE_USER_DICTIONARY:
      return 'inputMethodOptionsJapaneseManageUserDictionarySubtitle';
    case OptionType.VIETNAMESE_TELEX_NEW_STYLE_TONE_MARK_PLACEMENT:
    case OptionType.VIETNAMESE_VNI_NEW_STYLE_TONE_MARK_PLACEMENT:
      return 'inputMethodOptionsVietnameseModernToneMarkPlacementDescription';
    case OptionType.VIETNAMESE_TELEX_SHOW_UNDERLINE:
    case OptionType.VIETNAMESE_VNI_SHOW_UNDERLINE:
      return 'inputMethodOptionsVietnameseShowUnderlineDescription';
    case OptionType.VIETNAMESE_TELEX_ALLOW_FLEXIBLE_DIACRITICS:
      return 'inputMethodOptionsVietnameseTelexFlexibleTypingDescription';
    case OptionType.VIETNAMESE_VNI_ALLOW_FLEXIBLE_DIACRITICS:
      return 'inputMethodOptionsVietnameseVniFlexibleTypingDescription';
    default:
      return '';
  }
}

/**
 * `isOptionLabelTranslated(option)` must be false.
 */
export function getUntranslatedOptionLabelName(option: OptionType): string {
  switch (option) {
    case OptionType.PINYIN_AN_ANG:
      return 'an_ang';
    case OptionType.PINYIN_EN_ENG:
      return 'en_eng';
    case OptionType.PINYIN_IAN_IANG:
      return 'ian_iang';
    case OptionType.PINYIN_K_G:
      return 'k_g';
    case OptionType.PINYIN_R_L:
      return 'r_l';
    case OptionType.PINYIN_UAN_UANG:
      return 'uan_uang';
    case OptionType.PINYIN_C_CH:
      return 'c_ch';
    case OptionType.PINYIN_F_H:
      return 'f_h';
    case OptionType.PINYIN_IN_ING:
      return 'in_ing';
    case OptionType.PINYIN_L_N:
      return 'l_n';
    case OptionType.PINYIN_S_SH:
      return 's_sh';
    case OptionType.PINYIN_Z_ZH:
      return 'z_zh';
    default:
      assert(!isOptionLabelTranslated(option));
      assertExhaustive(option);
  }
}

/**
 * @param option The option type.
 * @return The list of items to be displayed in the dropdown for |option|.
 */
export function getOptionMenuItems(option: OptionType):
    Array<{name?: string, value: string | number}> {
  switch (option) {
    case OptionType.PHYSICAL_KEYBOARD_AUTO_CORRECTION_LEVEL:
    case OptionType.VIRTUAL_KEYBOARD_AUTO_CORRECTION_LEVEL:
      return [
        {value: 0, name: 'inputMethodOptionsAutoCorrectionOff'},
        {value: 1, name: 'inputMethodOptionsAutoCorrectionModest'},
        {value: 2, name: 'inputMethodOptionsAutoCorrectionAggressive'},
      ];
    case OptionType.XKB_LAYOUT:
      return [
        {value: 'US', name: 'inputMethodOptionsUsKeyboard'},
        {value: 'Dvorak', name: 'inputMethodOptionsDvorakKeyboard'},
        {value: 'Colemak', name: 'inputMethodOptionsColemakKeyboard'},
      ];
    case OptionType.ZHUYIN_KEYBOARD_LAYOUT:
      return [
        {value: 'Default', name: 'inputMethodOptionsZhuyinLayoutDefault'},
        {value: 'IBM', name: 'inputMethodOptionsZhuyinLayoutIBM'},
        {value: 'Eten', name: 'inputMethodOptionsZhuyinLayoutEten'},
      ];
    case OptionType.ZHUYIN_SELECT_KEYS:
      // Zhuyin select keys correspond to physical keys so are not
      // translated.
      return [
        {value: '1234567890'},
        {value: 'asdfghjkl;'},
        {value: 'asdfzxcv89'},
        {value: 'asdfjkl789'},
        {value: '1234qweras'},
      ];
    case OptionType.ZHUYIN_PAGE_SIZE:
      // Zhuyin page size is just a number, so is not translated.
      return [
        {value: '10'},
        {value: '9'},
        {value: '8'},
      ];
    case OptionType.JAPANESE_INPUT_MODE:
      return [
        {
          value: JapaneseInputMode.KANA,
          name: 'inputMethodOptionsJapaneseInputModeKana',
        },
        {
          value: JapaneseInputMode.ROMAJI,
          name: 'inputMethodOptionsJapaneseInputModeRomaji',
        },
      ];
    case OptionType.JAPANESE_PUNCTUATION_STYLE:
      return [
        {
          value: JapanesePunctuationStyle.KUTEN_TOUTEN,
          name: 'inputMethodOptionsJapanesePunctuationStyleKutenTouten',
        },
        {
          value: JapanesePunctuationStyle.COMMA_PERIOD,
          name: 'inputMethodOptionsJapanesePunctuationStyleCommaPeriod',
        },
        {
          value: JapanesePunctuationStyle.KUTEN_PERIOD,
          name: 'inputMethodOptionsJapanesePunctuationStyleKutenPeriod',
        },
        {
          value: JapanesePunctuationStyle.COMMA_TOUTEN,
          name: 'inputMethodOptionsJapanesePunctuationStyleCommaTouten',
        },
      ];
    case OptionType.JAPANESE_SYMBOL_STYLE:
      return [
        {
          value: JapaneseSymbolStyle.CORNER_BRACKET_MIDDLE_DOT,
          name: 'inputMethodOptionsJapaneseSymbolStyleCornerBracketMiddleDot',
        },
        {
          value: JapaneseSymbolStyle.SQUARE_BRACKET_SLASH,
          name: 'inputMethodOptionsJapaneseSymbolStyleSquareBracketSlash',
        },
        {
          value: JapaneseSymbolStyle.CORNER_BRACKET_SLASH,
          name: 'inputMethodOptionsJapaneseSymbolStyleCornerBracketSlash',
        },
        {
          value: JapaneseSymbolStyle.SQUARE_BRACKET_MIDDLE_DOT,
          name: 'inputMethodOptionsJapaneseSymbolStyleSquareBracketMiddleDot',
        },
      ];
    case OptionType.JAPANESE_SPACE_INPUT_STYLE:
      return [
        {
          value: JapaneseSpaceInputStyle.INPUT_MODE,
          name: 'inputMethodOptionsJapaneseSpaceInputStyleInputMode',
        },
        {
          value: JapaneseSpaceInputStyle.FULLWIDTH,
          name: 'inputMethodOptionsJapaneseSpaceInputStyleFullwidth',
        },
        {
          value: JapaneseSpaceInputStyle.HALFWIDTH,
          name: 'inputMethodOptionsJapaneseSpaceInputStyleHalfwidth',
        },
      ];
    case OptionType.JAPANESE_SECTION_SHORTCUT:
      return [
        {
          value: JapaneseSectionShortcut.NO_SHORTCUT,
          name: 'inputMethodOptionsJapaneseSectionShortcutNoShortcut',
        },
        {
          value: JapaneseSectionShortcut.DIGITS_123456789,
          name: 'inputMethodOptionsJapaneseSectionShortcut123456789',
        },
        {
          value: JapaneseSectionShortcut.ASDFGHJKL,
          name: 'inputMethodOptionsJapaneseSectionShortcutAsdfghjkl',
        },
      ];
    case OptionType.JAPANESE_KEYMAP_STYLE:
      return [
        {
          value: JapaneseKeymapStyle.CUSTOM,
          name: 'inputMethodOptionsJapaneseKeymapStyleCustom',
        },
        {
          value: JapaneseKeymapStyle.ATOK,
          name: 'inputMethodOptionsJapaneseKeymapStyleAtok',
        },
        {
          value: JapaneseKeymapStyle.MS_IME,
          name: 'inputMethodOptionsJapaneseKeymapStyleMsIme',
        },
        {
          value: JapaneseKeymapStyle.KOTOERI,
          name: 'inputMethodOptionsJapaneseKeymapStyleKotoeri',
        },
        {
          value: JapaneseKeymapStyle.MOBILE,
          name: 'inputMethodOptionsJapaneseKeymapStyleMobile',
        },
        {
          value: JapaneseKeymapStyle.CHROME_OS,
          name: 'inputMethodOptionsJapaneseKeymapStyleChromeOs',
        },
      ];
    case OptionType.JAPANESE_SHIFT_KEY_MODE_STYLE:
      return [
        {
          value: JapaneseShiftKeyModeStyle.OFF,
          name: 'inputMethodOptionsJapaneseShiftKeyModeStyleOff',
        },
        {
          value: JapaneseShiftKeyModeStyle.ALPHANUMERIC,
          name: 'inputMethodOptionsJapaneseShiftKeyModeStyleAlphanumeric',
        },
        {
          value: JapaneseShiftKeyModeStyle.KATAKANA,
          name: 'inputMethodOptionsJapaneseShiftKeyModeStyleKatakana',
        },
      ];
    case OptionType.JAPANESE_NUMBER_OF_SUGGESTIONS:
      return [
        {
          value: 1,
        },
        {
          value: 2,
        },
        {
          value: 3,
        },
        {
          value: 4,
        },
        {
          value: 5,
        },
        {
          value: 6,
        },
        {
          value: 7,
        },
        {
          value: 8,
        },
        {
          value: 9,
        },
      ];
    case OptionType.KOREAN_KEYBOARD_LAYOUT:
      // Korean layout strings are already Korean / English, so not
      // translated. The literal values of these strings are critical.
      return [
        {value: '2 Set / 두벌식'},
        {value: '3 Set (390) / 세벌식 (390)'},
        {value: '3 Set (Final) / 세벌식 (최종)'},
        {value: '3 Set (No Shift) / 세벌식 (순아래)'},
        {value: '2 Set (Old Hangul) / 두벌식 (옛글)'},
        {value: '3 Set (Old Hangul) / 세벌식 (옛글)'},
      ];
    default:
      return [];
  }
}

/**
 * Used to work around dropdowns in the UI always storing their values as
 * strings.
 * @param option The option type.
 * @return true if the value for |option| is a number and |option| is a
 *     dropdown.
 */
// TODO(b/265557721): Remove this when we remove Polymer's two-way native
// binding of value changes.
export function shouldStoreAsNumber(option: OptionType):
    option is OptionType.PHYSICAL_KEYBOARD_AUTO_CORRECTION_LEVEL|
    OptionType.VIRTUAL_KEYBOARD_AUTO_CORRECTION_LEVEL|
    OptionType.JAPANESE_NUMBER_OF_SUGGESTIONS {
  return option === OptionType.PHYSICAL_KEYBOARD_AUTO_CORRECTION_LEVEL ||
      option === OptionType.VIRTUAL_KEYBOARD_AUTO_CORRECTION_LEVEL ||
      option === OptionType.JAPANESE_NUMBER_OF_SUGGESTIONS;
}
/**
 * @param option The option type.
 * @return The url to open for |option|, returns undefined if |option| does not
 *     have a url.
 */
export function getOptionUrl(option: OptionType): Route|undefined {
  if (option === OptionType.EDIT_USER_DICT) {
    return routes.OS_LANGUAGES_EDIT_DICTIONARY;
  }
  if (option === OptionType.JAPANESE_MANAGE_USER_DICTIONARY) {
    return routes.OS_LANGUAGES_JAPANESE_MANAGE_USER_DICTIONARY;
  }
  return undefined;
}

/**
 * @param option The option type.
 * @return The submenu button type for |option|, returns undefined if |option|
 *     does not have a submenu button type.
 */
export function getSubmenuButtonType(option: OptionType): SubmenuButton|
    undefined {
  if (option === OptionType.JAPANESE_DELETE_PERSONALIZATION_DATA) {
    return SubmenuButton.JAPANESE_DELETE_PERSONALIZATION_DATA;
  }
  return undefined;
}

/**
 * @param id Input method ID.
 * @return true if |id| is a first party input method ID.
 */
function isFirstPartyInputMethodId(id: string): boolean {
  return id.startsWith(FIRST_PARTY_INPUT_METHOD_ID_PREFIX);
}
