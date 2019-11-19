// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/power_button_screenshot_controller.h"

#include <memory>

#include "ash/login_status.h"
#include "ash/shell.h"
#include "ash/system/power/power_button_controller.h"
#include "ash/system/power/power_button_controller_test_api.h"
#include "ash/system/power/power_button_screenshot_controller_test_api.h"
#include "ash/system/power/power_button_test_base.h"
#include "ash/test_screenshot_delegate.h"
#include "ash/wm/lock_state_controller_test_api.h"
#include "base/test/simple_test_tick_clock.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "ui/events/event.h"

namespace ash {

// Test fixture used for testing power button screenshot behavior under tablet
// power button.
class PowerButtonScreenshotControllerTest : public PowerButtonTestBase {
 public:
  PowerButtonScreenshotControllerTest() = default;
  ~PowerButtonScreenshotControllerTest() override = default;

  // PowerButtonTestBase:
  void SetUp() override {
    PowerButtonTestBase::SetUp();
    InitPowerButtonControllerMembers(
        chromeos::PowerManagerClient::TabletMode::ON);
    InitScreenshotTestApi();
    screenshot_delegate_ = GetScreenshotDelegate();
    EnableTabletMode(true);

    // Advance a duration longer than |kIgnorePowerButtonAfterResumeDelay| to
    // avoid events being ignored.
    tick_clock_.Advance(
        PowerButtonController::kIgnorePowerButtonAfterResumeDelay +
        base::TimeDelta::FromMilliseconds(2));
  }

 protected:
  // PowerButtonTestBase:
  void PressKey(ui::KeyboardCode key_code) override {
    last_key_event_ = std::make_unique<ui::KeyEvent>(ui::ET_KEY_PRESSED,
                                                     key_code, ui::EF_NONE);
    screenshot_controller_->OnKeyEvent(last_key_event_.get());
  }
  void ReleaseKey(ui::KeyboardCode key_code) override {
    last_key_event_ = std::make_unique<ui::KeyEvent>(ui::ET_KEY_RELEASED,
                                                     key_code, ui::EF_NONE);
    screenshot_controller_->OnKeyEvent(last_key_event_.get());
  }

  void InitScreenshotTestApi() {
    screenshot_test_api_ =
        std::make_unique<PowerButtonScreenshotControllerTestApi>(
            screenshot_controller_);
  }

  int GetScreenshotCount() const {
    return screenshot_delegate_->handle_take_screenshot_count();
  }

  void ResetScreenshotCount() {
    return screenshot_delegate_->reset_handle_take_screenshot_count();
  }

  bool LastKeyConsumed() const {
    DCHECK(last_key_event_);
    return last_key_event_->stopped_propagation();
  }

  TestScreenshotDelegate* screenshot_delegate_ = nullptr;  // Not owned.
  std::unique_ptr<PowerButtonScreenshotControllerTestApi> screenshot_test_api_;

  // Stores the last key event. Can be NULL if not set through PressKey() or
  // ReleaseKey().
  std::unique_ptr<ui::KeyEvent> last_key_event_;

  DISALLOW_COPY_AND_ASSIGN(PowerButtonScreenshotControllerTest);
};

// Tests the functionalities that press the power button first and then press
// volume down and volume up key alternative.
TEST_F(PowerButtonScreenshotControllerTest,
       PowerButtonPressedFirst_Screenshot) {
  PressPowerButton();
  tick_clock_.Advance(PowerButtonScreenshotController::kScreenshotChordDelay -
                      base::TimeDelta::FromMilliseconds(5));
  PressKey(ui::VKEY_VOLUME_DOWN);
  // Verifies screenshot is taken, volume down is consumed.
  EXPECT_EQ(1, GetScreenshotCount());
  EXPECT_TRUE(LastKeyConsumed());
  // Presses volume up key under screenshot chord condition will not take
  // screenshot again, volume up is also consumed.
  tick_clock_.Advance(base::TimeDelta::FromMilliseconds(2));
  ResetScreenshotCount();
  PressKey(ui::VKEY_VOLUME_UP);
  EXPECT_EQ(0, GetScreenshotCount());
  EXPECT_TRUE(LastKeyConsumed());
  // Presses volume down key again under screenshot chord condition will not
  // take screenshot and still consume volume down event.
  ResetScreenshotCount();
  tick_clock_.Advance(base::TimeDelta::FromMilliseconds(2));
  PressKey(ui::VKEY_VOLUME_DOWN);
  EXPECT_EQ(0, GetScreenshotCount());
  EXPECT_TRUE(LastKeyConsumed());
  // Keeps pressing volume down key outside of screenshot chord condition will
  // not take screenshot and still consume volume down event.
  tick_clock_.Advance(base::TimeDelta::FromMilliseconds(2));
  PressKey(ui::VKEY_VOLUME_DOWN);
  EXPECT_EQ(0, GetScreenshotCount());
  EXPECT_TRUE(LastKeyConsumed());
  // Pressing volume up key outside of screenshot chord condition will not take
  // screenshot and still consume volume up event.
  PressKey(ui::VKEY_VOLUME_UP);
  EXPECT_EQ(0, GetScreenshotCount());
  EXPECT_TRUE(LastKeyConsumed());

  // Releases power button now should not set display off.
  ReleasePowerButton();
  EXPECT_FALSE(power_manager_client()->backlights_forced_off());
  // Releases volume up key, and verifies nothing happens.
  ReleaseKey(ui::VKEY_VOLUME_UP);
  EXPECT_EQ(0, GetScreenshotCount());
  EXPECT_FALSE(LastKeyConsumed());
  // Releases volume down key, and verifies nothing happens.
  ReleaseKey(ui::VKEY_VOLUME_DOWN);
  EXPECT_EQ(0, GetScreenshotCount());
  EXPECT_FALSE(LastKeyConsumed());
}

// Tests the functionalities that press the volume key first and then press
// volume down and volume up key alternative.
TEST_F(PowerButtonScreenshotControllerTest, VolumeKeyPressedFirst_Screenshot) {
  // Tests when volume up pressed first, it waits for power button pressed
  // screenshot chord.
  PressKey(ui::VKEY_VOLUME_UP);
  EXPECT_TRUE(LastKeyConsumed());
  // Presses power button under screenshot chord condition, and verifies that
  // screenshot is taken.
  tick_clock_.Advance(PowerButtonScreenshotController::kScreenshotChordDelay -
                      base::TimeDelta::FromMilliseconds(5));
  PressPowerButton();
  EXPECT_EQ(1, GetScreenshotCount());
  // Presses volume down key under screenshot chord condition will not take
  // screenshot, volume down is also consumed.
  tick_clock_.Advance(base::TimeDelta::FromMilliseconds(2));
  ResetScreenshotCount();
  PressKey(ui::VKEY_VOLUME_DOWN);
  EXPECT_EQ(0, GetScreenshotCount());
  EXPECT_TRUE(LastKeyConsumed());
  // Presses volume up key under screenshot chord condition again will not take
  // screenshot and still consume volume up event.
  tick_clock_.Advance(base::TimeDelta::FromMilliseconds(2));
  PressKey(ui::VKEY_VOLUME_UP);
  EXPECT_EQ(0, GetScreenshotCount());
  EXPECT_TRUE(LastKeyConsumed());
  // Keeps pressing volume up key outside of screenshot chord condition will not
  // take screenshot and still consume volume up event.
  tick_clock_.Advance(base::TimeDelta::FromMilliseconds(2));
  PressKey(ui::VKEY_VOLUME_UP);
  EXPECT_EQ(0, GetScreenshotCount());
  EXPECT_TRUE(LastKeyConsumed());
  // Presses volume down key outside of screenshot chord condition will not take
  // screenshot and still consume volume down event.
  PressKey(ui::VKEY_VOLUME_DOWN);
  EXPECT_EQ(0, GetScreenshotCount());
  EXPECT_TRUE(LastKeyConsumed());

  // Releases power button now should not set display off.
  ReleasePowerButton();
  EXPECT_FALSE(power_manager_client()->backlights_forced_off());
  // Releases volume down key, and verifies nothing happens.
  ReleaseKey(ui::VKEY_VOLUME_DOWN);
  EXPECT_EQ(0, GetScreenshotCount());
  EXPECT_FALSE(LastKeyConsumed());
  // Releases volume up key, and verifies nothing happens.
  ReleaseKey(ui::VKEY_VOLUME_UP);
  EXPECT_EQ(0, GetScreenshotCount());
  EXPECT_FALSE(LastKeyConsumed());
}

class PowerButtonScreenshotControllerWithKeyCodeTest
    : public PowerButtonScreenshotControllerTest,
      public testing::WithParamInterface<ui::KeyboardCode> {
 public:
  PowerButtonScreenshotControllerWithKeyCodeTest() : key_code_(GetParam()) {}

  ui::KeyboardCode key_code() const { return key_code_; }

 private:
  // Value of the |key_code_| will only be ui::VKEY_VOLUME_DOWN or
  // ui::VKEY_VOLUME_UP.
  ui::KeyboardCode key_code_ = ui::VKEY_UNKNOWN;

  DISALLOW_COPY_AND_ASSIGN(PowerButtonScreenshotControllerWithKeyCodeTest);
};

// Tests power button screenshot accelerator works in tablet mode only.
TEST_P(PowerButtonScreenshotControllerWithKeyCodeTest, TabletMode) {
  // Tests in tablet mode pressing power button and volume down/up
  // simultaneously takes a screenshot.
  PressKey(key_code());
  PressPowerButton();
  ReleaseKey(key_code());
  ReleasePowerButton();
  EXPECT_EQ(1, GetScreenshotCount());

  // Tests screenshot handling is not active when not in tablet mode.
  ResetScreenshotCount();
  EnableTabletMode(false);
  PressKey(key_code());
  PressPowerButton();
  ReleaseKey(key_code());
  ReleasePowerButton();
  EXPECT_EQ(0, GetScreenshotCount());
}

// Tests if power-button/volume-down(volume-up) is released before
// volume-down(volume-up)/power-button is pressed, it does not take screenshot.
TEST_P(PowerButtonScreenshotControllerWithKeyCodeTest,
       ReleaseBeforeAnotherPressed) {
  // Releases volume down/up before power button is pressed.
  PressKey(key_code());
  ReleaseKey(key_code());
  PressPowerButton();
  ReleasePowerButton();
  EXPECT_EQ(0, GetScreenshotCount());

  // Releases power button before volume down/up is pressed.
  PressPowerButton();
  ReleasePowerButton();
  PressKey(key_code());
  ReleaseKey(key_code());
  EXPECT_EQ(0, GetScreenshotCount());
}

// Tests power button is pressed first and meets screenshot chord condition.
TEST_P(PowerButtonScreenshotControllerWithKeyCodeTest,
       PowerButtonPressedFirst_ScreenshotChord) {
  PressPowerButton();
  tick_clock_.Advance(PowerButtonScreenshotController::kScreenshotChordDelay -
                      base::TimeDelta::FromMilliseconds(2));
  PressKey(key_code());
  // Verifies screenshot is taken, volume down/up is consumed.
  EXPECT_EQ(1, GetScreenshotCount());
  EXPECT_TRUE(LastKeyConsumed());
  // Keeps pressing volume down/up key under screenshot chord condition will not
  // take screenshot again, volume down/up is also consumed.
  tick_clock_.Advance(base::TimeDelta::FromMilliseconds(1));
  ResetScreenshotCount();
  PressKey(key_code());
  EXPECT_EQ(0, GetScreenshotCount());
  EXPECT_TRUE(LastKeyConsumed());
  // Keeps pressing volume down/up key off screenshot chord condition will not
  // take screenshot and still consume volume down/up event.
  tick_clock_.Advance(base::TimeDelta::FromMilliseconds(2));
  PressKey(key_code());
  EXPECT_EQ(0, GetScreenshotCount());
  EXPECT_TRUE(LastKeyConsumed());

  // Releases power button now should not set display off.
  ReleasePowerButton();
  EXPECT_FALSE(power_manager_client()->backlights_forced_off());
  // Releases volume down/up key, and verifies nothing happens.
  ReleaseKey(key_code());
  EXPECT_EQ(0, GetScreenshotCount());
  EXPECT_FALSE(LastKeyConsumed());
}

// Tests power button is pressed first, and then volume down/up pressed but
// doesn't meet screenshot chord condition.
TEST_P(PowerButtonScreenshotControllerWithKeyCodeTest,
       PowerButtonPressedFirst_NoScreenshotChord) {
  PressPowerButton();
  tick_clock_.Advance(PowerButtonScreenshotController::kScreenshotChordDelay +
                      base::TimeDelta::FromMilliseconds(1));
  PressKey(key_code());
  // Verifies screenshot is not taken, volume down/up is not consumed.
  EXPECT_EQ(0, GetScreenshotCount());
  EXPECT_FALSE(LastKeyConsumed());
  // Keeps pressing volume down/up key should continue triggerring volume
  // down/up.
  tick_clock_.Advance(base::TimeDelta::FromMilliseconds(2));
  PressKey(key_code());
  EXPECT_EQ(0, GetScreenshotCount());
  EXPECT_FALSE(LastKeyConsumed());
  // Releases power button now should not set display off.
  ReleasePowerButton();
  EXPECT_FALSE(power_manager_client()->backlights_forced_off());
  // Releases volume down/up key, and verifies nothing happens.
  ReleaseKey(key_code());
  EXPECT_EQ(0, GetScreenshotCount());
  EXPECT_FALSE(LastKeyConsumed());
}

// Tests volume key pressed cancels the ongoing power button behavior.
TEST_P(PowerButtonScreenshotControllerWithKeyCodeTest,
       PowerButtonPressedFirst_VolumeKeyCancelPowerButton) {
  // Tests volume down/up key can stop power button's shutdown timer and power
  // button menu timer.
  PressPowerButton();
  EXPECT_TRUE(power_button_test_api_->PowerButtonMenuTimerIsRunning());
  PressKey(key_code());
  EXPECT_FALSE(power_button_test_api_->PowerButtonMenuTimerIsRunning());
  ReleasePowerButton();
  ReleaseKey(key_code());
  EXPECT_FALSE(power_manager_client()->backlights_forced_off());
}

// Tests volume key pressed can not cancel the started pre-shutdown animation.
TEST_P(PowerButtonScreenshotControllerWithKeyCodeTest,
       PowerButtonPressedFirst_VolumeKeyNotCancelPowerButton) {
  PressPowerButton();
  ASSERT_TRUE(power_button_test_api_->TriggerPowerButtonMenuTimeout());
  EXPECT_TRUE(power_button_test_api_->PreShutdownTimerIsRunning());
  EXPECT_TRUE(power_button_test_api_->TriggerPreShutdownTimeout());
  EXPECT_TRUE(lock_state_test_api_->shutdown_timer_is_running());
  PressKey(key_code());
  ReleaseKey(key_code());
  EXPECT_TRUE(lock_state_test_api_->shutdown_timer_is_running());
  ReleasePowerButton();
  EXPECT_FALSE(lock_state_test_api_->shutdown_timer_is_running());
}

// Tests volume down/up key pressed first and meets screenshot chord condition.
TEST_P(PowerButtonScreenshotControllerWithKeyCodeTest,
       VolumeKeyPressedFirst_ScreenshotChord) {
  // Tests when volume down/up pressed first, it waits for power button pressed
  // screenshot chord.
  PressKey(key_code());
  EXPECT_TRUE(LastKeyConsumed());
  // Presses power button under screenshot chord condition, and verifies that
  // screenshot is taken.
  tick_clock_.Advance(PowerButtonScreenshotController::kScreenshotChordDelay -
                      base::TimeDelta::FromMilliseconds(2));
  PressPowerButton();
  EXPECT_EQ(1, GetScreenshotCount());
  // Keeps pressing volume down/up key under screenshot chord condition will not
  // take screenshot again, volume down/up is also consumed.
  tick_clock_.Advance(base::TimeDelta::FromMilliseconds(1));
  ResetScreenshotCount();
  PressKey(key_code());
  EXPECT_EQ(0, GetScreenshotCount());
  EXPECT_TRUE(LastKeyConsumed());
  // Keeps pressing volume down/up key off screenshot chord condition will not
  // take screenshot and still consume volume down/up event.
  tick_clock_.Advance(base::TimeDelta::FromMilliseconds(2));
  PressKey(key_code());
  EXPECT_EQ(0, GetScreenshotCount());
  EXPECT_TRUE(LastKeyConsumed());

  // Releases power button now should not set display off.
  ReleasePowerButton();
  EXPECT_FALSE(power_manager_client()->backlights_forced_off());
  // Releases volume down/up key, and verifies nothing happens.
  ReleaseKey(key_code());
  EXPECT_EQ(0, GetScreenshotCount());
  EXPECT_FALSE(LastKeyConsumed());
}

// Tests volume down/up key pressed first, and then power button pressed but
// doesn't meet screenshot chord condition.
TEST_P(PowerButtonScreenshotControllerWithKeyCodeTest,
       VolumeKeyPressedFirst_NoScreenshotChord) {
  // Tests when volume down/up pressed first, it waits for power button pressed
  // screenshot chord.
  PressKey(key_code());
  EXPECT_TRUE(LastKeyConsumed());
  // Advances |tick_clock_| to off screenshot chord point. This will also
  // trigger volume down/up timer timeout, which will perform a volume down/up
  // operation.
  tick_clock_.Advance(PowerButtonScreenshotController::kScreenshotChordDelay +
                      base::TimeDelta::FromMilliseconds(1));
  if (key_code() == ui::VKEY_VOLUME_DOWN)
    EXPECT_TRUE(screenshot_test_api_->TriggerVolumeDownTimer());
  else
    EXPECT_TRUE(screenshot_test_api_->TriggerVolumeUpTimer());
  // Presses power button would not take screenshot.
  PressPowerButton();
  EXPECT_EQ(0, GetScreenshotCount());
  // Keeps pressing volume down/up key should continue triggerring volume
  // down/up.
  tick_clock_.Advance(base::TimeDelta::FromMilliseconds(2));
  PressKey(key_code());
  EXPECT_EQ(0, GetScreenshotCount());
  EXPECT_FALSE(LastKeyConsumed());
  // Releases power button now should not set display off.
  ReleasePowerButton();
  EXPECT_FALSE(power_manager_client()->backlights_forced_off());
  // Releases volume down/up key, and verifies nothing happens.
  ReleaseKey(key_code());
  EXPECT_EQ(0, GetScreenshotCount());
  EXPECT_FALSE(LastKeyConsumed());
}

// Tests volume key pressed first invalidates the power button behavior.
TEST_P(PowerButtonScreenshotControllerWithKeyCodeTest,
       VolumeKeyPressedFirst_InvalidateConvertiblePowerButton) {
  // Tests volume down/up key invalidates the power button behavior.
  PressKey(key_code());
  PressPowerButton();
  EXPECT_FALSE(power_button_test_api_->PowerButtonMenuTimerIsRunning());
  ReleasePowerButton();
  ReleaseKey(key_code());
  EXPECT_FALSE(power_manager_client()->backlights_forced_off());
}

INSTANTIATE_TEST_SUITE_P(AshPowerButtonScreenshot,
                         PowerButtonScreenshotControllerWithKeyCodeTest,
                         testing::Values(ui::VKEY_VOLUME_DOWN,
                                         ui::VKEY_VOLUME_UP));

}  // namespace ash
