// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_EDUCATION_CONTROLLER_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_EDUCATION_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/memory/weak_ptr.h"
#include "base/time/clock.h"
#include "ui/views/widget/unique_widget_ptr.h"

class PrefRegistrySimple;

namespace ash {

// Controller for showing the different forms of user education for Screen
// Capture entry points. Education is split into three different arms:
//  - Arm 1: Shortcut Nudge. A simple system nudge appears with text indicating
//    the keyboard shortcut to take a screenshot.
//  - Arm 2: Shortcut Tutorial. Similar to Arm 1, but the nudge also appears
//    with a button that opens a new popup, showing the keyboard layout of where
//    the shortcut keys are found.
//  - Arm 3: Quick Settings Nudge. A system nudge anchored to the quick settings
//    button in the shelf, with text alerting users to the Screen Capture tile
//    in the quick settings menu.
class ASH_EXPORT CaptureModeEducationController {
 public:
  CaptureModeEducationController();
  CaptureModeEducationController(const CaptureModeEducationController&) =
      delete;
  CaptureModeEducationController& operator=(
      const CaptureModeEducationController&) = delete;
  ~CaptureModeEducationController();

  // Registers prefs related to user education show count and time last shown.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Returns true if the feature flag 'kCaptureModeEducation' is enabled and
  // the associated param is 'kShortcutNudge';
  static bool IsArm1ShortcutNudgeEnabled();

  // Returns true if the feature flag 'kCaptureModeEducation' is enabled and
  // the associated param is 'kShortcutTutorial';
  static bool IsArm2ShortcutTutorialEnabled();

  // Returns true if the feature flag 'kCaptureModeEducation' is enabled and
  // the associated param is 'kQuickSettingsNudge';
  static bool IsArm3QuickSettingsNudgeEnabled();

  // If a form of user education has already been shown 3 times or once in the
  // past 24 hours, returns. Otherwise, shows the appropriate form of user
  // education based on the enabled arm/feature param.
  void MaybeShowEducation();

  // Closes any Screen Capture nudges or tutorials that may be open.
  void CloseAllEducationNudgesAndTutorials();

  views::Widget* tutorial_widget_for_test() { return tutorial_widget_.get(); }

 private:
  friend class CaptureModeEducationControllerTest;

  // Used to control the clock in a test setting.
  static void SetOverrideClockForTesting(base::Clock* test_clock);

  // Shows Arm 1, an unanchored system nudge indicating the keyboard shortcut to
  // take a screenshot.
  void ShowShortcutNudge();

  // Shows Arm 2, an unanchored system nudge indicating the keyboard shortcut to
  // take a screenshot, with a button to open a new tutorial widget.
  void ShowTutorialNudge();

  // Shows Arm 3, a system nudge anchored to the unified system tray button,
  // indicating the location of the screen capture tool in the quick settings
  // menu.
  void ShowQuickSettingsNudge();

  // Creates and shows the system dialog displaying the keyboard shortcut and
  // illustration for taking a screenshot.
  void CreateAndShowTutorialDialog();

  // Closes the nudge and shows the tutorial dialog for Arm 2.
  void OnShowMeHowButtonPressed();

  // If set to true, ignores the 3 times/24 hours show limit for testing.
  bool skip_prefs_for_test_ = false;

  // The widget that contains the tutorial dialog view for Arm 2.
  views::UniqueWidgetPtr tutorial_widget_;

  base::WeakPtrFactory<CaptureModeEducationController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_EDUCATION_CONTROLLER_H_
