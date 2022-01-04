// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DeviceOperator} from '../mojo/device_operator.js';
import {
  Facing,
  FpsRangeList,
  MaxFpsInfo,
  Resolution,
  ResolutionList,
  VideoConfig,
} from '../type.js';

/**
 * Video device information queried from HALv3 mojo private API.
 */
export class Camera3DeviceInfo {
  readonly deviceId: string;
  readonly videoResols: ResolutionList = [];
  readonly videoMaxFps: MaxFpsInfo = {};

  /**
   * @param deviceInfo Information of the video device.
   * @param facing Camera facing of the video device.
   * @param photoResols Supported available photo resolutions
   *     of the video device.
   * @param videoResolFpses Supported available video
   *     resolutions and maximal capture fps of the video device.
   * @param fpsRanges Supported fps ranges of the video device.
   * @param supportPTZ Is supported PTZ controls.
   */
  constructor(
      deviceInfo: MediaDeviceInfo,
      readonly facing: Facing,
      readonly photoResols: ResolutionList,
      videoResolFpses: VideoConfig[],
      readonly fpsRanges: FpsRangeList,
      readonly supportPTZ: boolean,
  ) {
    this.deviceId = deviceInfo.deviceId;
    videoResolFpses.filter(({maxFps}) => maxFps >= 24)
        .forEach(({width, height, maxFps}) => {
          const r = new Resolution(width, height);
          this.videoResols.push(r);
          this.videoMaxFps[r.toString()] = maxFps;
        });
  }

  /**
   * Creates a Camera3DeviceInfo by given device info and the mojo device
   *     operator.
   * @param videoConfigFilter Filters the available video capability exposed by
   *     device.
   * @throws Thrown when the device operation is not supported.
   */
  static async create(
      deviceInfo: MediaDeviceInfo,
      videoConfigFilter: (videoConfig: VideoConfig) => boolean):
      Promise<Camera3DeviceInfo> {
    const deviceId = deviceInfo.deviceId;

    const deviceOperator = await DeviceOperator.getInstance();
    if (!deviceOperator) {
      throw new Error('Device operation is not supported');
    }
    const facing = await deviceOperator.getCameraFacing(deviceId);
    const supportPTZ =
        (await deviceOperator.getPanDefault(deviceId)) !== undefined ||
        (await deviceOperator.getTiltDefault(deviceId)) !== undefined ||
        (await deviceOperator.getZoomDefault(deviceId)) !== undefined;
    const photoResolution = await deviceOperator.getPhotoResolutions(deviceId);
    const videoConfigs = await deviceOperator.getVideoConfigs(deviceId);
    const filteredVideoConfigs = videoConfigs.filter(videoConfigFilter);
    const supportedFpsRanges =
        await deviceOperator.getSupportedFpsRanges(deviceId);

    return new Camera3DeviceInfo(
        deviceInfo, facing, photoResolution, filteredVideoConfigs,
        supportedFpsRanges, supportPTZ);
  }
}
