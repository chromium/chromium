// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/ash/common/network/network_shared.css.js';

import {Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertNotReached} from 'chrome://resources/ash/common/assert.js';
import {I18nBehavior} from 'chrome://resources/ash/common/i18n_behavior.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {NetworkType, PortalState} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {NetworkHealthService, NetworkHealthServiceRemote} from 'chrome://resources/mojo/chromeos/services/network_health/public/mojom/network_health.mojom-webui.js';
import {Network, NetworkHealthState, NetworkState, UInt32Value} from 'chrome://resources/mojo/chromeos/services/network_health/public/mojom/network_health_types.mojom-webui.js';

import {getTemplate} from './network_health_summary.html.js';

const TechnologyIcons = {
  CELLULAR: 'cellular_0.svg',
  ETHERNET: 'ethernet.svg',
  VPN: 'vpn.svg',
  WIFI: 'wifi_0.svg',
};

/**
 * @fileoverview Polymer element for displaying NetworkHealth properties.
 */
Polymer({
  _template: getTemplate(),
  is: 'network-health-summary',

  behaviors: [
    I18nBehavior,
  ],

  /**
   * Network Health State object.
   * @private
   * @type {NetworkHealthState}
   */
  networkHealthState_: null,

  /**
   * Network Health mojo remote.
   * @private
   * @type {?NetworkHealthServiceRemote}
   */
  networkHealth_: null,

  /**
   * Expanded state per network type.
   * @private
   * @type {!Array<boolean>}
   */
  typeExpanded_: [],

  /** @override */
  created() {
    this.networkHealth_ = NetworkHealthService.getRemote();
  },

  /** @override */
  attached() {
    this.requestNetworkHealth_();

    // Automatically refresh Network Health every second.
    window.setInterval(() => {
      this.requestNetworkHealth_();
    }, 1000);
  },

  /**
   * Requests the NetworkHealthState and updates the page.
   * @private
   */
  requestNetworkHealth_() {
    this.networkHealth_.getHealthSnapshot().then(result => {
      this.networkHealthState_ = result.state;
    });
  },

  /**
   * Returns a string for the given NetworkState.
   * @private
   * @param {NetworkState} state
   * @return {string}
   */
  getNetworkStateString_(state) {
    switch (state) {
      case NetworkState.kUninitialized:
        return this.i18n('NetworkHealthStateUninitialized');
      case NetworkState.kDisabled:
        return this.i18n('NetworkHealthStateDisabled');
      case NetworkState.kProhibited:
        return this.i18n('NetworkHealthStateProhibited');
      case NetworkState.kNotConnected:
        return this.i18n('NetworkHealthStateNotConnected');
      case NetworkState.kConnecting:
        return this.i18n('NetworkHealthStateConnecting');
      case NetworkState.kPortal:
        return this.i18n('NetworkHealthStatePortal');
      case NetworkState.kConnected:
        return this.i18n('NetworkHealthStateConnected');
      case NetworkState.kOnline:
        return this.i18n('NetworkHealthStateOnline');
    }

    assertNotReached('Unexpected enum value');
    return '';
  },

  /**
   * Returns a boolean flag to show the PortalState attribute. The information
   * is not meaningful in all cases and should be hidden to prevent confusion.
   * @private
   * @param {Network} network
   * @return {boolean}
   */
  showPortalState_(network) {
    if (network.state === NetworkState.kOnline &&
        network.portalState === PortalState.kOnline) {
      return false;
    }

    const notApplicableStates = [
      NetworkState.kUninitialized,
      NetworkState.kDisabled,
      NetworkState.kProhibited,
      NetworkState.kConnecting,
      NetworkState.kNotConnected,
    ];
    if (notApplicableStates.includes(network.state)) {
      return false;
    }

    return true;
  },

  /**
   * Returns a string for the given PortalState.
   * @private
   * @param {PortalState} state
   * @return {string}
   */
  getPortalStateString_(state) {
    return this.i18n('OncPortalState' + OncMojo.getPortalStateString(state));
  },

  /**
   * Returns a string for the given NetworkType.
   * @private
   * @param {NetworkType} type
   * @return {string}
   */
  getNetworkTypeString_(type) {
    return this.i18n('OncType' + OncMojo.getNetworkTypeString(type));
  },

  /**
   * Returns a icon for the given NetworkType.
   * @private
   * @param {NetworkType} type
   * @return {string}
   */
  getNetworkTypeIcon_(type) {
    switch (type) {
      case NetworkType.kEthernet:
        return TechnologyIcons.ETHERNET;
      case NetworkType.kWiFi:
        return TechnologyIcons.WIFI;
      case NetworkType.kVPN:
        return TechnologyIcons.VPN;
      case NetworkType.kTether:
      case NetworkType.kMobile:
      case NetworkType.kCellular:
        return TechnologyIcons.CELLULAR;
      default:
        return '';
    }
  },

  /**
   * Returns a string for the given signal strength.
   * @private
   * @param {?UInt32Value} signalStrength
   * @return {string}
   */
  getSignalStrengthString_(signalStrength) {
    return signalStrength ? signalStrength.value.toString() : '';
  },

  /**
   * Returns a boolean flag if the open to settings link should be shown.
   * @private
   * @param {Network} network
   * @return {boolean}
   */
  showSettingsLink_(network) {
    const validStates = [
      NetworkState.kConnected,
      NetworkState.kConnecting,
      NetworkState.kPortal,
      NetworkState.kOnline,
    ];
    return validStates.includes(network.state);
  },

  /**
   * Returns a URL for the network's settings page.
   * @private
   * @param {Network} network
   * @return {string}
   */
  getNetworkUrl_(network) {
    return 'chrome://os-settings/networkDetail?guid=' + network.guid;
  },

  /**
   * Returns a concatenated list of strings.
   * @private
   * @param {!Array<string>} addresses
   * @return {string}
   */
  joinAddresses_(addresses) {
    return addresses.join(', ');
  },

  /**
   * Returns a boolean flag if the routine type should be expanded.
   * @param {NetworkType} type
   * @private
   */
  getTypeExpanded_(type) {
    if (this.typeExpanded_[type] === undefined) {
      this.set('typeExpanded_.' + type, false);
      return false;
    }

    return this.typeExpanded_[type];
  },

  /**
   * Helper function to toggle the expanded properties when the network
   * container is toggled.
   * @param {!Event} event
   * @private
   */
  onToggleExpanded_(event) {
    const type = event.model.network.type;
    this.set('typeExpanded_.' + type, !this.typeExpanded_[type]);
  },
});
