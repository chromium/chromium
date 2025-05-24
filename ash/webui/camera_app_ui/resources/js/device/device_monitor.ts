// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {reportError} from '../error.js';
import {I18nString} from '../i18n_string.js';
import * as loadTimeData from '../models/load_time_data.js';
import {DeviceOperator} from '../mojo/device_operator.js';
import {speak} from '../spoken_msg.js';
import * as state from '../state.js';
import {ErrorLevel, ErrorType, VideoConfig} from '../type.js';
import {sleep} from '../util.js';

import {Camera3DeviceInfo} from './camera3_device_info.js';

/**
 * DeviceInfo includes MediaDeviceInfo and Camera3DeviceInfo.
 */
export interface DeviceInfo {
  v1Info: MediaDeviceInfo;
  v3Info: Camera3DeviceInfo|null;
}

/**
 * Monitors device changes and provides different listener callbacks for
 * device changes.
 */
export class DeviceMonitor {
  /**
   * Array of DeviceInfo of all available video devices.
   */
  private devicesInfo: DeviceInfo[] = [];

  /**
   * Filters out lagging 720p on grunt. See https://crbug.com/1122852.
   */
  private readonly videoConfigFilter: (config: VideoConfig) => boolean;

  /**
   * Indicates whether the device list has been updated at least once
   * since initialization.
   */
  private hasUpdated = false;

  constructor(private readonly listener: (devices: DeviceInfo[]) => void) {
    if (loadTimeData.getBoard() === 'grunt') {
      this.videoConfigFilter = ({height}: VideoConfig) => {
        if (state.get(
                state.State.DISABLE_VIDEO_RESOLUTION_FILTER_FOR_TESTING)) {
          return true;
        }
        return height < 720;
      };
    } else {
      this.videoConfigFilter = () => true;
    }

    navigator.mediaDevices.addEventListener(
        'devicechange', () => this.deviceUpdate());
  }

  /**
   * Handling function for device changing.
   */
  async deviceUpdate(): Promise<void> {
    const devices = await this.doDeviceInfoUpdate();
    this.doDeviceNotify(devices);
  }

  /**
   * Updates devices information via mojo IPC.
   */
  private async doDeviceInfoUpdate(): Promise<DeviceInfo[]> {
    try {
      const devicesInfo = await this.enumerateDevices();
      return await this.queryMojoDevicesInfo(devicesInfo);
    } catch (e) {
      if (loadTimeData.isCCADisallowed()) {
        // The failure is expected due to the policy so don't throw any error.
        // TODO(b/297317408): Show messages on the UI.
        // eslint-disable-next-line no-console
        console.log('Failed to load camera since it is blocked by policy');
      } else {
        reportError(ErrorType.DEVICE_INFO_UPDATE_FAILURE, ErrorLevel.ERROR, e);
      }
    }
    return this.devicesInfo;
  }

  /**
   * Notifies device changes to listeners and creates a mapping for real and
   * virtual device.
   */
  private doDeviceNotify(devices: DeviceInfo[]) {
    let isDeviceChanged = false;
    for (const added of this.getDifference(devices, this.devicesInfo)) {
      speak(I18nString.STATUS_MSG_CAMERA_PLUGGED, added.v1Info.label);
      isDeviceChanged = true;
    }
    for (const removed of this.getDifference(this.devicesInfo, devices)) {
      speak(I18nString.STATUS_MSG_CAMERA_UNPLUGGED, removed.v1Info.label);
      isDeviceChanged = true;
    }
    if (!this.hasUpdated || isDeviceChanged) {
      this.listener(devices);
    }
    this.devicesInfo = devices;
    this.hasUpdated = true;
  }

  /**
   * Computes |devices| - |devices2|.
   */
  private getDifference(devices: DeviceInfo[], devices2: DeviceInfo[]):
      DeviceInfo[] {
    const ids = new Set(devices2.map((d) => d.v1Info.deviceId));
    return devices.filter((d) => !ids.has(d.v1Info.deviceId));
  }

  /**
   * Enumerates all available devices and gets their MediaDeviceInfo. Retries at
   * one-second intervals if devices length is zero.
   */
  private async enumerateDevices(): Promise<MediaDeviceInfo[]> {
    const deviceType = loadTimeData.getDeviceType();
    const shouldHaveBuiltinCamera =
        deviceType === 'chromebook' || deviceType === 'chromebase';
    let attempts = 5;
    while (attempts-- > 0) {
      const devices = (await navigator.mediaDevices.enumerateDevices())
                          .filter((device) => device.kind === 'videoinput');
      if (!shouldHaveBuiltinCamera || devices.length > 0) {
        return devices;
      }
      await sleep(1000);
    }
    throw new Error('Device list empty.');
  }

  /**
   * Queries Camera3DeviceInfo of available devices through private mojo API.
   *
   * @return Camera3DeviceInfo of available devices. Maybe null on HALv1
   *     devices without supporting private mojo api.
   * @throws Thrown when camera unplugging happens between enumerating devices
   *     and querying mojo APIs with current device info results.
   */
  private async queryMojoDevicesInfo(devices: MediaDeviceInfo[]):
      Promise<DeviceInfo[]> {
    const isV3Supported = DeviceOperator.isSupported();
    return Promise.all(devices.map(
        async (d) => ({
          v1Info: d,
          v3Info: isV3Supported ?
              (await Camera3DeviceInfo.create(d, this.videoConfigFilter)) :
              null,
        })));
  }
}
