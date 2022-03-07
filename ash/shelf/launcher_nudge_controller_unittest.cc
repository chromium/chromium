// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/launcher_nudge_controller.h"

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/home_button.h"
#include "ash/shelf/shelf_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/json/values_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"

namespace ash {

class TestNudgeAnimationObserver : public HomeButton::NudgeAnimationObserver {
 public:
  explicit TestNudgeAnimationObserver(HomeButton* home_button)
      : home_button_(home_button) {
    home_button_->AddNudgeAnimationObserverForTest(this);
  }
  ~TestNudgeAnimationObserver() override {
    home_button_->RemoveNudgeAnimationObserverForTest(this);
  }

  // HomeButton::NudgeAnimationObserver:
  void NudgeAnimationStarted(HomeButton* home_button) override {
    if (home_button != home_button_)
      return;

    ++started_animation_count_;
  }
  void NudgeAnimationEnded(HomeButton* home_button) override {
    if (home_button != home_button_)
      return;

    DCHECK_EQ(started_animation_count_, ended_animation_count_ + 1);
    ++ended_animation_count_;
    run_loop_.Quit();
  }

  void WaitUntilAnimationEnded() {
    DCHECK_GE(started_animation_count_, ended_animation_count_);
    if (started_animation_count_ == ended_animation_count_)
      return;

    // Block the test to wait until the animation ended.
    run_loop_.Run();
  }

  // Returns the number of finished animation on this home_button_.
  int GetShownCount() const { return ended_animation_count_; }

 private:
  base::RunLoop run_loop_;
  HomeButton* const home_button_;

  // Counts the number of started/ended animations.
  int started_animation_count_ = 0;
  int ended_animation_count_ = 0;
};

class LauncherNudgeControllerTest : public AshTestBase {
 public:
  LauncherNudgeControllerTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    scoped_feature_list_.InitWithFeatures({features::kShelfLauncherNudge}, {});
  }
  LauncherNudgeControllerTest(const LauncherNudgeControllerTest&) = delete;
  LauncherNudgeControllerTest& operator=(const LauncherNudgeControllerTest&) =
      delete;
  ~LauncherNudgeControllerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    nudge_controller_ =
        Shell::Get()->shelf_controller()->launcher_nudge_controller();
    nudge_controller_->SetClockForTesting(
        task_environment()->GetMockClock(),
        task_environment()->GetMockTickClock());
  }

  // Advances the mock clock in the task environment and wait until it is idle.
  // Note that AdvanceClock is used here instead of FastForwardBy because
  // `delay` used in test cases are too long for FastForwardBy to process and
  // will cause timeout running the tests.
  void AdvanceClock(base::TimeDelta delay) {
    task_environment()->AdvanceClock(delay);
    task_environment()->RunUntilIdle();
  }

  int GetNudgeShownCount() {
    PrefService* pref_service =
        Shell::Get()->session_controller()->GetLastActiveUserPrefService();
    return LauncherNudgeController::GetShownCount(pref_service);
  }

  LauncherNudgeController* nudge_controller_;
  std::unique_ptr<TestNudgeAnimationObserver> observer_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(LauncherNudgeControllerTest, DisableNudgeForGuestSession) {
  // Simulate a guest user logging in, where the session is ephemeral.
  SimulateGuestLogin();

  // Do not show the nudge in the guest session.
  EXPECT_FALSE(nudge_controller_->IsRecheckTimerRunningForTesting());
  EXPECT_EQ(0, GetNudgeShownCount());
}

TEST_F(LauncherNudgeControllerTest, DisableNudgeForExistingUser) {
  // Simulate a existing user logging in.
  SimulateUserLogin("user@gmail.com");
  ASSERT_FALSE(Shell::Get()->session_controller()->IsUserFirstLogin());

  // Do not show the nudge to an existing user.
  EXPECT_FALSE(nudge_controller_->IsRecheckTimerRunningForTesting());
  EXPECT_EQ(0, GetNudgeShownCount());
}

TEST_F(LauncherNudgeControllerTest, BasicTest) {
  // Set the animation duration mode to non-zero for the launcher nudge
  // animation to actually run in the tests.
  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  SimulateNewUserFirstLogin("user@gmail.com");
  ASSERT_TRUE(Shell::Get()->session_controller()->IsUserFirstLogin());
  EXPECT_EQ(0, GetNudgeShownCount());

  for (int i = 0; i < LauncherNudgeController::kMaxShownCount; ++i) {
    // After the new user logged in or the nudge was just shown, the recheck
    // timer should be running to wait for a certain amount of time to check if
    // the nudge should be shown later.
    EXPECT_TRUE(nudge_controller_->IsRecheckTimerRunningForTesting());
    if (i == 0) {
      // The first waiting interval is set different from the following ones.
      AdvanceClock(nudge_controller_->GetNudgeInterval(/*is_first_time=*/true));
    } else {
      AdvanceClock(
          nudge_controller_->GetNudgeInterval(/*is_first_time=*/false));
    }
    // The nudge animation should be shown and the shown count should be
    // incremented.
    EXPECT_EQ(i + 1, GetNudgeShownCount());
  }

  // After showing `kMaxShownCount` times of the nudge animation, stop showing
  // nudges to the users.
  EXPECT_FALSE(nudge_controller_->IsRecheckTimerRunningForTesting());
}

TEST_F(LauncherNudgeControllerTest, StopShowingNudgeAfterLauncherIsOpened) {
  // Set the animation duration mode to non-zero for the launcher nudge
  // animation to actually run in the tests.
  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  SimulateNewUserFirstLogin("user@gmail.com");
  EXPECT_EQ(0, GetNudgeShownCount());

  EXPECT_TRUE(nudge_controller_->IsRecheckTimerRunningForTesting());
  AdvanceClock(nudge_controller_->GetNudgeInterval(/*is_first_time=*/true));
  EXPECT_EQ(1, GetNudgeShownCount());

  // Toggle the app list to show. Open fullscreen to avoid showing the expand
  // arrow button, because the animation for that button causes an MSAN
  // use-of-uninitialized-value. This workaround can be removed when
  // ProductivityLauncher is the default, since it does not have an expand
  // button. See https://crbug.com/926038.
  Shell::Get()->app_list_controller()->ToggleAppList(
      display::Screen::GetScreen()->GetPrimaryDisplay().id(),
      AppListShowSource::kShelfButtonFullscreen, base::TimeTicks());
  ASSERT_TRUE(Shell::Get()->app_list_controller()->IsVisible());
  AdvanceClock(nudge_controller_->GetNudgeInterval(/*is_first_time=*/false));

  // If the launcher app list has been opened, stop showing the nudge to the
  // user.
  EXPECT_FALSE(nudge_controller_->IsRecheckTimerRunningForTesting());
  EXPECT_EQ(1, GetNudgeShownCount());
}

TEST_F(LauncherNudgeControllerTest, DoNotShowNudgeInTabletMode) {
  // Set the animation duration mode to non-zero for the launcher nudge
  // animation to actually run in the tests.
  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  SimulateNewUserFirstLogin("user@gmail.com");
  EXPECT_EQ(0, GetNudgeShownCount());

  // Change to the tablet mode. Do not show the nudge in tablet mode.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  AdvanceClock(nudge_controller_->GetNudgeInterval(/*is_first_time=*/true));
  EXPECT_EQ(0, GetNudgeShownCount());

  // Return to the clamshell mode.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  EXPECT_EQ(0, GetNudgeShownCount());

  // The nudge has to wait for `kMinIntervalAfterHomeButtonAppears` after
  // changing back to clamshell mode.
  AdvanceClock(LauncherNudgeController::kMinIntervalAfterHomeButtonAppears);
  EXPECT_EQ(1, GetNudgeShownCount());
}

TEST_F(LauncherNudgeControllerTest, ShowNudgeOnDisplayWhereCursorIsOn) {
  // Set the animation duration mode to non-zero for the launcher nudge
  // animation to actually run in the tests.
  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  SimulateNewUserFirstLogin("user@gmail.com");
  EXPECT_EQ(0, GetNudgeShownCount());

  // Create 2 displays for the test.
  UpdateDisplay("800x600,800x600");
  ASSERT_EQ(2u, Shell::Get()->display_manager()->GetNumDisplays());

  HomeButton* primary_home_button =
      LauncherNudgeController::GetHomeButtonForDisplay(
          GetPrimaryDisplay().id());
  HomeButton* secondary_home_button =
      LauncherNudgeController::GetHomeButtonForDisplay(
          GetSecondaryDisplay().id());

  TestNudgeAnimationObserver waiter_primary(primary_home_button);
  TestNudgeAnimationObserver waiter_secondary(secondary_home_button);

  // Move the cursor to primary display. The nudge after advancing the clock
  // should be shown on the primary display.
  Shell::Get()->cursor_manager()->SetDisplay(GetPrimaryDisplay());
  AdvanceClock(nudge_controller_->GetNudgeInterval(/*is_first_time=*/true));

  // Wait until the animation ends to count the finished animation.
  waiter_primary.WaitUntilAnimationEnded();
  EXPECT_EQ(1, GetNudgeShownCount());
  EXPECT_EQ(1, waiter_primary.GetShownCount());
  EXPECT_EQ(0, waiter_secondary.GetShownCount());

  // Move the cursor to primary display. The nudge after advancing the clock
  // should be shown on the primary display.
  Shell::Get()->cursor_manager()->SetDisplay(GetSecondaryDisplay());
  AdvanceClock(nudge_controller_->GetNudgeInterval(/*is_first_time=*/false));

  // Wait until the animation ends to count the finished animation.
  waiter_secondary.WaitUntilAnimationEnded();
  EXPECT_EQ(2, GetNudgeShownCount());
  EXPECT_EQ(1, waiter_primary.GetShownCount());
  EXPECT_EQ(1, waiter_secondary.GetShownCount());
}

TEST_F(LauncherNudgeControllerTest,
       WaitUntilHomeButtonStaysLongEnoughToShowNudge) {
  // Set the animation duration mode to non-zero for the launcher nudge
  // animation to actually run in the tests.
  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // New user logs in.
  SimulateNewUserFirstLogin("user@gmail.com");
  EXPECT_EQ(0, GetNudgeShownCount());
  base::TimeDelta small_delta = base::Seconds(10);

  // Log out right before the nudge should be shown.
  AdvanceClock(nudge_controller_->GetNudgeInterval(/*is_first_time=*/true) -
               small_delta);
  ClearLogin();

  // Log in again.
  SimulateUserLogin("user@gmail.com");
  AdvanceClock(small_delta);

  // Even if the nudge interval has passed since the first log in, the nudge has
  // to wait `kMinIntervalAfterHomeButtonAppears` amount of time since the
  // recent login to be shown.
  EXPECT_EQ(0, GetNudgeShownCount());
  AdvanceClock(LauncherNudgeController::kMinIntervalAfterHomeButtonAppears -
               small_delta);
  EXPECT_EQ(1, GetNudgeShownCount());

  // Change to the tablet mode.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  AdvanceClock(nudge_controller_->GetNudgeInterval(/*is_first_time=*/true) -
               small_delta);

  // Return to the clamshell mode.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  AdvanceClock(small_delta);

  // Even if the nudge interval has passed since the last nudge shown, the nudge
  // has to wait `kMinIntervalAfterHomeButtonAppears` amount of time since the
  // last change to clamshell mode to be shown.
  EXPECT_EQ(1, GetNudgeShownCount());
  AdvanceClock(LauncherNudgeController::kMinIntervalAfterHomeButtonAppears -
               small_delta);
  EXPECT_EQ(2, GetNudgeShownCount());
}

}  // namespace ash
