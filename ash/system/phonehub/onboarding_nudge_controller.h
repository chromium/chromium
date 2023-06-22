// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_ONBOARDING_NUDGE_CONTROLLER_H_
#define ASH_SYSTEM_PHONEHUB_ONBOARDING_NUDGE_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/functional/callback.h"

namespace ash {

class PhoneHubTray;

// This class is responsible for displaying Phone Hub onboarding nudge when
// applicable.
class ASH_EXPORT OnboardingNudgeController {
 public:
  OnboardingNudgeController(PhoneHubTray* phone_hub_tray,
                            base::RepeatingClosure stop_animation_callback,
                            base::RepeatingClosure start_animation_callback);
  OnboardingNudgeController(const OnboardingNudgeController&) = delete;
  OnboardingNudgeController& operator=(const OnboardingNudgeController&) =
      delete;
  ~OnboardingNudgeController();

  void ShowNudgeIfNeeded();
  void HideNudge();

  // Attempts recording nudge action metric when Phone Hub icon is activated.
  void MaybeRecordNudgeAction();

 private:
  PhoneHubTray* phone_hub_tray_;
  base::RepeatingClosure stop_animation_callback_;
  base::RepeatingClosure start_animation_callback_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_ONBOARDING_NUDGE_CONTROLLER_H_
