// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/power_button_controller.h"

#include "ash/accessibility/test_accessibility_controller_client.h"
#include "ash/constants/ash_switches.h"
#include "ash/display/screen_orientation_controller.h"
#include "ash/display/screen_orientation_controller_test_api.h"
#include "ash/media/media_controller_impl.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/power/power_button_controller_test_api.h"
#include "ash/system/power/power_button_menu_item_view.h"
#include "ash/system/power/power_button_menu_view.h"
#include "ash/system/power/power_button_menu_view_util.h"
#include "ash/system/power/power_button_test_base.h"
#include "ash/test_media_client.h"
#include "ash/touch/touch_devices_controller.h"
#include "ash/wm/lock_state_controller_test_api.h"
#include "ash/wm/test/test_session_state_animator.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/simple_test_tick_clock.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/suspend.pb.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/event.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/widget/widget.h"

using PowerManagerClient = chromeos::PowerManagerClient;

namespace ash {

namespace {

// A non-zero brightness used for test.
constexpr double kNonZeroBrightness = 10.;

// Width of the display.
constexpr int kDisplayWidth = 2000;

// Height of the display.
constexpr int kDisplayHeight = 1200;

// Power button position offset percentage.
constexpr double kPowerButtonPercentage = 0.9f;

// Shorthand for some long constants.
constexpr power_manager::BacklightBrightnessChange_Cause kUserCause =
    power_manager::BacklightBrightnessChange_Cause_USER_REQUEST;
constexpr power_manager::BacklightBrightnessChange_Cause kOtherCause =
    power_manager::BacklightBrightnessChange_Cause_OTHER;

}  // namespace

using PowerButtonPosition = PowerButtonController::PowerButtonPosition;

class PowerButtonControllerTest : public PowerButtonTestBase {
 public:
  PowerButtonControllerTest() = default;

  PowerButtonControllerTest(const PowerButtonControllerTest&) = delete;
  PowerButtonControllerTest& operator=(const PowerButtonControllerTest&) =
      delete;

  ~PowerButtonControllerTest() override = default;

  void SetUp() override {
    PowerButtonTestBase::SetUp();
    InitPowerButtonControllerMembers(PowerManagerClient::TabletMode::ON);

    SendBrightnessChange(kNonZeroBrightness, kUserCause);
    EXPECT_FALSE(power_manager_client()->backlights_forced_off());

    // Advance a duration longer than |kIgnorePowerButtonAfterResumeDelay| to
    // avoid events being ignored.
    tick_clock_.Advance(
        PowerButtonController::kIgnorePowerButtonAfterResumeDelay +
        base::Milliseconds(2));

    // Run the event loop so that PowerButtonDisplayController can receive the
    // initial backlights-forced-off state.
    base::RunLoop().RunUntilIdle();
  }

 protected:
  void SendBrightnessChange(
      double percent,
      power_manager::BacklightBrightnessChange_Cause cause) {
    power_manager::BacklightBrightnessChange change;
    change.set_percent(percent);
    change.set_cause(cause);
    power_manager_client()->SendScreenBrightnessChanged(change);
  }

  bool GetLockedState() {
    // LockScreen is an async mojo call.
    GetSessionControllerClient()->FlushForTest();
    return Shell::Get()->session_controller()->IsScreenLocked();
  }

  bool GetGlobalTouchscreenEnabled() const {
    return Shell::Get()->touch_devices_controller()->GetTouchscreenEnabled(
        TouchDeviceEnabledSource::GLOBAL);
  }

  // Tapping power button when screen is off will turn the screen on but not
  // showing the menu.
  void TappingPowerButtonWhenScreenIsIdleOff() {
    SendBrightnessChange(0, kUserCause);
    PressPowerButton();
    EXPECT_FALSE(power_manager_client()->backlights_forced_off());
    SendBrightnessChange(kNonZeroBrightness, kUserCause);
    ReleasePowerButton();
    EXPECT_FALSE(power_manager_client()->backlights_forced_off());
    EXPECT_FALSE(power_button_test_api_->PowerButtonMenuTimerIsRunning());
  }

  // Press the power button to show the menu.
  void OpenPowerButtonMenu() {
    PressPowerButton();
    if (display::Screen::GetScreen()->InTabletMode()) {
      EXPECT_TRUE(power_button_test_api_->PowerButtonMenuTimerIsRunning());
      ASSERT_TRUE(power_button_test_api_->TriggerPowerButtonMenuTimeout());
    }
    ReleasePowerButton();
    ASSERT_TRUE(power_button_test_api_->IsMenuOpened());
  }

  // Tap outside of the menu view to dismiss the menu.
  void TapToDismissPowerButtonMenu() {
    gfx::Rect menu_bounds = power_button_test_api_->GetMenuBoundsInScreen();
    gfx::Point point = menu_bounds.bottom_right();
    point.Offset(5, 5);
    GetEventGenerator()->GestureTapAt(point);

    EXPECT_FALSE(power_button_test_api_->IsMenuOpened());
  }

  void PressLockButton() {
    power_button_controller_->OnLockButtonEvent(true, base::TimeTicks::Now());
  }

  void ReleaseLockButton() {
    power_button_controller_->OnLockButtonEvent(false, base::TimeTicks::Now());
  }
};

TEST_F(PowerButtonControllerTest, LockScreenIfRequired) {
  Initialize(ButtonType::NORMAL, LoginStatus::USER);
  EnableTabletMode(true);
  SetShouldLockScreenAutomatically(true);
  ASSERT_FALSE(GetLockedState());

  // On User logged in status, power-button-press-release should lock screen if
  // automatic screen-locking was requested.
  PressPowerButton();
  ReleasePowerButton();
  EXPECT_TRUE(GetLockedState());

  // On locked state, power-button-press-release should do nothing.
  PressPowerButton();
  ReleasePowerButton();
  EXPECT_TRUE(GetLockedState());

  // Unlock the sceen.
  UnlockScreen();
  ASSERT_FALSE(GetLockedState());

  // power-button-press-release should not lock the screen if automatic
  // screen-locking wasn't requested.
  SetShouldLockScreenAutomatically(false);
  PressPowerButton();
  ReleasePowerButton();
  EXPECT_FALSE(GetLockedState());
}

// Tests tapping the power button of a clamshell device.
TEST_F(PowerButtonControllerTest, TappingPowerButtonOfClamshell) {
  // Should not turn the screen off when screen is on.
  InitPowerButtonControllerMembers(PowerManagerClient::TabletMode::UNSUPPORTED);
  EXPECT_FALSE(power_manager_client()->backlights_forced_off());
  PressPowerButton();
  power_button_test_api_->SetShowMenuAnimationDone(false);
  // Start the showing power menu animation immediately as pressing the
  // clamshell power button.
  EXPECT_FALSE(power_button_test_api_->PowerButtonMenuTimerIsRunning());
  EXPECT_TRUE(power_button_test_api_->IsMenuOpened());
  ReleasePowerButton();
  EXPECT_FALSE(power_manager_client()->backlights_forced_off());
  // Start the dimissing power menu animation immediately as releasing the
  // clamsehll power button if showing animation hasn't finished.
  EXPECT_FALSE(power_button_test_api_->IsMenuOpened());

  AdvanceClockToAvoidIgnoring();
  // Should turn screen on if screen is off.
  TappingPowerButtonWhenScreenIsIdleOff();
  ASSERT_TRUE(power_button_test_api_->IsMenuOpened());

  AdvanceClockToAvoidIgnoring();
  // Should not start the dismissing menu animation if showing menu animation
  // has finished.
  PressPowerButton();
  // Start the showing power menu animation immediately as pressing the
  // clamshell power button.
  EXPECT_FALSE(power_button_test_api_->PowerButtonMenuTimerIsRunning());
  EXPECT_TRUE(power_button_test_api_->IsMenuOpened());
  power_button_test_api_->SetShowMenuAnimationDone(true);
  ASSERT_TRUE(power_button_test_api_->TriggerPreShutdownTimeout());
  ReleasePowerButton();
  EXPECT_FALSE(power_manager_client()->backlights_forced_off());
  // Power button menu should keep opened if showing animation has finished.
  EXPECT_TRUE(power_button_test_api_->IsMenuOpened());

  // Tapping power button when menu is already shown should keep the screen on
  // and dismiss the power menu.
  AdvanceClockToAvoidIgnoring();
  PressPowerButton();
  ReleasePowerButton();
  EXPECT_FALSE(power_manager_client()->backlights_forced_off());
  EXPECT_FALSE(power_button_test_api_->IsMenuOpened());
}

// Tests tapping the power button of a device that has a tablet mode switch.
TEST_F(PowerButtonControllerTest, TappingPowerButtonOfTablet) {
  EnableTabletMode(true);
  // Should turn screen off if screen is on and power button menu will not be
  // shown.
  PressPowerButton();
  // Showing power menu animation hasn't started as power menu timer is running.
  EXPECT_TRUE(power_button_test_api_->PowerButtonMenuTimerIsRunning());
  EXPECT_FALSE(power_manager_client()->backlights_forced_off());
  EXPECT_FALSE(power_button_test_api_->IsMenuOpened());
  ReleasePowerButton();
  EXPECT_FALSE(power_button_test_api_->PowerButtonMenuTimerIsRunning());
  EXPECT_TRUE(power_manager_client()->backlights_forced_off());
  EXPECT_FALSE(power_button_test_api_->IsMenuOpened());

  // Should turn screen on if screen is off.
  AdvanceClockToAvoidIgnoring();
  TappingPowerButtonWhenScreenIsIdleOff();

  // Showing power menu animation should start until power menu timer is
  // timeout.
  AdvanceClockToAvoidIgnoring();
  PressPowerButton();
  power_button_test_api_->SetShowMenuAnimationDone(false);
  EXPECT_TRUE(power_button_test_api_->TriggerPowerButtonMenuTimeout());
  EXPECT_FALSE(power_button_test_api_->PowerButtonMenuTimerIsRunning());
  EXPECT_TRUE(power_button_test_api_->IsMenuOpened());
  ReleasePowerButton();
  // Showing animation will continue until show the power button menu even
  // release the power button.
  EXPECT_TRUE(power_button_test_api_->IsMenuOpened());

  // Tapping power button when menu is already shown should still turn screen
  // off and dismiss the menu.
  AdvanceClockToAvoidIgnoring();
  PressPowerButton();
  EXPECT_TRUE(power_button_test_api_->PreShutdownTimerIsRunning());
  ReleasePowerButton();
  EXPECT_FALSE(power_button_test_api_->PreShutdownTimerIsRunning());
  EXPECT_TRUE(power_manager_client()->backlights_forced_off());
  EXPECT_FALSE(power_button_test_api_->IsMenuOpened());

  // Should turn screen on if screen is off.
  AdvanceClockToAvoidIgnoring();
  TappingPowerButtonWhenScreenIsIdleOff();
}

// Tests that power button taps turn the screen off while in tablet mode but not
// in laptop mode.
TEST_F(PowerButtonControllerTest, ModeSpecificPowerButton) {
  // While the device is in tablet mode, tapping the power button should turn
  // the display off (and then back on if pressed a second time).
  EnableTabletMode(true);
  PressPowerButton();
  ReleasePowerButton();
  EXPECT_TRUE(power_manager_client()->backlights_forced_off());
  PressPowerButton();
  ReleasePowerButton();
  EXPECT_FALSE(power_manager_client()->backlights_forced_off());

  // In laptop mode, tapping the power button shouldn't turn the screen off.
  // Instead, we should start showing the power menu animation.
  EnableTabletMode(false);
  AdvanceClockToAvoidIgnoring();
  PressPowerButton();
  EXPECT_TRUE(power_button_test_api_->IsMenuOpened());
  ReleasePowerButton();
  EXPECT_FALSE(power_manager_client()->backlights_forced_off());

  // Tapping power button again in laptop mode when menu is opened should
  // dismiss the menu but keep the screen on.
  EXPECT_TRUE(power_button_test_api_->IsMenuOpened());
  AdvanceClockToAvoidIgnoring();
  PressPowerButton();
  EXPECT_FALSE(power_button_test_api_->PowerButtonMenuTimerIsRunning());
  EXPECT_TRUE(power_button_test_api_->PreShutdownTimerIsRunning());
  ReleasePowerButton();
  EXPECT_FALSE(power_manager_client()->backlights_forced_off());
  EXPECT_FALSE(power_button_test_api_->IsMenuOpened());
}

// Tests that when the kForceTabletPowerButton flag is passed (indicating that
// the device is tablet-like) tapping the power button turns the screen off
// regardless of what the tablet mode switch reports.
TEST_F(PowerButtonControllerTest, ForceTabletPowerButton) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kForceTabletPowerButton);
  ResetPowerButtonController();

  PressPowerButton();
  ReleasePowerButton();
  EXPECT_TRUE(power_manager_client()->backlights_forced_off());
  PressPowerButton();
  ReleasePowerButton();
  EXPECT_FALSE(power_manager_client()->backlights_forced_off());

  EnableTabletMode(false);
  AdvanceClockToAvoidIgnoring();
  PressPowerButton();
  ReleasePowerButton();
  EXPECT_TRUE(power_manager_client()->backlights_forced_off());
  PressPowerButton();
  ReleasePowerButton();
  EXPECT_FALSE(power_manager_client()->backlights_forced_off());
}

// Tests that release power button after menu is opened but before trigger
// shutdown will not turn screen off.
TEST_F(PowerButtonControllerTest, ReleasePowerButtonBeforeTriggerShutdown) {
  EnableTabletMode(true);
  PressPowerButton();
  EXPECT_TRUE(power_button_test_api_->PowerButtonMenuTimerIsRunning());
  EXPECT_FALSE(power_manager_client()->backlights_forced_off());
  ASSERT_TRUE(power_button_test_api_->TriggerPowerButtonMenuTimeout());
  ASSERT_TRUE(power_button_test_api_->TriggerPreShutdownTimeout());
  EXPECT_TRUE(lock_state_test_api_->shutdown_timer_is_running());
  ReleasePowerButton();
  EXPECT_TRUE(power_button_test_api_->IsMenuOpened());
  EXPECT_FALSE(lock_state_test_api_->shutdown_timer_is_running());
  EXPECT_FALSE(power_button_test_api_->PowerButtonMenuTimerIsRunning());
  EXPECT_FALSE(power_manager_client()->backlights_forced_off());
}

// Tests that tapping the power button dismisses the menu while in laptop mode.
TEST_F(PowerButtonControllerTest, HoldPowerButtonWhileMenuShownInLaptopMode) {
  // Hold the power button long enough to show the menu and start the
  // cancellable shutdown animation. The menu should remain open.
  PressPowerButton();
  ASSERT_TRUE(power_button_test_api_->IsMenuOpened());
  power_button_test_api_->SetShowMenuAnimationDone(true);
  ASSERT_TRUE(power_button_test_api_->TriggerPreShutdownTimeout());
  ASSERT_TRUE(lock_state_test_api_->shutdown_timer_is_running());
  ReleasePowerButton();
  EXPECT_FALSE(lock_state_test_api_->shutdown_timer_is_running());
  EXPECT_TRUE(power_button_test_api_->IsMenuOpened());

  // Hold the power button long enough to start the cancellable shutdown
  // animation again. The menu should remain open.
  AdvanceClockToAvoidIgnoring();
  PressPowerButton();
  ASSERT_TRUE(power_button_test_api_->TriggerPreShutdownTimeout());
  ASSERT_TRUE(lock_state_test_api_->shutdown_timer_is_running());
  ReleasePowerButton();
  EXPECT_FALSE(lock_state_test_api_->shutdown_timer_is_running());
  EXPECT_TRUE(power_button_test_api_->IsMenuOpened());

  // This time, just tap the power button (i.e. release it before the
  // cancellable shutdown animation starts). The menu should be dismissed.
  AdvanceClockToAvoidIgnoring();
  PressPowerButton();
  ReleasePowerButton();
  EXPECT_FALSE(power_button_test_api_->IsMenuOpened());
}

// Tests press lock button and power button in sequence.
TEST_F(PowerButtonControllerTest, PressAfterAnotherReleased) {
  // Tap power button after press lock button should still turn screen off.
  Initialize(ButtonType::NORMAL, LoginStatus::USER);
  EnableTabletMode(true);
  PressLockButton();
  ReleaseLockButton();
  EXPECT_FALSE(power_manager_client()->backlights_forced_off());
  PressPowerButton();
  ReleasePowerButton();
  EXPECT_TRUE(power_manager_client()->backlights_forced_off());

  // Press lock button after tap power button should still lock screen.
  PressPowerButton();
  ReleasePowerButton();
  PressLockButton();
  ReleaseLockButton();
  EXPECT_TRUE(lock_state_test_api_->is_animating_lock());
  EXPECT_TRUE(GetLockedState());
}

// Tests press lock/power button before release power/lock button.
TEST_F(PowerButtonControllerTest, PressBeforeAnotherReleased) {
  // Press lock button when power button is still being pressed will be ignored
  // and continue to turn screen off.
  Initialize(ButtonType::NORMAL, LoginStatus::USER);
  EnableTabletMode(true);
  EXPECT_FALSE(power_manager_client()->backlights_forced_off());
  PressPowerButton();
  PressLockButton();
  ReleaseLockButton();
  ReleasePowerButton();
  EXPECT_FALSE(lock_state_test_api_->is_animating_lock());
  EXPECT_FALSE(GetLockedState());
  EXPECT_TRUE(power_manager_client()->backlights_forced_off());

  // Turn the screen on.
  PressPowerButton();
  ReleasePowerButton();
  // Press power button when lock button is still being pressed. The pressing of
  // power button will be ignored and continue to lock screen.
  PressLockButton();
  PressPowerButton();
  ReleasePowerButton();
  ReleaseLockButton();
  EXPECT_TRUE(lock_state_test_api_->is_animating_lock());
  EXPECT_TRUE(GetLockedState());
  EXPECT_FALSE(power_manager_client()->backlights_forced_off());
}

// Tests tapping power button when device is suspended without backlights forced
// off.
TEST_F(PowerButtonControllerTest,
       TappingPowerButtonWhenSuspendedWithoutBacklightsForcedOff) {
  EnableTabletMode(true);
  power_manager_client()->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_OTHER);
  SendBrightnessChange(0, kUserCause);
  // There is a power button pressed here, but PowerButtonEvent is sent later.
  power_manager_client()->SendSuspendDone();
  SendBrightnessChange(kNonZeroBrightness, kUserCause);

  // Send the power button event after a short delay and check that backlights
  // are not forced off.
  tick_clock_.Advance(base::Milliseconds(500));
  PressPowerButton();
  EXPECT_TRUE(power_button_test_api_->PowerButtonMenuTimerIsRunning());
  ReleasePowerButton();
  EXPECT_FALSE(power_button_test_api_->PowerButtonMenuTimerIsRunning());
  EXPECT_FALSE(power_manager_client()->backlights_forced_off());

  // Send the power button event after a longer delay and check that backlights
  // are forced off.
  tick_clock_.Advance(base::Milliseconds(1600));
  PressPowerButton();
  EXPECT_TRUE(power_button_test_api_->PowerButtonMenuTimerIsRunning());
  ReleasePowerButton();
  SendBrightnessChange(0, kUserCause);
  EXPECT_FALSE(power_button_test_api_->PowerButtonMenuTimerIsRunning());
  EXPECT_TRUE(power_manager_client()->backlights_forced_off());
}

// Tests tapping power button when device is suspended with backlights forced
// off.
TEST_F(PowerButtonControllerTest,
       TappingPowerButtonWhenSuspendedWithBacklightsForcedOff) {
  EnableTabletMode(true);
  PressPowerButton();
  ReleasePowerButton();
  SendBrightnessChange(0, kUserCause);
  ASSERT_TRUE(power_manager_client()->backlights_forced_off());
  power_manager_client()->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_OTHER);
  // There is a power button pressed here, but PowerButtonEvent is sent later.
  // Because of backlights forced off, resuming system will not restore
  // brightness.
  power_manager_client()->SendSuspendDone();

  // Send the power button event after a short delay and check that backlights
  // are not forced off.
  tick_clock_.Advance(base::Milliseconds(500));
  PressPowerButton();
  SendBrightnessChange(kNonZeroBrightness, kUserCause);
  EXPECT_TRUE(power_button_test_api_->PowerButtonMenuTimerIsRunning());
  ReleasePowerButton();
  EXPECT_FALSE(power_button_test_api_->PowerButtonMenuTimerIsRunning());
  EXPECT_FALSE(power_manager_client()->backlights_forced_off());

  // Send the power button event after a longer delay and check that backlights
  // are forced off.
  tick_clock_.Advance(base::Milliseconds(1600));
  PressPowerButton();
  EXPECT_TRUE(power_button_test_api_->PowerButtonMenuTimerIsRunning());
  ReleasePowerButton();
  SendBrightnessChange(0, kUserCause);
  EXPECT_FALSE(power_button_test_api_->PowerButtonMenuTimerIsRunning());
  EXPECT_TRUE(power_manager_client()->backlights_forced_off());
}

// For convertible device working on tablet mode, keyboard/mouse event should
// not SetBacklightsForcedOff(false) when screen is off.
TEST_F(PowerButtonControllerTest, ConvertibleOnTabletMode) {
  EnableTabletMode(true);

  PressPowerButton();
  ReleasePowerButton();
  SendBrightnessChange(0, kUserCause);
  ASSERT_TRUE(power_manager_client()->backlights_forced_off());
  PressKey(ui::VKEY_L);
  EXPECT_TRUE(power_manager_client()->backlights_forced_off());

  GenerateMouseMoveEvent();
  EXPECT_TRUE(power_manager_client()->backlights_forced_off());
}

// Tests that a single set of power button pressed-and-released operation should
// cause only one SetBacklightsForcedOff call.
TEST_F(PowerButtonControllerTest, IgnorePowerOnKeyEvent) {
  ui::KeyEvent power_key_pressed(ui::EventType::kKeyPressed, ui::VKEY_POWER,
                                 ui::EF_NONE);
  ui::KeyEvent power_key_released(ui::EventType::kKeyReleased, ui::VKEY_POWER,
                                  ui::EF_NONE);

  // There are two |power_key_pressed| events and |power_key_released| events
  // generated for each pressing and releasing, and multiple repeating pressed
  // events depending on holding.
  ASSERT_EQ(0, power_manager_client()->num_set_backlights_forced_off_calls());
  EnableTabletMode(true);
  power_button_test_api_->SendKeyEvent(&power_key_pressed);
  power_button_test_api_->SendKeyEvent(&power_key_pressed);
  PressPowerButton();
  power_button_test_api_->SendKeyEvent(&power_key_pressed);
  power_button_test_api_->SendKeyEvent(&power_key_pressed);
  power_button_test_api_->SendKeyEvent(&power_key_pressed);
  ReleasePowerButton();
  power_button_test_api_->SendKeyEvent(&power_key_released);
  power_button_test_api_->SendKeyEvent(&power_key_released);
  EXPECT_EQ(1, power_manager_client()->num_set_backlights_forced_off_calls());
}

// Tests that when the power button is pressed/released in tablet mode,
// requesting/stopping backlights forced off should update the global
// touchscreen enabled status.
TEST_F(PowerButtonControllerTest, DisableTouchscreenWhileForcedOff) {
  // Tests tablet power button.
  EnableTabletMode(true);
  ASSERT_TRUE(GetGlobalTouchscreenEnabled());
  PressPowerButton();
  ReleasePowerButton();
  SendBrightnessChange(0, kUserCause);
  EXPECT_FALSE(GetGlobalTouchscreenEnabled());

  PressPowerButton();
  SendBrightnessChange(kNonZeroBrightness, kUserCause);
  ReleasePowerButton();
  EXPECT_TRUE(GetGlobalTouchscreenEnabled());
}

// When the screen is turned off automatically, the touchscreen should also be
// disabled.
TEST_F(PowerButtonControllerTest, DisableTouchscreenForInactivity) {
  ASSERT_TRUE(GetGlobalTouchscreenEnabled());

  // Turn screen off for automated change (e.g. user is inactive).
  SendBrightnessChange(0, kOtherCause);
  EXPECT_FALSE(GetGlobalTouchscreenEnabled());
  SendBrightnessChange(kNonZeroBrightness, kUserCause);
  EXPECT_TRUE(GetGlobalTouchscreenEnabled());

  // After decreasing the brightness to zero for a user request, the touchscreen
  // should remain enabled.
  SendBrightnessChange(0, kUserCause);
  EXPECT_TRUE(GetGlobalTouchscreenEnabled());
}

// When user switches convertible device between tablet mode and laptop mode,
// power button may be pressed and held, which may cause unwanted unclean
// shutdown.
TEST_F(PowerButtonControllerTest, LeaveTabletModeWhilePressingPowerButton) {
  EnableTabletMode(true);
  PressPowerButton();
  EXPECT_TRUE(power_button_test_api_->PowerButtonMenuTimerIsRunning());
  EnableTabletMode(false);
  EXPECT_FALSE(power_button_test_api_->PowerButtonMenuTimerIsRunning());
  tick_clock_.Advance(base::Milliseconds(1500));
  ReleasePowerButton();
  EXPECT_FALSE(power_manager_client()->backlights_forced_off());
  EXPECT_FALSE(power_button_test_api_->IsMenuOpened());
}

// Tests that repeated power button releases are ignored (crbug.com/675291).
TEST_F(PowerButtonControllerTest, IgnoreRepeatedPowerButtonReleases) {
  // Set backlights forced off for starting point.
  EnableTabletMode(true);
  PressPowerButton();
  ReleasePowerButton();
  SendBrightnessChange(0, kUserCause);
  ASSERT_TRUE(power_manager_client()->backlights_forced_off());

  // Test that a pressing-releasing operation after a short duration, backlights
  // forced off is stopped since we don't drop request for power button pressed.
  tick_clock_.Advance(base::Milliseconds(200));
  PressPowerButton();
  SendBrightnessChange(kNonZeroBrightness, kUserCause);
  ReleasePowerButton();
  EXPECT_FALSE(power_manager_client()->backlights_forced_off());

  // Test that after another short duration, backlights will not be forced off
  // since this immediately following forcing off request needs to be dropped.
  tick_clock_.Advance(base::Milliseconds(200));
  PressPowerButton();
  ReleasePowerButton();
  EXPECT_FALSE(power_manager_client()->backlights_forced_off());

  // Test that after another long duration, backlights should be forced off.
  tick_clock_.Advance(base::Milliseconds(800));
  PressPowerButton();
  ReleasePowerButton();
  SendBrightnessChange(0, kUserCause);
  EXPECT_TRUE(power_manager_client()->backlights_forced_off());
}

// Tests that repeated power button releases of clamshell should cancel the
// ongoing showing menu animation.
TEST_F(PowerButtonControllerTest,
       ClamshellRepeatedPowerButtonReleasesCancelledAnimation) {
  InitPowerButtonControllerMembers(PowerManagerClient::TabletMode::UNSUPPORTED);
  EnableTabletMode(false);

  // Enable animations so that we can make sure that they occur.
  ui::ScopedAnimationDurationScaleMode regular_animations(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  PressPowerButton();
  ReleasePowerButton();
  EXPECT_FALSE(power_button_test_api_->IsMenuOpened());

  tick_clock_.Advance(base::Milliseconds(200));
  PressPowerButton();
  ReleasePowerButton();
  // Showing menu animation should be cancelled and menu is not shown.
  EXPECT_FALSE(power_button_test_api_->IsMenuOpened());
  EXPECT_FALSE(power_button_test_api_->PreShutdownTimerIsRunning());
}

// Tests that lid closed events stop forcing off backlights.
TEST_F(PowerButtonControllerTest, LidEventsStopForcingOff) {
  // Pressing/releasing power button to set backlights forced off.
  EnableTabletMode(true);
  PressPowerButton();
  ReleasePowerButton();
  ASSERT_TRUE(power_manager_client()->backlights_forced_off());

  // A lid closed event is received, we should stop forcing off backlights.
  power_manager_client()->SetLidState(PowerManagerClient::LidState::CLOSED,
                                      tick_clock_.NowTicks());
  EXPECT_FALSE(power_manager_client()->backlights_forced_off());
}

// Tests that tablet mode events from powerd stop forcing off backlights.
TEST_F(PowerButtonControllerTest, TabletModeEventsStopForcingOff) {
  EnableTabletMode(true);
  PressPowerButton();
  ReleasePowerButton();
  ASSERT_TRUE(power_manager_client()->backlights_forced_off());
  power_manager_client()->SetTabletMode(PowerManagerClient::TabletMode::OFF,
                                        tick_clock_.NowTicks());
  EXPECT_FALSE(power_manager_client()->backlights_forced_off());

  AdvanceClockToAvoidIgnoring();
  PressPowerButton();
  ReleasePowerButton();
  ASSERT_TRUE(power_manager_client()->backlights_forced_off());
  power_manager_client()->SetTabletMode(PowerManagerClient::TabletMode::ON,
                                        tick_clock_.NowTicks());
  EXPECT_FALSE(power_manager_client()->backlights_forced_off());
}

// Tests that with system reboot, the global touchscreen enabled status should
// be synced with new backlights forced off state from powerd.
TEST_F(PowerButtonControllerTest, SyncTouchscreenEnabled) {
  Shell::Get()->touch_devices_controller()->SetTouchscreenEnabled(
      false, TouchDeviceEnabledSource::GLOBAL);
  ASSERT_FALSE(GetGlobalTouchscreenEnabled());

  // Simulate system reboot by resetting backlights forced off state in powerd
  // and PowerButtonController.
  power_manager_client()->SetBacklightsForcedOff(false);
  ResetPowerButtonController();
  SetTabletModeSwitchState(PowerManagerClient::TabletMode::ON);

  // Run the event loop for PowerButtonDisplayController to get backlight state
  // and check that the global touchscreen status is correct.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(power_manager_client()->backlights_forced_off());
  EXPECT_TRUE(GetGlobalTouchscreenEnabled());
}

// Tests that when backlights get forced off due to tablet power button, media
// sessions should be suspended.
TEST_F(PowerButtonControllerTest, SuspendMediaSessions) {
  TestMediaClient client;
  Shell::Get()->media_controller()->SetClient(&client);
  ASSERT_FALSE(client.media_sessions_suspended());

  EnableTabletMode(true);
  PressPowerButton();
  ReleasePowerButton();
  // Run the event loop for PowerButtonDisplayController to get backlight state.
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(power_manager_client()->backlights_forced_off());
  EXPECT_TRUE(client.media_sessions_suspended());
}

// Tests that when system is suspended with backlights forced off, and then
// system resumes due to power button pressed without power button event fired
// (crbug.com/735291), that we stop forcing off backlights.
TEST_F(PowerButtonControllerTest, SuspendDoneStopsForcingOff) {
  EnableTabletMode(true);
  PressPowerButton();
  ReleasePowerButton();
  SendBrightnessChange(0, kUserCause);
  ASSERT_TRUE(power_manager_client()->backlights_forced_off());

  // Simulate an edge case that system resumes because of tablet power button
  // pressed, but power button event is not delivered.
  power_manager_client()->SendSuspendDone();

  EXPECT_FALSE(power_manager_client()->backlights_forced_off());
}

// Tests that during the interval that the display is turning on, tablet power
// button should not set display off (crbug.com/735225).
TEST_F(PowerButtonControllerTest, IgnoreForcingOffWhenDisplayIsTurningOn) {
  EnableTabletMode(true);
  PressPowerButton();
  ReleasePowerButton();
  SendBrightnessChange(0, kUserCause);
  ASSERT_TRUE(power_manager_client()->backlights_forced_off());

  // Simiulate the backlight no longer being forced off due to a key event
  // (which we need to briefly leave tablet mode to receive). Chrome will
  // receive a brightness changed signal, but the display may still be off.
  EnableTabletMode(false);
  PressKey(ui::VKEY_L);
  SendBrightnessChange(kNonZeroBrightness, kUserCause);
  EXPECT_FALSE(power_manager_client()->backlights_forced_off());

  // Since display could still be off, ignore additional button presses.
  tick_clock_.Advance(PowerButtonController::kScreenStateChangeDelay -
                      base::Milliseconds(1));
  EnableTabletMode(true);
  PressPowerButton();
  ReleasePowerButton();
  EXPECT_FALSE(power_manager_client()->backlights_forced_off());

  // After waiting long enough, we should be able to force the display off.
  AdvanceClockToAvoidIgnoring();
  PressPowerButton();
  ReleasePowerButton();
  SendBrightnessChange(0, kUserCause);
  EXPECT_TRUE(power_manager_client()->backlights_forced_off());
}

// Tests that a11y alert is sent on tablet power button induced screen state
// change.
TEST_F(PowerButtonControllerTest, A11yAlert) {
  TestAccessibilityControllerClient a11y_client;

  EnableTabletMode(true);
  PressPowerButton();
  ReleasePowerButton();
  SendBrightnessChange(0, kUserCause);
  EXPECT_EQ(AccessibilityAlert::SCREEN_OFF, a11y_client.last_a11y_alert());

  PressPowerButton();
  SendBrightnessChange(kNonZeroBrightness, kUserCause);
  EXPECT_EQ(AccessibilityAlert::SCREEN_ON, a11y_client.last_a11y_alert());
  ReleasePowerButton();
}

// Tests that tap outside of the menu bounds should dismiss the menu.
TEST_F(PowerButtonControllerTest, TapToDismissMenu) {
  OpenPowerButtonMenu();
  TapToDismissPowerButtonMenu();
}

// Test that mouse click outside of the menu bounds should dismiss the menu.
TEST_F(PowerButtonControllerTest, MouseClickToDismissMenu) {
  OpenPowerButtonMenu();
  gfx::Rect menu_bounds = power_button_test_api_->GetMenuBoundsInScreen();
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(gfx::Point(menu_bounds.x() - 5, menu_bounds.y() - 5));
  generator->ClickLeftButton();
  generator->ReleaseLeftButton();
  EXPECT_FALSE(power_button_test_api_->IsMenuOpened());
}

// Tests the menu items according to the login and screen locked status.
TEST_F(PowerButtonControllerTest, MenuItemsToLoginAndLockedStatus) {
  // Should have feedback but not sign out and lock screen items if there is no
  // user signed in.
  ClearLogin();
  Shell::Get()->UpdateAfterLoginStatusChange(LoginStatus::NOT_LOGGED_IN);
  OpenPowerButtonMenu();
  EXPECT_FALSE(power_button_test_api_->MenuHasSignOutItem());
  EXPECT_FALSE(power_button_test_api_->MenuHasLockScreenItem());
  EXPECT_TRUE(power_button_test_api_->MenuHasFeedbackItem());
  TapToDismissPowerButtonMenu();

  // Should have sign out and feedback items if in guest mode (or, generally,
  // if screen locking is disabled).
  ClearLogin();
  Initialize(ButtonType::NORMAL, LoginStatus::GUEST);
  OpenPowerButtonMenu();
  EXPECT_FALSE(GetLockedState());
  EXPECT_TRUE(power_button_test_api_->MenuHasSignOutItem());
  EXPECT_FALSE(power_button_test_api_->MenuHasLockScreenItem());
  EXPECT_TRUE(power_button_test_api_->MenuHasFeedbackItem());
  TapToDismissPowerButtonMenu();

  // Should have sign out, lock screen and feedback items if user is logged in
  // and screen is unlocked.
  ClearLogin();
  CreateUserSessions(1);
  OpenPowerButtonMenu();
  EXPECT_FALSE(GetLockedState());
  EXPECT_TRUE(power_button_test_api_->MenuHasSignOutItem());
  EXPECT_TRUE(power_button_test_api_->MenuHasLockScreenItem());
  EXPECT_TRUE(power_button_test_api_->MenuHasFeedbackItem());
  TapToDismissPowerButtonMenu();

  // Should have sign out but not lock screen and feedback items if user is
  // logged in but screen is locked.
  LockScreen();
  EXPECT_TRUE(GetLockedState());
  OpenPowerButtonMenu();
  EXPECT_TRUE(power_button_test_api_->MenuHasSignOutItem());
  EXPECT_FALSE(power_button_test_api_->MenuHasLockScreenItem());
  EXPECT_FALSE(power_button_test_api_->MenuHasFeedbackItem());
}

// Tests long-pressing the power button when the menu is open.
TEST_F(PowerButtonControllerTest, LongPressButtonWhenMenuIsOpened) {
  OpenPowerButtonMenu();
  AdvanceClockToAvoidIgnoring();

  // Long pressing the power button when menu is opened should not dismiss the
  // menu but trigger the pre-shutdown animation instead. Menu should stay
  // opened if releasing the button can cancel the animation.
  PressPowerButton();
  EXPECT_FALSE(power_button_test_api_->PowerButtonMenuTimerIsRunning());
  ASSERT_TRUE(power_button_test_api_->TriggerPreShutdownTimeout());
  EXPECT_TRUE(lock_state_test_api_->shutdown_timer_is_running());
  ReleasePowerButton();
  EXPECT_FALSE(lock_state_test_api_->shutdown_timer_is_running());
  EXPECT_TRUE(power_button_test_api_->IsMenuOpened());

  // Change focus to 'sign out'
  PressKey(ui::VKEY_TAB);
  EXPECT_TRUE(power_button_test_api_->GetPowerButtonMenuView()
                  ->sign_out_item_for_test()
                  ->HasFocus());

  // Long press when menu is opened with focus on 'sign out' item will change
  // the focus to 'power off' after starting the pre-shutdown animation.
  PressPowerButton();
  ASSERT_TRUE(power_button_test_api_->TriggerPreShutdownTimeout());
  EXPECT_TRUE(lock_state_test_api_->shutdown_timer_is_running());
  EXPECT_TRUE(power_button_test_api_->GetPowerButtonMenuView()
                  ->power_off_item_for_test()
                  ->HasFocus());
  ReleasePowerButton();
}

// Tests that switches between laptop mode and tablet mode should dismiss the
// opened menu.
TEST_F(PowerButtonControllerTest, EnterOrLeaveTabletModeDismissMenu) {
  OpenPowerButtonMenu();
  EnableTabletMode(true);
  EXPECT_FALSE(power_button_test_api_->IsMenuOpened());

  OpenPowerButtonMenu();
  EnableTabletMode(false);
  EXPECT_FALSE(power_button_test_api_->IsMenuOpened());
}

// Tests that screen changes to idle off will dismiss the opened menu.
TEST_F(PowerButtonControllerTest, DismissMenuWhenScreenIsIdleOff) {
  OpenPowerButtonMenu();
  // Mock screen idle off.
  SendBrightnessChange(0, kUserCause);
  EXPECT_FALSE(power_button_test_api_->IsMenuOpened());
}

// Tests that tapping the power button should dimiss the opened menu.
TEST_F(PowerButtonControllerTest, TappingPowerButtonWhenMenuIsOpened) {
  EnableTabletMode(true);
  OpenPowerButtonMenu();

  // Tapping the power button when menu is opened will dismiss the menu.
  AdvanceClockToAvoidIgnoring();
  PressPowerButton();
  ReleasePowerButton();
  SendBrightnessChange(0, kUserCause);
  EXPECT_TRUE(power_manager_client()->backlights_forced_off());
  EXPECT_FALSE(power_button_test_api_->IsMenuOpened());

  // Long press the power button when backlights are off will show the menu.
  PressPowerButton();
  SendBrightnessChange(kNonZeroBrightness, kUserCause);
  EXPECT_TRUE(power_button_test_api_->TriggerPowerButtonMenuTimeout());
  ReleasePowerButton();
  EXPECT_TRUE(power_button_test_api_->IsMenuOpened());
  // Tapping the power button will dismiss the menu.
  AdvanceClockToAvoidIgnoring();
  PressPowerButton();
  ReleasePowerButton();
  SendBrightnessChange(0, kUserCause);
  EXPECT_TRUE(power_manager_client()->backlights_forced_off());
  EXPECT_FALSE(power_button_test_api_->IsMenuOpened());
}

// Tests that suspend will dismiss the opened menu.
TEST_F(PowerButtonControllerTest, SuspendWithMenuOn) {
  OpenPowerButtonMenu();
  power_manager_client()->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_OTHER);
  EXPECT_FALSE(power_button_test_api_->IsMenuOpened());
  power_manager_client()->SendSuspendDone();
  EXPECT_FALSE(power_button_test_api_->IsMenuOpened());
}

// Tests the formerly-active window state in showing power menu.
TEST_F(PowerButtonControllerTest, FormerlyActiveWindowInShowingMenu) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  ASSERT_TRUE(widget->IsActive());

  OpenPowerButtonMenu();
  // The active window becomes inactive after menu is shown but it is still
  // painted as active to avoid frame color change.
  EXPECT_FALSE(widget->IsActive());
  EXPECT_TRUE(widget->ShouldPaintAsActive());
  EXPECT_TRUE(widget->non_client_view()->frame_view()->ShouldPaintAsActive());
  EXPECT_TRUE(
      wm::IsActiveWindow(power_button_test_api_->GetPowerButtonMenuView()
                             ->GetWidget()
                             ->GetNativeWindow()));
  // Should reset the previous painting as active setting of the active window
  // if dismissing the menu.
  TapToDismissPowerButtonMenu();

  // Focus may fall to the widget if it's the only remaining widget on the
  // screen. Deactivate it to verify that it's no longer being forced to render
  // as active.
  widget->Deactivate();
  EXPECT_FALSE(widget->ShouldPaintAsActive());

  // A widget which is not the active widget is not affected by opening the
  // power button menu.
  OpenPowerButtonMenu();
  EXPECT_FALSE(widget->ShouldPaintAsActive());
  TapToDismissPowerButtonMenu();

  // If focus didn't fall to the widget after the menu was closed, focus it.
  widget->Activate();

  // Dismiss menu should work well after the active window is closed between
  // showing and dismissing menu.
  OpenPowerButtonMenu();
  widget->Close();
  TapToDismissPowerButtonMenu();
}

// Tests that cursor is hidden after show the menu and should reappear if mouse
// moves.
TEST_F(PowerButtonControllerTest, HideCursorAfterShowMenu) {
  // Cursor is hidden after show the menu.
  ::wm::CursorManager* cursor_manager = Shell::Get()->cursor_manager();
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
  PressPowerButton();
  ReleasePowerButton();
  EXPECT_TRUE(power_button_test_api_->IsMenuOpened());
  EXPECT_FALSE(cursor_manager->IsCursorVisible());

  // Cursor reappears if mouse moves.
  GenerateMouseMoveEvent();
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
}

// Tests that press VKEY_ESCAPE should dismiss the opened menu.
TEST_F(PowerButtonControllerTest, ESCDismissMenu) {
  OpenPowerButtonMenu();

  PressKey(ui::VKEY_VOLUME_DOWN);
  EXPECT_TRUE(power_button_test_api_->IsMenuOpened());

  PressKey(ui::VKEY_BRIGHTNESS_UP);
  EXPECT_TRUE(power_button_test_api_->IsMenuOpened());

  PressKey(ui::VKEY_BROWSER_SEARCH);
  EXPECT_TRUE(power_button_test_api_->IsMenuOpened());

  PressKey(ui::VKEY_ESCAPE);
  EXPECT_FALSE(power_button_test_api_->IsMenuOpened());
}

// Tests the navigation of the menu.
TEST_F(PowerButtonControllerTest, MenuNavigation) {
  ClearLogin();
  Shell::Get()->UpdateAfterLoginStatusChange(LoginStatus::NOT_LOGGED_IN);
  OpenPowerButtonMenu();
  ASSERT_TRUE(power_button_test_api_->MenuHasFeedbackItem());
  auto* menu_view = power_button_test_api_->GetPowerButtonMenuView();
  PressKey(ui::VKEY_TAB);
  EXPECT_TRUE(menu_view->power_off_item_for_test()->HasFocus());
  PressKey(ui::VKEY_TAB);
  EXPECT_TRUE(menu_view->feedback_item_for_test()->HasFocus());
  TapToDismissPowerButtonMenu();

  ClearLogin();
  CreateUserSessions(1);
  OpenPowerButtonMenu();
  ASSERT_TRUE(power_button_test_api_->MenuHasSignOutItem());
  ASSERT_TRUE(power_button_test_api_->MenuHasLockScreenItem());
  ASSERT_TRUE(power_button_test_api_->MenuHasFeedbackItem());
  menu_view = power_button_test_api_->GetPowerButtonMenuView();
  PressKey(ui::VKEY_TAB);
  EXPECT_TRUE(menu_view->power_off_item_for_test()->HasFocus());

  PressKey(ui::VKEY_RIGHT);
  EXPECT_TRUE(menu_view->sign_out_item_for_test()->HasFocus());

  PressKey(ui::VKEY_DOWN);
  EXPECT_TRUE(menu_view->lock_screen_item_for_test()->HasFocus());

  PressKey(ui::VKEY_TAB);
  EXPECT_TRUE(menu_view->feedback_item_for_test()->HasFocus());

  PressKey(ui::VKEY_TAB);
  EXPECT_TRUE(menu_view->power_off_item_for_test()->HasFocus());

  PressKey(ui::VKEY_UP);
  EXPECT_TRUE(menu_view->feedback_item_for_test()->HasFocus());

  PressKey(ui::VKEY_UP);
  EXPECT_TRUE(menu_view->lock_screen_item_for_test()->HasFocus());

  PressKey(ui::VKEY_LEFT);
  EXPECT_TRUE(menu_view->sign_out_item_for_test()->HasFocus());

  PressKey(ui::VKEY_UP);
  EXPECT_TRUE(menu_view->power_off_item_for_test()->HasFocus());
}

// Tests that the partially shown menu will be dismissed by power button up in
// tablet mode, and screen should not be turned off at the same time.
TEST_F(PowerButtonControllerTest, PartiallyShownMenuInTabletMode) {
  EnableTabletMode(true);

  // Enable animations so that we can make sure that they occur.
  ui::ScopedAnimationDurationScaleMode regular_animations(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  PressPowerButton();
  EXPECT_TRUE(power_button_test_api_->PowerButtonMenuTimerIsRunning());
  EXPECT_TRUE(power_button_test_api_->TriggerPowerButtonMenuTimeout());
  EXPECT_FALSE(power_button_test_api_->PowerButtonMenuTimerIsRunning());
  // Power menu is in the partially shown state.
  EXPECT_TRUE(power_button_test_api_->IsMenuOpened());
  EXPECT_FALSE(power_button_test_api_->ShowMenuAnimationDone());
  ReleasePowerButton();
  EXPECT_FALSE(power_button_test_api_->ShowMenuAnimationDone());
  // The partially shown menu should be dismissed by power button up.
  EXPECT_FALSE(power_button_test_api_->IsMenuOpened());
  // Screen should not be turned off with power button released.
  EXPECT_FALSE(power_manager_client()->backlights_forced_off());
}

TEST_F(PowerButtonControllerTest, PowerMenuItemsInTabletKiosk) {
  ClearLogin();
  SimulateKioskMode(user_manager::UserType::kWebKioskApp);
  SetCanLockScreen(false);

  EnableTabletMode(true);

  OpenPowerButtonMenu();

  EXPECT_TRUE(power_button_test_api_->MenuHasPowerOffItem());
  EXPECT_TRUE(power_button_test_api_->MenuHasSignOutItem());
  EXPECT_FALSE(power_button_test_api_->MenuHasLockScreenItem());
  EXPECT_FALSE(power_button_test_api_->MenuHasCaptureModeItem());
  EXPECT_FALSE(power_button_test_api_->MenuHasFeedbackItem());
}

TEST_F(PowerButtonControllerTest, PowerMenuItemsInLaptopKiosk) {
  ClearLogin();
  SimulateKioskMode(user_manager::UserType::kWebKioskApp);
  SetCanLockScreen(false);

  EnableTabletMode(false);

  OpenPowerButtonMenu();

  EXPECT_TRUE(power_button_test_api_->MenuHasPowerOffItem());
  EXPECT_TRUE(power_button_test_api_->MenuHasSignOutItem());
  EXPECT_FALSE(power_button_test_api_->MenuHasLockScreenItem());
  EXPECT_FALSE(power_button_test_api_->MenuHasCaptureModeItem());
  EXPECT_FALSE(power_button_test_api_->MenuHasFeedbackItem());
}

class PowerButtonControllerWithPositionTest
    : public PowerButtonControllerTest,
      public testing::WithParamInterface<PowerButtonPosition> {
 public:
  PowerButtonControllerWithPositionTest() : power_button_position_(GetParam()) {
    base::Value::Dict position_info;
    switch (power_button_position_) {
      case PowerButtonPosition::LEFT:
        position_info.Set(PowerButtonController::kEdgeField,
                          PowerButtonController::kLeftEdge);
        break;
      case PowerButtonPosition::RIGHT:
        position_info.Set(PowerButtonController::kEdgeField,
                          PowerButtonController::kRightEdge);
        break;
      case PowerButtonPosition::TOP:
        position_info.Set(PowerButtonController::kEdgeField,
                          PowerButtonController::kTopEdge);
        break;
      case PowerButtonPosition::BOTTOM:
        position_info.Set(PowerButtonController::kEdgeField,
                          PowerButtonController::kBottomEdge);
        break;
      default:
        return;
    }
    position_info.Set(PowerButtonController::kPositionField,
                      kPowerButtonPercentage);

    std::string json_position_info;
    base::JSONWriter::Write(position_info, &json_position_info);
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kAshPowerButtonPosition, json_position_info);
  }

  PowerButtonControllerWithPositionTest(
      const PowerButtonControllerWithPositionTest&) = delete;
  PowerButtonControllerWithPositionTest& operator=(
      const PowerButtonControllerWithPositionTest&) = delete;

  bool IsLeftOrRightPosition() const {
    return power_button_position_ == PowerButtonPosition::LEFT ||
           power_button_position_ == PowerButtonPosition::RIGHT;
  }

  // Returns true if it is in tablet mode.
  bool IsTabletMode() const {
    return display::Screen::GetScreen()->InTabletMode();
  }

  // Returns true if the menu is at the center of the display.
  bool IsMenuCentered() const {
    return power_button_test_api_->GetMenuBoundsInScreen().CenterPoint() ==
           display::Screen::GetScreen()
               ->GetPrimaryDisplay()
               .bounds()
               .CenterPoint();
  }

  PowerButtonPosition power_button_position() const {
    return power_button_position_;
  }

 private:
  PowerButtonPosition power_button_position_;
};

// TODO(crbug.com/40101364).
TEST_P(PowerButtonControllerWithPositionTest,
       DISABLED_MenuNextToPowerButtonInTabletMode) {
  std::string display = base::NumberToString(kDisplayWidth) + "x" +
                        base::NumberToString(kDisplayHeight);
  UpdateDisplay(display);
  display::test::ScopedSetInternalDisplayId set_internal(
      display_manager(), GetPrimaryDisplay().id());

  ScreenOrientationControllerTestApi test_api(
      Shell::Get()->screen_orientation_controller());
  // Set the screen orientation to LANDSCAPE_PRIMARY.
  test_api.SetDisplayRotation(display::Display::ROTATE_0,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            chromeos::OrientationType::kLandscapePrimary);

  // Menu is set at the center of the display if it is not in tablet mode.
  OpenPowerButtonMenu();
  ASSERT_FALSE(IsTabletMode());
  EXPECT_TRUE(IsMenuCentered());
  TapToDismissPowerButtonMenu();

  int animation_transform = kPowerButtonMenuTransformDistanceDp;
  EnableTabletMode(true);
  EXPECT_TRUE(IsTabletMode());
  OpenPowerButtonMenu();
  EXPECT_FALSE(IsMenuCentered());
  if (power_button_position() == PowerButtonPosition::LEFT) {
    EXPECT_EQ(animation_transform,
              power_button_test_api_->GetMenuBoundsInScreen().x());
  } else if (power_button_position() == PowerButtonPosition::RIGHT) {
    EXPECT_EQ(animation_transform,
              kDisplayWidth -
                  power_button_test_api_->GetMenuBoundsInScreen().right());
  } else if (power_button_position() == PowerButtonPosition::TOP) {
    EXPECT_EQ(animation_transform,
              power_button_test_api_->GetMenuBoundsInScreen().y());
  } else if (power_button_position() == PowerButtonPosition::BOTTOM) {
    EXPECT_EQ(animation_transform,
              kDisplayHeight -
                  power_button_test_api_->GetMenuBoundsInScreen().bottom());
  }

  // Rotate the screen by 270 degree.
  test_api.SetDisplayRotation(display::Display::ROTATE_270,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            chromeos::OrientationType::kPortraitPrimary);
  EXPECT_FALSE(IsMenuCentered());
  if (power_button_position() == PowerButtonPosition::LEFT) {
    EXPECT_EQ(animation_transform,
              power_button_test_api_->GetMenuBoundsInScreen().y());
  } else if (power_button_position() == PowerButtonPosition::RIGHT) {
    EXPECT_EQ(animation_transform,
              kDisplayWidth -
                  power_button_test_api_->GetMenuBoundsInScreen().bottom());
  } else if (power_button_position() == PowerButtonPosition::TOP) {
    EXPECT_EQ(animation_transform,
              kDisplayHeight -
                  power_button_test_api_->GetMenuBoundsInScreen().right());
  } else if (power_button_position() == PowerButtonPosition::BOTTOM) {
    EXPECT_EQ(animation_transform,
              power_button_test_api_->GetMenuBoundsInScreen().x());
  }

  // Rotate the screen by 180 degree.
  test_api.SetDisplayRotation(display::Display::ROTATE_180,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            chromeos::OrientationType::kLandscapeSecondary);
  EXPECT_FALSE(IsMenuCentered());
  if (power_button_position() == PowerButtonPosition::LEFT) {
    EXPECT_EQ(animation_transform,
              kDisplayWidth -
                  power_button_test_api_->GetMenuBoundsInScreen().right());
  } else if (power_button_position() == PowerButtonPosition::RIGHT) {
    EXPECT_EQ(animation_transform,
              power_button_test_api_->GetMenuBoundsInScreen().x());
  } else if (power_button_position() == PowerButtonPosition::TOP) {
    EXPECT_EQ(animation_transform,
              kDisplayHeight -
                  power_button_test_api_->GetMenuBoundsInScreen().bottom());
  } else if (power_button_position() == PowerButtonPosition::BOTTOM) {
    EXPECT_EQ(animation_transform,
              power_button_test_api_->GetMenuBoundsInScreen().y());
  }

  // Rotate the screen by 90 degree.
  test_api.SetDisplayRotation(display::Display::ROTATE_90,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            chromeos::OrientationType::kPortraitSecondary);
  EXPECT_FALSE(IsMenuCentered());
  if (power_button_position() == PowerButtonPosition::LEFT) {
    EXPECT_EQ(animation_transform,
              kDisplayWidth -
                  power_button_test_api_->GetMenuBoundsInScreen().bottom());
  } else if (power_button_position() == PowerButtonPosition::RIGHT) {
    EXPECT_EQ(animation_transform,
              power_button_test_api_->GetMenuBoundsInScreen().y());
  } else if (power_button_position() == PowerButtonPosition::TOP) {
    EXPECT_EQ(animation_transform,
              power_button_test_api_->GetMenuBoundsInScreen().x());
  } else if (power_button_position() == PowerButtonPosition::BOTTOM) {
    EXPECT_EQ(animation_transform,
              kDisplayHeight -
                  power_button_test_api_->GetMenuBoundsInScreen().right());
  }
}

// Tests that the menu is always shown at the percentage of position when
// display has different scale factors.
TEST_P(PowerButtonControllerWithPositionTest, MenuShownAtPercentageOfPosition) {
  const int scale_factor = 2;
  std::string display = "8000x2400*" + base::NumberToString(scale_factor);
  UpdateDisplay(display);
  int64_t primary_id = GetPrimaryDisplay().id();
  display::test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                         primary_id);
  ASSERT_EQ(scale_factor, GetPrimaryDisplay().device_scale_factor());

  EnableTabletMode(true);
  OpenPowerButtonMenu();
  EXPECT_FALSE(IsMenuCentered());
  gfx::Point menu_center_point =
      power_button_test_api_->GetMenuBoundsInScreen().CenterPoint();
  gfx::Rect display_bounds = GetPrimaryDisplay().bounds();
  int original_width = display_bounds.width();
  int original_height = display_bounds.height();
  if (IsLeftOrRightPosition()) {
    EXPECT_EQ(menu_center_point.y(), static_cast<int>(display_bounds.height() *
                                                      kPowerButtonPercentage));
  } else {
    EXPECT_EQ(menu_center_point.x(), static_cast<int>(display_bounds.width() *
                                                      kPowerButtonPercentage));
  }
  TapToDismissPowerButtonMenu();

  display_manager()->UpdateZoomFactor(primary_id, 1.f / scale_factor);
  ASSERT_EQ(1.0f, GetPrimaryDisplay().device_scale_factor());
  display_bounds = GetPrimaryDisplay().bounds();
  int scale_up_width = display_bounds.width();
  int scale_up_height = display_bounds.height();
  EXPECT_EQ(scale_up_width, original_width * scale_factor);
  EXPECT_EQ(scale_up_height, original_height * scale_factor);
  OpenPowerButtonMenu();
  menu_center_point =
      power_button_test_api_->GetMenuBoundsInScreen().CenterPoint();
  // Menu is still at the kPowerButtonPercentage position after scale up screen.
  if (IsLeftOrRightPosition()) {
    EXPECT_EQ(menu_center_point.y(), static_cast<int>(display_bounds.height() *
                                                      kPowerButtonPercentage));
  } else {
    EXPECT_EQ(menu_center_point.x(), static_cast<int>(display_bounds.width() *
                                                      kPowerButtonPercentage));
  }
}

TEST_P(PowerButtonControllerWithPositionTest, AdjustMenuShownForDisplaySize) {
  OpenPowerButtonMenu();
  gfx::Rect menu_bounds = power_button_test_api_->GetMenuBoundsInScreen();
  TapToDismissPowerButtonMenu();

  // (1 - kPowerButtonPercentage) * display_height < 0.5 * menu_height makes
  // sure menu will be cut off by display when button is on LEFT/RIGHT, and (1 -
  // kPowerButtonPercentage) * display_width < 0.5 * menu_width makes sure menu
  // will be cut off by display when button is on TOP/BOTTOM.
  int display_width =
      0.5 / (1.0f - kPowerButtonPercentage) * menu_bounds.width() - 5;
  int display_height =
      0.5 / (1.0f - kPowerButtonPercentage) * menu_bounds.height() - 5;
  std::string display = base::NumberToString(display_width) + "x" +
                        base::NumberToString(display_height);
  UpdateDisplay(display);
  display::test::ScopedSetInternalDisplayId set_internal(
      display_manager(), GetPrimaryDisplay().id());

  ScreenOrientationControllerTestApi test_api(
      Shell::Get()->screen_orientation_controller());
  // Set the screen orientation to LANDSCAPE_PRIMARY.
  test_api.SetDisplayRotation(display::Display::ROTATE_0,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            chromeos::OrientationType::kLandscapePrimary);
  EnableTabletMode(true);
  OpenPowerButtonMenu();
  // Menu's bounds is always inside the display.
  EXPECT_TRUE(GetPrimaryDisplay().bounds().Contains(
      power_button_test_api_->GetMenuBoundsInScreen()));

  // Rotate the screen by 270 degrees.
  test_api.SetDisplayRotation(display::Display::ROTATE_270,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            chromeos::OrientationType::kPortraitPrimary);
  EXPECT_TRUE(GetPrimaryDisplay().bounds().Contains(
      power_button_test_api_->GetMenuBoundsInScreen()));

  // Rotate the screen by 180 degrees.
  test_api.SetDisplayRotation(display::Display::ROTATE_180,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            chromeos::OrientationType::kLandscapeSecondary);
  EXPECT_TRUE(GetPrimaryDisplay().bounds().Contains(
      power_button_test_api_->GetMenuBoundsInScreen()));

  // Rotate the screen by 90 degrees.
  test_api.SetDisplayRotation(display::Display::ROTATE_90,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            chromeos::OrientationType::kPortraitSecondary);
  EXPECT_TRUE(GetPrimaryDisplay().bounds().Contains(
      power_button_test_api_->GetMenuBoundsInScreen()));
}

// Tests that a power button press before the menu is fully shown will not
// create a new menu.
TEST_F(PowerButtonControllerTest, LegacyPowerButtonIgnoreExtraPress) {
  Initialize(ButtonType::LEGACY, LoginStatus::USER);

  // Enable animations so that we can make sure that they occur.
  ui::ScopedAnimationDurationScaleMode regular_animations(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  PressPowerButton();
  // Power menu is in the partially shown state.
  ASSERT_TRUE(power_button_test_api_->IsMenuOpened());
  ASSERT_FALSE(power_button_test_api_->ShowMenuAnimationDone());
  PowerButtonMenuView* menu_view_before =
      power_button_test_api_->GetPowerButtonMenuView();
  // Press power button again and make sure new PowerButtonMenuView is not
  // created. This makes sure that we do not create a new menu while we are in
  // the process of creating one for an old power button press.
  PressPowerButton();
  EXPECT_EQ(menu_view_before, power_button_test_api_->GetPowerButtonMenuView());
  // This is needed to simulate the shutdown sound having been played,
  // which blocks the shutdown timer.
  // Make sure that the second press did not trigger a shutdown.
  EXPECT_FALSE(lock_state_test_api_->real_shutdown_timer_is_running());
  // Make sure that power menu is still in partially shown state.
  ASSERT_TRUE(power_button_test_api_->IsMenuOpened());
  ASSERT_FALSE(power_button_test_api_->ShowMenuAnimationDone());
}

TEST_F(PowerButtonControllerTest,
       ArcPowerButtonEventShowMenuWithoutPreShutdown) {
  LaunchArcPowerButtonEvent();
  ASSERT_TRUE(power_button_test_api_->IsMenuOpened());
  EXPECT_FALSE(power_button_test_api_->TriggerPreShutdownTimeout());
  EXPECT_FALSE(lock_state_test_api_->shutdown_timer_is_running());
  EXPECT_TRUE(power_button_test_api_->IsMenuOpened());
}

INSTANTIATE_TEST_SUITE_P(AshPowerButtonPosition,
                         PowerButtonControllerWithPositionTest,
                         testing::Values(PowerButtonPosition::LEFT,
                                         PowerButtonPosition::RIGHT,
                                         PowerButtonPosition::TOP,
                                         PowerButtonPosition::BOTTOM));

}  // namespace ash
