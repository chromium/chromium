// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_positioning_utils.h"

#include "ash/public/cpp/window_properties.h"
#include "ash/test/ash_test_base.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/display/screen.h"

namespace ash {

using WindowPositioningUtilsTest = AshTestBase;

TEST_F(WindowPositioningUtilsTest, SnapBoundsWithOddNumberedScreenWidth) {
  UpdateDisplay("999x700");

  auto window = CreateToplevelTestWindow();
  gfx::Rect left_bounds = GetDefaultSnappedWindowBoundsInParent(
      window.get(), SnapViewType::kPrimary);
  gfx::Rect right_bounds = GetDefaultSnappedWindowBoundsInParent(
      window.get(), SnapViewType::kSecondary);
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
  gfx::Rect left_bounds = GetDefaultSnappedWindowBoundsInParent(
      window.get(), SnapViewType::kPrimary);
  EXPECT_EQ(left_bounds.width(), 400);
  gfx::Rect right_bounds = GetDefaultSnappedWindowBoundsInParent(
      window.get(), SnapViewType::kSecondary);
  EXPECT_EQ(right_bounds.width(), 400);
  EXPECT_EQ(right_bounds.right(), 800);

  test_delegate->set_minimum_size(gfx::Size(600, 200));
  left_bounds = GetDefaultSnappedWindowBoundsInParent(window.get(),
                                                      SnapViewType::kPrimary);
  EXPECT_EQ(left_bounds.width(), 600);
  right_bounds = GetDefaultSnappedWindowBoundsInParent(
      window.get(), SnapViewType::kSecondary);
  EXPECT_EQ(right_bounds.width(), 600);
  EXPECT_EQ(right_bounds.right(), 800);

  test_delegate->set_minimum_size(gfx::Size(1200, 200));
  left_bounds = GetDefaultSnappedWindowBoundsInParent(window.get(),
                                                      SnapViewType::kPrimary);
  EXPECT_EQ(left_bounds.width(), 800);
  right_bounds = GetDefaultSnappedWindowBoundsInParent(
      window.get(), SnapViewType::kSecondary);
  EXPECT_EQ(right_bounds.width(), 800);
  EXPECT_EQ(right_bounds.right(), 800);
}

TEST_F(WindowPositioningUtilsTest, SnapBoundsWithUnresizableSnapProperty) {
  auto window = CreateToplevelTestWindow();
  window->SetProperty(aura::client::kResizeBehaviorKey,
                      aura::client::kResizeBehaviorNone);
  {
    // Test landscape display.
    UpdateDisplay("800x600");
    const auto work_area =
        display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
    window->SetProperty(kUnresizableSnappedSizeKey, new gfx::Size(200, 0));

    gfx::Rect left_bounds = GetDefaultSnappedWindowBoundsInParent(
        window.get(), SnapViewType::kPrimary);
    gfx::Rect right_bounds = GetDefaultSnappedWindowBoundsInParent(
        window.get(), SnapViewType::kSecondary);
    EXPECT_EQ(left_bounds.x(), work_area.x());
    EXPECT_EQ(left_bounds.y(), work_area.y());
    EXPECT_EQ(left_bounds.width(), 200);

    EXPECT_EQ(right_bounds.right(), work_area.right());
    EXPECT_EQ(right_bounds.y(), work_area.y());
    EXPECT_EQ(right_bounds.width(), 200);
  }
  {
    // Test portrait display.
    UpdateDisplay("600x800");
    const auto work_area =
        display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
    window->SetProperty(kUnresizableSnappedSizeKey, new gfx::Size(0, 200));

    gfx::Rect top_bounds = GetDefaultSnappedWindowBoundsInParent(
        window.get(), SnapViewType::kPrimary);
    gfx::Rect bottom_bounds = GetDefaultSnappedWindowBoundsInParent(
        window.get(), SnapViewType::kSecondary);
    EXPECT_EQ(top_bounds.x(), work_area.x());
    EXPECT_EQ(top_bounds.y(), work_area.y());
    EXPECT_EQ(top_bounds.height(), 200);

    EXPECT_EQ(bottom_bounds.x(), work_area.x());
    EXPECT_EQ(bottom_bounds.bottom(), work_area.bottom());
    EXPECT_EQ(bottom_bounds.height(), 200);
  }
}

}  // namespace ash
