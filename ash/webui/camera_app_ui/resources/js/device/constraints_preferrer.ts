// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertExists} from '../assert.js';
import * as localStorage from '../models/local_storage.js';
import * as state from '../state.js';
import {
  Facing,
  Resolution,
  ResolutionList,
} from '../type.js';

import {Camera3DeviceInfo} from './camera3_device_info.js';
import {StreamConstraints} from './stream_constraints.js';

/**
 * Candidate of capturing with specified photo or video resolution and
 * constraints-candidates it corresponding preview.
 * Video/photo capture resolution and the constraints-candidates of its
 * corresponding preview stream.
 */
export interface CaptureCandidate {
  resolution: Resolution|null;
  previewCandidates: StreamConstraints[];
}

/**
 * Controller for managing preference of capture settings and generating a list
 * of stream constraints-candidates sorted by user preference.
 */
export abstract class ConstraintsPreferrer {
  /**
   * Map saving resolution preference that each of its key as device id and
   * value to be preferred width, height of resolution of that video device.
   */
  protected prefResolution = new Map<string, Resolution>();

  /**
   * Device id of currently working video device.
   */
  protected deviceId: string|null = null;

  /**
   * Maps video device id to all of available capture resolutions supported by
   * that video device.
   */
  protected supportedResolutions = new Map<string, ResolutionList>();

  /**
   * Restores saved preferred capture resolution per video device.
   *
   * @param key Key of local storage saving preferences.
   */
  protected restoreResolutionPreference(key: string): void {
    const preference =
        localStorage.getObject<{width: number, height: number}>(key);
    this.prefResolution = new Map();
    for (const [deviceId, {width, height}] of Object.entries(preference)) {
      this.prefResolution.set(deviceId, new Resolution(width, height));
    }
  }

  /**
   * Saves preferred capture resolution per video device.
   *
   * @param key Key of local storage saving preferences.
   */
  protected saveResolutionPreference(key: string): void {
    localStorage.set(key, Object.fromEntries(this.prefResolution));
  }

  /**
   * Gets user preferred capture resolution for a specific device.
   *
   * @param deviceId Device id of the device.
   * @return Returns preferred resolution or null if no preferred resolution
   *     found in user preference.
   */
  getPrefResolution(deviceId: string): Resolution|null {
    return this.prefResolution.get(deviceId) ?? null;
  }

  /**
   * Updates with new video device information.
   */
  abstract updateDevicesInfo(devices: Camera3DeviceInfo[]): void;

  /**
   * Updates values according to currently working video device and capture
   * settings.
   *
   * @param deviceId Device id of video device to be updated.
   * @param stream Currently active preview stream.
   * @param facing Camera facing of video device to be updated.
   * @param resolution Resolution to be updated to.
   */
  abstract updateValues(
      deviceId: string, stream: MediaStream, facing: Facing,
      resolution: Resolution): void;

  /**
   * Gets all available candidates for capturing under this controller and its
   * corresponding preview constraints for the specified video device. Returned
   * resolutions and constraints candidates are both sorted in desired trying
   * order.
   *
   * @param deviceId Device id of video device.
   * @return Capture resolution and its preview constraints-candidates.
   */
  abstract getSortedCandidates(deviceId: string): CaptureCandidate[];

  /**
   * Changes user preferred capture resolution.
   *
   * @param deviceId Device id of the video device to be changed.
   * @param resolution Preferred capture resolution.
   */
  abstract changePreferredResolution(deviceId: string, resolution: Resolution):
      void;

  /**
   * Sorts the preview resolution (Rp) according to the capture resolution
   * (Rc) and the screen size (Rs) with the following orders:
   * If |Rc| <= |Rs|:
   *   1. All |Rp| <= |Rc|, and the larger, the better.
   *   2. All |Rp| > |Rc|, and the smaller, the better.
   *
   * If |Rc| > |Rs|:
   *   1. All |Rp| where |Rs| <= |Rp| <= |Rc|, and the smaller, the
   *   better.
   *   2. All |Rp| < |Rs|, and the larger, the better.
   *   3. All |Rp| > |Rc|, and the smaller, the better.
   *
   * Note that generally we compare resolutions by their width. But since the
   * aspect ratio of |Rs| might be different from the |Rc| and |Rp|, we also
   * consider |screenHeight * captureAspectRatio| as a possible |Rs| and prefer
   * using the smaller one.
   */
  protected sortPreview(
      previewResolutions: ResolutionList,
      captureResolution: Resolution): ResolutionList {
    if (previewResolutions.length === 0) {
      return [];
    }

    const screenWidth =
        Math.floor(window.screen.width * window.devicePixelRatio);
    const screenHeight =
        Math.floor(window.screen.height * window.devicePixelRatio);
    const aspectRatio = captureResolution.width / captureResolution.height;
    const rs = Math.min(screenWidth, Math.floor(screenHeight * aspectRatio));
    const rc = captureResolution.width;
    function cmpDescending(r1: Resolution, r2: Resolution) {
      return r2.width - r1.width;
    }
    function cmpAscending(r1: Resolution, r2: Resolution) {
      return r1.width - r2.width;
    }

    if (rc <= rs) {
      const notLargerThanR =
          previewResolutions.filter((r) => r.width <= rc).sort(cmpDescending);
      const largerThanR =
          previewResolutions.filter((r) => r.width > rc).sort(cmpAscending);
      return notLargerThanR.concat(largerThanR);
    } else {
      const betweenRsR =
          previewResolutions.filter((r) => rs <= r.width && r.width <= rc)
              .sort(cmpAscending);
      const smallerThanRs =
          previewResolutions.filter((r) => r.width < rs).sort(cmpDescending);
      const largerThanR =
          previewResolutions.filter((r) => r.width > rc).sort(cmpAscending);
      return betweenRsR.concat(smallerThanRs).concat(largerThanR);
    }
  }

  /**
   * Sorts prefer resolutions.
   *
   * @param prefResolution Preferred resolution.
   * @return Return compare function for comparing based on preferred
   *     resolution.
   */
  protected getPreferResolutionSort(prefResolution: Resolution):
      (c0: CaptureCandidate, c1: CaptureCandidate) => number {
    return ({resolution: r1}, {resolution: r2}) => {
      if (r1 === null || r2 === null) {
        return (r1 === null ? 1 : 0) - (r2 === null ? 1 : 0);
      }
      if (r1.equals(r2)) {
        return 0;
      }
      // Exactly the preferred resolution.
      if (r1.equals(prefResolution)) {
        return -1;
      }
      if (r2.equals(prefResolution)) {
        return 1;
      }

      // Aspect ratio same as preferred resolution.
      if (!r1.aspectRatioEquals(r2)) {
        if (r1.aspectRatioEquals(prefResolution)) {
          return -1;
        }
        if (r2.aspectRatioEquals(prefResolution)) {
          return 1;
        }
      }
      return r2.area - r1.area;
    };
  }

  /**
   * Groups resolutions with same ratio into same list.
   *
   * @return Ratio as key, all resolutions with that ratio as value.
   */
  protected groupResolutionRatio(resolutions: ResolutionList):
      Map<number, ResolutionList> {
    function toSupportedPreviewRatio(r: Resolution): number {
      // Special aspect ratio mapping rule, see http://b/147986763.
      if (r.width === 848 && r.height === 480) {
        return (new Resolution(16, 9)).aspectRatio;
      }
      return r.aspectRatio;
    }

    const result = new Map<number, ResolutionList>();
    for (const r of resolutions) {
      const ratio = toSupportedPreviewRatio(r);
      const ratios = result.get(ratio) ?? [];
      ratios.push(r);
      result.set(ratio, ratios);
    }
    return result;
  }
}

/**
 * All supported constant fps options of video recording.
 */
const SUPPORTED_CONSTANT_FPS = [30, 60];

interface VideoPreviewResolutions {
  videoResolutions: ResolutionList;
  previewResolutions: ResolutionList;
}

/**
 * Controller for handling video resolution preference.
 */
export class VideoConstraintsPreferrer extends ConstraintsPreferrer {
  /**
   * Object saving information of device supported constant fps. Each of its
   * key as device id and value as an object mapping from resolution to all
   * constant fps options supported by that resolution.
   */
  private constFpsInfo: Record<string, Record<string, number[]>> = {};

  /**
   * Object saving fps preference that each of its key as device id and value
   * as an object mapping from resolution to preferred constant fps for that
   * resolution.
   */
  private prefFpses: Record<string, Record<string, number>> = {};

  /**
   * Currently in used recording resolution.
   */
  private resolution = new Resolution(0, -1);

  /**
   * Maps from device id as key to video and preview resolutions of
   * same aspect ratio supported by that video device as value.
   */
  private deviceVideoPreviewResolutionMap =
      new Map<string, VideoPreviewResolutions[]>();

  constructor() {
    super();

    this.restoreResolutionPreference('deviceVideoResolution');
    this.restoreFpsPreference();
  }

  /**
   * Restores saved preferred fps per video resolution per video device.
   */
  private restoreFpsPreference() {
    this.prefFpses = localStorage.getObject('deviceVideoFps');
  }

  /**
   * Saves preferred fps per video resolution per video device.
   */
  private saveFpsPreference(): void {
    localStorage.set('deviceVideoFps', this.prefFpses);
  }

  changePreferredResolution(deviceId: string, resolution: Resolution): void {
    this.prefResolution.set(deviceId, resolution);
    this.saveResolutionPreference('deviceVideoResolution');
  }

  /**
   * Sets the preferred fps used in video recording for particular video device
   * with particular resolution.
   *
   * @param deviceId Device id of video device to be set with.
   * @param resolution Resolution to be set with.
   * @param prefFps Preferred fps to be set with.
   */
  setPreferredConstFps(
      deviceId: string, resolution: Resolution, prefFps: number): void {
    if (!SUPPORTED_CONSTANT_FPS.includes(prefFps)) {
      return;
    }
    for (const fps of SUPPORTED_CONSTANT_FPS) {
      state.set(state.assertState(`fps-${fps}`), fps === prefFps);
    }
    const resolutionFpses = this.prefFpses[deviceId] || {};
    resolutionFpses[resolution.toString()] = prefFps;
    this.prefFpses[deviceId] = resolutionFpses;
    this.saveFpsPreference();
  }

  /**
   * Gets user preferred video constant fps for a specific device/resolution.
   *
   * @param deviceId Device id of the device.
   * @return Returns preferred fps or null if no preferred fps found in user
   *     preference.
   */
  getPreferredConstFps(deviceId: string, resolution: Resolution): number|null {
    return this.prefFpses?.[deviceId]?.[resolution.toString()] || null;
  }

  /**
   * Gets the constant fps info for particular video device with particular
   * resolution.
   */
  private getConstFpsInfo(deviceId: string, resolution: Resolution): number[] {
    return this.constFpsInfo[deviceId]?.[resolution.toString()] ?? [];
  }

  updateDevicesInfo(devices: Camera3DeviceInfo[]): void {
    this.deviceVideoPreviewResolutionMap = new Map();
    this.supportedResolutions = new Map();
    this.constFpsInfo = {};

    for (const {deviceId, videoResolutions, videoMaxFps, fpsRanges} of
             devices) {
      this.supportedResolutions.set(
          deviceId, [...videoResolutions].sort((r1, r2) => r2.area - r1.area));

      // Filter out preview resolution greater than 1920x1080 and 1600x1200.
      const previewRatios = this.groupResolutionRatio(videoResolutions.filter(
          ({width, height}) => width <= 1920 && height <= 1200));
      const videoRatios = this.groupResolutionRatio(videoResolutions);
      const pairedResolutions: VideoPreviewResolutions[] = [];
      for (const [ratio, videoResolutions] of videoRatios) {
        const previewResolutions = previewRatios.get(ratio);
        if (previewResolutions === undefined) {
          continue;
        }
        pairedResolutions.push({videoResolutions, previewResolutions});
      }
      this.deviceVideoPreviewResolutionMap.set(deviceId, pairedResolutions);

      function findResolution(width: number, height: number) {
        return videoResolutions.find(
            (r) => r.width === width && r.height === height);
      }

      let prefResolution = this.getPrefResolution(deviceId) ??
          findResolution(1920, 1080) ?? findResolution(1280, 720) ??
          new Resolution(0, -1);

      if (findResolution(prefResolution.width, prefResolution.height) ===
          undefined) {
        prefResolution = videoResolutions.reduce(
            (maxR, r) => (maxR.area < r.area ? r : maxR),
            new Resolution(0, -1));
      }
      this.prefResolution.set(deviceId, prefResolution);

      const constFpses =
          fpsRanges.filter(({minFps, maxFps}) => minFps === maxFps)
              .map(({minFps}) => minFps);
      const fpsInfo: {[s: string]: number[]} = {};
      for (const [resolution, maxFps] of Object.entries(videoMaxFps)) {
        fpsInfo[resolution.toString()] =
            constFpses.filter((fps) => fps <= maxFps);
      }
      this.constFpsInfo[deviceId] = fpsInfo;
    }
    this.saveResolutionPreference('deviceVideoResolution');
  }

  updateValues(
      deviceId: string, stream: MediaStream, facing: Facing,
      resolution: Resolution): void {
    this.deviceId = deviceId;
    this.resolution = resolution;
    this.prefResolution.set(deviceId, this.resolution);
    this.saveResolutionPreference('deviceVideoResolution');

    const videoTrack = assertExists(stream.getVideoTracks()[0]);
    const fps = assertExists(videoTrack.getSettings().frameRate);
    this.setPreferredConstFps(deviceId, this.resolution, fps);
    const supportedConstFpses =
        this.getConstFpsInfo(deviceId, this.resolution)
            .filter((fps) => SUPPORTED_CONSTANT_FPS.includes(fps));
    // Only enable multi fps UI on external camera.
    // See https://crbug.com/1059191 for details.
    state.set(
        state.State.MULTI_FPS,
        facing === Facing.EXTERNAL && supportedConstFpses.length > 1);
  }

  private getMultiStreamSortedCandidates(deviceId: string): CaptureCandidate[] {
    const prefResolution =
        this.getPrefResolution(deviceId) ?? new Resolution(0, -1);

    /**
     * Maps specified video resolution to object of resolution and all supported
     * constant fps under that resolution or null fps for not support constant
     * fps. The resolution-fpses are sorted by user preference of constant fps.
     */
    const getFpses =
        (r: Resolution): Array<{r: Resolution, fps: number | null}> => {
          let constFpses: Array<number|null> = [null];
          const constFpsInfo = this.getConstFpsInfo(deviceId, r);
          // The higher constant fps will be ignored if constant 30 and 60
          // presented due to currently lack of UI support for toggling it.
          if (constFpsInfo.includes(30) && constFpsInfo.includes(60)) {
            const prefFpses = this.prefFpses[deviceId];
            const prefFps =
                prefFpses !== undefined ? prefFpses[r.toString()] : 30;
            constFpses = prefFps === 30 ? [30, 60] : [60, 30];
          } else {
            constFpses = [
              ...constFpsInfo.filter((fps) => fps >= 30).sort((a, b) => b - a),
              null,
            ];
          }
          return constFpses.map((fps) => ({r, fps}));
        };

    const toVideoCandidate =
        ({videoResolutions,
          previewResolutions}: VideoPreviewResolutions): CaptureCandidate[] => {
          let videoResolution = prefResolution;
          if (!videoResolutions.some((r) => r.equals(prefResolution))) {
            videoResolution = videoResolutions.reduce(
                (videoR, r) => (r.width > videoR.width ? r : videoR));
          }

          return getFpses(videoResolution)
              .map(({fps}) => ({
                     resolution: videoResolution,
                     previewCandidates:
                         this.sortPreview(previewResolutions, videoResolution)
                             .map(({width, height}) => ({
                                    deviceId,
                                    audio: true,
                                    video: {
                                      frameRate: fps ? {exact: fps} :
                                                       {min: 20, ideal: 30},
                                      width,
                                      height,
                                    },
                                  })),
                   }));
        };

    return assertExists(this.deviceVideoPreviewResolutionMap.get(deviceId))
        .flatMap(toVideoCandidate)
        .sort(this.getPreferResolutionSort(prefResolution));
  }

  private getSortedCandidatesInternal(deviceId: string): CaptureCandidate[] {
    /**
     * Maps specified video resolution to object of resolution and all supported
     * constant fps under that resolution or null fps for not support constant
     * fps. The resolution-fpses are sorted by user preference of constant fps.
     */
    const getFpses =
        (r: Resolution): Array<{r: Resolution, fps: number | null}> => {
          let constFpses: Array<number|null> = [null];
          const constFpsInfo = this.getConstFpsInfo(deviceId, r);
          // The higher constant fps will be ignored if constant 30 and 60
          // presented due to currently lack of UI support for toggling it.
          if (constFpsInfo.includes(30) && constFpsInfo.includes(60)) {
            const prefFpses = this.prefFpses[deviceId];
            const prefFps =
                prefFpses !== undefined ? prefFpses[r.toString()] : 30;
            constFpses = prefFps === 30 ? [30, 60] : [60, 30];
          } else {
            constFpses = [
              ...constFpsInfo.filter((fps) => fps >= 30).sort((a, b) => b - a),
              null,
            ];
          }
          return constFpses.map((fps) => ({r, fps}));
        };

    function toPreivewConstraints(
        {width, height}: Resolution, fps: number|null): StreamConstraints {
      return {
        deviceId,
        audio: true,
        video: {
          frameRate: fps !== null ? {exact: fps} : {min: 20, ideal: 30},
          width,
          height,
        },
      };
    }

    const prefResolution =
        this.getPrefResolution(deviceId) ?? new Resolution(0, -1);
    return [...assertExists(this.supportedResolutions.get(deviceId))]
        .flatMap(getFpses)
        .map(({r, fps}) => ({
               resolution: r,
               // For non-multistream recording, preview stream is used directly
               // to do video recording.
               previewCandidates: [toPreivewConstraints(r, fps)],
             }))
        .sort(this.getPreferResolutionSort(prefResolution));
  }

  getSortedCandidates(deviceId: string): CaptureCandidate[] {
    if (state.get(state.State.ENABLE_MULTISTREAM_RECORDING)) {
      return this.getMultiStreamSortedCandidates(deviceId);
    }
    return this.getSortedCandidatesInternal(deviceId);
  }
}

interface CapturePreviewResolutions {
  captureResolutions: ResolutionList;
  previewResolutions: ResolutionList;
}

/**
 * Controller for handling photo resolution preference.
 */
export class PhotoConstraintsPreferrer extends ConstraintsPreferrer {
  /**
   * Maps from device id as key to capture and preview resolutions of
   * same aspect ratio supported by that video device as value.
   */
  private deviceCapturePreviewResolutionMap =
      new Map<string, CapturePreviewResolutions[]>();

  /**
   * Maps from device id as key to whether PTZ is support from device level.
   */
  private devicePTZSupportMap = new Map<string, boolean>();

  constructor() {
    super();

    this.restoreResolutionPreference('devicePhotoResolution');
  }

  changePreferredResolution(deviceId: string, resolution: Resolution): void {
    this.prefResolution.set(deviceId, resolution);
    this.saveResolutionPreference('devicePhotoResolution');
  }

  updateDevicesInfo(devices: Camera3DeviceInfo[]): void {
    this.deviceCapturePreviewResolutionMap = new Map();
    this.supportedResolutions = new Map();
    this.devicePTZSupportMap = new Map(
        devices.map(({deviceId, supportPTZ}) => [deviceId, supportPTZ]));

    for (const {
           deviceId,
           photoResolutions,
           videoResolutions: previewResolutions,
         } of devices) {
      const previewRatios = this.groupResolutionRatio(previewResolutions);
      const captureRatios = this.groupResolutionRatio(photoResolutions);
      const pairedResolutions: CapturePreviewResolutions[] = [];
      for (const [ratio, captureResolutions] of captureRatios) {
        const previewResolutions = previewRatios.get(ratio);
        if (previewResolutions === undefined) {
          continue;
        }
        pairedResolutions.push({captureResolutions, previewResolutions});
      }

      this.deviceCapturePreviewResolutionMap.set(deviceId, pairedResolutions);
      this.supportedResolutions.set(
          deviceId,
          pairedResolutions
              .flatMap(({captureResolutions}) => captureResolutions)
              .sort((r1, r2) => r2.area - r1.area));

      let prefResolution =
          this.getPrefResolution(deviceId) ?? new Resolution(0, -1);
      const captureResolutions = this.supportedResolutions.get(deviceId);
      assert(captureResolutions !== undefined);
      if (!captureResolutions.some((r) => r.equals(prefResolution))) {
        prefResolution = captureResolutions.reduce(
            (maxR, r) => (maxR.area < r.area ? r : maxR),
            new Resolution(0, -1));
      }
      this.prefResolution.set(deviceId, prefResolution);
    }
    this.saveResolutionPreference('devicePhotoResolution');
  }

  updateValues(
      deviceId: string, _stream: MediaStream, _facing: Facing,
      resolution: Resolution): void {
    this.deviceId = deviceId;
    this.prefResolution.set(deviceId, resolution);
    this.saveResolutionPreference('devicePhotoResolution');
  }

  getSortedCandidates(deviceId: string): CaptureCandidate[] {
    const prefResolution =
        this.getPrefResolution(deviceId) ?? new Resolution(0, -1);
    const supportPTZ = this.devicePTZSupportMap.get(deviceId) ?? false;

    const toCaptureCandidate =
        ({captureResolutions,
          previewResolutions}: CapturePreviewResolutions): CaptureCandidate => {
          let captureResolution = prefResolution;
          if (!captureResolutions.some((r) => r.equals(prefResolution))) {
            captureResolution = captureResolutions.reduce(
                (captureR, r) => (r.width > captureR.width ? r : captureR));
          }

          // Use workaround for b/184089334 on PTZ camera to use preview frame
          // as photo result.
          if (supportPTZ &&
              previewResolutions.find((r) => captureResolution.equals(r)) !==
                  undefined) {
            previewResolutions = [captureResolution];
          }

          const previewCandidates =
              this.sortPreview(previewResolutions, captureResolution)
                  .map(({width, height}) => ({
                         deviceId,
                         audio: false,
                         video: {
                           width,
                           height,
                         },
                       }));
          return {resolution: captureResolution, previewCandidates};
        };

    return assertExists(this.deviceCapturePreviewResolutionMap.get(deviceId))
        .map(toCaptureCandidate)
        .sort(this.getPreferResolutionSort(prefResolution));
  }
}
