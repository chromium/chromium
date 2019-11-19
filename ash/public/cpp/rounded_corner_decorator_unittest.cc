// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/rounded_corner_decorator.h"

#include "ash/public/cpp/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"

namespace ash {

using RoundedCornerDecoratorTest = aura::test::AuraTestBase;

// Test that the decorator doesn't try to apply itself to destroyed layers.
TEST_F(RoundedCornerDecoratorTest, RoundedCornerMaskProperlyInvalidatesItself) {
  constexpr int kCornerRadius = 4;
  constexpr gfx::RoundedCornersF kRadii(kCornerRadius);
  std::unique_ptr<aura::Window> window(aura::test::CreateTestWindowWithBounds(
      gfx::Rect(100, 100, 100, 100), root_window()));
  auto decorator = std::make_unique<ash::RoundedCornerDecorator>(
      window.get(), window.get(), window->layer(), kCornerRadius);

  // Confirm a mask layer exists and the decorator is valid.
  EXPECT_TRUE(window->layer());
  ASSERT_FALSE(window->layer()->layer_mask_layer());
  EXPECT_EQ(window->layer()->rounded_corner_radii(), kRadii);
  EXPECT_TRUE(decorator->IsValid());

  // Destroy window.
  window.reset();

  // Existing layer was destroyed, so the decorator should no longer be valid.
  EXPECT_FALSE(decorator->IsValid());
}

// Test that mask layer changes bounds with the window it is applied to.
TEST_F(RoundedCornerDecoratorTest,
       RoundedCornerMaskChangesBoundsOnWindowBoundsChange) {
  constexpr int kCornerRadius = 4;
  constexpr gfx::RoundedCornersF kRadii(kCornerRadius);
  std::unique_ptr<aura::Window> window(aura::test::CreateTestWindowWithBounds(
      gfx::Rect(100, 100, 100, 100), root_window()));
  auto decorator = std::make_unique<ash::RoundedCornerDecorator>(
      window.get(), window.get(), window->layer(), kCornerRadius);

  ASSERT_FALSE(window->layer()->layer_mask_layer());
  EXPECT_EQ(window->layer()->rounded_corner_radii(), kRadii);

  // Change the bounds of the window. Set zero duration animations to apply
  // changes immediately.
  window->SetBounds(gfx::Rect(0, 0, 150, 150));

  // Make sure the mask layer's bounds are also changed.
  EXPECT_EQ(gfx::Rect(0, 0, 150, 150).ToString(), window->bounds().ToString());

  ASSERT_FALSE(window->layer()->layer_mask_layer());
  EXPECT_EQ(window->layer()->rounded_corner_radii(), kRadii);
}

}  // namespace ash
