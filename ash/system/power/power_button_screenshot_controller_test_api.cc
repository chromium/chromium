// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/power_button_screenshot_controller_test_api.h"

#include "ash/system/power/power_button_screenshot_controller.h"

namespace ash {

PowerButtonScreenshotControllerTestApi::PowerButtonScreenshotControllerTestApi(
    PowerButtonScreenshotController* controller)
    : controller_(controller) {
  DCHECK(controller_);
}

PowerButtonScreenshotControllerTestApi::
    ~PowerButtonScreenshotControllerTestApi() = default;

bool PowerButtonScreenshotControllerTestApi::TriggerVolumeDownTimer() {
  if (!controller_->volume_down_timer_.IsRunning())
    return false;

  controller_->volume_down_timer_.FireNow();
  return true;
}

bool PowerButtonScreenshotControllerTestApi::TriggerVolumeUpTimer() {
  if (!controller_->volume_up_timer_.IsRunning())
    return false;

  controller_->volume_up_timer_.FireNow();
  return true;
}

}  // namespace ash
