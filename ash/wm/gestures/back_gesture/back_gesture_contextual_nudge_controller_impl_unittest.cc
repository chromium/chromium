// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/gestures/back_gesture/back_gesture_contextual_nudge_controller_impl.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/controls/contextual_tooltip.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test_shell_delegate.h"
#include "ash/wm/gestures/back_gesture/back_gesture_contextual_nudge.h"
#include "ash/wm/gestures/back_gesture/back_gesture_event_handler.h"
#include "ash/wm/gestures/back_gesture/test_back_gesture_contextual_nudge_delegate.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "build/build_config.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

constexpr char kUser1Email[] = "user1@test.com";
constexpr char kUser2Email[] = "user2@test.com";

// Distance that swiping from left edge to let the affordance achieve
// activated state.
static constexpr int kSwipingDistanceForGoingBack = 80;

}  // namespace

class BackGestureContextualNudgeControllerTest : public NoSessionAshTestBase {
 public:
  explicit BackGestureContextualNudgeControllerTest(bool can_go_back = true)
      : can_go_back_(can_go_back) {
    scoped_feature_list_.InitAndEnableFeature(
        features::kHideShelfControlsInTabletMode);
  }
  BackGestureContextualNudgeControllerTest(
      base::test::TaskEnvironment::TimeSource time,
      bool can_go_back = true)
      : NoSessionAshTestBase(time), can_go_back_(can_go_back) {
    scoped_feature_list_.InitAndEnableFeature(
        features::kHideShelfControlsInTabletMode);
  }
  ~BackGestureContextualNudgeControllerTest() override = default;

  // NoSessionAshTestBase:
  void SetUp() override {
    std::unique_ptr<TestShellDelegate> delegate;
    if (!can_go_back_) {
      delegate = std::make_unique<TestShellDelegate>();
      delegate->SetCanGoBack(false);
    }
    NoSessionAshTestBase::SetUp(std::move(delegate));

    GetSessionControllerClient()->AddUserSession(kUser1Email);
    GetSessionControllerClient()->AddUserSession(kUser2Email);

    // Simulate login of user 1.
    SwitchActiveUser(kUser1Email);
    GetSessionControllerClient()->SetSessionState(
        session_manager::SessionState::ACTIVE);

    // Is only allowed after the drag handle nudge has been shown - simulate
    // drag handle so back gesture gets enabled.
    contextual_tooltip::OverrideClockForTesting(&test_clock_);
    test_clock_.Advance(base::Seconds(360));
    contextual_tooltip::HandleNudgeShown(
        user1_pref_service(), contextual_tooltip::TooltipType::kInAppToHome);
    contextual_tooltip::HandleNudgeShown(
        user2_pref_service(), contextual_tooltip::TooltipType::kInAppToHome);
    test_clock_.Advance(
        contextual_tooltip::kMinIntervalBetweenBackAndDragHandleNudge * 2);

    // Enter tablet mode.
    TabletModeControllerTestApi().EnterTabletMode();
  }

  void TearDown() override {
    contextual_tooltip::ClearClockOverrideForTesting();
    NoSessionAshTestBase::TearDown();
  }

  void SwitchActiveUser(const std::string& email) {
    GetSessionControllerClient()->SwitchActiveUser(
        AccountId::FromUserEmail(email));
  }

  void WaitNudgeAnimationDone() {
    while (nudge()) {
      base::RunLoop run_loop;
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(100));
      run_loop.Run();
    }
  }

  // Set nudge as shown for testing. Only after nudge is counted as shown,
  // the nudge dismiss metrics can be correctly logged. This is to simulate
  // something happens in the middle of nudge animation to dismiss the nudge.
  void SetNudgeShownForTesting() {
    if (nudge())
      nudge()->SetNudgeShownForTesting();
  }

  PrefService* user1_pref_service() {
    return Shell::Get()->session_controller()->GetUserPrefServiceForUser(
        AccountId::FromUserEmail(kUser1Email));
  }

  PrefService* user2_pref_service() {
    return Shell::Get()->session_controller()->GetUserPrefServiceForUser(
        AccountId::FromUserEmail(kUser2Email));
  }

  BackGestureContextualNudgeControllerImpl* nudge_controller() {
    return Shell::Get()
        ->back_gesture_event_handler()
        ->nudge_controller_for_testing();
  }

  BackGestureContextualNudge* nudge() { return nudge_controller()->nudge(); }

  base::SimpleTestClock* clock() { return &test_clock_; }

  // Generates a scroll sequence that will create a back gesture.
  void GenerateBackSequence() {
    GetEventGenerator()->GestureScrollSequence(
        gfx::Point(0, 100), gfx::Point(kSwipingDistanceForGoingBack + 10, 100),
        base::Milliseconds(100), 3);
  }

 private:
  bool can_go_back_;
  base::SimpleTestClock test_clock_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

class BackGestureContextualNudgeControllerTestCantGoBack
    : public BackGestureContextualNudgeControllerTest {
 public:
  BackGestureContextualNudgeControllerTestCantGoBack()
      : BackGestureContextualNudgeControllerTest(false) {}
};

class BackGestureContextualNudgeControllerTestA11yPrefs
    : public BackGestureContextualNudgeControllerTest,
      public ::testing::WithParamInterface<std::string> {
 public:
  BackGestureContextualNudgeControllerTestA11yPrefs()
      : BackGestureContextualNudgeControllerTest(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  BackGestureContextualNudgeControllerTestA11yPrefs(
      const BackGestureContextualNudgeControllerTestA11yPrefs&) = delete;
  BackGestureContextualNudgeControllerTestA11yPrefs& operator=(
      const BackGestureContextualNudgeControllerTestA11yPrefs&) = delete;
  ~BackGestureContextualNudgeControllerTestA11yPrefs() override = default;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    BackGestureContextualNudgeControllerTestA11yPrefs,
    testing::Values(prefs::kAccessibilityAutoclickEnabled,
                    prefs::kAccessibilitySpokenFeedbackEnabled,
                    prefs::kAccessibilitySwitchAccessEnabled));

// Tests the timing when BackGestureContextualNudgeControllerImpl should monitor
// window activation changes.
TEST_F(BackGestureContextualNudgeControllerTest, MonitorWindowsTest) {
  // Only monitor windows in tablet mode.
  EXPECT_TRUE(nudge_controller()->is_monitoring_windows());
  TabletModeControllerTestApi tablet_mode_api;
  tablet_mode_api.LeaveTabletMode();
  EXPECT_FALSE(nudge_controller()->is_monitoring_windows());
  tablet_mode_api.EnterTabletMode();
  EXPECT_TRUE(nudge_controller()->is_monitoring_windows());

  // Only monitor windows in active user session.
  GetSessionControllerClient()->LockScreen();
  EXPECT_FALSE(nudge_controller()->is_monitoring_windows());
  GetSessionControllerClient()->UnlockScreen();
  EXPECT_TRUE(nudge_controller()->is_monitoring_windows());

  // Exit tablet mode for kUser1Email.
  tablet_mode_api.LeaveTabletMode();
  EXPECT_FALSE(nudge_controller()->is_monitoring_windows());
  // Then enter tablet mode for kUserEmail2.
  SwitchActiveUser(kUser2Email);
  tablet_mode_api.EnterTabletMode();
  EXPECT_TRUE(nudge_controller()->is_monitoring_windows());
}

// Tests the activation of another window will cancel the in-waiting or
// in-progress nudge animation.
TEST_F(BackGestureContextualNudgeControllerTest,
       ActivationCancelAnimationTest) {
  ui::ScopedAnimationDurationScaleMode non_zero(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  EXPECT_FALSE(nudge());

  EXPECT_TRUE(contextual_tooltip::ShouldShowNudge(
      user1_pref_service(), contextual_tooltip::TooltipType::kBackGesture,
      nullptr));

  std::unique_ptr<aura::Window> window1 = CreateTestWindow();
  // If nudge() is true, it indicates that it's currently in animation.
  EXPECT_TRUE(nudge());

  // At this moment, change window activation should cancel the previous nudge
  // showup animation on |window1|, and start show nudge on |window2|.
  std::unique_ptr<aura::Window> window2 = CreateTestWindow();
  EXPECT_FALSE(nudge()->ShouldNudgeCountAsShown());
  EXPECT_TRUE(contextual_tooltip::ShouldShowNudge(
      user1_pref_service(), contextual_tooltip::TooltipType::kBackGesture,
      nullptr));

  // Wait until nudge animation is finished.
  WaitNudgeAnimationDone();
  EXPECT_FALSE(contextual_tooltip::ShouldShowNudge(
      user1_pref_service(), contextual_tooltip::TooltipType::kBackGesture,
      nullptr));
}

// Test that ending tablet mode will cancel in-waiting or in-progress nudge
// animation.
TEST_F(BackGestureContextualNudgeControllerTest,
       EndTabletModeCancelAnimationTest) {
  ui::ScopedAnimationDurationScaleMode non_zero(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  EXPECT_FALSE(nudge());

  EXPECT_TRUE(contextual_tooltip::ShouldShowNudge(
      user1_pref_service(), contextual_tooltip::TooltipType::kBackGesture,
      nullptr));

  std::unique_ptr<aura::Window> window = CreateTestWindow();
  EXPECT_TRUE(nudge());

  TabletModeControllerTestApi().LeaveTabletMode();
  WaitNudgeAnimationDone();
  EXPECT_TRUE(contextual_tooltip::ShouldShowNudge(
      user1_pref_service(), contextual_tooltip::TooltipType::kBackGesture,
      nullptr));
}

// Do not show nudge ui on window that can't perform "go back" operation.
TEST_F(BackGestureContextualNudgeControllerTestCantGoBack, WindowTest) {
  EXPECT_FALSE(nudge());
  EXPECT_TRUE(contextual_tooltip::ShouldShowNudge(
      user1_pref_service(), contextual_tooltip::TooltipType::kBackGesture,
      nullptr));

  std::unique_ptr<aura::Window> window = CreateTestWindow();
  EXPECT_FALSE(nudge());
  EXPECT_TRUE(contextual_tooltip::ShouldShowNudge(
      user1_pref_service(), contextual_tooltip::TooltipType::kBackGesture,
      nullptr));
}

TEST_F(BackGestureContextualNudgeControllerTest, ShowNudgeOnExistingWindow) {
  TabletModeControllerTestApi tablet_mode_api;
  tablet_mode_api.LeaveTabletMode();
  EXPECT_FALSE(nudge());
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  EXPECT_FALSE(nudge());

  tablet_mode_api.EnterTabletMode();
  EXPECT_TRUE(nudge());
}

// Do not show nudge ui on window if shelf drag handle nudge should be shown at
// the same time.
TEST_F(BackGestureContextualNudgeControllerTest, NotShownWithDragHandleNudge) {
  TabletModeControllerTestApi tablet_mode_api;
  tablet_mode_api.LeaveTabletMode();

  // Advance the contextual tooltip manager's clock so drag handle nudge can be
  // shown again (note that the drag handle nudge is first shown during test
  // setup to enable the back gesture nudge).
  clock()->Advance(contextual_tooltip::kMinInterval);

  tablet_mode_api.EnterTabletMode();
  ASSERT_TRUE(contextual_tooltip::ShouldShowNudge(
      user1_pref_service(), contextual_tooltip::TooltipType::kInAppToHome,
      nullptr));

  std::unique_ptr<aura::Window> window = CreateTestWindow();
  EXPECT_FALSE(nudge());
  EXPECT_FALSE(contextual_tooltip::ShouldShowNudge(
      user1_pref_service(), contextual_tooltip::TooltipType::kBackGesture,
      nullptr));
}

// Verifies that back gesture nudge will be shown after drag handle nudge if
// enough time passes, even if the user does not leave tablet mode.
TEST_F(BackGestureContextualNudgeControllerTest,
       CanBeShownAfterDragHandleNudgeWithoutLeavingTabletMode) {
  ui::ScopedAnimationDurationScaleMode non_zero(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  TabletModeControllerTestApi tablet_mode_api;
  tablet_mode_api.LeaveTabletMode();

  // Advance the contextual tooltip manager's clock so drag handle nudge can be
  // shown again (note that the drag handle nudge is first shown during test
  // setup to enable the back gesture nudge).
  clock()->Advance(contextual_tooltip::kMinInterval);

  tablet_mode_api.EnterTabletMode();
  ASSERT_TRUE(contextual_tooltip::ShouldShowNudge(
      user1_pref_service(), contextual_tooltip::TooltipType::kInAppToHome,
      nullptr));

  std::unique_ptr<aura::Window> window = CreateTestWindow();
  EXPECT_FALSE(nudge());
  EXPECT_FALSE(contextual_tooltip::ShouldShowNudge(
      user1_pref_service(), contextual_tooltip::TooltipType::kBackGesture,
      nullptr));

  contextual_tooltip::HandleNudgeShown(
      user1_pref_service(), contextual_tooltip::TooltipType::kInAppToHome);
  clock()->Advance(
      contextual_tooltip::kMinIntervalBetweenBackAndDragHandleNudge);
  ASSERT_TRUE(nudge_controller()->auto_show_timer_for_testing()->IsRunning());
  nudge_controller()->auto_show_timer_for_testing()->FireNow();

  std::unique_ptr<aura::Window> window_2 = CreateTestWindow();
  EXPECT_TRUE(nudge());
}

// Verifies that back gesture nudge will be shown again if enough time passes,
// even it the user does not leave tablet mode.
TEST_F(BackGestureContextualNudgeControllerTest,
       CanBeShownAfterRenteringTabletMode) {
  ui::ScopedAnimationDurationScaleMode non_zero(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Verify the nudge is created and wait until nudge animation is shown.
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  EXPECT_TRUE(nudge());
  SetNudgeShownForTesting();

  EXPECT_FALSE(contextual_tooltip::ShouldShowNudge(
      user1_pref_service(), contextual_tooltip::TooltipType::kBackGesture,
      nullptr));

  // Reenter tablet mode, and verify the nudge can be shown again after the
  // nudge interval passes.
  TabletModeControllerTestApi tablet_mode_api;
  tablet_mode_api.LeaveTabletMode();
  tablet_mode_api.EnterTabletMode();
  clock()->Advance(contextual_tooltip::kMinInterval);
  contextual_tooltip::HandleNudgeShown(
      user1_pref_service(), contextual_tooltip::TooltipType::kInAppToHome);

  clock()->Advance(
      contextual_tooltip::kMinIntervalBetweenBackAndDragHandleNudge);
  ASSERT_TRUE(nudge_controller()->auto_show_timer_for_testing()->IsRunning());
  nudge_controller()->auto_show_timer_for_testing()->FireNow();

  std::unique_ptr<aura::Window> window_2 = CreateTestWindow();
  EXPECT_TRUE(nudge());
}

// Back gesture metrics should be recorded after performing gesture.
TEST_F(BackGestureContextualNudgeControllerTest, GesturePerformedMetricTest) {
  ui::ScopedAnimationDurationScaleMode non_zero(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  // Verify the nudge is created and wait until nudge animation is shown.
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  EXPECT_TRUE(nudge());
  SetNudgeShownForTesting();

  GenerateBackSequence();
}

TEST_P(BackGestureContextualNudgeControllerTestA11yPrefs, TimeoutMetricsTest) {
  ui::ScopedAnimationDurationScaleMode non_zero(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  EXPECT_TRUE(nudge());
  WaitNudgeAnimationDone();
  EXPECT_FALSE(nudge());
}

TEST_P(BackGestureContextualNudgeControllerTestA11yPrefs,
       LogDismissMetricsAfterNudgeShown) {
  ui::ScopedAnimationDurationScaleMode non_zero(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  EXPECT_TRUE(nudge());
  // Before nudge is still waiting to be shown, exit tablet mode. The nudge will
  // be dismissed immediately.
  TabletModeControllerTestApi tablet_mode_api;
  tablet_mode_api.LeaveTabletMode();
  EXPECT_FALSE(nudge());

  tablet_mode_api.EnterTabletMode();
  std::unique_ptr<aura::Window> window2 = CreateTestWindow();
  EXPECT_TRUE(nudge());
  SetNudgeShownForTesting();

  // Exit tablet mode in the middle of the animation, test the dismissmal
  // metrics should be correctly logged.
  tablet_mode_api.LeaveTabletMode();
  WaitNudgeAnimationDone();
}

// Back Gesture Nudge should be hidden when shelf controls are enabled.
TEST_P(BackGestureContextualNudgeControllerTestA11yPrefs,
       HideNudgesForShelfControls) {
  SCOPED_TRACE(testing::Message() << "Pref=" << GetParam());
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  EXPECT_TRUE(nudge());
  SetNudgeShownForTesting();

  // Turn on accessibility settings to enable shelf controls.
  Shell::Get()
      ->session_controller()
      ->GetLastActiveUserPrefService()
      ->SetBoolean(GetParam(), true);
  EXPECT_FALSE(nudge());

  TabletModeControllerTestApi tablet_mode_api;
  tablet_mode_api.LeaveTabletMode();
}

// Back Gesture Nudge should be disabled when shelf controls are enabled.
TEST_P(BackGestureContextualNudgeControllerTestA11yPrefs,
       DisableNudgesForShelfControls) {
  SCOPED_TRACE(testing::Message() << "Pref=" << GetParam());

  // Turn on accessibility settings to enable shelf controls.
  Shell::Get()
      ->session_controller()
      ->GetLastActiveUserPrefService()
      ->SetBoolean(GetParam(), true);
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  EXPECT_FALSE(nudge());
}

}  // namespace ash
