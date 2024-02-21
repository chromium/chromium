// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as state from '../state.js';

type PTZAttr = 'pan'|'tilt'|'zoom';
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
   * Returns whether the camera can do pan and tilt when the video is fully
   * zoomed out.
   */
  canPanTiltWhenZoomedOut(): boolean;

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

  canPanTiltWhenZoomedOut(): boolean {
    return state.get(state.State.USE_FAKE_CAMERA) ||
        (this.vidPid !== null && panTiltRestrictedCameras.has(this.vidPid));
  }

  getCapabilities(): PTZCapabilities {
    return this.track.getCapabilities();
  }

  getSettings(): PTZSettings {
    return this.track.getSettings();
  }

  async resetPTZ(): Promise<void> {
    await this.track.applyConstraints({advanced: [this.defaultPTZ]});
  }

  async pan(value: number): Promise<void> {
    await this.applyPTZ('pan', value);
  }

  async tilt(value: number): Promise<void> {
    await this.applyPTZ('tilt', value);
  }

  async zoom(value: number): Promise<void> {
    await this.applyPTZ('zoom', value);
  }

  private async applyPTZ(attr: PTZAttr, value: number): Promise<void> {
    if (!this.track.enabled) {
      return;
    }
    await this.track.applyConstraints({advanced: [{[attr]: value}]});
  }
}
