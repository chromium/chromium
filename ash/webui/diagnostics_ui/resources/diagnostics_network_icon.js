// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-lite.js';
import 'chrome://resources/cr_components/chromeos/network/network_icon.m.js';
import './diagnostics_shared_css.js';

import {assertNotReached} from 'chrome://resources/js/assert.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {Network, NetworkState, NetworkType} from './diagnostics_types.js';

/**
 * Type alias for network_config ConnectionStateType enum.
 * @typedef {chromeos.networkConfig.mojom.ConnectionStateType}
 */
export let ConnectionStateType =
    chromeos.networkConfig.mojom.ConnectionStateType;

/**
 * Type alias for network_config NetworkType enum.
 * @typedef {chromeos.networkConfig.mojom.NetworkType}
 */
export let CrosNetworkType = chromeos.networkConfig.mojom.NetworkType;

/**
 * Struct for minimal required network state required to display network-icon
 * element.
 * @typedef {{
 *    connectionState: !ConnectionStateType,
 *    type: !CrosNetworkType,
 *   }}
 */
export let NetworkIconNetworkState;

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
 * Adapter to convert network data to fit data required for network-icon.
 * @param {!Network} network
 * @return {!NetworkIconNetworkState}
 */
export function networkToNetworkStateAdapter(network) {
  const type = convertNetworkTypeToCrosNetworkType(network.type);
  const connectionState = convertNetworkStateToCrosNetworkState(network.state);
  return {connectionState, type};
}

/**
 * @fileoverview
 * 'diagnostics-network-icon' is a wrapper for 'network-icon' to ensure the
 * correct icon displayed based on network type, state, and technology.
 * @see //ui/webui/resources/cr_components/chromeos/network/network_icon.js
 */
export class DiagnosticsNetworkIconElement extends PolymerElement {
  static get is() {
    return 'diagnostics-network-icon';
  }

  static get template() {
    return html`{__html_template__}`
  }
}

customElements.define(
    DiagnosticsNetworkIconElement.is, DiagnosticsNetworkIconElement);
