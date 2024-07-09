// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestImportManager} from '/common/testing/test_import_manager.js';
import type {FaceLandmarkerResult} from '/third_party/mediapipe/vision.js';

import {GestureHandler} from './gesture_handler.js';
import {MetricsUtils} from './metrics_utils.js';
import {MouseController} from './mouse_controller.js';
import {FaceLandmarkerResultWithLatency, WebCamFaceLandmarker} from './web_cam_face_landmarker.js';

type PrefObject = chrome.settingsPrivate.PrefObject;

/** Main class for FaceGaze. */
export class FaceGaze {
  private mouseController_: MouseController;
  private gestureHandler_: GestureHandler;
  private onInitCallbackForTest_: (() => void)|undefined;
  private initialized_ = false;
  private cursorControlEnabled_ = false;
  private actionsEnabled_ = false;
  private prefsListener_: (prefs: PrefObject[]) => void;
  private metricsUtils_: MetricsUtils;
  private webCamFaceLandmarker_: WebCamFaceLandmarker;
  private skipInitializeWebCamFaceLandmarkerForTesting_ = false;

  constructor() {
    this.webCamFaceLandmarker_ = new WebCamFaceLandmarker(
        (resultWithLatency: FaceLandmarkerResultWithLatency) => {
          const {result, latency} = resultWithLatency;
          this.processFaceLandmarkerResult_(result, latency);
        });

    this.mouseController_ = new MouseController();
    this.gestureHandler_ = new GestureHandler(this.mouseController_);
    this.metricsUtils_ = new MetricsUtils();
    this.prefsListener_ = prefs => this.updateFromPrefs_(prefs);
    this.init_();
  }

  /** Initializes FaceGaze. */
  private init_(): void {
    // TODO(b/309121742): Listen to magnifier bounds changed so as to update
    // cursor relative position logic when magnifier is running.

    chrome.settingsPrivate.getAllPrefs(prefs => this.updateFromPrefs_(prefs));
    chrome.settingsPrivate.onPrefsChanged.addListener(this.prefsListener_);

    if (this.onInitCallbackForTest_) {
      this.onInitCallbackForTest_();
      this.onInitCallbackForTest_ = undefined;
    }
    this.initialized_ = true;

    // Use a timeout to defer the initialization of the WebCamFaceLandmarker.
    // For tests, we can guard the initialization using a testing-specific
    // variable. For production, this will initialize the WebCamFaceLandmarker
    // after the timeout has elapsed.
    setTimeout(() => {
      this.maybeInitializeWebCamFaceLandmarker_();
    }, FaceGaze.INITIALIZE_WEB_CAM_FACE_LANDMARKER_TIMEOUT);

    // TODO(b/309121742): Add logic to open the camera stream page so that
    // developers can quickly customize weights.
  }

  private maybeInitializeWebCamFaceLandmarker_(): void {
    if (this.skipInitializeWebCamFaceLandmarkerForTesting_) {
      return;
    }

    this.webCamFaceLandmarker_.init();
  }

  private updateFromPrefs_(prefs: PrefObject[]): void {
    prefs.forEach(pref => {
      switch (pref.key) {
        case FaceGaze.PREF_CURSOR_CONTROL_ENABLED:
          this.cursorControlEnabledChanged_(pref.value);
          break;
        case FaceGaze.PREF_ACTIONS_ENABLED:
          this.actionsEnabledChanged_(pref.value);
          break;
        default:
          return;
      }
    });
  }

  private cursorControlEnabledChanged_(value: boolean): void {
    if (this.cursorControlEnabled_ === value) {
      return;
    }
    this.cursorControlEnabled_ = value;
    if (this.cursorControlEnabled_) {
      this.mouseController_.start();
    } else {
      this.mouseController_.stop();
    }
  }

  private actionsEnabledChanged_(value: boolean): void {
    if (this.actionsEnabled_ === value) {
      return;
    }
    this.actionsEnabled_ = value;
    if (this.actionsEnabled_) {
      this.gestureHandler_.start();
    } else {
      this.gestureHandler_.stop();
    }
  }

  private processFaceLandmarkerResult_(
      result: FaceLandmarkerResult, latency?: number): void {
    if (!result) {
      return;
    }

    if (latency !== undefined) {
      this.metricsUtils_.addFaceLandmarkerResultLatency(latency);
    }

    if (this.cursorControlEnabled_) {
      this.mouseController_.onFaceLandmarkerResult(result);
    }

    if (this.actionsEnabled_) {
      const macros = this.gestureHandler_.detectMacros(result);
      for (const macro of macros) {
        const checkContextResult = macro.checkContext();
        if (!checkContextResult.canTryAction) {
          console.warn(
              'Cannot execute macro in this context', macro.getName(),
              checkContextResult.error, checkContextResult.failedContext);
          continue;
        }
        const runMacroResult = macro.run();
        if (!runMacroResult.isSuccess) {
          console.warn(
              'Failed to execute macro ', macro.getName(),
              runMacroResult.error);
        }
      }
    }
  }

  /** Destructor to remove any listeners. */
  onFaceGazeDisabled(): void {
    this.mouseController_.reset();
    this.gestureHandler_.stop();
  }

  /** Allows tests to wait for FaceGaze to be fully initialized. */
  setOnInitCallbackForTest(callback: () => void): void {
    if (!this.initialized_) {
      this.onInitCallbackForTest_ = callback;
      return;
    }

    callback();
  }

  /**
   * Used to set the value of `skipInitializeWebCamFaceLandmarkerForTesting_`.
   * We want to use this method in tests because tests will want to avoid
   * initializing the WebCamFaceLandmarker, since it starts the webcam stream
   * and causes errors.
   */
  setSkipInitializeWebCamFaceLandmarkerForTesting(skip: boolean): void {
    this.skipInitializeWebCamFaceLandmarkerForTesting_ = skip;
  }
}

export namespace FaceGaze {
  export const INITIALIZE_WEB_CAM_FACE_LANDMARKER_TIMEOUT = 5 * 1000;
  // Pref names. Should be in sync with with values at ash_pref_names.h.
  export const PREF_CURSOR_CONTROL_ENABLED =
      'settings.a11y.face_gaze.cursor_control_enabled';
  export const PREF_ACTIONS_ENABLED = 'settings.a11y.face_gaze.actions_enabled';
}

TestImportManager.exportForTesting(FaceGaze);
