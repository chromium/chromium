// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_animation_impl.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_histogram_enums.h"
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

}  // namespace ash
