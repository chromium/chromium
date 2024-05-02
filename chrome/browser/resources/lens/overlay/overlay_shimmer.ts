// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shimmer_circle.js';

import {assert} from '//resources/js/assert.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './overlay_shimmer.html.js';

interface CircleProperties {
  // The HEX value to color the circle.
  colorHex: string;
  // A number between 0-100 that represents the x position of this circles
  // steady state, relative to the parent rect.
  steadyStateCenterX: number;
  // A number between 0-100 that represents the y position of this circles
  // steady state, relative to the parent rect.
  steadyStateCenterY: number;
}

// The colors of the different circles that compose the shimmer. One circle
// will be generated for each color hex in this array.
export const COLOR_HEXES = [
  '#88A9FF',
  '#E5EDFF',
  '#FDACEE',
  '#5B8CFF',
  '#0B57D0',
];

// INVOCATION CONSTANTS: These are the values that the circles will have on
// the initial invocation, which then transition to the steady state constants.
const INVOCATION_OPACITY_PERCENT = '0%';
const INVOCATION_RADIUS_PERCENT = '0%';
const INVOCATION_CENTER_X_PERCENT = '50%';
const INVOCATION_CENTER_Y_PERCENT = '0%';
// Amplitude is the amount amount of randomness that is applied to each value.
// For example, if the base radius is 10% and radius amplitude is 10%, the
// actual rendered radius can be between 0% and 20%.
const INVOCATION_RADIUS_AMPLITUDE_PERCENT = '0%';
const INVOCATION_CENTER_X_AMPLITUDE_PERCENT = '0%';
const INVOCATION_CENTER_Y_AMPLITUDE_PERCENT = '0%';

// STEADY STATE CONSTANTS: These are the values that the circles will have when
// no other state is being applied.
const STEADY_STATE_OPACITY_PERCENT = '60%';
const STEADY_STATE_RADIUS_PERCENT = '21%';
// All circles Wiggle around a different position in the steady state. These
// offset control how far from the center the randomized position can be. For
// example, a 10% offset on the X axis means the X value of the randomized
// position can be between 40% and 60%.
const STEADY_STATE_CENTER_X_PERCENT_OFFSET = '50%';
const STEADY_STATE_CENTER_Y_PERCENT_OFFSET = '30%';
// Amplitude is the amount amount of randomness that is applied to each value.
// For example, if the base radius is 10% and radius amplitude is 10%, the
// actual rendered radius can be between 0% and 20%.
const STEADY_STATE_RADIUS_AMPLITUDE_PERCENT = '0%';
const STEADY_STATE_CENTER_X_AMPLITUDE_PERCENT = '21%';
const STEADY_STATE_CENTER_Y_AMPLITUDE_PERCENT = '21%';

/*
 * Controls the shimmer overlaid on the selection elements. The shimmer is
 * composed of multiple circles, each following a randomized pattern. Some
 * properties like the randomized movement, are per circle and controlled by
 * each circle, not this class. This class is responsible for controlling
 * behavior shared by each circle, like their general positioning and opacity.
 */
export class OverlayShimmerElement extends PolymerElement {
  static get is() {
    return 'overlay-shimmer';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      circles: Array,
    };
  }

  // The properties of circles currently being rendered.
  private circles: CircleProperties[] = [];
  private resizeObserver: ResizeObserver =
      new ResizeObserver((entries: ResizeObserverEntry[]) => {
        assert(entries.length === 1);
        this.handleResize(entries[0]);
      });

  override connectedCallback() {
    super.connectedCallback();
    this.resizeObserver.observe(this);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.resizeObserver.unobserve(this);
  }

  private handleResize(shimmerEntry: ResizeObserverEntry) {
    // Ignore if the parent has no rect bounds.
    if (shimmerEntry.contentRect.width * shimmerEntry.contentRect.height <= 0) {
      return;
    }

    this.resizeObserver.unobserve(this);
    this.startAnimation();
  }

  private async startAnimation() {
    const centerXOffsetInt = parseInt(STEADY_STATE_CENTER_X_PERCENT_OFFSET);
    const centerYOffsetInt = parseInt(STEADY_STATE_CENTER_Y_PERCENT_OFFSET);
    // Create a circle for each colorHex defined.
    this.circles = COLOR_HEXES.map((colorHex: string) => {
      return {
        colorHex,
        steadyStateCenterX: 50 - centerXOffsetInt * (Math.random() * 2 - 1),
        steadyStateCenterY: 50 - centerYOffsetInt * (Math.random() * 2 - 1),
      };
    });

    // Since each circles initial position is different, the
    // ShimmerCircleElement is responsible for animating to the steady state
    // center position. We set the initial value since that is shared between
    // all circles.
    this.style.setProperty(
        '--shimmer-circle-center-x', INVOCATION_CENTER_X_PERCENT);
    this.style.setProperty(
        '--shimmer-circle-center-y', INVOCATION_CENTER_Y_PERCENT);

    // Animate in the opacity
    const opacityAnimation = this.animate(
        [
          {
            opacity: INVOCATION_OPACITY_PERCENT,
          },
          {
            opacity: STEADY_STATE_OPACITY_PERCENT,
          },
        ],
        {
          duration: 150,
          easing: 'linear',
          fill: 'forwards',
        });

    // Animate the invocation values.
    const invocationAnimation = this.animate(
        [
          {
            [`--shimmer-circle-radius`]: INVOCATION_RADIUS_PERCENT,
            [`--shimmer-circle-radius-amplitude`]:
                INVOCATION_RADIUS_AMPLITUDE_PERCENT,
            [`--shimmer-circle-center-x-amplitude`]:
                INVOCATION_CENTER_X_AMPLITUDE_PERCENT,
            [`--shimmer-circle-center-y-amplitude`]:
                INVOCATION_CENTER_Y_AMPLITUDE_PERCENT,
          },
          {
            [`--shimmer-circle-radius`]: STEADY_STATE_RADIUS_PERCENT,
            [`--shimmer-circle-radius-amplitude`]:
                STEADY_STATE_RADIUS_AMPLITUDE_PERCENT,
            [`--shimmer-circle-center-x-amplitude`]:
                STEADY_STATE_CENTER_X_AMPLITUDE_PERCENT,
            [`--shimmer-circle-center-y-amplitude`]:
                STEADY_STATE_CENTER_Y_AMPLITUDE_PERCENT,
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
    await opacityAnimation.finished;
    opacityAnimation.commitStyles();
    opacityAnimation.cancel();
    await invocationAnimation.finished;
    invocationAnimation.commitStyles();
    invocationAnimation.cancel();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'overlay-shimmer': OverlayShimmerElement;
  }
}

customElements.define(OverlayShimmerElement.is, OverlayShimmerElement);

// Setup CSS Houdini API.
CSS.paintWorklet.addModule('shimmer_circle_paint_worklet.js');

// Variables controlling the shimmer circles.
CSS.registerProperty({
  name: '--shimmer-circle-color',
  syntax: '<color>',
  inherits: false,
  initialValue: 'transparent',
});

CSS.registerProperty({
  name: '--shimmer-circle-opacity',
  syntax: '<percentage>',
  inherits: true,
  initialValue: '0%',
});

CSS.registerProperty({
  name: '--shimmer-circle-radius',
  syntax: '<percentage>',
  inherits: true,
  initialValue: '0%',
});

CSS.registerProperty({
  name: '--shimmer-circle-center-x',
  syntax: '<percentage>',
  inherits: true,
  initialValue: '0%',
});

CSS.registerProperty({
  name: '--shimmer-circle-center-y',
  syntax: '<percentage>',
  inherits: true,
  initialValue: '0%',
});

CSS.registerProperty({
  name: '--shimmer-circle-radius-amplitude',
  syntax: '<percentage>',
  inherits: true,
  initialValue: '0%',
});

CSS.registerProperty({
  name: '--shimmer-circle-center-x-amplitude',
  syntax: '<percentage>',
  inherits: true,
  initialValue: '0%',
});

CSS.registerProperty({
  name: '--shimmer-circle-center-y-amplitude',
  syntax: '<percentage>',
  inherits: true,
  initialValue: '0%',
});

CSS.registerProperty({
  name: '--shimmer-circle-radius-wiggle',
  syntax: '<number>',
  inherits: false,
  initialValue: '0',
});

CSS.registerProperty({
  name: '--shimmer-circle-center-x-wiggle',
  syntax: '<number>',
  inherits: false,
  initialValue: '0',
});

CSS.registerProperty({
  name: '--shimmer-circle-center-y-wiggle',
  syntax: '<number>',
  inherits: false,
  initialValue: '0',
});
