// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeObservables} from 'chrome://resources/ash/common/fake_observables.js';

import {FirmwareUpdate, InstallControllerRemote, UpdateObserverRemote} from './firmware_update.mojom-webui.js';
import {FakeInstallControllerInterface, FakeUpdateProviderInterface} from './firmware_update_types.js';
import {getUpdateController, setUseFakeProviders} from './mojo_interface_provider.js';

// Method names.
export const ON_UPDATE_LIST_CHANGED = 'UpdateObserver_onUpdateListChanged';

/**
 * @fileoverview
 * Implements a fake version of the UpdateProvider mojo interface.
 */

export class FakeUpdateProvider implements FakeUpdateProviderInterface {
  private observables = new FakeObservables();
  private observePeripheralUpdatesPromise: Promise<void>|null = null;
  private inflightUpdate: FirmwareUpdate|null = null;

  constructor() {
    setUseFakeProviders(true);
    this.registerObservables();
  }

  /*
   * Implements UpdateProviderInterface.ObservePeripheralUpdates.
   */
  observePeripheralUpdates(remote: UpdateObserverRemote): void {
    this.observePeripheralUpdatesPromise = this.observe<FirmwareUpdate[]>(
        ON_UPDATE_LIST_CHANGED, (firmwareUpdates: FirmwareUpdate[]) => {
          remote.onUpdateListChanged(firmwareUpdates);
        });
  }

  /*
   * Implements UpdateProviderInterface.FetchInProgressUpdate.
   */
  fetchInProgressUpdate(): Promise<{update: FirmwareUpdate | null}> {
    return new Promise((resolve) => resolve({update: this.inflightUpdate}));
  }

  /*
   * Implements UpdateProviderInterface.PrepareForUpdate.
   */
  prepareForUpdate(deviceId: string):
      Promise<{controller: InstallControllerRemote | null}> {
    const controller = getUpdateController();
    (controller as FakeInstallControllerInterface)
        .setDeviceIdForUpdateInProgress(deviceId);
    return new Promise(
        (resolve) =>
            resolve({controller: (controller as InstallControllerRemote)}));
  }

  /**
   * Sets the values that will be observed from observePeripheralUpdates.
   */
  setFakeFirmwareUpdates(firmwareUpdates: FirmwareUpdate[][]): void {
    this.observables.setObservableData(
        ON_UPDATE_LIST_CHANGED, [firmwareUpdates]);
  }

  /**
   * Sets the inflight update.
   */
  setInflightUpdate(update: FirmwareUpdate): void {
    this.inflightUpdate = update;
  }

  /**
   * Returns the promise for the most recent peripheral updates observation.
   */
  getObservePeripheralUpdatesPromiseForTesting(): Promise<void>|null {
    return this.observePeripheralUpdatesPromise;
  }

  /**
   * Causes the device added observer to fire.
   */
  triggerDeviceAddedObserver(): void {
    this.observables.trigger(ON_UPDATE_LIST_CHANGED);
  }

  registerObservables(): void {
    this.observables.register(ON_UPDATE_LIST_CHANGED);
  }

  /**
   * Disables all observers and resets provider to its initial state.
   */
  reset(): void {
    this.observables = new FakeObservables();
    this.registerObservables();
  }

  /**
   * Sets up an observer for methodName.
   */
  private observe<T>(methodName: string, callback: (arg: T) => void):
      Promise<void> {
    return new Promise((resolve) => {
      this.observables.observe(methodName, callback);
      this.observables.trigger(methodName);
      resolve();
    });
  }
}
