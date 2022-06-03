// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_bubble_apps_page.h"

#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/constants/ash_features.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/test_utils.h"
#include "ui/views/controls/scroll_view.h"

namespace ash {
namespace {

class AppListBubbleAppsPageTest : public AshTestBase {
 private:
  base::test::ScopedFeatureList features_{features::kProductivityLauncher};
};

TEST_F(AppListBubbleAppsPageTest, SlideViewIntoPositionCleansUpLayers) {
  // Open the app list without animation.
  ASSERT_EQ(ui::ScopedAnimationDurationScaleMode::duration_multiplier(),
            ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  auto* helper = GetAppListTestHelper();
  helper->AddRecentApps(5);
  helper->AddAppItems(5);
  helper->ShowAppList();

  // Recent apps view starts without a layer.
  auto* recent_apps = helper->GetBubbleRecentAppsView();
  ASSERT_FALSE(recent_apps->layer());

  // Trigger a slide animation.
  ui::ScopedAnimationDurationScaleMode duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  auto* apps_page = helper->GetBubbleAppsPage();
  constexpr int kVerticalOffset = 20;
  apps_page->SlideViewIntoPosition(recent_apps, kVerticalOffset);
  ASSERT_TRUE(recent_apps->layer());
  EXPECT_TRUE(recent_apps->layer()->GetAnimator()->is_animating());

  // While that animation is running, run another animation.
  apps_page->SlideViewIntoPosition(recent_apps, kVerticalOffset);
  auto* compositor = recent_apps->layer()->GetCompositor();
  while (recent_apps->layer() &&
         recent_apps->layer()->GetAnimator()->is_animating()) {
    EXPECT_TRUE(ui::WaitForNextFrameToBePresented(compositor));
  }

  // At the end of the animation, the recent apps layer is still destroyed,
  // even though the layer existed at the start of the second animation.
  EXPECT_FALSE(recent_apps->layer());
}

TEST_F(AppListBubbleAppsPageTest, GradientMaskCreatedWhenAnimationsDisabled) {
  // Force disable animation.
  base::test::ScopedFeatureList feature;
  feature.InitAndDisableFeature(features::kProductivityLauncherAnimation);

  // Show an app list with enough apps to fill the page and trigger a gradient
  // at the bottom.
  auto* helper = GetAppListTestHelper();
  helper->AddAppItems(50);
  helper->ShowAppList();

  // Scroll view gradient mask layer is created.
  auto* apps_page = helper->GetBubbleAppsPage();
  EXPECT_TRUE(apps_page->scroll_view()->layer()->layer_mask_layer());
}

}  // namespace
}  // namespace ash
