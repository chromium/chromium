// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_POWER_BUTTON_TEST_BASE_H_
#define ASH_SYSTEM_POWER_POWER_BUTTON_TEST_BASE_H_

#include <memory>

#include "ash/system/power/power_button_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/test/simple_test_tick_clock.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ash {

class LockStateController;
class LockStateControllerTestApi;
class PowerButtonControllerTestApi;
class PowerButtonScreenshotController;
enum class LoginStatus;

// Base test fixture and utils for testing power button related functions.
class PowerButtonTestBase : public AshTestBase {
 public:
  PowerButtonTestBase();
  ~PowerButtonTestBase() override;

  using ButtonType = PowerButtonController::ButtonType;

  // AshTestBase:
  void SetUp() override;
  void TearDown() override;

 protected:
  // Resets the PowerButtonController and associated members.
  void ResetPowerButtonController();

  // Initializes |power_button_controller_| and other members that point at
  // objects owned by it. If |initial_tablet_mode_switch_state| is not
  // UNSUPPORTED, tablet mode switch will be set and PowerButtonController will
  // create PowerButtonScreenshotController on getting the switch.
  void InitPowerButtonControllerMembers(chromeos::PowerManagerClient::TabletMode
                                            initial_tablet_mode_switch_state);

  // Sets the tablet mode switch state. PowerButtonController will initialize
  // |screenshot_controller_| if the switch state is not UNSUPPORTED.
  void SetTabletModeSwitchState(
      chromeos::PowerManagerClient::TabletMode tablet_mode_switch_state);

  // Simulates a power button press.
  void PressPowerButton();

  // Simulates a power button release.
  void ReleasePowerButton();

  // Simulates a key press based on the given |key_code|.
  virtual void PressKey(ui::KeyboardCode key_code);

  // Simulates a key release based on the given |key_code|.
  virtual void ReleaseKey(ui::KeyboardCode key_code);

  // Simulates a mouse move event.
  void GenerateMouseMoveEvent();

  // Initializes login status and sets power button type.
  void Initialize(ButtonType button_type, LoginStatus status);

  // Triggers a lock screen operation.
  void LockScreen();

  // Triggers a unlock screen operation.
  void UnlockScreen();

  // Enables or disables tablet mode based on |enable|.
  void EnableTabletMode(bool enable);

  // Advance clock to ensure the intended tablet power button display forcing
  // off is not ignored since we will ignore the repeated power button up if
  // they come too close.
  void AdvanceClockToAvoidIgnoring();

  PowerButtonController* power_button_controller_ = nullptr;  // Not owned.
  LockStateController* lock_state_controller_ = nullptr;      // Not owned.
  PowerButtonScreenshotController* screenshot_controller_ =
      nullptr;  // Not owned.
  std::unique_ptr<LockStateControllerTestApi> lock_state_test_api_;
  std::unique_ptr<PowerButtonControllerTestApi> power_button_test_api_;
  base::SimpleTestTickClock tick_clock_;

  DISALLOW_COPY_AND_ASSIGN(PowerButtonTestBase);
};

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_POWER_BUTTON_TEST_BASE_H_
