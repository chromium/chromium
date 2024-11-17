// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertExists, assertInstanceof} from '../assert.js';
import {DeviceOperator} from '../mojo/device_operator.js';
import {
  AspectRatioSet,
  Facing,
  Mode,
  PhotoResolutionLevel,
  Resolution,
  VideoResolutionLevel,
} from '../type.js';

import {Camera3DeviceInfo} from './camera3_device_info.js';
import {CaptureCandidate} from './capture_candidate.js';
import {DeviceInfo} from './device_monitor.js';
import {CaptureHandler} from './mode/index.js';

/**
 * All supported constant fps options of video recording.
 */
export const SUPPORTED_CONSTANT_FPS = [30, 60];


export interface ModeConstraints {
  kind: 'default'|'exact';
  mode: Mode;
}

export type CameraViewUi = CaptureHandler;

export class CameraInfo {
  readonly devicesInfo: MediaDeviceInfo[];

  readonly camera3DevicesInfo: Camera3DeviceInfo[]|null;

  private readonly idToDeviceInfo: Map<string, MediaDeviceInfo>;

  private readonly idToCamera3DeviceInfo: Map<string, Camera3DeviceInfo>|null;

  constructor(rawDevicesInfo: DeviceInfo[]) {
    this.devicesInfo = rawDevicesInfo.map((d) => d.v1Info);
    this.camera3DevicesInfo = (DeviceOperator.isSupported()) ?
        rawDevicesInfo.map((d) => assertExists(d.v3Info)) :
        null;
    this.idToDeviceInfo = new Map(this.devicesInfo.map((d) => [d.deviceId, d]));
    this.idToCamera3DeviceInfo = this.camera3DevicesInfo === null ?
        null :
        new Map(this.camera3DevicesInfo.map((d) => [d.deviceId, d]));
  }

  getDeviceInfo(deviceId: string): MediaDeviceInfo {
    const info = this.idToDeviceInfo.get(deviceId);
    assert(info !== undefined);
    return info;
  }

  getCamera3DeviceInfo(deviceId: string): Camera3DeviceInfo|null {
    if (this.idToCamera3DeviceInfo === null) {
      return null;
    }
    const info = this.idToCamera3DeviceInfo.get(deviceId);
    return assertInstanceof(info, Camera3DeviceInfo);
  }

  hasBuiltinPtzSupport(deviceId: string): boolean {
    const info = this.getCamera3DeviceInfo(deviceId);
    return info === null ? false : info.builtinPtzSupport;
  }
}

/**
 * The configuration of currently opened camera or the configuration which
 * camera will be opened with.
 */
export interface CameraConfig {
  deviceId: string;
  facing: Facing;
  mode: Mode;
  captureCandidate: CaptureCandidate;
}

/**
 * The next |CameraConfig| to be tried.
 */
export interface CameraConfigCandidate {
  /**
   * The only null case is for opening the default facing camera on non-ChromeOS
   * VCD.
   */
  deviceId: string|null;
  /**
   * On device using non-ChromeOS VCD, camera facing is unknown before opening
   * the camera.
   */
  facing: Facing|null;
  mode: Mode;
  captureCandidate: CaptureCandidate;
}

export interface CameraUi {
  onUpdateCapability?(cameraInfo: CameraInfo): void;
  onTryingNewConfig?(config: CameraConfigCandidate): void;
  onUpdateConfig?(config: CameraConfig): Promise<void>|void;
  onCameraUnavailable?(): void;
  onCameraAvailable?(): void;
}

export interface BaseSettingsOptionGroup<T extends BaseSettingsOption> {
  deviceId: string;
  facing: Facing;
  options: T[];
}

export interface BaseSettingsOption {
  checked: boolean;
}

export type PhotoResolutionOptionListener =
    (groups: PhotoResolutionOptionGroup[]) => void;

export type PhotoResolutionOptionGroup =
    BaseSettingsOptionGroup<PhotoResolutionOption>;

export interface PhotoResolutionOption extends BaseSettingsOption {
  resolutionLevel: PhotoResolutionLevel;
  resolutions: Resolution[];
}

export type PhotoAspectRatioOptionListener =
    (groups: PhotoAspectRatioOptionGroup[]) => void;

export type PhotoAspectRatioOptionGroup =
    BaseSettingsOptionGroup<PhotoAspectRatioOption>;

export interface PhotoAspectRatioOption extends BaseSettingsOption {
  aspectRatioSet: AspectRatioSet;
}

export type VideoResolutionOptionListener =
    (groups: VideoResolutionOptionGroup[]) => void;

export type VideoResolutionOptionGroup =
    BaseSettingsOptionGroup<VideoResolutionOption>;

export interface VideoResolutionOption extends BaseSettingsOption {
  resolutionLevel: VideoResolutionLevel;
  fpsOptions: VideoFpsOption[];
}

export interface VideoFpsOption extends BaseSettingsOption {
  constFps: number|null;
  resolutions: Resolution[];
}
