// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestImportManager} from '/common/testing/test_import_manager.js';

export const SettingsPath = 'manageAccessibility/faceGaze';

/** Keep in sync with with values at ash_pref_names.h. */
export enum PrefNames {
  ACCELERATOR_DIALOG_HAS_BEEN_ACCEPTED =
      'settings.a11y.face_gaze.accelerator_dialog_has_been_accepted',
  ACTIONS_ENABLED = 'settings.a11y.face_gaze.actions_enabled',
  ACTIONS_ENABLED_SENTINEL = 'settings.a11y.face_gaze.actions_enabled_sentinel',
  CURSOR_CONTROL_ENABLED = 'settings.a11y.face_gaze.cursor_control_enabled',
  CURSOR_CONTROL_ENABLED_SENTINEL =
      'settings.a11y.face_gaze.cursor_control_enabled_sentinel',
  CURSOR_USE_ACCELERATION = 'settings.a11y.face_gaze.cursor_use_acceleration',
  FACE_GAZE_ENABLED = 'settings.a11y.face_gaze.enabled',
  FACE_GAZE_ENABLED_SENTINEL = 'settings.a11y.face_gaze.enabled_sentinel',
  FACE_GAZE_ENABLED_SENTINEL_SHOW_DIALOG =
      'settings.a11y.face_gaze.enabled_sentinel_show_dialog',
  GESTURE_TO_CONFIDENCE = 'settings.a11y.face_gaze.gestures_to_confidence',
  GESTURE_TO_KEY_COMBO = 'settings.a11y.face_gaze.gestures_to_key_combos',
  GESTURE_TO_MACRO = 'settings.a11y.face_gaze.gestures_to_macros',
  PRECISION_CLICK = 'settings.a11y.face_gaze.precision_click',
  PRECISION_CLICK_SPEED_FACTOR =
      'settings.a11y.face_gaze.precision_click_speed_factor',
  SPD_DOWN = 'settings.a11y.face_gaze.cursor_speed_down',
  SPD_LEFT = 'settings.a11y.face_gaze.cursor_speed_left',
  SPD_RIGHT = 'settings.a11y.face_gaze.cursor_speed_right',
  SPD_UP = 'settings.a11y.face_gaze.cursor_speed_up',
  VELOCITY_THRESHOLD = 'settings.a11y.face_gaze.velocity_threshold',
}

TestImportManager.exportForTesting(['PrefNames', PrefNames]);
