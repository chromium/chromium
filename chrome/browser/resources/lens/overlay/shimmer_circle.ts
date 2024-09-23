// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CURSOR_STATE_INITIAL_FOCUS_DURATION} from './overlay_shimmer.js';
import {getTemplate} from './shimmer_circle.html.js';
import {Wiggle} from './wiggle.js';

/** The frequency val used in the Wiggle functions. */
export const STEADY_STATE_FREQ_VAL = 0.06;
export const INTERACTION_STATE_FREQ_VAL = 0.03;

/*
 * Controls a single shimmer circle. This class is responsible for controlling
 * properties of an individual circle, that is different on each circle.
 * Shared properties are controlled by the OverlayShimmerElement.
 */
export class ShimmerCircleElement extends PolymerElement {
  static get is() {
    return 'shimmer-circle';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      colorHex: String,
      steadyStateCenterX: Number,
      steadyStateCenterY: Number,
      isSteadyState: {
        type: Boolean,
        observer: 'isSteadyStateChanged',
      },
      isWiggling: {
        type: Boolean,
        observer: 'isWigglingChanged',
      },
    };
  }

  // The color of this shimmer circle.
  private colorHex: string;
  // The randomized X value that this circle hovers around in the steady state.
  private steadyStateCenterX: number;
  // The randomized Y value that this circle hovers around in the steady state.
  private steadyStateCenterY: number;
  // Whether the circles are focusing the steady state center position.
  private isSteadyState: boolean;
  // Whether the circles are providing randomized movement.
  private isWiggling: boolean;
  // The animation transitioning the center positions to the steady state.
  // Undefined if no steady state animation has played yet.
  private steadyStateAnimation?: Animation;
  // The current time in our wiggle animation.
  private currentWiggleTime = 0;
  // The time in MS of the last requestAnimationFrame call
  private previousAnimationFrameTime = 0;

  // Wiggles.
  private radiusWiggle: Wiggle;
  private centerXWiggle: Wiggle;
  private centerYWiggle: Wiggle;

  constructor() {
    super();

    // Initialize the wiggles
    this.radiusWiggle = new Wiggle(STEADY_STATE_FREQ_VAL);
    this.centerXWiggle = new Wiggle(STEADY_STATE_FREQ_VAL);
    this.centerYWiggle = new Wiggle(STEADY_STATE_FREQ_VAL);
  }

  override connectedCallback() {
    super.connectedCallback();

    // Set this circles color.
    this.style.setProperty('--shimmer-circle-color', this.colorHex);

    // Begin following a randomized motion.
    this.stepRandomMotion(0);
  }

  private isSteadyStateChanged() {
    if (this.isSteadyState) {
      this.focusSteadyState();
    } else {
      this.unfocusSteadyState();
    }
  }

  private isWigglingChanged() {
    if (this.isWiggling) {
      this.startWiggles();
    }
  }

  private async focusSteadyState() {
    this.radiusWiggle.setFrequency(STEADY_STATE_FREQ_VAL);
    this.centerXWiggle.setFrequency(STEADY_STATE_FREQ_VAL);
    this.centerYWiggle.setFrequency(STEADY_STATE_FREQ_VAL);

    const animation = this.animate(
        [
          {
            [`--shimmer-circle-center-x`]: `${this.steadyStateCenterX}%`,
            [`--shimmer-circle-center-y`]: `${this.steadyStateCenterY}%`,
          },
        ],
        {
          duration: 800,
          easing: 'cubic-bezier(0.05, 0.7, 0.1, 1.0)',
          fill: 'forwards',
        });
    this.steadyStateAnimation = animation;
  }

  private unfocusSteadyState() {
    if (this.steadyStateAnimation) {
      // Cancel any current state state animations so they don't override the
      // new inherit properties.
      this.steadyStateAnimation.cancel();
    }

    this.radiusWiggle.setFrequency(INTERACTION_STATE_FREQ_VAL);
    this.centerXWiggle.setFrequency(INTERACTION_STATE_FREQ_VAL);
    this.centerYWiggle.setFrequency(INTERACTION_STATE_FREQ_VAL);

    // Unset the center point so the region to focus is controlled by
    // OverlayShimmerElement. We need to animate this if not there will be an
    // unsightly jump.
    this.animate(
        [
          {
            [`--shimmer-circle-center-x`]: `inherit`,
            [`--shimmer-circle-center-y`]: `inherit`,
          },
        ],
        {
          duration: CURSOR_STATE_INITIAL_FOCUS_DURATION,
          easing: 'cubic-bezier(0.2, 0.0, 0, 1.0)',
          fill: 'forwards',
        });
  }

  private startWiggles() {
    this.stepRandomMotion(this.currentWiggleTime);
  }

  private stepRandomMotion(timeMs: number) {
    this.currentWiggleTime += timeMs - this.previousAnimationFrameTime;
    this.previousAnimationFrameTime = timeMs;
    this.style.setProperty(
        '--shimmer-circle-radius-wiggle',
        this.radiusWiggle.calculateNext(timeMs / 1000).toString());
    this.style.setProperty(
        '--shimmer-circle-center-x-wiggle',
        this.centerXWiggle.calculateNext(timeMs / 1000).toString());
    this.style.setProperty(
        '--shimmer-circle-center-y-wiggle',
        this.centerYWiggle.calculateNext(timeMs / 1000).toString());
    // If not wiggling, stop requesting new animation frames.
    if (this.isWiggling) {
      requestAnimationFrame(this.stepRandomMotion.bind(this));
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'shimmer-circle': ShimmerCircleElement;
  }
}

customElements.define(ShimmerCircleElement.is, ShimmerCircleElement);
