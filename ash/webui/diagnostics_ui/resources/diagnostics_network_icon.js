// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'diagnostics-network-icon' is a wrapper for 'network-icon' to ensure the
 * correct icon displayed based on network type, state, and technology.
 * @see //ui/webui/resources/cr_components/chromeos/network/network_icon.js
 */

import 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-lite.js';
import 'chrome://resources/cr_components/chromeos/network/network_icon.m.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import './diagnostics_shared_css.js';

import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Network, NetworkState, NetworkType, SecurityType} from './diagnostics_types.js';
import {getNetworkType} from './diagnostics_utils.js';

/**
 * Type alias for network_config NetworkStateProperties struct.
 * @typedef {chromeos.networkConfig.mojom.NetworkStateProperties}
 */
export const CrosNetworkStateProperties =
    chromeos.networkConfig.mojom.NetworkStateProperties;

/**
 * Type alias for network_config ConnectionStateType enum.
 * @typedef {chromeos.networkConfig.mojom.ConnectionStateType}
 */
export const ConnectionStateType =
    chromeos.networkConfig.mojom.ConnectionStateType;

/**
 * Type alias for network_config NetworkType enum.
 * @typedef {chromeos.networkConfig.mojom.NetworkType}
 */
export const CrosNetworkType = chromeos.networkConfig.mojom.NetworkType;

/**
 * Type alias for network_config SecurityType enum.
 * @typedef {chromeos.networkConfig.mojom.SecurityType}
 */
export const CrosSecurityType = chromeos.networkConfig.mojom.SecurityType;

/**
 * Struct for minimal required network state required to display network-icon
 * element.
 * @typedef {{
 *    connectionState: !ConnectionStateType,
 *    guid: string,
 *    type: !CrosNetworkType,
 *    typeState: ?CrosNetworkStateProperties,
 *   }}
 */
export let NetworkIconNetworkState;

/**
 * Required Cellular type properties to display network-icon.
 * @typedef {{
 *    networkTechnology: string,
 *    roaming: boolean,
 *    signalStrength: number,
 *    simLocked: boolean
 *  }}
 */
export let RequiredCellularProperties;

/**
 * Required WiFi type properties to display network-icon.
 * @typedef {{
 *    security: !CrosSecurityType,
 *    signalStrength: number,
 *  }}
 */
export let RequiredWiFiProperties;

/**
 * @param {!NetworkState} state
 * @return {!ConnectionStateType}
 */
function convertNetworkStateToCrosNetworkState(state) {
  switch (state) {
    case NetworkState.kOnline:
      return ConnectionStateType.kOnline;
    case NetworkState.kConnected:
      return ConnectionStateType.kConnected;
    case NetworkState.kPortal:
      return ConnectionStateType.kPortal;
    case NetworkState.kConnecting:
      return ConnectionStateType.kConnecting;
    case NetworkState.kNotConnected:
    // kDisabled is a device state which is merged with kNotConnected.
    case NetworkState.kDisabled:
      return ConnectionStateType.kNotConnected;
    default:
      assertNotReached();
  }
}

/**
 * @param {!NetworkType} type
 * @return {!CrosNetworkType}
 */
function convertNetworkTypeToCrosNetworkType(type) {
  switch (type) {
    case NetworkType.kEthernet:
      return CrosNetworkType.kEthernet;
    case NetworkType.kWiFi:
      return CrosNetworkType.kWiFi;
    case NetworkType.kCellular:
      return CrosNetworkType.kCellular;
    default:
      assertNotReached();
  }
}

/**
 * Helper function to get the required properties to display a cellular network
 * icon.
 * @param {!Network} network
 * @return {!CrosNetworkStateProperties}
 */
function getCellularTypeState(network) {
  assert(network.type === NetworkType.kCellular);
  /**
   * Default type properties for cellular.
   * @type {!RequiredCellularProperties}
   */
  const defaultCellularTypeStateProperties = {
    networkTechnology: '',
    roaming: false,
    signalStrength: 0,
    simLocked: false,
  };
  let typeState = /** @type {!CrosNetworkStateProperties} */ (
      {cellular: defaultCellularTypeStateProperties});

  if (!network.typeProperties) {
    return typeState;
  }

  // Override type properties if data is available.
  const networkTechnology = network.typeProperties.cellular.networkTechnology;
  const roaming = network.typeProperties.cellular.roaming;
  const signalStrength = network.typeProperties.cellular.signalStrength;
  const simLocked = network.typeProperties.cellular.simLocked;
  typeState = /** @type {!CrosNetworkStateProperties} */ (
      {cellular: {networkTechnology, roaming, signalStrength, simLocked}});

  return typeState;
}

/**
 * Helper function to get the required properties to display a wifi network
 * icon.
 * @param {!Network} network
 * @returns
 */
function getWifiTypeState(network) {
  /** @type {!RequiredWiFiProperties} */
  const defaultWifiTypeStateProperties = {
    security: CrosSecurityType.kNone,
    signalStrength: 0,
  };
  let typeState = /** @type {!CrosNetworkStateProperties} */ (
    {wifi: defaultWifiTypeStateProperties});

  if (!network.typeProperties) {
    return typeState;
  }

  // Override type properties if data is available.
  const signalStrength = network.typeProperties.wifi.signalStrength;
  const security = convertSecurityTypeToCrosSecurityType(
      network.typeProperties.wifi.security);
  typeState = /** @type {CrosNetworkStateProperties} */ (
      {wifi: {security, signalStrength}});

  return typeState;
}

/**
 * Helper function to get the typeState required for a given `network.type`
 * to display `network-icon` correctly.
 * @param {!Network} network
 * @return {?CrosNetworkStateProperties}
 */
function getTypeState(network) {
  switch(network.type) {
    case NetworkType.kEthernet:
      return null;
    case NetworkType.kCellular:
      return getCellularTypeState(network);
    case NetworkType.kWiFi:
      return getWifiTypeState(network);
    default:
      assertNotReached();
  }
  return null;
}

/*
 * @param {!SecurityType} type
 * @return {!CrosSecurityType}
 */
function convertSecurityTypeToCrosSecurityType(type) {
  switch (type) {
    case SecurityType.kNone:
      return CrosSecurityType.kNone;
    case SecurityType.kWep8021x:
      return CrosSecurityType.kWep8021x;
    case SecurityType.kWepPsk:
      return CrosSecurityType.kWepPsk;
    case SecurityType.kWpaEap:
      return CrosSecurityType.kWpaEap;
    case SecurityType.kWpaPsk:
      return CrosSecurityType.kWpaPsk;
    default:
      assertNotReached();
  }
}

/**
 * Adapter to convert network data to fit data required for network-icon.
 * @param {!Network} network
 * @return {!NetworkIconNetworkState}
 */
export function networkToNetworkStateAdapter(network) {
  const type = convertNetworkTypeToCrosNetworkType(network.type);
  const connectionState = convertNetworkStateToCrosNetworkState(network.state);
  const guid = network.observerGuid;
  const typeState = getTypeState(network);

  return {guid, connectionState, type, typeState};
}

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const DiagnosticsNetworkIconBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class DiagnosticsNetworkIconElement extends DiagnosticsNetworkIconBase {
  static get is() {
    return 'diagnostics-network-icon';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {!Network} */
      network: {
        type: Object,
      },
    };
  }

  /**
   * @protected
   * @return {?NetworkIconNetworkState}
   */
  computeNetworkState_() {
    // Block should only be entered when element is being initialized.
    if (!this.network) {
      return null;
    }

    return networkToNetworkStateAdapter(this.network);
  }

  /**
   * @protected
   * @return {boolean}
   */
  computeShouldDisplaySpinner_() {
    if (!this.network) {
      return false;
    }

    return this.network.state === NetworkState.kConnecting;
  }

  /**
   * @protected
   * @return {string}
   */
  computeSpinnerAriaLabel_() {
    if (!this.network) {
      return '';
    }
    const networkType = getNetworkType(this.network.type);

    return this.i18nDynamic(
        this.locale, 'networkIconLabelConnecting', networkType);
  }
}

customElements.define(
    DiagnosticsNetworkIconElement.is, DiagnosticsNetworkIconElement);
