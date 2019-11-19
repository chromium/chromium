// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/views_utils.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace login_views_utils {

namespace {

class ViewsUtilsUnittest : public testing::Test {
 protected:
  ViewsUtilsUnittest() = default;
  ~ViewsUtilsUnittest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(ViewsUtilsUnittest);
};

}  // namespace

TEST_F(ViewsUtilsUnittest, LeftRightStrategySimpleTest) {
  const gfx::Rect anchor(10, 10, 10, 10);
  const gfx::Size bubble(5, 5);
  const gfx::Rect bounds(20, 20);
  const gfx::Point result_point(anchor.x() - bubble.width(), anchor.y());
  EXPECT_EQ(result_point,
            CalculateBubblePositionLeftRightStrategy(anchor, bubble, bounds));
}

TEST_F(ViewsUtilsUnittest, LeftRightStrategyNotEnoughHeightBottom) {
  const gfx::Rect anchor(10, 10, 10, 10);
  const gfx::Size bubble(5, 15);
  const gfx::Rect bounds(20, 20);
  const gfx::Point result_point(anchor.x() - bubble.width(),
                                bounds.height() - bubble.height());
  EXPECT_EQ(result_point,
            CalculateBubblePositionLeftRightStrategy(anchor, bubble, bounds));
}

TEST_F(ViewsUtilsUnittest, LeftRightStrategyNotEnoughHeightBottomAndTop) {
  const gfx::Rect anchor(10, 10, 10, 10);
  const gfx::Size bubble(5, 25);
  const gfx::Rect bounds(20, 20);
  const gfx::Point result_point(anchor.x() - bubble.width(), bounds.y());
  EXPECT_EQ(result_point,
            CalculateBubblePositionLeftRightStrategy(anchor, bubble, bounds));
}

TEST_F(ViewsUtilsUnittest, LeftRightStrategyNotEnoughWidthLeft) {
  const gfx::Rect anchor(2, 10, 10, 10);
  const gfx::Size bubble(5, 5);
  const gfx::Rect bounds(20, 20);
  const gfx::Point result_point(anchor.x() + anchor.width(), anchor.y());
  EXPECT_EQ(result_point,
            CalculateBubblePositionLeftRightStrategy(anchor, bubble, bounds));
}

TEST_F(ViewsUtilsUnittest, RightLeftStrategySimpleTest) {
  const gfx::Rect anchor(0, 10, 10, 10);
  const gfx::Size bubble(5, 5);
  const gfx::Rect bounds(20, 20);
  const gfx::Point result_point(anchor.x() + anchor.width(), anchor.y());
  EXPECT_EQ(result_point,
            CalculateBubblePositionRigthLeftStrategy(anchor, bubble, bounds));
}

TEST_F(ViewsUtilsUnittest, RightLeftStrategyNotEnoughHeightBottom) {
  const gfx::Rect anchor(0, 10, 10, 10);
  const gfx::Size bubble(5, 15);
  const gfx::Rect bounds(20, 20);
  const gfx::Point result_point(anchor.x() + anchor.width(),
                                bounds.height() - bubble.height());
  EXPECT_EQ(result_point,
            CalculateBubblePositionRigthLeftStrategy(anchor, bubble, bounds));
}

TEST_F(ViewsUtilsUnittest, RightLeftStrategyNotEnoughHeightBottomAndTop) {
  const gfx::Rect anchor(0, 10, 10, 10);
  const gfx::Size bubble(5, 25);
  const gfx::Rect bounds(20, 20);
  const gfx::Point result_point(anchor.x() + anchor.width(), bounds.y());
  EXPECT_EQ(result_point,
            CalculateBubblePositionRigthLeftStrategy(anchor, bubble, bounds));
}

TEST_F(ViewsUtilsUnittest, RightLeftStrategyNotEnoughWidthRight) {
  const gfx::Rect anchor(10, 10, 10, 10);
  const gfx::Size bubble(5, 5);
  const gfx::Rect bounds(20, 20);
  const gfx::Point result_point(anchor.x() - bubble.width(), anchor.y());
  EXPECT_EQ(result_point,
            CalculateBubblePositionRigthLeftStrategy(anchor, bubble, bounds));
}

}  // namespace login_views_utils

}  // namespace ash
