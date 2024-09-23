// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from '//resources/js/assert.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
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
// Exception: segmentations use a different opacity.
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
// The time it takes in MS to transition the shimmer circles to scale down to
// zero.
const CURSOR_SHRINK_TRANSITION_DURATION = 750;
const FADE_OUT_STATE_OPACITY_PERCENT = 0;
// The transition duration for the fade out animation
const FADE_OUT_TRANSITION_DURATION = 100;
const FADE_OUT_EASING_FUNCTION = new CubicBezier(0, 0, 1, 1);

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

// SEGMENTATION STATE CONSTANTS: These are the values that are only applied when
// the shimmer is focusing on a segmentation mask. In the segmentation state,
// these values are in relation to the bounding box smallest size, rather than
// the entire viewport.
const SEGMENTATION_STATE_OPACITY_PERCENT = 0.3;
const SEGMENTATION_STATE_RADIUS_PERCENT = 30;
const SEGMENTATION_STATE_CIRCLE_BLUR = 1.8;
const SEGMENTATION_STATE_RADIUS_AMPLITUDE_PERCENT = 0;
const SEGMENTATION_STATE_CENTER_X_AMPLITUDE_PERCENT = 20;
const SEGMENTATION_STATE_CENTER_Y_AMPLITUDE_PERCENT = 20;
// The time it takes in MS to transition from a different state to the
// segmentation state.
const SEGMENTATION_TRANSITION_DURATION = 750;

// The opacity of the sparkles. The sparkles opacity also is dictated by the
// circle pixel opacity below it. Meaning, the true opacity value is
// SPARKLES_OPACITY * CIRCLE_OPACITY.
const SPARKLES_OPACITY = 1;

// Specifies the current animation state of the shader canvas.
enum ShimmerState {
  NONE = 0,
  INVOCATION = 1,
  TRANSITION_TO_STEADY_STATE = 2,
  STEADY_STATE = 3,
  TRANSITION_SHRINK_TO_CURSOR = 4,
  CURSOR = 5,
  TRANSITION_FADE_IN_TO_REGION = 6,
  REGION = 7,
  TRANSITION_FADE_OUT_TO_CURSOR = 8,
  TRANSITION_FADE_OUT_TO_REGION = 9,
  TRANSITION_FADE_IN_TO_SEGMENTATION = 10,
  SEGMENTATION = 11,
  TRANSITION_FADE_OUT_TO_SEGMENTATION = 12,
  TRANSITION_FADE_OUT_TO_TRANSLATE = 13,
  TRANSLATE = 14,
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
    sparklesSvg: SVGImageElement,
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

  // The sparkles pattern used for rendering the sparkling animation. Created
  // when the sparkles PNG is loaded in. Null otherwise.
  private sparklesPattern: CanvasPattern|null = null;
  // A randomized value every 100ms to transform the sparkles to create
  // movement.
  private sparklesOffset: number = 0;
  // The ID of the setInterval call that updates the sparklesOffset.
  private sparklesIntervalId?: number;
  // Whether the sparkles are enabled or not.
  private enableSparkles: boolean =
      loadTimeData.getBoolean('enableShimmerSparkles');

  // The current shimmer state.
  private shimmerState: ShimmerState = ShimmerState.NONE;
  // The start time of the current animation. When undefined, it will be set at
  // next draw if there is an animiation needed.
  private animationStartTime?: number = undefined;
  // Whether the last transition was given time to finish.
  private didLastTransitionFinish = true;

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

    // Stop updating the sparkles if they are currently updating.
    if (this.sparklesIntervalId) {
      clearInterval(this.sparklesIntervalId);
      this.sparklesIntervalId = undefined;
    }
  }

  private onSparklesLoad() {
    // If the flag to enable sparkles is off, ignore the SVG loading in which
    // will cause skip initializing sparklesPattern so no sparkles appear.
    if (!this.enableSparkles) {
      return;
    }

    this.sparklesPattern =
        this.context.createPattern(this.$.sparklesSvg, 'repeat');

    this.sparklesIntervalId = setInterval(() => {
      this.sparklesOffset = Math.round(Math.random() * 500);
    }, 100);
  }


  // Starts the initial animation into the steady state or into a post
  // selection region if already focused.
  startAnimation() {
    // Draw invocation state.
    this.context.globalAlpha = INVOCATION_OPACITY_PERCENT;
    this.shimmerState = ShimmerState.INVOCATION;

    // Create a circle for each color rgb string defined. We do this when the
    // invocation animation is started to make sure we grab the latest set
    // overlay theme.
    this.circles = this.shaderLayerRgbaColors.map((colorRgbaString: string) => {
      return {
        colorRgba: colorRgbaString,
        steadyStateCenter: {
          x: 50 -
              STEADY_STATE_CENTER_X_PERCENT_OFFSET * (Math.random() * 2 - 1),
          y: 50 -
              STEADY_STATE_CENTER_Y_PERCENT_OFFSET * (Math.random() * 2 - 1),
        },
        blur: STEADY_STATE_CIRCLE_BLUR,
        radius: INVOCATION_RADIUS_PERCENT,
        center: this.regionCenter ?
            this.regionCenter :
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

      // Transition to the post selection region if it has already been set.
      if (this.regionCenter) {
        // Do not wiggle if going into post selection state.
        this.isWiggling = false;
        this.setTransitionState(ShimmerState.TRANSITION_FADE_IN_TO_REGION);
        return;
      }
      this.setTransitionState(ShimmerState.TRANSITION_TO_STEADY_STATE);
    });
  }

  // Resets the canvas size and stores the physical size for setting on the
  // next redraw.
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
    this.drawSparkles();
    requestAnimationFrame(this.stepAnimation.bind(this));
  }

  private onFocusRegion(e: CustomEvent<OverlayShimmerFocusedRegion>) {
    const centerX = e.detail.left + e.detail.width / 2;
    const centerY = e.detail.top + e.detail.height / 2;

    // Ignore invalid regions if not translate mode.
    if (e.detail.requester !== ShimmerControlRequester.TRANSLATE &&
        (centerX <= 0 || centerY <= 0)) {
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
    const controllerBeforeUnfocus = this.getCurrentShimmerController();
    const index = this.shimmerControllerStack.indexOf(e.detail.requester);
    // Only relinquish control if the requester currently has control.
    if (index === -1) {
      return;
    }
    // Remove the control requester from the stack.
    this.shimmerControllerStack.splice(index, 1);

    // Only make changes to the shimmmer if the controller was changed.
    const newCurrentController = this.getCurrentShimmerController();
    if (newCurrentController === controllerBeforeUnfocus) {
      return;
    }

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
        this.setTransitionState(ShimmerState.TRANSITION_TO_STEADY_STATE);
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
        this.regionCenter = {x: centerX * 100, y: centerY * 100};
        this.regionWidth = width;
        this.regionHeight = height;
        this.setTransitionState(
            ShimmerState.TRANSITION_FADE_OUT_TO_SEGMENTATION);
        break;
      case ShimmerControlRequester.POST_SELECTION:
        this.regionCenter = {x: centerX * 100, y: centerY * 100};
        this.regionWidth = width;
        this.regionHeight = height;

        // If the current shimmer controller was already the post selection
        // requester, the bounds are changing on the post selection region so do
        // not fade out.
        if (currentShimmerController ===
            ShimmerControlRequester.POST_SELECTION) {
          this.setTransitionState(ShimmerState.TRANSITION_FADE_IN_TO_REGION);
          break;
        }

        // We want to fade out from the region if it is currently drawn or has
        // already begun to fade in.
        if (this.shimmerState === ShimmerState.REGION ||
            this.shimmerState === ShimmerState.TRANSITION_FADE_IN_TO_REGION) {
          this.setTransitionState(ShimmerState.TRANSITION_FADE_OUT_TO_REGION);
          break;
        }

        // If the shimmer is already fading out to a region, we don't need to
        // start fading in since that will occur when the fade out finishes.
        if (this.shimmerState !== ShimmerState.TRANSITION_FADE_OUT_TO_REGION) {
          this.setTransitionState(ShimmerState.TRANSITION_FADE_IN_TO_REGION);
        }
        break;
      case ShimmerControlRequester.MANUAL_REGION:
        this.regionCenter = {x: centerX * 100, y: centerY * 100};
        this.regionWidth = width;
        this.regionHeight = height;

        // Only restart the animation if we are going to a new region and the
        // previous animation has finished.
        if (this.shimmerState !== ShimmerState.TRANSITION_FADE_IN_TO_REGION) {
          this.setTransitionState(ShimmerState.TRANSITION_FADE_IN_TO_REGION);
        }
        break;
      case ShimmerControlRequester.CURSOR:
        this.cursorCenter = {x: centerX * 100, y: centerY * 100};

        // Only start the animation if the circles haven't already transitioned
        // to the cursor state from the steady state.
        if (this.shimmerState !== ShimmerState.TRANSITION_FADE_OUT_TO_CURSOR &&
            this.shimmerState !== ShimmerState.TRANSITION_SHRINK_TO_CURSOR &&
            this.shimmerState !== ShimmerState.CURSOR) {
          // The shimmer should only transition to the cursor if the previous
          // controller was the steady state. Otherwise, we want the shimmer to
          // fade out in place.
          const transitionState =
              currentShimmerController === ShimmerControlRequester.NONE ?
              ShimmerState.TRANSITION_SHRINK_TO_CURSOR :
              ShimmerState.TRANSITION_FADE_OUT_TO_CURSOR;
          this.setTransitionState(transitionState);
        }
        break;
      case ShimmerControlRequester.TRANSLATE:
        this.setTransitionState(ShimmerState.TRANSITION_FADE_OUT_TO_TRANSLATE);
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

  // Draws sparkles on the canvas using circles as an alpha mask.
  private drawSparkles(): void {
    if (!this.sparklesPattern) {
      return;
    }

    // Update the sparkles position to use across the circles.
    this.sparklesPattern!.setTransform(new DOMMatrixReadOnly().translate(
        this.sparklesOffset, this.sparklesOffset));

    this.context.save();

    // Draw a path over the entire canvas.
    this.context.beginPath();
    this.context.rect(0, 0, this.canvasWidth, this.canvasHeight);
    this.context.closePath();

    this.context.globalCompositeOperation = 'source-atop';
    this.context.globalAlpha = SPARKLES_OPACITY;
    this.context.fillStyle = this.sparklesPattern;
    this.context.fill();

    this.context.restore();
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
      if (this.shimmerState === ShimmerState.TRANSITION_SHRINK_TO_CURSOR) {
        endCenter = structuredClone(this.cursorCenter);
      } else if (
          this.shimmerState === ShimmerState.TRANSITION_FADE_IN_TO_REGION ||
          this.shimmerState ===
              ShimmerState.TRANSITION_FADE_IN_TO_SEGMENTATION) {
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
      if (this.shimmerState === ShimmerState.TRANSITION_SHRINK_TO_CURSOR) {
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
    if (this.shimmerState === ShimmerState.TRANSITION_FADE_IN_TO_REGION ||
        this.shimmerState === ShimmerState.TRANSITION_FADE_OUT_TO_REGION ||
        this.shimmerState === ShimmerState.REGION ||
        this.shimmerState === ShimmerState.TRANSITION_FADE_IN_TO_SEGMENTATION ||
        this.shimmerState ===
            ShimmerState.TRANSITION_FADE_OUT_TO_SEGMENTATION ||
        this.shimmerState === ShimmerState.SEGMENTATION) {
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
    const centerPoint = {x: circle.center.x, y: circle.center.y};
    let blur = circle.blur;
    let centerXAmpPercent = circle.centerXAmpPercent;
    let centerYAmpPercent = circle.centerYAmpPercent;
    let radius = circle.radius;
    let radiusAmpPercent = circle.radiusAmpPercent;
    // When fading in, the circles should use the region position.
    if (this.shimmerState === ShimmerState.TRANSITION_FADE_IN_TO_REGION) {
      const smallestLength = Math.min(this.regionHeight, this.regionWidth);
      centerPoint.x = this.regionCenter.x;
      centerPoint.y = this.regionCenter.y;
      radius = REGION_SELECTION_STATE_RADIUS_PERCENT * smallestLength;
      radiusAmpPercent =
          REGION_SELECTION_STATE_RADIUS_AMPLITUDE_PERCENT * smallestLength;
      blur = REGION_SELECTION_STATE_CIRCLE_BLUR;
      centerXAmpPercent =
          REGION_SELECTION_STATE_CENTER_X_AMPLITUDE_PERCENT * this.regionWidth;
      centerYAmpPercent =
          REGION_SELECTION_STATE_CENTER_Y_AMPLITUDE_PERCENT * this.regionHeight;
    } else if (
        this.shimmerState === ShimmerState.TRANSITION_FADE_IN_TO_SEGMENTATION) {
      const smallestLength = Math.min(this.regionHeight, this.regionWidth);
      centerPoint.x = this.regionCenter.x;
      centerPoint.y = this.regionCenter.y;
      radius = SEGMENTATION_STATE_RADIUS_PERCENT * smallestLength;
      radiusAmpPercent =
          SEGMENTATION_STATE_RADIUS_AMPLITUDE_PERCENT * smallestLength;
      blur = SEGMENTATION_STATE_CIRCLE_BLUR;
      centerXAmpPercent =
          SEGMENTATION_STATE_CENTER_X_AMPLITUDE_PERCENT * this.regionWidth;
      centerYAmpPercent =
          SEGMENTATION_STATE_CENTER_Y_AMPLITUDE_PERCENT * this.regionHeight;
    }
    return {
      blur,
      center: centerPoint,
      centerXAmpPercent,
      centerYAmpPercent,
      radius,
      radiusAmpPercent,
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
    } else if (this.shimmerState === ShimmerState.TRANSITION_SHRINK_TO_CURSOR) {
      // The centerX and centerY can change in between key frames, so we use an
      // instance member of this component to track that end.
      keyframe.centerXAmpPercent = CURSOR_STATE_CENTER_X_AMPLITUDE_PERCENT;
      keyframe.centerYAmpPercent = CURSOR_STATE_CENTER_Y_AMPLITUDE_PERCENT;
      keyframe.radiusAmpPercent = CURSOR_STATE_RADIUS_AMPLITUDE_PERCENT;
      keyframe.radius = CURSOR_STATE_RADIUS_PERCENT;
    } else if (
        this.shimmerState === ShimmerState.TRANSITION_FADE_OUT_TO_CURSOR ||
        this.shimmerState === ShimmerState.TRANSITION_FADE_OUT_TO_REGION ||
        this.shimmerState ===
            ShimmerState.TRANSITION_FADE_OUT_TO_SEGMENTATION ||
        this.shimmerState === ShimmerState.TRANSITION_FADE_OUT_TO_TRANSLATE) {
      keyframe.opacity = FADE_OUT_STATE_OPACITY_PERCENT;
    } else if (
        this.shimmerState === ShimmerState.TRANSITION_FADE_IN_TO_REGION) {
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
    } else if (
        this.shimmerState === ShimmerState.TRANSITION_FADE_IN_TO_SEGMENTATION) {
      // The centerX and centerY can change in between key frames, so we use an
      // instance member of this component to track that end.
      keyframe.blur = SEGMENTATION_STATE_CIRCLE_BLUR;
      keyframe.centerXAmpPercent =
          SEGMENTATION_STATE_CENTER_X_AMPLITUDE_PERCENT;
      keyframe.centerYAmpPercent =
          SEGMENTATION_STATE_CENTER_Y_AMPLITUDE_PERCENT;
      // This radius is dependent on a instance member of this component because
      // it can change quickly in between key frames.
      keyframe.radius = SEGMENTATION_STATE_RADIUS_PERCENT;
      keyframe.radiusAmpPercent = SEGMENTATION_STATE_RADIUS_AMPLITUDE_PERCENT;
      keyframe.opacity = SEGMENTATION_STATE_OPACITY_PERCENT;
    }
    return keyframe;
  }

  private isShimmerInTransitionState(): boolean {
    return this.shimmerState === ShimmerState.TRANSITION_FADE_IN_TO_REGION ||
        this.shimmerState === ShimmerState.TRANSITION_FADE_IN_TO_SEGMENTATION ||
        this.shimmerState === ShimmerState.TRANSITION_TO_STEADY_STATE ||
        this.shimmerState === ShimmerState.TRANSITION_SHRINK_TO_CURSOR ||
        this.shimmerState === ShimmerState.TRANSITION_FADE_OUT_TO_CURSOR ||
        this.shimmerState === ShimmerState.TRANSITION_FADE_OUT_TO_REGION ||
        this.shimmerState ===
        ShimmerState.TRANSITION_FADE_OUT_TO_SEGMENTATION ||
        this.shimmerState === ShimmerState.TRANSITION_FADE_OUT_TO_TRANSLATE;
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
    } else if (
        this.shimmerState === ShimmerState.TRANSITION_FADE_OUT_TO_CURSOR ||
        this.shimmerState === ShimmerState.TRANSITION_FADE_OUT_TO_REGION ||
        this.shimmerState ===
            ShimmerState.TRANSITION_FADE_OUT_TO_SEGMENTATION ||
        this.shimmerState === ShimmerState.TRANSITION_FADE_OUT_TO_TRANSLATE) {
      return FADE_OUT_EASING_FUNCTION;
    }
    return INTERACTION_STATE_EASING_FUNCTION;
  }

  private setTransitionState(state: ShimmerState) {
    if (this.isShimmerInTransitionState()) {
      this.didLastTransitionFinish = false;
    }
    // Mark the fade out as complete unless we are going to begin fading out.
    this.dispatchEvent(new CustomEvent('shimmer-fade-out-complete', {
      bubbles: true,
      composed: true,
      detail: state !== ShimmerState.TRANSITION_FADE_OUT_TO_REGION &&
          state !== ShimmerState.TRANSITION_FADE_OUT_TO_SEGMENTATION &&
          state !== ShimmerState.TRANSITION_FADE_OUT_TO_TRANSLATE,
    }));
    this.animationStartTime = undefined;
    this.shimmerState = state;
  }

  private finishAnimationIfNeeded(elapsed: number) {
    if (this.shimmerState === ShimmerState.TRANSITION_TO_STEADY_STATE &&
        elapsed >= STEADY_STATE_TRANSITION_DURATION) {
      this.shimmerState = ShimmerState.STEADY_STATE;
      this.didLastTransitionFinish = true;
    } else if (
        this.shimmerState === ShimmerState.TRANSITION_FADE_IN_TO_REGION &&
        elapsed >= REGION_SELECTION_TRANSITION_DURATION) {
      this.shimmerState = ShimmerState.REGION;
      this.didLastTransitionFinish = true;
    } else if (
        this.shimmerState === ShimmerState.TRANSITION_FADE_IN_TO_SEGMENTATION &&
        elapsed >= SEGMENTATION_TRANSITION_DURATION) {
      this.shimmerState = ShimmerState.SEGMENTATION;
      this.didLastTransitionFinish = true;
    } else if (
        this.shimmerState === ShimmerState.TRANSITION_SHRINK_TO_CURSOR &&
        elapsed >= CURSOR_STATE_TRANSITION_DURATION) {
      this.shimmerState = ShimmerState.CURSOR;
      this.didLastTransitionFinish = true;
    } else if (
        this.shimmerState === ShimmerState.TRANSITION_FADE_OUT_TO_CURSOR &&
        elapsed >= FADE_OUT_TRANSITION_DURATION) {
      this.shimmerState = ShimmerState.CURSOR;
      this.didLastTransitionFinish = true;
    } else if (
        this.shimmerState === ShimmerState.TRANSITION_FADE_OUT_TO_REGION &&
        elapsed >= FADE_OUT_TRANSITION_DURATION) {
      this.dispatchEvent(new CustomEvent('shimmer-fade-out-complete', {
        bubbles: true,
        composed: true,
        detail: true,
      }));
      this.didLastTransitionFinish = true;
      this.shimmerState = ShimmerState.NONE;
      this.setTransitionState(ShimmerState.TRANSITION_FADE_IN_TO_REGION);
    } else if (
        this.shimmerState ===
            ShimmerState.TRANSITION_FADE_OUT_TO_SEGMENTATION &&
        elapsed >= FADE_OUT_TRANSITION_DURATION) {
      this.dispatchEvent(new CustomEvent('shimmer-fade-out-complete', {
        bubbles: true,
        composed: true,
        detail: true,
      }));
      this.didLastTransitionFinish = true;
      this.shimmerState = ShimmerState.NONE;
      this.setTransitionState(ShimmerState.TRANSITION_FADE_IN_TO_SEGMENTATION);
    } else if (
        this.shimmerState === ShimmerState.TRANSITION_FADE_OUT_TO_TRANSLATE &&
        elapsed >= FADE_OUT_TRANSITION_DURATION) {
      this.dispatchEvent(new CustomEvent('shimmer-fade-out-complete', {
        bubbles: true,
        composed: true,
        detail: true,
      }));
      this.didLastTransitionFinish = true;
      this.shimmerState = ShimmerState.TRANSLATE;
    }
  }

  /** Returns the expected duration of the current transition. */
  private getTransitionDuration(): number {
    if (this.shimmerState === ShimmerState.TRANSITION_TO_STEADY_STATE) {
      return STEADY_STATE_TRANSITION_DURATION;
    } else if (
        this.shimmerState === ShimmerState.TRANSITION_FADE_IN_TO_REGION) {
      return REGION_SELECTION_TRANSITION_DURATION;
    } else if (
        this.shimmerState === ShimmerState.TRANSITION_FADE_IN_TO_SEGMENTATION) {
      return SEGMENTATION_TRANSITION_DURATION;
    } else if (this.shimmerState === ShimmerState.TRANSITION_SHRINK_TO_CURSOR) {
      return CURSOR_SHRINK_TRANSITION_DURATION;
    } else if (
        this.shimmerState === ShimmerState.TRANSITION_FADE_OUT_TO_CURSOR ||
        this.shimmerState === ShimmerState.TRANSITION_FADE_OUT_TO_REGION ||
        this.shimmerState ===
            ShimmerState.TRANSITION_FADE_OUT_TO_SEGMENTATION ||
        this.shimmerState === ShimmerState.TRANSITION_FADE_OUT_TO_TRANSLATE) {
      return FADE_OUT_TRANSITION_DURATION;
    }
    return 0;
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
