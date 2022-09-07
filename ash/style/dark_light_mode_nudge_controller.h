// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_DARK_LIGHT_MODE_NUDGE_CONTROLLER_H_
#define ASH_STYLE_DARK_LIGHT_MODE_NUDGE_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/system/tray/system_nudge_controller.h"

namespace ash {

class SystemNudge;

// Controls the showing and hiding of the dark/light mode educational nudge.
// SystemNudgeController will control the animation, time duration etc of the
// nudge.
class ASH_EXPORT DarkLightModeNudgeController : public SystemNudgeController {
 public:
  DarkLightModeNudgeController();
  DarkLightModeNudgeController(const DarkLightModeNudgeController&) = delete;
  DarkLightModeNudgeController& operator=(const DarkLightModeNudgeController&) =
      delete;
  ~DarkLightModeNudgeController() override;

  // Gets the remaining number of times that the educational nudge can be shown.
  static int GetRemainingShownCount();

  // If possible, this will show the nudge that educates the user how to switch
  // between the dark and light mode.
  void MaybeShowNudge();

  // Called when the feature's state is toggled manually by the user.
  void ToggledByUser();

  void set_show_nudge_for_testing(bool value) {
    hide_nudge_for_testing_ = !value;
  }

 protected:
  // SystemNudgeController:
  std::unique_ptr<SystemNudge> CreateSystemNudge() override;

 private:
  // Returns true if the educational nudge should be shown.
  bool ShouldShowNudge() const;

  // Used to indicate whether to hide the nudge in tests. Will be initialized to
  // hide for ash tests through `set_show_nudge_for_testing` above. See
  // AshTestHelper::SetUp for more details.
  bool hide_nudge_for_testing_ = false;
};

}  // namespace ash

#endif  // ASH_STYLE_DARK_LIGHT_MODE_NUDGE_CONTROLLER_H_
