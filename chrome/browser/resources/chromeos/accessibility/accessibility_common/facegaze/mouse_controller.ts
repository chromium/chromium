// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AsyncUtil} from '/common/async_util.js';
import {EventGenerator} from '/common/event_generator.js';
import {EventHandler} from '/common/event_handler.js';

import {FaceLandmarkerResult} from '../third_party/mediapipe/task_vision/vision.js';

import ScreenRect = chrome.accessibilityPrivate.ScreenRect;
import ScreenPoint = chrome.accessibilityPrivate.ScreenPoint;

// A ScreenPoint represents an integer screen coordinate, whereas
// a FloatingPoint2D represents a (x, y) floating point number
// (which may be used for screen position or velocity).
interface FloatingPoint2D {
  x: number;
  y: number;
}

/** Handles all interaction with the mouse. */
export class MouseController {
  /** Last seen mouse location (cached from event in onMouseMovedOrDragged_). */
  private mouseLocation_: ScreenPoint|undefined;
  private onMouseMovedHandler_: EventHandler;
  private onMouseDraggedHandler_: EventHandler;
  private screenBounds_: ScreenRect|undefined;

  /** These should come from user prefs, but are currently simply hard-coded. */
  private targetBufferSize_ = MouseController.BUFFER_SIZE;
  private useMouseAcceleration_ = MouseController.USE_MOUSE_ACCELERATION;
  private spdRight_ = MouseController.SPD_RIGHT;
  private spdLeft_ = MouseController.SPD_LEFT;
  private spdUp_ = MouseController.SPD_UP;
  private spdDown_ = MouseController.SPD_DOWN;

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

  constructor() {
    this.onMouseMovedHandler_ = new EventHandler(
        [], chrome.automation.EventType.MOUSE_MOVED,
        event => this.onMouseMovedOrDragged_(event));

    this.onMouseDraggedHandler_ = new EventHandler(
        [], chrome.automation.EventType.MOUSE_DRAGGED,
        event => this.onMouseMovedOrDragged_(event));

    this.calcSmoothKernel_();
  }

  async init(): Promise<void> {
    chrome.accessibilityPrivate.enableMouseEvents(true);
    const desktop = await AsyncUtil.getDesktop();
    this.onMouseMovedHandler_.setNodes(desktop);
    this.onMouseMovedHandler_.start();
    this.onMouseDraggedHandler_.setNodes(desktop);
    this.onMouseDraggedHandler_.start();

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

    // Start the logic to move the mouse.
    this.mouseInterval_ = setInterval(
        () => this.updateMouseLocation_(), MouseController.MOUSE_INTERVAL_MS);
  }

  /**
   * Update the current location of the tracked face landmark.
   */
  onFaceLandmarkerResult(result: FaceLandmarkerResult): void {
    if (!this.screenBounds_) {
      return;
    }
    if (!result.faceLandmarks || !result.faceLandmarks[0] ||
        !result.faceLandmarks[0][MouseController.FOREHEAD_LANDMARK_INDEX]) {
      return;
    }

    // These scale from 0 to 1.
    const foreheadLocation =
        result.faceLandmarks[0][MouseController.FOREHEAD_LANDMARK_INDEX];

    // Calculate the absolute position on the screen, where the top left
    // corner represents (0,0) and the bottom right corner represents
    // (this.screenBounds_.width, this.screenBounds_.height).
    // TODO(b/309121742): Handle multiple displays.
    const absoluteY = Math.round(
        foreheadLocation.y * this.screenBounds_.height +
        this.screenBounds_.top);
    // Reflect the x coordinate since the webcam doesn't mirror in the
    // horizontal direction.
    const scaledX = Math.round(foreheadLocation.x * this.screenBounds_.width);
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
    if (!this.lastLandmarkLocation_ || !this.mouseLocation_ ||
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

    // Only update if it's been long enough since the last time the user
    // touched their physical mouse or trackpad.
    if (new Date().getTime() - this.lastMouseMovedTime_ >
        MouseController.IGNORE_UPDATES_AFTER_MOUSE_MOVE_MS) {
      chrome.accessibilityPrivate.setCursorPosition(this.mouseLocation_);
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

  clickLeft(): void {
    if (!this.mouseLocation_) {
      return;
    }
    EventGenerator.sendMouseClick(this.mouseLocation_.x, this.mouseLocation_.y);
  }

  clickRight(): void {
    if (!this.mouseLocation_) {
      return;
    }
    EventGenerator.sendMouseClick(
        this.mouseLocation_.x, this.mouseLocation_.y, {
          mouseButton:
              chrome.accessibilityPrivate.SyntheticMouseEventButton.RIGHT,
          delayMs: 0,
        });
  }

  resetLocation(): void {
    if (!this.screenBounds_) {
      return;
    }
    const x =
        Math.round(this.screenBounds_.width / 2) + this.screenBounds_.left;
    const y =
        Math.round(this.screenBounds_.height / 2) + this.screenBounds_.top;
    this.mouseLocation_ = {x, y};
    chrome.accessibilityPrivate.setCursorPosition({x, y});
  }

  stopEventListeners(): void {
    this.onMouseMovedHandler_.stop();
    this.onMouseDraggedHandler_.stop();
    if (this.mouseInterval_ !== -1) {
      clearInterval(this.mouseInterval_);
      this.mouseInterval_ = -1;
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
    }
  }

  /**
   * Construct a kernel for smoothing the recent facegaze points.
   * Specifically, this is a Hamming curve with M = BUFFER_SIZE * 2,
   * matching the project-gameface Python implementation.
   * Note: Whenever the buffer size is updated, we must reconstruct
   * the smoothing kernel so that it is the right length.
   */
  private calcSmoothKernel_(): void {
    let sum = 0;
    for (let i = 0; i < this.targetBufferSize_; i++) {
      const value = .54 -
          .46 * Math.cos((2 * Math.PI * i) / (this.targetBufferSize_ * 2 - 1));
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
   * a y intercept around ~.2 for velocity == 0 and
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
}

export namespace MouseController {
  /** The index of the forehead landmark in a FaceLandmarkerResult. */
  export const FOREHEAD_LANDMARK_INDEX = 8;

  /** How frequently to run the mouse movement logic. */
  export const MOUSE_INTERVAL_MS = 16;

  // TODO(b/309121742): These constants could become prefs.

  /**
   * How long to wait after the user moves the mouse with a physical device
   * before moving the mouse with facegaze.
   */
  export const IGNORE_UPDATES_AFTER_MOUSE_MOVE_MS = 500;

  export const BUFFER_SIZE = 6;
  export const USE_MOUSE_ACCELERATION = true;
  export const SPD_RIGHT = 20;
  export const SPD_LEFT = 20;
  export const SPD_DOWN = 20;
  export const SPD_UP = 20;
}
