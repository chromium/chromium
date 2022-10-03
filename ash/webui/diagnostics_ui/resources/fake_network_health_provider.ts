// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeObservables} from 'chrome://resources/ash/common/fake_observables.js';
import {assert} from 'chrome://resources/js/assert_ts.js';

import {NetworkGuidInfo} from './diagnostics_types.js';
import {Network, NetworkHealthProviderInterface, NetworkListObserverRemote, NetworkStateObserverRemote} from './network_health_provider.mojom-webui.js';

// Method names.
export const ON_NETWORK_LIST_CHANGED_METHOD_NAME =
    'NetworkListObserver_onNetworkListChanged';

const ON_NETWORK_STATE_CHANGED_METHOD_NAME =
    'NetworkStateObserver_onNetworkStateChanged';

/**
 * @fileoverview
 * Implements a fake version of the NetworkHealthProvider mojo interface.
 */

export class FakeNetworkHealthProvider implements
    NetworkHealthProviderInterface {
  private observables_: FakeObservables = new FakeObservables();
  private observeNetworkListPromise_: Promise<void>|null = null;
  private observeNetworkStatePromise_: Promise<void>|null = null;

  constructor() {
    this.registerObservables();
  }

  /**
   * Implements NetworkHealthProviderInterface.ObserveNetworkList.
   */
  observeNetworkList(remote: NetworkListObserverRemote): void {
    this.observeNetworkListPromise_ = this.observe_(
        ON_NETWORK_LIST_CHANGED_METHOD_NAME, (networkGuidInfo) => {
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
    this.observeNetworkStatePromise_ = this.observeWithArg_(
        ON_NETWORK_STATE_CHANGED_METHOD_NAME, guid, (network) => {
          remote.onNetworkStateChanged(
              /** @type {!Network} */ (network));
        });
  }

  // Sets the values that will be observed from observeNetworkList.
  setFakeNetworkGuidInfo(networkGuidInfoList: NetworkGuidInfo[]): void {
    this.observables_.setObservableData(
        ON_NETWORK_LIST_CHANGED_METHOD_NAME, networkGuidInfoList);
  }

  setFakeNetworkState(guid: string, networkStateList: Network[]): void {
    this.observables_.setObservableDataForArg(
        ON_NETWORK_STATE_CHANGED_METHOD_NAME, guid, networkStateList);
  }

  // Returns the promise for the most recent network list observation.
  getObserveNetworkListPromiseForTesting(): Promise<void> {
    assert(this.observeNetworkListPromise_);
    return this.observeNetworkListPromise_;
  }

  // Returns the promise for the most recent network state observation.
  getObserveNetworkStatePromiseForTesting(): Promise<void> {
    assert(this.observeNetworkStatePromise_);
    return this.observeNetworkStatePromise_;
  }

  // Causes the network list observer to fire.
  triggerNetworkListObserver(): void {
    this.observables_.trigger(ON_NETWORK_LIST_CHANGED_METHOD_NAME);
  }

  // Make the observable fire automatically on provided interval.
  startTriggerInterval(methodName: string, intervalMs: number): void {
    this.observables_.startTriggerOnInterval(methodName, intervalMs);
  }

  // Stop automatically triggering observables.
  stopTriggerIntervals(): void {
    this.observables_.stopAllTriggerIntervals();
  }

  registerObservables(): void {
    this.observables_.register(ON_NETWORK_LIST_CHANGED_METHOD_NAME);
    this.observables_.registerObservableWithArg(
        ON_NETWORK_STATE_CHANGED_METHOD_NAME);
  }

  // Disables all observers and resets provider to its initial state.
  reset(): void {
    this.observables_.stopAllTriggerIntervals();
    this.observables_ = new FakeObservables();
    this.registerObservables();
  }

  // Sets up an observer for methodName.
  private observe_(methodName: string, callback: (T: any) => void):
      Promise<void> {
    return new Promise((resolve) => {
      this.observables_.observe(methodName, callback);
      this.observables_.trigger(methodName);
      resolve();
    });
  }

  // Sets up an observer for a methodName that takes an additional arg.
  private observeWithArg_(
      methodName: string, arg: string,
      callback: (T: any) => void): Promise<void> {
    return new Promise((resolve) => {
      this.observables_.observeWithArg(methodName, arg, callback);
      this.observables_.triggerWithArg(methodName, arg);
      resolve();
    });
  }
}
