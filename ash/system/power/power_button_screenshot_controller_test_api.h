// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_POWER_BUTTON_SCREENSHOT_CONTROLLER_TEST_API_H_
#define ASH_SYSTEM_POWER_POWER_BUTTON_SCREENSHOT_CONTROLLER_TEST_API_H_

#include "base/compiler_specific.h"
#include "base/macros.h"

namespace ash {

class PowerButtonScreenshotController;

// Helper class used by tests to access PowerButtonScreenshotController's
// internal state.
class PowerButtonScreenshotControllerTestApi {
 public:
  explicit PowerButtonScreenshotControllerTestApi(
      PowerButtonScreenshotController* controller);
  ~PowerButtonScreenshotControllerTestApi();

  // If |controller_->volume_down_timer_| is running, stops it, runs its task,
  // and returns true. Otherwise returns false.
  bool TriggerVolumeDownTimer() WARN_UNUSED_RESULT;

  // If |controller_->volume_up_timer_| is running, stops it, runs its task,
  // and returns true. Otherwise returns false.
  bool TriggerVolumeUpTimer() WARN_UNUSED_RESULT;

 private:
  PowerButtonScreenshotController* controller_;

  DISALLOW_COPY_AND_ASSIGN(PowerButtonScreenshotControllerTestApi);
};

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_POWER_BUTTON_SCREENSHOT_CONTROLLER_TEST_API_H_
