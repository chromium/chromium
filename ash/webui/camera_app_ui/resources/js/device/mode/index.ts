// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  assert,
  assertInstanceof,
} from '../../assert.js';
import {DeviceOperator} from '../../mojo/device_operator.js';
import {CaptureIntent} from '../../mojo/type.js';
import * as state from '../../state.js';
import {
  Facing,
  Mode,
  Resolution,
} from '../../type.js';
import {assertEnumVariant} from '../../util.js';
import {
  CaptureCandidate,
  ConstraintsPreferrer,
  PhotoConstraintsPreferrer,
  VideoConstraintsPreferrer,
} from '../constraints_preferrer.js';
import {StreamConstraints} from '../stream_constraints.js';

import {
  ModeBase,
  ModeFactory,
} from './mode_base.js';
import {
  PhotoFactory,
  PhotoHandler,
} from './photo.js';
import {PortraitFactory, PortraitHandler} from './portrait.js';
import {
  ScanFactory,
  ScanHandler,
} from './scan.js';
import {SquareFactory} from './square.js';
import {
  VideoFactory,
  VideoHandler,
} from './video.js';

export {PhotoHandler, PhotoResult} from './photo.js';
export {getDefaultScanCorners, ScanHandler} from './scan.js';
export {
  GifResult,
  setAvc1Parameters,
  Video,
  VideoHandler,
  VideoResult,
} from './video.js';

/**
 * Callback to trigger mode switching. Should return whether mode switching
 * succeed.
 */
export type DoSwitchMode = () => Promise<boolean>;

export type CaptureHandler =
    PhotoHandler&VideoHandler&PortraitHandler&ScanHandler;

/**
 * Parameters for capture settings.
 */
interface CaptureParams {
  mode: Mode;
  constraints: StreamConstraints;
  captureResolution: Resolution;
  videoSnapshotResolution: Resolution;
}

/**
 * The abstract interface for the mode configuration.
 */
interface ModeConfig {
  /**
   * @return Resolves to boolean indicating whether the mode is supported by
   *     video device with specified device id.
   */
  isSupported(deviceId: string|null): Promise<boolean>;

  isSupportPTZ(captureResolution: Resolution, previewResolution: Resolution):
      boolean;

  /**
   * Makes video capture device prepared for capturing in this mode.
   * @param constraints Constraints for preview stream.
   */
  prepareDevice(constraints: StreamConstraints, captureResolution: Resolution):
      Promise<void>;

  /**
   * Get general stream constraints of this mode for fake cameras.
   */
  getConstraintsForFakeCamera(deviceId: string|null): StreamConstraints[];

  /**
   * Gets factory to create capture object for this mode.
   */
  getCaptureFactory(): ModeFactory;

  /**
   * HALv3 constraints preferrer for this mode.
   */
  readonly constraintsPreferrer: ConstraintsPreferrer;

  /**
   * Mode to be fallbacked to when fail to configure this mode.
   */
  readonly fallbackMode: Mode;
}

/**
 * Mode controller managing capture sequence of different camera mode.
 */
export class Modes {
  /**
   * Capture controller of current camera mode.
   */
  current: ModeBase|null = null;
  /**
   * Parameters to create mode capture controller.
   */
  private captureParams: CaptureParams|null = null;
  /**
   * Mode classname and related functions and attributes.
   */
  private readonly allModes: {[mode in Mode]: ModeConfig};

  private handler: CaptureHandler|null = null;

  constructor(
      photoPreferrer: PhotoConstraintsPreferrer,
      videoPreferrer: VideoConstraintsPreferrer,
  ) {
    /**
     * Returns a set of general constraints for fake cameras.
     * @param videoMode Is getting constraints for video mode.
     * @param deviceId Id of video device.
     * @return Result of constraints-candidates.
     */
    const getConstraintsForFakeCamera = function(
        videoMode: boolean, deviceId: string): StreamConstraints[] {
      const frameRate = {min: 20, ideal: 30};
      return [
        {
          deviceId,
          audio: videoMode,
          video: {
            aspectRatio: {ideal: videoMode ? 1.7777777778 : 1.3333333333},
            width: {min: 1280},
            frameRate,
          },
        },
        {
          deviceId,
          audio: videoMode,
          video: {
            width: {min: 640},
            frameRate,
          },
        },
      ];
    };

    // Workaround for b/184089334 on PTZ camera to use preview frame as photo
    // result.
    const checkSupportPTZForPhotoMode =
        (captureResolution: Resolution, previewResolution: Resolution) =>
            captureResolution.equals(previewResolution);

    // clang-format format this wrong if we use async (...) => {...} (missing a
    // space after async). Using async function instead to get around this.
    // TODO(pihsun): style guide recommends using function xxx() instead of
    // lambda anyway, change other location too.
    async function prepareDeviceForPhoto(
        constraints: StreamConstraints, resolution: Resolution,
        captureIntent: CaptureIntent): Promise<void> {
      const deviceOperator = await DeviceOperator.getInstance();
      if (deviceOperator === null) {
        return;
      }
      const deviceId = constraints.deviceId;
      await deviceOperator.setCaptureIntent(deviceId, captureIntent);
      await deviceOperator.setStillCaptureResolution(deviceId, resolution);
    }

    this.allModes = {
      [Mode.VIDEO]: {
        getCaptureFactory: () => {
          const params = this.getCaptureParams();
          return new VideoFactory(
              params.constraints, params.captureResolution,
              params.videoSnapshotResolution, this.handler);
        },
        isSupported: async () => true,
        isSupportPTZ: () => true,
        prepareDevice: async (constraints) => {
          const deviceOperator = await DeviceOperator.getInstance();
          if (deviceOperator === null) {
            return;
          }
          const deviceId = constraints.deviceId;
          await deviceOperator.setCaptureIntent(
              deviceId, CaptureIntent.VIDEO_RECORD);
          if (await deviceOperator.isBlobVideoSnapshotEnabled(deviceId)) {
            await deviceOperator.setStillCaptureResolution(
                deviceId, this.getCaptureParams().videoSnapshotResolution);
          }

          let minFrameRate = 0;
          let maxFrameRate = 0;
          if (constraints.video && constraints.video.frameRate) {
            const frameRate = constraints.video.frameRate;
            if (typeof frameRate === 'number') {
              minFrameRate = frameRate;
              maxFrameRate = frameRate;
            } else if (frameRate.exact) {
              minFrameRate = frameRate.exact;
              maxFrameRate = frameRate.exact;
            } else if (frameRate.min && frameRate.max) {
              minFrameRate = frameRate.min;
              maxFrameRate = frameRate.max;
            }
            // TODO(wtlee): To set the fps range to the default value, we should
            // remove the frameRate from constraints instead of using incomplete
            // range.
          }
          await deviceOperator.setFpsRange(
              deviceId, minFrameRate, maxFrameRate);
        },
        constraintsPreferrer: videoPreferrer,
        getConstraintsForFakeCamera:
            getConstraintsForFakeCamera.bind(this, true),
        fallbackMode: Mode.PHOTO,
      },
      [Mode.PHOTO]: {
        getCaptureFactory: () => {
          const params = this.getCaptureParams();
          return new PhotoFactory(
              params.constraints, params.captureResolution, this.handler);
        },
        isSupported: async () => true,
        isSupportPTZ: checkSupportPTZForPhotoMode,
        prepareDevice: async (constraints, resolution) => prepareDeviceForPhoto(
            constraints, resolution, CaptureIntent.STILL_CAPTURE),
        constraintsPreferrer: photoPreferrer,
        getConstraintsForFakeCamera:
            getConstraintsForFakeCamera.bind(this, false),
        fallbackMode: Mode.SQUARE,
      },
      [Mode.SQUARE]: {
        getCaptureFactory: () => {
          const params = this.getCaptureParams();
          return new SquareFactory(
              params.constraints, params.captureResolution, this.handler);
        },
        isSupported: async () => true,
        isSupportPTZ: checkSupportPTZForPhotoMode,
        prepareDevice: async (constraints, resolution) => prepareDeviceForPhoto(
            constraints, resolution, CaptureIntent.STILL_CAPTURE),
        constraintsPreferrer: photoPreferrer,
        getConstraintsForFakeCamera:
            getConstraintsForFakeCamera.bind(this, false),
        fallbackMode: Mode.PHOTO,
      },
      [Mode.PORTRAIT]: {
        getCaptureFactory: () => {
          const params = this.getCaptureParams();
          return new PortraitFactory(
              params.constraints, params.captureResolution, this.handler);
        },
        isSupported: async (deviceId) => {
          if (deviceId === null) {
            return false;
          }
          const deviceOperator = await DeviceOperator.getInstance();
          if (deviceOperator === null) {
            return false;
          }
          return await deviceOperator.isPortraitModeSupported(deviceId);
        },
        isSupportPTZ: checkSupportPTZForPhotoMode,
        prepareDevice: async (constraints, resolution) => prepareDeviceForPhoto(
            constraints, resolution, CaptureIntent.STILL_CAPTURE),
        constraintsPreferrer: photoPreferrer,
        getConstraintsForFakeCamera:
            getConstraintsForFakeCamera.bind(this, false),
        fallbackMode: Mode.PHOTO,
      },
      [Mode.SCAN]: {
        getCaptureFactory: () => {
          const params = this.getCaptureParams();
          return new ScanFactory(
              params.constraints, params.captureResolution, this.handler);
        },
        isSupported: async () => state.get(state.State.SHOW_SCAN_MODE),
        isSupportPTZ: checkSupportPTZForPhotoMode,
        prepareDevice: async (constraints, resolution) => prepareDeviceForPhoto(
            constraints, resolution, CaptureIntent.STILL_CAPTURE),
        constraintsPreferrer: photoPreferrer,
        getConstraintsForFakeCamera:
            getConstraintsForFakeCamera.bind(this, false),
        fallbackMode: Mode.PHOTO,
      },
    };

    [state.State.EXPERT, state.State.SAVE_METADATA].forEach((s) => {
      state.addObserver(s, () => {
        this.updateSaveMetadata();
      });
    });
  }

  initialize(handler: CaptureHandler): void {
    this.handler = handler;
  }

  private getCaptureParams(): CaptureParams {
    assert(this.captureParams !== null);
    return this.captureParams;
  }

  /**
   * Gets current mode. Should only be called when mode is properly configured.
   */
  getMode(): Mode {
    return assertEnumVariant(Mode, this.captureParams?.mode);
  }

  getCaptureResolution(): Resolution {
    return assertInstanceof(this.captureParams?.captureResolution, Resolution);
  }

  /**
   * @param deviceId
   * @param startingMode
   * @return Supported mode candidates for specific |deviceId| starting from
   *     |startingMode| followed by its fallback modes.
   */
  async getModeCandidates(deviceId: string|null, startingMode: Mode):
      Promise<Mode[]> {
    const tried = new Set<Mode>();
    const results: Mode[] = [];
    let mode = startingMode;
    while (!tried.has(mode)) {
      tried.add(mode);
      if (await this.isSupported(mode, deviceId)) {
        results.push(mode);
      }
      mode = this.allModes[mode].fallbackMode;
    }
    return results;
  }

  /**
   * Gets all available capture resolution and its corresponding preview
   * constraints for the given |mode| and |deviceId|.
   */
  getResolutionCandidates(mode: Mode, deviceId: string): CaptureCandidate[] {
    return this.allModes[mode].constraintsPreferrer.getSortedCandidates(
        deviceId);
  }

  /**
   * Gets a general set of resolution candidates given by |mode| and |deviceId|
   * for fake cameras.
   */
  getFakeResolutionCandidates(mode: Mode, deviceId: string):
      CaptureCandidate[] {
    const previewCandidates =
        this.allModes[mode].getConstraintsForFakeCamera(deviceId);
    return [{resolution: null, previewCandidates}];
  }

  /**
   * Gets factory to create mode capture object.
   */
  getModeFactory(mode: Mode): ModeFactory {
    return this.allModes[mode].getCaptureFactory();
  }

  /**
   * @param constraints Constraints for preview stream.
   */
  setCaptureParams(
      mode: Mode, constraints: StreamConstraints, captureResolution: Resolution,
      videoSnapshotResolution: Resolution): void {
    this.captureParams =
        {mode, constraints, captureResolution, videoSnapshotResolution};
  }

  /**
   * Makes video capture device prepared for capturing in this mode.
   */
  async prepareDevice(): Promise<void> {
    if (state.get(state.State.USE_FAKE_CAMERA)) {
      return;
    }
    const {mode, captureResolution, constraints} = this.getCaptureParams();
    return this.allModes[mode].prepareDevice(
        constraints, assertInstanceof(captureResolution, Resolution));
  }

  async isSupported(mode: Mode, deviceId: string|null): Promise<boolean> {
    return this.allModes[mode].isSupported(deviceId);
  }

  isSupportPTZ(
      mode: Mode, captureResolution: Resolution,
      previewResolution: Resolution): boolean {
    return this.allModes[mode].isSupportPTZ(
        captureResolution, previewResolution);
  }

  /**
   * Creates and updates current mode object.
   * @param factory The factory ready for producing mode capture object.
   * @param stream Stream of the new switching mode.
   * @param facing Camera facing of the current mode.
   * @param deviceId Device id of currently working video device.
   */
  async updateMode(
      factory: ModeFactory, stream: MediaStream, facing: Facing,
      deviceId: string|null): Promise<void> {
    if (this.current !== null) {
      await this.current.clear();
      await this.disableSaveMetadata();
    }
    const {mode, captureResolution} = this.getCaptureParams();
    this.current = factory.produce();
    if (deviceId && captureResolution) {
      this.allModes[mode].constraintsPreferrer.updateValues(
          deviceId, stream, facing, captureResolution);
    }
    await this.updateSaveMetadata();
  }

  /**
   * Clears everything when mode is not needed anymore.
   */
  async clear(): Promise<void> {
    if (this.current !== null) {
      await this.current.clear();
      await this.disableSaveMetadata();
    }
    this.captureParams = null;
    this.current = null;
  }

  /**
   * Checks whether to save image metadata or not.
   * @return Promise for the operation.
   */
  private async updateSaveMetadata(): Promise<void> {
    if (state.get(state.State.EXPERT) && state.get(state.State.SAVE_METADATA)) {
      await this.enableSaveMetadata();
    } else {
      await this.disableSaveMetadata();
    }
  }

  /**
   * Enables save metadata of subsequent photos in the current mode.
   * @return Promise for the operation.
   */
  private async enableSaveMetadata(): Promise<void> {
    if (this.current !== null) {
      await this.current.addMetadataObserver();
    }
  }

  /**
   * Disables save metadata of subsequent photos in the current mode.
   * @return Promise for the operation.
   */
  private async disableSaveMetadata(): Promise<void> {
    if (this.current !== null) {
      await this.current.removeMetadataObserver();
    }
  }
}
