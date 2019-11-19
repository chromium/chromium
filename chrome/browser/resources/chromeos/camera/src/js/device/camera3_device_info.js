// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * Namespace for device.
 */
cca.device = cca.device || {};

/**
 * import {Resolution} from '../type.js';
 */
var Resolution = Resolution || {};

/**
 * Video device information queried from HALv3 mojo private API.
 */
cca.device.Camera3DeviceInfo = class {
  /**
   * @public
   * @param {!MediaDeviceInfo} deviceInfo Information of the video device.
   * @param {!cros.mojom.CameraFacing} facing Camera facing of the video device.
   * @param {!ResolutionList} photoResols Supported available photo resolutions
   *     of the video device.
   * @param {!Array<!VideoConfig>} videoResolFpses Supported available video
   *     resolutions and maximal capture fps of the video device.
   * @param {!FpsRangeList} fpsRanges Supported fps ranges of the video device.
   */
  constructor(deviceInfo, facing, photoResols, videoResolFpses, fpsRanges) {
    /**
     * @type {string}
     * @public
     */
    this.deviceId = deviceInfo.deviceId;

    /**
     * @type {cros.mojom.CameraFacing}
     * @public
     */
    this.facing = facing;

    /**
     * @type {!ResolutionList}
     * @public
     */
    this.photoResols = photoResols;

    /**
     * @type {!ResolutionList}
     * @public
     */
    this.videoResols = [];

    /**
     * @type {!MaxFpsInfo}
     * @public
     */
    this.videoMaxFps = {};

    /**
     * @type {!FpsRangeList}
     * @public
     */
    this.fpsRanges = fpsRanges;

    videoResolFpses.filter(({maxFps}) => maxFps >= 24)
        .forEach(({width, height, maxFps}) => {
          const r = new Resolution(width, height);
          this.videoResols.push(r);
          this.videoMaxFps[r] = maxFps;
        });
  }

  /**
   * Create a Camera3DeviceInfo by given device info and the mojo device
   *     operator.
   * @param {!MediaDeviceInfo} deviceInfo
   * @return {!Promise<!cca.device.Camera3DeviceInfo>}
   * @throws {Error} Thrown when the device operation is not supported.
   */
  static async create(deviceInfo) {
    const deviceId = deviceInfo.deviceId;

    const deviceOperator = await cca.mojo.DeviceOperator.getInstance();
    if (!deviceOperator) {
      throw new Error('Device operation is not supported');
    }
    const facing = await deviceOperator.getCameraFacing(deviceId);
    const photoResolution = await deviceOperator.getPhotoResolutions(deviceId);
    const videoConfigs = await deviceOperator.getVideoConfigs(deviceId);
    const supportedFpsRanges =
        await deviceOperator.getSupportedFpsRanges(deviceId);

    return new cca.device.Camera3DeviceInfo(
        deviceInfo, facing, photoResolution, videoConfigs, supportedFpsRanges);
  }
};
