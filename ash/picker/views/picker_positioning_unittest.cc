// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_positioning.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/outsets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {
namespace {

constexpr gfx::Outsets kPaddingAroundCaret(4);
constexpr gfx::Point kDefaultCursorPoint(42, 42);

TEST(PickerPositioningTest,
     UsesCaretBoundsWhenCaretBoundsIsWithinWindowBounds) {
  gfx::Rect caret_bounds(100, 200, 5, 5);
  const gfx::Rect anchor_bounds = GetPickerAnchorBounds(
      /*caret_bounds=*/caret_bounds, kDefaultCursorPoint,
      /*focused_window_bounds=*/gfx::Rect(0, 0, 500, 500));

  caret_bounds.Outset(kPaddingAroundCaret);
  EXPECT_EQ(anchor_bounds, caret_bounds);
}

TEST(PickerPositioningTest,
     UsesCursorPointWhenCaretBoundsIsOutsideWindowBounds) {
  const gfx::Rect anchor_bounds = GetPickerAnchorBounds(
      /*caret_bounds=*/gfx::Rect(600, 200, 5, 5), kDefaultCursorPoint,
      /*focused_window_bounds=*/gfx::Rect(0, 0, 500, 500));

  EXPECT_EQ(anchor_bounds.origin(), kDefaultCursorPoint);
  EXPECT_EQ(anchor_bounds.width(), 0);
  EXPECT_EQ(anchor_bounds.height(), 0);
}

TEST(PickerPositioningTest, UsesCursorPointWhenCaretBoundsIsEmpty) {
  const gfx::Rect anchor_bounds = GetPickerAnchorBounds(
      /*caret_bounds=*/gfx::Rect(), kDefaultCursorPoint,
      /*focused_window_bounds=*/gfx::Rect(0, 0, 500, 500));

  EXPECT_EQ(anchor_bounds.origin(), kDefaultCursorPoint);
  EXPECT_EQ(anchor_bounds.width(), 0);
  EXPECT_EQ(anchor_bounds.height(), 0);
}

}  // namespace
}  // namespace ash
