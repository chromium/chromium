// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './motion_overlay.css.js';
import {getHtml} from './motion_overlay.html.js';

const EXIT_ANIMATION_DELAY_MS: number = 8000;
const LOADING_CIRCLES_START_DELAY_MS: number = 1300;
const FINISHED_TIMEOUT_MS: number = 2000;

export interface IndigoMotionOverlayElement {
  $: {
    blurLayer: HTMLElement,
    swipeEllipse1: HTMLElement,
    swipeEllipse2: HTMLElement,
    loadingCircleDark: HTMLElement,
    loadingCircleLight: HTMLElement,
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
      animationState_: {
        type: String,
        reflect: true,
        attribute: 'animation-state',
      },
      showLoadingCircles_: {
        type: Boolean,
        reflect: true,
        attribute: 'show-loading-circles',
      },
    };
  }

  protected accessor animationState_: 'entry'|'exit'|'none' = 'none';
  protected accessor showLoadingCircles_: boolean = false;

  private loadingTimeout_: number|null = null;

  override firstUpdated() {
    this.startTransitionAnimation_();
  }

  private startTransitionAnimation_() {
    this.calculateDimensions_();

    this.animationState_ = 'entry';
    this.loadingTimeout_ = window.setTimeout(() => {
      this.showLoadingCircles_ = true;
    }, LOADING_CIRCLES_START_DELAY_MS);

    // TODO(b/486887445): Remove the hardcoded duration once there is a way
    // to receive the transformed image.
    window.setTimeout(() => {
      this.startExitAnimation_();
    }, EXIT_ANIMATION_DELAY_MS);
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
  }

  private startExitAnimation_() {
    if (this.loadingTimeout_) {
      window.clearTimeout(this.loadingTimeout_);
    }

    // This is to handle a scenario where the exit animation is started before
    // the entry animation has completed. In this case, we want the exit
    // animation to start from the entry animation's last keyframe's opacity
    // value.
    const blurLayer = this.$.blurLayer;
    if (blurLayer) {
      const computedStyle = window.getComputedStyle(blurLayer);
      const currentBlur = computedStyle.getPropertyValue('backdrop-filter');
      if (currentBlur && currentBlur !== 'none') {
        blurLayer.style.setProperty('backdrop-filter', currentBlur);
      }
    }

    this.showLoadingCircles_ = false;
    this.animationState_ = 'exit';

    window.setTimeout(() => {
      this.animationState_ = 'none';
      this.fire('motion-complete');
    }, FINISHED_TIMEOUT_MS);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'indigo-motion-overlay': IndigoMotionOverlayElement;
  }
}

customElements.define(
    IndigoMotionOverlayElement.is, IndigoMotionOverlayElement);
