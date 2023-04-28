// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertInstanceof, assertNumber} from '../../assert.js';
import * as dom from '../../dom.js';
import {reportError} from '../../error.js';
import * as expert from '../../expert.js';
import * as h264 from '../../h264.js';
import {
  ErrorLevel,
  ErrorType,
  Resolution,
} from '../../type.js';
import * as util from '../../util.js';

/**
 * Creates a controller for expert mode video encoder options of Camera view.
 */
export class VideoEncoderOptions {
  private readonly videoProfile = dom.get('#video-profile', HTMLSelectElement);

  private readonly bitrateSlider = dom.get('#bitrate-slider', HTMLDivElement);

  private readonly bitrateMultiplierInput: HTMLInputElement;

  private readonly bitrateMultiplerText =
      dom.get('#bitrate-multiplier', HTMLDivElement);

  private readonly bitrateText = dom.get('#bitrate-number', HTMLDivElement);

  private resolution: Resolution|null = null;

  private fps: number|null = null;

  /**
   * @param onChange Called when video encoder option changed.
   */
  constructor(
      private readonly onChange: (param: h264.EncoderParameters|null) => void) {
    this.bitrateMultiplierInput =
        dom.getFrom(this.bitrateSlider, 'input[type=range]', HTMLInputElement);
  }

  private get enable(): boolean {
    return expert.isEnabled(expert.ExpertOption.CUSTOM_VIDEO_PARAMETERS) &&
        this.resolution !== null && this.fps !== null;
  }

  private get selectedProfile(): h264.Profile|null {
    if (this.videoProfile.value === '') {
      return null;
    }
    return h264.assertProfile(Number(this.videoProfile.value));
  }

  private disableBitrateSlider() {
    this.bitrateSlider.hidden = true;
  }

  private updateBitrate() {
    if (!this.enable || this.selectedProfile === null) {
      this.onChange(null);
      return;
    }
    const fps = assertNumber(this.fps);
    const resolution = assertInstanceof(this.resolution, Resolution);
    const profile = this.selectedProfile;
    assert(profile !== null);
    const multiplier = this.bitrateMultiplierInput.valueAsNumber;
    this.bitrateMultiplerText.textContent = 'x' + multiplier;
    const bitrate = multiplier * resolution.area;
    this.bitrateText.textContent = `${(bitrate / 1e6).toFixed(1)} Mbps`;
    const level = h264.getMinimalLevel(profile, bitrate, fps, resolution);
    if (level === null) {
      reportError(
          ErrorType.NO_AVAILABLE_LEVEL, ErrorLevel.WARNING,
          new Error(
              `No available level for profile=${
                  h264.getProfileName(profile)}, ` +
              `resolution=${resolution}, ` +
              `fps=${fps}, ` +
              `bitrate=${bitrate}`));
      this.onChange(null);
      return;
    }
    this.onChange({profile, level, bitrate});
  }

  private updateBitrateRange() {
    if (!this.enable || this.selectedProfile === null) {
      this.disableBitrateSlider();
      this.onChange(null);
      return;
    }
    const fps = assertNumber(this.fps);
    const resolution = assertInstanceof(this.resolution, Resolution);
    const profile = this.selectedProfile;
    assert(profile !== null);

    const maxLevel = h264.LEVELS[h264.LEVELS.length - 1];
    if (!h264.checkLevelLimits(maxLevel, fps, resolution)) {
      reportError(
          ErrorType.NO_AVAILABLE_LEVEL, ErrorLevel.WARNING,
          new Error(
              `No available level for profile=${
                  h264.getProfileName(profile)}, ` +
              `resolution=${resolution}, ` +
              `fps=${fps}`));
      this.disableBitrateSlider();
      this.onChange(null);
      return;
    }
    const maxBitrate = h264.getMaxBitrate(profile, maxLevel);

    // The slider is used to select bitrate multiplier with respect to
    // resolution pixels.  It comply with chrome's logic of selecting default
    // bitrate with multiplier 2. Limits multiplier up to 15 for confining
    // result video size.
    const max = Math.min(Math.floor(maxBitrate / resolution.area), 15);
    this.bitrateMultiplierInput.max = max.toString();
    if (this.bitrateMultiplierInput.valueAsNumber === 0) {
      this.bitrateMultiplierInput.value = Math.min(max, 2).toString();
    }
    this.updateBitrate();
    this.bitrateSlider.hidden = false;
  }

  private initBitrateSlider() {
    for (const evt of ['input', 'change']) {
      this.bitrateMultiplierInput.addEventListener(
          evt, () => this.updateBitrate());
    }
  }

  private initVideoProfile() {
    // TODO(b/151047420): Remove options and use the largest supported profile.
    for (const profile of h264.profileValues) {
      const tpl = util.instantiateTemplate('#video-profile-option-template');
      const option = dom.getFrom(tpl, 'option', HTMLOptionElement);
      option.value = profile.toString();
      option.textContent = h264.getProfileName(profile);
      this.videoProfile.appendChild(option);
    }

    this.videoProfile.addEventListener(
        'change', () => this.updateBitrateRange());
  }

  initialize(): void {
    this.initVideoProfile();
    this.initBitrateSlider();

    expert.addObserver(
        expert.ExpertOption.CUSTOM_VIDEO_PARAMETERS,
        () => this.updateBitrateRange());
  }

  updateValues(resolution: Resolution, fps: number): void {
    this.resolution = resolution;
    this.fps = fps;
    // TODO(b/151047420): Restore profile/bitrate preference for current camera,
    // resolution, fps.
    this.updateBitrateRange();
  }
}
