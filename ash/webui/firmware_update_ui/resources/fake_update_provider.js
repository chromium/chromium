// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeObservables} from 'chrome://resources/ash/common/fake_observables.js';

import {FakeInstallControllerInterface, FirmwareUpdate, InstallControllerInterface, UpdateObserver, UpdateProviderInterface} from './firmware_update_types.js';
import {getUpdateController, getUpdateProvider, setUseFakeProviders} from './mojo_interface_provider.js';

// Method names.
export const ON_UPDATE_LIST_CHANGED = 'UpdateObserver_onUpdateListChanged';

/**
 * @fileoverview
 * Implements a fake version of the UpdateProvider mojo interface.
 */

/** @implements {UpdateProviderInterface} */
export class FakeUpdateProvider {
  constructor() {
    setUseFakeProviders(true);
    this.observables_ = new FakeObservables();

    /** @private {?Promise} */
    this.observePeripheralUpdatesPromise_ = null;

    /** @private {?FirmwareUpdate} */
    this.inflight_update_ = null;

    this.registerObservables();
  }

  /*
   * Implements UpdateProviderInterface.ObservePeripheralUpdates.
   * @param {!UpdateObserver} remote
   * @return {!Promise}
   */
  observePeripheralUpdates(remote) {
    this.observePeripheralUpdatesPromise_ =
        this.observe_(ON_UPDATE_LIST_CHANGED, (firmwareUpdates) => {
          remote.onUpdateListChanged(firmwareUpdates);
        });
  }

  fetchInProgressUpdate() {
    return new Promise((resolve) => resolve({update: this.inflight_update_}));
  }

  /**
   * @param {string} deviceId
   * @return {!Promise}
   */
  prepareForUpdate(deviceId) {
    /** @type {InstallControllerInterface|FakeInstallControllerInterface} */
    const controller = getUpdateController();
    controller.setDeviceIdForUpdateInProgress(deviceId);
    return new Promise((resolve) => resolve({controller}));
  }

  /**
   * Sets the values that will be observed from observePeripheralUpdates.
   * @param {!Array<!Array<!FirmwareUpdate>>} firmwareUpdates
   */
  setFakeFirmwareUpdates(firmwareUpdates) {
    this.observables_.setObservableData(
        ON_UPDATE_LIST_CHANGED, [firmwareUpdates]);
  }

  /**
   * Sets the inflight update.
   * @param {!FirmwareUpdate} update
   */
  setInflightUpdate(update) {
    this.inflight_update_ = update;
  }

  /**
   * Returns the promise for the most recent peripheral updates observation.
   * @return {?Promise}
   */
  getObservePeripheralUpdatesPromiseForTesting() {
    return this.observePeripheralUpdatesPromise_;
  }

  /**
   * Causes the device added observer to fire.
   */
  triggerDeviceAddedObserver() {
    this.observables_.trigger(ON_UPDATE_LIST_CHANGED);
  }

  registerObservables() {
    this.observables_.register(ON_UPDATE_LIST_CHANGED);
  }

  /**
   * Disables all observers and resets provider to its initial state.
   */
  reset() {
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
}
