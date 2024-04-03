// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {KeyPressMacro} from '/common/action_fulfillment/macros/key_press_macro.js';
import {Macro} from '/common/action_fulfillment/macros/macro.js';
import {MacroName} from '/common/action_fulfillment/macros/macro_names.js';
import {MouseClickMacro} from '/common/action_fulfillment/macros/mouse_click_macro.js';
import {ToggleDictationMacro} from '/common/action_fulfillment/macros/toggle_dictation_macro.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';
import type {FaceLandmarkerResult} from '/third_party/mediapipe/vision.js';

import {FacialGesture} from './facial_gestures.js';
import {GestureDetector} from './gesture_detector.js';
import {ResetCursorMacro} from './macros/reset_cursor_macro.js';
import {MouseController} from './mouse_controller.js';

type PrefObject = chrome.settingsPrivate.PrefObject;

/**
 * Handles converting facial gestures to Macros.
 */
export class GestureHandler {
  private gestureToMacroName_: Map<FacialGesture, MacroName> = new Map();
  private gestureToConfidence_: Map<FacialGesture, number> = new Map();
  private gestureLastRecognized_: Map<FacialGesture, number> = new Map();
  private mouseController_: MouseController;
  private repeatDelayMs_ = GestureHandler.DEFAULT_REPEAT_DELAY_MS;
  private prefsListener_: (prefs: any) => void;
  // The most recently detected gestures. We track this to know when a gesture
  // has ended.
  private previousGestures_: FacialGesture[] = [];
  private macrosToCompleteLater_: Map<FacialGesture, Macro> = new Map();

  constructor(mouseController: MouseController) {
    this.mouseController_ = mouseController;

    this.prefsListener_ = prefs => this.updateFromPrefs_(prefs);
    chrome.settingsPrivate.getAllPrefs(prefs => this.updateFromPrefs_(prefs));
    chrome.settingsPrivate.onPrefsChanged.addListener(this.prefsListener_);
  }

  private updateFromPrefs_(prefs: PrefObject[]): void {
    prefs.forEach(pref => {
      switch (pref.key) {
        case GestureHandler.GESTURE_TO_MACRO_PREF:
          if (pref.value) {
            // Update the whole map from this preference.
            this.gestureToMacroName_.clear();
            if (Object.entries(pref.value).length === 0) {
              // TODO(b:322510392): Remove this hard-coded mapping after
              // settings page lands when users can pick their own mappings.
              pref.value[FacialGesture.JAW_OPEN] = MacroName.MOUSE_CLICK_LEFT;
              pref.value[FacialGesture.BROW_INNER_UP] =
                  MacroName.MOUSE_CLICK_RIGHT;
              pref.value[FacialGesture.BROWS_DOWN] = MacroName.RESET_CURSOR;
            }
            for (const [gesture, assignedMacro] of Object.entries(pref.value)) {
              if (assignedMacro === MacroName.UNSPECIFIED) {
                continue;
              }
              this.gestureToMacroName_.set(
                  gesture as FacialGesture, Number(assignedMacro));
              // Ensure the confidence for this gesture is set to the default,
              // if it wasn't set yet. This might happen if the user hasn't
              // opened the settings subpage yet.
              if (!this.gestureToConfidence_.has(gesture as FacialGesture)) {
                this.gestureToConfidence_.set(
                    gesture as FacialGesture,
                    GestureHandler.DEFAULT_CONFIDENCE_THRESHOLD);
              }
            }
          }
          break;
        case GestureHandler.GESTURE_TO_CONFIDENCE_PREF:
          if (pref.value) {
            for (const [gesture, confidence] of Object.entries(pref.value)) {
              this.gestureToConfidence_.set(
                  gesture as FacialGesture, Number(confidence) / 100.);
            }
          }
          break;
        default:
          return;
      }
    });
  }

  detectMacros(result: FaceLandmarkerResult): Macro[] {
    const gestures = GestureDetector.detect(result, this.gestureToConfidence_);
    const macros = this.gesturesToMacros_(gestures);
    macros.push(
        ...this.popMacrosOnGestureEnd(gestures, this.previousGestures_));
    this.previousGestures_ = gestures;
    return macros;
  }

  private gesturesToMacros_(gestures: FacialGesture[]): Macro[] {
    const macroNames: Map<MacroName, FacialGesture> = new Map();
    for (const gesture of gestures) {
      const currentTime = new Date().getTime();
      if (this.gestureLastRecognized_.has(gesture) &&
              currentTime - this.gestureLastRecognized_.get(gesture)! <
                  this.repeatDelayMs_ ||
          this.macrosToCompleteLater_.has(gesture)) {
        // Avoid responding to the same macro repeatedly in too short a time
        // or if we are still waiting to complete them later (they shouldn't be
        // repeated until completed).
        continue;
      }
      this.gestureLastRecognized_.set(gesture, currentTime);
      const name = this.gestureToMacroName_.get(gesture);
      if (name) {
        macroNames.set(name, gesture);
      }
    }

    // Construct macros from all the macro names.
    const result: Macro[] = [];
    for (const [macroName, gesture] of macroNames) {
      const macro = this.macroFromName_(macroName);
      if (macro) {
        result.push(macro);
        if (macro.triggersAtActionStartAndEnd()) {
          // Cache this macro to be run a second time later, for the key
          // release.
          this.macrosToCompleteLater_.set(gesture, macro);
        }
      }
    }

    return result;
  }

  /**
   * Gets the cached macros that are run again when a gesture ends. For example,
   * for a left click macro, the left click starts when the gesture is first
   * detected and the macro is run a second time when the gesture is no longer
   * detected, thus the click will be held as long as the gesture is still
   * detected.
   */
  private popMacrosOnGestureEnd(
      gestures: FacialGesture[], previousGestures: FacialGesture[]): Macro[] {
    const macrosForLater: Macro[] = [];
    previousGestures.forEach(previousGesture => {
      if (!gestures.includes(previousGesture)) {
        // The gesture has stopped being recognized. Run the second half of this
        // macro, and stop saving it.
        const macro = this.macrosToCompleteLater_.get(previousGesture);
        if (!macro) {
          return;
        }
        if (macro instanceof MouseClickMacro) {
          macro.updateLocation(this.mouseController_.mouseLocation());
        }
        macrosForLater.push(macro);
        this.macrosToCompleteLater_.delete(previousGesture);
      }
    });
    return macrosForLater;
  }

  private macroFromName_(name: MacroName): Macro|undefined {
    switch (name) {
      case MacroName.TOGGLE_DICTATION:
        return new ToggleDictationMacro();
      case MacroName.MOUSE_CLICK_LEFT:
        return new MouseClickMacro(this.mouseController_.mouseLocation());
      case MacroName.MOUSE_CLICK_RIGHT:
        return new MouseClickMacro(
            this.mouseController_.mouseLocation(), /*leftClick=*/ false);
      case MacroName.RESET_CURSOR:
        return new ResetCursorMacro(this.mouseController_);
      case MacroName.KEY_PRESS_SPACE:
      case MacroName.KEY_PRESS_DOWN:
      case MacroName.KEY_PRESS_LEFT:
      case MacroName.KEY_PRESS_RIGHT:
      case MacroName.KEY_PRESS_UP:
        return new KeyPressMacro(name);
      default:
        return;
    }
  }
}

export namespace GestureHandler {
  /** The default confidence threshold for facial gestures. */
  export const DEFAULT_CONFIDENCE_THRESHOLD = 0.6;

  /** Minimum repeat rate of a gesture. */
  // TODO(b:322511275): Move to a pref in settings.
  export const DEFAULT_REPEAT_DELAY_MS = 500;

  /**
   * Pref name of preference mapping facegaze gestures to macro action names.
   */
  export const GESTURE_TO_MACRO_PREF =
      'settings.a11y.face_gaze.gestures_to_macros';

  export const GESTURE_TO_CONFIDENCE_PREF =
      'settings.a11y.face_gaze.gestures_to_confidence';
}

TestImportManager.exportForTesting(GestureHandler);
