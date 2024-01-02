// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AsyncUtil} from '../../common/async_util.js';
import {EventGenerator} from '../../common/event_generator.js';
import {EventHandler} from '../../common/event_handler.js';

import {FaceLandmarkerResult} from './mediapipe_task_vision/vision.js';

/** Handles all interaction with the mouse. */
export class MouseController {
  /** Last seen mouse location (cached from event in onMouseMovedOrDragged_). */
  private mouseLocation_: chrome.accessibilityPrivate.ScreenPoint|null;
  private onMouseMovedHandler_: EventHandler;
  private onMouseDraggedHandler_: EventHandler;
  /** TODO(b/309121742): Set this according to the device's screen bounds. */
  private screenBounds_: {x: number, y: number} = {x: 1200, y: 800};
  /**
   * TODO(b/309121742): Set this according to the user's preference.
   * The delta value that must be exceeded for FaceGaze to move the mouse.
   * Represented in density-independent pixels.
   */
  private movementThreshold_ = 0;

  constructor() {
    this.mouseLocation_ = null;

    this.onMouseMovedHandler_ = new EventHandler(
        [], chrome.automation.EventType.MOUSE_MOVED,
        event => this.onMouseMovedOrDragged_(event));

    this.onMouseDraggedHandler_ = new EventHandler(
        [], chrome.automation.EventType.MOUSE_DRAGGED,
        event => this.onMouseMovedOrDragged_(event));

    this.init_();
  }

  private async init_(): Promise<void> {
    chrome.accessibilityPrivate.enableMouseEvents(true);
    const desktop = await AsyncUtil.getDesktop();
    this.onMouseMovedHandler_.setNodes(desktop);
    this.onMouseMovedHandler_.start();
    this.onMouseDraggedHandler_.setNodes(desktop);
    this.onMouseDraggedHandler_.start();
  }

  /**
   * Uses the forehead landmark to update the location of the mouse.
   * This function doesn't use the absolute position of the forehead
   * landmark. Instead, it calculates deltas to be applied to the
   * current mouse location based on the forehead's location relative
   * to the center of the screen.
   */
  updateMouseLocation(result: FaceLandmarkerResult): void {
    if (!result.faceLandmarks || !result.faceLandmarks[0] ||
        !result.faceLandmarks[0][MouseController.FOREHEAD_LANDMARK_INDEX]) {
      return;
    }

    const foreheadLocation =
        result.faceLandmarks[0][MouseController.FOREHEAD_LANDMARK_INDEX];
    const x = foreheadLocation.x;
    const y = foreheadLocation.y;

    // Calculate the absolute position on the screen, where the top left
    // corner represents (0,0) and the bottom right corner represents
    // (this.screenBounds_.x, this.screenBounds_.y).
    const absoluteY = Math.round(y * this.screenBounds_.y);
    // Reflect the x coordinate since the webcam doesn't mirror in the
    // horizontal direction.
    const scaledX = Math.round(x * this.screenBounds_.x);
    const absoluteX = (scaledX * -1) + this.screenBounds_.x;

    // Now translate this into a position on a coordinate system where (0,0) is
    // at the center of the screen. This will give us delta values for x and y.
    const deltaX = absoluteX - (this.screenBounds_.x / 2);
    const deltaY = (absoluteY * -1) + (this.screenBounds_.y / 2);

    // Apply deltas to current mouse position. `this.mouseLocation_` is
    // operating on the coordinate system where (0,0) is in the top left corner,
    // so ensure these deltas are applied in the correct direction.
    // TODO(b/309121742): Apply sensitivity and smoothing to improve the user
    // experience.
    if (Math.abs(deltaX) > this.movementThreshold_ &&
        Math.abs(deltaY) > this.movementThreshold_) {
      const newX = this.mouseLocation_!.x + deltaX;
      const newY = this.mouseLocation_!.y - deltaY;
      chrome.accessibilityPrivate.setCursorPosition({x: newX, y: newY});
    }
  }

  clickLeft(): void {
    EventGenerator.sendMouseClick(
        this.mouseLocation_!.x, this.mouseLocation_!.y);
  }

  clickRight(): void {
    EventGenerator.sendMouseClick(
        this.mouseLocation_!.x, this.mouseLocation_!.y, {
          mouseButton:
              chrome.accessibilityPrivate.SyntheticMouseEventButton.RIGHT,
          delayMs: 0,
        });
  }

  resetLocation(): void {
    const x = this.screenBounds_.x / 2;
    const y = this.screenBounds_.y / 2;
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
