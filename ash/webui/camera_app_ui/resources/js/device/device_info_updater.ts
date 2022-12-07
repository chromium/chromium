// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertExists} from '../assert.js';
import {DeviceOperator} from '../mojo/device_operator.js';

import {
  DeviceInfo,
  StreamManager,
} from './stream_manager.js';
import {CameraInfo} from './type.js';

type DeviceChangeListener = (cameraInfo: CameraInfo) => void;

/**
 * Contains information of all cameras on the device and will updates its value
 * when any plugin/unplug external camera changes.
 */
export class DeviceInfoUpdater {
  /**
   * Listeners to be called after new camera information is available.
   */
  private readonly deviceChangeListeners: DeviceChangeListener[] = [];

  constructor() {
    StreamManager.getInstance().addRealDeviceChangeListener((devicesInfo) => {
      this.update(devicesInfo);
    });
  }

  /**
   * Updates devices information.
   *
   * @param devicesInfo Updated devices info.
   */
  private update(devicesInfo: DeviceInfo[]) {
    const v1DevicesInfo = devicesInfo.map((d) => d.v1Info);
    const camera3DevicesInfo = (DeviceOperator.isSupported()) ?
        devicesInfo.map((d) => assertExists(d.v3Info)) :
        null;
    for (const listener of this.deviceChangeListeners) {
      listener(new CameraInfo(v1DevicesInfo, camera3DevicesInfo));
    }
  }

  /**
   * Registers listener to be called when state of available devices changes.
   */
  addDeviceChangeListener(listener: DeviceChangeListener): void {
    this.deviceChangeListeners.push(listener);
  }
}
