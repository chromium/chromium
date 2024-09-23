// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/polymer/v3_0/iron-media-query/iron-media-query.js';
import './fingerprint_icons.html.js';
import '//resources/cr_elements/cr_lottie/cr_lottie.js';

import type {CrIconElement} from '//resources/cr_elements/cr_icon/cr_icon.js';
import type {CrLottieElement} from '//resources/cr_elements/cr_lottie/cr_lottie.js';
import {assert} from '//resources/js/assert.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './fingerprint_progress_arc.html.js';

/**
 * The dark-mode fingerprint icon displayed temporarily each time a user scans
 * their fingerprint and persistently once the enrollment process is complete.
 */
export const FINGERPRINT_SCANNED_ICON_DARK: string =
    'fingerprint-icon:fingerprint-scanned-dark';

/**
 * The light-mode fingerprint icon displayed temporarily each time a user scans
 * their fingerprint and persistently once the enrollment process is complete.
 */
export const FINGERPRINT_SCANNED_ICON_LIGHT: string =
    'fingerprint-icon:fingerprint-scanned-light';

export const FINGERPRINT_CHECK_DARK_URL: string =
    'chrome://theme/IDR_FINGERPRINT_COMPLETE_CHECK_DARK';

export const FINGERPRINT_CHECK_LIGHT_URL: string =
    'chrome://theme/IDR_FINGERPRINT_COMPLETE_CHECK_LIGHT';

/**
 * The dark-mode color of the progress circle background: Google Grey 700.
 */
export const PROGRESS_CIRCLE_BACKGROUND_COLOR_DARK: string =
    'rgba(95, 99, 104, 1.0)';

/**
 * The light-mode color of the progress circle background: Google Grey 200.
 */
export const PROGRESS_CIRCLE_BACKGROUND_COLOR_LIGHT: string =
    'rgba(232, 234, 237, 1.0)';

/**
 * The dark-mode color of the setup progress arc: Google Blue 400.
 */
export const PROGRESS_CIRCLE_FILL_COLOR_DARK: string =
    'rgba(102, 157, 246, 1.0)';

/**
 * The light-mode color of the setup progress arc: Google Blue 500.
 */
export const PROGRESS_CIRCLE_FILL_COLOR_LIGHT: string =
    'rgba(66, 133, 244, 1.0)';

/**
 * The time in milliseconds of the animation updates.
 */
const ANIMATE_TICKS_MS: number = 20;

/**
 * The duration in milliseconds of the animation of the progress circle when the
 * user is touching the scanner.
 */
const ANIMATE_DURATION_MS: number = 200;

/**
 * The radius of the add fingerprint progress circle.
 */
const DEFAULT_PROGRESS_CIRCLE_RADIUS: number = 114;

/**
 * The default height of the icon located in the center of the fingerprint
 * progress circle.
 */
const ICON_HEIGHT: number = 118;

/**
 * The default width of the icon located in the center of the fingerprint
 * progress circle.
 */
const ICON_WIDTH: number = 106;

/**
 * The default size of the check mark located in the bottom-right corner of the
 * fingerprint progress circle.
 */
const CHECK_MARK_SIZE: number = 53;

/**
 * The time in milliseconds of the fingerprint scan success timeout.
 */
const FINGERPRINT_SCAN_SUCCESS_MS: number = 500;

/**
 * The thickness of the fingerprint progress circle.
 */
const PROGRESS_CIRCLE_STROKE_WIDTH: number = 4;


export interface FingerprintProgressArcElement {
  $: {
    canvas: HTMLCanvasElement,
    fingerprintScanned: CrIconElement,
    scanningAnimation: CrLottieElement,
  };
}

export class FingerprintProgressArcElement extends PolymerElement {
  static get is() {
    return 'fingerprint-progress-arc';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Radius of the fingerprint progress circle being displayed.
       */
      circleRadius: {
        type: Number,
        value: DEFAULT_PROGRESS_CIRCLE_RADIUS,
      },

      /**
       * Whether lottie animation should be autoplayed.
       */
      autoplay: {
        type: Boolean,
        value: false,
      },

      /**
       * Scale factor based the configured radius (circleRadius) vs the default
       * radius (DEFAULT_PROGRESS_CIRCLE_RADIUS).
       * This will affect the size of icons and check mark.
       */
      scale_: {
        type: Number,
        value: 1.0,
      },

      /**
       * Whether fingerprint enrollment is complete.
       */
      isComplete_: Boolean,

      /**
       * Whether the fingerprint progress page is being rendered in dark mode.
       */
      isDarkModeActive_: {
        type: Boolean,
        value: false,
        observer: 'onDarkModeChanged_',
      },
    };
  }

  circleRadius: number;
  autoplay: boolean;
  private scale_: number;
  private isComplete_: boolean;
  private isDarkModeActive_: boolean;

  // Animation ID for the fingerprint progress circle.
  private progressAnimationIntervalId_: number|undefined = undefined;

  // Percentage of the enrollment process completed as of the last update.
  private progressPercentDrawn_: number = 0;

  // Timer ID for fingerprint scan success update.
  private updateTimerId_: number|undefined = undefined;

  /**
   * Updates the current state to account for whether dark mode is enabled.
   */
  private onDarkModeChanged_() {
    this.clearCanvas_();
    this.drawProgressCircle_(this.progressPercentDrawn_);
    this.updateAnimationAsset_();
    this.updateIconAsset_();
  }

  override connectedCallback() {
    super.connectedCallback();

    this.scale_ = this.circleRadius / DEFAULT_PROGRESS_CIRCLE_RADIUS;
    this.updateIconAsset_();
    this.updateImages_();
  }

  /**
   * Reset the element to initial state, when the enrollment just starts.
   */
  reset() {
    this.cancelAnimations_();
    this.clearCanvas_();
    this.isComplete_ = false;
    // Draw an empty background for the progress circle.
    this.drawProgressCircle_(/** currentPercent = */ 0);
    this.$.fingerprintScanned.hidden = true;

    const scanningAnimation = this.$.scanningAnimation;
    scanningAnimation.singleLoop = false;
    scanningAnimation.classList.add('translucent');
    this.updateAnimationAsset_();
    this.resizeAndCenterIcon_(scanningAnimation);
    scanningAnimation.hidden = false;
  }

  /**
   * Animates the progress circle. Animates an arc that starts at the top of
   * the circle to prevPercentComplete, to an arc that starts at the top of the
   * circle to currPercentComplete.
   * @param prevPercentComplete The previous progress indicates the start angle
   *     of the arc we want to draw.
   * @param currPercentComplete The current progress indicates the end angle of
   *    the arc we want to draw.
   * @param isComplete Indicate whether enrollment is complete.
   */
  setProgress(
      prevPercentComplete: number, currPercentComplete: number,
      isComplete: boolean) {
    if (this.isComplete_) {
      return;
    }
    this.isComplete_ = isComplete;
    this.cancelAnimations_();

    let nextPercentToDraw = prevPercentComplete;
    const endPercent = isComplete ? 100 : Math.min(100, currPercentComplete);
    // The value by which to update the progress percent each tick.
    const step = (endPercent - prevPercentComplete) /
        (ANIMATE_DURATION_MS / ANIMATE_TICKS_MS);

    // Function that is called every tick of the interval, draws the arc a bit
    // closer to the final destination each tick, until it reaches the final
    // destination.
    const doAnimate = () => {
      if (nextPercentToDraw >= endPercent) {
        if (this.progressAnimationIntervalId_) {
          clearInterval(this.progressAnimationIntervalId_);
          this.progressAnimationIntervalId_ = undefined;
        }
        nextPercentToDraw = endPercent;
      }

      this.clearCanvas_();
      this.drawProgressCircle_(nextPercentToDraw);
      if (!this.progressAnimationIntervalId_) {
        this.dispatchEvent(new CustomEvent(
            'fingerprint-progress-arc-drawn', {bubbles: true, composed: true}));
      }
      nextPercentToDraw += step;
    };

    this.progressAnimationIntervalId_ =
        setInterval(doAnimate, ANIMATE_TICKS_MS);

    if (isComplete) {
      this.animateScanComplete_();
    } else {
      this.animateScanProgress_();
    }
  }

  /**
   * Controls the animation based on the value of |shouldPlay|.
   * @param shouldPlay Will play the animation if true else pauses it.
   */
  setPlay(shouldPlay: boolean) {
    this.$.scanningAnimation.setPlay(shouldPlay);
  }

  isComplete(): boolean {
    return this.isComplete_;
  }

  /**
   * Draws an arc on the canvas element around the center with radius
   * |circleRadius|.
   * @param startAngle The start angle of the arc we want to draw.
   * @param endAngle The end angle of the arc we want to draw.
   * @param color The color of the arc we want to draw. The string is
   *     in the format rgba(r',g',b',a'). r', g', b' are values from [0-255]
   *     and a' is a value from [0-1].
   */
  private drawArc_(startAngle: number, endAngle: number, color: string) {
    const c = this.$.canvas;
    const ctx = c.getContext('2d');
    assert(!!ctx);

    ctx.beginPath();
    ctx.arc(c.width / 2, c.height / 2, this.circleRadius, startAngle, endAngle);
    ctx.lineWidth = PROGRESS_CIRCLE_STROKE_WIDTH;
    ctx.strokeStyle = color;
    ctx.stroke();
  }

  /**
   * Draws a circle on the canvas element around the center with radius
   * |circleRadius|. The first |currentPercent| of the circle, starting at the
   * top, is drawn with |PROGRESS_CIRCLE_FILL_COLOR|; the remainder of the
   * circle is drawn |PROGRESS_CIRCLE_BACKGROUND_COLOR|.
   * @param currentPercent A value from [0-100] indicating the
   *     percentage of progress to display.
   */
  private drawProgressCircle_(currentPercent: number) {
    // Angles on HTML canvases start at 0 radians on the positive x-axis and
    // increase in the clockwise direction. We want to start at the top of the
    // circle, which is 3pi/2.
    const start = 3 * Math.PI / 2;
    const currentAngle = 2 * Math.PI * currentPercent / 100;

    // Drawing two arcs to form a circle gives a nicer look than drawing an arc
    // on top of a circle (i.e., compared to drawing a full background circle
    // first). If |currentAngle| is 0, draw from 3pi/2 to 7pi/2 explicitly;
    // otherwise, the regular draw from |start| + |currentAngle| to |start|
    // will do nothing.
    this.drawArc_(
        start, start + currentAngle,
        this.isDarkModeActive_ ? PROGRESS_CIRCLE_FILL_COLOR_DARK :
                                 PROGRESS_CIRCLE_FILL_COLOR_LIGHT);
    this.drawArc_(
        start + currentAngle, currentAngle <= 0 ? 7 * Math.PI / 2 : start,
        this.isDarkModeActive_ ? PROGRESS_CIRCLE_BACKGROUND_COLOR_DARK :
                                 PROGRESS_CIRCLE_BACKGROUND_COLOR_LIGHT);
    this.progressPercentDrawn_ = currentPercent;
  }

  /**
   * Updates the lottie animation taking into account the current state and
   * whether dark mode is enabled.
   */
  private updateAnimationAsset_() {
    const scanningAnimation = this.$.scanningAnimation;
    if (this.isComplete_) {
      scanningAnimation.animationUrl = this.isDarkModeActive_ ?
          FINGERPRINT_CHECK_DARK_URL :
          FINGERPRINT_CHECK_LIGHT_URL;
      return;
    }
    scanningAnimation.animationUrl = this.isDarkModeActive_ ?
        'chrome://theme/IDR_FINGERPRINT_ICON_ANIMATION_DARK' :
        'chrome://theme/IDR_FINGERPRINT_ICON_ANIMATION_LIGHT';
  }

  /**
   * Updates the fingerprint-scanned icon based on whether dark mode is enabled.
   */
  private updateIconAsset_() {
    this.$.fingerprintScanned.icon = this.isDarkModeActive_ ?
        FINGERPRINT_SCANNED_ICON_DARK :
        FINGERPRINT_SCANNED_ICON_LIGHT;
  }

  /*
   * Cleans up any pending animation update created by setInterval().
   */
  private cancelAnimations_() {
    this.progressPercentDrawn_ = 0;
    if (this.progressAnimationIntervalId_) {
      clearInterval(this.progressAnimationIntervalId_);
      this.progressAnimationIntervalId_ = undefined;
    }
    if (this.updateTimerId_) {
      window.clearTimeout(this.updateTimerId_);
      this.updateTimerId_ = undefined;
    }
  }

  /**
   * Show animation for enrollment completion.
   */
  private animateScanComplete_() {
    const scanningAnimation = this.$.scanningAnimation;
    scanningAnimation.singleLoop = true;
    scanningAnimation.autoplay = true;
    scanningAnimation.classList.remove('translucent');
    this.updateAnimationAsset_();
    this.resizeCheckMark_(scanningAnimation);
    this.$.fingerprintScanned.hidden = false;
  }

  /**
   * Show animation for enrollment in progress.
   */
  private animateScanProgress_() {
    this.$.fingerprintScanned.hidden = false;
    this.$.scanningAnimation.hidden = true;
    this.updateTimerId_ = window.setTimeout(() => {
      this.$.scanningAnimation.hidden = false;
      this.$.fingerprintScanned.hidden = true;
    }, FINGERPRINT_SCAN_SUCCESS_MS);
  }

  /**
   * Clear the canvas of any renderings.
   */
  private clearCanvas_() {
    const c = this.$.canvas;
    const ctx = c.getContext('2d');
    assert(!!ctx);
    ctx.clearRect(0, 0, c.width, c.height);
  }

  /**
   * Update the size and position of the animation images.
   */
  private updateImages_() {
    this.resizeAndCenterIcon_(this.$.scanningAnimation);
    this.resizeAndCenterIcon_(this.$.fingerprintScanned);
  }

  /**
   * Resize the icon based on the scale and place it in the center of the
   * fingerprint progress circle.
   */
  private resizeAndCenterIcon_(target: HTMLElement) {
    // Resize icon based on the default width/height and scale.
    target.style.width = ICON_WIDTH * this.scale_ + 'px';
    target.style.height = ICON_HEIGHT * this.scale_ + 'px';

    // Place in the center of the canvas.
    const left = this.$.canvas.width / 2 - ICON_WIDTH * this.scale_ / 2;
    const top = this.$.canvas.height / 2 - ICON_HEIGHT * this.scale_ / 2;
    target.style.left = left + 'px';
    target.style.top = top + 'px';
  }

  /**
   * Resize the check mark based on the scale and place it in the bottom-right
   * corner of the fingerprint progress circle.
   */
  private resizeCheckMark_(target: HTMLElement) {
    // Resize check mark based on the default size and scale.
    target.style.width = CHECK_MARK_SIZE * this.scale_ + 'px';
    target.style.height = CHECK_MARK_SIZE * this.scale_ + 'px';

    // Place it in the bottom-right corner of the fingerprint progress circle.
    const top = this.$.canvas.height / 2 + this.circleRadius -
        CHECK_MARK_SIZE * this.scale_;
    const left = this.$.canvas.width / 2 + this.circleRadius -
        CHECK_MARK_SIZE * this.scale_;
    target.style.left = left + 'px';
    target.style.top = top + 'px';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'fingerprint-progress-arc': FingerprintProgressArcElement;
  }
}

customElements.define(
    FingerprintProgressArcElement.is, FingerprintProgressArcElement);
