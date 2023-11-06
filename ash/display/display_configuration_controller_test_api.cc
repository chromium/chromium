// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_configuration_controller_test_api.h"

#include "ash/display/display_configuration_controller.h"
#include "ash/root_window_controller.h"
#include "ash/rotator/screen_rotation_animator.h"
#include "ash/shell.h"

namespace ash {

DisplayConfigurationControllerTestApi::DisplayConfigurationControllerTestApi(
    DisplayConfigurationController* controller)
    : controller_(controller) {}

void DisplayConfigurationControllerTestApi::SetDisplayAnimator(bool enable) {
  controller_->SetAnimatorForTest(enable);
}

ScreenRotationAnimator*
DisplayConfigurationControllerTestApi::GetScreenRotationAnimatorForDisplay(
    int64_t display_id) {
  return controller_->GetScreenRotationAnimatorForDisplay(display_id);
}

void DisplayConfigurationControllerTestApi::SetScreenRotationAnimatorForDisplay(
    int64_t display_id,
    std::unique_ptr<ScreenRotationAnimator> animator) {
  Shell::GetRootWindowControllerWithDisplayId(display_id)
      ->SetScreenRotationAnimatorForTest(std::move(animator));
}

}  // namespace ash
