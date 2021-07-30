// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {NetworkState, NetworkType, RoutineType} from './diagnostics_types.js';

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
  switch (type) {
    case NetworkType.kWiFi:
      return loadTimeData.getString('wifiLabel');
    case NetworkType.kEthernet:
      return loadTimeData.getString('ethernetLabel');
    case NetworkType.kCellular:
      return loadTimeData.getString('cellularLabel');
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
      return loadTimeData.getString('networkStateOnlineText');
    case NetworkState.kConnected:
      return loadTimeData.getString('networkStateConnectedText');
    case NetworkState.kPortal:
      return loadTimeData.getString('networkStatePortalText');
    case NetworkState.kConnecting:
      return loadTimeData.getString('networkStateConnectingText');
    case NetworkState.kNotConnected:
      return loadTimeData.getString('networkStateNotConnectedText');
    default:
      assertNotReached();
      return '';
  }
}

/**
 * @param {!NetworkType} type
 * @return {!Array<!RoutineType>}
 */
export function getRoutinesByNetworkType(type) {
  // TODO(ashleydp): Update function to support routine groups.
  /** @type {!Array<!RoutineType>} */
  let networkRoutines = [
    RoutineType.kCaptivePortal,
    RoutineType.kDnsLatency,
    RoutineType.kDnsResolution,
    RoutineType.kDnsResolverPresent,
    RoutineType.kGatewayCanBePinged,
    RoutineType.kHttpFirewall,
    RoutineType.kHttpsFirewall,
    RoutineType.kHttpsLatency,
    RoutineType.kLanConnectivity,
  ];

  // Add wifi-only routines to common networking routine array.
  if (type === NetworkType.kWiFi) {
    networkRoutines.push(RoutineType.kHasSecureWiFiConnection);
    networkRoutines.push(RoutineType.kSignalStrength);
  }

  return networkRoutines;
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
