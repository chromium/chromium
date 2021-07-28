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

import {Network, NetworkHealthProviderInterface, NetworkState, NetworkStateObserverInterface, NetworkStateObserverReceiver, NetworkType} from './diagnostics_types.js';
import {getNetworkState, getNetworkType} from './diagnostics_utils.js';
import {getNetworkHealthProvider} from './mojo_interface_provider.js';

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
    showTroubleConnectingState_: {
      type: Boolean,
      computed: 'computeShouldShowTroubleConnecting_(network.state)',
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
    this.set('network', network);
  },

  /** @protected */
  onTroubleConnectingClicked_() {
    // TODO(michaelcheco): Add correct URL.
    window.open('https://support.google.com/chromebook?p=diagnostics_');
  },

  /** @protected */
  getNetworkCardTitle_() {
    var title = this.networkType_;
    if (this.networkState_) {
      title = title + ' (' + this.networkState_ + ')';
    }

    return title;
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
});
