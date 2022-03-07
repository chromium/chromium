// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertExists} from '../assert.js';
import {DeviceOperator} from '../mojo/device_operator.js';
import {Camera3DeviceInfo} from './camera3_device_info.js';
import {
  DeviceInfo,
  StreamManager,
} from './stream_manager.js';

/**
 * Thrown for no camera available on the device.
 */
export class NoCameraError extends Error {
  constructor(message = 'No camera available on the device') {
    super(message);
    this.name = this.constructor.name;
  }
}

/**
 * Contains information of all cameras on the device and will updates its value
 * when any plugin/unplug external camera changes.
 */
export class DeviceInfoUpdater {
  /**
   * Listeners to be called after new camera information is available.
   */
  private readonly deviceChangeListeners:
      Array<(updater: DeviceInfoUpdater) => void> = [];

  /**
   * Action locking update of camera information.
   */
  private lockingUpdate: Promise<void>|null = null;

  /**
   * Pending camera information update while update capability is locked.
   */
  private pendingUpdate: Promise<void>|null = null;

  /**
   * MediaDeviceInfo of all available video devices.
   */
  private devicesInfo: MediaDeviceInfo[] = [];

  /**
   * Camera3DeviceInfo of all available video devices. Is null on fake cameras
   * which do not have private mojo API support.
   */
  private camera3DevicesInfo: Camera3DeviceInfo[]|null = null;

  /**
   * Pending device Information.
   */
  private pendingDevicesInfo: DeviceInfo[] = [];

  constructor() {
    StreamManager.getInstance().addRealDeviceChangeListener(
        async (devicesInfo) => {
          this.pendingDevicesInfo = devicesInfo;
          await this.update();
        });
  }

  /**
   * Tries to gain lock and initiates update process.
   */
  private async update() {
    if (this.lockingUpdate) {
      if (this.pendingUpdate) {
        return;
      }
      this.pendingUpdate = (async () => {
        while (this.lockingUpdate) {
          try {
            await this.lockingUpdate;
          } catch (e) {
            // Ignore exception from waiting for existing update.
          }
        }
        this.lockingUpdate = this.pendingUpdate;
        this.pendingUpdate = null;
        await this.doUpdate();
        this.lockingUpdate = null;
      })();
    } else {
      this.lockingUpdate = (async () => {
        await this.doUpdate();
        this.lockingUpdate = null;
      })();
    }
  }

  /**
   * Updates devices information.
   */
  private async doUpdate() {
    this.devicesInfo = this.pendingDevicesInfo.map((d) => d.v1Info);
    if (await DeviceOperator.isSupported()) {
      this.camera3DevicesInfo =
          this.pendingDevicesInfo.map((d) => assertExists(d.v3Info));
    } else {
      this.camera3DevicesInfo = null;
    }
    for (const listener of this.deviceChangeListeners) {
      listener(this);
    }
  }

  /**
   * Registers listener to be called when state of available devices changes.
   */
  addDeviceChangeListener(listener: (updater: DeviceInfoUpdater) => void):
      void {
    this.deviceChangeListeners.push(listener);
  }

  /**
   * Requests to lock update of device information. This function is preserved
   * for device information reader to lock the update capability so as to ensure
   * getting consistent data between all information providers.
   *
   * @param callback Called after update capability is locked. Getting
   *     information from all providers in callback are guaranteed to be
   *     consistent.
   */
  async lockDeviceInfo(callback: () => Promise<void>): Promise<void> {
    await StreamManager.getInstance().deviceUpdate();
    while (this.lockingUpdate || this.pendingUpdate) {
      try {
        await this.lockingUpdate;
        await this.pendingUpdate;
      } catch (e) {
        // Ignore exception from waiting for existing update.
      }
    }
    this.lockingUpdate = (async () => {
      try {
        await callback();
      } finally {
        this.lockingUpdate = null;
      }
    })();
    await this.lockingUpdate;
  }

  /**
   * Gets MediaDeviceInfo for all available video devices.
   */
  getDevicesInfo(): MediaDeviceInfo[] {
    return this.devicesInfo;
  }

  /**
   * Gets MediaDeviceInfo of specific video device.
   *
   * @param deviceId Device id of video device to get information from.
   */
  getDeviceInfo(deviceId: string): MediaDeviceInfo|null {
    const infos = this.getDevicesInfo();
    return infos.find((d) => d.deviceId === deviceId) ?? null;
  }

  /**
   * Gets Camera3DeviceInfo for all available video devices.
   */
  getCamera3DevicesInfo(): Camera3DeviceInfo[]|null {
    return this.camera3DevicesInfo;
  }
}
