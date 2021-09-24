// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './diagnostics_card.js';
import './diagnostics_fonts_css.js';
import './diagnostics_shared_css.js';
import './ip_config_info_drawer.js';
import './network_info.js';
import './network_troubleshooting.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Network, NetworkHealthProviderInterface, NetworkState, NetworkStateObserverInterface, NetworkStateObserverReceiver, NetworkType, TroubleshootingInfo} from './diagnostics_types.js';
import {formatMacAddress, getNetworkState, getNetworkType, isConnectedOrOnline} from './diagnostics_utils.js';
import {getNetworkHealthProvider} from './mojo_interface_provider.js';

const BASE_SUPPORT_URL = 'https://support.google.com/chromebook?p=diagnostics_';
const SETTINGS_URL = 'chrome://os-settings/';

/**
 * @fileoverview
 * 'network-card' is a styling wrapper for a network-info element.
 */
Polymer({
  is: 'network-card',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  /**
   * @private {?NetworkHealthProviderInterface}
   */
  networkHealthProvider_: null,

  /**
   * Receiver responsible for observing a single active network connection.
   * @private {?NetworkStateObserverReceiver}
   */
  networkStateObserverReceiver_: null,

  properties: {
    /** @type {string} */
    guid: {
      type: String,
      value: '',
    },

    /** @private {string} */
    networkType_: {
      type: String,
      value: '',
    },

    /** @private {string} */
    networkState_: {
      type: String,
      value: '',
    },

    /** @type {!Network} */
    network: {
      type: Object,
    },

    /** @protected {boolean} */
    showNetworkDataPoints_: {
      type: Boolean,
      computed: 'computeShouldShowNetworkDataPoints_(network.state,' +
          ' unableToObtainIpAddress_)',
    },

    /** @protected {boolean} */
    showTroubleshootingCard_: {
      type: Boolean,
      value: false,
    },

    /** @protected {string} */
    macAddress_: {
      type: String,
      value: '',
    },

    /** @protected {boolean} */
    unableToObtainIpAddress_: {
      type: Boolean,
      value: false,
    },

    /** @protected {TroubleshootingInfo} */
    troubleshootingInfo_: {
      type: Object,
      computed: 'computeTroubleshootingInfo_(network.*,' +
          ' unableToObtainIpAddress_)',
    },

    /** @private */
    timerId_: {
      type: Number,
      value: -1,
    },
  },

  observers: ['observeNetwork_(guid)'],

  /** @override */
  created() {
    this.networkHealthProvider_ = getNetworkHealthProvider();
  },

  /** @private */
  observeNetwork_() {
    if (!this.guid) {
      return;
    }

    if (this.networkStateObserverReceiver_) {
      this.networkStateObserverReceiver_.$.close();
      this.networkStateObserverReceiver_ = null;
    }

    this.networkStateObserverReceiver_ = new NetworkStateObserverReceiver(
        /**
         * @type {!NetworkStateObserverInterface}
         */
        (this));

    this.networkHealthProvider_.observeNetwork(
        this.networkStateObserverReceiver_.$.bindNewPipeAndPassRemote(),
        this.guid);
  },

  /**
   * Implements NetworkStateObserver.onNetworkStateChanged
   * @param {!Network} network
   */
  onNetworkStateChanged(network) {
    this.networkType_ = getNetworkType(network.type);
    this.networkState_ = getNetworkState(network.state);
    this.macAddress_ = network.macAddress || '';
    this.set('network', network);
    let isIpAddressMissing = !network.ipConfig || !network.ipConfig.ipAddress;
    let isTimerInProgress = this.timerId_ !== -1;

    if (!isIpAddressMissing && this.unableToObtainIpAddress_) {
      this.unableToObtainIpAddress_ = false;
    }

    if (isIpAddressMissing && !isTimerInProgress) {
      // Seconds to wait before displaying the troubleshooting banner.
      let maxTicks = 30;
      let tickCount = 0;
      this.timerId_ = setInterval(() => {
        if (tickCount >= maxTicks) {
          clearInterval(this.timerId_);
          this.timerId_ = -1;
          this.unableToObtainIpAddress_ = true;
        }
        tickCount++;
      }, 1000);
    }
  },

  /** @protected */
  onTroubleConnectingClicked_() {
    // TODO(michaelcheco): Add correct URL.
    window.open('https://support.google.com/chromebook?p=diagnostics_');
  },

  /**
   * @protected
   * @return {string}
   */
  getNetworkCardTitle_() {
    return `${this.networkType_} (${this.networkState_})`;
  },

  /**
   * @protected
   * @return {boolean}
   */
  computeShouldShowNetworkDataPoints_() {
    // Wait until the network is present before deciding.
    if (!this.network) {
      return false;
    }

    if (this.unableToObtainIpAddress_) {
      return false;
    }

    // Show the data-points when portal, online, connected, or connecting.
    switch (this.network.state) {
      case NetworkState.kPortal:
      case NetworkState.kOnline:
      case NetworkState.kConnected:
      case NetworkState.kConnecting:
        return true;
      default:
        return false;
    }
  },

  /** @protected */
  computeShouldShowTroubleConnecting_() {
    // Wait until the network is present before deciding.
    if (!this.network) {
      return false;
    }

    // Show the troubleshooting state when not connected or connecting.
    switch (this.network.state) {
      case NetworkState.kOnline:
      case NetworkState.kConnected:
      case NetworkState.kConnecting:
        return false;
      default:
        return true;
    }
  },

  /**
   * @protected
   * @return {boolean}
   */
  isNetworkDisabled_() {
    return this.network.state === NetworkState.kDisabled;
  },

  /**
   * @protected
   * @return {string}
   */
  getMacAddress_() {
    if (!this.macAddress_) {
      return '';
    }
    return formatMacAddress(this.macAddress_);
  },

  /**
   * @private
   * @return {!TroubleshootingInfo}
   */
  getDisabledTroubleshootingInfo_() {
    return {
      header: this.i18n('disabledText', this.networkType_),
          linkText: this.i18n('reconnectLinkText'), url: SETTINGS_URL,
    }
  },

  /**
   * @private
   * @return {!TroubleshootingInfo}
   */
  getNotConnectedTroubleshootingInfo_() {
    return {
      header: this.i18n('troubleshootingText', this.networkType_),
          linkText: this.i18n('troubleConnecting'), url: BASE_SUPPORT_URL,
    }
  },

  /**
   * @private
   * @return {!TroubleshootingInfo}
   */
  computeTroubleshootingInfo_() {
    if (!this.network || isConnectedOrOnline(this.network.state)) {
      // Hide the troubleshooting banner if we're in an active state
      // unlesss the network's IP Address has been missing for >=30
      // seconds in which case we'd like to display the bannner to
      // the user.
      if (this.unableToObtainIpAddress_) {
        this.showTroubleshootingCard_ = true;
        return this.getMissingIpAddressInfo_();
      }

      this.showTroubleshootingCard_ = false;
      return {
        header: '',
        linkText: '',
        url: '',
      };
    }

    // At this point, |isConnectedOrOnline| was falsy which means
    // out network state is either disabled or not connected.
    this.showTroubleshootingCard_ = true;
    let isDisabled = this.network.state === NetworkState.kDisabled;
    return isDisabled ? this.getDisabledTroubleshootingInfo_() :
                        this.getNotConnectedTroubleshootingInfo_();
  },

  /**
   * @private
   * @return {!TroubleshootingInfo}
   */
  getMissingIpAddressInfo_() {
    return {
      header: this.i18n('noIpAddressText'),
      linkText: this.i18n('visitSettingsToConfigureLinkText'),
      url: SETTINGS_URL,
    }
  },
});
