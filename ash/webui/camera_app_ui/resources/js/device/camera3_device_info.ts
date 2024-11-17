// Copyright 2019 The Chromium Authors
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
 * Groups resolutions with same ratio into same list.
 *
 * @param resolutions Resolutions to be grouped.
 * @return Ratio as key, all resolutions with that ratio as value.
 */
function groupResolutionRatio(resolutions: Resolution[]):
    Map<number, Resolution[]> {
  const result = new Map<number, ResolutionList>();
  for (const r of resolutions) {
    const ratio = r.aspectRatio;
    const ratios = result.get(ratio) ?? [];
    ratios.push(r);
    result.set(ratio, ratios);
  }
  return result;
}

export type CapturePreviewPairs =
    Array<{captureResolutions: Resolution[], previewResolutions: Resolution[]}>;

/**
 * Video device information queried from HALv3 mojo private API.
 */
export class Camera3DeviceInfo {
  readonly deviceId: string;

  readonly videoResolutions: ResolutionList = [];

  readonly videoMaxFps: MaxFpsInfo = {};

  readonly photoPreviewPairs: CapturePreviewPairs;

  readonly videoPreviewPairs: CapturePreviewPairs;

  /**
   * @param deviceInfo Information of the video device.
   * @param facing Camera facing of the video device.
   * @param photoResolutions Supported available photo resolutions
   *     of the video device.
   * @param videoResolutionFpses Supported available video
   *     resolutions and maximal capture fps of the video device.
   * @param fpsRanges Supported fps ranges of the video device.
   * @param builtinPtzSupport Is PTZ controls supported by the camera.
   */
  constructor(
      deviceInfo: MediaDeviceInfo,
      readonly facing: Facing,
      readonly photoResolutions: ResolutionList,
      videoResolutionFpses: VideoConfig[],
      readonly fpsRanges: FpsRangeList,
      readonly builtinPtzSupport: boolean,
  ) {
    this.deviceId = deviceInfo.deviceId;
    // If all fps supported by the camera is lower than 24, use the maximum
    // supported fps.
    const maxSupportedFps =
        Math.max(...videoResolutionFpses.map(({maxFps}) => maxFps));
    for (const {width, height, maxFps} of videoResolutionFpses) {
      if (maxFps < 24 && maxFps !== maxSupportedFps) {
        continue;
      }
      const r = new Resolution(width, height);
      this.videoResolutions.push(r);
      this.videoMaxFps[r.toString()] = maxFps;
    }
    this.photoPreviewPairs =
        this.pairedPreviewCaptureResolutions(this.photoResolutions);
    this.videoPreviewPairs =
        this.pairedPreviewCaptureResolutions(this.videoResolutions);
  }

  /**
   * @return The constant fps supported by this video resolution.
   */
  getConstFpses(videoResolution: Resolution): number[] {
    const deviceConstFpses =
        this.fpsRanges.filter(({minFps, maxFps}) => minFps === maxFps)
            .map(({minFps}) => minFps);
    const resolutionMaxFps = this.videoMaxFps[videoResolution.toString()];
    return deviceConstFpses.filter((constFps) => constFps <= resolutionMaxFps);
  }

  private pairedPreviewCaptureResolutions(captureRs: Resolution[]):
      CapturePreviewPairs {
    // Filters out preview resolution greater than 1920x1080 and 1600x1200 for
    // preventing performance issue.
    const previewRs = this.videoResolutions.filter(
        ({width, height}) => width <= 1920 && height <= 1200);
    const previewRatios = groupResolutionRatio(previewRs);
    const captureRatios = groupResolutionRatio(captureRs);
    // Pairing preview and capture resolution with same aspect ratio.
    const pairedResolutions: CapturePreviewPairs = [];
    for (const [ratio, captureResolutions] of captureRatios) {
      const previewResolutions = previewRatios.get(ratio);
      if (previewResolutions === undefined) {
        continue;
      }
      pairedResolutions.push({captureResolutions, previewResolutions});
    }
    return pairedResolutions;
  }

  /**
   * Creates a Camera3DeviceInfo by the given device info and the mojo device
   *     operator.
   *
   * @param deviceInfo Given device info.
   * @param videoConfigFilter Filters the available video capability exposed by
   *     device.
   * @throws Thrown when the device operation is not supported.
   */
  static async create(
      deviceInfo: MediaDeviceInfo,
      videoConfigFilter: (videoConfig: VideoConfig) => boolean):
      Promise<Camera3DeviceInfo> {
    const deviceId = deviceInfo.deviceId;

    const deviceOperator = DeviceOperator.getInstance();
    if (deviceOperator === null) {
      throw new Error('Device operation is not supported');
    }
    const facing = await deviceOperator.getCameraFacing(deviceId);
    // Check if the camera has PTZ support, not the PTZ support through stream
    // manipulators, by checking with USB HAL vendor tags.
    const builtinPtzSupport =
        !(await deviceOperator.isDigitalZoomSupported(deviceId)) &&
        ((await deviceOperator.getPanDefault(deviceId)) !== undefined ||
         (await deviceOperator.getTiltDefault(deviceId)) !== undefined ||
         (await deviceOperator.getZoomDefault(deviceId)) !== undefined);
    const photoResolution = await deviceOperator.getPhotoResolutions(deviceId);
    const videoConfigs = await deviceOperator.getVideoConfigs(deviceId);
    const filteredVideoConfigs = videoConfigs.filter(videoConfigFilter);
    const supportedFpsRanges =
        await deviceOperator.getSupportedFpsRanges(deviceId);

    return new Camera3DeviceInfo(
        deviceInfo, facing, photoResolution, filteredVideoConfigs,
        supportedFpsRanges, builtinPtzSupport);
  }
}
