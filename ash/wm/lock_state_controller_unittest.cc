// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/lock_state_controller.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/shutdown_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/shutdown_reason.h"
#include "ash/system/power/power_button_controller.h"
#include "ash/system/power/power_button_controller_test_api.h"
#include "ash/system/power/power_button_test_base.h"
#include "ash/touch/touch_devices_controller.h"
#include "ash/wm/lock_state_controller_test_api.h"
#include "ash/wm/session_state_animator.h"
#include "ash/wm/test_session_state_animator.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "ui/display/fake/fake_display_snapshot.h"
#include "ui/display/manager/display_configurator.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/geometry/size.h"

namespace ash {
namespace {

// Shorthand for some long constants.
constexpr power_manager::BacklightBrightnessChange_Cause kUserCause =
    power_manager::BacklightBrightnessChange_Cause_USER_REQUEST;
constexpr power_manager::BacklightBrightnessChange_Cause kOtherCause =
    power_manager::BacklightBrightnessChange_Cause_OTHER;
bool cursor_visible() {
  return Shell::Get()->cursor_manager()->IsCursorVisible();
}

void CheckCalledCallback(bool* flag) {
  if (flag)
    (*flag) = true;
}

// ShutdownController that tracks how many shutdown requests have been made.
class TestShutdownController : public ShutdownController {
 public:
  TestShutdownController() = default;
  ~TestShutdownController() override = default;

  int num_shutdown_requests() const { return num_shutdown_requests_; }

 private:
  // ShutdownController:
  void SetRebootOnShutdown(bool reboot_on_shutdown) override {}
  void ShutDownOrReboot(ShutdownReason reason) override {
    num_shutdown_requests_++;
  }

  int num_shutdown_requests_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestShutdownController);
};

}  // namespace

class LockStateControllerTest : public PowerButtonTestBase {
 public:
  LockStateControllerTest() = default;
  ~LockStateControllerTest() override = default;

  // PowerButtonTestBase:
  void SetUp() override {
    PowerButtonTestBase::SetUp();
    InitPowerButtonControllerMembers(
        chromeos::PowerManagerClient::TabletMode::UNSUPPORTED);

    test_animator_ = new TestSessionStateAnimator;
    lock_state_controller_->set_animator_for_test(test_animator_);

    shutdown_controller_resetter_ =
        std::make_unique<ShutdownController::ScopedResetterForTest>();
    test_shutdown_controller_ = std::make_unique<TestShutdownController>();
    lock_state_test_api_->set_shutdown_controller(
        test_shutdown_controller_.get());
  }
  void TearDown() override {
    test_shutdown_controller_.reset();
    shutdown_controller_resetter_.reset();
    PowerButtonTestBase::TearDown();
  }

 protected:
  int NumShutdownRequests() {
    return test_shutdown_controller_->num_shutdown_requests();
  }

  void Advance(SessionStateAnimator::AnimationSpeed speed) {
    test_animator_->Advance(test_animator_->GetDuration(speed));
  }

  void AdvancePartially(SessionStateAnimator::AnimationSpeed speed,
                        float factor) {
    test_animator_->Advance(test_animator_->GetDuration(speed) * factor);
  }

  void SendBrightnessChange(
      double percent,
      power_manager::BacklightBrightnessChange_Cause cause) {
    power_manager::BacklightBrightnessChange change;
    change.set_percent(percent);
    change.set_cause(cause);
    power_manager_client()->SendScreenBrightnessChanged(change);
  }

  void ExpectPreLockAnimationStarted() {
    SCOPED_TRACE("Failure in ExpectPreLockAnimationStarted");
    EXPECT_LT(0u, test_animator_->GetAnimationCount());
    EXPECT_TRUE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
        SessionStateAnimator::ANIMATION_LIFT));
    EXPECT_TRUE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::SHELF, SessionStateAnimator::ANIMATION_FADE_OUT));
    EXPECT_TRUE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
        SessionStateAnimator::ANIMATION_HIDE_IMMEDIATELY));
    EXPECT_TRUE(lock_state_test_api_->is_animating_lock());
  }

  void ExpectPreLockAnimationRunning() {
    SCOPED_TRACE("Failure in ExpectPreLockAnimationRunning");
    EXPECT_LT(0u, test_animator_->GetAnimationCount());
    EXPECT_TRUE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
        SessionStateAnimator::ANIMATION_LIFT));
    EXPECT_TRUE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::SHELF, SessionStateAnimator::ANIMATION_FADE_OUT));
    EXPECT_TRUE(lock_state_test_api_->is_animating_lock());
  }

  void ExpectPreLockAnimationCancel() {
    SCOPED_TRACE("Failure in ExpectPreLockAnimationCancel");
    EXPECT_LT(0u, test_animator_->GetAnimationCount());
    EXPECT_TRUE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
        SessionStateAnimator::ANIMATION_UNDO_LIFT));
    EXPECT_TRUE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::SHELF, SessionStateAnimator::ANIMATION_FADE_IN));
  }

  void ExpectPreLockAnimationFinished() {
    SCOPED_TRACE("Failure in ExpectPreLockAnimationFinished");
    EXPECT_FALSE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
        SessionStateAnimator::ANIMATION_LIFT));
    EXPECT_FALSE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::SHELF, SessionStateAnimator::ANIMATION_FADE_OUT));
    EXPECT_FALSE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
        SessionStateAnimator::ANIMATION_HIDE_IMMEDIATELY));
  }

  void ExpectPostLockAnimationStarted() {
    SCOPED_TRACE("Failure in ExpectPostLockAnimationStarted");
    EXPECT_LT(0u, test_animator_->GetAnimationCount());
    EXPECT_TRUE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
        SessionStateAnimator::ANIMATION_RAISE_TO_SCREEN));
    EXPECT_TRUE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::SHELF, SessionStateAnimator::ANIMATION_FADE_IN));
  }

  void ExpectPostLockAnimationFinished() {
    SCOPED_TRACE("Failure in ExpectPostLockAnimationFinished");
    EXPECT_FALSE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
        SessionStateAnimator::ANIMATION_RAISE_TO_SCREEN));
  }

  void ExpectUnlockBeforeUIDestroyedAnimationStarted() {
    SCOPED_TRACE("Failure in ExpectUnlockBeforeUIDestroyedAnimationStarted");
    EXPECT_LT(0u, test_animator_->GetAnimationCount());
    EXPECT_TRUE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
        SessionStateAnimator::ANIMATION_LIFT));
  }

  void ExpectUnlockBeforeUIDestroyedAnimationFinished() {
    SCOPED_TRACE("Failure in ExpectUnlockBeforeUIDestroyedAnimationFinished");
    EXPECT_FALSE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
        SessionStateAnimator::ANIMATION_LIFT));
  }

  void ExpectUnlockAfterUIDestroyedAnimationStarted() {
    SCOPED_TRACE("Failure in ExpectUnlockAfterUIDestroyedAnimationStarted");
    EXPECT_LT(0u, test_animator_->GetAnimationCount());
    EXPECT_TRUE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
        SessionStateAnimator::ANIMATION_DROP));
    EXPECT_TRUE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::SHELF, SessionStateAnimator::ANIMATION_FADE_IN));
  }

  void ExpectUnlockAfterUIDestroyedAnimationFinished() {
    SCOPED_TRACE("Failure in ExpectUnlockAfterUIDestroyedAnimationFinished");
    EXPECT_EQ(0u, test_animator_->GetAnimationCount());
    EXPECT_FALSE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
        SessionStateAnimator::ANIMATION_DROP));
    EXPECT_FALSE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::SHELF, SessionStateAnimator::ANIMATION_FADE_IN));
  }

  void ExpectShutdownAnimationStarted() {
    SCOPED_TRACE("Failure in ExpectShutdownAnimationStarted");
    EXPECT_LT(0u, test_animator_->GetAnimationCount());
    EXPECT_TRUE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::ROOT_CONTAINER,
        SessionStateAnimator::ANIMATION_GRAYSCALE_BRIGHTNESS));
  }

  void ExpectShutdownAnimationFinished() {
    SCOPED_TRACE("Failure in ExpectShutdownAnimationFinished");
    EXPECT_EQ(0u, test_animator_->GetAnimationCount());
    EXPECT_FALSE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::ROOT_CONTAINER,
        SessionStateAnimator::ANIMATION_GRAYSCALE_BRIGHTNESS));
  }

  void ExpectShutdownAnimationCancel() {
    SCOPED_TRACE("Failure in ExpectShutdownAnimationCancel");
    EXPECT_LT(0u, test_animator_->GetAnimationCount());
    EXPECT_TRUE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::ROOT_CONTAINER,
        SessionStateAnimator::ANIMATION_UNDO_GRAYSCALE_BRIGHTNESS));
  }

  void ExpectWallpaperIsShowing() {
    SCOPED_TRACE("Failure in ExpectWallpaperIsShowing");
    EXPECT_LT(0u, test_animator_->GetAnimationCount());
    EXPECT_TRUE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::WALLPAPER,
        SessionStateAnimator::ANIMATION_FADE_IN));
  }

  void ExpectWallpaperIsHiding() {
    SCOPED_TRACE("Failure in ExpectWallpaperIsHiding");
    EXPECT_LT(0u, test_animator_->GetAnimationCount());
    EXPECT_TRUE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::WALLPAPER,
        SessionStateAnimator::ANIMATION_FADE_OUT));
  }

  void ExpectRestoringWallpaperVisibility() {
    SCOPED_TRACE("Failure in ExpectRestoringWallpaperVisibility");
    EXPECT_LT(0u, test_animator_->GetAnimationCount());
    EXPECT_TRUE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::WALLPAPER,
        SessionStateAnimator::ANIMATION_FADE_IN));
  }

  void ExpectUnlockedState() {
    SCOPED_TRACE("Failure in ExpectUnlockedState");
    EXPECT_EQ(0u, test_animator_->GetAnimationCount());
    EXPECT_FALSE(Shell::Get()->session_controller()->IsScreenLocked());
  }

  void ExpectLockedState() {
    SCOPED_TRACE("Failure in ExpectLockedState");
    EXPECT_EQ(0u, test_animator_->GetAnimationCount());
    EXPECT_TRUE(Shell::Get()->session_controller()->IsScreenLocked());
  }

  void HideWallpaper() { test_animator_->HideWallpaper(); }

  void PressLockButton() {
    power_button_controller_->OnLockButtonEvent(true, base::TimeTicks::Now());
  }

  void ReleaseLockButton() {
    power_button_controller_->OnLockButtonEvent(false, base::TimeTicks::Now());
  }

  void SuccessfulAuthentication(bool* call_flag) {
    base::Closure closure = base::Bind(&CheckCalledCallback, call_flag);
    lock_state_controller_->OnLockScreenHide(closure);
  }

  std::unique_ptr<ShutdownController::ScopedResetterForTest>
      shutdown_controller_resetter_;
  std::unique_ptr<TestShutdownController> test_shutdown_controller_;
  TestSessionStateAnimator* test_animator_ = nullptr;   // not owned

 private:
  DISALLOW_COPY_AND_ASSIGN(LockStateControllerTest);
};

// Test the show menu and shutdown flow for non-Chrome-OS hardware that doesn't
// correctly report power button releases.  We should show menu the first
// time the button is pressed and shut down when it's pressed from the locked
// state.
TEST_F(LockStateControllerTest, LegacyShowMenuAndShutDown) {
  Initialize(ButtonType::LEGACY, LoginStatus::USER);

  ExpectUnlockedState();

  // We should request that the screen be locked immediately after seeing the
  // power button get pressed.
  PressPowerButton();

  EXPECT_TRUE(power_button_test_api_->IsMenuOpened());

  // We shouldn't progress towards the shutdown state, however.
  EXPECT_FALSE(lock_state_test_api_->shutdown_timer_is_running());

  ReleasePowerButton();

  // Hold the button again and check that we start shutting down.
  PressPowerButton();

  ExpectShutdownAnimationStarted();

  EXPECT_EQ(0, NumShutdownRequests());
  // Make sure a mouse move event won't show the cursor.
  GenerateMouseMoveEvent();
  EXPECT_FALSE(cursor_visible());

  EXPECT_TRUE(lock_state_test_api_->real_shutdown_timer_is_running());
  lock_state_test_api_->trigger_real_shutdown_timeout();
  EXPECT_EQ(1, NumShutdownRequests());
}

// Test that we ignore power button presses when the screen is turned off on an
// unofficial system.
TEST_F(LockStateControllerTest, LegacyIgnorePowerButtonIfScreenIsOff) {
  Initialize(ButtonType::LEGACY, LoginStatus::USER);

  // When the screen brightness is at 0%, we shouldn't do anything in response
  // to power button presses.
  SendBrightnessChange(0, kUserCause);
  PressPowerButton();
  EXPECT_FALSE(power_button_test_api_->IsMenuOpened());
  ReleasePowerButton();

  // After increasing the brightness to 10%, we should show the menu as usual.
  SendBrightnessChange(10, kUserCause);
  PressPowerButton();
  EXPECT_TRUE(power_button_test_api_->IsMenuOpened());
  ReleasePowerButton();
}

TEST_F(LockStateControllerTest, LegacyHonorPowerButtonInDockedMode) {
  Initialize(ButtonType::LEGACY, LoginStatus::USER);
  // Create two outputs, the first internal and the second external.
  display::DisplayConfigurator::DisplayStateList outputs;

  std::unique_ptr<display::DisplaySnapshot> internal_display =
      display::FakeDisplaySnapshot::Builder()
          .SetId(123)
          .SetNativeMode(gfx::Size(1, 1))
          .SetType(display::DISPLAY_CONNECTION_TYPE_INTERNAL)
          .Build();
  outputs.push_back(internal_display.get());

  std::unique_ptr<display::DisplaySnapshot> external_display =
      display::FakeDisplaySnapshot::Builder()
          .SetId(456)
          .SetNativeMode(gfx::Size(1, 1))
          .SetType(display::DISPLAY_CONNECTION_TYPE_HDMI)
          .Build();
  outputs.push_back(external_display.get());

  // When all of the displays are turned off (e.g. due to user inactivity), the
  // power button should be ignored.
  SendBrightnessChange(0, kUserCause);
  internal_display->set_current_mode(nullptr);
  external_display->set_current_mode(nullptr);
  power_button_controller_->OnDisplayModeChanged(outputs);
  PressPowerButton();
  EXPECT_FALSE(power_button_test_api_->IsMenuOpened());
  ReleasePowerButton();

  // When the screen brightness is 0% but the external display is still turned
  // on (indicating either docked mode or the user having manually decreased the
  // brightness to 0%), the power button should still be handled.
  external_display->set_current_mode(external_display->modes().back().get());
  power_button_controller_->OnDisplayModeChanged(outputs);
  PressPowerButton();
  EXPECT_TRUE(power_button_test_api_->IsMenuOpened());
  ReleasePowerButton();
}

// Test the basic operation of the lock button (not logged in).
TEST_F(LockStateControllerTest, LockButtonBasicNotLoggedIn) {
  // The lock button shouldn't do anything if we aren't logged in.
  Initialize(ButtonType::NORMAL, LoginStatus::NOT_LOGGED_IN);

  PressLockButton();
  EXPECT_FALSE(lock_state_test_api_->is_animating_lock());
  ReleaseLockButton();
  EXPECT_FALSE(Shell::Get()->session_controller()->IsScreenLocked());
}

// Test the basic operation of the lock button (guest).
TEST_F(LockStateControllerTest, LockButtonBasicGuest) {
  // The lock button shouldn't do anything when we're logged in as a guest.
  Initialize(ButtonType::NORMAL, LoginStatus::GUEST);

  PressLockButton();
  EXPECT_FALSE(lock_state_test_api_->is_animating_lock());
  ReleaseLockButton();
  EXPECT_FALSE(Shell::Get()->session_controller()->IsScreenLocked());
}

// Test the basic operation of the lock button.
TEST_F(LockStateControllerTest, LockButtonBasic) {
  // If we're logged in as a regular user, we should start the lock timer and
  // the pre-lock animation.
  Initialize(ButtonType::NORMAL, LoginStatus::USER);

  PressLockButton();
  ExpectPreLockAnimationStarted();
  AdvancePartially(SessionStateAnimator::ANIMATION_SPEED_UNDOABLE, 0.5f);
  ExpectPreLockAnimationRunning();

  // If the button is released immediately, we shouldn't lock the screen.
  ReleaseLockButton();
  ExpectPreLockAnimationCancel();
  Advance(SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS);

  ExpectUnlockedState();
  EXPECT_FALSE(Shell::Get()->session_controller()->IsScreenLocked());

  // Press the button again and let the lock timeout fire.  We should request
  // that the screen be locked.
  PressLockButton();
  ExpectPreLockAnimationStarted();
  Advance(SessionStateAnimator::ANIMATION_SPEED_UNDOABLE);

  GetSessionControllerClient()->FlushForTest();
  EXPECT_TRUE(Shell::Get()->session_controller()->IsScreenLocked());

  // Pressing the lock button while we have a pending lock request shouldn't do
  // anything.
  ReleaseLockButton();
  PressLockButton();
  ExpectPreLockAnimationFinished();
  ReleaseLockButton();

  // Pressing the button also shouldn't do anything after the screen is locked.
  ExpectPostLockAnimationStarted();

  PressLockButton();
  ReleaseLockButton();
  ExpectPostLockAnimationStarted();

  Advance(SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS);
  ExpectPostLockAnimationFinished();

  PressLockButton();
  ReleaseLockButton();
  ExpectPostLockAnimationFinished();
}

#if 0
// When the screen is locked without going through the usual power-button
// slow-close path (e.g. via the wrench menu), test that we still show the
// fast-close animation.
TEST_F(LockStateControllerTest, LockWithoutButton) {
  Initialize(ButtonType::NORMAL, LoginStatus::USER);
  lock_state_controller_->OnStartingLock();

  ExpectPreLockAnimationStarted();
  EXPECT_FALSE(lock_state_test_api_->is_lock_cancellable());
  EXPECT_LT(0u, test_animator_->GetAnimationCount());

  test_animator_->CompleteAllAnimations(true);
  EXPECT_FALSE(Shell::Get()->session_controller()->IsScreenLocked());
}
#endif

// When we hear that the process is exiting but we haven't had a chance to
// display an animation, we should just blank the screen.
TEST_F(LockStateControllerTest, ShutdownWithoutButton) {
  Initialize(ButtonType::NORMAL, LoginStatus::USER);
  lock_state_controller_->OnChromeTerminating();

  EXPECT_TRUE(test_animator_->AreContainersAnimated(
      SessionStateAnimator::kAllNonRootContainersMask,
      SessionStateAnimator::ANIMATION_HIDE_IMMEDIATELY));
  GenerateMouseMoveEvent();
  EXPECT_FALSE(cursor_visible());
}

// Test that we display the fast-close animation and shut down when we get an
// outside request to shut down (e.g. from the login or lock screen).
TEST_F(LockStateControllerTest, RequestShutdownFromLoginScreen) {
  Initialize(ButtonType::NORMAL, LoginStatus::NOT_LOGGED_IN);

  lock_state_controller_->RequestShutdown(
      ShutdownReason::LOGIN_SHUT_DOWN_BUTTON);

  ExpectShutdownAnimationStarted();
  Advance(SessionStateAnimator::ANIMATION_SPEED_SHUTDOWN);

  GenerateMouseMoveEvent();
  EXPECT_FALSE(cursor_visible());

  EXPECT_EQ(0, NumShutdownRequests());
  EXPECT_TRUE(lock_state_test_api_->real_shutdown_timer_is_running());
  lock_state_test_api_->trigger_real_shutdown_timeout();
  EXPECT_EQ(1, NumShutdownRequests());
}

TEST_F(LockStateControllerTest, RequestShutdownFromLockScreen) {
  Initialize(ButtonType::NORMAL, LoginStatus::USER);

  LockScreen();

  Advance(SessionStateAnimator::ANIMATION_SPEED_SHUTDOWN);
  ExpectPostLockAnimationFinished();

  lock_state_controller_->RequestShutdown(
      ShutdownReason::LOGIN_SHUT_DOWN_BUTTON);

  ExpectShutdownAnimationStarted();
  Advance(SessionStateAnimator::ANIMATION_SPEED_SHUTDOWN);

  GenerateMouseMoveEvent();
  EXPECT_FALSE(cursor_visible());

  EXPECT_EQ(0, NumShutdownRequests());
  EXPECT_TRUE(lock_state_test_api_->real_shutdown_timer_is_running());
  lock_state_test_api_->trigger_real_shutdown_timeout();
  EXPECT_EQ(1, NumShutdownRequests());
}

// Test that hidden wallpaper appears and reverts correctly on lock/cancel.
TEST_F(LockStateControllerTest, TestHiddenWallpaperLockCancel) {
  Initialize(ButtonType::NORMAL, LoginStatus::USER);
  HideWallpaper();

  ExpectUnlockedState();
  PressLockButton();

  ExpectPreLockAnimationStarted();
  ExpectWallpaperIsShowing();

  // Forward only half way through.
  AdvancePartially(SessionStateAnimator::ANIMATION_SPEED_UNDOABLE, 0.5f);

  // Release the button before the lock timer fires.
  ReleaseLockButton();
  ExpectPreLockAnimationCancel();
  ExpectWallpaperIsHiding();

  Advance(SessionStateAnimator::ANIMATION_SPEED_UNDO_MOVE_WINDOWS);

  // When the CancelPrelockAnimation sequence finishes it queues up a
  // restore wallpaper visibility sequence when the wallpaper is hidden.
  ExpectRestoringWallpaperVisibility();

  Advance(SessionStateAnimator::ANIMATION_SPEED_IMMEDIATE);

  ExpectUnlockedState();
}

// Test that hidden wallpaper appears and revers correctly on lock/unlock.
TEST_F(LockStateControllerTest, TestHiddenWallpaperLockUnlock) {
  Initialize(ButtonType::NORMAL, LoginStatus::USER);
  HideWallpaper();

  ExpectUnlockedState();

  // Press the lock button and check that the lock timer is started and that we
  // start lifting the non-screen-locker containers.
  PressLockButton();

  ExpectPreLockAnimationStarted();
  ExpectWallpaperIsShowing();

  Advance(SessionStateAnimator::ANIMATION_SPEED_UNDOABLE);

  ExpectPreLockAnimationFinished();

  LockScreen();

  ReleaseLockButton();

  ExpectPostLockAnimationStarted();
  Advance(SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS);
  ExpectPostLockAnimationFinished();

  ExpectLockedState();

  SuccessfulAuthentication(nullptr);

  ExpectUnlockBeforeUIDestroyedAnimationStarted();
  Advance(SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS);
  ExpectUnlockBeforeUIDestroyedAnimationFinished();

  UnlockScreen();

  ExpectUnlockAfterUIDestroyedAnimationStarted();
  ExpectWallpaperIsHiding();

  Advance(SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS);

  // When the StartUnlockAnimationAfterUIDestroyed sequence finishes it queues
  // up a restore wallpaper visibility sequence when the wallpaper is hidden.
  ExpectRestoringWallpaperVisibility();

  Advance(SessionStateAnimator::ANIMATION_SPEED_IMMEDIATE);

  ExpectUnlockAfterUIDestroyedAnimationFinished();

  ExpectUnlockedState();
}

// Tests the default behavior of disabling the touchscreen when the screen is
// turned off due to user inactivity.
TEST_F(LockStateControllerTest, DisableTouchscreenForScreenOff) {
  Initialize(ButtonType::NORMAL, LoginStatus::USER);
  // Run the event loop so PowerButtonDisplayController will get the initial
  // backlights-forced-off state from chromeos::PowerManagerClient.
  base::RunLoop().RunUntilIdle();

  // Manually turn the screen off and check that the touchscreen is enabled.
  SendBrightnessChange(0, kUserCause);
  EXPECT_TRUE(Shell::Get()->touch_devices_controller()->GetTouchscreenEnabled(
      TouchDeviceEnabledSource::GLOBAL));

  // It should be disabled if the screen is turned off due to user inactivity.
  SendBrightnessChange(100, kUserCause);
  SendBrightnessChange(0, kOtherCause);
  EXPECT_FALSE(Shell::Get()->touch_devices_controller()->GetTouchscreenEnabled(
      TouchDeviceEnabledSource::GLOBAL));
}

// Tests that the kTouchscreenUsableWhileScreenOff switch keeps the touchscreen
// enabled when the screen is turned off due to user inactivity.
TEST_F(LockStateControllerTest, TouchscreenUnableWhileScreenOff) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kTouchscreenUsableWhileScreenOff);
  ResetPowerButtonController();
  Initialize(ButtonType::NORMAL, LoginStatus::USER);
  // Run the event loop so PowerButtonDisplayController will get the initial
  // backlights-forced-off state from chromeos::PowerManagerClient.
  base::RunLoop().RunUntilIdle();

  // The touchscreen should remain enabled.
  SendBrightnessChange(0, kOtherCause);
  EXPECT_TRUE(Shell::Get()->touch_devices_controller()->GetTouchscreenEnabled(
      TouchDeviceEnabledSource::GLOBAL));
}

// Tests that continue pressing the power button for a while after power menu is
// shown should trigger the cancellable pre-shutdown animation.
TEST_F(LockStateControllerTest, ShutDownAfterShowPowerMenu) {
  Initialize(ButtonType::NORMAL, LoginStatus::USER);
  PressPowerButton();
  EXPECT_TRUE(power_button_test_api_->IsMenuOpened());
  ASSERT_TRUE(power_button_test_api_->TriggerPreShutdownTimeout());
  EXPECT_TRUE(lock_state_test_api_->shutdown_timer_is_running());

  ExpectShutdownAnimationStarted();
  AdvancePartially(SessionStateAnimator::ANIMATION_SPEED_SHUTDOWN, 0.5f);
  // Release the power button before the shutdown timer fires.
  ReleasePowerButton();
  EXPECT_FALSE(lock_state_test_api_->shutdown_timer_is_running());
  ExpectShutdownAnimationCancel();

  power_button_controller_->DismissMenu();
  EXPECT_FALSE(power_button_test_api_->IsMenuOpened());

  // Press the button again and make the shutdown timeout fire this time.
  // Check that we start the timer for actually requesting the shutdown.
  PressPowerButton();
  ASSERT_TRUE(power_button_test_api_->TriggerPreShutdownTimeout());
  EXPECT_TRUE(lock_state_test_api_->shutdown_timer_is_running());

  Advance(SessionStateAnimator::ANIMATION_SPEED_SHUTDOWN);
  ExpectShutdownAnimationFinished();
  lock_state_test_api_->trigger_shutdown_timeout();

  EXPECT_TRUE(lock_state_test_api_->real_shutdown_timer_is_running());
  EXPECT_EQ(0, NumShutdownRequests());

  // When the timeout fires, we should request a shutdown.
  lock_state_test_api_->trigger_real_shutdown_timeout();
  EXPECT_EQ(1, NumShutdownRequests());
}

}  // namespace ash
