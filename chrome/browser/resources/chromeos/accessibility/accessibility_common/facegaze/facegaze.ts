// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestImportManager} from '/common/testing/test_import_manager.js';
import type {FaceLandmarkerResult} from '/third_party/mediapipe/vision.js';

import {BubbleController} from './bubble_controller.js';
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
  private bubbleController_: BubbleController;

  constructor() {
    this.webCamFaceLandmarker_ = new WebCamFaceLandmarker(
        (resultWithLatency: FaceLandmarkerResultWithLatency) => {
          const {result, latency} = resultWithLatency;
          this.processFaceLandmarkerResult_(result, latency);
        });

    this.bubbleController_ = new BubbleController(() => {
      return {
        paused: this.gestureHandler_.isPaused(),
        scrollModeActive: this.mouseController_.isScrollModeActive(),
      };
    });

    this.mouseController_ = new MouseController(this.bubbleController_);
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

    chrome.settingsPrivate.getPref(
        FaceGaze.PREF_ACCELERATOR_DIALOG_HAS_BEEN_ACCEPTED, pref => {
          if (pref.value === undefined || pref.value === null) {
            return;
          }

          if (pref.value) {
            // If the confirmation dialog has already been accepted, there is no
            // need to show it again. We can proceed as if it's been accepted.
            this.onConfirmationDialog_(true);
            return;
          }

          // If the confirmation dialog has not been accepted yet, display it to
          // the user.
          const title =
              chrome.i18n.getMessage('facegaze_confirmation_dialog_title');
          const description =
              chrome.i18n.getMessage('facegaze_confirmation_dialog_desc');
          chrome.accessibilityPrivate.showConfirmationDialog(
              title, description, /*cancelName=*/ undefined, (accepted) => {
                this.onConfirmationDialog_(accepted);
              });
        });
  }

  /** Runs when the confirmation dialog has either been accepted or rejected. */
  private onConfirmationDialog_(accepted: boolean): void {
    chrome.settingsPrivate.setPref(
        FaceGaze.PREF_ACCELERATOR_DIALOG_HAS_BEEN_ACCEPTED, accepted);
    if (!accepted) {
      // If the dialog was rejected, then disable the FaceGaze feature.
      chrome.settingsPrivate.setPref(FaceGaze.PREF_FACE_GAZE_ENABLED, false);
      return;
    }

    // If the dialog was accepted, then initialize FaceGaze.
    chrome.accessibilityPrivate.openSettingsSubpage(
        FaceGaze.SETTINGS_PAGE_ROUTE);

    this.bubbleController_.updateBubble('');
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
      if (this.mouseController_.isScrollModeActive()) {
        // If actions are turned off while scroll mode is active, then we should
        // toggle out of scroll mode. Otherwise, the user will be stuck in
        // scroll mode with no way to exit.
        this.mouseController_.toggleScrollMode();
      }
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
      const {macros, displayText} = this.gestureHandler_.detectMacros(result);
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

      if (displayText) {
        this.bubbleController_.updateBubble(displayText);
      }
    }
  }

  /** Destructor to remove any listeners. */
  onFaceGazeDisabled(): void {
    this.mouseController_.reset();
    this.gestureHandler_.stop();
    this.webCamFaceLandmarker_.stop();
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
  export const PREF_ACCELERATOR_DIALOG_HAS_BEEN_ACCEPTED =
      'settings.a11y.face_gaze.accelerator_dialog_has_been_accepted';
  export const PREF_FACE_GAZE_ENABLED = 'settings.a11y.face_gaze.enabled';
  export const PREF_ACTIONS_ENABLED = 'settings.a11y.face_gaze.actions_enabled';
  export const PREF_CURSOR_CONTROL_ENABLED =
      'settings.a11y.face_gaze.cursor_control_enabled';

  export const SETTINGS_PAGE_ROUTE = 'manageAccessibility/faceGaze';
}

TestImportManager.exportForTesting(FaceGaze);
