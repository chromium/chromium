// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from '../assert.js';
import * as expert from '../expert.js';
import {getBoard} from '../models/load_time_data.js';
import * as localStorage from '../models/local_storage.js';
import {
  AspectRatioSet,
  LocalStorageKey,
  Mode,
  PhotoResolutionLevel,
  Resolution,
  VideoResolutionLevel,
} from '../type.js';
import {toAspectRatioSet} from '../util.js';

import {
  Camera3DeviceInfo,
  CapturePreviewPairs,
} from './camera3_device_info.js';
import {
  CaptureCandidate,
  PhotoCaptureCandidate,
  VideoCaptureCandidate,
} from './capture_candidate.js';
import {
  CameraConfig,
  PhotoAspectRatioOptionListener,
  PhotoResolutionOption,
  PhotoResolutionOptionGroup,
  PhotoResolutionOptionListener,
  SUPPORTED_CONSTANT_FPS,
  VideoFpsOption,
  VideoResolutionOption,
  VideoResolutionOptionListener,
} from './type.js';

interface VideoLevelResolution {
  level: VideoResolutionLevel;
  resolutions: Resolution[];
}

export class CaptureCandidatePreferrer {
  /**
   * Map of camera infos with the device id as keys.
   */
  private readonly cameraInfos = new Map<string, Camera3DeviceInfo>();

  /**
   * Current camera config.
   */
  private cameraConfig: CameraConfig|null = null;

  /**
   * Map of all available photo resolutions grouped by the device id which are
   * used for aspect ratio which needs cropping.
   */
  private readonly photoOptionsForCrop =
      new Map<string, PhotoResolutionOption[]>();

  /**
   * Map of the current available photo resolutions grouped by the device id and
   * the aspect ratio set.
   */
  private readonly photoOptions =
      new Map<string, Map<AspectRatioSet, PhotoResolutionOption[]>>();

  /**
   * Map of the current available video resolutions grouped by the device id.
   */
  private readonly videoOptions = new Map<string, VideoResolutionOption[]>();

  /**
   * Object saving fps preference that each of its key as device id and value
   * as an object mapping from resolution to preferred constant fps for that
   * resolution.
   */
  private prefVideoFpsesMap:
      Record<string, Record<VideoResolutionLevel, number>> =
          localStorage.getObject(
              LocalStorageKey.PREF_DEVICE_VIDEO_RESOLUTION_FPS);

  /**
   * Map saving preference that each of its key as device id and value to be
   * preferred photo resolution level.
   */
  private prefPhotoResolutionLevelMap: Record<string, PhotoResolutionLevel> =
      localStorage.getObject(
          LocalStorageKey.PREF_DEVICE_PHOTO_RESOLUTION_LEVEL);

  /**
   * Map saving preference that each of its key as device id and
   * value to be preferred photo aspect ratio set.
   */
  private prefPhotoAspectRatioSetMap: Record<string, AspectRatioSet> =
      localStorage.getObject(
          LocalStorageKey.PREF_DEVICE_PHOTO_ASPECT_RATIO_SET);

  /**
   * Map saving prioritized photo aspect ratio order. Keys are device IDs and
   * values are the corresponding arrays of aspect ratio sets.
   */
  private prioritizedPhotoAspectRatioOrderMap:
      Record<string, AspectRatioSet[]> = {};

  /**
   * Map saving preference that each of its key as device id and value to be
   * preferred video resolution level.
   */
  private prefVideoResolutionLevelMap: Record<string, VideoResolutionLevel> =
      localStorage.getObject(
          LocalStorageKey.PREF_DEVICE_VIDEO_RESOLUTION_LEVEL);

  /**
   * Map saving preference that each of its key as device id and value to be
   * preferred photo resolution. It is used when showing all resolutions is on.
   */
  private prefPhotoResolutionMap:
      Record<string, Record<AspectRatioSet, Resolution>> =
          localStorage.getObject(
              LocalStorageKey.PREF_DEVICE_PHOTO_RESOLUTION_EXPERT);

  /**
   * Map saving preference that each of its key as device id and value to be
   * preferred video resolution. It is used when showing all resolutions is on.
   */
  private prefVideoResolutionMap: Record<string, Resolution> =
      localStorage.getObject(
          LocalStorageKey.PREF_DEVICE_VIDEO_RESOLUTION_EXPERT);

  private readonly photoResolutionOptionListeners:
      PhotoResolutionOptionListener[] = [];

  private readonly photoAspectRatioOptionListeners:
      PhotoAspectRatioOptionListener[] = [];

  private readonly videoResolutionOptionListeners:
      VideoResolutionOptionListener[] = [];

  /**
   * Adds `listener` for photo resolution options.
   */
  addPhotoResolutionOptionListener(listener: PhotoResolutionOptionListener):
      void {
    this.photoResolutionOptionListeners.push(listener);
  }

  /**
   * Adds `listener` for photo aspect ratio options.
   */
  addPhotoAspectRatioOptionListener(listener: PhotoAspectRatioOptionListener):
      void {
    this.photoAspectRatioOptionListeners.push(listener);
  }

  /**
   * Adds `listener` for video resolution options.
   */
  addVideoResolutionOptionListener(listener: VideoResolutionOptionListener):
      void {
    this.videoResolutionOptionListeners.push(listener);
  }

  /**
   * Updates the camera capabilities.
   */
  updateCapability(infos: Camera3DeviceInfo[]): void {
    this.cameraInfos.clear();
    for (const info of infos) {
      this.cameraInfos.set(info.deviceId, info);
    }
    this.buildOptions();
    this.notifyListeners();
  }

  /**
   * Called when the current camera config is updated.
   */
  onUpdateConfig(config: CameraConfig): void {
    this.cameraConfig = config;
    this.notifyListeners();
  }

  /**
   * Gets all the capture candidates sorted based on users preferences.
   */
  getSortedCandidates(
      infos: Camera3DeviceInfo[], deviceId: string, mode: Mode,
      hasAudio: boolean): CaptureCandidate[] {
    if (this.cameraInfos === null) {
      this.updateCapability(infos);
    }
    if (mode === Mode.VIDEO) {
      return this.getVideoCandidates(deviceId, hasAudio);
    } else {
      const candidates = this.getPhotoCandidates(deviceId);
      if (mode === Mode.SCAN) {
        candidates.sort(
            (c1, c2) => (c2.resolution?.mp ?? 0) - (c1.resolution?.mp ?? 0));
      }
      return candidates;
    }
  }

  /**
   * Sets photo `resolutionLevel` preference.
   */
  setPrefPhotoResolutionLevel(
      deviceId: string, resolutionLevel: PhotoResolutionLevel): void {
    this.prefPhotoResolutionLevelMap[deviceId] = resolutionLevel;
    localStorage.set(
        LocalStorageKey.PREF_DEVICE_PHOTO_RESOLUTION_LEVEL,
        this.prefPhotoResolutionLevelMap);

    // For opening camera, it will be notified after the reconfiguration.
    if (deviceId !== this.cameraConfig?.deviceId) {
      this.notifyListeners();
    }
  }

  /**
   * Sets photo `aspectRatioSet` preference.
   */
  setPrefPhotoAspectRatioSet(deviceId: string, aspectRatioSet: AspectRatioSet):
      void {
    this.prefPhotoAspectRatioSetMap[deviceId] = aspectRatioSet;
    localStorage.set(
        LocalStorageKey.PREF_DEVICE_PHOTO_ASPECT_RATIO_SET,
        this.prefPhotoAspectRatioSetMap);

    // For opening camera, it will be notified after the reconfiguration.
    if (deviceId !== this.cameraConfig?.deviceId) {
      this.notifyListeners();
    }
  }

  /**
   * Sets video `resolutionLevel` preference.
   */
  setPrefVideoResolutionLevel(
      deviceId: string, resolutionLevel: VideoResolutionLevel): void {
    this.prefVideoResolutionLevelMap[deviceId] = resolutionLevel;
    localStorage.set(
        LocalStorageKey.PREF_DEVICE_VIDEO_RESOLUTION_LEVEL,
        this.prefVideoResolutionLevelMap);

    // For opening camera, it will be notified after the reconfiguration.
    if (deviceId !== this.cameraConfig?.deviceId) {
      this.notifyListeners();
    }
  }

  /**
   * Sets video constant frame rate preference.
   */
  setPrefVideoConstFps(
      deviceId: string, resolutionLevel: VideoResolutionLevel, prefFps: number,
      shouldReconfigure: boolean): void {
    this.prefVideoFpsesMap[deviceId] =
        {...this.prefVideoFpsesMap[deviceId], [resolutionLevel]: prefFps};
    localStorage.set(
        LocalStorageKey.PREF_DEVICE_VIDEO_RESOLUTION_FPS,
        this.prefVideoFpsesMap);

    // For opening camera, it will be notified after the reconfiguration.
    if (!shouldReconfigure) {
      this.notifyListeners();
    }
  }

  /**
   * Used when showing all resolutions.
   */
  setPrefPhotoResolution(deviceId: string, resolution: Resolution): void {
    const aspectRatioSet = this.preferSquarePhoto(deviceId) ?
        AspectRatioSet.RATIO_SQUARE :
        toAspectRatioSet(resolution);
    this.setPreferPhotoResolution(deviceId, aspectRatioSet, resolution);
    localStorage.set(
        LocalStorageKey.PREF_DEVICE_PHOTO_RESOLUTION_EXPERT,
        this.prefPhotoResolutionMap);

    // For opening camera, it will be notified after the reconfiguration.
    if (deviceId !== this.cameraConfig?.deviceId) {
      this.notifyListeners();
    }
  }

  /**
   * Used when showing all resolutions.
   */
  setPrefVideoResolution(deviceId: string, resolution: Resolution): void {
    this.prefVideoResolutionMap[deviceId] = resolution;
    localStorage.set(
        LocalStorageKey.PREF_DEVICE_VIDEO_RESOLUTION_EXPERT,
        this.prefVideoResolutionMap);

    // For opening camera, it will be notified after the reconfigure.
    if (deviceId !== this.cameraConfig?.deviceId) {
      this.notifyListeners();
    }
  }

  /**
   * Builds the photo and video options according to the camera info.
   */
  buildOptions(): void {
    function extractCaptureResolutions(pairs: CapturePreviewPairs) {
      const resolutions = [];
      for (const pair of pairs) {
        resolutions.push(...pair.captureResolutions);
      }
      return resolutions;
    }

    if (this.cameraInfos === null) {
      return;
    }
    this.photoOptions.clear();
    this.photoOptionsForCrop.clear();
    this.videoOptions.clear();
    for (const [deviceId, info] of this.cameraInfos.entries()) {
      this.buildPhotoOptions(
          deviceId, extractCaptureResolutions(info.photoPreviewPairs));
      this.buildPhotoOptionsForCrop(
          deviceId, extractCaptureResolutions(info.photoPreviewPairs));
      this.buildVideoOptions(
          deviceId, extractCaptureResolutions(info.videoPreviewPairs),
          (r) => info.getConstFpses(r));
    }
  }

  /**
   * Returns whether it currently prefers square photo.
   */
  preferSquarePhoto(deviceId: string): boolean {
    return this.prefPhotoAspectRatioSetMap[deviceId] ===
        AspectRatioSet.RATIO_SQUARE;
  }

  /**
   * Returns the photo resolution level where the `resolution` belongs in the
   * current opened camera.
   */
  getPhotoResolutionLevel(resolution: Resolution): PhotoResolutionLevel {
    if (this.photoOptions.size === 0) {
      // Only fake camera will reach here.
      return PhotoResolutionLevel.UNKNOWN;
    }

    assert(this.cameraConfig !== null);
    const optionsGroups =
        this.photoOptions.get(this.cameraConfig.deviceId)?.values();
    assert(optionsGroups !== undefined);
    for (const options of optionsGroups) {
      for (const {resolutionLevel, resolutions} of options) {
        if (resolutions.some((r) => resolution.equalsWithRotation(r))) {
          return resolutionLevel;
        }
      }
    }
    assertNotReached();
  }

  /**
   * Returns the video resolution level where the `resolution` belongs in the
   * current opened camera.
   */
  getVideoResolutionLevel(resolution: Resolution): VideoResolutionLevel {
    if (this.videoOptions.size === 0) {
      // Only fake camera will reach here.
      return VideoResolutionLevel.UNKNOWN;
    }

    assert(this.cameraConfig !== null);
    const options = this.videoOptions.get(this.cameraConfig.deviceId);
    assert(options !== undefined);
    for (const {resolutionLevel, fpsOptions} of options) {
      for (const {resolutions} of fpsOptions) {
        if (resolutions.some((r) => resolution.equalsWithRotation(r))) {
          return resolutionLevel;
        }
      }
    }
    assertNotReached();
  }

  private getPhotoCandidates(deviceId: string): CaptureCandidate[] {
    const cameraInfo = this.cameraInfos.get(deviceId);
    assert(cameraInfo !== undefined);

    const candidates = [];

    const prefLevel = this.prefPhotoResolutionLevelMap[deviceId];
    const showAllResolutions =
        expert.isEnabled(expert.ExpertOption.SHOW_ALL_RESOLUTIONS);
    const prefAspectRatioSet = this.prefPhotoAspectRatioSetMap[deviceId];
    const aspectRatioOptions = this.photoOptions.get(deviceId);
    assert(aspectRatioOptions !== undefined);
    for (const [aspectRatioSet, options] of aspectRatioOptions.entries()) {
      const prefResolution =
          this.getPreferPhotoResolution(deviceId, aspectRatioSet);
      const candidatesByAspectRatio = [];
      const photoPreviewPair = cameraInfo.photoPreviewPairs.find(
          (pair) => pair.captureResolutions[0].aspectRatioEquals(
              options[0].resolutions[0]));
      assert(photoPreviewPair !== undefined);
      for (const option of options) {
        const candidatesByLevel = option.resolutions.map(
            (r) => new PhotoCaptureCandidate(
                deviceId, r, photoPreviewPair.previewResolutions,
                cameraInfo.builtinPtzSupport));
        if (showAllResolutions &&
            option.resolutions[0].equals(prefResolution)) {
          candidatesByAspectRatio.unshift(...candidatesByLevel);
        } else if (
            !showAllResolutions && option.resolutionLevel === prefLevel) {
          candidatesByAspectRatio.unshift(...candidatesByLevel);
        } else {
          candidatesByAspectRatio.push(...candidatesByLevel);
        }
      }
      if (aspectRatioSet === prefAspectRatioSet) {
        candidates.unshift(...candidatesByAspectRatio);
      } else {
        candidates.push(...candidatesByAspectRatio);
      }
    }
    return candidates;
  }

  private getVideoCandidates(deviceId: string, hasAudio: boolean):
      CaptureCandidate[] {
    const cameraInfo = this.cameraInfos.get(deviceId);
    assert(cameraInfo !== undefined);
    const candidates = [];
    const prefLevel = this.prefVideoResolutionLevelMap[deviceId];
    const prefResolution = this.prefVideoResolutionMap[deviceId] ?? null;
    const options = this.videoOptions.get(deviceId);
    const showAllResolutions =
        expert.isEnabled(expert.ExpertOption.SHOW_ALL_RESOLUTIONS);
    assert(options !== undefined);
    for (const option of options) {
      const prefFps = this.getFallbackFps(deviceId, option.resolutionLevel);
      const targetFpsCandidates = [];
      const otherFpsCandidates = [];
      const videoPreviewPair = cameraInfo.videoPreviewPairs.find(
          (pair) => pair.captureResolutions[0].aspectRatioEquals(
              option.fpsOptions[0].resolutions[0]));
      assert(videoPreviewPair !== undefined);
      const previewResolutions = videoPreviewPair.previewResolutions;
      for (const {constFps, resolutions} of option.fpsOptions) {
        for (const resolution of resolutions) {
          const candidate = new VideoCaptureCandidate(
              deviceId, resolution, previewResolutions, constFps, hasAudio);
          if (prefFps === constFps) {
            targetFpsCandidates.push(candidate);
          } else {
            otherFpsCandidates.push(candidate);
          }
        }
      }
      if (showAllResolutions &&
          option.fpsOptions.some(
              (fpsOption) => fpsOption.resolutions[0].equals(prefResolution))) {
        candidates.unshift(...otherFpsCandidates);
        candidates.unshift(...targetFpsCandidates);
      } else if (!showAllResolutions && option.resolutionLevel === prefLevel) {
        candidates.unshift(...otherFpsCandidates);
        candidates.unshift(...targetFpsCandidates);
      } else {
        candidates.push(...targetFpsCandidates);
        candidates.push(...otherFpsCandidates);
      }
    }
    return candidates;
  }

  /**
   * Splits the given `resolutions` to up to 2 groups by the 60% of the maximum
   * resolution and converts them to photo resolution options.
   */
  private createPhotoResolutionOptions(resolutions: Resolution[]):
      PhotoResolutionOption[] {
    if (resolutions.length === 0) {
      return [];
    }

    resolutions.sort((r1, r2) => r2.area - r1.area);
    const threshold = resolutions[0].area * 0.6;
    const splitIndex = resolutions.findIndex((r) => r.area < threshold);
    const options = [];
    if (splitIndex === -1) {
      options.push({
        resolutionLevel: PhotoResolutionLevel.FULL,
        resolutions,
        checked: false,
      });
    } else {
      options.push(
          {
            resolutionLevel: PhotoResolutionLevel.FULL,
            resolutions: resolutions.slice(0, splitIndex),
            checked: false,
          },
          {
            resolutionLevel: PhotoResolutionLevel.MEDIUM,
            resolutions: resolutions.slice(splitIndex),
            checked: false,
          },
      );
    }
    if (expert.isEnabled(expert.ExpertOption.SHOW_ALL_RESOLUTIONS)) {
      return options.flatMap(
          (option) =>
              option.resolutions.map((r) => ({
                                       resolutionLevel: option.resolutionLevel,
                                       resolutions: [r],
                                       checked: false,
                                     })));
    }
    return options;
  }

  private buildPhotoOptions(deviceId: string, resolutions: Resolution[]): void {
    const aspectRatioSetPreferOrder = getAspectRatioSetPreferOrder();

    // Making sure that the prefer aspect ratio has resolution which is equal to
    // or larger than 720p.
    const prioritizedAspectRatioSet =
        aspectRatioSetPreferOrder.find(
            (ratio) => resolutions.some(
                (r) => toAspectRatioSet(r) === ratio && r.height >= 720)) ??
        aspectRatioSetPreferOrder[0];
    const prioritizedAspectRatioOrder = [
      prioritizedAspectRatioSet,
      ...aspectRatioSetPreferOrder.filter(
          (ratio) => ratio !== prioritizedAspectRatioSet),
    ];
    this.prioritizedPhotoAspectRatioOrderMap[deviceId] =
        prioritizedAspectRatioOrder;

    /**
     * Categorizes the photo resolutions according to their aspect ratio and
     * sorts them.
     */
    function groupResolutions(
        resolutions: Resolution[], preferAspectRatioSetOrder: AspectRatioSet[]):
        Map<AspectRatioSet, Resolution[]> {
      const resolutionGroups = new Map<AspectRatioSet, Resolution[]>();
      for (const aspectRatioSet of preferAspectRatioSetOrder) {
        resolutionGroups.set(aspectRatioSet, []);
      }

      for (const resolution of resolutions) {
        const aspectRatioSet = toAspectRatioSet(resolution);
        resolutionGroups.get(aspectRatioSet)?.push(resolution);
      }
      return resolutionGroups;
    }

    const resolutionGroups =
        groupResolutions(resolutions, prioritizedAspectRatioOrder);
    const options = new Map<AspectRatioSet, PhotoResolutionOption[]>();
    for (const aspectRatioSet of prioritizedAspectRatioOrder) {
      const resolutionGroup = resolutionGroups.get(aspectRatioSet);
      assert(resolutionGroup !== undefined);
      if (resolutionGroup.length > 0) {
        options.set(
            aspectRatioSet, this.createPhotoResolutionOptions(resolutionGroup));
      }
      if (this.getPreferPhotoResolution(deviceId, aspectRatioSet) === null) {
        const maxResolution = resolutionGroup.reduce(
            (max, r) => r.mp > max.mp ? r : max, new Resolution());
        this.setPreferPhotoResolution(deviceId, aspectRatioSet, maxResolution);
      }
    }
    this.photoOptions.set(deviceId, options);
  }

  private buildPhotoOptionsForCrop(deviceId: string, resolutions: Resolution[]):
      void {
    if (this.getPreferPhotoResolution(deviceId, AspectRatioSet.RATIO_SQUARE) ===
        null) {
      const maxResolution = resolutions.reduce(
          (max, r) => r.mp > max.mp ? r : max, new Resolution());
      this.setPreferPhotoResolution(
          deviceId, AspectRatioSet.RATIO_SQUARE, maxResolution);
    }
    this.photoOptionsForCrop.set(
        deviceId, this.createPhotoResolutionOptions(resolutions));
  }

  private buildVideoOptions(
      deviceId: string, resolutions: Resolution[],
      getConstFpses: (resolution: Resolution) => number[]): void {
    function toVideoOptions(levelResolutions: VideoLevelResolution[]) {
      const options: VideoResolutionOption[] = [];

      for (const entry of levelResolutions) {
        const fpsMap = new Map<number|null, Resolution[]>();
        function putResolution(fps: number|null, resolution: Resolution) {
          const list = fpsMap.get(fps);
          if (list === undefined) {
            fpsMap.set(fps, [resolution]);
          } else {
            list.push(resolution);
          }
        }
        for (const resolution of entry.resolutions) {
          for (const fps of getConstFpses(resolution)
                   .filter((fps) => SUPPORTED_CONSTANT_FPS.includes(fps))) {
            putResolution(fps, resolution);
          }
          // Every resolution is a candidate of non-constant fps.
          putResolution(null, resolution);
        }
        const fpsOptions: VideoFpsOption[] = [];
        for (const [constFps, resolutions] of fpsMap.entries()) {
          fpsOptions.push({
            constFps,
            resolutions,
            checked: false,
          });
        }
        options.push({
          resolutionLevel: entry.level,
          fpsOptions,
          checked: false,
        });
      }
      return options;
    }

    const COMMON_VIDEO_OPTIONS = [
      {
        level: VideoResolutionLevel.FOUR_K,
        resolution: new Resolution(3840, 2160),
      },
      {
        level: VideoResolutionLevel.QUAD_HD,
        resolution: new Resolution(2560, 1440),
      },
      {
        level: VideoResolutionLevel.FULL_HD,
        resolution: new Resolution(1920, 1080),
      },
      {
        level: VideoResolutionLevel.HD,
        resolution: new Resolution(1280, 720),
      },
      {
        level: VideoResolutionLevel.THREE_SIXTY_P,
        resolution: new Resolution(640, 360),
      },
    ];
    resolutions.sort((r1, r2) => r2.area - r1.area);

    let matches: VideoLevelResolution[] = [];
    if (!expert.isEnabled(expert.ExpertOption.SHOW_ALL_RESOLUTIONS)) {
      for (const resolution of resolutions) {
        const option = COMMON_VIDEO_OPTIONS.find(
            (option) => option.resolution.equals(resolution));
        if (option === undefined) {
          continue;
        }
        matches.push({
          level: option.level,
          resolutions: [option.resolution],
        });
      }
    }
    if (matches.length === 0) {
      const threshold = resolutions[0].area * 0.6;
      const splitIndex = resolutions.findIndex((r) => r.area < threshold);
      if (splitIndex === -1) {
        matches.push({
          level: VideoResolutionLevel.FULL,
          resolutions,
        });
      } else {
        matches.push({
          level: VideoResolutionLevel.FULL,
          resolutions: resolutions.slice(0, splitIndex),
        });
        matches.push({
          level: VideoResolutionLevel.MEDIUM,
          resolutions: resolutions.slice(splitIndex),
        });
      }
    }

    if (expert.isEnabled(expert.ExpertOption.SHOW_ALL_RESOLUTIONS)) {
      matches =
          matches.flatMap((match) => match.resolutions.map((r) => ({
                                                             level: match.level,
                                                             resolutions: [r],
                                                           })));
    }
    this.videoOptions.set(deviceId, toVideoOptions(matches));

    if (this.prefVideoResolutionMap[deviceId] === undefined) {
      const maxResolution = resolutions.reduce(
          (max, r) => r.mp > max.mp ? r : max, new Resolution());
      this.prefVideoResolutionMap[deviceId] = maxResolution;
    }
  }

  private getChosenAspectRatio(
      deviceId: string,
      aspectRatioOptionsMap: Map<AspectRatioSet, PhotoResolutionOption[]>):
      AspectRatioSet {
    // For opening camera, select the corresponding aspect ratio for current
    // resolution if the user preference is not square. Otherwise, select
    // according to the use user preference.
    const prefAspectRatioSet = this.prefPhotoAspectRatioSetMap[deviceId];
    if (deviceId === this.cameraConfig?.deviceId &&
        this.cameraConfig?.mode !== Mode.VIDEO &&
        prefAspectRatioSet !== AspectRatioSet.RATIO_SQUARE) {
      return toAspectRatioSet(this.cameraConfig.captureCandidate.resolution);
    } else {
      return prefAspectRatioSet ??
          getFallbackAspectRatioSet(
                 aspectRatioOptionsMap,
                 this.prioritizedPhotoAspectRatioOrderMap[deviceId]);
    }
  }

  /**
   * Returns the photo resolution level preference of the given device.
   *
   * Fallback to the first resolution level if the preferred resolution level
   * doesn't exist in the option set.
   */
  private getPreferredPhotoResolutionLevel(
      deviceId: string,
      photoResoltionOptions: PhotoResolutionOption[]): PhotoResolutionLevel {
    assert(photoResoltionOptions.length > 0);
    const prefResolutionLevel =
        this.prefPhotoResolutionLevelMap[deviceId] ?? PhotoResolutionLevel.FULL;
    if (photoResoltionOptions.find(
            (option) => option.resolutionLevel === prefResolutionLevel) !==
        undefined) {
      return prefResolutionLevel;
    }
    return photoResoltionOptions[0].resolutionLevel;
  }

  private getPhotoOptionsGroup(deviceId: string): PhotoResolutionOptionGroup {
    const aspectRatioOptionsMap = this.photoOptions.get(deviceId);
    assert(aspectRatioOptionsMap !== undefined);
    const facing = this.cameraInfos.get(deviceId)?.facing;
    assert(facing !== undefined);

    const chosenAspectRatioSet =
        this.getChosenAspectRatio(deviceId, aspectRatioOptionsMap);
    const options = aspectRatioOptionsMap.get(chosenAspectRatioSet);
    assert(options !== undefined);
    const prefResolutionLevel =
        this.getPreferredPhotoResolutionLevel(deviceId, options);
    const prefResolution =
        this.getPreferPhotoResolution(deviceId, chosenAspectRatioSet);
    for (const option of options) {
      // Select the level corresponding to current resolution for opening
      // camera. Otherwise, select according to the user preference.
      if (deviceId === this.cameraConfig?.deviceId &&
          this.cameraConfig?.mode !== Mode.VIDEO) {
        const currentResolution =
            this.cameraConfig.captureCandidate?.resolution;
        assert(currentResolution !== null);
        option.checked =
            option.resolutions.some((r) => r.equals(currentResolution));
      } else {
        if (expert.isEnabled(expert.ExpertOption.SHOW_ALL_RESOLUTIONS)) {
          option.checked = option.resolutions[0].equals(prefResolution);
        } else {
          option.checked = option.resolutionLevel === prefResolutionLevel;
        }
      }
    }
    return {deviceId, facing, options};
  }

  private getPhotoOptionsGroupForCrop(deviceId: string):
      PhotoResolutionOptionGroup {
    const facing = this.cameraInfos.get(deviceId)?.facing;
    assert(facing !== undefined);

    const options = this.photoOptionsForCrop.get(deviceId);
    assert(options !== undefined);

    const prefResolutionLevel =
        this.getPreferredPhotoResolutionLevel(deviceId, options);
    const prefResolution =
        this.getPreferPhotoResolution(deviceId, AspectRatioSet.RATIO_SQUARE);
    for (const option of options) {
      if (expert.isEnabled(expert.ExpertOption.SHOW_ALL_RESOLUTIONS)) {
        option.checked = option.resolutions[0].equals(prefResolution);
      } else {
        option.checked = option.resolutionLevel === prefResolutionLevel;
      }
    }
    return {deviceId, facing, options};
  }

  /**
   * Notifies listeners for the new options changes according to the built
   * options and the current camera config.
   */
  private notifyListeners(): void {
    this.notifyPhotoResolutionListeners();
    this.notifyPhotoAspectRatioListeners();
    this.notifyVideoResolutionListeners();
  }

  private notifyPhotoResolutionListeners(): void {
    const groups = [];
    for (const deviceId of this.photoOptions.keys()) {
      if (this.prefPhotoAspectRatioSetMap[deviceId] ===
          AspectRatioSet.RATIO_SQUARE) {
        groups.push(this.getPhotoOptionsGroupForCrop(deviceId));
      } else {
        groups.push(this.getPhotoOptionsGroup(deviceId));
      }
    }
    for (const listener of this.photoResolutionOptionListeners) {
      listener(groups);
    }
  }

  private notifyPhotoAspectRatioListeners(): void {
    const groups = [];
    for (const [deviceId, aspectRatioOptionsMap] of this.photoOptions
             .entries()) {
      const facing = this.cameraInfos.get(deviceId)?.facing;
      assert(facing !== undefined);

      const chosenAspectRatioSet =
          this.getChosenAspectRatio(deviceId, aspectRatioOptionsMap);
      const options = [];
      // Always put a "Square" option in the aspect ratio options.
      for (const aspectRatioSet
               of [...aspectRatioOptionsMap.keys(),
                   AspectRatioSet.RATIO_SQUARE]) {
        options.push({
          aspectRatioSet,
          checked: aspectRatioSet === chosenAspectRatioSet,
        });
      }
      groups.push({deviceId, facing, options});
    }
    for (const listener of this.photoAspectRatioOptionListeners) {
      listener(groups);
    }
  }

  private notifyVideoResolutionListeners(): void {
    const groups = [];
    for (const [deviceId, options] of this.videoOptions.entries()) {
      const facing = this.cameraInfos.get(deviceId)?.facing;
      assert(facing !== undefined);

      const prefLevel = this.prefVideoResolutionLevelMap[deviceId] ??
          getFallbackVideoResolutionLevel(options);
      const prefResolution = this.prefVideoResolutionMap[deviceId] ?? null;
      for (const option of options) {
        if (this.cameraConfig === null) {
          continue;
        }
        const prefFps = this.getFallbackFps(deviceId, option.resolutionLevel);
        const captureCandidate = this.cameraConfig.captureCandidate;
        const configuredResolution = captureCandidate?.resolution;
        const isRunningCameraOption = deviceId === this.cameraConfig.deviceId &&
            configuredResolution !== null &&
            option.fpsOptions.some(
                (fpsOption) => fpsOption.resolutions.some(
                    (r) => r.equals(configuredResolution)));
        // Select the level corresponding to current resolution for opening
        // camera. Otherwise, select according to the use user preference.
        if (deviceId === this.cameraConfig.deviceId &&
            this.cameraConfig?.mode === Mode.VIDEO) {
          option.checked = isRunningCameraOption;
        } else {
          if (expert.isEnabled(expert.ExpertOption.SHOW_ALL_RESOLUTIONS)) {
            option.checked = option.fpsOptions.some(
                (fpsOption) => fpsOption.resolutions[0].equals(prefResolution));
          } else {
            option.checked = option.resolutionLevel === prefLevel;
          }
        }
        for (const fpsOption of option.fpsOptions) {
          if (isRunningCameraOption) {
            fpsOption.checked =
                fpsOption.constFps === captureCandidate.getConstFps();
          } else {
            fpsOption.checked = fpsOption.constFps === prefFps;
          }
        }
      }
      groups.push({deviceId, facing, options});
    }
    for (const listener of this.videoResolutionOptionListeners) {
      listener(groups);
    }
  }

  private getFallbackFps(deviceId: string, level: VideoResolutionLevel):
      number {
    return this.prefVideoFpsesMap[deviceId]?.[level] ?? 30;
  }

  private getPreferPhotoResolution(
      deviceId: string, aspectRatioSet: AspectRatioSet): Resolution|null {
    const map = this.prefPhotoResolutionMap[deviceId];
    if (map === undefined) {
      return null;
    }

    const entry = map[aspectRatioSet];
    return entry !== undefined ? new Resolution(entry.width, entry.height) :
                                 null;
  }

  private setPreferPhotoResolution(
      deviceId: string, aspectRatioSet: AspectRatioSet,
      resolution: Resolution): void {
    this.prefPhotoResolutionMap[deviceId] = {
      ...this.prefPhotoResolutionMap[deviceId],
      [aspectRatioSet]: resolution,
    };
  }
}

function getFallbackAspectRatioSet(
    aspectRatioOptionsMap: Map<AspectRatioSet, PhotoResolutionOption[]>,
    preferAspectRatioSetOrder: AspectRatioSet[]): AspectRatioSet {
  for (const aspectRatioSet of preferAspectRatioSetOrder) {
    if (aspectRatioOptionsMap.has(aspectRatioSet)) {
      return aspectRatioSet;
    }
  }
  assertNotReached();
}

function getFallbackVideoResolutionLevel(options: VideoResolutionOption[]):
    VideoResolutionLevel {
  const preferenceOrder = [
    VideoResolutionLevel.FOUR_K,
    VideoResolutionLevel.QUAD_HD,
    VideoResolutionLevel.FULL_HD,
    VideoResolutionLevel.HD,
    VideoResolutionLevel.THREE_SIXTY_P,
    VideoResolutionLevel.FULL,
    VideoResolutionLevel.MEDIUM,
  ];
  for (const level of preferenceOrder) {
    if (options.some((option) => option.resolutionLevel === level)) {
      return level;
    }
  }
  assertNotReached();
}

function getAspectRatioSetPreferOrder() {
  const board = getBoard();
  switch (board) {
    case 'rex':
      return [
        AspectRatioSet.RATIO_16_9,
        AspectRatioSet.RATIO_4_3,
        AspectRatioSet.RATIO_OTHER,
      ];
    default:
      return [
        AspectRatioSet.RATIO_4_3,
        AspectRatioSet.RATIO_16_9,
        AspectRatioSet.RATIO_OTHER,
      ];
  }
}
