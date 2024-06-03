// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from '//resources/js/assert.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import {getFallbackTheme, getShaderLayerColorRgbas, modifyRgbaTransparency} from './color_utils.js';
import {CubicBezier} from './cubic_bezier.js';
import type {OverlayTheme} from './lens.mojom-webui.js';
import {getTemplate} from './overlay_shimmer_canvas.html.js';
import type {OverlayShimmerFocusedRegion, OverlayShimmerUnfocusRegion, Point} from './selection_utils.js';
import {ShimmerControlRequester} from './selection_utils.js';
import {Wiggle} from './wiggle.js';

// The frequency val used in the Wiggle functions.
const STEADY_STATE_FREQ_VAL = 0.06;
const INTERACTION_STATE_FREQ_VAL = 0.03;

// INVOCATION CONSTANTS: These are the values that the circles will have on
// the initial invocation, which then transition to the steady state
// constants.
const INVOCATION_OPACITY_PERCENT = 0;
const INVOCATION_RADIUS_PERCENT = 0;
const INVOCATION_CENTER_X_PERCENT = 50;
const INVOCATION_CENTER_Y_PERCENT = 0;
// Amplitude is the amount amount of randomness that is applied to each value.
// For example, if the base radius is 10% and radius amplitude is 10%, the
// actual rendered radius can be between 0% and 20%.
const INVOCATION_RADIUS_AMPLITUDE_PERCENT = 0;
const INVOCATION_CENTER_X_AMPLITUDE_PERCENT = 0;
const INVOCATION_CENTER_Y_AMPLITUDE_PERCENT = 0;

// STEADY STATE CONSTANTS: These are the values that the circles will have when
// no other state is being applied.
const STEADY_STATE_OPACITY_PERCENT = 0.3;
const STEADY_STATE_RADIUS_PERCENT = 21;
// The blur value is relative to the radius of the circle. A value of 2, doubles
// the circle radius to visually make the circle more blurred across the screen.
// The blur value should always be greater than 1.
const STEADY_STATE_CIRCLE_BLUR = 2;
// All circles Wiggle around a different position in the steady state. These
// offset control how far from the center the randomized position can be. For
// example, a 10% offset on the X axis means the X value of the randomized
// position can be between 40% and 60%.
const STEADY_STATE_CENTER_X_PERCENT_OFFSET = 50;
const STEADY_STATE_CENTER_Y_PERCENT_OFFSET = 30;
// Amplitude is the amount amount of randomness that is applied to each value.
// For example, if the base radius is 10% and radius amplitude is 10%, the
// actual rendered radius can be between 0% and 20%.
const STEADY_STATE_RADIUS_AMPLITUDE_PERCENT = 0;
const STEADY_STATE_CENTER_X_AMPLITUDE_PERCENT = 21;
const STEADY_STATE_CENTER_Y_AMPLITUDE_PERCENT = 21;
const STEADY_STATE_TRANSITION_DURATION = 800;
const STEADY_STATE_EASING_FUNCTION = new CubicBezier(0.05, 0.7, 0.1, 1.0);

// INTERACTION STATE CONSTANTS: These are the values that the circles will have
// the circles are interacting with the interaction the user is making (via
// their cursor, on a post selection bounding box, etc).
const INTERACTION_STATE_OPACITY_PERCENT = 0.4;
const INTERACTION_STATE_EASING_FUNCTION = new CubicBezier(0.2, 0.0, 0.0, 1.0);

// CURSOR STATE CONSTANTS: These are the values that are only applied when the
// shimmer is following the cursor.
// The cursor radius is relative to the size of the cursor icon.
const CURSOR_STATE_RADIUS_PERCENT = 0;
// Amplitude is the amount amount of randomness that is applied to each value.
// For example, if the base radius is 10% and radius amplitude is 10%, the
// actual rendered radius can be between 0% and 20%.
const CURSOR_STATE_RADIUS_AMPLITUDE_PERCENT = 0;
const CURSOR_STATE_CENTER_X_AMPLITUDE_PERCENT = 0;
const CURSOR_STATE_CENTER_Y_AMPLITUDE_PERCENT = 0;
// The time it takes in MS to transition to the cursor state.
const CURSOR_STATE_TRANSITION_DURATION = 1000;
// The time it takes in MS to transition the shimmer circles to overlay one
// another rather than follow their wiggles.
const CURSOR_STATE_INITIAL_FOCUS_DURATION = 750;

// REGION SELECTION STATE CONSTANTS: These are the values that are only applied
// when the shimmer is focusing on a selected region. In the region selection
// state, these values are in relation to the bounding box smallest size, rather
// than the entire viewport.
const REGION_SELECTION_STATE_RADIUS_PERCENT = 45;
const REGION_SELECTION_STATE_CIRCLE_BLUR = 1.8;
const REGION_SELECTION_STATE_RADIUS_AMPLITUDE_PERCENT = 0;
const REGION_SELECTION_STATE_CENTER_X_AMPLITUDE_PERCENT = 40;
const REGION_SELECTION_STATE_CENTER_Y_AMPLITUDE_PERCENT = 40;
// The time it takes in MS to transition from a different state to the region
// selection state.
const REGION_SELECTION_TRANSITION_DURATION = 750;

// Specifies the current animation state of the shader canvas.
enum ShimmerState {
  NONE = 0,
  INVOCATION = 1,
  TRANSITION_TO_STEADY_STATE = 2,
  STEADY_STATE = 3,
  TRANSITION_TO_CURSOR = 4,
  CURSOR = 5,
  TRANSITION_TO_REGION = 6,
  REGION = 7,
}

// An interface representing the current values of a circle on the canvas.
interface ShimmerCircle {
  // The rgba value to color the circle.
  colorRgba: string;
  // A point with values between 0-100 that represents the (x,y) position of
  // this circles steady state, relative to the parent rect.
  steadyStateCenter: Point;
  // The current blur of the circle.
  blur: number;
  // The current values of the circle. The are values between 0-1 and
  // represents a percentage of the canvas size.
  radius: number;
  center: Point;
  // These are the amplitudes that should be applied to each wiggle for the
  // corresponding attribute.
  centerXAmpPercent: number;
  centerYAmpPercent: number;
  radiusAmpPercent: number;
  // The wiggles for each circle. This is a random noise generator so each
  // circle can simulate its own wiggle.
  radiusWiggle: Wiggle;
  centerXWiggle: Wiggle;
  centerYWiggle: Wiggle;
}

// An interface representing a shimmer animation with keyframes for its start
// and ending attributes.
interface ShimmerAnimation {
  startKeyframe: ShimmerAnimationKeyframe;
  endKeyframe: ShimmerAnimationKeyframe;
}

// An interface representing a keyframe or starting/ending position of the
// shimmer during an animation.
interface ShimmerAnimationKeyframe {
  blur: number;
  radius: number;
  center: Point;
  centerXAmpPercent: number;
  centerYAmpPercent: number;
  radiusAmpPercent: number;
  radiusWiggleValue: number;
  centerXWiggleValue: number;
  centerYWiggleValue: number;
  opacity: number;
}

/** Function for performing linear interpolation. */
function lerp(a: number, b: number, x: number): number {
  x = Math.max(0, Math.min(1, x));
  return a + (b - a) * x;
}

/**
 * Function for creating a radial gradient from the provided input parameters.
 */
function createCircleGradient(
    ctx: OffscreenCanvasRenderingContext2D, centerX: number, centerY: number,
    radius: number, colorRgba: string): CanvasGradient {
  // Centered radial gradient.
  const radialGradient = ctx.createRadialGradient(
      centerX,
      centerY,
      0,
      centerX,
      centerY,
      radius,
  );
  // Simulate a Gaussian full page blur by mimicking a smooth step alpha
  // change through the circle radius.
  radialGradient.addColorStop(0, modifyRgbaTransparency(colorRgba, 1));
  radialGradient.addColorStop(0.125, modifyRgbaTransparency(colorRgba, 0.957));
  radialGradient.addColorStop(0.25, modifyRgbaTransparency(colorRgba, 0.84375));
  radialGradient.addColorStop(
      0.375, modifyRgbaTransparency(colorRgba, 0.68359));
  radialGradient.addColorStop(0.5, modifyRgbaTransparency(colorRgba, 0.5));
  radialGradient.addColorStop(0.625, modifyRgbaTransparency(colorRgba, 0.3164));
  radialGradient.addColorStop(0.75, modifyRgbaTransparency(colorRgba, 0.15625));
  radialGradient.addColorStop(
      0.875, modifyRgbaTransparency(colorRgba, 0.04297));
  radialGradient.addColorStop(1, modifyRgbaTransparency(colorRgba, 0));
  return radialGradient;
}

export interface OverlayShimmerCanvasElement {
  $: {
    shaderCanvas: HTMLCanvasElement,
  };
}

/*
 * Controls the shimmer overlaid on the selection elements. The shimmer is
 * composed of multiple circles, each following a randomized pattern. Some
 * properties like the randomized movement, are per circle and controlled by
 * each circle, not this class. This class is responsible for controlling
 * behavior shared by each circle, like their general positioning and opacity.
 */
export class OverlayShimmerCanvasElement extends PolymerElement {
  static get is() {
    return 'overlay-shimmer-canvas';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      canvasHeight: Number,
      canvasWidth: Number,
      shaderLayerRgbaColors: {
        type: Array,
        computed: 'computeShaderLayerColorRgbas(theme)',
      },
      theme: {
        type: Object,
        value: () => getFallbackTheme(),
      },
    };
  }


  // Canvas height property for setting the pixel height of the canvas element.
  private canvasHeight: number;
  // Canvas width property for setting the pixel width of the canvas element.
  private canvasWidth: number;
  // Shader rgba colors.
  private shaderLayerRgbaColors: string[];
  // The overlay theme.
  private theme: OverlayTheme;

  // The properties of circles currently being rendered.
  private circles: ShimmerCircle[] = [];
  // Whether the circles should be applying wiggle.
  private isWiggling: boolean = true;
  // Event tracker for receiving DOM events.
  private eventTracker_: EventTracker = new EventTracker();
  // Stack that represents the current requesters for control. Once control is
  // relinquished, the next highest priority requester in the stack gains
  // control. If no one is in the stack, returns to steady state.
  private shimmerControllerStack: ShimmerControlRequester[] =
      [ShimmerControlRequester.NONE];
  // Used to put the shimmer back on the post selection after shimmer focuses on
  // another object, (like Segmentation).
  private previousPostSelection?: OverlayShimmerFocusedRegion;
  // Whether the results are showing or not.
  private areResultsShowing: boolean = false;

  // Listener ids for events from the browser side.
  private listenerIds: number[];

  // The height of the canvas taking into account device pixel ratio. Used to
  // resize the canvas on the next draw instead of immediately.
  private canvasPhysicalHeight: number;
  // The width of the canvas taking into account device pixel ratio. Used to
  // resize the canvas on the next draw instead of immediately.
  private canvasPhysicalWidth: number;
  private canvas: OffscreenCanvas;
  private context: OffscreenCanvasRenderingContext2D;
  // A list of animations for each shimmer circle. There should be one object
  // per circle and the list should be ordered identically to |circles|.
  private shimmerAnimation: ShimmerAnimation[] = [];

  // The last received cursor values that requested focusing the shimmer.
  private cursorCenter: Point;
  // The last received region values that requested focusing the shimmer.
  private regionCenter: Point;
  private regionWidth: number;
  private regionHeight: number;

  // The current shimmer state.
  private shimmerState: ShimmerState = ShimmerState.NONE;
  // The start time of the current animation. When undefined, it will be set at
  // next draw if there is an animiation needed.
  private animationStartTime?: number = undefined;

  override ready() {
    super.ready();

    this.canvas = this.$.shaderCanvas.transferControlToOffscreen();
    this.context = this.canvas.getContext('2d')!;
  }

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

  /** Starts the invocation animation into the steady state. */
  startInvocationAnimation() {
    // Draw invocation state.
    this.context.globalAlpha = INVOCATION_OPACITY_PERCENT;
    this.shimmerState = ShimmerState.INVOCATION;

    // Create a circle for each color rgb string defined. We do this when the
    // invocation animation is started to make sure we grab the latest set
    // overlay theme.
    const centerXOffsetInt = STEADY_STATE_CENTER_X_PERCENT_OFFSET;
    const centerYOffsetInt = STEADY_STATE_CENTER_Y_PERCENT_OFFSET;
    this.circles = this.shaderLayerRgbaColors.map((colorRgbaString: string) => {
      return {
        colorRgba: colorRgbaString,
        steadyStateCenter: {
          x: 50 - centerXOffsetInt * (Math.random() * 2 - 1),
          y: 50 - centerYOffsetInt * (Math.random() * 2 - 1),
        },
        blur: STEADY_STATE_CIRCLE_BLUR,
        radius: INVOCATION_RADIUS_PERCENT,
        center:
            {x: INVOCATION_CENTER_X_PERCENT, y: INVOCATION_CENTER_Y_PERCENT},
        centerXAmpPercent: INVOCATION_CENTER_X_AMPLITUDE_PERCENT,
        centerYAmpPercent: INVOCATION_CENTER_Y_AMPLITUDE_PERCENT,
        radiusAmpPercent: INVOCATION_RADIUS_AMPLITUDE_PERCENT,
        radiusWiggle: new Wiggle(STEADY_STATE_FREQ_VAL),
        centerXWiggle: new Wiggle(STEADY_STATE_FREQ_VAL),
        centerYWiggle: new Wiggle(STEADY_STATE_FREQ_VAL),
      };
    });

    // Start the animation.
    requestAnimationFrame((timeMs: number) => {
      this.stepAnimation(timeMs);
      this.transitionToSteadyState();
    });
  }

  /**
   * Resets the canvas size and stores the physical size for setting on the
   * next redraw.
   */
  setCanvasSizeTo(width: number, height: number) {
    this.canvasWidth = width;
    this.canvasHeight = height;
    this.canvasPhysicalWidth = Math.floor(width * window.devicePixelRatio);
    this.canvasPhysicalHeight = Math.floor(height * window.devicePixelRatio);
  }

  private computeShaderLayerColorRgbas() {
    return getShaderLayerColorRgbas(this.theme);
  }

  private stepAnimation(timeMs: number) {
    // We need to clear the canvas ourselves if we did not resize.
    if (!this.resetCanvasSizeIfNeeded()) {
      this.context.clearRect(0, 0, this.canvasWidth, this.canvasHeight);
    }
    this.resetCanvasPixelRatioIfNeeded();

    this.drawCircles(timeMs);
    requestAnimationFrame(this.stepAnimation.bind(this));
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
      if (this.areResultsShowing) {
        // Hide shimmer if user focusing on results.
        this.context.globalAlpha = 0;
      } else {
        this.transitionToSteadyState();
      }
    }
  }

  // Focuses the shimmer on a specific region of the screen. The inputted values
  // should be percentage values between 0-1 representing the region to focus.
  private async focusRegion(
      centerX: number, centerY: number, width: number, height: number,
      requester: ShimmerControlRequester) {
    const currentShimmerController = this.getCurrentShimmerController();
    if (currentShimmerController > requester) {
      // Ignore this request because the current controller has a higher
      // priority than the requester.
      return;
    } else if (currentShimmerController < requester) {
      this.shimmerControllerStack.push(requester);
    }

    switch (requester) {
      case ShimmerControlRequester.SEGMENTATION:
      // Intended fall through since SEGMENTATION AND POST_SELECTION follow
      // the same values.
      case ShimmerControlRequester.POST_SELECTION:
        this.regionCenter = {x: centerX * 100, y: centerY * 100};
        this.regionWidth = width;
        this.regionHeight = height;

        // Always restart the animation as this means we are going to a new
        // region.
        this.setTransitionState(ShimmerState.TRANSITION_TO_REGION);
        break;
      case ShimmerControlRequester.MANUAL_REGION:
        this.regionCenter = {x: centerX * 100, y: centerY * 100};
        this.regionWidth = width;
        this.regionHeight = height;

        // Only restart the animation if we are going to a new region and the
        // previous animation has finished.
        if (this.shimmerState !== ShimmerState.TRANSITION_TO_REGION) {
          this.setTransitionState(ShimmerState.TRANSITION_TO_REGION);
        }
        break;
      case ShimmerControlRequester.CURSOR:
        this.cursorCenter = {x: centerX * 100, y: centerY * 100};

        // Only start the animation if the circles haven't already transitioned
        // to the cursor state.
        if (this.shimmerState !== ShimmerState.TRANSITION_TO_CURSOR &&
            this.shimmerState !== ShimmerState.CURSOR) {
          this.setTransitionState(ShimmerState.TRANSITION_TO_CURSOR);
        }
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
  }

  private drawCircles(timeMs: number) {
    // Update the animation time variables. These should be the same for all of
    // the animations we need to do.
    this.setCurrentAnimationStartTimeIfNeeded(timeMs);
    const elapsed = this.getElapsedAnimationTime(timeMs);
    const easingFunction = this.getEasingFunctionForCurrentState();

    // Animate the opacity if required for the upcoming drawing functions.
    this.animateOpacityIfNeeded(elapsed, easingFunction);

    for (let i = 0; i < this.circles.length; i++) {
      const circle = this.circles[i];
      const shimmerAnimation = this.shimmerAnimation[i];
      this.setWiggleFrequency(circle);

      // If we are no longer wiggling, we do not want the circles to abruptly
      // shift where they were not before. So use the previous wiggle value.
      let radiusWiggle = circle.radiusWiggle.getPreviousWiggleValue();
      let centerXWiggle = circle.centerXWiggle.getPreviousWiggleValue();
      let centerYWiggle = circle.centerYWiggle.getPreviousWiggleValue();
      if (this.isWiggling) {
        radiusWiggle = circle.radiusWiggle.calculateNext(timeMs / 1000);
        centerXWiggle = circle.centerXWiggle.calculateNext(timeMs / 1000);
        centerYWiggle = circle.centerYWiggle.calculateNext(timeMs / 1000);
      }

      this.stepUpdateCircle(circle, shimmerAnimation, elapsed, easingFunction);

      // We need to set the latest cursor point after we are done transitioning
      // to make sure the circles appear wherever the cursor last was.
      if (this.shimmerState === ShimmerState.CURSOR) {
        circle.center.x = this.cursorCenter.x;
        circle.center.y = this.cursorCenter.y;
      }

      const baseRadius = circle.radius / 100 *
          Math.max(this.canvasPhysicalWidth, this.canvasPhysicalHeight) *
          circle.blur;
      const baseCircleX = circle.center.x / 100 * this.canvasPhysicalWidth;
      const baseCircleY = circle.center.y / 100 * this.canvasPhysicalHeight;

      // Get the actual values as they should be rendered on the screen.
      const radiusAmp = circle.radiusAmpPercent / 100 *
          Math.max(this.canvasPhysicalWidth, this.canvasPhysicalHeight);
      const centerXAmp =
          circle.centerXAmpPercent / 100 * this.canvasPhysicalWidth;
      const centerYAmp =
          circle.centerYAmpPercent / 100 * this.canvasPhysicalHeight;

      // Floor these values to prevent sub pixel rendering. This provides better
      // performance.
      const adjustedRadius = Math.floor(
          (baseRadius + radiusAmp * radiusWiggle) / window.devicePixelRatio);
      const adjustedCenterX = Math.floor(
          (baseCircleX + centerXAmp * centerXWiggle) / window.devicePixelRatio);
      const adjustedCenterY = Math.floor(
          (baseCircleY + centerYAmp * centerYWiggle) / window.devicePixelRatio);

      this.drawCircle(
          adjustedRadius, adjustedCenterX, adjustedCenterY, circle.colorRgba);
    }

    // If the last transition update resulted in a completed animation, clean up
    // an leftover animation state.
    this.finishAnimationIfNeeded(elapsed);
  }

  // Checks the set canvas size and if it has changed, resets it on the canvas.
  // Returns false if no changes were made.
  private resetCanvasSizeIfNeeded(): boolean {
    if (this.canvas.width !== this.canvasPhysicalWidth ||
        this.canvas.height !== this.canvasPhysicalHeight) {
      // The opacity of the canvas is cleared on resize. So store it before it
      // is reset so we can set it back on the context after the resizing.
      const currentOpacity = this.context.globalAlpha;

      this.canvas.height = this.canvasPhysicalHeight;
      this.canvas.width = this.canvasPhysicalWidth;
      this.context.globalAlpha = currentOpacity;
      return true;
    }

    return false;
  }

  // Check if the devicePixelRatio has changed since the last redraw and modify
  // the canvas context if it has.
  private resetCanvasPixelRatioIfNeeded() {
    const transform = this.context.getTransform();
    if (transform.a !== window.devicePixelRatio ||
        transform.d !== window.devicePixelRatio) {
      this.context.setTransform(
          window.devicePixelRatio, 0, 0, window.devicePixelRatio, 0, 0);
    }
  }

  // Animate opacity if required. This sets the opacity for the all of the
  // drawing operations that follow.
  private animateOpacityIfNeeded(elapsed: number, easingFunction: CubicBezier) {
    if (this.isShimmerInTransitionState()) {
      // All of the circles should share the same end opacity.
      const shimmerAnimation = this.shimmerAnimation[0];
      if (shimmerAnimation) {
        const opacityProgress =
            Math.min(elapsed / this.getTransitionDuration(), 1);
        const easedOpacityProgress =
            Math.min(easingFunction.solveForY(opacityProgress), 1);
        this.context.globalAlpha = lerp(
            shimmerAnimation.startKeyframe.opacity,
            shimmerAnimation.endKeyframe.opacity, easedOpacityProgress);
      }
    }
  }

  // Updates the current circle's attributes according to the current shimmer
  // transition state, and provided elapsed duration, easing function, and
  // shimmer animation. This is a no-op if the shimmer is not in a transition
  // state.
  private stepUpdateCircle(
      circle: ShimmerCircle, shimmerAnimation: ShimmerAnimation,
      elapsed: number, easingFunction: CubicBezier) {
    // Animate the circles if needed according to the current shimmer state.
    if (this.isShimmerInTransitionState()) {
      let endCenter = structuredClone(shimmerAnimation.endKeyframe.center);
      let endCenterXAmpPrecent = shimmerAnimation.endKeyframe.centerXAmpPercent;
      let endCenterYAmpPrecent = shimmerAnimation.endKeyframe.centerYAmpPercent;
      let endRadius = shimmerAnimation.endKeyframe.radius;
      let endRadiusAmpPercent = shimmerAnimation.endKeyframe.radiusAmpPercent;

      // The cursor and region states set base values in their key frames. We
      // use instance members to make sure we always use the most up to date
      // values without needing to update key frames.
      if (this.shimmerState === ShimmerState.TRANSITION_TO_CURSOR) {
        endCenter = structuredClone(this.cursorCenter);
      } else if (this.shimmerState === ShimmerState.TRANSITION_TO_REGION) {
        const smallestLength = Math.min(this.regionHeight, this.regionWidth);
        endCenter = structuredClone(this.regionCenter);
        endCenterXAmpPrecent = endCenterXAmpPrecent * this.regionWidth;
        endCenterYAmpPrecent = endCenterYAmpPrecent * this.regionHeight;
        endRadius = endRadius * smallestLength;
        endRadiusAmpPercent = endRadiusAmpPercent * smallestLength;
      }

      const progress = Math.min(elapsed / this.getTransitionDuration(), 1);
      const easedProgress = Math.min(easingFunction.solveForY(progress), 1);

      // The cursor has a different transition duration for the radius than
      // for the other attributes.
      let easedRadiusProgress = easedProgress;
      if (this.shimmerState === ShimmerState.TRANSITION_TO_CURSOR) {
        const radiusProgress =
            Math.min(elapsed / CURSOR_STATE_TRANSITION_DURATION, 1);
        easedRadiusProgress =
            Math.min(easingFunction.solveForY(radiusProgress), 1);
      }

      circle.center.x = lerp(
          shimmerAnimation.startKeyframe.center.x, endCenter.x, easedProgress);
      circle.center.y = lerp(
          shimmerAnimation.startKeyframe.center.y, endCenter.y, easedProgress);
      circle.centerXAmpPercent = lerp(
          shimmerAnimation.startKeyframe.centerXAmpPercent,
          endCenterXAmpPrecent, easedProgress);
      circle.centerYAmpPercent = lerp(
          shimmerAnimation.startKeyframe.centerYAmpPercent,
          endCenterYAmpPrecent, easedProgress);
      circle.radiusAmpPercent = lerp(
          shimmerAnimation.startKeyframe.radiusAmpPercent, endRadiusAmpPercent,
          easedProgress);

      circle.radius = lerp(
          shimmerAnimation.startKeyframe.radius, endRadius,
          easedRadiusProgress);
    }
  }

  private drawCircle(
      radius: number, centerX: number, centerY: number, colorRgba: string) {
    this.context.beginPath();
    this.context.fillStyle =
        createCircleGradient(this.context, centerX, centerY, radius, colorRgba);
    this.context.arc(centerX, centerY, radius, 0, 2 * Math.PI);
    this.context.closePath();
    this.context.fill();
  }

  private createShimmerAnimation() {
    this.shimmerAnimation = this.circles.map(
        item => ({
          startKeyframe: this.createStartKeyframeFromCircle(item),
          endKeyframe: this.createEndKeyframeFromCircle(item),
        }));
  }

  private setWiggleFrequency(circle: ShimmerCircle) {
    if (this.shimmerState === ShimmerState.TRANSITION_TO_REGION ||
        this.shimmerState === ShimmerState.REGION) {
      circle.radiusWiggle.setFrequency(INTERACTION_STATE_FREQ_VAL);
      circle.centerXWiggle.setFrequency(INTERACTION_STATE_FREQ_VAL);
      circle.centerYWiggle.setFrequency(INTERACTION_STATE_FREQ_VAL);
      return;
    }
    circle.radiusWiggle.setFrequency(STEADY_STATE_FREQ_VAL);
    circle.centerXWiggle.setFrequency(STEADY_STATE_FREQ_VAL);
    circle.centerYWiggle.setFrequency(STEADY_STATE_FREQ_VAL);
  }

  private createStartKeyframeFromCircle(circle: ShimmerCircle):
      ShimmerAnimationKeyframe {
    return {
      blur: circle.blur,
      center: {x: circle.center.x, y: circle.center.y},
      centerXAmpPercent: circle.centerXAmpPercent,
      centerYAmpPercent: circle.centerYAmpPercent,
      radius: circle.radius,
      radiusAmpPercent: circle.radiusAmpPercent,
      radiusWiggleValue: circle.radiusWiggle.getPreviousWiggleValue(),
      centerXWiggleValue: circle.centerXWiggle.getPreviousWiggleValue(),
      centerYWiggleValue: circle.centerYWiggle.getPreviousWiggleValue(),
      opacity: this.context.globalAlpha,
    };
  }

  private createEndKeyframeFromCircle(circle: ShimmerCircle):
      ShimmerAnimationKeyframe {
    // Assume we are staying the same.
    const keyframe = this.createStartKeyframeFromCircle(circle);
    if (this.shimmerState === ShimmerState.TRANSITION_TO_STEADY_STATE) {
      keyframe.blur = STEADY_STATE_CIRCLE_BLUR;
      keyframe.center = structuredClone(circle.steadyStateCenter);
      keyframe.centerXAmpPercent = STEADY_STATE_CENTER_X_AMPLITUDE_PERCENT;
      keyframe.centerYAmpPercent = STEADY_STATE_CENTER_Y_AMPLITUDE_PERCENT;
      keyframe.radius = STEADY_STATE_RADIUS_PERCENT;
      keyframe.radiusAmpPercent = STEADY_STATE_RADIUS_AMPLITUDE_PERCENT;
      keyframe.opacity = STEADY_STATE_OPACITY_PERCENT;
    } else if (this.shimmerState === ShimmerState.TRANSITION_TO_CURSOR) {
      // The centerX and centerY can change in between key frames, so we use an
      // instance member of this component to track that end.
      keyframe.centerXAmpPercent = CURSOR_STATE_CENTER_X_AMPLITUDE_PERCENT;
      keyframe.centerYAmpPercent = CURSOR_STATE_CENTER_Y_AMPLITUDE_PERCENT;
      keyframe.radius = CURSOR_STATE_RADIUS_PERCENT;
      keyframe.radiusAmpPercent = CURSOR_STATE_RADIUS_AMPLITUDE_PERCENT;
    } else if (this.shimmerState === ShimmerState.TRANSITION_TO_REGION) {
      // The centerX and centerY can change in between key frames, so we use an
      // instance member of this component to track that end.
      keyframe.blur = REGION_SELECTION_STATE_CIRCLE_BLUR;
      keyframe.centerXAmpPercent =
          REGION_SELECTION_STATE_CENTER_X_AMPLITUDE_PERCENT;
      keyframe.centerYAmpPercent =
          REGION_SELECTION_STATE_CENTER_Y_AMPLITUDE_PERCENT;
      // This radius is dependent on a instance member of this component because
      // it can change quickly in between key frames.
      keyframe.radius = REGION_SELECTION_STATE_RADIUS_PERCENT;
      keyframe.radiusAmpPercent =
          REGION_SELECTION_STATE_RADIUS_AMPLITUDE_PERCENT;
      keyframe.opacity = INTERACTION_STATE_OPACITY_PERCENT;
    }
    return keyframe;
  }

  private isShimmerInTransitionState(): boolean {
    return this.shimmerState === ShimmerState.TRANSITION_TO_REGION ||
        this.shimmerState === ShimmerState.TRANSITION_TO_STEADY_STATE ||
        this.shimmerState === ShimmerState.TRANSITION_TO_CURSOR;
  }

  private setCurrentAnimationStartTimeIfNeeded(currentTimeMs: number) {
    if (this.isShimmerInTransitionState() &&
        this.animationStartTime === undefined) {
      this.createShimmerAnimation();
      this.animationStartTime = currentTimeMs;
    }
  }

  private getElapsedAnimationTime(currentTimeMs: number): number {
    if (this.animationStartTime && this.animationStartTime > 0) {
      return currentTimeMs - this.animationStartTime;
    }
    return 0;
  }

  private getEasingFunctionForCurrentState(): CubicBezier {
    if (this.shimmerState === ShimmerState.TRANSITION_TO_STEADY_STATE) {
      return STEADY_STATE_EASING_FUNCTION;
    }
    return INTERACTION_STATE_EASING_FUNCTION;
  }

  private setTransitionState(state: ShimmerState) {
    this.animationStartTime = undefined;
    this.shimmerState = state;
  }

  private finishAnimationIfNeeded(elapsed: number) {
    if (this.shimmerState === ShimmerState.TRANSITION_TO_STEADY_STATE &&
        elapsed >= STEADY_STATE_TRANSITION_DURATION) {
      this.shimmerState = ShimmerState.STEADY_STATE;
    } else if (
        this.shimmerState === ShimmerState.TRANSITION_TO_REGION &&
        elapsed >= REGION_SELECTION_TRANSITION_DURATION) {
      this.shimmerState = ShimmerState.REGION;
    } else if (
        this.shimmerState === ShimmerState.TRANSITION_TO_CURSOR &&
        elapsed >= CURSOR_STATE_TRANSITION_DURATION) {
      this.shimmerState = ShimmerState.CURSOR;
    }
  }

  /** Returns the expected duration of the current transition. */
  private getTransitionDuration(): number {
    if (this.shimmerState === ShimmerState.TRANSITION_TO_STEADY_STATE) {
      return STEADY_STATE_TRANSITION_DURATION;
    } else if (this.shimmerState === ShimmerState.TRANSITION_TO_REGION) {
      return REGION_SELECTION_TRANSITION_DURATION;
    } else if (this.shimmerState === ShimmerState.TRANSITION_TO_CURSOR) {
      return CURSOR_STATE_INITIAL_FOCUS_DURATION;
    }
    return 0;
  }

  private transitionToSteadyState() {
    this.setTransitionState(ShimmerState.TRANSITION_TO_STEADY_STATE);
  }

  private getCurrentShimmerController(): ShimmerControlRequester {
    return this.shimmerControllerStack[this.shimmerControllerStack.length - 1];
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'overlay-shimmer-canvas': OverlayShimmerCanvasElement;
  }
}

customElements.define(
    OverlayShimmerCanvasElement.is, OverlayShimmerCanvasElement);
