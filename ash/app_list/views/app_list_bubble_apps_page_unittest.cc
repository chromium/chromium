// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_bubble_apps_page.h"

#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/app_list_bubble_search_page.h"
#include "ash/constants/ash_features.h"
#include "ash/test/ash_test_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/test_utils.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
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
  constexpr int kVerticalOffset = 20;
  constexpr base::TimeDelta kSlideDuration = base::Milliseconds(100);
  helper->StartSlideAnimationOnBubbleAppsPage(recent_apps, kVerticalOffset,
                                              kSlideDuration);
  ASSERT_TRUE(recent_apps->layer());
  EXPECT_TRUE(recent_apps->layer()->GetAnimator()->is_animating());

  // While that animation is running, run another animation.
  helper->StartSlideAnimationOnBubbleAppsPage(recent_apps, kVerticalOffset,
                                              kSlideDuration);
  auto* compositor = recent_apps->layer()->GetCompositor();
  while (recent_apps->layer() &&
         recent_apps->layer()->GetAnimator()->is_animating()) {
    EXPECT_TRUE(ui::WaitForNextFrameToBePresented(compositor));
  }

  // At the end of the animation, the recent apps layer is still destroyed,
  // even though the layer existed at the start of the second animation.
  EXPECT_FALSE(recent_apps->layer());
}

TEST_F(AppListBubbleAppsPageTest, AnimateHidePage) {
  // Open the app list without animation.
  ASSERT_EQ(ui::ScopedAnimationDurationScaleMode::duration_multiplier(),
            ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  auto* helper = GetAppListTestHelper();
  helper->AddAppItems(5);
  helper->ShowAppList();

  auto* apps_page = helper->GetBubbleAppsPage();
  ASSERT_TRUE(apps_page->GetVisible());

  // Enable animations.
  ui::ScopedAnimationDurationScaleMode duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  base::HistogramTester histograms;

  // Type a key to trigger the animation to transition to the search page.
  PressAndReleaseKey(ui::VKEY_A);
  helper->WaitForLayerAnimation(apps_page->GetPageAnimationLayerForTest());

  // Apps page is not visible.
  EXPECT_FALSE(apps_page->GetVisible());

  // Smoothness was recorded.
  histograms.ExpectTotalCount(
      "Apps.ClamshellLauncher.AnimationSmoothness.HideAppsPage", 1);
}

TEST_F(AppListBubbleAppsPageTest, AnimateShowPage) {
  // Open the app list without animation.
  ASSERT_EQ(ui::ScopedAnimationDurationScaleMode::duration_multiplier(),
            ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  auto* helper = GetAppListTestHelper();
  helper->AddAppItems(5);
  helper->ShowAppList();

  // Type a key switch to the search page.
  PressAndReleaseKey(ui::VKEY_A);

  auto* apps_page = helper->GetBubbleAppsPage();
  ASSERT_FALSE(apps_page->GetVisible());

  // Enable animations. NON_ZERO_DURATION does not work here. The animation
  // end callback is not called, for reasons I don't understand. It works fine
  // in production, and in tests with NORMAL_DURATION.
  ui::ScopedAnimationDurationScaleMode duration(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  base::HistogramTester histograms;

  // Press escape to trigger animation back to the apps page.
  PressAndReleaseKey(ui::VKEY_ESCAPE);
  helper->WaitForLayerAnimation(apps_page->GetPageAnimationLayerForTest());

  // Apps page is visible.
  EXPECT_TRUE(apps_page->GetVisible());

  // Smoothness was recorded.
  histograms.ExpectTotalCount(
      "Apps.ClamshellLauncher.AnimationSmoothness.ShowAppsPage", 1);
}

TEST_F(AppListBubbleAppsPageTest, ScrollPositionResetOnShow) {
  // Show an app list with enough apps to allow scrolling.
  auto* helper = GetAppListTestHelper();
  helper->AddAppItems(50);
  helper->ShowAppList();

  // Press the up arrow, which will scroll the view to select an app in the
  // last row.
  PressAndReleaseKey(ui::VKEY_UP);
  auto* apps_page = helper->GetBubbleAppsPage();
  ASSERT_GT(apps_page->scroll_view()->vertical_scroll_bar()->GetPosition(), 0);

  // Hide the launcher, then show it again.
  helper->Dismiss();
  helper->ShowAppList();

  // Scroll position is reset to top.
  EXPECT_EQ(apps_page->scroll_view()->vertical_scroll_bar()->GetPosition(), 0);
}

}  // namespace
}  // namespace ash
