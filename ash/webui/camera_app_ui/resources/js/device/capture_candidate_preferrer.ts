// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertInstanceof} from '../assert.js';
import * as localStorage from '../models/local_storage.js';
import * as state from '../state.js';
import {Mode, Resolution} from '../type.js';

import {Camera3DeviceInfo} from './camera3_device_info.js';
import {
  CaptureCandidate,
  MultiStreamVideoCaptureCandidate,
  PhotoCaptureCandidate,
  VideoCaptureCandidate,
} from './capture_candidate.js';

export interface CaptureCandidatePreferrer {
  updateCapability(camera3DevicesInfo: Camera3DeviceInfo[]): void;
  getPrefPhotoResolution(deviceId: string): Resolution|null;
  setPrefPhotoResolution(deviceId: string, r: Resolution): void;
  getPrefVideoResolution(deviceId: string): Resolution|null;
  setPrefVideoResolution(deviceId: string, r: Resolution): void;
  getPrefVideoConstFps(deviceId: string, r: Resolution): number|null;
  setPrefVideoConstFps(deviceId: string, r: Resolution, fps: number): void;
  getSortedCandidates(camera3Info: Camera3DeviceInfo, mode: Mode):
      CaptureCandidate[];
}

const PREF_DEVICE_PHOTO_RESOLUTION_KEY = 'deviceVideoResolution';
const PREF_DEVICE_VIDEO_RESOLUTION_KEY = 'deviceVideoResolution';
const PREF_DEVICE_VIDEO_FPS_KEY = 'deviceVideoFps';

/**
 * Restores saved preferred capture resolution per video device.
 *
 * @param key Key of local storage saving preferences.
 */
function restoreResolutionPreference(key: string): Map<string, Resolution> {
  const preference =
      localStorage.getObject<{width: number, height: number}>(key);
  const prefResolution = new Map();
  for (const [deviceId, {width, height}] of Object.entries(preference)) {
    prefResolution.set(deviceId, new Resolution(width, height));
  }
  return prefResolution;
}

function getLargestResolution(resolutions: Resolution[]): Resolution {
  const first = assertInstanceof(resolutions[0], Resolution);
  return resolutions.reduce(
      (maxR, r) => (maxR.area < r.area ? r : maxR), first);
}

function compareWithPrefResolution(
    prefResolution: Resolution, r1: Resolution, r2: Resolution): number {
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
}

export class DefaultPreferrer implements CaptureCandidatePreferrer {
  /**
   * Object saving fps preference that each of its key as device id and value
   * as an object mapping from resolution to preferred constant fps for that
   * resolution.
   */
  private prefFpses: Record<string, Record<string, number>> =
      localStorage.getObject(PREF_DEVICE_VIDEO_FPS_KEY);

  /**
   * Map saving resolution preference that each of its key as device id and
   * value to be preferred width, height of resolution of that video device.
   */
  protected prefDevicePhotoResolution: Map<string, Resolution> =
      restoreResolutionPreference(PREF_DEVICE_PHOTO_RESOLUTION_KEY);

  /**
   * Map saving resolution preference that each of its key as device id and
   * value to be preferred width, height of resolution of that video device.
   */
  protected prefDeviceVideoResolution: Map<string, Resolution> =
      restoreResolutionPreference(PREF_DEVICE_VIDEO_RESOLUTION_KEY);

  private sortPhotoCandidates(
      deviceId: string,
      candidates: PhotoCaptureCandidate[]): PhotoCaptureCandidate[] {
    const prefResolution =
        this.prefDevicePhotoResolution.get(deviceId) ?? new Resolution();
    return [...candidates].sort(
        ({resolution: r1}, {resolution: r2}) => r1.equals(r2) ?
            0 :
            compareWithPrefResolution(prefResolution, r1, r2));
  }

  private sortVideoCandidates(
      deviceId: string,
      candidates: VideoCaptureCandidate[]): VideoCaptureCandidate[] {
    const prefResolution =
        this.prefDeviceVideoResolution.get(deviceId) ?? new Resolution();
    const cmp =
        (c: VideoCaptureCandidate, c2: VideoCaptureCandidate): number => {
          const r1 = c.resolution;
          const r2 = c2.resolution;
          if (r1.equals(r2)) {
            const prefFps = this.prefFpses[deviceId]?.[r1.toString()];
            if (c.constFps === prefFps) {
              return -1;
            }
            if (c2.constFps === prefFps) {
              return 1;
            }
            return (c2.constFps ?? -Infinity) - (c.constFps ?? -Infinity);
          }
          return compareWithPrefResolution(prefResolution, r1, r2);
        };
    return [...candidates].sort(cmp);
  }

  private getVideoCandidates(info: Camera3DeviceInfo): VideoCaptureCandidate[] {
    const enableMultiStreamRecording =
        state.get(state.State.ENABLE_MULTISTREAM_RECORDING);

    const prefResolution = this.prefDeviceVideoResolution.get(info.deviceId);
    const ret: VideoCaptureCandidate[] = [];
    // For every aspect ratio, try one either the resolution in the preference
    // set or the largest.
    for (const {captureResolutions, previewResolutions} of info
             .videoPreviewPairs) {
      const captureR =
          prefResolution?.aspectRatioEquals(captureResolutions[0]) ?
          prefResolution :
          getLargestResolution(captureResolutions);
      let constFpses: Array<number|null> = info.getConstFpses(captureR);

      // The higher constant fps will be ignored if constant 30 and 60
      // presented due to currently lack of UI support for toggling it.
      // TODO(b/215484798): Remove the logic here and show all constant fps
      // in new UI.
      if (constFpses.includes(30) && constFpses.includes(60)) {
        constFpses = [30, 60];
      } else {
        constFpses.push(null);
      }
      for (const constFps of constFpses) {
        if (enableMultiStreamRecording) {
          ret.push(new MultiStreamVideoCaptureCandidate(
              info.deviceId, captureR, previewResolutions, constFps));
        } else {
          ret.push(new VideoCaptureCandidate(
              info.deviceId, captureR, previewResolutions, constFps));
        }
      }
    }
    return ret;
  }

  private getPhotoCandidates(info: Camera3DeviceInfo): PhotoCaptureCandidate[] {
    const prefResolution = this.prefDevicePhotoResolution.get(info.deviceId);
    // For every aspect ratio, try one either the resolution in the preference
    // set or the largest.
    const ret: PhotoCaptureCandidate[] = [];
    for (const {captureResolutions, previewResolutions} of info
             .photoPreviewPairs) {
      const captureR =
          prefResolution?.aspectRatioEquals(captureResolutions[0]) ?
          prefResolution :
          getLargestResolution(captureResolutions);
      ret.push(new PhotoCaptureCandidate(
          info.deviceId, captureR, previewResolutions, info.supportPTZ));
    }
    return ret;
  }

  private updateDevicePhotoPreference(info: Camera3DeviceInfo): void {
    let prefResolution =
        this.prefDevicePhotoResolution.get(info.deviceId) ?? new Resolution();
    prefResolution =
        info.photoResolutions.find((r) => r.equals(prefResolution)) ??
        getLargestResolution(info.photoResolutions);
    this.prefDevicePhotoResolution.set(info.deviceId, prefResolution);
  }

  private updateDeviceVideoPreference(info: Camera3DeviceInfo): void {
    const prefResolution = (() => {
      function findResolution(width: number, height: number) {
        return info.videoResolutions.find(
            (r) => r.width === width && r.height === height);
      }

      const {width = 0, height = -1} =
          this.prefDeviceVideoResolution.get(info.deviceId) ?? {};
      return findResolution(width, height) ?? findResolution(1920, 1080) ??
          findResolution(1280, 720) ??
          getLargestResolution(info.videoResolutions);
    })();
    this.prefDeviceVideoResolution.set(info.deviceId, prefResolution);
  }

  updateCapability(camera3DevicesInfo: Camera3DeviceInfo[]): void {
    for (const info of camera3DevicesInfo) {
      this.updateDevicePhotoPreference(info);
      this.updateDeviceVideoPreference(info);
    }
    this.savePrefPhotoResolution();
    this.savePrefVideoResolution();
  }

  getPrefPhotoResolution(deviceId: string): Resolution|null {
    return this.prefDevicePhotoResolution.get(deviceId) ?? null;
  }

  private savePrefPhotoResolution(): void {
    localStorage.set(
        PREF_DEVICE_PHOTO_RESOLUTION_KEY,
        Object.fromEntries(this.prefDevicePhotoResolution));
  }

  setPrefPhotoResolution(deviceId: string, r: Resolution): void {
    this.prefDevicePhotoResolution.set(deviceId, r);
    this.savePrefPhotoResolution();
  }

  getPrefVideoResolution(deviceId: string): Resolution|null {
    return this.prefDeviceVideoResolution.get(deviceId) ?? null;
  }

  private savePrefVideoResolution(): void {
    localStorage.set(
        PREF_DEVICE_VIDEO_RESOLUTION_KEY,
        Object.fromEntries(this.prefDeviceVideoResolution));
  }

  setPrefVideoResolution(deviceId: string, r: Resolution): void {
    this.prefDeviceVideoResolution.set(deviceId, r);
    this.savePrefVideoResolution();
  }

  getPrefVideoConstFps(deviceId: string, resolution: Resolution): number|null {
    return this.prefFpses?.[deviceId]?.[resolution.toString()] ?? null;
  }

  setPrefVideoConstFps(deviceId: string, r: Resolution, prefFps: number): void {
    this.prefFpses[deviceId] = Object.assign(
        this.prefFpses[deviceId] ?? {}, {[r.toString()]: prefFps});
    localStorage.set(PREF_DEVICE_VIDEO_FPS_KEY, this.prefFpses);
  }

  getSortedCandidates(camera3Info: Camera3DeviceInfo, mode: Mode):
      CaptureCandidate[] {
    if (mode === Mode.VIDEO) {
      return this.sortVideoCandidates(
          camera3Info.deviceId, this.getVideoCandidates(camera3Info));
    } else {
      return this.sortPhotoCandidates(
          camera3Info.deviceId, this.getPhotoCandidates(camera3Info));
    }
  }
}
