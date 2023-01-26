// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_bubble_search_page.h"

#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/app_list_bubble_apps_page.h"
#include "ash/test/ash_test_base.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/layer_animation_stopped_waiter.h"
#include "ui/compositor/test/test_utils.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ash {
namespace {

using AppListBubbleSearchPageTest = AshTestBase;

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

  auto* apps_page = helper->GetBubbleAppsPage();
  ASSERT_TRUE(apps_page->GetVisible());

  // Type a key to switch to the search page. This should also be done without
  // animations.
  ASSERT_EQ(ui::ScopedAnimationDurationScaleMode::duration_multiplier(),
            ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
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
  ui::LayerAnimationStoppedWaiter().Wait(layer);
  EXPECT_FALSE(search_page->GetVisible());
}

// Regression test for https://crbug.com/1323035
TEST_F(AppListBubbleSearchPageTest,
       SearchPageVisibleAfterQuicklyClearingAndRepopulatingSearch) {
  // Open the app list without animation.
  ASSERT_EQ(ui::ScopedAnimationDurationScaleMode::duration_multiplier(),
            ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  auto* helper = GetAppListTestHelper();
  helper->AddAppItems(5);
  helper->ShowAppList();

  auto* apps_page = helper->GetBubbleAppsPage();
  ASSERT_TRUE(apps_page->GetVisible());
  auto* search_page = helper->GetBubbleSearchPage();
  ASSERT_FALSE(search_page->GetVisible());

  // Enable animations.
  ui::ScopedAnimationDurationScaleMode duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Type a key to trigger the animation to transition to the search page.
  ui::Layer* layer = apps_page->GetPageAnimationLayerForTest();
  PressAndReleaseKey(ui::VKEY_A);
  ASSERT_TRUE(layer->GetAnimator()->is_animating());

  // Before the animation completes, delete the search then quickly re-enter it.
  // This should abort animations, animate back to the apps page, abort
  // animations again, then animate back to the search page.
  PressAndReleaseKey(ui::VKEY_BACK);
  ASSERT_TRUE(layer->GetAnimator()->is_animating());
  PressAndReleaseKey(ui::VKEY_A);
  ASSERT_TRUE(layer->GetAnimator()->is_animating());

  ui::LayerAnimationStoppedWaiter().Wait(layer);

  EXPECT_FALSE(apps_page->GetVisible());
  EXPECT_TRUE(search_page->GetVisible());
}

}  // namespace
}  // namespace ash
