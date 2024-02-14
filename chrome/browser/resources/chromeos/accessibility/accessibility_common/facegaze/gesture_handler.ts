// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Macro} from '/common/action_fulfillment/macros/macro.js';
import {MacroName} from '/common/action_fulfillment/macros/macro_names.js';
import {MouseClickMacro} from '/common/action_fulfillment/macros/mouse_click_macro.js';
import {ToggleDictationMacro} from '/common/action_fulfillment/macros/toggle_dictation_macro.js';

import {FaceLandmarkerResult} from '../third_party/mediapipe/task_vision/vision.js';

import {FacialGesture, GestureDetector} from './gesture_detector.js';
import {ResetCursorMacro} from './macros/reset_cursor_macro.js';
import {MouseController} from './mouse_controller.js';

/**
 * Handles converting facial gestures to Macros.
 */
export class GestureHandler {
  private gestureToMacroName_: Map<FacialGesture, MacroName> = new Map();
  private gestureToConfidence_: Map<FacialGesture, number> = new Map();
  private gestureLastRecognized_: Map<FacialGesture, number> = new Map();
  private mouseController_: MouseController;

  constructor(mouseController: MouseController) {
    this.mouseController_ = mouseController;

    // Initialize default mapping of facial gestures to actions.
    // TODO(b/309121742): Set this using the user's preferences.
    this.gestureToMacroName_
        .set(FacialGesture.JAW_OPEN, MacroName.MOUSE_CLICK_LEFT)
        .set(FacialGesture.BROW_INNER_UP, MacroName.MOUSE_CLICK_RIGHT)
        .set(FacialGesture.BROWS_DOWN, MacroName.RESET_CURSOR);

    // Initialize default mapping of facial gestures to confidence
    // threshold.
    // TODO(b/309121742): Set this using the user's preferences.
    this.gestureToConfidence_
        .set(
            FacialGesture.JAW_OPEN, GestureHandler.DEFAULT_CONFIDENCE_THRESHOLD)
        .set(
            FacialGesture.BROW_INNER_UP,
            GestureHandler.DEFAULT_CONFIDENCE_THRESHOLD)
        .set(
            FacialGesture.BROWS_DOWN,
            GestureHandler.DEFAULT_CONFIDENCE_THRESHOLD);
  }

  detectMacros(result: FaceLandmarkerResult): Macro[] {
    const gestures = GestureDetector.detect(result, this.gestureToConfidence_);
    return this.gesturesToMacros_(gestures);
  }

  private gesturesToMacros_(gestures: FacialGesture[]): Macro[] {
    const macroNames: MacroName[] = [];
    for (const gesture of gestures) {
      const currentTime = new Date().getTime();
      if (this.gestureLastRecognized_.has(gesture) &&
          currentTime - this.gestureLastRecognized_.get(gesture)! <
              GestureHandler.DEFAULT_REPEAT_DELAY_MS) {
        // Avoid responding to the same macro repeatedly in too short a time.
        continue;
      }
      this.gestureLastRecognized_.set(gesture, currentTime);
      const name = this.gestureToMacroName_.get(gesture);
      if (name) {
        macroNames.push(name);
      }
    }

    // Construct macros from all the macro names.
    const result: Macro[] = [];
    for (const macroName of macroNames) {
      const macro = this.macroFromName_(macroName);
      if (macro) {
        result.push(macro);
      }
    }

    return result;
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
}
