// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_education_controller.h"

#include "ash/constants/ash_features.h"

namespace ash {

CaptureModeEducationController::CaptureModeEducationController() = default;

CaptureModeEducationController::~CaptureModeEducationController() = default;

// static
bool CaptureModeEducationController::IsArm1ShortcutNudgeEnabled() {
  return features::IsCaptureModeEducationEnabled() &&
         features::kCaptureModeEducationParam.Get() ==
             features::CaptureModeEducationParam::kShortcutNudge;
}

// static
bool CaptureModeEducationController::IsArm2ShortcutTutorialEnabled() {
  return features::IsCaptureModeEducationEnabled() &&
         features::kCaptureModeEducationParam.Get() ==
             features::CaptureModeEducationParam::kShortcutTutorial;
}

// static
bool CaptureModeEducationController::IsArm3SettingsNudgeEnabled() {
  return features::IsCaptureModeEducationEnabled() &&
         features::kCaptureModeEducationParam.Get() ==
             features::CaptureModeEducationParam::kSettingsNudge;
}

}  // namespace ash
