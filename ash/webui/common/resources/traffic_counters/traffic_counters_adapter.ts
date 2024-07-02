// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Class that provides the functionality for interacting with traffic counters.
 */

import {CrosNetworkConfigInterface, FilterType, NO_LIMIT, TrafficCounter, UInt32Value} from '//resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {NetworkType} from '//resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {Time} from '//resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';

import {MojoInterfaceProviderImpl} from '../network/mojo_interface_provider.js';

/** Default traffic counter reset day. */
const kDefaultResetDay = 1;

/**
 * Information about a network.
 */
export interface Network {
  guid: string;
  name: string;
  type: NetworkType;
  counters: TrafficCounter[];
  lastResetTime: Time|null;
  friendlyDate: string|null;
  userSpecifiedResetDay: number;
}


/**
 * Helper function to create a Network object.
 */
function createNetwork(
    guid: string,
    name: string,
    type: NetworkType,
    counters: TrafficCounter[],
    lastResetTime: Time|null,
    friendlyDate: string|null,
    userSpecifiedResetDay: number): Network {
  return {
    guid: guid,
    name: name,
    type: type,
    counters: counters,
    lastResetTime: lastResetTime,
    friendlyDate: friendlyDate,
    userSpecifiedResetDay: userSpecifiedResetDay,
  };
}

export class TrafficCountersAdapter {
  private networkConfig_: CrosNetworkConfigInterface;

  constructor() {
    /**
     * Network Config mojo remote.
     */
    this.networkConfig_ =
        MojoInterfaceProviderImpl.getInstance().getMojoServiceRemote();
  }

  /**
   * Requests traffic counters for active networks.
   */
  async requestTrafficCountersForActiveNetworks(): Promise<Network[]> {
    const filter = {
      filter: FilterType.kActive,
      networkType: NetworkType.kAll,
      limit: NO_LIMIT,
    };
    const networks = [];
    const networkStateList =
        await this.networkConfig_.getNetworkStateList(filter);
    for (const networkState of networkStateList.result) {
      const trafficCounters =
          await this.requestTrafficCountersForNetwork(networkState.guid);
      const lastResetTime =
          await this.requestLastResetTimeForNetwork(networkState.guid);
      const friendlyDate =
          await this.requestFriendlyDateForNetwork(networkState.guid);
      const userSpecifiedResetDay =
          await this.requestUserSpecifiedResetDayForNetwork(networkState.guid);
      networks.push(createNetwork(
          networkState.guid, networkState.name, networkState.type,
          trafficCounters, lastResetTime, friendlyDate, userSpecifiedResetDay));
    }
    return networks;
  }

  /**
   * Resets traffic counters for the given network.
   */
  async resetTrafficCountersForNetwork(guid: string): Promise<void> {
    await this.networkConfig_.resetTrafficCounters(guid);
  }

  /**
   * Requests traffic counters for the given network.
   */
  async requestTrafficCountersForNetwork(
    guid: string): Promise<TrafficCounter[]> {
    const trafficCountersObj =
        await this.networkConfig_.requestTrafficCounters(guid);
    return trafficCountersObj.trafficCounters;
  }

  /**
   * Requests last reset time for the given network.
   */
  async requestLastResetTimeForNetwork(guid: string): Promise<Time|null> {
    const managedPropertiesPromise =
        await this.networkConfig_.getManagedProperties(guid);

    if (!managedPropertiesPromise || !managedPropertiesPromise.result) {
      return null;
    }

    const trafficCounterProperties =
        managedPropertiesPromise.result.trafficCounterProperties;
    if (!trafficCounterProperties) {
      return null;
    }

    return trafficCounterProperties.lastResetTime || null;
  }


  /**
   * Requests a reader friendly date, corresponding to the last reset time,
   * for the given network.
   */
  async requestFriendlyDateForNetwork(guid: string): Promise<string|null> {
    const managedPropertiesPromise =
        await this.networkConfig_.getManagedProperties(guid);

    if (!managedPropertiesPromise || !managedPropertiesPromise.result) {
      return null;
    }

    const trafficCounterProperties =
        managedPropertiesPromise.result.trafficCounterProperties;
    if (!trafficCounterProperties) {
      return null;
    }

    return trafficCounterProperties.friendlyDate || null;
  }

  /**
   * Requests user specified reset day for the given network.
   */
  async requestUserSpecifiedResetDayForNetwork(guid: string): Promise<number> {
    const managedPropertiesPromise =
        await this.networkConfig_.getManagedProperties(guid);

    if (!managedPropertiesPromise || !managedPropertiesPromise.result) {
      return kDefaultResetDay;
    }

    const trafficCounterProperties =
        managedPropertiesPromise.result.trafficCounterProperties;
    if (!trafficCounterProperties) {
      return kDefaultResetDay;
    }

    return trafficCounterProperties.userSpecifiedResetDay;
  }

  /**
   * Sets values for auto reset.
   */
  async setTrafficCountersResetDayForNetwork(
      guid: string, resetDay: UInt32Value|null): Promise<void> {
    await this.networkConfig_.setTrafficCountersResetDay(guid, resetDay);
  }
}
