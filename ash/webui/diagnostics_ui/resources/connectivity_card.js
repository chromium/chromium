// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './diagnostics_card.js';
import './diagnostics_fonts_css.js';
import './diagnostics_shared_css.js';
import './ip_config_info_drawer.js';
import './network_info.js';
import './routine_section.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Network, NetworkHealthProviderInterface, NetworkStateObserverInterface, NetworkStateObserverReceiver, NetworkType, RoutineType} from './diagnostics_types.js';
import {getNetworkState, getNetworkType, getRoutinesByNetworkType} from './diagnostics_utils.js';
import {getNetworkHealthProvider} from './mojo_interface_provider.js';
import {TestSuiteStatus} from './routine_list_executor.js';


/**
 * @fileoverview
 * 'connectivity-card' runs network routines and displays network health data.
 */
Polymer({
  is: 'connectivity-card',

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
    /** @type {!TestSuiteStatus} */
    testSuiteStatus: {
      type: Number,
      value: TestSuiteStatus.kNotRunning,
      notify: true,
    },

    /** @private {!Array<!RoutineType>} */
    routines_: {
      type: Array,
      value: () => [],
    },

    /** @type {string} */
    activeGuid: {
      type: String,
      value: '',
      observer: 'activeGuidChanged_',
    },

    /** @type {boolean} */
    isActive: {
      type: Boolean,
      observer: 'isActiveChanged_',
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
  },

  /** @override */
  created() {
    this.networkHealthProvider_ = getNetworkHealthProvider();
  },

  /** @private */
  getRoutineSectionElem_() {
    return /** @type {!RoutineSectionElement} */ (this.$$('routine-section'));
  },

  /** @override */
  detached() {
    this.getRoutineSectionElem_().stopTests();
  },

  displayRoutines_() {
    return this.routines_ && this.routines_.length > 0;
  },

  /** @private */
  observeNetwork_() {
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

    if (this.testSuiteStatus === TestSuiteStatus.kNotRunning) {
      this.routines_ = getRoutinesByNetworkType(network.type);
      this.getRoutineSectionElem_().runTests();
    }

    this.set('network', network);
  },

  /** @protected */
  getEstimateRuntimeInMinutes_() {
    // Connectivity routines will always last <= 1 minute.
    return 1;
  },

  /** @protected */
  getNetworkCardTitle_() {
    var title = this.networkType_;
    if (this.networkState_) {
      title = title + ' (' + this.networkState_ + ')';
    }

    return title;
  },

  /**
   * @protected
   * @param {string} activeGuid
   */
  activeGuidChanged_(activeGuid) {
    if (this.testSuiteStatus === TestSuiteStatus.kCompleted) {
      this.testSuiteStatus = TestSuiteStatus.kNotRunning;
    }

    if (!activeGuid) {
      return;
    }
    this.getRoutineSectionElem_().stopTests();
    this.observeNetwork_();
  },

  /**
   * @protected
   * @param {boolean} active
   */
  isActiveChanged_(active) {
    if (!active) {
      return;
    }

    if (this.routines_.length > 0) {
      this.getRoutineSectionElem_().runTests();
    }
  },
});
