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

export interface IndigoMotionOverlayElement {
  $: {
    blurLayer: HTMLElement,
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
    };
  }

  accessor animationState: 'entry'|'exit'|'none' = 'none';
  protected accessor showLoadingCircles_: boolean = false;
  private loadingTimeout_: number|null = null;

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

  private startTransitionAnimation_() {
    this.calculateDimensions_();
    this.loadingTimeout_ = window.setTimeout(() => {
      this.showLoadingCircles_ = true;
    }, LOADING_CIRCLES_START_DELAY_MS);

    window.setTimeout(() => {
      this.$.textOverlay.startSequence();
    }, TEXT_OVERLAY_START_DELAY_MS);
  }

  private startExitAnimation_() {
    if (this.loadingTimeout_) {
      window.clearTimeout(this.loadingTimeout_);
    }

    this.$.textOverlay.stopSequence();

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

    window.setTimeout(() => {
      this.fire('motion-complete');
    }, SWIPE_DURATION_MS);
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
