// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './text_overlay.js';

import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './motion_overlay.css.js';
import {getHtml} from './motion_overlay.html.js';
import type {IndigoTextOverlayElement} from './text_overlay.js';

const SWIPE_DURATION_MS: number = 2000;
const LOADING_CIRCLES_START_DELAY_MS: number = 1300;
const TEXT_OVERLAY_START_DELAY_MS: number = 1000;

// If the exit starts within this opacity of either end of #maxBlurLayer's
// fade-in, it snaps to that end, so the exit draws one blur instead of two.
const MAX_BLUR_OPACITY_SNAP: number = 0.01;

// How much of #maxBlurLayer is showing. maxBlur_ is null while the entry fades
// it in, 'full' once the fade-in has finished, and 'hidden' or 'partial' if the
// exit starts before that.
type MaxBlur = 'hidden'|'partial'|'full';

export interface IndigoMotionOverlayElement {
  $: {
    maxBlurLayer: HTMLElement,
    swipeEllipse1: HTMLElement,
    swipeEllipse2: HTMLElement,
    loadingCircleDark: HTMLElement,
    loadingCircleLight: HTMLElement,
    textOverlay: IndigoTextOverlayElement,
  };
}

export class IndigoMotionOverlayElement extends CrLitElement {
  static get is() {
    return 'indigo-motion-overlay';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      animationState: {
        type: String,
        reflect: true,
        attribute: 'animation-state',
      },
      showLoadingCircles_: {
        type: Boolean,
        reflect: true,
        attribute: 'show-loading-circles',
      },
      maxBlur_: {
        type: String,
        reflect: true,
        attribute: 'max-blur',
      },
    };
  }

  accessor animationState: 'entry'|'exit'|'none' = 'none';
  protected accessor showLoadingCircles_: boolean = false;
  protected accessor maxBlur_: MaxBlur|null = null;
  private loadingTimeout_: number|null = null;
  private entryTimeout_: number|null = null;
  private textOverlayTimeout_: number|null = null;
  private motionCompleteTimeout_: number|null = null;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    if (!changedProperties.has('animationState')) {
      return;
    }
    if (this.animationState === 'entry') {
      this.maxBlur_ = null;
    } else if (this.animationState === 'exit') {
      // The exit styles drop the max blur's fade-in, so that a later entry
      // restarts it. Read how far it got here: willUpdate() runs before
      // update() reflects animation-state, so the entry styles still apply.
      this.maxBlur_ = this.computeMaxBlurForExit_();
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);
    if (changedProperties.has('animationState')) {
      const oldState = changedProperties.get('animationState') as string;
      const newState = this.animationState;

      if (newState === 'entry' && oldState !== 'entry') {
        this.startTransitionAnimation_();
      } else if (newState === 'exit' && oldState !== 'exit') {
        this.startExitAnimation_();
      }
    }
  }

  protected onMaxBlurAnimationend_(e: AnimationEvent) {
    if (e.animationName === 'maxBlurFadeIn') {
      this.maxBlur_ = 'full';
    }
  }

  private startTransitionAnimation_() {
    // A regenerated image can start a new entry before the previous exit ends.
    if (this.motionCompleteTimeout_) {
      window.clearTimeout(this.motionCompleteTimeout_);
      this.motionCompleteTimeout_ = null;
    }

    this.calculateDimensions_();
    this.entryTimeout_ = window.setTimeout(() => {
      this.entryTimeout_ = null;
      this.fire('entry-complete');
    }, SWIPE_DURATION_MS);

    this.loadingTimeout_ = window.setTimeout(() => {
      this.loadingTimeout_ = null;
      this.showLoadingCircles_ = true;
    }, LOADING_CIRCLES_START_DELAY_MS);

    this.textOverlayTimeout_ = window.setTimeout(() => {
      this.textOverlayTimeout_ = null;
      this.$.textOverlay.startSequence();
    }, TEXT_OVERLAY_START_DELAY_MS);
  }

  private startExitAnimation_() {
    if (this.entryTimeout_) {
      window.clearTimeout(this.entryTimeout_);
      this.entryTimeout_ = null;
      this.fire('entry-complete');
    }

    if (this.loadingTimeout_) {
      window.clearTimeout(this.loadingTimeout_);
      this.loadingTimeout_ = null;
    }

    if (this.textOverlayTimeout_) {
      window.clearTimeout(this.textOverlayTimeout_);
      this.textOverlayTimeout_ = null;
    }

    this.$.textOverlay.stopSequence();

    this.showLoadingCircles_ = false;

    this.motionCompleteTimeout_ = window.setTimeout(() => {
      this.motionCompleteTimeout_ = null;
      this.fire('motion-complete');
    }, SWIPE_DURATION_MS);
  }

  private computeMaxBlurForExit_(): MaxBlur {
    if (this.maxBlur_ === 'full') {
      return 'full';
    }
    // There's no local DOM before the first render, and CrLitElement throws if
    // `$` is used within willUpdate() then.
    const opacity = this.hasUpdated ?
        Number(getComputedStyle(this.$.maxBlurLayer).opacity) :
        0;
    if (opacity <= MAX_BLUR_OPACITY_SNAP) {
      return 'hidden';
    }
    if (opacity >= 1 - MAX_BLUR_OPACITY_SNAP) {
      return 'full';
    }
    this.style.setProperty('--indigo-max-blur-opacity', `${opacity}`);
    return 'partial';
  }

  private calculateDimensions_() {
    const rect = this.getBoundingClientRect();
    const w = rect.width || window.innerWidth;
    const h = rect.height || window.innerHeight;
    const diagonal = Math.sqrt(w * w + h * h);

    const majorAxis = diagonal * 4;
    const minorAxis = majorAxis * 0.7;
    const borderWidth = (h + w) / 4;
    const gap = h * 0.7;

    this.style.setProperty('--major-axis', `${majorAxis}px`);
    this.style.setProperty('--minor-axis', `${minorAxis}px`);
    this.style.setProperty('--border-width', `${borderWidth}px`);
    this.style.setProperty('--gap', `${gap}px`);
    this.style.setProperty('--indigo-swipe-duration', `${SWIPE_DURATION_MS}ms`);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'indigo-motion-overlay': IndigoMotionOverlayElement;
  }
}

customElements.define(
    IndigoMotionOverlayElement.is, IndigoMotionOverlayElement);

// The blur layers' sweep mask is drawn by a paint worklet from --indigo-sweep
// (see sweep_mask_worklet.ts). The property is registered as a number so that
// animating it can run on the compositor thread.
CSS.registerProperty({
  name: '--indigo-sweep',
  syntax: '<number>',
  inherits: false,
  initialValue: '0',
});

// Resolves once the worklet has loaded. Don't start the entry animation before
// that, or #blurLayer shows unmasked until the worklet loads and the sweep runs
// on the main thread.
export const sweepMaskReady: Promise<void> =
    CSS.paintWorklet.addModule('sweep_mask_worklet.js');
