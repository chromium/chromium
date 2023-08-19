// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Specifies the parameters that can be used to alter the settings returned via
 * the getInputMethodSettings function.
 */
export interface SettingsContext {
  isPhysicalKeyboardAutocorrectAllowed: boolean;
  isPhysicalKeyboardPredictiveWritingAllowed: boolean;
  isJapaneseSettingsAllowed: boolean;
  isVietnameseFirstPartyInputSettingsAllowed: boolean;
}

/**
 * Type of settings to use for an input method.
 */
export enum SettingsType {
  LATIN_SETTINGS = 0,
  ZHUYIN_SETTINGS = 1,
  KOREAN_SETTINGS = 2,
  PINYIN_SETTINGS = 3,
  PINYIN_FUZZY_SETTINGS = 4,
  BASIC_SETTINGS = 5,
  ENGLISH_BASIC_WITH_AUTOSHIFT_SETTINGS = 6,
  SUGGESTION_SETTINGS = 7,
  JAPANESE_SETTINGS = 9,
  LATIN_PHYSICAL_KEYBOARD_SETTINGS = 10,
  VIETNAMESE_VNI_SETTINGS = 11,
  VIETNAMESE_TELEX_SETTINGS = 12,
}

type SettingsMap = Partial<Record<string, SettingsType[]>>;

export function getInputMethodSettings(context: SettingsContext): SettingsMap {
  const latinSettings = context.isPhysicalKeyboardAutocorrectAllowed ?
      [
        SettingsType.LATIN_PHYSICAL_KEYBOARD_SETTINGS,
        SettingsType.LATIN_SETTINGS,
      ] :
      [
        SettingsType.LATIN_SETTINGS,
      ];
  const usEnglishSettings = context.isPhysicalKeyboardPredictiveWritingAllowed ?
      [...latinSettings, SettingsType.SUGGESTION_SETTINGS] :
      [...latinSettings];
  const settingsMap: SettingsMap = {
    // NOTE: Please group by SettingsType, and keep entries sorted
    // alphabetically by ID within each group, just for readability.

    // The values here should be kept in sync with IsAutocorrectSupported in
    // chrome/browser/ash/input_method/input_method_settings.cc
    // LATIN_SETTINGS
    'xkb:be::fra': latinSettings,
    'xkb:be::ger': latinSettings,
    'xkb:be::nld': latinSettings,
    'xkb:br::por': latinSettings,
    'xkb:ca::fra': latinSettings,
    'xkb:ca:eng:eng': latinSettings,
    'xkb:ca:multix:fra': latinSettings,
    'xkb:ch::ger': latinSettings,
    'xkb:ch:fr:fra': latinSettings,
    'xkb:de::ger': latinSettings,
    'xkb:de:neo:ger': latinSettings,
    'xkb:dk::dan': latinSettings,
    'xkb:es::spa': latinSettings,
    'xkb:fi::fin': latinSettings,
    'xkb:fr::fra': latinSettings,
    'xkb:fr:bepo:fra': latinSettings,
    'xkb:gb:dvorak:eng': latinSettings,
    'xkb:gb:extd:eng': latinSettings,
    'xkb:it::ita': latinSettings,
    'xkb:latam::spa': latinSettings,
    'xkb:no::nob': latinSettings,
    'xkb:pl::pol': latinSettings,
    'xkb:pt::por': latinSettings,
    'xkb:se::swe': latinSettings,
    'xkb:tr::tur': latinSettings,
    'xkb:tr:f:tur': latinSettings,
    'xkb:us:intl:nld': latinSettings,
    'xkb:us:intl:por': latinSettings,
    'xkb:us:intl_pc:nld': latinSettings,
    'xkb:us:intl_pc:por': latinSettings,

    // US English variant settings
    'xkb:us::eng': usEnglishSettings,
    'xkb:us:altgr-intl:eng': usEnglishSettings,
    'xkb:us:colemak:eng': usEnglishSettings,
    'xkb:us:dvorak:eng': usEnglishSettings,
    'xkb:us:dvp:eng': usEnglishSettings,
    'xkb:us:intl:eng': usEnglishSettings,
    'xkb:us:intl_pc:eng': usEnglishSettings,
    'xkb:us:workman-intl:eng': usEnglishSettings,
    'xkb:us:workman:eng': usEnglishSettings,

    // ZHUYIN_SETTINGS
    'zh-hant-t-i0-und': [SettingsType.ZHUYIN_SETTINGS],

    // KOREAN_SETTINGS
    'ko-t-i0-und': [SettingsType.KOREAN_SETTINGS],

    // PINYIN_SETTINGS
    'zh-hant-t-i0-pinyin': [SettingsType.PINYIN_SETTINGS],

    // PINYIN_FUZZY_SETTINGS
    'zh-t-i0-pinyin':
        [SettingsType.PINYIN_SETTINGS, SettingsType.PINYIN_FUZZY_SETTINGS],

    // BASIC_SETTINGS
    'xkb:am:phonetic:arm': [SettingsType.BASIC_SETTINGS],
    'xkb:bg::bul': [SettingsType.BASIC_SETTINGS],
    'xkb:bg:phonetic:bul': [SettingsType.BASIC_SETTINGS],
    'xkb:by::bel': [SettingsType.BASIC_SETTINGS],
    'xkb:cz::cze': [SettingsType.BASIC_SETTINGS],
    'xkb:cz:qwerty:cze': [SettingsType.BASIC_SETTINGS],
    'xkb:ee::est': [SettingsType.BASIC_SETTINGS],
    'xkb:es:cat:cat': [SettingsType.BASIC_SETTINGS],
    'xkb:fo::fao': [SettingsType.BASIC_SETTINGS],
    'xkb:ge::geo': [SettingsType.BASIC_SETTINGS],
    'xkb:gr::gre': [SettingsType.BASIC_SETTINGS],
    'xkb:hr::scr': [SettingsType.BASIC_SETTINGS],
    'xkb:hu::hun': [SettingsType.BASIC_SETTINGS],
    'xkb:hu:qwerty:hun': [SettingsType.BASIC_SETTINGS],
    'xkb:ie::ga': [SettingsType.BASIC_SETTINGS],
    'xkb:il::heb': [SettingsType.BASIC_SETTINGS],
    'xkb:is::ice': [SettingsType.BASIC_SETTINGS],
    'xkb:jp::jpn': [SettingsType.BASIC_SETTINGS],
    'xkb:kz::kaz': [SettingsType.BASIC_SETTINGS],
    'xkb:lt::lit': [SettingsType.BASIC_SETTINGS],
    'xkb:lv:apostrophe:lav': [SettingsType.BASIC_SETTINGS],
    'xkb:mk::mkd': [SettingsType.BASIC_SETTINGS],
    'xkb:mn::mon': [SettingsType.BASIC_SETTINGS],
    'xkb:mt::mlt': [SettingsType.BASIC_SETTINGS],
    'xkb:ro::rum': [SettingsType.BASIC_SETTINGS],
    'xkb:ro:std:rum': [SettingsType.BASIC_SETTINGS],
    'xkb:rs::srp': [SettingsType.BASIC_SETTINGS],
    'xkb:ru::rus': [SettingsType.BASIC_SETTINGS],
    'xkb:ru:phonetic:rus': [SettingsType.BASIC_SETTINGS],
    'xkb:si::slv': [SettingsType.BASIC_SETTINGS],
    'xkb:sk::slo': [SettingsType.BASIC_SETTINGS],
    'xkb:ua::ukr': [SettingsType.BASIC_SETTINGS],
    'xkb:us::fil': [SettingsType.BASIC_SETTINGS],
    'xkb:us::ind': [SettingsType.BASIC_SETTINGS],
    'xkb:us::msa': [SettingsType.BASIC_SETTINGS],

    // ENGLISH_BASIC_WITH_AUTOSHIFT_SETTINGS
    'xkb:in::eng': [SettingsType.ENGLISH_BASIC_WITH_AUTOSHIFT_SETTINGS],
    'xkb:pk::eng': [SettingsType.ENGLISH_BASIC_WITH_AUTOSHIFT_SETTINGS],
    'xkb:za:gb:eng': [SettingsType.ENGLISH_BASIC_WITH_AUTOSHIFT_SETTINGS],

  };

  // MOZC settings
  if (context.isJapaneseSettingsAllowed) {
    settingsMap['nacl_mozc_jp'] = [SettingsType.JAPANESE_SETTINGS];
    settingsMap['nacl_mozc_us'] = [SettingsType.JAPANESE_SETTINGS];
  }

  // Vietnamese first party input
  if (context.isVietnameseFirstPartyInputSettingsAllowed) {
    settingsMap['vkd_vi_telex'] = [SettingsType.VIETNAMESE_TELEX_SETTINGS];
    settingsMap['vkd_vi_vni'] = [SettingsType.VIETNAMESE_VNI_SETTINGS];
  }

  return settingsMap;
}
