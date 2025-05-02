// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestImportManager} from '/common/testing/test_import_manager.js';
import type {FaceLandmarkerResult} from '/third_party/mediapipe/vision.js';

import {BubbleController} from './bubble_controller.js';
import {PrefNames, SettingsPath} from './constants.js';
import {GestureHandler} from './gesture_handler.js';
import {MetricsUtils} from './metrics_utils.js';
import {MouseController} from './mouse_controller.js';
import type {FaceLandmarkerResultWithLatency} from './web_cam_face_landmarker.js';
import {WebCamFaceLandmarker} from './web_cam_face_landmarker.js';

type PrefObject = chrome.settingsPrivate.PrefObject;

/** Main class for FaceGaze. */
export class FaceGaze {
  // References to core classes.
  private bubbleController_: BubbleController;
  private gestureHandler_: GestureHandler;
  private metricsUtils_: MetricsUtils;
  private mouseController_: MouseController;
  private webCamFaceLandmarker_: WebCamFaceLandmarker;

  // Other variables, such as state and callbacks.
  private actionsEnabled_ = false;
  private cursorControlEnabled_ = false;
  private initialized_ = false;
  // isFaceLandmarkerResultValid_ is initialized to true to ensure the correct
  // UI messages are shown on startup.
  private isFaceLandmarkerResultValid_ = true;
  private isCameraMuted_ = false;
  private onInitCallbackForTest_: (() => void)|undefined;
  private prefsListener_: (prefs: PrefObject[]) => void;

  constructor(isDictationActive: () => boolean) {
    this.bubbleController_ = new BubbleController(() => {
      return {
        paused: this.gestureHandler_.isPaused() ?
            this.gestureHandler_.getGestureForPause() :
            undefined,
        scrollMode: this.mouseController_.isScrollModeActive() ?
            this.gestureHandler_.getGestureForScroll() :
            undefined,
        longClick: this.mouseController_.isLongClickActive() ?
            this.gestureHandler_.getGestureForLongClick() :
            undefined,
        dictation: isDictationActive() ?
            this.gestureHandler_.getGestureForDictation() :
            undefined,
        heldMacros: this.gestureHandler_.getHeldMacroDisplayStrings(),
        precision: this.mouseController_.isPrecisionActive() ?
            this.gestureHandler_.getGestureForPrecision() :
            undefined,
        isFaceLandmarkerResultValid: this.isFaceLandmarkerResultValid_,
        isCameraMuted: this.isCameraMuted_,
      };
    });

    this.webCamFaceLandmarker_ = new WebCamFaceLandmarker(
        this.bubbleController_,
        (resultWithLatency: FaceLandmarkerResultWithLatency) => {
          const {result, latency} = resultWithLatency;
          this.processFaceLandmarkerResult_(result, latency);
        },
        () => this.onCameraFeedMuted_(), () => this.onCameraFeedUnmuted_());

    this.mouseController_ = new MouseController(this.bubbleController_);
    this.gestureHandler_ = new GestureHandler(
        this.mouseController_, this.bubbleController_, isDictationActive);
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

    this.maybeShowConfirmationDialog_();
  }

  private maybeShowConfirmationDialog_(): void {
    chrome.settingsPrivate.getPref(
        PrefNames.ACCELERATOR_DIALOG_HAS_BEEN_ACCEPTED, pref => {
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
        PrefNames.ACCELERATOR_DIALOG_HAS_BEEN_ACCEPTED, accepted);
    if (!accepted) {
      // If the dialog was rejected, then disable the FaceGaze feature and do
      // not show the confirmation dialog for disabling.
      chrome.settingsPrivate.setPref(PrefNames.FACE_GAZE_ENABLED, false);

      return;
    }

    // If the dialog was accepted, then initialize FaceGaze.
    chrome.accessibilityPrivate.openSettingsSubpage(SettingsPath);

    this.bubbleController_.updateBubble('');
    this.webCamFaceLandmarker_.init();
  }

  private updateFromPrefs_(prefs: PrefObject[]): void {
    prefs.forEach(pref => {
      if (pref.value === undefined || pref.value === null) {
        return;
      }

      switch (pref.key) {
        case PrefNames.ACTIONS_ENABLED:
          this.actionsEnabledChanged_(pref.value);
          break;
        case PrefNames.CURSOR_CONTROL_ENABLED:
          this.cursorControlEnabledChanged_(pref.value);
          break;
        case PrefNames.CURSOR_USE_ACCELERATION:
          this.mouseController_.useCursorAccelerationChanged(pref.value);
          break;
        case PrefNames.GESTURE_TO_CONFIDENCE:
          this.gestureHandler_.gesturesToConfidencesChanged(pref.value);
          break;
        case PrefNames.GESTURE_TO_KEY_COMBO:
          this.gestureHandler_.gesturesToKeyCombosChanged(pref.value);
          break;
        case PrefNames.GESTURE_TO_MACRO:
          this.gestureHandler_.gesturesToMacrosChanged(pref.value);
          break;
        case PrefNames.PRECISION_CLICK:
          this.mouseController_.precisionClickChanged(pref.value);
          break;
        case PrefNames.PRECISION_CLICK_SPEED_FACTOR:
          this.mouseController_.precisionSpeedFactorChanged(pref.value);
          break;
        case PrefNames.SPD_UP:
          this.mouseController_.speedUpChanged(pref.value);
          break;
        case PrefNames.SPD_DOWN:
          this.mouseController_.speedDownChanged(pref.value);
          break;
        case PrefNames.SPD_LEFT:
          this.mouseController_.speedLeftChanged(pref.value);
          break;
        case PrefNames.SPD_RIGHT:
          this.mouseController_.speedRightChanged(pref.value);
          break;
        case PrefNames.VELOCITY_THRESHOLD:
          this.mouseController_.velocityThresholdChanged(pref.value);
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

      // If actions are turned off while a toggled action is active, then we
      // should toggle out of the relevant action. Otherwise, the user will be
      // stuck in the action with no way to exit.
      if (this.mouseController_.isScrollModeActive()) {
        this.mouseController_.toggleScrollMode();
      }

      if (this.mouseController_.isLongClickActive()) {
        this.mouseController_.toggleLongClick();
      }

      if (this.mouseController_.isPrecisionActive()) {
        this.mouseController_.togglePrecision();
      }
    }
  }

  private processFaceLandmarkerResult_(
      result: FaceLandmarkerResult, latency?: number): void {
    if (!result) {
      return;
    }

    if (result.faceBlendshapes.length === 0 &&
        result.faceLandmarks.length === 0 &&
        result.facialTransformationMatrixes.length === 0) {
      // In practice, we can get results that are empty. Typically this happens
      // when the camera is obstructed, blocked by permissions, if the user is
      // out of frame, or if the user's face can't be detected for any other
      // reason. In all of these cases, the camera feed is active but either
      // gives us empty frames or frames where a face cannot be recognized,
      // which causes the FaceLandmarker to return this type of result.
      this.updateIsFaceLandmarkerResultValid_(/*valid=*/ false);
      return;
    }

    this.updateIsFaceLandmarkerResultValid_(/*valid=*/ true);
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

  private updateIsFaceLandmarkerResultValid_(valid: boolean): void {
    if (valid === this.isFaceLandmarkerResultValid_) {
      return;
    }

    this.isFaceLandmarkerResultValid_ = valid;
    this.bubbleController_.resetBubble();
  }

  private onCameraFeedMuted_(): void {
    if (this.isCameraMuted_) {
      return;
    }

    this.isCameraMuted_ = true;
    this.bubbleController_.resetBubble();
  }

  private onCameraFeedUnmuted_(): void {
    if (!this.isCameraMuted_) {
      return;
    }

    this.isCameraMuted_ = false;
    this.bubbleController_.resetBubble();
  }

  /** Destructor to remove any listeners. */
  onFaceGazeDisabled(): void {
    this.mouseController_.reset();
    this.gestureHandler_.stop();
    this.webCamFaceLandmarker_.stop();
    chrome.settingsPrivate.onPrefsChanged.removeListener(this.prefsListener_);
  }

  /** Allows tests to wait for FaceGaze to be fully initialized. */
  setOnInitCallbackForTest(callback: VoidFunction): void {
    if (!this.initialized_) {
      this.onInitCallbackForTest_ = callback;
      return;
    }

    callback();
  }
}

TestImportManager.exportForTesting(FaceGaze);
