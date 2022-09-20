// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './diagnostics_card.js';
import './diagnostics_network_icon.js';
import './diagnostics_shared.css.js';
import './ip_config_info_drawer.js';
import './network_info.js';
import './routine_section.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/cr_elements/i18n_behavior.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './connectivity_card.html.js';
import {filterNameServers, formatMacAddress, getNetworkCardTitle, getNetworkState, getNetworkType, getRoutineGroups} from './diagnostics_utils.js';
import {getNetworkHealthProvider} from './mojo_interface_provider.js';
import {Network, NetworkHealthProviderInterface, NetworkStateObserverInterface, NetworkStateObserverReceiver} from './network_health_provider.mojom-webui.js';
import {RoutineGroup} from './routine_group.js';
import {TestSuiteStatus} from './routine_list_executor.js';
import {RoutineSectionElement} from './routine_section.js';

/**
 * @fileoverview
 * 'connectivity-card' runs network routines and displays network health data.
 */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const ConnectivityCardElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class ConnectivityCardElement extends ConnectivityCardElementBase {
  static get is() {
    return 'connectivity-card';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** @type {!TestSuiteStatus} */
      testSuiteStatus: {
        type: Number,
        value: TestSuiteStatus.kNotRunning,
        notify: true,
      },

      /** @private {!Array<!RoutineGroup>} */
      routineGroups_: {
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

      /** @protected {string} */
      macAddress_: {
        type: String,
        value: '',
      },

    };
  }

  /** @override */
  constructor() {
    super();
    /**
     * @private {?NetworkHealthProviderInterface}
     */
    this.networkHealthProvider_ = null;

    /**
     * Receiver responsible for observing a single active network connection.
     * @private {?NetworkStateObserverReceiver}
     */
    this.networkStateObserverReceiver_ = null;

    this.networkHealthProvider_ = getNetworkHealthProvider();
  }

  /** @private */
  getRoutineSectionElem_() {
    return /** @type {!RoutineSectionElement} */ (
        this.shadowRoot.querySelector('routine-section'));
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();

    this.getRoutineSectionElem_().stopTests();
  }

  /**
   * @protected
   * @return {boolean}
   */
  hasRoutines_() {
    return this.routineGroups_ && this.routineGroups_.length > 0;
  }

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
  }

  /**
   * Implements NetworkStateObserver.onNetworkStateChanged
   * @param {!Network} network
   */
  onNetworkStateChanged(network) {
    this.networkType_ = getNetworkType(network.type);
    this.networkState_ = getNetworkState(network.state);
    this.macAddress_ = network.macAddress || '';

    if (this.testSuiteStatus === TestSuiteStatus.kNotRunning) {
      const isArcEnabled =
          loadTimeData.getBoolean('enableArcNetworkDiagnostics');
      this.routineGroups_ = getRoutineGroups(network.type, isArcEnabled);
      this.getRoutineSectionElem_().runTests();
    }

    // Remove '0.0.0.0' (if present) from list of name servers.
    filterNameServers(network);
    this.set('network', network);
  }

  /** @protected */
  getEstimateRuntimeInMinutes_() {
    // Connectivity routines will always last <= 1 minute.
    return 1;
  }

  /** @protected */
  getNetworkCardTitle_() {
    return getNetworkCardTitle(this.networkType_, this.networkState_);
  }

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
  }

  /**
   * @protected
   * @param {boolean} active
   */
  isActiveChanged_(active) {
    if (!active) {
      return;
    }

    if (this.routineGroups_.length > 0) {
      this.getRoutineSectionElem_().runTests();
    }
  }

  /**
   * @protected
   * @return {string}
   */
  getMacAddress_() {
    if (!this.macAddress_) {
      return '';
    }
    return formatMacAddress(this.macAddress_);
  }
}

customElements.define(ConnectivityCardElement.is, ConnectivityCardElement);
