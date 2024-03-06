// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertExists} from '../assert.js';
import {reportError} from '../error.js';
import {I18nString} from '../i18n_string.js';
import * as loadTimeData from '../models/load_time_data.js';
import {DeviceOperator} from '../mojo/device_operator.js';
import {speak} from '../spoken_msg.js';
import {ErrorLevel, ErrorType, Facing, VideoConfig} from '../type.js';
import {sleep} from '../util.js';
import {WaitableEvent} from '../waitable_event.js';

import {Camera3DeviceInfo} from './camera3_device_info.js';
import {
  StreamConstraints,
  toMediaStreamConstraints,
} from './stream_constraints.js';

/**
 * The singleton instance of StreamManager. Initialized by the first
 * invocation of getInstance().
 */
let instance: StreamManager|null = null;

/**
 * Device information includes MediaDeviceInfo and Camera3DeviceInfo.
 */
export interface DeviceInfo {
  v1Info: MediaDeviceInfo;
  v3Info: Camera3DeviceInfo|null;
}

/**
 * Real and virtual device mapping.
 */
interface VirtualMap {
  realId: string;
  virtualId: string;
}

/**
 * Monitors device change and provides different listener callbacks for
 * device changes. It also provides streams for different modes.
 */
export class StreamManager {
  /**
   * MediaDeviceInfo of all available video devices.
   */
  private devicesInfo: Promise<MediaDeviceInfo[]>|null = null;

  /**
   * Camera3DeviceInfo of all available video devices. It's null on HALv1 device
   * without mojo api support.
   */
  private camera3DevicesInfo: Promise<DeviceInfo[]|null>|null = null;

  /**
   * Listeners for real device change event.
   */
  private readonly realListeners: Array<(devices: DeviceInfo[]) => void> = [];

  /**
   * Latest result of Camera3DeviceInfo of all real video devices.
   */
  private realDevices: DeviceInfo[] = [];

  /**
   * Real device id to corresponding virtual devices id mapping and it is
   * only available on HALv3.
   */
  private virtualMap: VirtualMap|null = null;

  /**
   * Signals it to indicate that the virtual device is ready.
   */
  private waitVirtual: WaitableEvent<string>|null = null;

  /**
   * Signals to indicate that the virtual device is successfully removed.
   */
  private waitVirtualRemoved: WaitableEvent|null = null;

  /**
   * Filters out lagging 720p on grunt. See https://crbug.com/1122852.
   */
  private readonly videoConfigFilter: (config: VideoConfig) => boolean;

  private constructor() {
    this.videoConfigFilter = (() => {
      const board = loadTimeData.getBoard();
      return board === 'grunt' ? ({height}: VideoConfig) => height < 720 :
                                 () => true;
    })();

    navigator.mediaDevices.addEventListener(
        'devicechange', () => this.deviceUpdate());
  }

  /**
   * Creates a new instance of StreamManager if it is not set. Returns the
   *     existing instance.
   *
   * @return The singleton instance.
   */
  static getInstance(): StreamManager {
    if (instance === null) {
      instance = new StreamManager();
    }
    return instance;
  }

  /**
   * Registers listener to be called when state of available real devices
   * changes.
   */
  addRealDeviceChangeListener(listener: (devices: DeviceInfo[]) => void): void {
    this.realListeners.push(listener);
  }

  /**
   * Creates extra stream according to the constraints.
   */
  async openCaptureStream(constraints: StreamConstraints):
      Promise<MediaStream> {
    const realDeviceId = constraints.deviceId;
    if (DeviceOperator.isSupported()) {
      try {
        await this.setVirtualDeviceEnabled(realDeviceId, true);
        assert(this.virtualMap !== null);
        constraints.deviceId = this.virtualMap.virtualId;
      } catch (e) {
        reportError(ErrorType.MULTIPLE_STREAMS_FAILURE, ErrorLevel.ERROR, e);
      }
    }

    const stream = await navigator.mediaDevices.getUserMedia(
        toMediaStreamConstraints(constraints));
    return stream;
  }

  /**
   * Closes the given `captureStream`.
   */
  async closeCaptureStream(captureStream: MediaStream): Promise<void> {
    assertExists(captureStream.getVideoTracks()[0]).stop();
    captureStream.getAudioTracks()[0]?.stop();
    const deviceOperator = DeviceOperator.getInstance();
    if (deviceOperator !== null) {
      // We need to cache |virtualId| first since it will be wiped out after
      // disabling multi-stream.
      assert(this.virtualMap !== null);
      const virtualId = this.virtualMap.virtualId;
      try {
        await this.setVirtualDeviceEnabled(this.virtualMap.realId, false);
      } catch (e) {
        reportError(ErrorType.MULTIPLE_STREAMS_FAILURE, ErrorLevel.ERROR, e);
      }
      await deviceOperator.dropConnection(virtualId);
    }
  }

  /**
   * Handling function for device changing.
   */
  async deviceUpdate(): Promise<void> {
    const devices = await this.doDeviceInfoUpdate();
    if (devices === null) {
      return;
    }
    this.doDeviceNotify(devices);
  }

  /**
   * Updates devices information via mojo IPC.
   */
  private async doDeviceInfoUpdate(): Promise<DeviceInfo[]|null> {
    this.devicesInfo = this.enumerateDevices();
    this.camera3DevicesInfo = this.queryMojoDevicesInfo();
    try {
      return await this.camera3DevicesInfo;
    } catch (e) {
      if (loadTimeData.isVideoCaptureDisallowed()) {
        // The failure is expected due to the policy so don't throw any error.
        // TODO(b/297317408): Show messages on the UI.
        // eslint-disable-next-line no-console
        console.log('Failed to load camera since it is blocked by policy');
      } else {
        reportError(ErrorType.DEVICE_INFO_UPDATE_FAILURE, ErrorLevel.ERROR, e);
      }
    }
    return null;
  }

  /**
   * Notifies device changes to listeners and creates a mapping for real and
   * virtual device.
   */
  private doDeviceNotify(devices: DeviceInfo[]) {
    function isVirtual(d: DeviceInfo) {
      return d.v3Info !== null &&
          (d.v3Info.facing === Facing.VIRTUAL_USER ||
           d.v3Info.facing === Facing.VIRTUAL_ENV ||
           d.v3Info.facing === Facing.VIRTUAL_EXT);
    }
    const realDevices = devices.filter((d) => !isVirtual(d));
    const virtualDevices = devices.filter(isVirtual);
    // We currently only support one virtual device.
    assert(virtualDevices.length <= 1);

    if (virtualDevices.length === 1 && this.waitVirtual !== null) {
      this.waitVirtual.signal(virtualDevices[0].v1Info.deviceId);
      this.waitVirtual = null;
    }

    if (virtualDevices.length === 0 && this.waitVirtualRemoved !== null) {
      this.waitVirtualRemoved.signal();
      this.waitVirtualRemoved = null;
    }

    let isRealDeviceChange = false;
    for (const added of this.getDifference(realDevices, this.realDevices)) {
      speak(I18nString.STATUS_MSG_CAMERA_PLUGGED, added.v1Info.label);
      isRealDeviceChange = true;
    }
    for (const removed of this.getDifference(this.realDevices, realDevices)) {
      speak(I18nString.STATUS_MSG_CAMERA_UNPLUGGED, removed.v1Info.label);
      isRealDeviceChange = true;
    }
    if (isRealDeviceChange) {
      for (const listener of this.realListeners) {
        listener(realDevices);
      }
    }
    this.realDevices = realDevices;
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
  private async queryMojoDevicesInfo(): Promise<DeviceInfo[]|null> {
    const deviceInfos = await this.devicesInfo;
    assert(deviceInfos !== null);
    const isV3Supported = DeviceOperator.isSupported();
    return Promise.all(deviceInfos.map(
        async (d) => ({
          v1Info: d,
          v3Info: isV3Supported ?
              (await Camera3DeviceInfo.create(d, this.videoConfigFilter)) :
              null,
        })));
  }

  /**
   * Enables/Disables virtual device on target camera device. The extra
   * stream will be reported as virtual video device from
   * navigator.mediaDevices.enumerateDevices().
   */
  async setVirtualDeviceEnabled(deviceId: string, enabled: boolean):
      Promise<void> {
    const deviceOperator = DeviceOperator.getInstance();
    assert(deviceOperator !== null);

    if (enabled) {
      const waitEvent = new WaitableEvent<string>();
      this.waitVirtual = waitEvent;

      await deviceOperator.setVirtualDeviceEnabled(deviceId, enabled);
      await this.deviceUpdate();

      const virtualId = await waitEvent.timedWait(3000);
      this.virtualMap = {realId: deviceId, virtualId};
    } else {
      const waitEvent = new WaitableEvent();
      this.waitVirtualRemoved = waitEvent;

      await deviceOperator.setVirtualDeviceEnabled(deviceId, enabled);
      await this.deviceUpdate();

      await waitEvent.timedWait(3000);
      this.virtualMap = null;
    }
  }
}
