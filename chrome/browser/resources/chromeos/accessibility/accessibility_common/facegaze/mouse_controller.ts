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

  /** Smoothing buffer size. */
  private bufferSize_ = MouseController.BUFFER_SIZE;

  /** The most recent raw face landmark mouse locations. */
  private buffer_: ScreenPoint[] = [];

  /** Used for smoothing the recent points in the buffer. */
  private smoothKernel_: number[] = [];

  /** The most recent smoothed mouse location. */
  private previousSmoothedLocation_: FloatingPoint2D|undefined;

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
  }

  /**
   * Uses the forehead landmark to update the location of the mouse.
   * This function doesn't use the absolute position of the forehead
   * landmark. Instead, it calculates deltas to be applied to the
   * current mouse location based on the forehead's location relative
   * to the center of the screen.
   */
  updateMouseLocation(result: FaceLandmarkerResult): void {
    if (!this.screenBounds_ || !this.mouseLocation_) {
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
    // TODO(b/309121742): Handle multiple displays, and displays where
    // the (left, top) is not (0, 0).
    const absoluteY =
        Math.round(foreheadLocation.y * this.screenBounds_.height);
    // Reflect the x coordinate since the webcam doesn't mirror in the
    // horizontal direction.
    const scaledX = Math.round(foreheadLocation.x * this.screenBounds_.width);
    const absoluteX = (scaledX * -1) + this.screenBounds_.width;

    // Add this latest point to the buffer.
    if (this.buffer_.length === this.bufferSize_) {
      this.buffer_.shift();
    }
    this.buffer_.push({x: absoluteX, y: absoluteY});
    while (this.buffer_.length < this.bufferSize_) {
      this.buffer_.push({x: absoluteX, y: absoluteY});
    }

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

    // The mouse location is the previous location plus the velocity.
    const newX = this.mouseLocation_.x + scaledVel.x;
    const newY = this.mouseLocation_.y + scaledVel.y;

    // Update mouse location: onMouseMovedOrChanged_ is async and may not
    // be called again until after another point is received from the
    // face tracking, so better to keep a fresh copy.
    this.mouseLocation_ = {x: Math.round(newX), y: Math.round(newY)};
    chrome.accessibilityPrivate.setCursorPosition(this.mouseLocation_);
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
    const x = Math.round(this.screenBounds_.width / 2);
    const y = Math.round(this.screenBounds_.height / 2);
    this.mouseLocation_ = {x, y};
    chrome.accessibilityPrivate.setCursorPosition({x, y});
  }

  stopEventListeners(): void {
    this.onMouseMovedHandler_.stop();
    this.onMouseDraggedHandler_.stop();
  }

  /** Listener for when the mouse position changes. */
  private onMouseMovedOrDragged_(event: chrome.automation.AutomationEvent):
      void {
    this.mouseLocation_ = {x: event.mouseX, y: event.mouseY};
  }

  /**
   * Construct a kernel for smoothing the recent facegaze points.
   * Specifically, this is a Hamming curve with M = BUFFER_SIZE * 2,
   * matching the project-gameface Python implementation.
   */
  private calcSmoothKernel_(): void {
    let sum = 0;
    for (let i = 0; i < this.bufferSize_; i++) {
      const value =
          .54 - .46 * Math.cos((2 * Math.PI * i) / (this.bufferSize_ * 2 - 1));
      this.smoothKernel_.push(value);
      sum += value;
    }
    for (let i = 0; i < this.bufferSize_; i++) {
      this.smoothKernel_[i] /= sum;
    }
  }

  /**
   * Applies the `smoothKernel_` to the `buffer_` of recent points to generate
   * a single point.
   */
  private applySmoothing_(): FloatingPoint2D {
    const result = {x: 0, y: 0};
    for (let i = 0; i < this.bufferSize_; i++) {
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
      vel.x *= MouseController.SPD_RIGHT;
    } else {
      vel.x *= MouseController.SPD_LEFT;
    }
    if (vel.y > 0) {
      vel.y *= MouseController.SPD_DOWN;
    } else {
      vel.y *= MouseController.SPD_UP;
    }
    return vel;
  }
}

export namespace MouseController {
  /** The index of the forehead landmark in a FaceLandmarkerResult. */
  export const FOREHEAD_LANDMARK_INDEX = 8;

  // TODO(b/309121742): These constants should become prefs.
  export const BUFFER_SIZE = 6;
  export const SPD_RIGHT = 20;
  export const SPD_LEFT = 20;
  export const SPD_DOWN = 20;
  export const SPD_UP = 20;
}
