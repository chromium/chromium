// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './diagnostics_card.js';
import './diagnostics_network_icon.js';
import './diagnostics_shared.css.js';
import './ip_config_info_drawer.js';
import './network_info.js';
import './routine_section.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
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
  static get is(): 'connectivity-card' {
    return 'connectivity-card' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      testSuiteStatus: {
        type: Number,
        value: TestSuiteStatus.NOT_RUNNING,
        notify: true,
      },

      routineGroups: {
        type: Array,
        value: () => [],
      },

      activeGuid: {
        type: String,
        value: '',
        observer: ConnectivityCardElement.prototype.activeGuidChanged,
      },

      isActive: {
        type: Boolean,
        observer: ConnectivityCardElement.prototype.isActiveChanged,
      },

      network: {
        type: Object,
      },

      networkType: {
        type: String,
        value: '',
      },

      networkState: {
        type: String,
        value: '',
      },

      macAddress: {
        type: String,
        value: '',
      },

    };
  }

  testSuiteStatus: TestSuiteStatus;
  activeGuid: string;
  isActive: boolean;
  network: Network;
  protected macAddress: string;
  private networkType: string;
  private networkState: string;
  private routineGroups: RoutineGroup[];
  private networkHealthProvider: NetworkHealthProviderInterface =
      getNetworkHealthProvider();
  private networkStateObserverReceiver: NetworkStateObserverReceiver|null =
      null;

  private getRoutineSectionElem(): RoutineSectionElement {
    const routineSection = this.shadowRoot!.querySelector('routine-section');
    assert(routineSection);
    return routineSection;
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();

    this.getRoutineSectionElem().stopTests();
  }

  getRoutineGroupsForTesting(): RoutineGroup[] {
    return this.routineGroups;
  }

  protected hasRoutines(): boolean {
    return this.routineGroups && this.routineGroups.length > 0;
  }

  private observeNetwork(): void {
    if (this.networkStateObserverReceiver) {
      this.networkStateObserverReceiver.$.close();
      this.networkStateObserverReceiver = null;
    }

    this.networkStateObserverReceiver = new NetworkStateObserverReceiver(this);

    this.networkHealthProvider.observeNetwork(
        this.networkStateObserverReceiver.$.bindNewPipeAndPassRemote(),
        this.activeGuid);
  }

  /**
   * Implements NetworkStateObserver.onNetworkStateChanged
   */
  onNetworkStateChanged(network: Network): void {
    this.networkType = getNetworkType(network.type);
    this.networkState = getNetworkState(network.state);
    this.macAddress = network.macAddress || '';

    if (this.testSuiteStatus === TestSuiteStatus.NOT_RUNNING) {
      this.routineGroups = getRoutineGroups(network.type);
      this.getRoutineSectionElem().runTests();
    }

    // Remove '0.0.0.0' (if present) from list of name servers.
    filterNameServers(network);
    this.set('network', network);
  }

  protected getEstimateRuntimeInMinutes(): 1 {
    // Connectivity routines will always last <= 1 minute.
    return 1;
  }

  protected getNetworkCardTitle(): string {
    return getNetworkCardTitle(this.networkType, this.networkState);
  }

  protected activeGuidChanged(activeGuid: string): void {
    if (this.testSuiteStatus === TestSuiteStatus.COMPLETED) {
      this.testSuiteStatus = TestSuiteStatus.NOT_RUNNING;
    }

    if (!activeGuid) {
      return;
    }
    this.getRoutineSectionElem().stopTests();
    this.observeNetwork();
  }

  protected isActiveChanged(active: boolean): void {
    if (!active) {
      return;
    }

    if (this.routineGroups.length > 0) {
      this.getRoutineSectionElem().runTests();
    }
  }

  protected getMacAddress(): string {
    if (!this.macAddress) {
      return '';
    }
    return formatMacAddress(this.macAddress);
  }

  getRoutineSectionElemForTesting(): RoutineSectionElement {
    return this.getRoutineSectionElem();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [ConnectivityCardElement.is]: ConnectivityCardElement;
  }
}

customElements.define(ConnectivityCardElement.is, ConnectivityCardElement);
