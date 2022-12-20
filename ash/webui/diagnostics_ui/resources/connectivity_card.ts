// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './diagnostics_card.js';
import './diagnostics_network_icon.js';
import './diagnostics_shared.css.js';
import './ip_config_info_drawer.js';
import './network_info.js';
import './routine_section.js';

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './connectivity_card.html.js';
import {filterNameServers, formatMacAddress, getNetworkCardTitle, getNetworkState, getNetworkType, getRoutineGroups} from './diagnostics_utils.js';
import {getNetworkHealthProvider} from './mojo_interface_provider.js';
import {Network, NetworkHealthProviderInterface, NetworkStateObserverReceiver} from './network_health_provider.mojom-webui.js';
import {RoutineGroup} from './routine_group.js';
import {TestSuiteStatus} from './routine_list_executor.js';
import {RoutineSectionElement} from './routine_section.js';

/**
 * @fileoverview
 * 'connectivity-card' runs network routines and displays network health data.
 */

const ConnectivityCardElementBase = I18nMixin(PolymerElement);

export class ConnectivityCardElement extends ConnectivityCardElementBase {
  static get is() {
    return 'connectivity-card';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      testSuiteStatus: {
        type: Number,
        value: TestSuiteStatus.NOT_RUNNING,
        notify: true,
      },

      routineGroups_: {
        type: Array,
        value: () => [],
      },

      activeGuid: {
        type: String,
        value: '',
        observer: 'activeGuidChanged_',
      },

      isActive: {
        type: Boolean,
        observer: 'isActiveChanged_',
      },

      network: {
        type: Object,
      },

      networkType_: {
        type: String,
        value: '',
      },

      networkState_: {
        type: String,
        value: '',
      },

      macAddress_: {
        type: String,
        value: '',
      },

    };
  }

  testSuiteStatus: TestSuiteStatus;
  activeGuid: string;
  isActive: boolean;
  network: Network;
  protected macAddress_: string;
  private networkType_: string;
  private networkState_: string;
  private routineGroups_: RoutineGroup[];
  private networkHealthProvider_: NetworkHealthProviderInterface =
      getNetworkHealthProvider();
  private networkStateObserverReceiver_: NetworkStateObserverReceiver|null =
      null;

  private getRoutineSectionElem_(): RoutineSectionElement {
    const routineSection = this.shadowRoot!.querySelector('routine-section');
    assert(routineSection);
    return routineSection;
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    this.getRoutineSectionElem_().stopTests();
  }

  protected hasRoutines_(): boolean {
    return this.routineGroups_ && this.routineGroups_.length > 0;
  }

  private observeNetwork_(): void {
    if (this.networkStateObserverReceiver_) {
      this.networkStateObserverReceiver_.$.close();
      this.networkStateObserverReceiver_ = null;
    }

    this.networkStateObserverReceiver_ = new NetworkStateObserverReceiver(this);

    this.networkHealthProvider_.observeNetwork(
        this.networkStateObserverReceiver_.$.bindNewPipeAndPassRemote(),
        this.activeGuid);
  }

  /**
   * Implements NetworkStateObserver.onNetworkStateChanged
   */
  onNetworkStateChanged(network: Network): void {
    this.networkType_ = getNetworkType(network.type);
    this.networkState_ = getNetworkState(network.state);
    this.macAddress_ = network.macAddress || '';

    if (this.testSuiteStatus === TestSuiteStatus.NOT_RUNNING) {
      const isArcEnabled =
          loadTimeData.getBoolean('enableArcNetworkDiagnostics');
      this.routineGroups_ = getRoutineGroups(network.type, isArcEnabled);
      this.getRoutineSectionElem_().runTests();
    }

    // Remove '0.0.0.0' (if present) from list of name servers.
    filterNameServers(network);
    this.set('network', network);
  }

  protected getEstimateRuntimeInMinutes_(): 1 {
    // Connectivity routines will always last <= 1 minute.
    return 1;
  }

  protected getNetworkCardTitle_(): string {
    return getNetworkCardTitle(this.networkType_, this.networkState_);
  }

  protected activeGuidChanged_(activeGuid: string): void {
    if (this.testSuiteStatus === TestSuiteStatus.COMPLETED) {
      this.testSuiteStatus = TestSuiteStatus.NOT_RUNNING;
    }

    if (!activeGuid) {
      return;
    }
    this.getRoutineSectionElem_().stopTests();
    this.observeNetwork_();
  }

  protected isActiveChanged_(active: boolean): void {
    if (!active) {
      return;
    }

    if (this.routineGroups_.length > 0) {
      this.getRoutineSectionElem_().runTests();
    }
  }

  protected getMacAddress_(): string {
    if (!this.macAddress_) {
      return '';
    }
    return formatMacAddress(this.macAddress_);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'connectivity-card': ConnectivityCardElement;
  }
}

customElements.define(ConnectivityCardElement.is, ConnectivityCardElement);
