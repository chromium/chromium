// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Types of controller
 * @enum {number}
 */
export const SettingsTypes = {
  LATIN_SETTINGS: 0,
};
/**
 * The string keys are the input methods ids.
 * @type {Object<string,!Array<!SettingsTypes>>}
 */
export const inputMethodSettings = {
  'xkb:us::eng': [SettingsTypes.LATIN_SETTINGS],
  'xkb:us:intl:eng': [SettingsTypes.LATIN_SETTINGS],
  'xkb:us:intl_pc:eng': [SettingsTypes.LATIN_SETTINGS],
  'xkb:us:intl:nld': [SettingsTypes.LATIN_SETTINGS],
  'xkb:us:intl_pc:nld': [SettingsTypes.LATIN_SETTINGS],
  'xkb:us:intl:por': [SettingsTypes.LATIN_SETTINGS],
  'xkb:us:intl_pc:por': [SettingsTypes.LATIN_SETTINGS],
  'xkb:us:altgr-intl:eng': [SettingsTypes.LATIN_SETTINGS],
  'xkb:us:dvorak:eng': [SettingsTypes.LATIN_SETTINGS],
  'xkb:us:dvp:eng': [SettingsTypes.LATIN_SETTINGS],
  'xkb:us:colemak:eng': [SettingsTypes.LATIN_SETTINGS],
  'xkb:us:workman:eng': [SettingsTypes.LATIN_SETTINGS],
  'xkb:us:workman-intl:eng': [SettingsTypes.LATIN_SETTINGS],
  'xkb:be::nld': [SettingsTypes.LATIN_SETTINGS],
  'xkb:fr::fra': [SettingsTypes.LATIN_SETTINGS],
  'xkb:fr:bepo:fra': [SettingsTypes.LATIN_SETTINGS],
  'xkb:be::fra': [SettingsTypes.LATIN_SETTINGS],
  'xkb:ca::fra': [SettingsTypes.LATIN_SETTINGS],
  'xkb:ch:fr:fra': [SettingsTypes.LATIN_SETTINGS],
  'xkb:ca:multix:fra': [SettingsTypes.LATIN_SETTINGS],
  'xkb:de::ger': [SettingsTypes.LATIN_SETTINGS],
  'xkb:de:neo:ger': [SettingsTypes.LATIN_SETTINGS],
  'xkb:be::ger': [SettingsTypes.LATIN_SETTINGS],
  'xkb:ch::ger': [SettingsTypes.LATIN_SETTINGS],
  'xkb:br::por': [SettingsTypes.LATIN_SETTINGS],
  'xkb:ca:eng:eng': [SettingsTypes.LATIN_SETTINGS],
  'xkb:es::spa': [SettingsTypes.LATIN_SETTINGS],
  'xkb:dk::dan': [SettingsTypes.LATIN_SETTINGS],
  'xkb:latam::spa': [SettingsTypes.LATIN_SETTINGS],
  'xkb:gb:extd:eng': [SettingsTypes.LATIN_SETTINGS],
  'xkb:gb:dvorak:eng': [SettingsTypes.LATIN_SETTINGS],
  'xkb:fi::fin': [SettingsTypes.LATIN_SETTINGS],
  'xkb:it::ita': [SettingsTypes.LATIN_SETTINGS],
  'xkb:no::nob': [SettingsTypes.LATIN_SETTINGS],
  'xkb:pl::pol': [SettingsTypes.LATIN_SETTINGS],
  'xkb:pt::por': [SettingsTypes.LATIN_SETTINGS],
  'xkb:se::swe': [SettingsTypes.LATIN_SETTINGS],
  'xkb:tr::tur': [SettingsTypes.LATIN_SETTINGS],
  'xkb:tr:f:tur': [SettingsTypes.LATIN_SETTINGS],
};
