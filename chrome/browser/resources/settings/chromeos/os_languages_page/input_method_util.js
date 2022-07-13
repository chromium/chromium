// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';

import {Route} from '../../router.js';
import {routes} from '../os_route.js';

import {getInputMethodSettings, SettingsType} from './input_method_settings.js';

/**
 * @fileoverview constants related to input method options.
 */
/**
 * The prefix string shared by all first party input method ID.
 * @private @const
 */
export const FIRST_PARTY_INPUT_METHOD_ID_PREFIX =
    '_comp_ime_jkghodnilhceideoidjikpgommlajknk';

/**
 * All possible keyboard layouts. Should match Google3.
 *
 * @enum {string}
 */
const KeyboardLayout = {
  STANDARD: 'Default',
  GINYIEH: 'Gin Yieh',
  ETEN: 'Eten',
  IBM: 'IBM',
  HSU: 'Hsu',
  ETEN26: 'Eten 26',
  SET2: '2 Set / 두벌식',
  SET2Y: '2 Set (Old Hangul) / 두벌식 (옛글)',
  SET390: '3 Set (390) / 세벌식 (390)',
  SET3_FINAL: '3 Set (Final) / 세벌식 (최종)',
  SET3_SUN: '3 Set (No Shift) / 세벌식 (순아래)',
  SET3_YET: '3 Set (Old Hangul) / 세벌식 (옛글)',
  XKB_US: 'US',
  XKB_DVORAK: 'Dvorak',
  XKB_COLEMAK: 'Colemak',
};

/**
 * All possible options on options pages. Should match Gooogle3.
 *
 * @enum {string}
 */
export const OptionType = {
  EDIT_USER_DICT: 'editUserDict',
  ENABLE_COMPLETION: 'enableCompletion',
  ENABLE_DOUBLE_SPACE_PERIOD: 'enableDoubleSpacePeriod',
  ENABLE_GESTURE_TYPING: 'enableGestureTyping',
  ENABLE_PREDICTION: 'enablePrediction',
  ENABLE_SOUND_ON_KEYPRESS: 'enableSoundOnKeypress',
  PHYSICAL_KEYBOARD_AUTO_CORRECTION_LEVEL:
      'physicalKeyboardAutoCorrectionLevel',
  PHYSICAL_KEYBOARD_ENABLE_CAPITALIZATION:
      'physicalKeyboardEnableCapitalization',
  PHYSICAL_KEYBOARD_ENABLE_PREDICTIVE_WRITING:
      'physicalKeyboardEnablePredictiveWriting',
  VIRTUAL_KEYBOARD_AUTO_CORRECTION_LEVEL: 'virtualKeyboardAutoCorrectionLevel',
  VIRTUAL_KEYBOARD_ENABLE_CAPITALIZATION: 'virtualKeyboardEnableCapitalization',
  XKB_LAYOUT: 'xkbLayout',
  // Options for Korean input method.
  KOREAN_ENABLE_SYLLABLE_INPUT: 'koreanEnableSyllableInput',
  KOREAN_KEYBOARD_LAYOUT: 'koreanKeyboardLayout',
  // Options for pinyin input method.
  PINYIN_CHINESE_PUNCTUATION: 'pinyinChinesePunctuation',
  PINYIN_DEFAULT_CHINESE: 'pinyinDefaultChinese',
  PINYIN_ENABLE_FUZZY: 'pinyinEnableFuzzy',
  PINYIN_ENABLE_LOWER_PAGING: 'pinyinEnableLowerPaging',
  PINYIN_ENABLE_UPPER_PAGING: 'pinyinEnableUpperPaging',
  PINYIN_FULL_WIDTH_CHARACTER: 'pinyinFullWidthCharacter',
  PINYIN_FUZZY_CONFIG: 'pinyinFuzzyConfig',
  PINYIN_EN_ENG: 'en:eng',
  PINYIN_AN_ANG: 'an:ang',
  PINYIN_IAN_IANG: 'ian:iang',
  PINYIN_K_G: 'k:g',
  PINYIN_R_L: 'r:l',
  PINYIN_UAN_UANG: 'uan:uang',
  PINYIN_C_CH: 'c:ch',
  PINYIN_F_H: 'f:h',
  PINYIN_IN_ING: 'in:ing',
  PINYIN_L_N: 'l:n',
  PINYIN_S_SH: 's:sh',
  PINYIN_Z_ZH: 'z:zh',
  // Options for zhuyin input method.
  ZHUYIN_KEYBOARD_LAYOUT: 'zhuyinKeyboardLayout',
  ZHUYIN_PAGE_SIZE: 'zhuyinPageSize',
  ZHUYIN_SELECT_KEYS: 'zhuyinSelectKeys',
};

/**
 * Default values for each option type.
 *
 * WARNING: Keep this in sync with corresponding Google3 file for extension.
 *
 * @type {Object<OptionType, *>}
 * @const
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
  [OptionType.XKB_LAYOUT]: 'US',
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
  [OptionType.ZHUYIN_PAGE_SIZE]: 10,
  [OptionType.ZHUYIN_SELECT_KEYS]: '1234567890',
};

/**
 * All possible UI elements for options.
 *
 * @enum {string}
 */
export const UiType = {
  TOGGLE_BUTTON: 'toggleButton',
  DROPDOWN: 'dropdown',
  LINK: 'link',
};

/**
 * All possible Settings headers
 *
 * @enum {string}
 */
const SettingsHeaders = {
  BASIC: 'basic',
  ADVANCED: 'advanced',
  PHYSICAL_KEYBOARD: 'physicalKeyboard',
  VIRTUAL_KEYBOARD: 'virtualKeyboard',
  SUGGESTIONS: 'suggestions',
};

/**
 * Contents of the settings page for different settings types.
 * These should be in the order they are expected to appear in
 * the actual settings pages.
 *
 * @type {Object<SettingsType, !Array<!{title: SettingsHeaders, optionNames:
 * !Array<OptionType>}>>}
 */
const Settings = {
  [SettingsType.LATIN_SETTINGS]: [
    {
      title: SettingsHeaders.PHYSICAL_KEYBOARD,
      optionNames: [{
        name: OptionType.PHYSICAL_KEYBOARD_AUTO_CORRECTION_LEVEL,
      }],
    },
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
  [SettingsType.ENGLISH_SOUTH_AFRICA_SETTINGS]: [{
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
};

/**
 * @param {string} id Input method ID.
 * @return {string} The corresponding engind ID of the input method.
 */
export function getFirstPartyInputMethodEngineId(id) {
  assert(isFirstPartyInputMethodId_(id));
  return id.substring(FIRST_PARTY_INPUT_METHOD_ID_PREFIX.length);
}

/**
 * @param {string} id Input method ID.
 * @param {boolean} predictiveWritingEnabled .
 * @return {boolean} true if the input method's options page is implemented.
 */
export function hasOptionsPageInSettings(id, predictiveWritingEnabled) {
  if (!isFirstPartyInputMethodId_(id)) {
    return false;
  }
  const engineId = getFirstPartyInputMethodEngineId(id);

  const inputMethodSettings = getInputMethodSettings(predictiveWritingEnabled);
  return !!inputMethodSettings[engineId];
}

/**
 * Generates options to be displayed in the options page, grouped by sections.
 * @param {string} engineId Input method engine ID.
 * @param {boolean} predictiveWritingEnabled .
 * @return {!Array<!{title: string, optionNames:
 *     !Array<OptionType>}>} the options to be
 *     displayed.
 */
export function generateOptions(engineId, predictiveWritingEnabled) {
  const options = [];

  const inputMethodSettings = getInputMethodSettings(predictiveWritingEnabled);
  const engineSettings = inputMethodSettings[engineId];
  if (engineSettings) {
    const pushedOptions = {};

    engineSettings.forEach((settingType) => {
      const settings = Settings[settingType];
      for (const {title, optionNames} of settings) {
        if (optionNames) {
          if (pushedOptions[title] === undefined) {
            pushedOptions[title] = options.length;
            options.push({
              title,
              optionNames: [...optionNames],
            });
          } else {
            options[pushedOptions[title]].optionNames.push(...optionNames);
          }
        }
      }
    });
  }

  return options;
}

/**
 * @param {!OptionType} option The option type.
 * @return {UiType} The UI type of |option|.
 */
export function getOptionUiType(option) {
  switch (option) {
    // TODO(b/191608723): Clean up switch statements.
    case OptionType.ENABLE_COMPLETION:
    case OptionType.ENABLE_DOUBLE_SPACE_PERIOD:
    case OptionType.ENABLE_GESTURE_TYPING:
    case OptionType.ENABLE_PREDICTION:
    case OptionType.ENABLE_SOUND_ON_KEYPRESS:
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
      return UiType.TOGGLE_BUTTON;
    case OptionType.PHYSICAL_KEYBOARD_AUTO_CORRECTION_LEVEL:
    case OptionType.VIRTUAL_KEYBOARD_AUTO_CORRECTION_LEVEL:
    case OptionType.XKB_LAYOUT:
    case OptionType.KOREAN_KEYBOARD_LAYOUT:
    case OptionType.ZHUYIN_KEYBOARD_LAYOUT:
    case OptionType.ZHUYIN_SELECT_KEYS:
    case OptionType.ZHUYIN_PAGE_SIZE:
      return UiType.DROPDOWN;
    case OptionType.EDIT_USER_DICT:
      return UiType.LINK;
    default:
      assertNotReached();
  }
}
export function isOptionLabelTranslated(option) {
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
 * @param {!OptionType} option The option type.
 * @return {string} The name of the i18n string for the label of |option|.
 */
export function getOptionLabelName(option) {
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
    default:
      assertNotReached();
  }
}
export function getUntranslatedOptionLabelName(option) {
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
      assertNotReached();
  }
}

/**
 * @param {!OptionType} option The option type.
 * @return {!Array<!{value: *, name: string}>} The list of items to be
 *     displayed in the dropdown for |option|.
 */
export function getOptionMenuItems(option) {
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
 * @param {!OptionType} option The option type.
 * @return {boolean} true if the value for |option| is a number.
 */
export function isNumberValue(option) {
  return option === OptionType.PHYSICAL_KEYBOARD_AUTO_CORRECTION_LEVEL ||
      option === OptionType.VIRTUAL_KEYBOARD_AUTO_CORRECTION_LEVEL;
}

/**
 * @param {!OptionType} option The option type.
 * @return {Route|undefined} The url to open for |option|, returns
 *     undefined if |option| does not have a url.
 */
export function getOptionUrl(option) {
  if (option === OptionType.EDIT_USER_DICT) {
    return routes.OS_LANGUAGES_EDIT_DICTIONARY;
  }
  return undefined;
}

  /**
   * @param {string} id Input method ID.
   * @return {boolean} true if |id| is a first party input method ID.
   * @private
   */
  function isFirstPartyInputMethodId_(id) {
    return id.startsWith(FIRST_PARTY_INPUT_METHOD_ID_PREFIX);
  }
