// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertExists, assertNotReached} from '../assert.js';
import {Flag} from '../flag.js';
import {Point} from '../geometry.js';
import * as loadTimeData from '../models/load_time_data.js';
import {DeviceOperator} from '../mojo/device_operator.js';
import * as state from '../state.js';
import {CropRegionRect, Mode, Resolution} from '../type.js';

enum PtzAttr {
  PAN = 'pan',
  TILT = 'tilt',
  ZOOM = 'zoom',
}

export interface PtzCapabilities {
  pan: MediaSettingsRange;
  tilt: MediaSettingsRange;
  zoom: MediaSettingsRange;
}

interface PtzSettings {
  pan?: number;
  tilt?: number;
  zoom?: number;
}

/**
 * All pan, tilt, and zoom values must be non-empty.
 */
export type StrictPtzSettings = Required<PtzSettings>;

export interface PtzController {
  /**
   * Returns whether pan control is supported.
   */
  canPan(): boolean;

  /**
   * Returns whether tilt control is supported.
   */
  canTilt(): boolean;

  /**
   * Returns whether zoom control is supported.
   */
  canZoom(): boolean;

  /**
   * Returns min, max, and step values for pan, tilt, and zoom controls.
   */
  getCapabilities(): PtzCapabilities;

  /**
   * Returns current pan, tilt, and zoom settings.
   */
  getSettings(): PtzSettings;

  /**
   * Updates PTZ settings when the screen is rotated.
   */
  handleScreenRotationUpdated(): Promise<void>;

  /**
   * Returns whether pan and tilt functionalities are disabled when the
   * video is fully zoomed out.
   */
  isPanTiltRestricted(): boolean;

  /**
   * Resets to the default PTZ value.
   */
  resetPtz(): Promise<void>;

  /**
   * Applies a new pan value.
   */
  pan(value: number): Promise<void>;

  /**
   * Applies a new tilt value.
   */
  tilt(value: number): Promise<void>;

  /**
   * Applies a new zoom value.
   */
  zoom(value: number): Promise<void>;
}

/**
 * A set of vid:pid of external cameras whose pan and tilt controls are disabled
 * when all zooming out.
 */
const panTiltRestrictedCameras = new Set([
  '046d:0809',
  '046d:0823',
  '046d:0825',
  '046d:082d',
  '046d:0843',
  '046d:085c',
  '046d:085e',
  '046d:0893',
]);

export class MediaStreamPtzController implements PtzController {
  constructor(
      readonly track: MediaStreamTrack,
      readonly defaultPtz: MediaTrackConstraintSet,
      readonly vidPid: string|null) {}

  canPan(): boolean {
    return this.track.getCapabilities().pan !== undefined;
  }

  canTilt(): boolean {
    return this.track.getCapabilities().tilt !== undefined;
  }

  canZoom(): boolean {
    return this.track.getCapabilities().zoom !== undefined;
  }

  getCapabilities(): PtzCapabilities {
    return this.track.getCapabilities();
  }

  getSettings(): PtzSettings {
    return this.track.getSettings();
  }

  async handleScreenRotationUpdated(): Promise<void> {
    /* Do nothing. */
  }

  isPanTiltRestricted(): boolean {
    return state.get(state.State.USE_FAKE_CAMERA) ||
        (this.vidPid !== null && panTiltRestrictedCameras.has(this.vidPid));
  }

  async resetPtz(): Promise<void> {
    await this.track.applyConstraints({advanced: [this.defaultPtz]});
  }

  async pan(value: number): Promise<void> {
    await this.applyPtz(PtzAttr.PAN, value);
  }

  async tilt(value: number): Promise<void> {
    await this.applyPtz(PtzAttr.TILT, value);
  }

  async zoom(value: number): Promise<void> {
    await this.applyPtz(PtzAttr.ZOOM, value);
  }

  private async applyPtz(attr: PtzAttr, value: number): Promise<void> {
    if (!this.track.enabled) {
      return;
    }
    await this.track.applyConstraints({advanced: [{[attr]: value}]});
  }
}

const DIGITAL_ZOOM_MAX_PAN = 1;
const DIGITAL_ZOOM_MAX_TILT = 1;
const DIGITAL_ZOOM_DEFAULT_MAX_ZOOM = 6;
export const DIGITAL_ZOOM_CAPABILITIES: PtzCapabilities = {
  pan: {min: -DIGITAL_ZOOM_MAX_PAN, max: DIGITAL_ZOOM_MAX_PAN, step: 0.1},
  tilt: {min: -DIGITAL_ZOOM_MAX_TILT, max: DIGITAL_ZOOM_MAX_TILT, step: 0.1},
  zoom: {min: 1, max: DIGITAL_ZOOM_DEFAULT_MAX_ZOOM, step: 0.1},
};
const DIGITAL_ZOOM_DEFAULT_SETTINGS: PtzSettings = {
  pan: 0,
  tilt: 0,
  zoom: 1,
};

/**
 * Calculate the crop region when fully zoomed out for the given aspect ratio.
 * The crop region is calculated based on camera metadata
 * ANDROID_SENSOR_INFO_ACTIVE_ARRAY_SIZE. If the target aspect ratio doesn't
 * match the active array's aspect ratio, the crop region is either cropped
 * vertically or horizontally, and centered within the active array.
 */
function getFullCropRegionForAspectRatio(
    activeArray: Resolution, targetAspectRatio: number): CropRegionRect {
  const {width: originalWidth, height: originalHeight} = activeArray;
  if (activeArray.aspectRatio > targetAspectRatio) {
    // Crop vertically if the original aspect ratio is wider than the target.
    const croppedWidth = Math.round(originalHeight * targetAspectRatio);
    return {
      x: Math.round((originalWidth - croppedWidth) / 2),
      y: 0,
      width: croppedWidth,
      height: originalHeight,
    };
  }
  // Otherwise, crop horizontally.
  const croppedHeight = Math.round(originalWidth / targetAspectRatio);
  return {
    x: 0,
    y: Math.round((originalHeight - croppedHeight) / 2),
    width: originalWidth,
    height: croppedHeight,
  };
}

/**
 * Asserts that all pan, tilt, and zoom fields have values.
 */
export function assertStrictPtzSettings({pan, tilt, zoom}: PtzSettings):
    StrictPtzSettings {
  assert(pan !== undefined);
  assert(tilt !== undefined);
  assert(zoom !== undefined && zoom > 0, `Zoom value ${zoom} is invalid.`);
  return {pan, tilt, zoom};
}

/**
 * Calculate a crop region from given PTZ settings. The crop region result is
 * normalized given full width and full height equal to 1.
 */
function calculateNormalizedCropRegion(ptzSettings: PtzSettings):
    CropRegionRect {
  const {pan, tilt, zoom} = assertStrictPtzSettings(ptzSettings);

  const width = 1 / zoom;
  const height = 1 / zoom;

  // Top-left coordinate of the crop region before the pan and tilt values are
  // applied.
  const startX = (1 - width) / 2;
  const startY = (1 - height) / 2;

  // Move x, y with pan and tilt values. Pan and tilt values are in the range
  // [-1, 1], with pan = -1 being leftmost, and tilt = -1 being bottommost.
  const x = startX + ((pan / DIGITAL_ZOOM_MAX_PAN) * startX);
  const y = startY - ((tilt / DIGITAL_ZOOM_MAX_TILT) * startY);

  // Verify that the calculated crop region is valid.
  const lowerBound = -1e-3;
  const upperBound = 1 + 1e-3;
  assert(x > lowerBound && x < upperBound && y > lowerBound && y < upperBound);
  assert((x + width) < upperBound && (y + height) < upperBound);

  return {x, y, width, height};
}

/**
 * Calculate a crop region from PTZ settings with respect to |fullCropRegion|.
 */
function calculateCropRegion(
    ptzSettings: PtzSettings, fullCropRegion: CropRegionRect): CropRegionRect {
  const normCropRegion = calculateNormalizedCropRegion(ptzSettings);
  const {width: fullWidth, height: fullHeight} = fullCropRegion;

  return {
    x: Math.round(fullCropRegion.x + (normCropRegion.x * fullWidth)),
    y: Math.round(fullCropRegion.y + (normCropRegion.y * fullHeight)),
    width: Math.round(normCropRegion.width * fullWidth),
    height: Math.round(normCropRegion.height * fullHeight),
  };
}

/**
 * Asserts that pan, tilt, or zoom value is within the range defined in
 * |DIGITAL_ZOOM_CAPABILITIES|.
 */
function assertPtzRange(attr: PtzAttr, value: number) {
  const {max: maxValue, min: minValue} = DIGITAL_ZOOM_CAPABILITIES[attr];
  const tolerance = 1e-3;
  assert(
      value >= minValue - tolerance && value <= maxValue + tolerance,
      `${attr} value ${value} is not within the allowed range.`);
}

/**
 * Rotates (x, y) clockwise for |rotation| degree around the coordinate (0, 0).
 */
function rotateClockwise(
    x: number, y: number, rotation: number): [number, number] {
  rotation = rotation % 360;
  switch (rotation) {
    case 0:
      return [x, y];
    case 90:
      return [y, -x];
    case 180:
      return [-x, -y];
    case 270:
      return [-y, x];
    default:
      assertNotReached(`Unexpected rotation: ${rotation}`);
  }
}

/**
 * Rotates PTZ settings clockwise by |rotation| degree.
 */
function rotatePtz(ptzSettings: PtzSettings, rotation: number): PtzSettings {
  const {pan, tilt, zoom} = assertStrictPtzSettings(ptzSettings);
  const [rotatedPan, rotatedTilt] = rotateClockwise(pan, tilt, rotation);
  return {pan: rotatedPan, tilt: rotatedTilt, zoom};
}

export class DigitalZoomPtzController implements PtzController {
  /**
   * Current PTZ settings based on the camera frame with rotation = 0. This
   * value remains the same regardless of the camera rotation.
   */
  private ptzSettings: PtzSettings = DIGITAL_ZOOM_DEFAULT_SETTINGS;

  /**
   * Current camera frame rotation.
   */
  private cameraRotation = 0;

  private constructor(
      private readonly deviceId: string,
      private readonly fullCropRegion: CropRegionRect) {}

  canPan(): boolean {
    return true;
  }

  canTilt(): boolean {
    return true;
  }

  canZoom(): boolean {
    return true;
  }

  getCapabilities(): PtzCapabilities {
    return DIGITAL_ZOOM_CAPABILITIES;
  }

  getSettings(): PtzSettings {
    // Rotates current PTZ settings because pan and tilt values are different in
    // different camera frame rotations.
    return rotatePtz(this.ptzSettings, this.cameraRotation);
  }

  async handleScreenRotationUpdated(): Promise<void> {
    const deviceOperator = assertExists(DeviceOperator.getInstance());
    this.cameraRotation =
        await deviceOperator.getCameraFrameRotation(this.deviceId);
  }

  isPanTiltRestricted(): boolean {
    // When fully zoomed out, calculated crop region equals to |fullCropRegion|,
    // this means pan and tilt are disabled.
    return true;
  }

  /**
   * Map a point on the preview frame to a corresponding point on the camera
   * frame based on the current crop region.
   *
   * @param point The point in normalize coordidate system, which means both
   *     |x| and |y| are in range [0, 1).
   */
  calculatePointOnCameraFrame(point: Point): Point {
    const activeRegion = calculateNormalizedCropRegion(this.getSettings());
    const x = activeRegion.x + (point.x * activeRegion.width);
    const y = activeRegion.y + (point.y * activeRegion.height);
    return new Point(x, y);
  }

  async resetPtz(): Promise<void> {
    const deviceOperator = assertExists(DeviceOperator.getInstance());
    await deviceOperator.resetCropRegion(this.deviceId);
    this.ptzSettings = DIGITAL_ZOOM_DEFAULT_SETTINGS;
    state.set(state.State.SUPER_RES_ZOOM, false);
  }

  async pan(value: number): Promise<void> {
    assertPtzRange(PtzAttr.PAN, value);
    const newSettings = {...this.getSettings(), pan: value};
    await this.applyPtz(newSettings);
  }

  async tilt(value: number): Promise<void> {
    assertPtzRange(PtzAttr.TILT, value);
    const newSettings = {...this.getSettings(), tilt: value};
    await this.applyPtz(newSettings);
  }

  async zoom(value: number): Promise<void> {
    assertPtzRange(PtzAttr.ZOOM, value);
    const newSettings = {...this.getSettings(), zoom: value};
    await this.applyPtz(newSettings);
  }

  private async applyPtz(settings: PtzSettings): Promise<void> {
    if (this.isFullFrame(settings)) {
      return this.resetPtz();
    }

    const deviceOperator = assertExists(DeviceOperator.getInstance());

    // Rotates |settings| counterclockwise to get the PTZ settings for 0
    // degree camera rotation.
    this.cameraRotation =
        await deviceOperator.getCameraFrameRotation(this.deviceId);
    const baseSettings = rotatePtz(settings, 360 - this.cameraRotation);

    const cropRegion = calculateCropRegion(baseSettings, this.fullCropRegion);
    await deviceOperator.setCropRegion(this.deviceId, cropRegion);
    this.ptzSettings = baseSettings;

    state.set(state.State.SUPER_RES_ZOOM, this.isSuperResZoomPhotoMode());
  }

  private isSuperResZoomPhotoMode(): boolean {
    return state.get(Mode.PHOTO) && loadTimeData.getChromeFlag(Flag.SUPER_RES);
  }

  private isFullFrame({zoom}: PtzSettings): boolean {
    assert(zoom !== undefined);
    const minZoom = assertExists(DIGITAL_ZOOM_CAPABILITIES.zoom.min);
    const zoomStep = assertExists(DIGITAL_ZOOM_CAPABILITIES.zoom.step);
    return Math.abs(zoom - minZoom) < zoomStep;
  }

  static async create(deviceId: string, aspectRatio: number):
      Promise<DigitalZoomPtzController> {
    const deviceOperator = assertExists(DeviceOperator.getInstance());
    const activeArray = await deviceOperator.getActiveArraySize(deviceId);
    const fullCropRegion =
        getFullCropRegionForAspectRatio(activeArray, aspectRatio);
    return new DigitalZoomPtzController(deviceId, fullCropRegion);
  }
}
