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
 * Converts a MHz frequency into channel number. Should the frequency requested
 * not fall into the algorithm range null is returned.
 * @param {number} frequency Given in MHz.
 * @return {?number} channel
 */
export function convertFrequencyToChannel(frequency) {
  // Handle 2.4GHz channel calculation for channel 1-13.
  if (frequency >= 2412 && frequency <= 2483) {
    return Math.ceil(1 + ((frequency - 2412) / 5));
  }
  // Handle 2.4GHz channel 14 which is a special case for Japan.
  if (frequency >= 2484 && frequency <= 2495) {
    return 14;
  }
  // TODO(ashleydp): Add algorithm for 5GHz.
  return null;
}

/**
 * Converts a KiB storage value to MiB.
 * @param {number} value
 * @return {number}
 */
export function convertKibToMib(value) {
  // 1024 KiB is 1 MiB.
  return value / (2 ** 10);
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
