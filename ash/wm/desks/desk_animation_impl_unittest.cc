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

}  // namespace ash
