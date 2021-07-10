// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';
import {NetworkState, NetworkType} from './diagnostics_types.js';

/**
 * Converts a KiB storage value to GiB and returns a fixed-point string
 * to the desired number of decimal places.
 * @param {number} value
 * @param {number} numDecimalPlaces
 * @return {string}
 */
export function convertKibToGibDecimalString(value, numDecimalPlaces) {
  return (value / 2 ** 20).toFixed(numDecimalPlaces);
}

/**
 * Returns an icon from the diagnostics icon set.
 * @param {string} id
 * @return {string}
 */
export function getDiagnosticsIcon(id) {
  return `diagnostics:${id}`;
}

/**
 * @param {!NetworkType} type
 * @return {string}
 */
export function getNetworkType(type) {
  // TODO(michaelcheco): Add localized strings.
  switch (type) {
    case NetworkType.kWiFi:
      return 'WiFi';
    case NetworkType.kEthernet:
      return 'Ethernet';
    case NetworkType.kCellular:
      return 'Cellular';
    default:
      assertNotReached();
      return '';
  }
}

/**
 * @param {!NetworkState} state
 * @return {string}
 */
export function getNetworkState(state) {
  // TODO(michaelcheco): Add localized strings.
  switch (state) {
    case NetworkState.kOnline:
      return 'Online';
    case NetworkState.kConnected:
      return 'Connected';
    case NetworkState.kPortal:
      return 'Portal';
    case NetworkState.kConnecting:
      return 'Connecting';
    case NetworkState.kNotConnected:
      return 'Not Connected';
    default:
      assertNotReached();
      return '';
  }
}

/**
 * @param {number} prefix
 * @return {string}
 */
export function getSubnetMaskFromRoutingPrefix(prefix) {
  assert(prefix > 0 && prefix <= 32);
  let zeroes = 32 - prefix;
  // Note: 0xffffffff is 32 bits, all set to 1.
  // Use << to knock off |zeroes| number of bits and then use that same number
  // to replace those bits with zeroes.
  // Ex: 11111111 11111111 11111111 11111111 becomes
  // 11111111 11111111 11111111 00000000.
  let mask = (0xffffffff >> zeroes) << zeroes;

  let pieces = new Array(4);
  for (let i = 0; i < 4; i++) {
    // Note: & is binary and. 0xff is 8 ones "11111111".
    // Use & with the mask to select the bits from the other number.
    // Repeat to split the 32 bit number into four 8-bit numbers
    pieces[3 - i] = mask & 0xff;
    mask = mask >> 8;
  }

  return pieces.join('.');
}
