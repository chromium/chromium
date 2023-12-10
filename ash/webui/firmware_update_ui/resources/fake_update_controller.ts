// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeObservables} from 'chrome://resources/ash/common/fake_observables.js';
import {PromiseResolver} from 'chrome://resources/ash/common/promise_resolver.js';
import {assert} from 'chrome://resources/js/assert.js';
import {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';

import {fakeFirmwareUpdates, fakeInstallationProgress, fakeInstallationProgressFailure} from './fake_data.js';
import {FirmwareUpdate, InstallationProgress, UpdateProgressObserverRemote, UpdateProviderInterface, UpdateState} from './firmware_update.mojom-webui.js';
import {FakeInstallControllerInterface, FakeUpdateProviderInterface} from './firmware_update_types.js';
import {getUpdateProvider, setUseFakeProviders} from './mojo_interface_provider.js';

// Method names.
export const ON_PROGRESS_CHANGED = 'UpdateProgressObserver_onStatusChanged';

/**
 * @fileoverview
 * Implements a fake version of the UpdateController mojo interface.
 */

export class FakeUpdateController implements FakeInstallControllerInterface {
  private observables = new FakeObservables();
  private completedFirmwareUpdates = new Set<string>();
  private startUpdatePromise: Promise<void>|null = null;
  private deviceId = '';
  private isUpdateInProgress = false;
  private updateIntervalInMs = 1000;
  private updateCompletedPromise: PromiseResolver<void>|null = null;

  constructor() {
    setUseFakeProviders(true);
    this.registerObservables();
  }

  /*
   * Implements InstallControllerInterface.addUpdateProgressObserver.
   */
  addUpdateProgressObserver(remote: UpdateProgressObserverRemote): void {
    this.isUpdateInProgress = true;
    this.updateCompletedPromise = new PromiseResolver();
    this.startUpdatePromise = this.observeWithArg(
        ON_PROGRESS_CHANGED, this.deviceId,
        (update: InstallationProgress): void => {
          remote.onStatusChanged(update);
          if (update.state === UpdateState.kSuccess ||
              update.state === UpdateState.kFailed) {
            this.isUpdateInProgress = false;
            this.completedFirmwareUpdates.add(this.deviceId);
            this.updateDeviceList();
            this.observables.stopTriggerOnIntervalWithArg(
                ON_PROGRESS_CHANGED, this.deviceId);
            assert(this.updateCompletedPromise);
            this.updateCompletedPromise.resolve();
          }
        });
  }

  beginUpdate(deviceId: string, path: FilePath): void {
    assert(deviceId);
    assert(path);
    assert(this.startUpdatePromise);
    this.triggerProgressChangedObserver();
  }

  setDeviceIdForUpdateInProgress(deviceId: string): void {
    this.deviceId = deviceId;
  }

  /**
   * Sets the values that will be observed from observePeripheralUpdates.
   */
  setFakeInstallationProgress(
      deviceId: string, installationProgress: InstallationProgress[]): void {
    this.observables.setObservableDataForArg(
        ON_PROGRESS_CHANGED, deviceId, installationProgress);
  }

  /**
   * Returns the promise for the most recent startUpdate observation.
   */
  getStartUpdatePromiseForTesting(): Promise<void>|null {
    return this.startUpdatePromise;
  }

  /**
   * Causes the progress changed observer to fire.
   */
  triggerProgressChangedObserver(): void {
    this.observables.startTriggerOnIntervalWithArg(
        ON_PROGRESS_CHANGED, this.deviceId, this.updateIntervalInMs);
  }

  registerObservables(): void {
    this.observables.registerObservableWithArg(ON_PROGRESS_CHANGED);
    // Set up fake installation progress data for each fake firmware update.
    fakeFirmwareUpdates.flat().forEach(({deviceId}) => {
      // Use the third fake firmware update to mock a failed installation.
      if (deviceId === '3') {
        this.setFakeInstallationProgress(
            deviceId, fakeInstallationProgressFailure);
      } else {
        this.setFakeInstallationProgress(deviceId, fakeInstallationProgress);
      }
    });
  }

  /**
   * Disables all observers and resets controller to its initial state.
   */
  reset(): void {
    this.stopTriggerIntervals();
    this.observables = new FakeObservables();
    this.completedFirmwareUpdates.clear();
    this.deviceId = '';
    this.isUpdateInProgress = false;
    this.registerObservables();
  }

  /**
   * Stop automatically triggering observables.
   */
  stopTriggerIntervals(): void {
    this.observables.stopAllTriggerIntervals();
  }

  /*
   * Sets up an observer for a methodName that takes an additional arg.
   */
  private observeWithArg<T>(
      methodName: string, arg: string,
      callback: (arg: T) => void): Promise<void> {
    return new Promise((resolve) => {
      this.observables.observeWithArg(methodName, arg, callback);
      resolve();
    });
  }

  /**
   * Returns true when the promise stored in |startUpdatePromise| has not
   * resolved.
   */
  getIsUpdateInProgressForTesting(): boolean {
    return this.isUpdateInProgress;
  }

  setUpdateIntervalInMs(intervalMs: number): void {
    this.updateIntervalInMs = intervalMs;
  }

  /**
   * Remove the completed firmware update and trigger the list observer.
   */
  private updateDeviceList(): void {
    const updatedFakeFirmwareUpdates = fakeFirmwareUpdates.flat().filter(
        u => !this.completedFirmwareUpdates.has(u.deviceId));

    const provider = getUpdateProvider() as UpdateProviderInterface &
        FakeUpdateProviderInterface;
    provider.setFakeFirmwareUpdates(
        [updatedFakeFirmwareUpdates] as FirmwareUpdate[][]);
    provider.triggerDeviceAddedObserver();
  }

  /**
   * Returns the pending run routine promise.
   */
  getUpdateCompletedPromiseForTesting(): Promise<void> {
    assert(this.updateCompletedPromise != null);
    return this.updateCompletedPromise.promise;
  }

  /**
   * Returns a list of |deviceId|s representing completed firmware updates.
   */
  getCompletedFirmwareUpdatesForTesting(): Set<string> {
    return this.completedFirmwareUpdates;
  }
}
