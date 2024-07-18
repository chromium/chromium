// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FacialGesture} from 'chrome://resources/ash/common/accessibility/facial_gestures.js';
import {MacroName} from 'chrome://resources/ash/common/accessibility/macro_names.js';

export const FACE_GAZE_GESTURE_TO_MACROS_PREF =
    'prefs.settings.a11y.face_gaze.gestures_to_macros.value';

// Currently supported macros in FaceGaze.
export const FaceGazeActions: MacroName[] = [
  MacroName.MOUSE_CLICK_LEFT,
  MacroName.MOUSE_CLICK_RIGHT,
  MacroName.MOUSE_LONG_CLICK_LEFT,
  MacroName.RESET_CURSOR,
  MacroName.TOGGLE_DICTATION,
  MacroName.KEY_PRESS_SPACE,
  MacroName.KEY_PRESS_DOWN,
  MacroName.KEY_PRESS_LEFT,
  MacroName.KEY_PRESS_RIGHT,
  MacroName.KEY_PRESS_UP,
  MacroName.KEY_PRESS_TOGGLE_OVERVIEW,
  MacroName.KEY_PRESS_MEDIA_PLAY_PAUSE,
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
        return 'Brow inner up';
      case FacialGesture.BROWS_DOWN:
        return 'Brows down';
      case FacialGesture.EYE_SQUINT_LEFT:
        return 'Squint left eye';
      case FacialGesture.EYE_SQUINT_RIGHT:
        return 'Squint right eye';
      case FacialGesture.EYES_BLINK:
        return 'Eyes blink';
      case FacialGesture.EYES_LOOK_DOWN:
        return 'Eyes look down';
      case FacialGesture.EYES_LOOK_LEFT:
        return 'Eyes look left';
      case FacialGesture.EYES_LOOK_RIGHT:
        return 'Eyes look right';
      case FacialGesture.EYES_LOOK_UP:
        return 'Eyes look up';
      case FacialGesture.JAW_OPEN:
        return 'Jaw open';
      case FacialGesture.MOUTH_LEFT:
        return 'Mouth left';
      case FacialGesture.MOUTH_PUCKER:
        return 'Mouth pucker';
      case FacialGesture.MOUTH_RIGHT:
        return 'Mouth right';
      case FacialGesture.MOUTH_SMILE:
        return 'Mouth smile';
      case FacialGesture.MOUTH_UPPER_UP:
        return 'Mouth upper up';
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
      case MacroName.MOUSE_CLICK_RIGHT:
        return 'Right-click the mouse';
      case MacroName.MOUSE_LONG_CLICK_LEFT:
        return 'Long click mouse';
      case MacroName.RESET_CURSOR:
        return 'Reset cursor to center';
      case MacroName.TOGGLE_DICTATION:
        return 'Start or stop dictation';
      case MacroName.KEY_PRESS_SPACE:
        return 'Press space key';
      case MacroName.KEY_PRESS_DOWN:
        return 'Press down key';
      case MacroName.KEY_PRESS_LEFT:
        return 'Press left key';
      case MacroName.KEY_PRESS_RIGHT:
        return 'Press right key';
      case MacroName.KEY_PRESS_UP:
        return 'Press up key';
      case MacroName.KEY_PRESS_TOGGLE_OVERVIEW:
        return 'Toggle overview';
      case MacroName.KEY_PRESS_MEDIA_PLAY_PAUSE:
        return 'Play or pause media';
      default:
        // Other macros are not supported in FaceGaze.
        console.error('Display text requested for unsupported macro ' + macro);
        return '';
    }
  }
}
