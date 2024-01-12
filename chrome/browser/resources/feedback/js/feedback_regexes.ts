// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Regular expression to check for all variants of blu[e]toot[h] with or without
 * space between the words; for BT when used as an individual word, or as two
 * individual characters, and for BLE, BlueZ, and Floss when used as an
 * individual word. Case insensitive matching.
 */
export const BT_REGEX: RegExp = new RegExp(
    'blu[e]?[ ]?toot[h]?|\\bb[ ]?t\\b|\\bble\\b|\\bfloss\\b|\\bbluez\\b', 'i');

/**
 * Regular expression to check for wifi-related keywords.
 */
export const WIFI_REGEX: RegExp =
    buildWordMatcher(['wifi', 'wi-fi', 'internet', 'network', 'hotspot']);

/**
 * Regular expression to check for cellular-related keywords.
 */
export const CELLULAR_REGEX: RegExp = buildWordMatcher([
  '2G',   '3G',    '4G',      '5G',       'LTE',      'UMTS',
  'SIM',  'eSIM',  'mmWave',  'mobile',   'APN',      'IMEI',
  'IMSI', 'eUICC', 'carrier', 'T.Mobile', 'TMO',      'Verizon',
  'VZW',  'AT&T',  'MVNO',    'pin.lock', 'cellular',
]);

/**
 * Regular expression to check for display-related keywords.
 */
export const DISPLAY_REGEX = buildWordMatcher([
  'display',
  'displayport',
  'hdmi',
  'monitor',
  'panel',
  'screen',
]);

/**
 * Regular expression to check for USB-related keywords.
 */
export const USB_REGEX = buildWordMatcher([
  'USB',
  'USB-C',
  'Type-C',
  'TypeC',
  'USBC',
  'USBTypeC',
  'USBPD',
  'hub',
  'charger',
  'dock',
]);

/**
 * Regular expression to check for thunderbolt-related keywords.
 */
export const THUNDERBOLT_REGEX = buildWordMatcher([
  'Thunderbolt',
  'Thunderbolt3',
  'Thunderbolt4',
  'TBT',
  'TBT3',
  'TBT4',
  'TB3',
  'TB4',
]);

/**
 * Regular expression to check for all strings indicating that a user can't
 * connect to a HID or Audio device. This is also a likely indication of a
 * Bluetooth related issue.
 * Sample strings this will match:
 * "I can't connect the speaker!",
 * "The keyboard has connection problem."
 */
export const CANNOT_CONNECT_REGEX: RegExp = new RegExp(
    '((headphone|keyboard|mouse|speaker)((?!(connect|pair)).*)(connect|pair))' +
        '|((connect|pair).*(headphone|keyboard|mouse|speaker))',
    'i');

/**
 * Regular expression to check for "tether" or "tethering". Case insensitive
 * matching.
 */
export const TETHER_REGEX: RegExp = new RegExp('tether(ing)?', 'i');

/**
 * Regular expression to check for "Smart (Un)lock" or "Easy (Un)lock" with or
 * without space between the words. Case insensitive matching.
 */
export const SMART_LOCK_REGEX: RegExp =
    new RegExp('(smart|easy)[ ]?(un)?lock', 'i');

/**
 * Regular expression to check for keywords related to Nearby Share like
 * "nearby (share)" or "phone (hub)".
 * Case insensitive matching.
 */
export const NEARBY_SHARE_REGEX: RegExp = new RegExp('nearby|phone', 'i');

/**
 * Regular expression to check for keywords related to Fast Pair like
 * "fast pair".
 * Case insensitive matching.
 */
export const FAST_PAIR_REGEX: RegExp = new RegExp('fast[ ]?pair', 'i');

/**
 * Regular expression to check for Bluetooth device specific keywords.
 */
export const BT_DEVICE_REGEX =
    buildWordMatcher(['apple', 'allegro', 'pixelbud', 'microsoft', 'sony']);

/**
 * Builds a RegExp that matches one of the given words. Each word has to match
 * at word boundary and is not at the end of the tested string. For example,
 * the word "SIM" would match the string "I have a sim card issue" but not
 * "I have a simple issue" nor "I have a sim" (because the user might not have
 * finished typing yet).
 * @param words The words to match.
 */
function buildWordMatcher(words: string[]): RegExp {
  return new RegExp(
      words.map((word) => '\\b' + word + '\\b[^$]').join('|'), 'i');
}
