// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertExists} from '../assert.js';
import {DeviceOperator} from '../mojo/device_operator.js';
import * as state from '../state.js';
import {CropRegionRect, Resolution} from '../type.js';

enum PTZAttr {
  PAN = 'pan',
  TILT = 'tilt',
  ZOOM = 'zoom',
}

interface PTZCapabilities {
  pan: MediaSettingsRange;
  tilt: MediaSettingsRange;
  zoom: MediaSettingsRange;
}

interface PTZSettings {
  pan?: number;
  tilt?: number;
  zoom?: number;
}

export interface PTZController {
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
  getCapabilities(): PTZCapabilities;

  /**
   * Returns current pan, tilt, and zoom settings.
   */
  getSettings(): PTZSettings;

  /**
   * Returns whether pan and tilt functionalities are disabled when the video is
   * fully zoomed out.
   */
  isPanTiltRestricted(): boolean;

  /**
   * Resets to the default PTZ value.
   */
  resetPTZ(): Promise<void>;

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

export class MediaStreamPTZController implements PTZController {
  constructor(
      readonly track: MediaStreamTrack,
      readonly defaultPTZ: MediaTrackConstraintSet,
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

  getCapabilities(): PTZCapabilities {
    return this.track.getCapabilities();
  }

  getSettings(): PTZSettings {
    return this.track.getSettings();
  }

  isPanTiltRestricted(): boolean {
    return state.get(state.State.USE_FAKE_CAMERA) ||
        (this.vidPid !== null && panTiltRestrictedCameras.has(this.vidPid));
  }

  async resetPTZ(): Promise<void> {
    await this.track.applyConstraints({advanced: [this.defaultPTZ]});
  }

  async pan(value: number): Promise<void> {
    await this.applyPTZ(PTZAttr.PAN, value);
  }

  async tilt(value: number): Promise<void> {
    await this.applyPTZ(PTZAttr.TILT, value);
  }

  async zoom(value: number): Promise<void> {
    await this.applyPTZ(PTZAttr.ZOOM, value);
  }

  private async applyPTZ(attr: PTZAttr, value: number): Promise<void> {
    if (!this.track.enabled) {
      return;
    }
    await this.track.applyConstraints({advanced: [{[attr]: value}]});
  }
}

const DIGITAL_ZOOM_MAX_PAN = 1;
const DIGITAL_ZOOM_MAX_TILT = 1;
const DIGITAL_ZOOM_DEFAULT_MAX_ZOOM = 6;
const DIGITAL_ZOOM_CAPABILITIES: PTZCapabilities = {
  pan: {min: -DIGITAL_ZOOM_MAX_PAN, max: DIGITAL_ZOOM_MAX_PAN, step: 0.1},
  tilt: {min: -DIGITAL_ZOOM_MAX_TILT, max: DIGITAL_ZOOM_MAX_TILT, step: 0.1},
  zoom: {min: 1, max: DIGITAL_ZOOM_DEFAULT_MAX_ZOOM, step: 0.1},
};
const DIGITAL_ZOOM_DEFAULT_SETTINGS: PTZSettings = {
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
 * Calculate a crop region from given PTZ settings. The crop region result is
 * normalized given full width and full height equal to 1.
 */
function calculateNormalizedCropRegion({pan, tilt, zoom}: PTZSettings):
    CropRegionRect {
  assert(pan !== undefined);
  assert(tilt !== undefined);
  assert(zoom !== undefined && zoom > 0, `Zoom value ${zoom} is invalid.`);

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
    ptzSettings: PTZSettings, fullCropRegion: CropRegionRect): CropRegionRect {
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
function assertPTZRange(attr: PTZAttr, value: number) {
  const {max: maxValue, min: minValue} = DIGITAL_ZOOM_CAPABILITIES[attr];
  const tolerance = 1e-3;
  assert(
      value >= minValue - tolerance && value <= maxValue + tolerance,
      `${attr} value ${value} is not within the allowed range.`);
}

export class DigitalZoomPTZController implements PTZController {
  private ptzSettings: PTZSettings = DIGITAL_ZOOM_DEFAULT_SETTINGS;

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

  getCapabilities(): PTZCapabilities {
    return DIGITAL_ZOOM_CAPABILITIES;
  }

  getSettings(): PTZSettings {
    return this.ptzSettings;
  }

  isPanTiltRestricted(): boolean {
    // When fully zoomed out, calculated crop region equals to |fullCropRegion|,
    // this means pan and tilt are disabled.
    return true;
  }

  async resetPTZ(): Promise<void> {
    const deviceOperator = assertExists(DeviceOperator.getInstance());
    await deviceOperator.resetCropRegion(this.deviceId);
    this.ptzSettings = DIGITAL_ZOOM_DEFAULT_SETTINGS;
  }

  async pan(value: number): Promise<void> {
    assertPTZRange(PTZAttr.PAN, value);
    const newSettings = {...this.ptzSettings, pan: value};
    await this.applyPTZ(newSettings);
  }

  async tilt(value: number): Promise<void> {
    assertPTZRange(PTZAttr.TILT, value);
    const newSettings = {...this.ptzSettings, tilt: value};
    await this.applyPTZ(newSettings);
  }

  async zoom(value: number): Promise<void> {
    assertPTZRange(PTZAttr.ZOOM, value);
    const newSettings = {...this.ptzSettings, zoom: value};
    await this.applyPTZ(newSettings);
  }

  private async applyPTZ(newSettings: PTZSettings): Promise<void> {
    if (this.isFullFrame(newSettings)) {
      return this.resetPTZ();
    }
    const deviceOperator = assertExists(DeviceOperator.getInstance());
    const cropRegion = calculateCropRegion(newSettings, this.fullCropRegion);
    await deviceOperator.setCropRegion(this.deviceId, cropRegion);
    this.ptzSettings = newSettings;
  }

  private isFullFrame({zoom}: PTZSettings): boolean {
    assert(zoom !== undefined);
    const minZoom = assertExists(DIGITAL_ZOOM_CAPABILITIES.zoom.min);
    const zoomStep = assertExists(DIGITAL_ZOOM_CAPABILITIES.zoom.step);
    return Math.abs(zoom - minZoom) < zoomStep;
  }

  static async create(deviceId: string, aspectRatio: number):
      Promise<DigitalZoomPTZController> {
    const deviceOperator = assertExists(DeviceOperator.getInstance());
    const activeArray = await deviceOperator.getActiveArraySize(deviceId);
    const fullCropRegion =
        getFullCropRegionForAspectRatio(activeArray, aspectRatio);
    return new DigitalZoomPTZController(deviceId, fullCropRegion);
  }
}
