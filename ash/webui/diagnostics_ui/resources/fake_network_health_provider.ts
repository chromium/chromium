// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeObservables} from 'chrome://resources/ash/common/fake_observables.js';
import {assert} from 'chrome://resources/js/assert.js';

import {CellularNetwork, EthernetNetwork, NetworkGuidInfo, WiFiNetwork} from './diagnostics_types.js';
import {NetworkHealthProviderInterface, NetworkListObserverRemote, NetworkStateObserverRemote} from './network_health_provider.mojom-webui.js';

// Method names.
export const ON_NETWORK_LIST_CHANGED_METHOD_NAME =
    'NetworkListObserver_onNetworkListChanged';

const ON_NETWORK_STATE_CHANGED_METHOD_NAME =
    'NetworkStateObserver_onNetworkStateChanged';

/**
 * @fileoverview
 * Implements a fake version of the NetworkHealthProvider mojo interface.
 */

/**
 * Type for methods needed for the fake NetworkHealthProvider implementation.
 */
export type FakeNetworkHealthProviderInterface =
    NetworkHealthProviderInterface&{
      setFakeNetworkGuidInfo(networkGuidInfoList: NetworkGuidInfo[]): void,
      setFakeNetworkState(
          guid: string,
          networkStateList: EthernetNetwork[]|WiFiNetwork[]|CellularNetwork[]):
          void,
    };

export class FakeNetworkHealthProvider implements
    FakeNetworkHealthProviderInterface {
  private observables: FakeObservables = new FakeObservables();
  private observeNetworkListPromise: Promise<void>|null = null;
  private observeNetworkStatePromise: Promise<void>|null = null;

  constructor() {
    this.registerObservables();
  }

  /**
   * Implements NetworkHealthProviderInterface.ObserveNetworkList.
   */
  observeNetworkList(remote: NetworkListObserverRemote): void {
    this.observeNetworkListPromise =
        this.observe(ON_NETWORK_LIST_CHANGED_METHOD_NAME, (networkGuidInfo) => {
          remote.onNetworkListChanged(
              networkGuidInfo.networkGuids, networkGuidInfo.activeGuid);
        });
  }

  /**
   * Implements NetworkHealthProviderInterface.ObserveNetwork.
   * The guid argument is used to observe a specific network identified
   * by |guid| within a group of observers.
   */
  observeNetwork(remote: NetworkStateObserverRemote, guid: string): void {
    this.observeNetworkStatePromise = this.observeWithArg(
        ON_NETWORK_STATE_CHANGED_METHOD_NAME, guid, (network) => {
          remote.onNetworkStateChanged(
              /** @type {!Network} */ (network));
        });
  }

  // Sets the values that will be observed from observeNetworkList.
  setFakeNetworkGuidInfo(networkGuidInfoList: NetworkGuidInfo[]): void {
    this.observables.setObservableData(
        ON_NETWORK_LIST_CHANGED_METHOD_NAME, networkGuidInfoList);
  }

  setFakeNetworkState(
      guid: string,
      networkStateList: EthernetNetwork[]|WiFiNetwork[]|
      CellularNetwork[]): void {
    this.observables.setObservableDataForArg(
        ON_NETWORK_STATE_CHANGED_METHOD_NAME, guid, networkStateList);
  }

  // Returns the promise for the most recent network list observation.
  getObserveNetworkListPromiseForTesting(): Promise<void> {
    assert(this.observeNetworkListPromise);
    return this.observeNetworkListPromise;
  }

  // Returns the promise for the most recent network state observation.
  getObserveNetworkStatePromiseForTesting(): Promise<void> {
    assert(this.observeNetworkStatePromise);
    return this.observeNetworkStatePromise;
  }

  // Causes the network list observer to fire.
  triggerNetworkListObserver(): void {
    this.observables.trigger(ON_NETWORK_LIST_CHANGED_METHOD_NAME);
  }

  // Make the observable fire automatically on provided interval.
  startTriggerInterval(methodName: string, intervalMs: number): void {
    this.observables.startTriggerOnInterval(methodName, intervalMs);
  }

  // Stop automatically triggering observables.
  stopTriggerIntervals(): void {
    this.observables.stopAllTriggerIntervals();
  }

  registerObservables(): void {
    this.observables.register(ON_NETWORK_LIST_CHANGED_METHOD_NAME);
    this.observables.registerObservableWithArg(
        ON_NETWORK_STATE_CHANGED_METHOD_NAME);
  }

  // Disables all observers and resets provider to its initial state.
  reset(): void {
    this.observables.stopAllTriggerIntervals();
    this.observables = new FakeObservables();
    this.registerObservables();
  }

  // Sets up an observer for methodName.
  private observe(methodName: string, callback: (T: any) => void):
      Promise<void> {
    return new Promise((resolve) => {
      this.observables.observe(methodName, callback);
      this.observables.trigger(methodName);
      resolve();
    });
  }

  // Sets up an observer for a methodName that takes an additional arg.
  private observeWithArg(
      methodName: string, arg: string,
      callback: (T: any) => void): Promise<void> {
    return new Promise((resolve) => {
      this.observables.observeWithArg(methodName, arg, callback);
      this.observables.triggerWithArg(methodName, arg);
      resolve();
    });
  }
}
