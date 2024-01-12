// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AsyncUtil} from '../../common/async_util.js';
import {EventGenerator} from '../../common/event_generator.js';
import {EventHandler} from '../../common/event_handler.js';

import {FaceLandmarkerResult} from './mediapipe_task_vision/vision.js';

import ScreenRect = chrome.accessibilityPrivate.ScreenRect;

/** Handles all interaction with the mouse. */
export class MouseController {
  /** Last seen mouse location (cached from event in onMouseMovedOrDragged_). */
  private mouseLocation_: chrome.accessibilityPrivate.ScreenPoint|undefined;
  private onMouseMovedHandler_: EventHandler;
  private onMouseDraggedHandler_: EventHandler;
  private screenBounds_: ScreenRect|undefined;
  /**
   * TODO(b/317235785): Set this according to the user's preference.
   * The delta value that must be exceeded for FaceGaze to move the mouse.
   * Represented in density-independent pixels.
   */
  private movementThreshold_ = 0;

  constructor() {

    this.onMouseMovedHandler_ = new EventHandler(
        [], chrome.automation.EventType.MOUSE_MOVED,
        event => this.onMouseMovedOrDragged_(event));

    this.onMouseDraggedHandler_ = new EventHandler(
        [], chrome.automation.EventType.MOUSE_DRAGGED,
        event => this.onMouseMovedOrDragged_(event));
  }

  async init(): Promise<void> {
    chrome.accessibilityPrivate.enableMouseEvents(true);
    const desktop = await AsyncUtil.getDesktop();
    this.onMouseMovedHandler_.setNodes(desktop);
    this.onMouseMovedHandler_.start();
    this.onMouseDraggedHandler_.setNodes(desktop);
    this.onMouseDraggedHandler_.start();

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

    const foreheadLocation =
        result.faceLandmarks[0][MouseController.FOREHEAD_LANDMARK_INDEX];
    // These scale from 0 to 1.
    const x = foreheadLocation.x;
    const y = foreheadLocation.y;

    // Calculate the absolute position on the screen, where the top left
    // corner represents (0,0) and the bottom right corner represents
    // (this.screenBounds_.width, this.screenBounds_.height).
    // TODO(b/309121742): Handle multiple displays.
    const absoluteY = Math.round(y * this.screenBounds_.height);
    // Reflect the x coordinate since the webcam doesn't mirror in the
    // horizontal direction.
    const scaledX = Math.round(x * this.screenBounds_.width);
    const absoluteX = (scaledX * -1) + this.screenBounds_.width;

    // Now translate this into a position on a coordinate system where (0,0) is
    // at the center of the screen. This will give us delta values for x and y.
    const deltaX = absoluteX - (this.screenBounds_.width / 2);
    const deltaY = (absoluteY * -1) + (this.screenBounds_.height / 2);

    // Apply deltas to current mouse position. `this.mouseLocation_` is
    // operating on the coordinate system where (0,0) is in the top left corner,
    // so ensure these deltas are applied in the correct direction.
    // TODO(b/317235785): Apply sensitivity and smoothing to improve the user
    // experience.
    if (Math.abs(deltaX) > this.movementThreshold_ &&
        Math.abs(deltaY) > this.movementThreshold_) {
      const newX = this.mouseLocation_.x + deltaX;
      const newY = this.mouseLocation_.y - deltaY;
      chrome.accessibilityPrivate.setCursorPosition({x: newX, y: newY});
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
    const x = this.screenBounds_.width / 2;
    const y = this.screenBounds_.height / 2;
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
}

export namespace MouseController {
  /** The index of the forehead landmark in a FaceLandmarkerResult. */
  export const FOREHEAD_LANDMARK_INDEX = 8;
}
