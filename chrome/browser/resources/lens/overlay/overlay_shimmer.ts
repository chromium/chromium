// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shimmer_circle.js';
import './strings.m.js';

import {assert, assertInstanceof, assertNotReached} from '//resources/js/assert.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {DomRepeat} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import {getFallbackTheme, getShaderLayerColorHexes} from './color_utils.js';
import type {OverlayTheme} from './lens.mojom-webui.js';
import {getTemplate} from './overlay_shimmer.html.js';
import type {OverlayShimmerFocusedRegion, OverlayShimmerUnfocusRegion} from './selection_utils.js';
import {ShimmerControlRequester} from './selection_utils.js';
import {toPercent} from './values_converter.js';

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
const STEADY_STATE_OPACITY_PERCENT = '30%';
const STEADY_STATE_RADIUS_PERCENT = '21%';
// The blur value is relative to the radius of the circle. A value of 2, doubles
// the circle radius to visually make the circle more blurred across the screen.
// The blur value should always be greater than 1.
const STEADY_STATE_CIRCLE_BLUR = '2';
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

// INTERACTION STATE CONSTANTS: These are the values that the circles will have
// the circles are interacting with the interaction the user is making (via
// their cursor, on a post selection bounding box, etc).
const INTERACTION_STATE_OPACITY_PERCENT = '40%';

// CURSOR STATE CONSTANTS: These are the values that are only applied when the
// shimmer is following the cursor.
// The cursor radius is relative to the size of the cursor icon.
const CURSOR_STATE_RADIUS_PERCENT = '80%';
// The blur value is relative to the radius of the circle. A value of 2, doubles
// the circle radius to visually make the circle more blurred across the screen.
// The blur value should always be greater than 1.
const CURSOR_STATE_CIRCLE_BLUR = '2.25';
// Amplitude is the amount amount of randomness that is applied to each value.
// For example, if the base radius is 10% and radius amplitude is 10%, the
// actual rendered radius can be between 0% and 20%.
const CURSOR_STATE_RADIUS_AMPLITUDE_PERCENT = '1.5%';
const CURSOR_STATE_CENTER_X_AMPLITUDE_PERCENT = '1.5%';
const CURSOR_STATE_CENTER_Y_AMPLITUDE_PERCENT = '1.5%';
// The time it takes in MS to transition from the steady state to the cursor
// state.
export const CURSOR_STATE_INITIAL_FOCUS_DURATION = 1000;
// The time it takes in MS to transition from any non steady state to the cursor
// state.
const CURSOR_STATE_FOCUS_DURATION = 750;

// REGION SELECTION STATE CONSTANTS: These are the values that are only applied
// when the shimmer is focusing on a selected region. In the region selection
// state, these values are in relation to the bounding box smallest size, rather
// than the entire viewport.
const REGION_SELECTION_STATE_RADIUS_PERCENT = '45%';
const REGION_SELECTION_STATE_CIRCLE_BLUR = '1.8';
const REGION_SELECTION_STATE_RADIUS_AMPLITUDE_PERCENT = '0%';
const REGION_SELECTION_STATE_CENTER_X_AMPLITUDE_PERCENT = '40%';
const REGION_SELECTION_STATE_CENTER_Y_AMPLITUDE_PERCENT = '40%';
// The time it takes in MS to transition from a different state to the  region
// selection state.
const REGION_SELECTION_STATE_FOCUS_DURATION = 750;

// The time (in MS) to wait between updating the sparkle seed.
const SPARKLE_MOTION_TIMEOUT_MS = 100;

export interface OverlayShimmerElement {
  $: {
    circlesContainer: DomRepeat,
  };
}

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
      isSteadyState: Boolean,
      isWiggling: Boolean,
      sparkleSeed: Number,
      enableSparkles: {
        type: Boolean,
        reflectToAttribute: true,
      },
      shaderLayerColorHexes: {
        type: Array,
        computed: 'computeShaderLayerColorHexes_(theme)',
      },
      theme: {
        type: Object,
        value: getFallbackTheme,
      },
    };
  }

  // The properties of circles currently being rendered.
  private circles: CircleProperties[] = [];
  // Whether the circles are in the steady state or not.
  private isSteadyState: boolean = true;
  // Whether the circles should be applying wiggle.
  private isWiggling: boolean = true;
  // The current seed of the sparkling noise.
  private sparkleSeed: number = 0;
  // Whether the sparkles are enabled or not.
  private enableSparkles: boolean =
      loadTimeData.getBoolean('enableShimmerSparkles');
  // The overlay theme.
  private theme: OverlayTheme;
  // Shader hex colors.
  private shaderLayerColorHexes: string[];

  // Event tracker for receiving DOM events.
  private eventTracker_: EventTracker = new EventTracker();
  // Stack the represents the current requesters for control. Once control is
  // relinquished, the next highest priority requester in the stack gains
  // control. If no one is in the stack, returns to steady state.
  private shimmerControllerStack: ShimmerControlRequester[] =
      [ShimmerControlRequester.NONE];
  // The last requester the created an animation. This might be different than
  // who currently has control, if control was relinquished between animations.
  private lastShimmerAnimator: ShimmerControlRequester =
      ShimmerControlRequester.NONE;
  // Used to put the shimmer back on the post selection after shimmer focuses on
  // another object, (like Segmentation).
  private previousPostSelection?: OverlayShimmerFocusedRegion;
  // Last animation that was triggered to play. Undefined if no animation has
  // triggered yet. The animation could be finished playing already.
  private lastAnimation?: Animation;
  // Whether the results are showing or not.
  private areResultsShowing: boolean = false;
  // The time stamp in MS of the last time the sparkles were animated.
  private lastSparkleTime: number = 0;

  // Listener ids for events from the browser side.
  private listenerIds: number[];


  override connectedCallback() {
    super.connectedCallback();
    this.eventTracker_.add(
        document, 'focus-region',
        (e: CustomEvent<OverlayShimmerFocusedRegion>) => {
          this.onFocusRegion(e);
        });
    this.eventTracker_.add(
        document, 'unfocus-region',
        (e: CustomEvent<OverlayShimmerUnfocusRegion>) => {
          this.onUnfocusRegion(e);
        });

    this.listenerIds = [
      BrowserProxyImpl.getInstance()
          .callbackRouter.notifyResultsPanelOpened.addListener(() => {
            this.areResultsShowing = true;
          }),
    ];
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
    this.listenerIds.forEach(
        id => assert(
            BrowserProxyImpl.getInstance().callbackRouter.removeListener(id)));
    this.listenerIds = [];
  }

  private computeShaderLayerColorHexes_() {
    return getShaderLayerColorHexes(this.theme);
  }

  async startAnimation() {
    // Set the invocation values.
    this.style.setProperty(
        '--shimmer-circle-radius', INVOCATION_RADIUS_PERCENT);
    this.style.setProperty(
        '--shimmer-circle-radius-amplitude',
        INVOCATION_RADIUS_AMPLITUDE_PERCENT);
    this.style.setProperty(
        '--shimmer-circle-center-x-amplitude',
        INVOCATION_CENTER_X_AMPLITUDE_PERCENT);
    this.style.setProperty(
        '--shimmer-circle-center-y-amplitude',
        INVOCATION_CENTER_Y_AMPLITUDE_PERCENT);
    this.style.opacity = INVOCATION_OPACITY_PERCENT;

    // Since each circles initial position is different, the
    // ShimmerCircleElement is responsible for animating to the steady state
    // center position. We set the initial value since that is shared between
    // all circles.
    this.style.setProperty(
        '--shimmer-circle-center-x', INVOCATION_CENTER_X_PERCENT);
    this.style.setProperty(
        '--shimmer-circle-center-y', INVOCATION_CENTER_Y_PERCENT);

    // Circle blur does not animate on invocation.
    this.style.setProperty('--shimmer-circle-blur', STEADY_STATE_CIRCLE_BLUR);

    // Start the random noise generation of the seed.
    this.startSparkles();

    // Allow the above styles to take effect.
    requestAnimationFrame(() => {
      const centerXOffsetInt = parseInt(STEADY_STATE_CENTER_X_PERCENT_OFFSET);
      const centerYOffsetInt = parseInt(STEADY_STATE_CENTER_Y_PERCENT_OFFSET);
      // Create a circle for each colorHex defined.
      this.circles = this.shaderLayerColorHexes.map((colorHex: string) => {
        return {
          colorHex,
          steadyStateCenterX: 50 - centerXOffsetInt * (Math.random() * 2 - 1),
          steadyStateCenterY: 50 - centerYOffsetInt * (Math.random() * 2 - 1),
        };
      });

      // Animate to the steady state.
      this.transitionToSteadyState();
    });
  }

  private startSparkles() {
    if (this.enableSparkles) {
      this.updateSparkles(0);
    }
  }

  private updateSparkles(time: number) {
    const delta = time - this.lastSparkleTime;
    if (delta > SPARKLE_MOTION_TIMEOUT_MS) {
      this.lastSparkleTime = time;
      this.sparkleSeed = Math.floor(Math.random() * 10);
    }

    requestAnimationFrame(this.updateSparkles.bind(this));
  }

  private onFocusRegion(e: CustomEvent<OverlayShimmerFocusedRegion>) {
    const centerX = e.detail.left + e.detail.width / 2;
    const centerY = e.detail.top + e.detail.height / 2;

    // Ignore invalid regions.
    if (centerX <= 0 || centerY <= 0) {
      return;
    }

    // Save the post selection in case we need to move the shimmer back to it.
    if (e.detail.requester === ShimmerControlRequester.POST_SELECTION) {
      // Include the post selection into the stack if it is not already present.
      if (!this.shimmerControllerStack.includes(
              ShimmerControlRequester.POST_SELECTION)) {
        this.shimmerControllerStack.push(
            ShimmerControlRequester.POST_SELECTION);
        this.shimmerControllerStack.sort();
      }
      this.previousPostSelection = e.detail;
    }

    this.focusRegion(
        centerX, centerY, e.detail.width, e.detail.height, e.detail.requester);
  }

  private onUnfocusRegion(e: CustomEvent<OverlayShimmerUnfocusRegion>) {
    const index = this.shimmerControllerStack.indexOf(e.detail.requester);
    // Only relinquish control if the requester currently has control.
    if (index === -1) {
      return;
    }
    // Remove the control requester from the stack.
    this.shimmerControllerStack.splice(index, 1);

    const newCurrentController = this.getCurrentShimmerController();
    if (newCurrentController === ShimmerControlRequester.POST_SELECTION &&
        this.previousPostSelection) {
      // Target the shimmer back to the post selection.
      const centerX = this.previousPostSelection.left +
          this.previousPostSelection.width / 2;
      const centerY = this.previousPostSelection.top +
          this.previousPostSelection.height / 2;
      this.focusRegion(
          centerX, centerY, this.previousPostSelection.width,
          this.previousPostSelection.height,
          this.previousPostSelection.requester);
    } else if (newCurrentController === ShimmerControlRequester.NONE) {
      if (!this.areResultsShowing) {
        this.transitionToSteadyState();
      } else {
        // Hide shimmer if user focusing on results.
        this.animate(
            [
              {
                opacity: '0%',
              },
            ],
            {
              duration: 150,
              easing: 'linear',
              fill: 'forwards',
            });
      }
    }
  }

  private async transitionToSteadyState() {
    this.isSteadyState = true;
    this.lastShimmerAnimator = ShimmerControlRequester.NONE;
    // Animate in the opacity
    this.animate(
        [
          {
            opacity: STEADY_STATE_OPACITY_PERCENT,
          },
        ],
        {
          duration: 150,
          easing: 'linear',
          fill: 'forwards',
        });
    this.lastAnimation = this.animate(
        [
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
  }

  // Focuses the shimmer on a specific region of the screen. The inputted values
  // should be percentage values between 0-1 representing the region to focus.
  private async focusRegion(
      centerX: number, centerY: number, width: number, height: number,
      requester: ShimmerControlRequester) {
    // Ignore if the shimmering hasn't been triggered yet.
    if (!this.lastAnimation) {
      return;
    }

    const currentShimmerController = this.getCurrentShimmerController();
    if (currentShimmerController > requester) {
      // Ignore this request because the current controller has a higher
      // priority than the requester.
      return;
    } else if (currentShimmerController < requester) {
      this.shimmerControllerStack.push(requester);
    }

    const smallestLength = Math.min(width, height);
    const circleCenterX: string = toPercent(centerX);
    const circleCenterY: string = toPercent(centerY);
    let circleRadius: string;
    let circleBlur: string;
    let circleRadiusAmp: string;
    let circleCenterXAmp: string;
    let circleCenterYAmp: string;
    let duration: number;

    switch (requester) {
      case ShimmerControlRequester.SEGMENTATION:
        // Intended fall through since SEGMENTATION AND POST_SELECTION follow
        // the same values.
      case ShimmerControlRequester.POST_SELECTION:
        // Intended fall through since MANUAL_REGION AND POST_SELECTION follow
        // the same values.
      case ShimmerControlRequester.MANUAL_REGION:
        circleRadius = toPercent(
            parseInt(REGION_SELECTION_STATE_RADIUS_PERCENT) / 100 *
            smallestLength);
        circleBlur = REGION_SELECTION_STATE_CIRCLE_BLUR;
        circleRadiusAmp = toPercent(
            parseInt(REGION_SELECTION_STATE_RADIUS_AMPLITUDE_PERCENT) / 100 *
            smallestLength);
        circleCenterXAmp = toPercent(
            parseInt(REGION_SELECTION_STATE_CENTER_X_AMPLITUDE_PERCENT) / 100 *
            width);
        circleCenterYAmp = toPercent(
            parseInt(REGION_SELECTION_STATE_CENTER_Y_AMPLITUDE_PERCENT) / 100 *
            height);
        duration = REGION_SELECTION_STATE_FOCUS_DURATION;
        break;
      case ShimmerControlRequester.CURSOR:
        circleRadius = toPercent(
            parseInt(CURSOR_STATE_RADIUS_PERCENT) / 100 * smallestLength);
        circleBlur = CURSOR_STATE_CIRCLE_BLUR;
        circleRadiusAmp = CURSOR_STATE_RADIUS_AMPLITUDE_PERCENT;
        circleCenterXAmp = CURSOR_STATE_CENTER_X_AMPLITUDE_PERCENT;
        circleCenterYAmp = CURSOR_STATE_CENTER_Y_AMPLITUDE_PERCENT;
        // If we aren't already following the cursor, it should take time to get
        // to the cursor.
        if (this.lastShimmerAnimator !== ShimmerControlRequester.CURSOR) {
          duration = this.isSteadyState ? CURSOR_STATE_INITIAL_FOCUS_DURATION :
                                          CURSOR_STATE_FOCUS_DURATION;
          break;
        }
        // If there is currently a text animation, we should modify the center
        // position to be the new mouse position, and return so the animation
        // automatically catches up to the cursor even if it is moving.
        if (this.lastAnimation.playState === 'running') {
          const keyframeEffect = this.lastAnimation.effect;
          assertInstanceof(keyframeEffect, KeyframeEffect);

          // Replace the old center values in the animation with the new ones.
          const newKeyFrames = keyframeEffect.getKeyframes();
          newKeyFrames[0][`--shimmer-circle-center-x`] = circleCenterX;
          newKeyFrames[0][`--shimmer-circle-center-y`] = circleCenterY;
          keyframeEffect.setKeyframes(newKeyFrames);
          return;
        }
        // There is no active animation, and we aren't transitioning states, so
        // the duration should be 0
        duration = 0;
        break;
      default:
        assertNotReached();
    }

    // We should only stop wiggling in the post selection state.
    if (requester === ShimmerControlRequester.POST_SELECTION) {
      this.isWiggling = false;
    } else {
      this.isWiggling = true;
    }

    if (this.isSteadyState) {
      // On the initial transition from steady state to interaction state,
      // each shimmer circle sets their center coordinates to 'inherit'. We
      // need to specify a value to inherit from, if not the transition will be
      // instant and look bad.
      this.style.setProperty(`--shimmer-circle-center-x`, circleCenterX);
      this.style.setProperty(`--shimmer-circle-center-y`, circleCenterY);
    }

    this.lastShimmerAnimator = requester;
    this.isSteadyState = false;

    // We only need to specify the final values we want. The browser is smart
    // enough to interpret the initial values as the values already set.
    this.lastAnimation = this.animate(
        [
          {
            [`--shimmer-circle-radius`]: circleRadius,
            [`--shimmer-circle-blur`]: circleBlur,
            [`--shimmer-circle-center-x`]: circleCenterX,
            [`--shimmer-circle-center-y`]: circleCenterY,
            [`--shimmer-circle-radius-amplitude`]: circleRadiusAmp,
            [`--shimmer-circle-center-x-amplitude`]: circleCenterXAmp,
            [`--shimmer-circle-center-y-amplitude`]: circleCenterYAmp,
          },
        ],
        {
          duration,
          easing: 'cubic-bezier(0.2, 0.0, 0, 1.0)',
          fill: 'forwards',
        });
    this.animate(
        [
          {
            opacity: INTERACTION_STATE_OPACITY_PERCENT,
          },
        ],
        {
          duration: 150,
          easing: 'linear',
          fill: 'forwards',
        });
  }

  private getCurrentShimmerController(): ShimmerControlRequester {
    return this.shimmerControllerStack[this.shimmerControllerStack.length - 1];
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
  name: '--shimmer-circle-blur',
  syntax: '<number>',
  inherits: true,
  initialValue: '0',
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
