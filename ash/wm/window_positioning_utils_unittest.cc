// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_positioning_utils.h"

#include "ash/test/ash_test_base.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"

namespace ash {

using WindowPositioningUtilsTest = AshTestBase;

TEST_F(WindowPositioningUtilsTest, SnapBoundsWithOddNumberedScreenWidth) {
  UpdateDisplay("999x700");

  auto window = CreateToplevelTestWindow();
  gfx::Rect left_bounds =
      GetDefaultLeftSnappedWindowBoundsInParent(window.get());
  gfx::Rect right_bounds =
      GetDefaultRightSnappedWindowBoundsInParent(window.get());
  EXPECT_EQ(left_bounds.x(), 0);
  EXPECT_EQ(left_bounds.y(), 0);
  EXPECT_EQ(right_bounds.right(), 999);
  EXPECT_EQ(right_bounds.y(), 0);
  EXPECT_EQ(left_bounds.right(), right_bounds.x());
  EXPECT_NEAR(left_bounds.width(), 499, 1);
  EXPECT_NEAR(right_bounds.width(), 499, 1);
}

TEST_F(WindowPositioningUtilsTest, SnapBoundsWithMinimumSize) {
  UpdateDisplay("800x600");

  auto window = CreateToplevelTestWindow();
  auto* test_delegate =
      static_cast<aura::test::TestWindowDelegate*>(window->delegate());
  test_delegate->set_minimum_size(gfx::Size(300, 200));
  gfx::Rect left_bounds =
      GetDefaultLeftSnappedWindowBoundsInParent(window.get());
  EXPECT_EQ(left_bounds.width(), 400);
  gfx::Rect right_bounds =
      GetDefaultRightSnappedWindowBoundsInParent(window.get());
  EXPECT_EQ(right_bounds.width(), 400);
  EXPECT_EQ(right_bounds.right(), 800);

  test_delegate->set_minimum_size(gfx::Size(600, 200));
  left_bounds = GetDefaultLeftSnappedWindowBoundsInParent(window.get());
  EXPECT_EQ(left_bounds.width(), 600);
  right_bounds = GetDefaultRightSnappedWindowBoundsInParent(window.get());
  EXPECT_EQ(right_bounds.width(), 600);
  EXPECT_EQ(right_bounds.right(), 800);

  test_delegate->set_minimum_size(gfx::Size(1200, 200));
  left_bounds = GetDefaultLeftSnappedWindowBoundsInParent(window.get());
  EXPECT_EQ(left_bounds.width(), 800);
  right_bounds = GetDefaultRightSnappedWindowBoundsInParent(window.get());
  EXPECT_EQ(right_bounds.width(), 800);
  EXPECT_EQ(right_bounds.right(), 800);
}

}  // namespace ash
