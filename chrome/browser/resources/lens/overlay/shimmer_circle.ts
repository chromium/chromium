// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './shimmer_circle.html.js';
import {Wiggle} from './wiggle.js';

/** The frequency val used in the Wiggle functions. */
export const FREQ_VAL = 0.06;

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
    };
  }

  // The color of this shimmer circle.
  private colorHex: string;
  // The randomized X value that this circle hovers around in the steady state.
  private steadyStateCenterX: number;
  // The randomized Y value that this circle hovers around in the steady state.
  private steadyStateCenterY: number;

  // Wiggles.
  private radiusWiggle!: Wiggle;
  private centerXWiggle!: Wiggle;
  private centerYWiggle!: Wiggle;

  override connectedCallback() {
    super.connectedCallback();

    // Initialize the wiggles
    this.radiusWiggle = new Wiggle(FREQ_VAL);
    this.centerXWiggle = new Wiggle(FREQ_VAL);
    this.centerYWiggle = new Wiggle(FREQ_VAL);

    // Set this circles color.
    this.style.setProperty('--shimmer-circle-color', this.colorHex);


    // Start the invocation animation.
    this.startAnimation();

    // Begin following a randomized motion.
    this.stepRandomMotion(0);
  }

  private async startAnimation() {
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

    // Animation styles do not get automatically applied as CSS styles. Since
    // animation styles take precedence over CSS styles, leaving a finished
    // animation is bad practice as it can lead to weird issues where the CSS
    // property is not being applied due to a finished animation. Therefore, we
    // need to commit the styles and cleanup the animation.
    await animation.finished;
    animation.commitStyles();
    animation.cancel();
  }

  private stepRandomMotion(timeMs: number) {
    this.style.setProperty(
        '--shimmer-circle-radius-wiggle',
        this.radiusWiggle.calculateNext(timeMs / 1000).toString());
    this.style.setProperty(
        '--shimmer-circle-center-x-wiggle',
        this.centerXWiggle.calculateNext(timeMs / 1000).toString());
    this.style.setProperty(
        '--shimmer-circle-center-y-wiggle',
        this.centerYWiggle.calculateNext(timeMs / 1000).toString());
    requestAnimationFrame(this.stepRandomMotion.bind(this));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'shimmer-circle': ShimmerCircleElement;
  }
}

customElements.define(ShimmerCircleElement.is, ShimmerCircleElement);
