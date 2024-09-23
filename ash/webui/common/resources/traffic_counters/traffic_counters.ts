// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/ash/common/network/network_shared.css.js';
import '//resources/ash/common/i18n_behavior.js';
import '//resources/ash/common/network/onc_mojo.js';

import {I18nMixin} from '//resources/ash/common/cr_elements/i18n_mixin.js';
import {TrafficCounter} from '//resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {NetworkType} from '//resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {Time} from '//resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OncMojo} from '../network/onc_mojo.js';

import {getTemplate} from './traffic_counters.html.js';
import {TrafficCountersAdapter} from './traffic_counters_adapter.js';

/**
 * @fileoverview Polymer element for a container used in displaying network
 * traffic information.
 */

/**
 * Image file name corresponding to network type.
 */
enum TechnologyIcons {
  CELLULAR = 'cellular_0.svg',
  ETHERNET = 'ethernet.svg',
  VPN = 'vpn.svg',
  WIFI = 'wifi_0.svg',
}

/**
 * Information about a network.
 */
interface Network {
  guid: string;
  name: string;
  type: NetworkType;
  counters: TrafficCounter[];
  lastResetTime: Time|null;
}

interface OnNetworkSelectedEvent {
  model: {network: Network};
}

/**
 * Helper function to create a Network object.
 */
function createNetwork(
  guid: string,
  name: string,
  type: NetworkType,
  counters: TrafficCounter[],
  lastResetTime: Time | null): Network {
  return {
    guid: guid,
    name: name,
    type: type,
    counters: counters,
    lastResetTime: lastResetTime,
  };
}

/**
 * Replacer function used to handle the bigint type.
 */
function replacer(_key: string, value: any): string|undefined|null {
  return typeof value === 'bigint' ? value.toString() : value;
}

/**
 * Converts a mojo time to JS. TODO(b/200327630)
 */
function convertMojoTimeToJS(mojoTime: Time): Date {
  // The JS Date() is based off of the number of milliseconds since the
  // UNIX epoch (1970-01-01 00::00:00 UTC), while |internalValue| of the
  // base::Time (represented in mojom.Time) represents the number of
  // microseconds since the Windows FILETIME epoch (1601-01-01 00:00:00 UTC).
  // This computes the final JS time by computing the epoch delta and the
  // conversion from microseconds to milliseconds.
  const windowsEpoch = Date.UTC(1601, 0, 1, 0, 0, 0, 0);
  const unixEpoch = Date.UTC(1970, 0, 1, 0, 0, 0, 0);
  // |epochDeltaInMs| equals to base::Time::kTimeToMicrosecondsOffset.
  const epochDeltaInMs = unixEpoch - windowsEpoch;
  const timeInMs = Number(mojoTime.internalValue) / 1000;

  return new Date(timeInMs - epochDeltaInMs);
}

export interface TrafficCountersElement {
  $: {
    container: HTMLDivElement,
  };
}

const TrafficCountersElementBase = I18nMixin(PolymerElement);

export class TrafficCountersElement extends TrafficCountersElementBase {
  static get is() {
    return 'traffic-counters' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Information about networks.
       */
      networks_: {type: Array, value: []},

      /**
       * Expanded state per network type.
       */
      typeExpanded_: {type: Array, value: []},
    };
  }

  private networks_: Network[];
  private typeExpanded_: boolean[];
  private trafficCountersAdapter_: TrafficCountersAdapter;

  constructor() {
    super();

    /**
     * Adapter to access traffic counters functionality.
     */
    this.trafficCountersAdapter_ = new TrafficCountersAdapter();
  }

  /**
   * Handles requests to request traffic counters.
   */
  private async onRequestTrafficCountersClick_(): Promise<void> {
    await this.fetchTrafficCountersForActiveNetworks_();
  }

  /**
   * Handles requests to reset traffic counters.
   */
  private async onResetTrafficCountersClick_(event: OnNetworkSelectedEvent):
    Promise<void> {
    const network = event.model.network;
    await this.trafficCountersAdapter_.resetTrafficCountersForNetwork(
        network.guid);
    const trafficCounters =
        await this.trafficCountersAdapter_.requestTrafficCountersForNetwork(
            network.guid);
    const lastResetTime =
        await this.trafficCountersAdapter_.requestLastResetTimeForNetwork(
            network.guid);
    const foundIdx = this.networks_.findIndex(n => n.guid === network.guid);
    if (foundIdx === -1) {
      return;
    }
    this.splice(
        'networks_', foundIdx, 1,
        createNetwork(
            network.guid, network.name, network.type, trafficCounters,
            lastResetTime));
  }

  /**
   * Requests traffic counters for networks.
   */
  private async fetchTrafficCountersForActiveNetworks_(): Promise<Network[]> {
    const networks = await this.trafficCountersAdapter_
                         .requestTrafficCountersForActiveNetworks();
    this.networks_ = networks;
    return this.networks_;
  }

  private getNetworkTypeString_(type: NetworkType): string {
    return this.i18n('OncType' + OncMojo.getNetworkTypeString(type));
  }

  private getNetworkTypeIcon_(type: NetworkType): string {
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
  }

  private getTypeExpanded_(type: NetworkType): boolean {
    if (this.typeExpanded_[type] === undefined) {
      this.set('typeExpanded_.' + type, false);
      return false;
    }
    return this.typeExpanded_[type];
  }

  /**
   * Helper function to toggle the expanded properties when the network
   * container is toggled.
   */
  private onToggleExpanded_(event: OnNetworkSelectedEvent) {
    const type = event.model.network.type;
    this.set('typeExpanded_.' + type, !this.typeExpanded_[type]);
  }

  private countersToString_(counters: TrafficCounter[]): string {
    // '\t' describes the number of white space characters to use as white space
    // while forming the JSON string.
    return JSON.stringify(counters, replacer, '\t');
  }

  private lastResetTimeString_(network: Network): string {
    if (network.lastResetTime === null || network.lastResetTime === undefined) {
      return '';
    }
    return convertMojoTimeToJS(network.lastResetTime).toLocaleString();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [TrafficCountersElement.is]: TrafficCountersElement;
  }
}

customElements.define(TrafficCountersElement.is, TrafficCountersElement);
