// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_EDUCATION_CONTROLLER_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_EDUCATION_CONTROLLER_H_

namespace ash {

// Controller for showing the different forms of user education for Screen
// Capture entry points. Education is split into three different arms:
//  - Arm 1: Shortcut Nudge. A simple system nudge appears with text indicating
//    the keyboard shortcut to take a screenshot.
//  - Arm 2: Shortcut Tutorial. Similar to Arm 1, but the nudge also appears
//    with a button that opens a new popup, showing the keyboard layout of where
//    the shortcut keys are found.
//  - Arm 3: Settings Nudge. A system nudge anchored to the quick settings
//    button in the shelf, with text alerting users to the Screen Capture tile
//    in the quick settings menu.
class CaptureModeEducationController {
 public:
  CaptureModeEducationController();
  CaptureModeEducationController(const CaptureModeEducationController&) =
      delete;
  CaptureModeEducationController& operator=(
      const CaptureModeEducationController&) = delete;
  ~CaptureModeEducationController();

  // Returns true if the feature flag 'kCaptureModeEducation' is enabled and
  // the associated param is 'kShortcutNudge';
  static bool IsArm1ShortcutNudgeEnabled();

  // Returns true if the feature flag 'kCaptureModeEducation' is enabled and
  // the associated param is 'kShortcutTutorial';
  static bool IsArm2ShortcutTutorialEnabled();

  // Returns true if the feature flag 'kCaptureModeEducation' is enabled and
  // the associated param is 'kSettingsNudge';
  static bool IsArm3SettingsNudgeEnabled();
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_EDUCATION_CONTROLLER_H_
