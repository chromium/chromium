// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

import {LockType, NavigationView, Network, NetworkState, NetworkType, RoutineProperties, RoutineResult, RoutineType, StandardRoutineResult} from './diagnostics_types.js';
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
 * Converts ID into matching navigation view. ID matches the 'id' field provided
 * to the navigation-view-panel {SelectorItem} array.
 * @param {string} id
 * @return {!NavigationView}
 */
export function getNavigationViewForPageId(id) {
  switch (id) {
    case 'system':
      return NavigationView.kSystem;
    case 'connectivity':
      return NavigationView.kConnectivity;
    case 'input':
      return NavigationView.kInput;
    default:
      assertNotReached();
      return NavigationView.kSystem;
  }
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
 * @param {!RoutineType} routine
 * @param {boolean} blocking If a routine is blocking, the remaining routines
 * will be skipped. For non-blocking routines, we'll continue running them
 * and display a 'WARNING' badge to signal that a non-blocking routine failed.
 * @return {!RoutineProperties}
 */
export function createRoutine(routine, blocking) {
  return {routine, blocking};
}

/**
 * @param {!NetworkType} type
 * @param {boolean} isArcEnabled
 * @return {!Array<!RoutineGroup>}
 */
export function getRoutineGroups(type, isArcEnabled) {
  const localNetworkGroup = new RoutineGroup(
      [
        createRoutine(RoutineType.kGatewayCanBePinged, false),
        createRoutine(RoutineType.kLanConnectivity, true),
      ],
      'localNetworkGroupLabel');

  const nameResolutionGroup = new RoutineGroup(
      [
        createRoutine(RoutineType.kDnsResolverPresent, true),
        createRoutine(RoutineType.kDnsResolution, true),
        createRoutine(RoutineType.kDnsLatency, true),
      ],
      'nameResolutionGroupLabel');

  const wifiGroup = new RoutineGroup(
      [
        createRoutine(RoutineType.kSignalStrength, false),
        createRoutine(RoutineType.kCaptivePortal, false),
        createRoutine(RoutineType.kHasSecureWiFiConnection, false),
      ],
      'wifiGroupLabel');
  const internetConnectivityGroup = new RoutineGroup(
      [
        createRoutine(RoutineType.kHttpsFirewall, true),
        createRoutine(RoutineType.kHttpFirewall, true),
        createRoutine(RoutineType.kHttpsLatency, true),
      ],
      'internetConnectivityGroupLabel');

  if (isArcEnabled) {
    // Add ARC routines to their corresponding groups.
    nameResolutionGroup.addRoutine(
        (createRoutine(RoutineType.kArcDnsResolution, false)));
    localNetworkGroup.addRoutine((createRoutine(RoutineType.kArcPing, false)));
    internetConnectivityGroup.addRoutine(
        (createRoutine(RoutineType.kArcHttp, false)));
  }

  const groupsToAdd = type === NetworkType.kWiFi ?
      [wifiGroup, internetConnectivityGroup] :
      [internetConnectivityGroup];

  const networkRoutineGroups = [
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
  assert(prefix >= 0 && prefix <= 32);

  // A routing prefix can not be 0. Zero indicates an unset value.
  if (prefix === 0) {
    return '';
  }

  const zeroes = 32 - prefix;
  // Note: 0xffffffff is 32 bits, all set to 1.
  // Use << to knock off |zeroes| number of bits and then use that same number
  // to replace those bits with zeroes.
  // Ex: 11111111 11111111 11111111 11111111 becomes
  // 11111111 11111111 11111111 00000000.
  let mask = (0xffffffff >> zeroes) << zeroes;

  const pieces = new Array(4);
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

/**
 * Resolves a networking routine type to its corresponding localized failure
 * message.
 * @param {!RoutineType} routineType
 * @return {string}
 */
export function getRoutineFailureMessage(routineType) {
  switch (routineType) {
    case RoutineType.kCaptivePortal:
      return loadTimeData.getString('captivePortalFailedText');
    case RoutineType.kDnsLatency:
      return loadTimeData.getString('dnsLatencyFailedText');
    case RoutineType.kDnsResolution:
      return loadTimeData.getString('dnsResolutionFailedText');
    case RoutineType.kDnsResolverPresent:
      return loadTimeData.getString('dnsResolverPresentFailedText');
    case RoutineType.kGatewayCanBePinged:
      return loadTimeData.getString('gatewayCanBePingedFailedText');
    case RoutineType.kHasSecureWiFiConnection:
      return loadTimeData.getString('hasSecureWiFiConnectionFailedText');
    case RoutineType.kHttpFirewall:
      return loadTimeData.getString('httpFirewallFailedText');
    case RoutineType.kHttpsFirewall:
      return loadTimeData.getString('httpsFirewallFailedText');
    case RoutineType.kHttpsLatency:
      return loadTimeData.getString('httpsLatencyFailedText');
    case RoutineType.kLanConnectivity:
      return loadTimeData.getString('lanConnectivityFailedText');
    case RoutineType.kSignalStrength:
      return loadTimeData.getString('signalStrengthFailedText');
    case RoutineType.kArcHttp:
      return loadTimeData.getString('arcHttpFailedText');
    case RoutineType.kArcPing:
      return loadTimeData.getString('arcPingFailedText');
    case RoutineType.kArcDnsResolution:
      return loadTimeData.getString('arcDnsResolutionFailedText');
    case RoutineType.kBatteryCharge:
    case RoutineType.kBatteryDischarge:
    case RoutineType.kCpuCache:
    case RoutineType.kCpuStress:
    case RoutineType.kCpuFloatingPoint:
    case RoutineType.kCpuPrime:
    case RoutineType.kMemory:
      assertNotReached();
      return '';
    default:
      // Values should always be found in the enum.
      assertNotReached();
      return '';
  }
}

/**
 * @param {!NetworkState} state
 * @return {boolean}
 */
export function isConnectedOrOnline(state) {
  switch (state) {
    case NetworkState.kOnline:
    case NetworkState.kConnected:
    case NetworkState.kConnecting:
      return true;
    default:
      return false;
  }
}

/**
 * @param {!Network} network
 * @return {boolean}
 */
export function isNetworkMissingNameServers(network) {
  return !network.ipConfig || !network.ipConfig.nameServers ||
      network.ipConfig.nameServers.length === 0;
}

/**
 * Removes '0.0.0.0' from list of name servers.
 * @param {!Network} network
 */
export function filterNameServers(network) {
  if (network && network.ipConfig && network.ipConfig.nameServers) {
    network.ipConfig.nameServers =
        network.ipConfig.nameServers.filter(n => n !== '0.0.0.0');
  }
}

/*
 * If true network state text is appended to network and connectivity card
 * title.
 * @type {boolean}
 */
let displayStateInTitle = false;

/**
 * Test helper function to allow change if state text is appended to the card
 * title.
 * @param {boolean} state
 */
export function setDisplayStateInTitleForTesting(state) {
  displayStateInTitle = state;
}

/**
 * Build common string for network title for network and connectivity card.
 * Current network state is included for debugging when `displayStateInTitle`
 * is true.
 * @param {string} networkType
 * @param {string} networkState
 * @returns
 */
export function getNetworkCardTitle(networkType, networkState) {
  let titleForCard = `${networkType}`;
  if (displayStateInTitle) {
    titleForCard = `${titleForCard} (${networkState})`;
  }

  return `${titleForCard}`;
}

/**
 * @param {number} value
 * @return {string}
 */
export function getSignalStrength(value) {
  assert(typeof value === 'number');
  assert(value >= 0 && value <= 100);

  if (value <= 1) {
    return '';
  }

  if (value <= 25) {
    return loadTimeData.getStringF('signalStrength_Weak', value);
  }

  if (value <= 50) {
    return loadTimeData.getStringF('signalStrength_Average', value);
  }

  if (value <= 75) {
    return loadTimeData.getStringF('signalStrength_Good', value);
  }

  return loadTimeData.getStringF('signalStrength_Excellent', value);
}
