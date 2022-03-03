// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_bubble_search_page.h"

#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/app_list_bubble_apps_page.h"
#include "ash/constants/ash_features.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/layer_animation_stopped_waiter.h"
#include "base/test/scoped_feature_list.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/test_utils.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ash {
namespace {

class AppListBubbleSearchPageTest : public AshTestBase {
 private:
  base::test::ScopedFeatureList features_{features::kProductivityLauncher};
};

TEST_F(AppListBubbleSearchPageTest, AnimateShowPage) {
  // Open the app list without animation.
  ASSERT_EQ(ui::ScopedAnimationDurationScaleMode::duration_multiplier(),
            ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  auto* helper = GetAppListTestHelper();
  helper->AddAppItems(5);
  helper->ShowAppList();

  // Enable animations.
  ui::ScopedAnimationDurationScaleMode duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Type a key to switch to the search page.
  PressAndReleaseKey(ui::VKEY_A);

  // Both apps page and search page are visible during the transition animation.
  auto* apps_page = helper->GetBubbleAppsPage();
  EXPECT_TRUE(apps_page->GetVisible());
  auto* search_page = helper->GetBubbleSearchPage();
  EXPECT_TRUE(search_page->GetVisible());

  // The entire search page fades in.
  ui::Layer* layer = search_page->GetPageAnimationLayerForTest();
  ASSERT_TRUE(layer);
  auto* animator = layer->GetAnimator();
  ASSERT_TRUE(animator);
  EXPECT_TRUE(animator->IsAnimatingProperty(
      ui::LayerAnimationElement::AnimatableProperty::OPACITY));
}

TEST_F(AppListBubbleSearchPageTest, AnimateHidePage) {
  // Open the app list without animation.
  ASSERT_EQ(ui::ScopedAnimationDurationScaleMode::duration_multiplier(),
            ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  auto* helper = GetAppListTestHelper();
  helper->AddAppItems(5);
  helper->ShowAppList();

  // Type a key to switch to the search page.
  PressAndReleaseKey(ui::VKEY_A);
  auto* search_page = helper->GetBubbleSearchPage();
  ASSERT_TRUE(search_page->GetVisible());

  // Enable animations.
  ui::ScopedAnimationDurationScaleMode duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Pressing backspace to delete the search triggers the hide animation.
  PressAndReleaseKey(ui::VKEY_BACK);
  ui::Layer* layer = search_page->GetPageAnimationLayerForTest();
  ASSERT_TRUE(layer);
  auto* animator = layer->GetAnimator();
  ASSERT_TRUE(animator);
  EXPECT_TRUE(animator->IsAnimatingProperty(
      ui::LayerAnimationElement::AnimatableProperty::OPACITY));
  EXPECT_TRUE(animator->IsAnimatingProperty(
      ui::LayerAnimationElement::AnimatableProperty::TRANSFORM));

  // Search page visibility updates at the end of the animation.
  LayerAnimationStoppedWaiter().Wait(layer);
  EXPECT_FALSE(search_page->GetVisible());
}

}  // namespace
}  // namespace ash
