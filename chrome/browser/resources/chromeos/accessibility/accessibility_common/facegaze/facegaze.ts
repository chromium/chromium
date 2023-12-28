// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AsyncUtil} from '../../common/async_util.js';
import {EventGenerator} from '../../common/event_generator.js';
import {EventHandler} from '../../common/event_handler.js';

import {MediapipeAvailability} from './mediapipe_availability.js';
import {FaceLandmarkerResult} from './mediapipe_task_vision/vision.js';

/** The facial gestures that are supported by FaceGaze. */
export enum FacialGesture {
  BROW_DOWN_LEFT = 'browDownLeft',
  BROW_DOWN_RIGHT = 'browDownRight',
  BROW_INNER_UP = 'browInnerUp',
  JAW_OPEN = 'jawOpen',
  MOUTH_LEFT = 'mouthLeft',
  MOUTH_RIGHT = 'mouthRight',
}

/**
 * TODO(b/309121742): Move this into a dedicated class for action fulfillment.
 * The actions that are supported by FaceGaze.
 */
export enum Action {
  CLICK_LEFT = 'clickLeft',
  CLICK_RIGHT = 'clickRight',
  RESET_MOUSE = 'resetMouse',
}

interface ActionData {
  action: Action;
  confidenceThreshold: number;
}

/** Main class for FaceGaze. */
export class FaceGaze {
  /** Last seen mouse location (cached from event in onMouseMovedOrDragged_). */
  declare private mouseLocation_: chrome.accessibilityPrivate.ScreenPoint|null;
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
  private gestureToActionData_: Map<FacialGesture, ActionData> = new Map();

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

  /** Destructor to remove any listeners. */
  onFaceGazeDisabled(): void {
    this.onMouseMovedHandler_.stop();
    this.onMouseDraggedHandler_.stop();
  }

  /** Initializes FaceGaze. */
  private async init_(): Promise<void> {
    // Initialize default mapping of facial gestures to actions.
    // TODO(b/309121742): Set this using the user's preferences.
    this.gestureToActionData_
        .set(
            FacialGesture.JAW_OPEN,
            FaceGaze.createDefaultActionData(Action.CLICK_LEFT))
        .set(
            FacialGesture.BROW_INNER_UP,
            FaceGaze.createDefaultActionData(Action.CLICK_RIGHT))
        .set(
            FacialGesture.BROW_DOWN_LEFT,
            FaceGaze.createDefaultActionData(Action.RESET_MOUSE))
        .set(
            FacialGesture.BROW_DOWN_RIGHT,
            FaceGaze.createDefaultActionData(Action.RESET_MOUSE));

    chrome.accessibilityPrivate.enableMouseEvents(true);
    const desktop = await AsyncUtil.getDesktop();
    this.onMouseMovedHandler_.setNodes(desktop);
    this.onMouseMovedHandler_.start();
    this.onMouseDraggedHandler_.setNodes(desktop);
    this.onMouseDraggedHandler_.start();

    this.connectToWebCam_();

    // TODO(b/309121742): Listen to magnifier bounds changed so as to update
    // cursor relative position logic when magnifier is running.
  }

  /** Listener for when the mouse position changes. */
  private onMouseMovedOrDragged_(event: chrome.automation.AutomationEvent):
      void {
    this.mouseLocation_ = {x: event.mouseX, y: event.mouseY};
  }

  private connectToWebCam_(): void {
    if (!MediapipeAvailability.isAvailable()) {
      // Mediapipe is required to interpret camera data, so only connect to
      // the webcam if mediapipe is available.
      return;
    }

    // Open camera_stream.html, which will connect to the webcam and pass
    // FaceLandmarker results back to the background page. Use chrome.windows
    // API to ensure page is opened in Ash-chrome.
    const params = {
      url: chrome.runtime.getURL(
          'accessibility_common/facegaze/camera_stream.html'),
      type: chrome.windows.CreateType.PANEL,
    };
    chrome.windows.create(params, () => {
      chrome.runtime.onMessage.addListener(message => {
        if (message.type === 'faceLandmarkerResult') {
          this.processFaceLandmarkerResult_(message.result);
        }

        return false;
      });
    });
  }

  /**
   * TODO(b/309121742): Add throttling here so that we don't perform duplicate
   * actions.
   */
  private processFaceLandmarkerResult_(result: FaceLandmarkerResult): void {
    if (!result) {
      return;
    }

    // TODO(b/309121742): Move logic for mouse movement, gesture detection,
    // and action fulfillment into their own classes.
    this.updateMouseLocation_(result);
    this.detectGesturesAndPerformActions_(result);
  }

  /**
   * Uses the forehead landmark to update the location of the mouse.
   * This function doesn't use the absolute position of the forehead
   * landmark. Instead, it calculates deltas to be applied to the
   * current mouse location based on the forehead's location relative
   * to the center of the screen.
   */
  private updateMouseLocation_(result: FaceLandmarkerResult): void {
    if (!result.faceLandmarks || !result.faceLandmarks[0] ||
        !result.faceLandmarks[0][FaceGaze.FOREHEAD_LANDMARK_INDEX]) {
      return;
    }

    const foreheadLocation =
        result.faceLandmarks[0][FaceGaze.FOREHEAD_LANDMARK_INDEX];
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

  /**
   * TODO(b/309121742): Gestures can be detected multiple times in a row, so
   * implement throttling/filtering so that actions aren't unintentionally
   * spammed.
   * Iterates through all detected facial gestures and performs associated
   * actions.
   */
  private detectGesturesAndPerformActions_(result: FaceLandmarkerResult): void {
    for (const classification of result.faceBlendshapes) {
      const browDownData = {
        [FacialGesture.BROW_DOWN_LEFT]: false,
        [FacialGesture.BROW_DOWN_RIGHT]: false,
      };

      for (const category of classification.categories) {
        const gesture = category.categoryName as FacialGesture;
        const data = this.gestureToActionData_.get(gesture);
        if (!data || (category.score < data.confidenceThreshold)) {
          // Skip over categories that don't meet the confidence threshold.
          continue;
        }

        if (gesture === FacialGesture.BROW_DOWN_LEFT ||
            gesture === FacialGesture.BROW_DOWN_RIGHT) {
          browDownData[gesture] = true;
          if (Object.values(browDownData).includes(false)) {
            // The BrowDown gesture is special because it is the combination of
            // two separate facial gestures. Ensure that the associated action
            // is only performed if both gestures are recognized.
            continue;
          }
        }

        this.performAction_(data.action);
      }
    }
  }

  private performAction_(action: Action): void {
    switch (action) {
      case Action.CLICK_LEFT:
        EventGenerator.sendMouseClick(
            this.mouseLocation_!.x, this.mouseLocation_!.y);
        break;
      case Action.CLICK_RIGHT:
        EventGenerator.sendMouseClick(
            this.mouseLocation_!.x, this.mouseLocation_!.y, {
              mouseButton:
                  chrome.accessibilityPrivate.SyntheticMouseEventButton.RIGHT,
              delayMs: 0,
            });
        break;
      case Action.RESET_MOUSE:
        const x = this.screenBounds_.x / 2;
        const y = this.screenBounds_.y / 2;
        chrome.accessibilityPrivate.setCursorPosition({x, y});
        break;
    }
  }

  /** Returns an ActionData with a default confidence threshold. */
  static createDefaultActionData(action: Action): ActionData {
    return {action, confidenceThreshold: FaceGaze.DEFAULT_CONFIDENCE_THRESHOLD};
  }
}

export namespace FaceGaze {
  /** The default confidence threshold for facial gestures. */
  export const DEFAULT_CONFIDENCE_THRESHOLD = 0.6;

  /** The index of the forehead landmark in a FaceLandmarkerResult. */
  export const FOREHEAD_LANDMARK_INDEX = 8;
}
