// Copyright 2019 The Chromium Authors
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
    StreamManager.getInstance().addRealDeviceChangeListener((devicesInfo) => {
      this.pendingDevicesInfo = devicesInfo;
      this.update();
    });
  }

  /**
   * Updates devices information.
   */
  private update() {
    this.devicesInfo = this.pendingDevicesInfo.map((d) => d.v1Info);
    if (DeviceOperator.isSupported()) {
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
