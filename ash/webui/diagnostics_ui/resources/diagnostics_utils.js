// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

import {LockType, NetworkState, NetworkType, RoutineResult, RoutineType, StandardRoutineResult} from './diagnostics_types.js';
import {RoutineGroup} from './routine_group.js';

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
 * Returns an icon from the navigation icon set.
 * @param {string} id
 * @return {string}
 */
export function getNavigationIcon(id) {
  return `navigation-selector:${id}`;
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
    case NetworkState.kDisabled:
      return loadTimeData.getString('networkStateDisabledText');
    default:
      assertNotReached();
      return '';
  }
}

/**
 * @param {!LockType} lockType
 * @return {string}
 */
export function getLockType(lockType) {
  switch (lockType) {
    case LockType.kSimPuk:
      return 'sim-puk';
    case LockType.kSimPin:
      return 'sim-pin';
    case LockType.kNone:
    default:
      assertNotReached();
      return '';
  }
}


/**
 * @param {!NetworkType} type
 * @param {boolean} isArcEnabled
 * @return {!Array<!RoutineGroup>}
 */
export function getRoutineGroups(type, isArcEnabled) {
  let localNetworkGroup = new RoutineGroup(
      [
        RoutineType.kGatewayCanBePinged,
        RoutineType.kLanConnectivity,
      ],
      'localNetworkGroupLabel');

  let nameResolutionGroup = new RoutineGroup(
      [
        RoutineType.kDnsResolverPresent,
        RoutineType.kDnsResolution,
        RoutineType.kDnsLatency,
      ],
      'nameResolutionGroupLabel');

  let wifiGroup = new RoutineGroup(
      [
        RoutineType.kSignalStrength,
        RoutineType.kCaptivePortal,
        RoutineType.kHasSecureWiFiConnection,
      ],
      'wifiGroupLabel');
  let internetConnectivityGroup = new RoutineGroup(
      [
        RoutineType.kHttpsFirewall,
        RoutineType.kHttpFirewall,
        RoutineType.kHttpsLatency,
      ],
      'internetConnectivityGroupLabel');

  if (isArcEnabled) {
    // Add ARC routines to their corresponding groups.
    nameResolutionGroup.routines.push(RoutineType.kArcDnsResolution);
    internetConnectivityGroup.routines.push(RoutineType.kArcPing);
    internetConnectivityGroup.routines.push(RoutineType.kArcHttp);
  }

  let groupsToAdd = type === NetworkType.kWiFi ?
      [wifiGroup, internetConnectivityGroup] :
      [internetConnectivityGroup];

  let networkRoutineGroups = [
    localNetworkGroup,
    nameResolutionGroup,
  ];

  return networkRoutineGroups.concat(groupsToAdd);
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

/** @return {boolean} */
export function isNavEnabled() {
  return loadTimeData.getBoolean('isNetworkingEnabled');
}


/**
 * @param {string} macAddress
 * @return {string}
 */
export function formatMacAddress(macAddress) {
  return `${loadTimeData.getString('macAddressLabel')}: ${macAddress}`;
}
