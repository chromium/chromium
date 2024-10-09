// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AsyncUtil} from '/common/async_util.js';
import {EventGenerator} from '/common/event_generator.js';
import {EventHandler} from '/common/event_handler.js';
import {NodeUtils} from '/common/node_utils.js';
import {RectUtil} from '/common/rect_util.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';
import type {FaceLandmarkerResult} from '/third_party/mediapipe/vision.js';

import {BubbleController} from './bubble_controller.js';
import {ScrollModeController} from './scroll_mode_controller.js';

import AutomationNode = chrome.automation.AutomationNode;
import RoleType = chrome.automation.RoleType;
import ScreenRect = chrome.accessibilityPrivate.ScreenRect;
import ScreenPoint = chrome.accessibilityPrivate.ScreenPoint;
import SyntheticMouseEventButton = chrome.accessibilityPrivate.SyntheticMouseEventButton;

type PrefObject = chrome.settingsPrivate.PrefObject;

// A ScreenPoint represents an integer screen coordinate, whereas
// a FloatingPoint2D represents a (x, y) floating point number
// (which may be used for screen position or velocity).
interface FloatingPoint2D {
  x: number;
  y: number;
}

enum LandmarkType {
  FOREHEAD = 'forehead',
  FOREHEAD_TOP = 'foreheadTop',
  LEFT_TEMPLE = 'leftTemple',
  NOSE_TIP = 'noseTip',
  RIGHT_TEMPLE = 'rightTemple',
  ROTATION = 'rotation',
}

/** Handles all interaction with the mouse. */
export class MouseController {
  /** Last seen mouse location (cached from event in onMouseMovedOrDragged_). */
  private mouseLocation_: ScreenPoint|undefined;
  private onMouseMovedHandler_: EventHandler;
  private onMouseDraggedHandler_: EventHandler;
  private screenBounds_: ScreenRect|undefined;

  private prefsListener_: ((prefs: PrefObject[]) => void);

  // These values will be updated when prefs are received in init_().
  private targetBufferSize_ = MouseController.DEFAULT_BUFFER_SIZE;
  private useMouseAcceleration_ =
      MouseController.DEFAULT_USE_MOUSE_ACCELERATION;
  private spdRight_ = MouseController.DEFAULT_MOUSE_SPEED;
  private spdLeft_ = MouseController.DEFAULT_MOUSE_SPEED;
  private spdUp_ = MouseController.DEFAULT_MOUSE_SPEED;
  private spdDown_ = MouseController.DEFAULT_MOUSE_SPEED;
  private velocityThreshold_ = 0;
  private velocityThresholdFactor_ = MouseController.DEFAULT_VELOCITY_FACTOR;
  private useVelocityThreshold_ = true;

  /** The most recent raw face landmark mouse locations. */
  private buffer_: ScreenPoint[] = [];

  /** Used for smoothing the recent points in the buffer. */
  private smoothKernel_: number[] = [];

  /** The most recent smoothed mouse location. */
  private previousSmoothedLocation_: FloatingPoint2D|undefined;

  /** The last location in screen coordinates of the tracked landmark. */
  private lastLandmarkLocation_: FloatingPoint2D|undefined;

  private mouseInterval_: number = -1;
  private lastMouseMovedTime_: number = 0;
  private landmarkWeights_: Map<string, number>;
  private paused_ = false;

  private useGravity_ = false;
  // Vector fields to track how the cursor should be adjusted toward controls.
  private gravityField_: FloatingPoint2D[] = [];
  // Timer to refresh the gravity field.
  private refreshGravityInterval_: number = -1;
  // Set of gravity nodes currently affecting the cursor mapping.
  private gravityNodes_: Map<string, ScreenRect> = new Map();
  // Reference to the accessibility tree.
  private desktop_: AutomationNode|undefined;

  private scrollModeController_: ScrollModeController;
  private bubbleController_: BubbleController;
  private longClickActive_ = false;

  constructor(bubbleController: BubbleController) {
    this.bubbleController_ = bubbleController;
    this.onMouseMovedHandler_ = new EventHandler(
        [], chrome.automation.EventType.MOUSE_MOVED,
        event => this.onMouseMovedOrDragged_(event));

    this.onMouseDraggedHandler_ = new EventHandler(
        [], chrome.automation.EventType.MOUSE_DRAGGED,
        event => this.onMouseMovedOrDragged_(event));

    this.scrollModeController_ = new ScrollModeController();

    this.calcSmoothKernel_();
    this.calcVelocityThreshold_();

    this.landmarkWeights_ = new Map();
    this.landmarkWeights_.set(LandmarkType.FOREHEAD, 0.1275);
    this.landmarkWeights_.set(LandmarkType.FOREHEAD_TOP, 0.0738);
    this.landmarkWeights_.set(LandmarkType.NOSE_TIP, 0.3355);
    this.landmarkWeights_.set(LandmarkType.LEFT_TEMPLE, 0.0336);
    this.landmarkWeights_.set(LandmarkType.RIGHT_TEMPLE, 0.0336);
    this.landmarkWeights_.set(LandmarkType.ROTATION, 0.3960);

    this.prefsListener_ = prefs => this.updateFromPrefs_(prefs);
    this.init();
  }

  async init(): Promise<void> {
    chrome.accessibilityPrivate.enableMouseEvents(true);
    const desktop = await AsyncUtil.getDesktop();
    this.onMouseMovedHandler_.setNodes(desktop);
    this.onMouseMovedHandler_.start();
    this.onMouseDraggedHandler_.setNodes(desktop);
    this.onMouseDraggedHandler_.start();
    this.desktop_ = desktop;
  }

  isScrollModeActive(): boolean {
    return this.scrollModeController_.active();
  }

  isLongClickActive(): boolean {
    return this.longClickActive_;
  }

  async start(): Promise<void> {
    this.paused_ = false;
    chrome.settingsPrivate.getAllPrefs(prefs => this.updateFromPrefs_(prefs));
    chrome.settingsPrivate.onPrefsChanged.addListener(this.prefsListener_);

    // TODO(b/309121742): Handle display bounds changed.
    const screens = await new Promise<ScreenRect[]>((resolve) => {
      chrome.accessibilityPrivate.getDisplayBounds((screens: ScreenRect[]) => {
        resolve(screens);
      });
    });
    if (!screens.length) {
      // TODO(b/309121742): Error handling for no detected screens.
      return;
    }
    this.screenBounds_ = screens[0];

    // Ensure the mouse location is set.
    // The user might not be touching the mouse because they only
    // have FaceGaze input, in which case we need to make the
    // mouse move to a known location in order to proceed.
    if (!this.mouseLocation_) {
      this.resetLocation();
    }

    chrome.accessibilityPrivate.isFeatureEnabled(
        chrome.accessibilityPrivate.AccessibilityFeature
            .FACE_GAZE_GRAVITY_WELLS,
        enabled => {
          this.useGravity_ = enabled;
          if (this.useGravity_) {
            this.resetGravity_();
            this.refreshGravityInterval_ = setInterval(
                () => this.refreshGravity_(),
                MouseController.GRAVITY_INTERVAL_MS);
          }
        });

    // Start the logic to move the mouse.
    this.mouseInterval_ = setInterval(
        () => this.updateMouseLocation_(), MouseController.MOUSE_INTERVAL_MS);
  }

  /** Update the current location of the tracked face landmark. */
  onFaceLandmarkerResult(result: FaceLandmarkerResult): void {
    if (this.paused_ || !this.screenBounds_ || !result.faceLandmarks ||
        !result.faceLandmarks[0]) {
      return;
    }

    // These scale from 0 to 1.
    const avgLandmarkLocation = {x: 0, y: 0};
    let hasLandmarks = false;
    for (const landmark of MouseController.LANDMARK_INDICES) {
      let landmarkLocation;
      if (landmark.name === 'rotation' && result.facialTransformationMatrixes &&
          result.facialTransformationMatrixes.length) {
        landmarkLocation =
            MouseController.calculateRotationFromFacialTransformationMatrix(
                result.facialTransformationMatrixes[0]);
      } else if (result.faceLandmarks[0][landmark.index] !== undefined) {
        landmarkLocation = result.faceLandmarks[0][landmark.index];
      }
      if (!landmarkLocation) {
        continue;
      }
      const x = landmarkLocation.x;
      const y = landmarkLocation.y;
      let weight = this.landmarkWeights_.get(landmark.name);
      if (!weight) {
        weight = 0;
      }
      avgLandmarkLocation.x += (x * weight);
      avgLandmarkLocation.y += (y * weight);
      hasLandmarks = true;
    }

    if (!hasLandmarks) {
      return;
    }

    // Calculate the absolute position on the screen, where the top left
    // corner represents (0,0) and the bottom right corner represents
    // (this.screenBounds_.width, this.screenBounds_.height).
    // TODO(b/309121742): Handle multiple displays.
    const absoluteY = Math.round(
        avgLandmarkLocation.y * this.screenBounds_.height +
        this.screenBounds_.top);
    // Reflect the x coordinate since the webcam doesn't mirror in the
    // horizontal direction.
    const scaledX =
        Math.round(avgLandmarkLocation.x * this.screenBounds_.width);
    const absoluteX =
        this.screenBounds_.width - scaledX + this.screenBounds_.left;

    this.lastLandmarkLocation_ = {x: absoluteX, y: absoluteY};
  }

  /**
   * Called every MOUSE_INTERVAL_MS, this function uses the most recent
   * landmark location to update the current mouse position within the
   * screen, applying appropriate scaling and smoothing.
   * This function doesn't simply set the absolute position of the tracked
   * landmark. Instead, it calculates deltas to be applied to the
   * current mouse location based on the landmark's location relative
   * to its previous location.
   */
  private updateMouseLocation_(): void {
    if (this.paused_ || !this.lastLandmarkLocation_ || !this.mouseLocation_ ||
        !this.screenBounds_) {
      return;
    }

    // Add the most recent landmark point to the buffer.
    this.addPointToBuffer_(this.lastLandmarkLocation_);

    // Smooth the buffer to get the latest target point.
    const smoothed = this.applySmoothing_();

    // Compute the velocity: how position has changed compared to the previous
    // point. Note that we are assuming points come in at a regular interval,
    // but we could also run this regularly in a timeout to reduce the rate at
    // which points must be seen.
    if (!this.previousSmoothedLocation_) {
      // Initialize previous location to the current to avoid a jump at
      // start-up.
      this.previousSmoothedLocation_ = smoothed;
    }

    const velocityX = smoothed.x - this.previousSmoothedLocation_.x;
    const velocityY = smoothed.y - this.previousSmoothedLocation_.y;
    const scaledVel = this.asymmetryScale_({x: velocityX, y: velocityY});
    this.previousSmoothedLocation_ = smoothed;
    if (this.useMouseAcceleration_) {
      scaledVel.x *= this.applySigmoidAcceleration_(scaledVel.x);
      scaledVel.y *= this.applySigmoidAcceleration_(scaledVel.y);
    }

    if (!this.scrollModeController_.active() &&
        !this.exceedsVelocityThreshold_(scaledVel.x) &&
        !this.exceedsVelocityThreshold_(scaledVel.y)) {
      // The velocity threshold wasn't exceeded, so we shouldn't update the
      // mouse location. We do this to avoid unintended jitteriness of the
      // mouse. When we're in scroll mode, we don't want to apply the velocity
      // threshold because we're not visibly moving the mouse.
      return;
    }

    scaledVel.x = this.applyVelocityThreshold_(scaledVel.x);
    scaledVel.y = this.applyVelocityThreshold_(scaledVel.y);
    // The mouse location is the previous location plus the velocity.
    const newX = this.mouseLocation_.x + scaledVel.x;
    const newY = this.mouseLocation_.y + scaledVel.y;

    // Update mouse location: onMouseMovedOrChanged_ is async and may not
    // be called again until after another point is received from the
    // face tracking, so better to keep a fresh copy.
    // Clamp to screen bounds.
    // TODO(b/309121742): Handle multiple displays.
    this.mouseLocation_ = {
      x: Math.max(
          Math.min(this.screenBounds_.width, Math.round(newX)),
          this.screenBounds_.left),
      y: Math.max(
          Math.min(this.screenBounds_.height, Math.round(newY)),
          this.screenBounds_.top),
    };

    if (this.scrollModeController_.active()) {
      this.scrollModeController_.scroll(this.mouseLocation_);
      return;
    }

    // Only update if it's been long enough since the last time the user
    // touched their physical mouse or trackpad.
    if (new Date().getTime() - this.lastMouseMovedTime_ >
        MouseController.IGNORE_UPDATES_AFTER_MOUSE_MOVE_MS) {
      let mappedLocation = this.mouseLocation_;
      if (this.useGravity_) {
        // If gravity is enabled, adjust the cursor position.
        mappedLocation = this.mapPoint_(mappedLocation);
      }
      EventGenerator.sendMouseMove(mappedLocation.x, mappedLocation.y);
      chrome.accessibilityPrivate.setCursorPosition(mappedLocation);
    }
  }

  /**
   * Maps a point from screen space to screen space, adjusting the position to
   * pull the cursor toward buttons.
   */
  private mapPoint_(point: FloatingPoint2D): FloatingPoint2D {
    if (!this.screenBounds_) {
      // Early return if things aren't initialized.
      return point;
    }

    // TODO(b/309121742): Remove this test when the fencepost bug in the
    // position is fixed.
    if (point.x < 0 || point.y < 0 || point.x >= this.screenBounds_.width ||
        point.y >= this.screenBounds_.height) {
      // Ignore points off the screen.
      return point;
    }

    // Get the position delta from the gravity field and adjust the point.
    const offset = this.gravityField_[this.gravityOffset_(point.x, point.y)];
    return {
      x: Math.floor(point.x + offset.x),
      y: Math.floor(point.y + offset.y),
    };
  }

  /**
   * Reset the Gravity field to zero vectors and remove cached nodes.
   */
  private resetGravity_(): void {
    if (!this.useGravity_ || !this.screenBounds_) {
      return;
    }
    this.gravityField_ =
        new Array(this.screenBounds_.width * this.screenBounds_.height);
    this.gravityField_.fill({x: 0, y: 0});
    this.gravityNodes_.clear();
  }

  /**
   * Update the gravity field to the current state of the screen.  This is
   * called every GRAVITY_INTERVAL_MS.
   */
  private refreshGravity_(): void {
    if (!this.desktop_) {
      return;
    }

    const added: Map<string, ScreenRect> = new Map();

    // Add all buttons to the current set.
    const buttons = this.desktop_.findAll({role: RoleType.BUTTON});
    buttons.forEach(button => {
      if (!NodeUtils.isNodeInvisible(button, /*includeOffscreen*/ false)) {
        // ScreenRects don't work with Set(), so use the string representation
        // as the key.
        const id = RectUtil.toString(button.location);
        added.set(id, button.location);
      }
    });

    // Check the cached nodes for deleted nodes.
    this.gravityNodes_.forEach((bounds, id) => {
      if (!added.has(id)) {
        this.adjustGravityWell_(id, bounds, /*add*/ false);
      }
    });

    // Update the gravity field with the existing nodes.
    added.forEach((bounds, id) => {
      this.adjustGravityWell_(id, bounds, /*add*/ true);
    });
  }

  /**
   * Returns the offset in the gravity field for a given coordinate.
   */
  private gravityOffset_(x: number, y: number): number {
    if (!this.screenBounds_) {
      throw 'screenBounds_ is not set';
    }
    return Math.floor(x) + this.screenBounds_.width * Math.floor(y);
  }

  private adjustGravityWell_(id: string, bounds: ScreenRect, add: boolean):
      void {
    if (!this.screenBounds_) {
      return;
    }

    // Check if we're adding or removing.
    if (this.gravityNodes_.has(id)) {
      if (add) {
        // Adding the node, return if the node already exists.
        return;
      } else {
        // Removing the node.
        this.gravityNodes_.delete(id);
      }
    } else {
      if (!add) {
        // Removing the node, return if the node doesn't exist.
        return;
      } else {
        // Adding the node.
        this.gravityNodes_.set(id, bounds);
      }
    }

    // Bounds of the region that will be affected.
    const startX = Math.floor(Math.max(
        0, bounds.left - bounds.width * MouseController.GRAVITY_SCALE));
    const startY = Math.floor(Math.max(
        0, bounds.top - bounds.height * MouseController.GRAVITY_SCALE));
    const endX = Math.floor(Math.min(
        this.screenBounds_.width,
        RectUtil.right(bounds) + bounds.width * MouseController.GRAVITY_SCALE));
    const endY = Math.floor(Math.min(
        this.screenBounds_.height,
        RectUtil.bottom(bounds) +
            bounds.height * MouseController.GRAVITY_SCALE));
    const center = RectUtil.center(bounds);
    const xRange = bounds.width * MouseController.GRAVITY_SCALE;
    const yRange = bounds.height * MouseController.GRAVITY_SCALE;

    for (let y = startY; y < endY; y++) {
      for (let x = startX; x < endX; x++) {
        // Gravity is applied as a factor of the square of the size of the
        // control, increasing with proximity.
        let deltaX = center.x - x;
        let deltaY = center.y - y;
        const scaleX = 1 - Math.abs(deltaX / xRange);
        const scaleY = 1 - Math.abs(deltaY / yRange);
        deltaX = deltaX * scaleX * scaleX;
        deltaY = deltaY * scaleY * scaleY;

        if (!add) {
          // If the node is being removed, subtract the vector.
          deltaX = -deltaX;
          deltaY = -deltaY;
        }

        // Read the current value from the field and adjust it.
        const delta = this.gravityField_[this.gravityOffset_(x, y)];
        this.gravityField_[this.gravityOffset_(x, y)] = {
          x: delta.x + deltaX,
          y: delta.y + deltaY,
        };
      }
    }
  }

  private addPointToBuffer_(point: FloatingPoint2D): void {
    // Add this latest point to the buffer.
    if (this.buffer_.length === this.targetBufferSize_) {
      this.buffer_.shift();
    }
    // Fill the buffer with this point until we reach buffer size.
    while (this.buffer_.length < this.targetBufferSize_) {
      this.buffer_.push(point);
    }
  }

  mouseLocation(): ScreenPoint|undefined {
    return this.mouseLocation_;
  }

  resetLocation(): void {
    if (this.paused_ || !this.screenBounds_ ||
        this.scrollModeController_.active()) {
      return;
    }

    const x =
        Math.round(this.screenBounds_.width / 2) + this.screenBounds_.left;
    const y =
        Math.round(this.screenBounds_.height / 2) + this.screenBounds_.top;
    this.mouseLocation_ = {x, y};
    chrome.accessibilityPrivate.setCursorPosition({x, y});
  }

  reset(): void {
    this.stop();
    this.onMouseMovedHandler_.stop();
    this.onMouseDraggedHandler_.stop();
  }

  stop(): void {
    if (this.longClickActive_ && this.mouseLocation_) {
      // Release the existing long click action when the mouse controller is
      // stopped to ensure we do not leave the user in a permanent "drag" state.
      EventGenerator.sendMouseRelease(
          this.mouseLocation_.x, this.mouseLocation_.y);
    }
    if (this.mouseInterval_ !== -1) {
      clearInterval(this.mouseInterval_);
      this.mouseInterval_ = -1;
    }
    if (this.refreshGravityInterval_ !== -1) {
      clearInterval(this.refreshGravityInterval_);
      this.refreshGravityInterval_ = -1;
    }
    this.desktop_ = undefined;
    this.gravityField_ = [];
    this.gravityNodes_.clear();

    this.lastLandmarkLocation_ = undefined;
    this.previousSmoothedLocation_ = undefined;
    this.lastMouseMovedTime_ = 0;
    this.buffer_ = [];
    this.paused_ = false;
    chrome.settingsPrivate.onPrefsChanged.removeListener(this.prefsListener_);
  }

  togglePaused(): void {
    const newPaused = !this.paused_;
    // Run start/stop before assigning the new pause value, since start/stop
    // will modify the pause value.
    newPaused ? this.stop() : this.start();
    this.paused_ = newPaused;
  }

  toggleScrollMode(): void {
    this.scrollModeController_.toggle(this.mouseLocation_, this.screenBounds_);
    if (!this.isScrollModeActive()) {
      this.bubbleController_.resetBubble();
    }
  }

  toggleLongClick(): void {
    if (!this.mouseLocation_) {
      return;
    }

    this.longClickActive_ = !this.longClickActive_;

    if (this.longClickActive_) {
      EventGenerator.sendMousePress(
          this.mouseLocation_.x, this.mouseLocation_.y,
          SyntheticMouseEventButton.LEFT);
    } else {
      EventGenerator.sendMouseRelease(
          this.mouseLocation_.x, this.mouseLocation_.y);
    }
  }

  /** Listener for when the mouse position changes. */
  private onMouseMovedOrDragged_(event: chrome.automation.AutomationEvent):
      void {
    if (event.eventFrom === 'user') {
      // Mouse changes that aren't synthesized should actually move the mouse.
      // Assume all synthesized mouse movements come from within FaceGaze.
      this.mouseLocation_ = {x: event.mouseX, y: event.mouseY};
      this.lastMouseMovedTime_ = new Date().getTime();

      if (this.scrollModeController_.active()) {
        // Scroll mode honors physical mouse movements.
        this.scrollModeController_.updateScrollLocation(this.mouseLocation_);
      }

      if (this.longClickActive_) {
        // Send a synthetic drag event from the user's mouse move event.
        // FaceGaze cursor control should already have sent a synthetic drag
        // event, so this only needs to occur on user mouse movements.
        EventGenerator.sendMouseMove(event.mouseX, event.mouseY);
      }
    }
  }

  /**
   * Construct a kernel for smoothing the recent facegaze points.
   * Specifically, this is an exponential curve with amplitude of 0.92 and
   * y-intercept of 0.08. This ensures that the curve hits the points (0, 0.08)
   * and (1, 1).
   * Note: Whenever the buffer size is updated, we must reconstruct
   * the smoothing kernel so that it is the right length.
   */
  private calcSmoothKernel_(): void {
    this.smoothKernel_ = [];
    let sum = 0;
    const step = 1 / this.targetBufferSize_;
    // We use values step <= i <= 1 to determine the weight of each point.
    for (let i = step; i <= 1; i += step) {
      const smoothFactor = Math.E;
      const numerator = (Math.E ** (smoothFactor * i)) - 1;
      const denominator = (Math.E ** smoothFactor) - 1;
      const value = 0.92 * (numerator / denominator) + 0.08;
      this.smoothKernel_.push(value);
      sum += value;
    }
    for (let i = 0; i < this.targetBufferSize_; i++) {
      this.smoothKernel_[i] /= sum;
    }
  }

  /**
   * Applies the `smoothKernel_` to the `buffer_` of recent points to generate
   * a single point.
   */
  private applySmoothing_(): FloatingPoint2D {
    const result = {x: 0, y: 0};
    for (let i = 0; i < this.targetBufferSize_; i++) {
      const kernelPart = this.smoothKernel_[i];
      result.x += this.buffer_[i].x * kernelPart;
      result.y += this.buffer_[i].y * kernelPart;
    }
    return result;
  }

  /**
   * Magnifies velocities. This means the user has to move their head less far
   * to get to the edges of the screens.
   */
  private asymmetryScale_(vel: FloatingPoint2D): FloatingPoint2D {
    if (vel.x > 0) {
      vel.x *= this.spdRight_;
    } else {
      vel.x *= this.spdLeft_;
    }
    if (vel.y > 0) {
      vel.y *= this.spdDown_;
    } else {
      vel.y *= this.spdUp_;
    }
    return vel;
  }

  /**
   * Calculate a sigmoid function that creates an S curve with
   * a y intercept around ~.2 for velocity === 0 and
   * approaches 1.2 around velocity of 22. Change is near-linear
   * around velocities 0 to 9, centered at velocity of five.
   */
  private applySigmoidAcceleration_(velocity: number): number {
    const shift = 5;
    const slope = 0.3;
    const multiply = 1.2;

    velocity = Math.abs(velocity);
    const sig = 1 / (1 + Math.exp(-slope * (velocity - shift)));
    return multiply * sig;
  }

  private updateFromPrefs_(prefs: PrefObject[]): void {
    prefs.forEach(pref => {
      switch (pref.key) {
        case MouseController.PREF_SPD_UP:
          if (pref.value) {
            this.spdUp_ = pref.value;
            this.calcVelocityThreshold_();
          }
          break;
        case MouseController.PREF_SPD_DOWN:
          if (pref.value) {
            this.spdDown_ = pref.value;
            this.calcVelocityThreshold_();
          }
          break;
        case MouseController.PREF_SPD_LEFT:
          if (pref.value) {
            this.spdLeft_ = pref.value;
            this.calcVelocityThreshold_();
          }
          break;
        case MouseController.PREF_SPD_RIGHT:
          if (pref.value) {
            this.spdRight_ = pref.value;
            this.calcVelocityThreshold_();
          }
          break;
        case MouseController.PREF_CURSOR_SMOOTHING:
          if (pref.value) {
            this.targetBufferSize_ = pref.value;
            this.calcSmoothKernel_();
            while (this.buffer_.length > this.targetBufferSize_) {
              this.buffer_.shift();
            }
          }
          break;
        case MouseController.PREF_CURSOR_USE_ACCELERATION:
          if (pref.value !== undefined) {
            this.useMouseAcceleration_ = pref.value;
          }
          break;
        case MouseController.PREF_VELOCITY_THRESHOLD:
          if (pref.value !== undefined) {
            // Ensure threshold factor is a decimal value.
            this.velocityThresholdFactor_ =
                pref.value / MouseController.MAX_VELOCITY_THRESHOLD_PREF_VALUE;
            this.calcVelocityThreshold_();
          }
          break;
        default:
          return;
      }
    });
  }

  private calcVelocityThreshold_(): void {
    // Threshold is a function of speed. Threshold increases as speed increases
    // because it's easier to move the mouse accidentally at high mouse speeds.
    // The velocity threshold factor can be tuned by the user.
    const averageSpeed =
        (this.spdUp_ + this.spdDown_ + this.spdLeft_ + this.spdRight_) / 4;
    this.velocityThreshold_ = averageSpeed * this.velocityThresholdFactor_;
  }

  private exceedsVelocityThreshold_(velocity: number): boolean {
    if (!this.useVelocityThreshold_) {
      return true;
    }

    return Math.abs(velocity) > this.velocityThreshold_;
  }

  private applyVelocityThreshold_(velocity: number): number {
    if (!this.useVelocityThreshold_) {
      return velocity;
    }

    if (Math.abs(velocity) < this.velocityThreshold_) {
      return 0;
    }

    return (velocity > 0) ? velocity - this.velocityThreshold_ :
                            velocity + this.velocityThreshold_;
  }

  setLandmarkWeightsForTesting(useWeights: boolean): void {
    if (!useWeights) {
      // If we don't want to use landmark weights, we should default to the
      // forehead location.
      this.landmarkWeights_ = new Map();
      this.landmarkWeights_.set('forehead', 1);
    }
  }

  setVelocityThresholdForTesting(useThreshold: boolean): void {
    this.useVelocityThreshold_ = useThreshold;
  }
}

export namespace MouseController {
  /**
   * The indices of the tracked landmarks in a FaceLandmarkerResult.
   * See all landmarks at
   * https://storage.googleapis.com/mediapipe-assets/documentation/mediapipe_face_landmark_fullsize.png.
   */
  export const LANDMARK_INDICES = [
    {name: LandmarkType.FOREHEAD, index: 8},
    {name: LandmarkType.FOREHEAD_TOP, index: 10},
    {name: LandmarkType.NOSE_TIP, index: 4},
    {name: LandmarkType.RIGHT_TEMPLE, index: 127},
    {name: LandmarkType.LEFT_TEMPLE, index: 356},
    // Rotation does not have a landmark index, but is included in this list
    // because it can be used as a landmark.
    {name: LandmarkType.ROTATION, index: -1},
  ];

  /**
   * The maximum value for the velocity threshold pref. We use this to ensure
   * this.velocityThresholdFactor_ is a decimal.
   */
  export const MAX_VELOCITY_THRESHOLD_PREF_VALUE = 20;

  /** How frequently to run the mouse movement logic. */
  export const MOUSE_INTERVAL_MS = 16;

  /**
   * How long to wait after the user moves the mouse with a physical device
   * before moving the mouse with facegaze.
   */
  export const IGNORE_UPDATES_AFTER_MOUSE_MOVE_MS = 500;

  // Pref names. Should be in sync with with values at ash_pref_names.h.
  export const PREF_SPD_UP = 'settings.a11y.face_gaze.cursor_speed_up';
  export const PREF_SPD_DOWN = 'settings.a11y.face_gaze.cursor_speed_down';
  export const PREF_SPD_LEFT = 'settings.a11y.face_gaze.cursor_speed_left';
  export const PREF_SPD_RIGHT = 'settings.a11y.face_gaze.cursor_speed_right';
  export const PREF_CURSOR_SMOOTHING =
      'settings.a11y.face_gaze.cursor_smoothing';
  export const PREF_CURSOR_USE_ACCELERATION =
      'settings.a11y.face_gaze.cursor_use_acceleration';
  export const PREF_VELOCITY_THRESHOLD =
      'settings.a11y.face_gaze.velocity_threshold';

  // Default values. Will be overwritten by prefs.
  export const DEFAULT_MOUSE_SPEED = 10;
  export const DEFAULT_USE_MOUSE_ACCELERATION = true;
  export const DEFAULT_BUFFER_SIZE = 7;
  export const DEFAULT_VELOCITY_FACTOR = 0.45;

  export const GRAVITY_INTERVAL_MS = 500;
  // How far the gravity reaches, relative to the size of the control.
  export const GRAVITY_SCALE = 4;

  export function calculateRotationFromFacialTransformationMatrix(
      facialTransformationMatrix: Matrix): {x: number, y: number}|undefined {
    const mat = facialTransformationMatrix.data;
    const m11 = mat[0];
    const m12 = mat[1];
    const m13 = mat[2];
    const m21 = mat[4];
    const m22 = mat[5];
    const m23 = mat[6];
    const m31 = mat[8];
    const m32 = mat[9];
    const m33 = mat[10];

    if (m31 === 1) {
      // cos(theta) is 0, so theta is pi/2 or -pi/2.
      // This seems like the head would have to be pretty rotated so we can
      // probably safely ignore it for now.
      console.log('cannot process matrix with m[3][1] == 1 yet.');
      return;
    }

    // First compute scaling and rotation from the facial transformation matrix.
    // Taken from glmatrix, https://glmatrix.net/docs/mat4.js.html.
    const scaling = [
      Math.hypot(m11, m12, m13),
      Math.hypot(m21, m22, m23),
      Math.hypot(m31, m32, m33),
    ];
    // Translation is unused but could be used in the future. Leaving it here
    // so we don't have to re-compute the math later.
    // const translation = [mat[12], mat[13], mat[14]];

    // Scale the m values to create sm values; used for x and y axis rotation
    // computation. On Brya, scaling is basically all 1s, so we could ignore it.
    // TODO(b:309121742): Determine if we can remove scaling from computation,
    // and use the matrix values directly.
    const sm31 = m31 / scaling[0];
    const sm32 = m32 / scaling[1];
    const sm33 = m33 / scaling[2];

    // Convert rotation matrix to Euler angles. Refer to math in
    // https://eecs.qmul.ac.uk/~gslabaugh/publications/euler.pdf.
    // This has units in radians.
    const xRotation = -1 * Math.asin(sm31);
    const yRotation =
        Math.atan2(sm32 / Math.cos(xRotation), sm33 / Math.cos(xRotation));

    // z-axis rotation is head tilt, and not used at the moment. Later, it could
    // be used during calibration. Leaving it here so we don't need to
    // re-compute the math later. const sm11 = m11 * is1; const sm21 = m21 *
    // is1; const zRotation =
    // Math.atan2(sm21 / Math.cos(xRotation), sm11 / Math.cos(xRotation));

    const x = 0.5 - xRotation / (Math.PI * 2);
    const y = 0.5 - yRotation / (Math.PI * 2);
    return {x, y};
  }
}

TestImportManager.exportForTesting(MouseController);
