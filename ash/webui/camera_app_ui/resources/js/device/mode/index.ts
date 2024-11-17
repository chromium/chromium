// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  assert,
  assertExists,
  assertInstanceof,
} from '../../assert.js';
import * as expert from '../../expert.js';
import {DeviceOperator} from '../../mojo/device_operator.js';
import {CaptureIntent} from '../../mojo/type.js';
import * as state from '../../state.js';
import {
  Mode,
  Resolution,
} from '../../type.js';
import {getFpsRangeFromConstraints} from '../../util.js';
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
import {
  VideoFactory,
  VideoHandler,
} from './video.js';

export type{PhotoHandler, PhotoResult} from './photo.js';
export {getDefaultScanCorners} from './scan.js';
export type{ScanHandler} from './scan.js';
export {setAvc1Parameters, Video} from './video.js';
export type{GifResult, VideoHandler, VideoResult} from './video.js';

/**
 * Callback to trigger mode switching. Should return whether mode switching
 * succeed.
 */
export type DoSwitchMode = () => Promise<boolean>;

export type CaptureHandler =
    PhotoHandler&PortraitHandler&ScanHandler&VideoHandler;

/**
 * Parameters for capture settings.
 */
interface CaptureParams {
  mode: Mode;
  constraints: StreamConstraints;
  captureResolution: Resolution|null;
  videoSnapshotResolution: Resolution|null;
}

/**
 * The abstract interface for the mode configuration.
 */
interface ModeConfig {
  /**
   * @return Resolves to boolean indicating whether the mode is supported by
   *     video device with specified `deviceId`.
   */
  isSupported(deviceId: string|null): Promise<boolean>;

  isSupportPtz(captureResolution: Resolution, previewResolution: Resolution):
      boolean;

  /**
   * Makes video capture device prepared for capturing in this mode.
   *
   * @param constraints Constraints for preview stream.
   */
  prepareDevice(constraints: StreamConstraints, captureResolution: Resolution):
      Promise<void>;

  /**
   * Gets factory to create capture object for this mode.
   */
  getCaptureFactory(): ModeFactory;

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

  constructor() {
    // Workaround for b/184089334 on PTZ camera to use preview frame as photo
    // result.
    function checkSupportPtzForPhotoMode(
        captureResolution: Resolution, previewResolution: Resolution) {
      return captureResolution.equals(previewResolution);
    }

    /**
     * Prepares the device for the specific `resolution` and `captureIntent`.
     */
    async function prepareDeviceForPhoto(
        constraints: StreamConstraints, resolution: Resolution,
        captureIntent: CaptureIntent): Promise<void> {
      const deviceOperator = DeviceOperator.getInstance();
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
              params.videoSnapshotResolution, assertExists(this.handler));
        },
        isSupported: () => Promise.resolve(true),
        isSupportPtz: () => true,
        prepareDevice: async (constraints) => {
          const deviceOperator = DeviceOperator.getInstance();
          if (deviceOperator === null) {
            return;
          }
          const deviceId = constraints.deviceId;
          await deviceOperator.setCaptureIntent(
              deviceId, CaptureIntent.kVideoRecord);

          if (await deviceOperator.isBlobVideoSnapshotEnabled(deviceId)) {
            await deviceOperator.setStillCaptureResolution(
                deviceId,
                assertExists(this.getCaptureParams().videoSnapshotResolution));
          }

          // TODO(wtlee): To set the fps range to the default value, we should
          // remove the frameRate from constraints instead of using incomplete
          // range.
          const {minFps, maxFps} =
              getFpsRangeFromConstraints(constraints.video?.frameRate);
          await deviceOperator.setFpsRange(deviceId, minFps, maxFps);
        },
        fallbackMode: Mode.PHOTO,
      },
      [Mode.PHOTO]: {
        getCaptureFactory: () => {
          const params = this.getCaptureParams();
          return new PhotoFactory(
              params.constraints, params.captureResolution,
              assertExists(this.handler));
        },
        isSupported: () => Promise.resolve(true),
        isSupportPtz: checkSupportPtzForPhotoMode,
        prepareDevice: async (constraints, resolution) => prepareDeviceForPhoto(
            constraints, resolution, CaptureIntent.kStillCapture),
        fallbackMode: Mode.SCAN,
      },
      [Mode.PORTRAIT]: {
        getCaptureFactory: () => {
          const params = this.getCaptureParams();
          return new PortraitFactory(
              params.constraints, params.captureResolution,
              assertExists(this.handler));
        },
        isSupported: async (deviceId) => {
          if (deviceId === null) {
            return false;
          }
          const deviceOperator = DeviceOperator.getInstance();
          if (deviceOperator === null) {
            return false;
          }
          return deviceOperator.isPortraitModeSupported(deviceId);
        },
        isSupportPtz: checkSupportPtzForPhotoMode,
        prepareDevice: async (constraints, resolution) => prepareDeviceForPhoto(
            constraints, resolution, CaptureIntent.kPortraitCapture),
        fallbackMode: Mode.PHOTO,
      },
      [Mode.SCAN]: {
        getCaptureFactory: () => {
          const params = this.getCaptureParams();
          return new ScanFactory(
              params.constraints, params.captureResolution,
              assertExists(this.handler));
        },
        isSupported: async () => Promise.resolve(true),
        isSupportPtz: checkSupportPtzForPhotoMode,
        prepareDevice: async (constraints, resolution) => prepareDeviceForPhoto(
            constraints, resolution, CaptureIntent.kStillCapture),
        fallbackMode: Mode.PHOTO,
      },
    };

    expert.addObserver(
        expert.ExpertOption.SAVE_METADATA, () => this.updateSaveMetadata());
  }

  initialize(handler: CaptureHandler): void {
    this.handler = handler;
  }

  private getCaptureParams(): CaptureParams {
    assert(this.captureParams !== null);
    return this.captureParams;
  }

  /**
   * Gets all mode candidates. Desired trying sequence of candidate modes is
   * reflected in the order of the returned array.
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
   * Gets factory to create `mode` capture object.
   */
  getModeFactory(mode: Mode): ModeFactory {
    return this.allModes[mode].getCaptureFactory();
  }

  /**
   * @param mode Mode for the capture.
   * @param constraints Constraints for preview stream.
   * @param captureResolution Capture resolution. May be null on device not
   *     support of setting resolution.
   * @param videoSnapshotResolution Video snapshot resolution. May be null on
   *     device not support of setting resolution.
   */
  setCaptureParams(
      mode: Mode, constraints: StreamConstraints,
      captureResolution: Resolution|null,
      videoSnapshotResolution: Resolution|null): void {
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

  isSupportPtz(
      mode: Mode, captureResolution: Resolution,
      previewResolution: Resolution): boolean {
    return this.allModes[mode].isSupportPtz(
        captureResolution, previewResolution);
  }

  /**
   * Creates and updates current mode object.
   *
   * @param factory The factory ready for producing mode capture object.
   */
  async updateMode(factory: ModeFactory): Promise<void> {
    if (this.current !== null) {
      await this.current.clear();
      this.disableSaveMetadata();
    }
    this.current = factory.produce();
    await this.updateSaveMetadata();
  }

  /**
   * Clears everything when mode is not needed anymore.
   */
  async clear(): Promise<void> {
    if (this.current !== null) {
      await this.current.clear();
      this.disableSaveMetadata();
    }
    this.captureParams = null;
    this.current = null;
  }

  /**
   * Checks whether to save image metadata or not.
   */
  private async updateSaveMetadata(): Promise<void> {
    if (expert.isEnabled(expert.ExpertOption.SAVE_METADATA)) {
      await this.enableSaveMetadata();
    } else {
      this.disableSaveMetadata();
    }
  }

  /**
   * Enables save metadata of subsequent photos in the current mode.
   */
  private async enableSaveMetadata(): Promise<void> {
    if (this.current !== null) {
      await this.current.addMetadataObserver();
    }
  }

  /**
   * Disables save metadata of subsequent photos in the current mode.
   */
  private disableSaveMetadata(): void {
    if (this.current !== null) {
      this.current.removeMetadataObserver();
    }
  }
}
