// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestImportManager} from '/common/testing/test_import_manager.js';
import type {FaceLandmarkerResult} from '/third_party/mediapipe/vision.js';

import {GestureHandler} from './gesture_handler.js';
import {MetricsUtils} from './metrics_utils.js';
import {MouseController} from './mouse_controller.js';

type PrefObject = chrome.settingsPrivate.PrefObject;

/** Main class for FaceGaze. */
export class FaceGaze {
  private mouseController_: MouseController;
  private gestureHandler_: GestureHandler;
  private onInitCallbackForTest_: (() => void)|undefined;
  private initialized_ = false;
  declare private cameraStreamReadyPromise_: Promise<void>;
  declare private cameraStreamClosedPromise_: Promise<void>;
  private cameraStreamReadyResolver_?: () => void;
  private cameraStreamClosedResolver_?: () => void;
  private cameraStreamWindowId_ = -1;
  private cursorControlEnabled_ = false;
  private actionsEnabled_ = false;
  private prefsListener_: (prefs: PrefObject[]) => void;
  private metricsUtils_: MetricsUtils;

  constructor() {
    this.mouseController_ = new MouseController();
    this.gestureHandler_ = new GestureHandler(this.mouseController_);
    this.metricsUtils_ = new MetricsUtils();
    this.cameraStreamReadyPromise_ = new Promise(resolve => {
      this.cameraStreamReadyResolver_ = resolve;
    });
    this.cameraStreamClosedPromise_ = new Promise(resolve => {
      this.cameraStreamClosedResolver_ = resolve;
    });
    this.prefsListener_ = prefs => this.updateFromPrefs_(prefs);
    this.init_();
  }

  /** Initializes FaceGaze. */
  private async init_(): Promise<void> {
    this.connectToWebCam_();

    // TODO(b/309121742): Listen to magnifier bounds changed so as to update
    // cursor relative position logic when magnifier is running.

    chrome.settingsPrivate.getAllPrefs(prefs => this.updateFromPrefs_(prefs));
    chrome.settingsPrivate.onPrefsChanged.addListener(this.prefsListener_);

    if (this.onInitCallbackForTest_) {
      this.onInitCallbackForTest_();
      this.onInitCallbackForTest_ = undefined;
    }
    this.initialized_ = true;
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

  private connectToWebCam_(): void {
    // Open camera_stream.html, which will connect to the webcam and pass
    // FaceLandmarker results back to the background page. Use chrome.windows
    // API to ensure page is opened in Ash-chrome.
    const params = {
      url: chrome.runtime.getURL(
          'accessibility_common/facegaze/camera_stream.html'),
      type: chrome.windows.CreateType.PANEL,
    };
    chrome.windows.create(params, (win) => {
      if (!win || win.id === undefined) {
        return;
      }

      this.cameraStreamWindowId_ = win.id;
      chrome.runtime.onMessage.addListener(message => {
        if (message.type === 'faceLandmarkerResult') {
          this.metricsUtils_.addFaceLandmarkerResultLatency(message.latency);
          this.processFaceLandmarkerResult_(message.result);
        } else if (message.type === 'cameraStreamReadyForTesting') {
          this.cameraStreamReadyResolver_!();
        } else if (message.type === 'updateLandmarkWeights') {
          this.mouseController_.updateLandmarkWeights(
              new Map(Object.entries(message.weights)));
        }

        return false;
      });
    });
  }

  private processFaceLandmarkerResult_(result: FaceLandmarkerResult): void {
    if (!result) {
      return;
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
    if (this.cameraStreamWindowId_ !== -1) {
      chrome.windows.remove(this.cameraStreamWindowId_, () => {
        this.cameraStreamClosedResolver_!();
      });
    }
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
  // Pref names. Should be in sync with with values at ash_pref_names.h.
  export const PREF_CURSOR_CONTROL_ENABLED =
      'settings.a11y.face_gaze.cursor_control_enabled';
  export const PREF_ACTIONS_ENABLED = 'settings.a11y.face_gaze.actions_enabled';
}

TestImportManager.exportForTesting(FaceGaze);
