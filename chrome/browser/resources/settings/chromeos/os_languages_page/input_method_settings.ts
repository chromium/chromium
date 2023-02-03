// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
  PK_DIACRITICS_SETTINGS = 8,
  JAPANESE_SETTINGS = 9,
}

type SettingsMap = Partial<Record<string, SettingsType[]>>;

export function getInputMethodSettings(
    predictiveWritingEnabled: boolean,
    physicalKeyboardDiacriticsEnabled: boolean,
    isJapaneseSettingsEnabled: boolean): SettingsMap {
  const usEnglishSettings = [SettingsType.LATIN_SETTINGS];
  if (predictiveWritingEnabled) {
    usEnglishSettings.push(SettingsType.SUGGESTION_SETTINGS);
  }
  if (physicalKeyboardDiacriticsEnabled) {
    usEnglishSettings.push(SettingsType.PK_DIACRITICS_SETTINGS);
  }
  const settingsMap: SettingsMap = {
    // NOTE: Please group by SettingsType, and keep entries sorted
    // alphabetically
    // by ID within each group, just for readability.

    // LATIN_SETTINGS
    'xkb:be::fra': [SettingsType.LATIN_SETTINGS],
    'xkb:be::ger': [SettingsType.LATIN_SETTINGS],
    'xkb:be::nld': [SettingsType.LATIN_SETTINGS],
    'xkb:br::por': [SettingsType.LATIN_SETTINGS],
    'xkb:ca::fra': [SettingsType.LATIN_SETTINGS],
    'xkb:ca:eng:eng': [SettingsType.LATIN_SETTINGS],
    'xkb:ca:multix:fra': [SettingsType.LATIN_SETTINGS],
    'xkb:ch::ger': [SettingsType.LATIN_SETTINGS],
    'xkb:ch:fr:fra': [SettingsType.LATIN_SETTINGS],
    'xkb:de::ger': [SettingsType.LATIN_SETTINGS],
    'xkb:de:neo:ger': [SettingsType.LATIN_SETTINGS],
    'xkb:dk::dan': [SettingsType.LATIN_SETTINGS],
    'xkb:es::spa': [SettingsType.LATIN_SETTINGS],
    'xkb:fi::fin': [SettingsType.LATIN_SETTINGS],
    'xkb:fr::fra': [SettingsType.LATIN_SETTINGS],
    'xkb:fr:bepo:fra': [SettingsType.LATIN_SETTINGS],
    'xkb:gb:dvorak:eng': [SettingsType.LATIN_SETTINGS],
    'xkb:gb:extd:eng': [SettingsType.LATIN_SETTINGS],
    'xkb:it::ita': [SettingsType.LATIN_SETTINGS],
    'xkb:latam::spa': [SettingsType.LATIN_SETTINGS],
    'xkb:no::nob': [SettingsType.LATIN_SETTINGS],
    'xkb:pl::pol': [SettingsType.LATIN_SETTINGS],
    'xkb:pt::por': [SettingsType.LATIN_SETTINGS],
    'xkb:se::swe': [SettingsType.LATIN_SETTINGS],
    'xkb:tr::tur': [SettingsType.LATIN_SETTINGS],
    'xkb:tr:f:tur': [SettingsType.LATIN_SETTINGS],
    'xkb:us:intl:nld': [SettingsType.LATIN_SETTINGS],
    'xkb:us:intl:por': [SettingsType.LATIN_SETTINGS],
    'xkb:us:intl_pc:nld': [SettingsType.LATIN_SETTINGS],
    'xkb:us:intl_pc:por': [SettingsType.LATIN_SETTINGS],

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
  if (isJapaneseSettingsEnabled) {
    settingsMap['nacl_mozc_jp'] = [SettingsType.JAPANESE_SETTINGS];
    settingsMap['nacl_mozc_us'] = [SettingsType.JAPANESE_SETTINGS];
  }
  return settingsMap;
}
