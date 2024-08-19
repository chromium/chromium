// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FacialGesture} from 'chrome://resources/ash/common/accessibility/facial_gestures.js';
import {MacroName} from 'chrome://resources/ash/common/accessibility/macro_names.js';

export const FACE_GAZE_GESTURE_TO_MACROS_PREF =
    'prefs.settings.a11y.face_gaze.gestures_to_macros.value';

export const FACE_GAZE_GESTURE_TO_CONFIDENCE_PREF_DICT =
    'settings.a11y.face_gaze.gestures_to_confidence';

export const FACE_GAZE_GESTURE_TO_CONFIDENCE_PREF =
    `prefs.${FACE_GAZE_GESTURE_TO_CONFIDENCE_PREF_DICT}.value`;

// Currently supported macros in FaceGaze.
export const FaceGazeActions: MacroName[] = [
  MacroName.MOUSE_CLICK_LEFT,
  MacroName.MOUSE_CLICK_LEFT_DOUBLE,
  MacroName.MOUSE_CLICK_RIGHT,
  MacroName.MOUSE_LONG_CLICK_LEFT,
  MacroName.RESET_CURSOR,
  MacroName.TOGGLE_DICTATION,
  MacroName.KEY_PRESS_TOGGLE_OVERVIEW,
  MacroName.KEY_PRESS_MEDIA_PLAY_PAUSE,
  MacroName.TOGGLE_SCROLL_MODE,
  MacroName.TOGGLE_VIRTUAL_KEYBOARD,
];

// All possible facial gestures.
// Values are extracted here for ease of use.
export const FaceGazeGestures = Object.values(FacialGesture);

export class FaceGazeCommandPair {
  action: MacroName;
  gesture: FacialGesture|null;

  constructor(action: MacroName, gesture: FacialGesture|null) {
    this.action = action;
    this.gesture = gesture;
  }

  equals(other: FaceGazeCommandPair): boolean {
    return this.action === other.action && this.gesture === other.gesture;
  }
}

export const FACEGAZE_COMMAND_PAIR_ADDED_EVENT_NAME =
    'facegaze-command-pair-added' as const;

export const FACEGAZE_ACTION_ASSIGN_GESTURE_EVENT_NAME =
    'facegaze-assign-gesture' as const;

export class FaceGazeUtils {
  /**
   * @param gesture The FacialGesture for which to return the display text.
   * @returns a string containing the user-friendly display text for the
   *     gesture.
   */
  static getGestureDisplayText(gesture: FacialGesture|null): string {
    // TODO(b:341770655): Localize these strings.
    switch (gesture) {
      case FacialGesture.BROW_INNER_UP:
        return 'Raise eyebrows';
      case FacialGesture.BROWS_DOWN:
        return 'Lower eyebrows';
      case FacialGesture.EYE_SQUINT_LEFT:
        return 'Close left eye';
      case FacialGesture.EYE_SQUINT_RIGHT:
        return 'Close right eye';
      case FacialGesture.EYES_BLINK:
        return 'Blink both eyes';
      case FacialGesture.EYES_LOOK_DOWN:
        return 'Look down';
      case FacialGesture.EYES_LOOK_LEFT:
        return 'Look left';
      case FacialGesture.EYES_LOOK_RIGHT:
        return 'Look right';
      case FacialGesture.EYES_LOOK_UP:
        return 'Look up';
      case FacialGesture.JAW_LEFT:
        return 'Open mouth shift left';
      case FacialGesture.JAW_OPEN:
        return 'Open your mouth wide';
      case FacialGesture.JAW_RIGHT:
        return 'Open mouth shift left';
      case FacialGesture.MOUTH_FUNNEL:
        return 'Mouth funnel shape';
      case FacialGesture.MOUTH_LEFT:
        return 'Stretch left corner of your mouth';
      case FacialGesture.MOUTH_PUCKER:
        return 'Put lips together (like a kiss)';
      case FacialGesture.MOUTH_RIGHT:
        return 'Stretch right corner of your mouth';
      case FacialGesture.MOUTH_SMILE:
        return 'Smile';
      case FacialGesture.MOUTH_UPPER_UP:
        return 'Wrinkle your nose';
      default:
        console.error(
            'Display text requested for unsupported FacialGesture ' + gesture);
        return '';
    }
  }

  /**
   * @param macro The MacroName for which to return the display text.
   * @returns a string containing the user-friendly display text for the macro.
   */
  static getMacroDisplayText(macro: MacroName): string {
    // TODO(b:341770655): Localize these strings.
    switch (macro) {
      case MacroName.MOUSE_CLICK_LEFT:
        return 'Click a mouse button';
      case MacroName.MOUSE_CLICK_LEFT_DOUBLE:
        return 'Double click the mouse';
      case MacroName.MOUSE_CLICK_RIGHT:
        return 'Right-click the mouse';
      case MacroName.MOUSE_LONG_CLICK_LEFT:
        return 'Drag and drop';
      case MacroName.RESET_CURSOR:
        return 'Reset cursor to center';
      case MacroName.TOGGLE_DICTATION:
        return 'Start or stop dictation';
      case MacroName.KEY_PRESS_TOGGLE_OVERVIEW:
        return 'Open overview of windows';
      case MacroName.KEY_PRESS_MEDIA_PLAY_PAUSE:
        return 'Play or pause media';
      case MacroName.TOGGLE_SCROLL_MODE:
        return 'Toggle scroll mode';
      case MacroName.TOGGLE_VIRTUAL_KEYBOARD:
        return 'Show or hide the virtual keyboard';
      default:
        // Other macros are not supported in FaceGaze.
        console.error('Display text requested for unsupported macro ' + macro);
        return '';
    }
  }
}
