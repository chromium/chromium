// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './diagnostics_card.js';
import './diagnostics_fonts_css.js';
import './diagnostics_shared_css.js';
import './network_info.js';
import './routine_section.js';
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.m.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Network, NetworkHealthProviderInterface, NetworkStateObserverInterface, NetworkStateObserverReceiver, NetworkType, RoutineType} from './diagnostics_types.js';
import {getNetworkState, getNetworkType} from './diagnostics_utils.js';
import {getNetworkHealthProvider} from './mojo_interface_provider.js';


/**
 * @fileoverview
 * 'connectivity-card' runs network routines and displays network health data.
 */
Polymer({
  is: 'connectivity-card',

  _template: html`{__html_template__}`,

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
    /** @type {boolean} */
    isTestRunning: {
      type: Boolean,
      value: false,
      notify: true,
    },

    /** @private {!Array<!RoutineType>} */
    routines_: {
      type: Array,
      value: [
        RoutineType.kCaptivePortal,
        RoutineType.kDnsLatency,
        RoutineType.kDnsResolution,
        RoutineType.kDnsResolverPresent,
        RoutineType.kGatewayCanBePinged,
        RoutineType.kHasSecureWiFiConnection,
        RoutineType.kHttpFirewall,
        RoutineType.kHttpsFirewall,
        RoutineType.kHttpsLatency,
        RoutineType.kLanConnectivity,
        RoutineType.kSignalStrength,
      ],
    },

    /** @type {string} */
    activeGuid: {
      type: String,
      value: '',
    },

    /** @type {boolean} */
    isActive: {
      type: Boolean,
    },

    /** @type {!Network} */
    network: {
      type: Object,
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

    /** @private {boolean} */
    expanded_: {
      type: Boolean,
      value: false,
    },
  },

  observers: ['observeNetwork_(activeGuid)'],

  /** @override */
  created() {
    this.networkHealthProvider_ = getNetworkHealthProvider();
  },

  /** @private */
  observeNetwork_() {
    if (!this.activeGuid) {
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
        this.activeGuid);
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
  getEstimateRuntimeInMinutes_() {
    // Connectivity routines will always last <= 1 minute.
    return 1;
  },

  /** @protected */
  getNetworkCardTitle_() {
    // TODO(michaelcheco): Map network state to an icon or localize.
    var title = this.networkType_;
    if (this.networkState_) {
      title = title + ' (' + this.networkState_ + ')';
    }

    return title;
  },
});
