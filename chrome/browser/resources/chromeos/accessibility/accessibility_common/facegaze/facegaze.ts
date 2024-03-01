// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestImportManager} from '/common/testing/test_import_manager.js';
import type {FaceLandmarkerResult} from '/third_party/mediapipe/vision.js';

import {GestureHandler} from './gesture_handler.js';
import {MouseController} from './mouse_controller.js';

/** Main class for FaceGaze. */
export class FaceGaze {
  private mouseController_: MouseController;
  private gestureHandler_: GestureHandler;
  private onInitCallbackForTest_: (() => void)|undefined;
  private initialized_ = false;
  declare private cameraStreamReadyPromise_: Promise<void>;
  private cameraStreamReadyResolver_?: () => void;

  constructor() {
    this.mouseController_ = new MouseController();
    this.gestureHandler_ = new GestureHandler(this.mouseController_);
    this.cameraStreamReadyPromise_ = new Promise(resolve => {
      this.cameraStreamReadyResolver_ = resolve;
    });
    this.init_();
  }

  /** Initializes FaceGaze. */
  private async init_(): Promise<void> {
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
        } else if (message.type === 'cameraStreamReadyForTesting') {
          this.cameraStreamReadyResolver_!();
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
    const macros = this.gestureHandler_.detectMacros(result);
    for (const macro of macros) {
      // TODO(b:322511275): Smooth macros by having some rate limit at which the
      // same one can be executed.
      const runMacroResult = macro.run();
      if (!runMacroResult.isSuccess) {
        console.warn(
            'Failed to execute macro ', macro.getName(), runMacroResult.error);
      }
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

TestImportManager.exportForTesting(FaceGaze);
