// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/views_utils.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace login_views_utils {

namespace {

class ViewsUtilsUnittest : public testing::Test {
 public:
  ViewsUtilsUnittest(const ViewsUtilsUnittest&) = delete;
  ViewsUtilsUnittest& operator=(const ViewsUtilsUnittest&) = delete;

 protected:
  ViewsUtilsUnittest() = default;
  ~ViewsUtilsUnittest() override = default;
};

}  // namespace

TEST_F(ViewsUtilsUnittest, BeforeAfterStrategySimpleTest) {
  const gfx::Rect anchor(10, 10, 10, 10);
  const gfx::Size bubble(5, 5);
  const gfx::Rect bounds(20, 20);
  const gfx::Point result_point(anchor.x() - bubble.width(), anchor.y());
  EXPECT_EQ(result_point,
            CalculateBubblePositionBeforeAfterStrategy(anchor, bubble, bounds));
}

TEST_F(ViewsUtilsUnittest, BeforeAfterStrategyNotEnoughHeightBottom) {
  const gfx::Rect anchor(10, 10, 10, 10);
  const gfx::Size bubble(5, 15);
  const gfx::Rect bounds(20, 20);
  const gfx::Point result_point(anchor.x() - bubble.width(),
                                bounds.height() - bubble.height());
  EXPECT_EQ(result_point,
            CalculateBubblePositionBeforeAfterStrategy(anchor, bubble, bounds));
}

TEST_F(ViewsUtilsUnittest, BeforeAfterStrategyNotEnoughHeightBottomAndTop) {
  const gfx::Rect anchor(10, 10, 10, 10);
  const gfx::Size bubble(5, 25);
  const gfx::Rect bounds(20, 20);
  const gfx::Point result_point(anchor.x() - bubble.width(), bounds.y());
  EXPECT_EQ(result_point,
            CalculateBubblePositionBeforeAfterStrategy(anchor, bubble, bounds));
}

TEST_F(ViewsUtilsUnittest, BeforeAfterStrategyNotEnoughWidthBefore) {
  const gfx::Rect anchor(2, 10, 10, 10);
  const gfx::Size bubble(5, 5);
  const gfx::Rect bounds(20, 20);
  const gfx::Point result_point(anchor.x() + anchor.width(), anchor.y());
  EXPECT_EQ(result_point,
            CalculateBubblePositionBeforeAfterStrategy(anchor, bubble, bounds));
}

TEST_F(ViewsUtilsUnittest, AfterBeforeStrategySimpleTest) {
  const gfx::Rect anchor(0, 10, 10, 10);
  const gfx::Size bubble(5, 5);
  const gfx::Rect bounds(20, 20);
  const gfx::Point result_point(anchor.x() + anchor.width(), anchor.y());
  EXPECT_EQ(result_point,
            CalculateBubblePositionAfterBeforeStrategy(anchor, bubble, bounds));
}

TEST_F(ViewsUtilsUnittest, AfterBeforeStrategyNotEnoughHeightBottom) {
  const gfx::Rect anchor(0, 10, 10, 10);
  const gfx::Size bubble(5, 15);
  const gfx::Rect bounds(20, 20);
  const gfx::Point result_point(anchor.x() + anchor.width(),
                                bounds.height() - bubble.height());
  EXPECT_EQ(result_point,
            CalculateBubblePositionAfterBeforeStrategy(anchor, bubble, bounds));
}

TEST_F(ViewsUtilsUnittest, AfterBeforeStrategyNotEnoughHeightBottomAndTop) {
  const gfx::Rect anchor(0, 10, 10, 10);
  const gfx::Size bubble(5, 25);
  const gfx::Rect bounds(20, 20);
  const gfx::Point result_point(anchor.x() + anchor.width(), bounds.y());
  EXPECT_EQ(result_point,
            CalculateBubblePositionAfterBeforeStrategy(anchor, bubble, bounds));
}

TEST_F(ViewsUtilsUnittest, AfterBeforeStrategyNotEnoughWidthAfter) {
  const gfx::Rect anchor(10, 10, 10, 10);
  const gfx::Size bubble(5, 5);
  const gfx::Rect bounds(20, 20);
  const gfx::Point result_point(anchor.x() - bubble.width(), anchor.y());
  EXPECT_EQ(result_point,
            CalculateBubblePositionAfterBeforeStrategy(anchor, bubble, bounds));
}

}  // namespace login_views_utils

}  // namespace ash
