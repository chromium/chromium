// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MediapipeAvailability} from '../third_party/mediapipe/availability/mediapipe_availability.js';
import {FaceLandmarkerResult} from '../third_party/mediapipe/task_vision/vision.js';

import {FacialGesture, GestureDetector} from './gesture_detector.js';
import {MouseController} from './mouse_controller.js';

/**
 * TODO(b/309121742): Move this into a dedicated class for action fulfillment.
 * The actions that are supported by FaceGaze.
 */
export enum Action {
  CLICK_LEFT = 'clickLeft',
  CLICK_RIGHT = 'clickRight',
  RESET_MOUSE = 'resetMouse',
}

/** Main class for FaceGaze. */
export class FaceGaze {
  private mouseController_: MouseController;
  private gestureToAction_: Map<FacialGesture, Action> = new Map();
  private gestureToConfidence_: Map<FacialGesture, number> = new Map();
  private onInitCallbackForTest_: (() => void)|undefined;
  private initialized_ = false;

  constructor() {
    this.mouseController_ = new MouseController();
    this.init_();
  }

  /** Initializes FaceGaze. */
  private async init_(): Promise<void> {
    // Initialize default mapping of facial gestures to actions.
    // TODO(b/309121742): Set this using the user's preferences.
    this.gestureToAction_.set(FacialGesture.JAW_OPEN, Action.CLICK_LEFT)
        .set(FacialGesture.BROW_INNER_UP, Action.CLICK_RIGHT)
        .set(FacialGesture.BROW_DOWN_LEFT, Action.RESET_MOUSE)
        .set(FacialGesture.BROW_DOWN_RIGHT, Action.RESET_MOUSE);
    // Initialize default mapping of facial gestures to confidence threshold.
    // TODO(b/309121742): Set this using the user's preferences.
    this.gestureToConfidence_
        .set(FacialGesture.JAW_OPEN, FaceGaze.DEFAULT_CONFIDENCE_THRESHOLD)
        .set(FacialGesture.BROW_INNER_UP, FaceGaze.DEFAULT_CONFIDENCE_THRESHOLD)
        .set(
            FacialGesture.BROW_DOWN_LEFT, FaceGaze.DEFAULT_CONFIDENCE_THRESHOLD)
        .set(
            FacialGesture.BROW_DOWN_RIGHT,
            FaceGaze.DEFAULT_CONFIDENCE_THRESHOLD);

    this.connectToWebCam_();

    await this.mouseController_.init();

    // TODO(b/309121742): Listen to magnifier bounds changed so as to update
    // cursor relative position logic when magnifier is running.

    if (this.onInitCallbackForTest_) {
      this.onInitCallbackForTest_();
      this.onInitCallbackForTest_ = undefined;
    }
    this.initialized_ = true;
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

    this.mouseController_.onFaceLandmarkerResult(result);
    const gestures = GestureDetector.detect(result, this.gestureToConfidence_);
    for (const gesture of gestures) {
      if (gesture === FacialGesture.BROW_DOWN_LEFT ||
          gesture === FacialGesture.BROW_DOWN_RIGHT) {
        // Handle the BrowDown gesture separately.
        continue;
      }

      this.performAction_(this.gestureToAction_.get(gesture));
    }

    if (gestures.includes(FacialGesture.BROW_DOWN_LEFT) &&
        gestures.includes(FacialGesture.BROW_DOWN_RIGHT)) {
      // The BrowDown gesture is special because it is the combination of
      // two separate facial gestures. Ensure that the associated action
      // is only performed if both gestures are recognized.
      this.performAction_(
          this.gestureToAction_.get(FacialGesture.BROW_DOWN_LEFT));
    }
  }

  /**
   * TODO(b/309121742): Move this into a dedicated class for action fulfillment.
   */
  private performAction_(action: Action|undefined): void {
    if (!action) {
      return;
    }

    switch (action) {
      case Action.CLICK_LEFT:
        this.mouseController_.clickLeft();
        break;
      case Action.CLICK_RIGHT:
        this.mouseController_.clickRight();
        break;
      case Action.RESET_MOUSE:
        this.mouseController_.resetLocation();
        break;
    }
  }

  /** Destructor to remove any listeners. */
  onFaceGazeDisabled(): void {
    this.mouseController_.stopEventListeners();
  }

  /** Allows tests to wait for FaceGaze to be fully initialized. */
  setOnInitCallbackForTest(callback: () => void): void {
    if (!this.initialized_) {
      this.onInitCallbackForTest_ = callback;
      return;
    }
    callback();
  }
}

export namespace FaceGaze {
  /** The default confidence threshold for facial gestures. */
  export const DEFAULT_CONFIDENCE_THRESHOLD = 0.6;
}
