// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/lock_state_controller.h"

#include <memory>
#include <utility>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/shutdown_controller.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/shutdown_reason.h"
#include "ash/system/power/power_button_controller.h"
#include "ash/system/power/power_button_controller_test_api.h"
#include "ash/system/power/power_button_test_base.h"
#include "ash/touch/touch_devices_controller.h"
#include "ash/utility/layer_copy_animator.h"
#include "ash/wallpaper/views/wallpaper_view.h"
#include "ash/wallpaper/views/wallpaper_widget_controller.h"
#include "ash/wm/lock_state_controller_test_api.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/session_state_animator.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/test/test_session_state_animator.h"
#include "ash/wm/window_restore/window_restore_metrics.h"
#include "ash/wm/window_restore/window_restore_util.h"
#include "ash/wm/window_util.h"
#include "base/barrier_closure.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "ui/aura/test/test_windows.h"
#include "ui/display/manager/display_configurator.h"
#include "ui/display/manager/test/fake_display_snapshot.h"
#include "ui/display/tablet_state.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/geometry/size.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

constexpr char kShelfShutdownConfirmationHistogramName[] =
    "Ash.Shelf.ShutdownConfirmationBubble.TimeToNextBoot."
    "LoginShutdownToPowerUpDuration";

// Shorthand for some long constants.
constexpr power_manager::BacklightBrightnessChange_Cause kUserCause =
    power_manager::BacklightBrightnessChange_Cause_USER_REQUEST;
constexpr power_manager::BacklightBrightnessChange_Cause kOtherCause =
    power_manager::BacklightBrightnessChange_Cause_OTHER;
bool cursor_visible() {
  return Shell::Get()->cursor_manager()->IsCursorVisible();
}

void CheckCalledCallback(bool* flag, bool aborted) {
  if (flag)
    (*flag) = true;
}

// ShutdownController that tracks how many shutdown requests have been made.
class TestShutdownController : public ShutdownController {
 public:
  TestShutdownController() = default;

  TestShutdownController(const TestShutdownController&) = delete;
  TestShutdownController& operator=(const TestShutdownController&) = delete;

  ~TestShutdownController() override = default;

  int num_shutdown_requests() const { return num_shutdown_requests_; }

 private:
  // ShutdownController:
  void SetRebootOnShutdown(bool reboot_on_shutdown) override {}
  void ShutDownOrReboot(ShutdownReason reason) override {
    num_shutdown_requests_++;
  }

  int num_shutdown_requests_ = 0;
};

}  // namespace

class LockStateControllerTest : public PowerButtonTestBase {
 public:
  LockStateControllerTest() = default;

  LockStateControllerTest(const LockStateControllerTest&) = delete;
  LockStateControllerTest& operator=(const LockStateControllerTest&) = delete;

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

    local_state_ = Shell::Get()->local_state();
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

  void ExpectPreLockAnimationStarted(const std::string& scope) {
    SCOPED_TRACE("ExpectPreLockAnimationStarted:" + scope);
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

  void ExpectPreLockAnimationRunning(const std::string& scope) {
    SCOPED_TRACE("ExpectPreLockAnimationRunning:" + scope);
    EXPECT_LT(0u, test_animator_->GetAnimationCount());
    EXPECT_TRUE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
        SessionStateAnimator::ANIMATION_LIFT));
    EXPECT_TRUE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::SHELF, SessionStateAnimator::ANIMATION_FADE_OUT));
    EXPECT_TRUE(lock_state_test_api_->is_animating_lock());
  }

  void ExpectPreLockAnimationCancel(const std::string& scope) {
    SCOPED_TRACE("ExpectPreLockAnimationCancel:" + scope);
    EXPECT_LT(0u, test_animator_->GetAnimationCount());
    EXPECT_TRUE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
        SessionStateAnimator::ANIMATION_UNDO_LIFT));
    EXPECT_TRUE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::SHELF, SessionStateAnimator::ANIMATION_FADE_IN));
  }

  void ExpectPreLockAnimationFinished(const std::string& scope) {
    SCOPED_TRACE("ExpectPreLockAnimationFinished:" + scope);
    EXPECT_FALSE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
        SessionStateAnimator::ANIMATION_LIFT));
    EXPECT_FALSE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::SHELF, SessionStateAnimator::ANIMATION_FADE_OUT));
    EXPECT_FALSE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
        SessionStateAnimator::ANIMATION_HIDE_IMMEDIATELY));
  }

  void ExpectPostLockAnimationStarted(const std::string& scope) {
    SCOPED_TRACE("ExpectPostLockAnimationStarted:" + scope);
    EXPECT_LT(0u, test_animator_->GetAnimationCount());
    EXPECT_TRUE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
        SessionStateAnimator::ANIMATION_RAISE_TO_SCREEN));
    EXPECT_TRUE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::SHELF, SessionStateAnimator::ANIMATION_FADE_IN));
  }

  void ExpectPostLockAnimationFinished(const std::string& scope) {
    SCOPED_TRACE("ExpectPostLockAnimationFinished:" + scope);
    EXPECT_FALSE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
        SessionStateAnimator::ANIMATION_RAISE_TO_SCREEN));
  }

  void ExpectUnlockBeforeUIDestroyedAnimationStarted(const std::string& scope) {
    SCOPED_TRACE("ExpectUnlockBeforeUIDestroyedAnimationStarted:" + scope);
    EXPECT_LT(0u, test_animator_->GetAnimationCount());
    EXPECT_TRUE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
        SessionStateAnimator::ANIMATION_LIFT));
  }

  void ExpectUnlockBeforeUIDestroyedAnimationFinished(
      const std::string& scope) {
    SCOPED_TRACE("ExpectUnlockBeforeUIDestroyedAnimationFinished:" + scope);
    EXPECT_FALSE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
        SessionStateAnimator::ANIMATION_LIFT));
  }

  void ExpectUnlockAfterUIDestroyedAnimationStarted(const std::string& scope) {
    SCOPED_TRACE("ExpectUnlockAfterUIDestroyedAnimationStarted:" + scope);
    EXPECT_LT(0u, test_animator_->GetAnimationCount());
    EXPECT_TRUE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
        SessionStateAnimator::ANIMATION_DROP));
    EXPECT_TRUE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::SHELF, SessionStateAnimator::ANIMATION_FADE_IN));
  }

  void ExpectUnlockAfterUIDestroyedAnimationFinished(const std::string& scope) {
    SCOPED_TRACE("ExpectUnlockAfterUIDestroyedAnimationFinished:" + scope);
    EXPECT_EQ(0u, test_animator_->GetAnimationCount());
    EXPECT_FALSE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
        SessionStateAnimator::ANIMATION_DROP));
    EXPECT_FALSE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::SHELF, SessionStateAnimator::ANIMATION_FADE_IN));
  }

  void ExpectShutdownAnimationStarted(const std::string& scope) {
    SCOPED_TRACE("ExpectShutdownAnimationStarted:" + scope);
    EXPECT_LT(0u, test_animator_->GetAnimationCount());
    EXPECT_TRUE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::ROOT_CONTAINER,
        SessionStateAnimator::ANIMATION_GRAYSCALE_BRIGHTNESS));
  }

  void ExpectShutdownAnimationFinished(const std::string& scope) {
    SCOPED_TRACE(" ExpectShutdownAnimationFinished:" + scope);
    EXPECT_EQ(0u, test_animator_->GetAnimationCount());
    EXPECT_FALSE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::ROOT_CONTAINER,
        SessionStateAnimator::ANIMATION_GRAYSCALE_BRIGHTNESS));
  }

  void ExpectShutdownAnimationCancel(const std::string& scope) {
    SCOPED_TRACE("ExpectShutdownAnimationCancel:" + scope);
    EXPECT_LT(0u, test_animator_->GetAnimationCount());
    EXPECT_TRUE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::ROOT_CONTAINER,
        SessionStateAnimator::ANIMATION_UNDO_GRAYSCALE_BRIGHTNESS));
  }

  void ExpectWallpaperIsShowing(const std::string& scope) {
    SCOPED_TRACE("ExpectWallpaperIsShowing:" + scope);
    EXPECT_LT(0u, test_animator_->GetAnimationCount());
    EXPECT_TRUE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::WALLPAPER,
        SessionStateAnimator::ANIMATION_FADE_IN));
  }

  void ExpectWallpaperIsHiding(const std::string& scope) {
    SCOPED_TRACE("ExpectWallpaperIsHiding:" + scope);
    EXPECT_LT(0u, test_animator_->GetAnimationCount());
    EXPECT_TRUE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::WALLPAPER,
        SessionStateAnimator::ANIMATION_FADE_OUT));
  }

  void ExpectRestoringWallpaperVisibility(const std::string& scope) {
    SCOPED_TRACE(" ExpectRestoringWallpaperVisibility:" + scope);
    EXPECT_LT(0u, test_animator_->GetAnimationCount());
    EXPECT_TRUE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::WALLPAPER,
        SessionStateAnimator::ANIMATION_FADE_IN));
  }

  void ExpectUnlockedState(const std::string& scope) {
    SCOPED_TRACE("ExpectUnlockedState:" + scope);
    EXPECT_EQ(0u, test_animator_->GetAnimationCount());
    EXPECT_FALSE(Shell::Get()->session_controller()->IsScreenLocked());
  }

  void ExpectLockedState(const std::string& scope) {
    SCOPED_TRACE("ExpectLockedState:" + scope);
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
    auto callback = base::BindOnce(&CheckCalledCallback, call_flag);
    lock_state_controller_->OnLockScreenHide(std::move(callback));
  }

  bool IsDefaultValueLoginShutdownTimestamp() {
    auto* login_shutdown_timestamp_pref =
        local_state_->FindPreference(prefs::kLoginShutdownTimestampPrefName);

    return login_shutdown_timestamp_pref->IsDefaultValue();
  }

  // To check if histogram of LockStateController is recorded correctly, we need
  // to simulate the restart of a device as the metrics measures the time delta
  // between a shutdown from login/lock screen and a following restart. By
  // calling the constructor of LockStateController, the restart of a device is
  // simulated and the call of UmaHistogramLongTimes is triggered if a previous
  // shutdown was initiated with ShutdownReason::LOGIN_SHUT_DOWN_BUTTON.
  void RestartDevice() {
    LockStateController(test_shutdown_controller_.get(), local_state_);
  }

  base::HistogramTester& histograms() { return histograms_; }

  std::unique_ptr<ShutdownController::ScopedResetterForTest>
      shutdown_controller_resetter_;
  std::unique_ptr<TestShutdownController> test_shutdown_controller_;
  raw_ptr<TestSessionStateAnimator, DanglingUntriaged> test_animator_ =
      nullptr;  // not owned

 private:
  // Histogram value verifier.
  base::HistogramTester histograms_;

  // To access the pref kLoginShutdownTimestampPrefName
  raw_ptr<PrefService> local_state_ = nullptr;
};

// Test the show menu and shutdown flow for non-Chrome-OS hardware that doesn't
// correctly report power button releases.  We should show menu the first
// time the button is pressed and shut down when it's pressed from the locked
// state.
TEST_F(LockStateControllerTest, LegacyShowMenuAndShutDown) {
  Initialize(ButtonType::LEGACY, LoginStatus::USER);

  ExpectUnlockedState("1");

  // We should request that the screen be locked immediately after seeing the
  // power button get pressed.
  PressPowerButton();

  EXPECT_TRUE(power_button_test_api_->IsMenuOpened());

  // We shouldn't progress towards the shutdown state, however.
  EXPECT_FALSE(lock_state_test_api_->shutdown_timer_is_running());

  ReleasePowerButton();

  // Hold the button again and check that we start shutting down.
  PressPowerButton();

  ExpectShutdownAnimationStarted("2");

  EXPECT_EQ(0, NumShutdownRequests());
  // Make sure a mouse move event won't show the cursor.
  GenerateMouseMoveEvent();
  EXPECT_FALSE(cursor_visible());

  EXPECT_TRUE(lock_state_test_api_->real_shutdown_timer_is_running());
  lock_state_test_api_->trigger_real_shutdown_timeout();
  EXPECT_EQ(1, NumShutdownRequests());
  // Shutdown was not initiated with reason LOGIN_SHUT_DOWN_BUTTON
  EXPECT_TRUE(IsDefaultValueLoginShutdownTimestamp());
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
  power_button_controller_->OnDisplayConfigurationChanged(outputs);
  PressPowerButton();
  EXPECT_FALSE(power_button_test_api_->IsMenuOpened());
  ReleasePowerButton();

  // When the screen brightness is 0% but the external display is still turned
  // on (indicating either docked mode or the user having manually decreased the
  // brightness to 0%), the power button should still be handled.
  external_display->set_current_mode(external_display->modes().back().get());
  power_button_controller_->OnDisplayConfigurationChanged(outputs);
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

class LockStateControllerAnimationTest
    : public LockStateControllerTest,
      public ::testing::WithParamInterface<bool> {
 public:
  void AdvanceOrAbort(SessionStateAnimator::AnimationSpeed speed) {
    if (GetParam()) {
      test_animator_->Advance(test_animator_->GetDuration(speed));
    } else {
      test_animator_->AbortAnimations(
          SessionStateAnimator::kAllNonRootContainersMask);
    }
  }

  // Switches to tablet mode for tests that want table mode power button
  // behavior, also sets other session related info to simulate being on a lock
  // screen with some other relevant user prefs.
  void PrepareSessionForUnlockAnimationInTabletModeTest() {
    power_button_controller_->OnDisplayTabletStateChanged(
        display::TabletState::kInTabletMode);
    // Advance mock clock to now. If we don't do this, PowerButtonController
    // will wrongly assume that we have accidental button presses due to all
    // timestamps zeroed.
    tick_clock_.SetNowTicks(base::TimeTicks::Now());

    Shell::Get()->session_controller()->SetSessionInfo(
        SessionInfo{.can_lock_screen = true,
                    .should_lock_screen_automatically = true,
                    .state = session_manager::SessionState::LOCKED});
  }
};

// Test the basic operation of the lock button.
TEST_P(LockStateControllerAnimationTest, LockButtonBasic) {
  // If we're logged in as a regular user, we should start the lock timer and
  // the pre-lock animation.
  Initialize(ButtonType::NORMAL, LoginStatus::USER);

  PressLockButton();
  ExpectPreLockAnimationStarted("1");
  AdvancePartially(SessionStateAnimator::ANIMATION_SPEED_UNDOABLE, 0.5f);
  ExpectPreLockAnimationRunning("2");

  // If the button is released immediately, we shouldn't lock the screen.
  ReleaseLockButton();

  ExpectPreLockAnimationCancel("3");
  AdvanceOrAbort(SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS);

  ExpectUnlockedState("4");
  EXPECT_FALSE(Shell::Get()->session_controller()->IsScreenLocked());

  // Press the button again and let the lock timeout fire.  We should request
  // that the screen be locked.
  PressLockButton();
  ExpectPreLockAnimationStarted("4");
  Advance(SessionStateAnimator::ANIMATION_SPEED_UNDOABLE);

  GetSessionControllerClient()->FlushForTest();
  EXPECT_TRUE(Shell::Get()->session_controller()->IsScreenLocked());

  // Pressing the lock button while we have a pending lock request shouldn't do
  // anything.
  ReleaseLockButton();
  PressLockButton();
  ExpectPreLockAnimationFinished("5");
  ReleaseLockButton();

  // Pressing the button also shouldn't do anything after the screen is locked.
  ExpectPostLockAnimationStarted("6");

  PressLockButton();
  ReleaseLockButton();
  ExpectPostLockAnimationStarted("7");

  AdvanceOrAbort(SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS);
  ExpectPostLockAnimationFinished("8");

  PressLockButton();
  ReleaseLockButton();
  ExpectPostLockAnimationFinished("9");
}

TEST_P(LockStateControllerAnimationTest,
       PowerButtonCancelsUnlockBeforeLockUIDestroyedInTabletMode) {
  PrepareSessionForUnlockAnimationInTabletModeTest();

  Shell::Get()->session_controller()->RunUnlockAnimation(
      base::BindLambdaForTesting([](bool aborted) { EXPECT_TRUE(aborted); }));

  ExpectUnlockBeforeUIDestroyedAnimationStarted("0");
  AdvancePartially(SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS, 0.5f);

  PressPowerButton();
  ReleasePowerButton();

  Advance(SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS);
  ExpectUnlockBeforeUIDestroyedAnimationFinished("1");
  EXPECT_TRUE(Shell::Get()->session_controller()->IsScreenLocked());
}

TEST_P(LockStateControllerAnimationTest,
       PowerButtonCancelsUnlockAfterLockUIDestroyedInTabletMode) {
  PrepareSessionForUnlockAnimationInTabletModeTest();

  Shell::Get()->session_controller()->RunUnlockAnimation(
      base::BindLambdaForTesting([](bool aborted) {
        EXPECT_TRUE(true);
        Shell::Get()->session_controller()->SetSessionInfo(
            SessionInfo{.can_lock_screen = true,
                        .should_lock_screen_automatically = true,
                        .state = session_manager::SessionState::ACTIVE});
      }));

  ExpectUnlockBeforeUIDestroyedAnimationStarted("0");
  Advance(SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS);
  ExpectUnlockBeforeUIDestroyedAnimationFinished("1");

  ExpectUnlockAfterUIDestroyedAnimationStarted("2");
  AdvancePartially(SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS, 0.5f);

  PressPowerButton();
  ReleasePowerButton();

  Advance(SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS);
  GetSessionControllerClient()->FlushForTest();
  EXPECT_TRUE(Shell::Get()->session_controller()->IsScreenLocked());
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
  // Shutdown was not initiated with reason LOGIN_SHUT_DOWN_BUTTON
  EXPECT_TRUE(IsDefaultValueLoginShutdownTimestamp());
}

// Test that we display the fast-close animation and shut down when we get an
// outside request to shut down (e.g. from the login or lock screen).
TEST_P(LockStateControllerAnimationTest, RequestShutdownFromLoginScreen) {
  Initialize(ButtonType::NORMAL, LoginStatus::NOT_LOGGED_IN);
  EXPECT_TRUE(IsDefaultValueLoginShutdownTimestamp());

  lock_state_controller_->RequestShutdown(
      ShutdownReason::LOGIN_SHUT_DOWN_BUTTON);

  // Shutdown was initiated with reason LOGIN_SHUT_DOWN_BUTTON
  EXPECT_FALSE(IsDefaultValueLoginShutdownTimestamp());
  ExpectShutdownAnimationStarted("1");
  AdvanceOrAbort(SessionStateAnimator::ANIMATION_SPEED_SHUTDOWN);

  GenerateMouseMoveEvent();
  EXPECT_FALSE(cursor_visible());

  EXPECT_EQ(0, NumShutdownRequests());
  EXPECT_TRUE(lock_state_test_api_->real_shutdown_timer_is_running());
  lock_state_test_api_->trigger_real_shutdown_timeout();
  EXPECT_EQ(1, NumShutdownRequests());
}

TEST_P(LockStateControllerAnimationTest, RequestShutdownFromLockScreen) {
  Initialize(ButtonType::NORMAL, LoginStatus::USER);

  LockScreen();

  AdvanceOrAbort(SessionStateAnimator::ANIMATION_SPEED_SHUTDOWN);
  ExpectPostLockAnimationFinished("1");
  EXPECT_TRUE(IsDefaultValueLoginShutdownTimestamp());

  lock_state_controller_->RequestShutdown(
      ShutdownReason::LOGIN_SHUT_DOWN_BUTTON);

  // Shutdown was initiated with reason LOGIN_SHUT_DOWN_BUTTON
  EXPECT_FALSE(IsDefaultValueLoginShutdownTimestamp());
  ExpectShutdownAnimationStarted("2");
  AdvanceOrAbort(SessionStateAnimator::ANIMATION_SPEED_SHUTDOWN);

  GenerateMouseMoveEvent();
  EXPECT_FALSE(cursor_visible());

  EXPECT_EQ(0, NumShutdownRequests());
  EXPECT_TRUE(lock_state_test_api_->real_shutdown_timer_is_running());
  lock_state_test_api_->trigger_real_shutdown_timeout();
  EXPECT_EQ(1, NumShutdownRequests());
}

// Test that histogram of time delta was recorded if a previous shutdown was
// initiated from login/lock screen.
TEST_F(LockStateControllerTest, RequestShutdownFromLoginScreenThenRestart) {
  Initialize(ButtonType::NORMAL, LoginStatus::NOT_LOGGED_IN);
  EXPECT_TRUE(IsDefaultValueLoginShutdownTimestamp());

  lock_state_controller_->RequestShutdown(
      ShutdownReason::LOGIN_SHUT_DOWN_BUTTON);

  // Shutdown was initiated with reason LOGIN_SHUT_DOWN_BUTTON
  EXPECT_FALSE(IsDefaultValueLoginShutdownTimestamp());

  GenerateMouseMoveEvent();
  EXPECT_FALSE(cursor_visible());

  EXPECT_EQ(0, NumShutdownRequests());
  EXPECT_TRUE(lock_state_test_api_->real_shutdown_timer_is_running());
  lock_state_test_api_->trigger_real_shutdown_timeout();
  EXPECT_EQ(1, NumShutdownRequests());

  // Simulate restarting device
  RestartDevice();
  histograms().ExpectTotalCount(kShelfShutdownConfirmationHistogramName, 1);
}

TEST_F(LockStateControllerTest, RequestShutdownFromLockScreenThenRestart) {
  Initialize(ButtonType::NORMAL, LoginStatus::USER);

  LockScreen();

  EXPECT_TRUE(IsDefaultValueLoginShutdownTimestamp());

  lock_state_controller_->RequestShutdown(
      ShutdownReason::LOGIN_SHUT_DOWN_BUTTON);

  // Shutdown was initiated with reason LOGIN_SHUT_DOWN_BUTTON
  EXPECT_FALSE(IsDefaultValueLoginShutdownTimestamp());

  GenerateMouseMoveEvent();
  EXPECT_FALSE(cursor_visible());

  EXPECT_EQ(0, NumShutdownRequests());
  EXPECT_TRUE(lock_state_test_api_->real_shutdown_timer_is_running());
  lock_state_test_api_->trigger_real_shutdown_timeout();
  EXPECT_EQ(1, NumShutdownRequests());

  // Simulate restarting device
  RestartDevice();
  histograms().ExpectTotalCount(kShelfShutdownConfirmationHistogramName, 1);
}

// Test that histogram of time delta was not recorded if a previous shutdown
// was not initiated from login/lock screen.
TEST_F(LockStateControllerTest, LegacyShowMenuAndShutDownThenRestart) {
  Initialize(ButtonType::LEGACY, LoginStatus::USER);

  ExpectUnlockedState("1");

  // We should request that the screen be locked immediately after seeing the
  // power button get pressed.
  PressPowerButton();

  EXPECT_TRUE(power_button_test_api_->IsMenuOpened());

  // We shouldn't progress towards the shutdown state, however.
  EXPECT_FALSE(lock_state_test_api_->shutdown_timer_is_running());

  ReleasePowerButton();

  // Hold the button again and check that we start shutting down.
  PressPowerButton();

  ExpectShutdownAnimationStarted("2");

  EXPECT_EQ(0, NumShutdownRequests());
  // Make sure a mouse move event won't show the cursor.
  GenerateMouseMoveEvent();
  EXPECT_FALSE(cursor_visible());

  EXPECT_TRUE(lock_state_test_api_->real_shutdown_timer_is_running());
  lock_state_test_api_->trigger_real_shutdown_timeout();
  EXPECT_EQ(1, NumShutdownRequests());
  // Shutdown was not initiated with reason LOGIN_SHUT_DOWN_BUTTON
  EXPECT_TRUE(IsDefaultValueLoginShutdownTimestamp());

  // Simulate restarting device
  RestartDevice();
  histograms().ExpectTotalCount(kShelfShutdownConfirmationHistogramName, 0);
}
// Test that hidden wallpaper appears and reverts correctly on lock/cancel.
TEST_P(LockStateControllerAnimationTest, TestHiddenWallpaperLockCancel) {
  Initialize(ButtonType::NORMAL, LoginStatus::USER);
  HideWallpaper();

  ExpectUnlockedState("1");
  PressLockButton();

  ExpectPreLockAnimationStarted("2");
  ExpectWallpaperIsShowing("3");

  // Forward only half way through.
  AdvancePartially(SessionStateAnimator::ANIMATION_SPEED_UNDOABLE, 0.5f);

  // Release the button before the lock timer fires.
  ReleaseLockButton();
  ExpectPreLockAnimationCancel("4");
  ExpectWallpaperIsHiding("5");

  AdvanceOrAbort(SessionStateAnimator::ANIMATION_SPEED_UNDO_MOVE_WINDOWS);

  // When the CancelPrelockAnimation sequence finishes it queues up a
  // restore wallpaper visibility sequence when the wallpaper is hidden.
  ExpectRestoringWallpaperVisibility("6");

  AdvanceOrAbort(SessionStateAnimator::ANIMATION_SPEED_IMMEDIATE);

  ExpectUnlockedState("6");
}

// Test that hidden wallpaper appears and revers correctly on lock/unlock.
TEST_P(LockStateControllerAnimationTest, TestHiddenWallpaperLockUnlock) {
  Initialize(ButtonType::NORMAL, LoginStatus::USER);
  HideWallpaper();

  ExpectUnlockedState("1");

  // Press the lock button and check that the lock timer is started and that we
  // start lifting the non-screen-locker containers.
  PressLockButton();

  ExpectPreLockAnimationStarted("2");
  ExpectWallpaperIsShowing("3");

  Advance(SessionStateAnimator::ANIMATION_SPEED_UNDOABLE);

  ExpectPreLockAnimationFinished("4");

  LockScreen();

  ReleaseLockButton();

  ExpectPostLockAnimationStarted("5");
  AdvanceOrAbort(SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS);
  ExpectPostLockAnimationFinished("6");

  ExpectLockedState("7");

  SuccessfulAuthentication(nullptr);

  ExpectUnlockBeforeUIDestroyedAnimationStarted("8");
  AdvanceOrAbort(SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS);
  ExpectUnlockBeforeUIDestroyedAnimationFinished("9");

  UnlockScreen();

  ExpectUnlockAfterUIDestroyedAnimationStarted("10");
  ExpectWallpaperIsHiding("11");

  AdvanceOrAbort(SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS);

  // When the StartUnlockAnimationAfterUIDestroyed sequence finishes it queues
  // up a restore wallpaper visibility sequence when the wallpaper is hidden.
  ExpectRestoringWallpaperVisibility("12");

  AdvanceOrAbort(SessionStateAnimator::ANIMATION_SPEED_IMMEDIATE);

  ExpectUnlockAfterUIDestroyedAnimationFinished("13");

  ExpectUnlockedState("14");
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

  ExpectShutdownAnimationStarted("1");
  AdvancePartially(SessionStateAnimator::ANIMATION_SPEED_SHUTDOWN, 0.5f);
  // Release the power button before the shutdown timer fires.
  ReleasePowerButton();
  EXPECT_FALSE(lock_state_test_api_->shutdown_timer_is_running());
  ExpectShutdownAnimationCancel("2");

  power_button_controller_->DismissMenu();
  EXPECT_FALSE(power_button_test_api_->IsMenuOpened());

  // Press the button again and make the shutdown timeout fire this time.
  // Check that we start the timer for actually requesting the shutdown.
  PressPowerButton();
  ASSERT_TRUE(power_button_test_api_->TriggerPreShutdownTimeout());
  EXPECT_TRUE(lock_state_test_api_->shutdown_timer_is_running());

  Advance(SessionStateAnimator::ANIMATION_SPEED_SHUTDOWN);
  ExpectShutdownAnimationFinished("3");
  lock_state_test_api_->trigger_shutdown_timeout();

  EXPECT_TRUE(lock_state_test_api_->real_shutdown_timer_is_running());
  EXPECT_EQ(0, NumShutdownRequests());

  // When the timeout fires, we should request a shutdown.
  lock_state_test_api_->trigger_real_shutdown_timeout();
  EXPECT_EQ(1, NumShutdownRequests());
  // Shutdown was not initiated with reason LOGIN_SHUT_DOWN_BUTTON
  EXPECT_TRUE(IsDefaultValueLoginShutdownTimestamp());
}

TEST_P(LockStateControllerAnimationTest, CancelShouldResetWallpaperBlur) {
  Initialize(ButtonType::NORMAL, LoginStatus::USER);

  ExpectUnlockedState("1");

  auto* wallpaper_view = Shell::Get()
                             ->GetPrimaryRootWindowController()
                             ->wallpaper_widget_controller()
                             ->wallpaper_view();

  // Enter Overview and verify wallpaper properties.
  EnterOverview();
  EXPECT_EQ(wallpaper_constants::kClear, wallpaper_view->blur_sigma());

  // Start lock animation and verify wallpaper properties.
  PressLockButton();
  ExpectPreLockAnimationStarted("2");
  EXPECT_EQ(wallpaper_constants::kLockLoginBlur, wallpaper_view->blur_sigma());

  // Cancel lock animation.
  AdvancePartially(SessionStateAnimator::ANIMATION_SPEED_UNDOABLE, 0.5f);
  ReleaseLockButton();
  ExpectPreLockAnimationCancel("3");
  AdvanceOrAbort(SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS);
  ExpectUnlockedState("4");

  // Verify wallpaper blur are restored to overview's.
  EXPECT_EQ(wallpaper_constants::kClear, wallpaper_view->blur_sigma());
}

INSTANTIATE_TEST_SUITE_P(LockStateControllerAnimation,
                         LockStateControllerAnimationTest,
                         /*complete or abort=*/::testing::Bool());

class LockStateControllerMockTimeTest : public PowerButtonTestBase {
 public:
  LockStateControllerMockTimeTest()
      : PowerButtonTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  LockStateControllerMockTimeTest(const LockStateControllerMockTimeTest&) =
      delete;
  LockStateControllerMockTimeTest& operator=(
      const LockStateControllerMockTimeTest&) = delete;
  ~LockStateControllerMockTimeTest() override = default;

  void Advance(const base::TimeDelta& delta) {
    task_environment()->FastForwardBy(delta);
  }

  void SetUp() override {
    PowerButtonTestBase::SetUp();
    InitPowerButtonControllerMembers(
        chromeos::PowerManagerClient::TabletMode::UNSUPPORTED);
  }
};

class TestLayerCopyAnimator final : public LayerCopyAnimator {
 public:
  TestLayerCopyAnimator(aura::Window* window, base::OnceClosure callback)
      : LayerCopyAnimator(window), callback_(std::move(callback)) {}
  TestLayerCopyAnimator(const TestLayerCopyAnimator&) = delete;
  TestLayerCopyAnimator& operator=(const TestLayerCopyAnimator&) = delete;
  ~TestLayerCopyAnimator() override = default;

  // LayerCopyAnimator:
  void OnLayerCopied(std::unique_ptr<ui::Layer> new_layer) override {
    // Move the callback first because the object may be deleted.
    auto callback = std::move(callback_);
    LayerCopyAnimator::OnLayerCopied(std::move(new_layer));
    std::move(callback).Run();
  }

 private:
  base::OnceClosure callback_;
};

TEST_F(LockStateControllerMockTimeTest, LockWithoutAnimation) {
  Initialize(ButtonType::LEGACY, LoginStatus::USER);
  EXPECT_FALSE(Shell::Get()->session_controller()->IsScreenLocked());
  auto* shelf_container = Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                                              kShellWindowId_ShelfContainer);
  auto* lock_container =
      Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                          kShellWindowId_LockScreenContainersContainer);

  base::RunLoop loop;
  base::RepeatingClosure done_closure =
      base::BarrierClosure(2, loop.QuitClosure());

  auto callback = [&]() { done_closure.Run(); };

  new TestLayerCopyAnimator(shelf_container,
                            base::BindLambdaForTesting(callback));
  new TestLayerCopyAnimator(lock_container,
                            base::BindLambdaForTesting(callback));

  lock_state_controller_->LockWithoutAnimation();
  EXPECT_TRUE(lock_state_controller_->animating_lock_for_test());
  loop.Run();
  EXPECT_FALSE(lock_state_controller_->animating_lock_for_test());
  EXPECT_TRUE(Shell::Get()->session_controller()->IsScreenLocked());
}

class LockStateControllerInformedRestoreTest : public LockStateControllerTest {
 public:
  LockStateControllerInformedRestoreTest() = default;
  LockStateControllerInformedRestoreTest(
      const LockStateControllerInformedRestoreTest&) = delete;
  LockStateControllerInformedRestoreTest& operator=(
      const LockStateControllerInformedRestoreTest&) = delete;
  ~LockStateControllerInformedRestoreTest() override = default;

  // LockStateControllerTest:
  void SetUp() override {
    LockStateControllerTest::SetUp();

    CHECK(temp_dir_.CreateUniqueTempDir());
    file_path_ = temp_dir_.GetPath().AppendASCII("test_informed_restore.png");
    SetInformedRestoreImagePathForTest(file_path_);
    Initialize(ButtonType::NORMAL, LoginStatus::USER);
  }

  void TearDown() override {
    SetInformedRestoreImagePathForTest(base::FilePath());
    LockStateControllerTest::TearDown();
  }

  void RequestShutdownWithoutFailTimer() {
    base::RunLoop run_loop;
    lock_state_test_api_->set_informed_restore_image_callback(
        run_loop.QuitClosure());
    lock_state_test_api_->disable_screenshot_timeout_for_test(true);
    lock_state_controller_->RequestShutdown(
        ShutdownReason::TRAY_SHUT_DOWN_BUTTON);
    run_loop.Run();
  }

  // Checks that the informed restore image was taken and saved at `file_path`
  // on disk successfully.
  void VerifyInformedRestoreImageOnDisk() {
    EXPECT_TRUE(base::PathExists(file_path()));
    int64_t file_size = 0;
    ASSERT_TRUE(base::GetFileSize(file_path(), &file_size));
    EXPECT_GT(file_size, 0);
  }

  const base::FilePath& file_path() const { return file_path_; }

 private:
  base::ScopedAllowBlockingForTesting allow_blocking_;
  base::ScopedTempDir temp_dir_;
  base::FilePath file_path_;
  base::test::ScopedFeatureList scoped_feature_list_{features::kForestFeature};
};

// Tests that a informed restore image is taken when there are windows open.
TEST_F(LockStateControllerInformedRestoreTest, ShutdownWithWindows) {
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  base::HistogramTester histogram_tester;

  RequestShutdownWithoutFailTimer();
  // The informed restore image was taken and not empty.
  VerifyInformedRestoreImageOnDisk();
  EXPECT_THAT(histogram_tester.GetAllSamples(kScreenshotOnShutdownStatus),
              testing::ElementsAre(
                  base::Bucket(ScreenshotOnShutdownStatus::kSucceeded, 1)));

  auto* local_state = Shell::Get()->local_state();
  // Informed restore screenshot related durations were recorded.
  const base::TimeDelta screenshot_taken_duration =
      local_state->GetTimeDelta(prefs::kInformedRestoreScreenshotTakenDuration);
  EXPECT_FALSE(screenshot_taken_duration.is_zero());
  const base::TimeDelta screenshot_encode_and_save_duration =
      local_state->GetTimeDelta(
          prefs::kInformedRestoreScreenshotEncodeAndSaveDuration);
  EXPECT_FALSE(screenshot_encode_and_save_duration.is_zero());
}

// Tests that no informed restore image is taken when there are no windows
// opened and the existing informed restore image should be deleted.
TEST_F(LockStateControllerInformedRestoreTest, ShutdownWithoutWindows) {
  // Create an empty file to simulate an old informed restore image.
  ASSERT_TRUE(base::WriteFile(file_path(), ""));

  base::HistogramTester histogram_tester;
  RequestShutdownWithoutFailTimer();
  EXPECT_THAT(histogram_tester.GetAllSamples(kScreenshotOnShutdownStatus),
              testing::ElementsAre(base::Bucket(
                  ScreenshotOnShutdownStatus::kFailedWithNoWindows, 1)));

  // Existing informed restore image was deleted.
  EXPECT_FALSE(lock_state_test_api_->mirror_wallpaper_layer());
  EXPECT_FALSE(base::PathExists(file_path()));
}

TEST_F(LockStateControllerInformedRestoreTest, ShutdownInOverview) {
  // Create an empty file to simulate an old informed restore image.
  ASSERT_TRUE(base::WriteFile(file_path(), ""));

  base::HistogramTester histogram_tester;
  // Create a window and enter the overview before requesting shutdown.
  CreateTestWindow();
  EnterOverview();

  RequestShutdownWithoutFailTimer();
  EXPECT_THAT(histogram_tester.GetAllSamples(kScreenshotOnShutdownStatus),
              testing::ElementsAre(base::Bucket(
                  ScreenshotOnShutdownStatus::kFailedInOverview, 1)));
  // The informed restore image should not be taken if it is in overview when
  // shutting down. The existing informed restore image should be deleted as
  // well.
  EXPECT_FALSE(lock_state_test_api_->mirror_wallpaper_layer());
  EXPECT_FALSE(base::PathExists(file_path()));
}

TEST_F(LockStateControllerInformedRestoreTest, ShutdownInGuest) {
  SimulateUserLogin("foo@example.com", user_manager::UserType::kGuest);

  // Create an empty file to simulate an old informed restore image.
  ASSERT_TRUE(base::WriteFile(file_path(), ""));

  base::HistogramTester histogram_tester;
  CreateTestWindow();
  ASSERT_TRUE(Shell::Get()->session_controller()->IsUserGuest());

  // Request shutdown while in guest mode.
  RequestShutdownWithoutFailTimer();
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kScreenshotOnShutdownStatus),
      testing::ElementsAre(base::Bucket(
          ScreenshotOnShutdownStatus::kFailedInGuestOrPublicUserSession, 1)));
  // The informed restore image should not be taken if it is in the guest
  // session. The existing informed restore image should be deleted as well.
  EXPECT_FALSE(lock_state_test_api_->mirror_wallpaper_layer());
  EXPECT_FALSE(base::PathExists(file_path()));
}

TEST_F(LockStateControllerInformedRestoreTest, ShutdownInLockScreen) {
  // Create an empty file to simulate an old informed restore image.
  ASSERT_TRUE(base::WriteFile(file_path(), ""));

  base::HistogramTester histogram_tester;
  // Create a window and go the lock screen before requesting shutdown.
  CreateTestWindowInShellWithId(0);
  GetSessionControllerClient()->LockScreen();
  EXPECT_TRUE(Shell::Get()->session_controller()->IsScreenLocked());

  RequestShutdownWithoutFailTimer();
  EXPECT_THAT(histogram_tester.GetAllSamples(kScreenshotOnShutdownStatus),
              testing::ElementsAre(base::Bucket(
                  ScreenshotOnShutdownStatus::kFailedInLockScreen, 1)));
  // The informed restore image should not be taken if it is in the lock screen.
  // The existing informed restore image should be deleted as well.
  EXPECT_FALSE(lock_state_test_api_->mirror_wallpaper_layer());
  EXPECT_FALSE(base::PathExists(file_path()));
}

TEST_F(LockStateControllerInformedRestoreTest, ShutdownInHomeLauncher) {
  // Create an empty file to simulate an old informed restore image.
  ASSERT_TRUE(base::WriteFile(file_path(), ""));

  base::HistogramTester histogram_tester;
  // Create a window and go to the home launcher page before requesting
  // shutdown.
  std::unique_ptr<aura::Window> window(CreateTestWindow());
  TabletModeControllerTestApi().EnterTabletMode();
  auto* app_list_controller = Shell::Get()->app_list_controller();
  app_list_controller->GoHome(GetPrimaryDisplay().id());
  ASSERT_TRUE(app_list_controller->IsHomeScreenVisible());
  EXPECT_TRUE(WindowState::Get(window.get())->IsMinimized());

  RequestShutdownWithoutFailTimer();
  EXPECT_THAT(histogram_tester.GetAllSamples(kScreenshotOnShutdownStatus),
              testing::ElementsAre(base::Bucket(
                  ScreenshotOnShutdownStatus::kFailedInHomeLauncher, 1)));

  // The informed restore image should not be taken if it is in the home
  // launcher page when shutting down. The existing image should be deleted as
  // well.
  EXPECT_FALSE(lock_state_test_api_->mirror_wallpaper_layer());
  EXPECT_FALSE(base::PathExists(file_path()));
}

TEST_F(LockStateControllerInformedRestoreTest, PinnedState) {
  // Create an empty file to simulate an old informed restore image.
  ASSERT_TRUE(base::WriteFile(file_path(), ""));

  base::HistogramTester histogram_tester;
  // Create and pin a window before requesting shutdown.
  std::unique_ptr<aura::Window> pinned_window = CreateAppWindow();
  wm::ActivateWindow(pinned_window.get());
  window_util::PinWindow(pinned_window.get(), /*trusted=*/false);

  RequestShutdownWithoutFailTimer();
  EXPECT_THAT(histogram_tester.GetAllSamples(kScreenshotOnShutdownStatus),
              testing::ElementsAre(base::Bucket(
                  ScreenshotOnShutdownStatus::kFailedInPinnedMode, 1)));
  // The informed restore image should not be taken when it is in pinned state.
  // The existing image should be deleted as well.
  EXPECT_FALSE(lock_state_test_api_->mirror_wallpaper_layer());
  EXPECT_FALSE(base::PathExists(file_path()));
}

TEST_F(LockStateControllerInformedRestoreTest, AllWindowsMinimized) {
  // Create an empty file to simulate an old informed restore image.
  ASSERT_TRUE(base::WriteFile(file_path(), ""));

  base::HistogramTester histogram_tester;
  std::unique_ptr<aura::Window> window1(CreateTestWindow());
  std::unique_ptr<aura::Window> window2(CreateTestWindow());
  WindowState::Get(window1.get())->Minimize();
  WindowState::Get(window2.get())->Minimize();

  RequestShutdownWithoutFailTimer();
  EXPECT_THAT(histogram_tester.GetAllSamples(kScreenshotOnShutdownStatus),
              testing::ElementsAre(base::Bucket(
                  ScreenshotOnShutdownStatus::kFailedWithNoWindows, 1)));
  // The informed restore image should not be taken if all the windows inside
  // the active desk are minimized. The existing image should be deleted as
  // well.
  EXPECT_FALSE(lock_state_test_api_->mirror_wallpaper_layer());
  EXPECT_FALSE(base::PathExists(file_path()));
}

// Tests that the informed restore image should be taken with only the floated
// window.
TEST_F(LockStateControllerInformedRestoreTest, ShutdownWithFloatWindow) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<aura::Window> floated_window = CreateAppWindow();
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(WindowState::Get(floated_window.get())->IsFloated());

  RequestShutdownWithoutFailTimer();
  EXPECT_THAT(histogram_tester.GetAllSamples(kScreenshotOnShutdownStatus),
              testing::ElementsAre(
                  base::Bucket(ScreenshotOnShutdownStatus::kSucceeded, 1)));
  // The informed restore image was taken and not empty with the float window
  // only.
  VerifyInformedRestoreImageOnDisk();
}

// Tests that the informed restore image should be taken with only the always on
// top window.
TEST_F(LockStateControllerInformedRestoreTest, ShutdownWithAlwaysOnTopWindow) {
  base::HistogramTester histogram_tester;
  aura::Window* top_container = Shell::GetContainer(
      Shell::GetPrimaryRootWindow(), kShellWindowId_AlwaysOnTopContainer);
  std::unique_ptr<aura::Window> window_always_on_top(
      aura::test::CreateTestWindowWithId(1, top_container));

  RequestShutdownWithoutFailTimer();
  EXPECT_THAT(histogram_tester.GetAllSamples(kScreenshotOnShutdownStatus),
              testing::ElementsAre(
                  base::Bucket(ScreenshotOnShutdownStatus::kSucceeded, 1)));
  // The informed restore image was taken and not empty with the always on top
  // window only.
  VerifyInformedRestoreImageOnDisk();
}

TEST_F(LockStateControllerInformedRestoreTest, TakeScreenshotTimeout) {
  // Create an empty file to simulate an old informed restore image.
  ASSERT_TRUE(base::WriteFile(file_path(), ""));

  base::HistogramTester histogram_tester;
  std::unique_ptr<aura::Window> window(CreateTestWindow());
  base::RunLoop run_loop;
  lock_state_test_api_->set_informed_restore_image_callback(
      run_loop.QuitClosure());
  lock_state_controller_->RequestShutdown(
      ShutdownReason::TRAY_SHUT_DOWN_BUTTON);

  // Fire the screenshot timeout before taking the screenshot completes. Then no
  // screenshot should be saved, the existing one should be deleted as well and
  // the shutdown process should be triggered directly.
  lock_state_test_api_->trigger_take_screenshot_timeout();
  run_loop.Run();
  EXPECT_FALSE(base::PathExists(file_path()));
  EXPECT_TRUE(lock_state_test_api_->real_shutdown_timer_is_running());
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kScreenshotOnShutdownStatus),
      testing::ElementsAre(base::Bucket(
          ScreenshotOnShutdownStatus::kFailedOnTakingScreenshotTimeout, 1)));
}

TEST_F(LockStateControllerInformedRestoreTest, CancelShutdown) {
  // Create an empty file to simulate an old informed restore image.
  ASSERT_TRUE(base::WriteFile(file_path(), ""));
  std::unique_ptr<aura::Window> window(CreateTestWindow());
  base::RunLoop run_loop;
  lock_state_test_api_->set_informed_restore_image_callback(
      run_loop.QuitClosure());
  lock_state_controller_->RequestCancelableShutdown(
      ShutdownReason::TRAY_SHUT_DOWN_BUTTON);

  // The shutdown should be cancelable and the existing informed restore image
  // should be deleted as the shutdown was canceled.
  EXPECT_TRUE(lock_state_controller_->MaybeCancelShutdownAnimation());
  run_loop.Run();
  EXPECT_FALSE(base::PathExists(file_path()));
}

}  // namespace ash
