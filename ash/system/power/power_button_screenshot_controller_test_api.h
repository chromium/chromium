// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_POWER_BUTTON_SCREENSHOT_CONTROLLER_TEST_API_H_
#define ASH_SYSTEM_POWER_POWER_BUTTON_SCREENSHOT_CONTROLLER_TEST_API_H_

#include "base/memory/raw_ptr.h"

namespace ash {

class PowerButtonScreenshotController;

// Helper class used by tests to access PowerButtonScreenshotController's
// internal state.
class PowerButtonScreenshotControllerTestApi {
 public:
  explicit PowerButtonScreenshotControllerTestApi(
      PowerButtonScreenshotController* controller);

  PowerButtonScreenshotControllerTestApi(
      const PowerButtonScreenshotControllerTestApi&) = delete;
  PowerButtonScreenshotControllerTestApi& operator=(
      const PowerButtonScreenshotControllerTestApi&) = delete;

  ~PowerButtonScreenshotControllerTestApi();

  // If |controller_->volume_down_timer_| is running, stops it, runs its task,
  // and returns true. Otherwise returns false.
  [[nodiscard]] bool TriggerVolumeDownTimer();

  // If |controller_->volume_up_timer_| is running, stops it, runs its task,
  // and returns true. Otherwise returns false.
  [[nodiscard]] bool TriggerVolumeUpTimer();

 private:
  raw_ptr<PowerButtonScreenshotController, DanglingUntriaged> controller_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_POWER_BUTTON_SCREENSHOT_CONTROLLER_TEST_API_H_
