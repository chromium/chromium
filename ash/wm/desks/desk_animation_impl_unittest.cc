// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_animation_impl.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_histogram_enums.h"
#include "ash/wm/desks/root_window_desk_switch_animator_test_api.h"
#include "base/barrier_closure.h"
#include "base/test/scoped_feature_list.h"

namespace ash {

using DeskActivationAnimationTest = AshTestBase;

// Tests that there is no crash when ending a swipe animation before the
// starting screenshot has been taken. Regression test for
// https://crbug.com/1148607.
TEST_F(DeskActivationAnimationTest, EndSwipeBeforeStartingScreenshot) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kEnhancedDeskAnimations);

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
  UpdateDisplay("600x600,601+0-2000x600");

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kEnhancedDeskAnimations);

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

  auto wait_ending_screenshot_taken = [](DeskActivationAnimation* animation) {
    base::RunLoop run_loop;
    auto* desk_switch_animator =
        animation->GetDeskSwitchAnimatorAtIndexForTesting(0);
    RootWindowDeskSwitchAnimatorTestApi(desk_switch_animator)
        .SetOnEndingScreenshotTakenCallback(run_loop.QuitClosure());
    run_loop.Run();
  };

  wait_ending_screenshot_taken(&animation);
  EXPECT_EQ(0, animation.visible_desk_changes());

  const int touchpad_swipe_length_for_desk_change =
      RootWindowDeskSwitchAnimator::kTouchpadSwipeLengthForDeskChange;

  // Swipe enough so that our third and fourth desk screenshots are taken, and
  // then swipe so that the fourth desk is fully shown. There should be 3
  // visible desk changes in total.
  animation.UpdateSwipeAnimation(-touchpad_swipe_length_for_desk_change);
  wait_ending_screenshot_taken(&animation);

  animation.UpdateSwipeAnimation(-touchpad_swipe_length_for_desk_change);
  wait_ending_screenshot_taken(&animation);

  animation.UpdateSwipeAnimation(-3 * touchpad_swipe_length_for_desk_change);
  EXPECT_EQ(3, animation.visible_desk_changes());

  // Do some minor swipes to the right. We should still be focused on the last
  // desk so the visible desk change count remains the same.
  animation.UpdateSwipeAnimation(touchpad_swipe_length_for_desk_change / 10);
  animation.UpdateSwipeAnimation(touchpad_swipe_length_for_desk_change / 10);
  EXPECT_EQ(3, animation.visible_desk_changes());

  // Do two full swipes to the right, and then two full swipes to the left. Test
  // that the desk change count has increased by four.
  animation.UpdateSwipeAnimation(touchpad_swipe_length_for_desk_change);
  animation.UpdateSwipeAnimation(touchpad_swipe_length_for_desk_change);
  animation.UpdateSwipeAnimation(-touchpad_swipe_length_for_desk_change);
  animation.UpdateSwipeAnimation(-touchpad_swipe_length_for_desk_change);
  EXPECT_EQ(7, animation.visible_desk_changes());
}

}  // namespace ash
