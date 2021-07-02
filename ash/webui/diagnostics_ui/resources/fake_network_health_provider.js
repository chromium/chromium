// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeObservables} from 'chrome://resources/ash/common/fake_observables.js';

import {Network, NetworkGuidInfo, NetworkHealthProviderInterface} from './diagnostics_types.js';

// Method names.
export const ON_NETWORK_LIST_CHANGED_METHOD_NAME =
    'NetworkListObserver_onNetworkListChanged';

const ON_NETWORK_STATE_CHANGED_METHOD_NAME =
    'NetworkStateObserver_onNetworkStateChanged';

/**
 * @fileoverview
 * Implements a fake version of the NetworkHealthProvider mojo interface.
 */

/** @implements {NetworkHealthProviderInterface} */
export class FakeNetworkHealthProvider {
  constructor() {
    this.observables_ = new FakeObservables();

    /** @private {?Promise} */
    this.observeNetworkListPromise_ = null;

    /** @private {?Promise} */
    this.observeNetworkStatePromise_ = null;

    this.registerObservables();
  }

  /*
   * Implements NetworkHealthProviderInterface.ObserveNetworkList.
   * @param {!NetworkListObserver} remote
   * @return {!Promise}
   */
  observeNetworkList(remote) {
    this.observeNetworkListPromise_ = this.observe_(
        ON_NETWORK_LIST_CHANGED_METHOD_NAME, (networkGuidInfo) => {
          remote.onNetworkListChanged(
              networkGuidInfo.networkGuids, networkGuidInfo.activeGuid);
        });
  }

  /*
   * Implements NetworkHealthProviderInterface.ObserveNetwork.
   * The guid argument is used to observe a specific network identified
   * by |guid| within a group of observers.
   * @param {!NetworkStateObserver} remote
   * @param {string} guid
   * @return {!Promise}
   */
  observeNetwork(remote, guid) {
    this.observeNetworkStatePromise_ = this.observeWithArg_(
        ON_NETWORK_STATE_CHANGED_METHOD_NAME, guid, (network) => {
          remote.onNetworkStateChanged(
              /** @type {!Network} */ (network));
        });
  }

  /**
   * Sets the values that will be observed from observeNetworkList.
   * @param {!Array<!NetworkGuidInfo>} networkGuidInfoList
   */
  setFakeNetworkGuidInfo(networkGuidInfoList) {
    this.observables_.setObservableData(
        ON_NETWORK_LIST_CHANGED_METHOD_NAME, networkGuidInfoList);
  }

  /**
   * @param {string} guid
   * @param {!Array<!Network>} networkStateList
   */
  setFakeNetworkState(guid, networkStateList) {
    this.observables_.setObservableDataForArg(
        ON_NETWORK_STATE_CHANGED_METHOD_NAME, guid, networkStateList);
  }

  /**
   * Returns the promise for the most recent network list observation.
   * @return {?Promise}
   */
  getObserveNetworkListPromiseForTesting() {
    return this.observeNetworkListPromise_;
  }

  /**
   * Returns the promise for the most recent network state observation.
   * @return {?Promise}
   */
  getObserveNetworkStatePromiseForTesting() {
    return this.observeNetworkStatePromise_;
  }

  /**
   * Causes the network list observer to fire.
   */
  triggerNetworkListObserver() {
    this.observables_.trigger(ON_NETWORK_LIST_CHANGED_METHOD_NAME);
  }

  /**
   * Make the observable fire automatically on provided interval.
   * @param {string} methodName
   * @param {number} intervalMs
   */
  startTriggerInterval(methodName, intervalMs) {
    this.observables_.startTriggerOnInterval(methodName, intervalMs);
  }

  /**
   * Stop automatically triggering observables.
   */
  stopTriggerIntervals() {
    this.observables_.stopAllTriggerIntervals();
  }

  registerObservables() {
    this.observables_.register(ON_NETWORK_LIST_CHANGED_METHOD_NAME);
    this.observables_.registerObservableWithArg(
        ON_NETWORK_STATE_CHANGED_METHOD_NAME);
  }

  /**
   * Disables all observers and resets provider to its initial state.
   */
  reset() {
    this.observables_.stopAllTriggerIntervals();
    this.observables_ = new FakeObservables();
    this.registerObservables();
  }

  /**
   * Sets up an observer for methodName.
   * @template T
   * @param {string} methodName
   * @param {!function(!T)} callback
   * @return {!Promise}
   * @private
   */
  observe_(methodName, callback) {
    return new Promise((resolve) => {
      this.observables_.observe(methodName, callback);
      this.observables_.trigger(methodName);
      resolve();
    });
  }

  /*
   * Sets up an observer for a methodName that takes an additional arg.
   * @template T
   * @param {string} methodName
   * @param {string} arg
   * @param {!function(!T)} callback
   * @return {!Promise}
   * @private
   */
  observeWithArg_(methodName, arg, callback) {
    return new Promise((resolve) => {
      this.observables_.observeWithArg(methodName, arg, callback);
      this.observables_.triggerWithArg(methodName, arg);
      resolve();
    });
  }
}
