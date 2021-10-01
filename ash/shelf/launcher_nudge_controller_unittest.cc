// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/launcher_nudge_controller.h"

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/home_button.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_controller.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/json/values_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"

namespace ash {

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

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(LauncherNudgeControllerTest, DisableNudgeForExistingUser) {
  // Simulate a existing user logging in.
  SimulateUserLogin("user@gmail.com");
  ASSERT_FALSE(Shell::Get()->session_controller()->IsUserFirstLogin());

  // Do not show the nudge to an existing user
  EXPECT_FALSE(nudge_controller_->IsRecheckTimerRunningForTesting());
  EXPECT_EQ(0, GetNudgeShownCount());
}

TEST_F(LauncherNudgeControllerTest, BasicTest) {
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
  SimulateNewUserFirstLogin("user@gmail.com");
  EXPECT_EQ(0, GetNudgeShownCount());

  EXPECT_TRUE(nudge_controller_->IsRecheckTimerRunningForTesting());
  AdvanceClock(nudge_controller_->GetNudgeInterval(/*is_first_time=*/true));
  EXPECT_EQ(1, GetNudgeShownCount());

  // Toggle the app list to show
  Shell::Get()->app_list_controller()->ToggleAppList(
      display::Screen::GetScreen()->GetPrimaryDisplay().id(),
      AppListShowSource::kShelfButton, base::TimeTicks());
  ASSERT_TRUE(Shell::Get()->app_list_controller()->IsVisible());
  AdvanceClock(nudge_controller_->GetNudgeInterval(/*is_first_time=*/false));

  // If the launcher app list has been opened, stop showing the nudge to the
  // user.
  EXPECT_FALSE(nudge_controller_->IsRecheckTimerRunningForTesting());
  EXPECT_EQ(1, GetNudgeShownCount());
}

TEST_F(LauncherNudgeControllerTest, DoNotShowNudgeInTabletMode) {
  SimulateNewUserFirstLogin("user@gmail.com");
  EXPECT_EQ(0, GetNudgeShownCount());

  // Change to the tablet mode. Do not show the nudge in tablet mode.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  AdvanceClock(nudge_controller_->GetNudgeInterval(/*is_first_time=*/true));
  EXPECT_EQ(0, GetNudgeShownCount());

  // Return to the clamshell mode. If the time has passed long enough since the
  // last nudge shown, show the nudge to the user immediately.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  EXPECT_EQ(1, GetNudgeShownCount());
}

}  // namespace ash