// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_EDUCATION_CONTROLLER_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_EDUCATION_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/time/clock.h"

class PrefRegistrySimple;

namespace ash {

// Controller for showing user education for Screen Capture entry points via a
// system nudge indicating the keyboard shortcut to take a screenshot.
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

  // If a form of user education has already been shown 3 times or once in the
  // past 24 hours, returns. Otherwise, shows the shortcut nudge.
  void MaybeShowEducation();

  // Closes any Screen Capture nudges that may be open.
  void CloseAllEducationNudgesAndTutorials();

 private:
  friend class CaptureModeEducationControllerTest;

  // Used to control the clock in a test setting.
  static void SetOverrideClockForTesting(base::Clock* test_clock);

  // Shows an unanchored system nudge indicating the keyboard shortcut to take a
  // screenshot.
  void ShowShortcutNudge();

  // If set to true, ignores the 3 times/24 hours show limit for testing.
  bool skip_prefs_for_test_ = false;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_EDUCATION_CONTROLLER_H_
