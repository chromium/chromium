// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertInstanceof, assertNumber} from '../../assert.js';
import * as dom from '../../dom.js';
import {reportError} from '../../error.js';
import * as h264 from '../../h264.js';
import * as state from '../../state.js';
// eslint-disable-next-line no-unused-vars
import {
  ErrorLevel,
  ErrorType,
  Resolution,
} from '../../type.js';
import * as util from '../../util.js';

/**
 * Precondition states to toggle custom video encoder parameters.
 * @type {!Array<!state.State>}
 */
const preconditions = [
  state.State.EXPERT,
  state.State.CUSTOM_VIDEO_PARAMETERS,
];

/**
 * Creates a controller for expert mode video encoder options of Camera view.
 */
export class VideoEncoderOptions {
  /**
   * @param {function(?h264.EncoderParameters): void} onChange Called when video
   * encoder option changed.
   */
  constructor(onChange) {
    /**
     * @const {function(?h264.EncoderParameters): void}
     * @private
     */
    this.onChange_ = onChange;

    /**
     * @const {!HTMLSelectElement}
     * @private
     */
    this.videoProfile_ = dom.get('#video-profile', HTMLSelectElement);

    /**
     * @const {!HTMLDivElement}
     * @private
     */
    this.bitrateSlider_ = dom.get('#bitrate-slider', HTMLDivElement);

    /**
     * @const {!HTMLInputElement}
     * @private
     */
    this.bitrateMultiplierInput_ =
        dom.getFrom(this.bitrateSlider_, 'input[type=range]', HTMLInputElement);

    /**
     * @const {!HTMLDivElement}
     * @private
     */
    this.bitrateMultiplerText_ = dom.get('#bitrate-multiplier', HTMLDivElement);

    /**
     * @const {!HTMLDivElement}
     * @private
     */
    this.bitrateText_ = dom.get('#bitrate-number', HTMLDivElement);

    /**
     * @type {?Resolution}
     * @private
     */
    this.resolution_ = null;

    /**
     * @type {?number}
     * @private
     */
    this.fps_ = null;
  }

  /**
   * @return {boolean}
   * @private
   */
  get enable_() {
    return preconditions.every((s) => state.get(s)) &&
        this.resolution_ !== null && this.fps_ !== null;
  }

  /**
   * @return {?h264.Profile}
   */
  get selectedProfile_() {
    if (this.videoProfile_.value === '') {
      return null;
    }
    return h264.assertProfile(Number(this.videoProfile_.value));
  }

  /**
   * @private
   */
  disableBitrateSlider_() {
    this.bitrateSlider_.hidden = true;
  }

  /**
   * @private
   */
  updateBitrate_() {
    if (!this.enable_ || this.selectedProfile_ === null) {
      this.onChange_(null);
      return;
    }
    const fps = assertNumber(this.fps_);
    const resolution = assertInstanceof(this.resolution_, Resolution);
    const profile = this.selectedProfile_;
    assert(profile !== null);
    const multiplier = this.bitrateMultiplierInput_.valueAsNumber;
    this.bitrateMultiplerText_.textContent = 'x' + multiplier;
    const bitrate = multiplier * resolution.area;
    this.bitrateText_.textContent = `${(bitrate / 1e6).toFixed(1)} Mbps`;
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
      this.onChange_(null);
      return;
    }
    this.onChange_({profile, level, bitrate});
  }

  /**
   * @private
   */
  updateBitrateRange_() {
    if (!this.enable_ || this.selectedProfile_ === null) {
      this.disableBitrateSlider_();
      this.onChange_(null);
      return;
    }
    const fps = assertNumber(this.fps_);
    const resolution = assertInstanceof(this.resolution_, Resolution);
    const profile = this.selectedProfile_;
    assert(profile !== null);

    const maxLevel = h264.Levels[h264.Levels.length - 1];
    if (!h264.checkLevelLimits(maxLevel, fps, resolution)) {
      reportError(
          ErrorType.NO_AVAILABLE_LEVEL, ErrorLevel.WARNING,
          new Error(
              `No available level for profile=${
                  h264.getProfileName(profile)}, ` +
              `resolution=${resolution}, ` +
              `fps=${fps}`));
      this.disableBitrateSlider_();
      this.onChange_(null);
      return;
    }
    const maxBitrate = h264.getMaxBitrate(profile, maxLevel);

    // The slider is used to select bitrate multiplier with respect to
    // resolution pixels.  It comply with chrome's logic of selecting default
    // bitrate with multiplier 2. Limits multiplier up to 15 for confining
    // result video size.
    const max = Math.min(Math.floor(maxBitrate / resolution.area), 15);
    this.bitrateMultiplierInput_.max = max.toString();
    this.bitrateMultiplierInput_.value =
        (this.bitrateMultiplierInput_.valueAsNumber || Math.min(max, 2))
            .toString();
    this.updateBitrate_();
    this.bitrateSlider_.hidden = false;
  }

  /**
   * @private
   */
  initBitrateSlider_() {
    for (const evt of ['input', 'change']) {
      this.bitrateMultiplierInput_.addEventListener(
          evt, () => this.updateBitrate_());
    }
  }

  /**
   * @private
   */
  initVideoProfile_() {
    // TODO(b/151047420): Remove options and use the largest supported profile.
    for (const profile of Object.values(h264.Profile)) {
      const tpl = util.instantiateTemplate('#video-profile-option-template');
      const option = dom.getFrom(tpl, 'option', HTMLOptionElement);
      option.value = profile.toString();
      option.textContent = h264.getProfileName(profile);
      this.videoProfile_.appendChild(option);
    }

    this.videoProfile_.addEventListener(
        'change', () => this.updateBitrateRange_());
  }

  /**
   * @public
   */
  initialize() {
    this.initVideoProfile_();
    this.initBitrateSlider_();

    for (const s of preconditions) {
      state.addObserver(s, () => this.updateBitrateRange_());
    }
  }

  /**
   * @param {!Resolution} resolution
   * @param {number} fps
   */
  updateValues(resolution, fps) {
    this.resolution_ = resolution;
    this.fps_ = fps;
    // TODO(b/151047420): Restore profile/bitrate preference for current camera,
    // resolution, fps.
    this.updateBitrateRange_();
  }
}
