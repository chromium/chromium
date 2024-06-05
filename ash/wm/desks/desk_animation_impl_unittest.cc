// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_animation_impl.h"

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desks_constants.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_histogram_enums.h"
#include "ash/wm/desks/desks_test_util.h"
#include "ash/wm/desks/overview_desk_bar_view.h"
#include "ash/wm/desks/root_window_desk_switch_animator_test_api.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_grid_test_api.h"
#include "ash/wm/overview/overview_test_util.h"
#include "base/barrier_closure.h"
#include "base/test/scoped_feature_list.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"

namespace ash {

using DeskActivationAnimationTest = AshTestBase;

// Tests that there is no crash when ending a swipe animation before the
// starting screenshot has been taken. Regression test for
// https://crbug.com/1148607.
TEST_F(DeskActivationAnimationTest, EndSwipeBeforeStartingScreenshot) {
  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);

  DeskActivationAnimation animation(desks_controller, 0, 1,
                                    DesksSwitchSource::kDeskSwitchTouchpad,
                                    /*update_window_activation=*/false);
  animation.set_skip_notify_controller_on_animation_finished_for_testing(true);
  animation.Launch();
  animation.UpdateSwipeAnimation(10);
  animation.EndSwipeAnimation();
}

// Tests that there is no crash when swiping with external displays. Regression
// test for https://crbug.com/1154868.
TEST_F(DeskActivationAnimationTest, UpdateSwipeNewScreenshotCrash) {
  // Crash is only reproducible on different resolution widths and easier to
  // repro when the widths differ by a lot.
  UpdateDisplay("700x600,601+0-2000x600");

  // Crash repro requires three desks.
  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);

  DeskActivationAnimation animation(desks_controller, 1, 2,
                                    DesksSwitchSource::kDeskSwitchTouchpad,
                                    /*update_window_activation=*/false);
  animation.set_skip_notify_controller_on_animation_finished_for_testing(true);
  animation.Launch();

  // Wait until all ending screenshots have been taken before swiping.
  size_t num_animators = 2u;
  base::RunLoop run_loop;
  base::RepeatingClosure end_screenshot_callback =
      base::BarrierClosure(num_animators, run_loop.QuitClosure());
  for (size_t i = 0; i < num_animators; ++i) {
    auto* desk_switch_animator =
        animation.GetDeskSwitchAnimatorAtIndexForTesting(i);
    RootWindowDeskSwitchAnimatorTestApi(desk_switch_animator)
        .SetOnEndingScreenshotTakenCallback(end_screenshot_callback);
  }
  run_loop.Run();

  // Swipe in a way which would have caused a crash using the old algorithm. See
  // bug for more details.
  animation.UpdateSwipeAnimation(-20);
  animation.UpdateSwipeAnimation(10);
  animation.EndSwipeAnimation();
}

// Tests that visible desk change count updates as expected. It is used higher
// up for metrics collection, but the logic is in this class.
TEST_F(DeskActivationAnimationTest, VisibleDeskChangeCount) {
  // Add three desks for a total of four.
  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);

  DeskActivationAnimation animation(desks_controller, 0, 1,
                                    DesksSwitchSource::kDeskSwitchTouchpad,
                                    /*update_window_activation=*/false);
  animation.set_skip_notify_controller_on_animation_finished_for_testing(true);
  animation.Launch();

  WaitUntilEndingScreenshotTaken(&animation);
  EXPECT_EQ(0, animation.visible_desk_changes());

  // Swipe enough so that our third and fourth desk screenshots are taken, and
  // then swipe so that the fourth desk is fully shown. There should be 3
  // visible desk changes in total.
  animation.UpdateSwipeAnimation(-kTouchpadSwipeLengthForDeskChange);
  WaitUntilEndingScreenshotTaken(&animation);

  animation.UpdateSwipeAnimation(-kTouchpadSwipeLengthForDeskChange);
  WaitUntilEndingScreenshotTaken(&animation);

  animation.UpdateSwipeAnimation(-3 * kTouchpadSwipeLengthForDeskChange);
  EXPECT_EQ(3, animation.visible_desk_changes());

  // Do some minor swipes to the right. We should still be focused on the last
  // desk so the visible desk change count remains the same.
  animation.UpdateSwipeAnimation(kTouchpadSwipeLengthForDeskChange / 10);
  animation.UpdateSwipeAnimation(kTouchpadSwipeLengthForDeskChange / 10);
  EXPECT_EQ(3, animation.visible_desk_changes());

  // Do two full swipes to the right, and then two full swipes to the left. Test
  // that the desk change count has increased by four.
  animation.UpdateSwipeAnimation(kTouchpadSwipeLengthForDeskChange);
  animation.UpdateSwipeAnimation(kTouchpadSwipeLengthForDeskChange);
  animation.UpdateSwipeAnimation(-kTouchpadSwipeLengthForDeskChange);
  animation.UpdateSwipeAnimation(-kTouchpadSwipeLengthForDeskChange);
  EXPECT_EQ(7, animation.visible_desk_changes());
}

// Tests that closing windows during a desk animation does not cause a crash.
TEST_F(DeskActivationAnimationTest, CloseWindowDuringAnimation) {
  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);

  std::unique_ptr<aura::Window> window = CreateAppWindow(gfx::Rect(250, 100));

  DeskActivationAnimation animation(desks_controller, 0, 1,
                                    DesksSwitchSource::kDeskSwitchTouchpad,
                                    /*update_window_activation=*/false);
  animation.set_skip_notify_controller_on_animation_finished_for_testing(true);
  animation.Launch();

  window.reset();
  WaitUntilEndingScreenshotTaken(&animation);
}

// Tests that if a fast swipe is detected, we will still wait for the ending
// screenshot to be taken and animated to.
TEST_F(DeskActivationAnimationTest, AnimatingAfterFastSwipe) {
  ui::ScopedAnimationDurationScaleMode animation_scale(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);

  DeskActivationAnimation animation(desks_controller, 0, 1,
                                    DesksSwitchSource::kDeskSwitchTouchpad,
                                    /*update_window_activation=*/false);
  animation.set_skip_notify_controller_on_animation_finished_for_testing(true);
  animation.Launch();

  // Wait until the starting screenshot is taken, otherwise on swipe end, we
  // will advance to the next desk with no animation.
  base::RunLoop run_loop;
  auto* desk_switch_animator =
      animation.GetDeskSwitchAnimatorAtIndexForTesting(0);
  // Verify the continuous animation is triggered.
  auto animator_test_api =
      RootWindowDeskSwitchAnimatorTestApi(desk_switch_animator);
  EXPECT_EQ(animator_test_api.GetAnimatorType(),
            DeskSwitchAnimationType::kContinuousAnimation);

  animator_test_api.SetOnStartingScreenshotTakenCallback(
      run_loop.QuitClosure());
  run_loop.Run();

  // Update a bit and then end swipe. Modify `last_start_or_replace_time_` to
  // ensure that a fast swipe is registered since different build configurations
  // may run slower than others.
  animation.UpdateSwipeAnimation(10);
  animation.last_start_or_replace_time_ = base::TimeTicks::Now();
  animation.EndSwipeAnimation();

  ASSERT_FALSE(desk_switch_animator->ending_desk_screenshot_taken());
  WaitUntilEndingScreenshotTaken(&animation);

  // Tests that there is an animation after the ending screenshots have been
  // taken.
  EXPECT_TRUE(desk_switch_animator->GetAnimationLayerForTesting()
                  ->GetAnimator()
                  ->is_animating());
}

// Tests that there is no use-after-free when starting and ending a swipe before
// the screenshots are taken. Regression test for https://crbug.com/1276203.
TEST_F(DeskActivationAnimationTest, StartAndEndSwipeBeforeScreenshotsTaken) {
  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);

  // Start and finish a animation without waiting for the screenshots to be
  // taken.
  desks_controller->StartSwipeAnimation(/*move_left=*/false);
  ASSERT_TRUE(desks_controller->animation());

  desks_controller->UpdateSwipeAnimation(10);
  desks_controller->EndSwipeAnimation();
  EXPECT_FALSE(desks_controller->animation());
}

class OverviewDeskNavigationTest : public AshTestBase {
 public:
  OverviewDeskNavigationTest()
      : scoped_feature_list_(features::kOverviewDeskNavigation) {}
  OverviewDeskNavigationTest(const OverviewDeskNavigationTest&) = delete;
  OverviewDeskNavigationTest& operator=(const OverviewDeskNavigationTest&) =
      delete;
  ~OverviewDeskNavigationTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    // Begin each test with two desks.
    NewDesk();
    auto* desks_controller = DesksController::Get();
    ASSERT_EQ(2u, desks_controller->desks().size());
    EXPECT_TRUE(desks_controller->GetDeskAtIndex(0)->is_active());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests when we switch between desks in overview that the desk switch animation
// doesn't exit overview.
TEST_F(OverviewDeskNavigationTest, SwitchDesksWithoutExitingOverview) {
  ui::ScopedAnimationDurationScaleMode animation_scale(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  EnterOverview();
  auto* overview_controller = Shell::Get()->overview_controller();
  ASSERT_TRUE(overview_controller->InOverviewSession());

  // Switch to the next desk while in overview and wait for the desk switch
  // animation.
  DeskSwitchAnimationWaiter waiter;
  auto* desks_controller = DesksController::Get();
  desks_controller->ActivateAdjacentDesk(
      /*going_left=*/false, DesksSwitchSource::kDeskSwitchShortcut);
  // Verify the continuous animation is triggered.
  auto* desk_switch_animator =
      desks_controller->animation()->GetDeskSwitchAnimatorAtIndexForTesting(0);
  auto animator_test_api =
      RootWindowDeskSwitchAnimatorTestApi(desk_switch_animator);
  EXPECT_EQ(animator_test_api.GetAnimatorType(),
            DeskSwitchAnimationType::kContinuousAnimation);
  waiter.Wait();

  // Verify that we have switched desks and are still in overview.
  EXPECT_TRUE(desks_controller->GetDeskAtIndex(1)->is_active());
  ASSERT_TRUE(overview_controller->InOverviewSession());
}

// Tests that clicking on the miniview preserves the previously expected
// functionality to exit overview.
TEST_F(OverviewDeskNavigationTest, ClickingMiniViewExitsOverview) {
  // Enter overview mode, and expect the desk bar is shown with exactly two
  // desks mini views.
  EnterOverview();
  auto* overview_controller = Shell::Get()->overview_controller();
  ASSERT_TRUE(overview_controller->InOverviewSession());
  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);
  ASSERT_EQ(2u, desks_bar_view->mini_views().size());

  // Activate the second desk by clicking on its mini view and wait for the desk
  // switch animation.
  auto* desks_controller = DesksController::Get();
  const Desk* desk_2 = desks_controller->GetDeskAtIndex(1);
  EXPECT_EQ(0, desks_controller->GetActiveDeskIndex());
  auto* mini_view = desks_bar_view->mini_views().back().get();
  EXPECT_EQ(desk_2, mini_view->desk());
  DeskSwitchAnimationWaiter waiter;
  LeftClickOn(mini_view);
  // Verify the quick animation is triggered.
  auto* desk_switch_animator =
      desks_controller->animation()->GetDeskSwitchAnimatorAtIndexForTesting(0);
  auto animator_test_api =
      RootWindowDeskSwitchAnimatorTestApi(desk_switch_animator);
  EXPECT_EQ(animator_test_api.GetAnimatorType(),
            DeskSwitchAnimationType::kQuickAnimation);
  waiter.Wait();

  // Expect that the second desk is now active, and overview mode exited.
  EXPECT_EQ(desk_2, desks_controller->active_desk());
  EXPECT_FALSE(overview_controller->InOverviewSession());
}

// Tests that overview is not exited when a short desk swipe is detected during
// a desk activation animation.
TEST_F(OverviewDeskNavigationTest, ShortSwipeStaysInOverview) {
  ui::ScopedAnimationDurationScaleMode animation_scale(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  EnterOverview();
  auto* overview_controller = Shell::Get()->overview_controller();
  ASSERT_TRUE(overview_controller->InOverviewSession());
  const gfx::Rect initial_overview_grid_bounds =
      OverviewGridTestApi(Shell::GetPrimaryRootWindow()).bounds();

  // Start a swipe animation, but only swipe to show 1/10 of the next desk. This
  // will cause the animation to animate back to the starting desk.
  auto* desks_controller = DesksController::Get();
  desks_controller->StartSwipeAnimation(/*move_left=*/false);
  DeskActivationAnimation* animation =
      static_cast<DeskActivationAnimation*>(desks_controller->animation());
  ASSERT_TRUE(animation);
  animation->set_skip_notify_controller_on_animation_finished_for_testing(true);
  // This ensures a fast swipe is not triggered.
  animation->last_start_or_replace_time_ =
      base::TimeTicks::Now() - base::Seconds(2);
  desks_controller->UpdateSwipeAnimation(-kTouchpadSwipeLengthForDeskChange /
                                         10);
  WaitUntilEndingScreenshotTaken(animation);

  // Checks that as part of the animation, we have already activated the
  // expected ending desk.
  EXPECT_TRUE(desks_controller->GetDeskAtIndex(1)->is_active());
  ASSERT_TRUE(overview_controller->InOverviewSession());

  // End the swipe animation and wait for the desk activation animation to
  // finish. This is required to prevent flakiness.
  base::RunLoop run_loop;
  animation->on_animation_finished_callback_for_testing_ =
      run_loop.QuitClosure();
  desks_controller->EndSwipeAnimation();
  run_loop.Run();

  // Verify that the original active desk is once again activated (meaning that
  // we animated back to it), and are still in overview.
  EXPECT_TRUE(desks_controller->GetDeskAtIndex(0)->is_active());
  ASSERT_TRUE(overview_controller->InOverviewSession());

  // Verify that the grid bounds haven't changed, especially since we
  // specifically use `OverviewEnterExitType::kImmediateEnter` to enter overview
  // in these cases.
  EXPECT_EQ(initial_overview_grid_bounds,
            OverviewGridTestApi(Shell::GetPrimaryRootWindow()).bounds());
}

// Tests that inputs to exit overview are ignored during the desk switch
// animation.
TEST_F(OverviewDeskNavigationTest, CannotToggleOverviewDuringAnimation) {
  EnterOverview();
  auto* overview_controller = Shell::Get()->overview_controller();
  ASSERT_TRUE(overview_controller->InOverviewSession());

  // Start a swipe animation.
  auto* desks_controller = DesksController::Get();
  desks_controller->StartSwipeAnimation(/*move_left=*/false);
  DeskActivationAnimation* animation =
      static_cast<DeskActivationAnimation*>(desks_controller->animation());
  ASSERT_TRUE(animation);
  animation->set_skip_notify_controller_on_animation_finished_for_testing(true);
  desks_controller->UpdateSwipeAnimation(-kTouchpadSwipeLengthForDeskChange);
  WaitUntilEndingScreenshotTaken(animation);

  // Attempt to exit overview during the animation. This should do nothing.
  ExitOverview();

  // End the swipe animation and verify that we have animated to the correct
  // desk (not the original active desk) and that the attempt to exit overview
  // during the animation was unsuccessful.
  desks_controller->EndSwipeAnimation();
  EXPECT_TRUE(desks_controller->GetDeskAtIndex(1)->is_active());
  ASSERT_TRUE(overview_controller->InOverviewSession());
}

}  // namespace ash
