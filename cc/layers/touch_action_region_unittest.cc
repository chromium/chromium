// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/touch_action_region.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

TEST(TouchActionRegionTest, GetAllowedTouchActionMapOverlapToZero) {
  TouchActionRegion touch_action_region;
  touch_action_region.Union(TouchAction::kPanLeft, gfx::Rect(0, 0, 50, 50));
  touch_action_region.Union(TouchAction::kPanRight, gfx::Rect(25, 25, 25, 25));
  // The point is only in PanLeft, so the result is PanLeft.
  EXPECT_EQ(TouchAction::kPanLeft,
            touch_action_region.GetAllowedTouchAction(gfx::Point(10, 10)));
  // The point is in both PanLeft and PanRight, and those actions have no
  // common components, so the result is None.
  EXPECT_EQ(TouchAction::kNone,
            touch_action_region.GetAllowedTouchAction(gfx::Point(30, 30)));
  // The point is in neither PanLeft nor PanRight, so the result is Auto since
  // the default touch action is auto.
  EXPECT_EQ(TouchAction::kAuto,
            touch_action_region.GetAllowedTouchAction(gfx::Point(60, 60)));
}

TEST(TouchActionRegionTest, GetAllowedTouchActionMapOverlapToNonZero) {
  TouchActionRegion touch_action_region;
  touch_action_region.Union(TouchAction::kPanX, gfx::Rect(0, 0, 50, 50));
  touch_action_region.Union(TouchAction::kPanRight, gfx::Rect(25, 25, 25, 25));
  // The point is only in PanX, so the result is PanX.
  EXPECT_EQ(TouchAction::kPanX,
            touch_action_region.GetAllowedTouchAction(gfx::Point(10, 10)));
  // The point is in both PanX and PanRight, and PanRight is a common component,
  // so the result is PanRight.
  EXPECT_EQ(TouchAction::kPanRight,
            touch_action_region.GetAllowedTouchAction(gfx::Point(30, 30)));
  // The point is neither PanX nor PanRight, so the result is Auto since the
  // default touch action is auto.
  EXPECT_EQ(TouchAction::kAuto,
            touch_action_region.GetAllowedTouchAction(gfx::Point(60, 60)));
}

TEST(TouchActionRegionTest, GetAllowedTouchActionEmptyMap) {
  TouchActionRegion touch_action_region;
  // The result is Auto since the map is empty and the default touch
  // action is auto.
  EXPECT_EQ(TouchAction::kAuto,
            touch_action_region.GetAllowedTouchAction(gfx::Point(10, 10)));
}

TEST(TouchActionRegionTest, GetAllowedTouchActionSingleMapEntry) {
  TouchActionRegion touch_action_region;
  touch_action_region.Union(TouchAction::kPanUp, gfx::Rect(0, 0, 50, 50));
  // The point is only in PanUp, so the result is PanUp.
  EXPECT_EQ(TouchAction::kPanUp,
            touch_action_region.GetAllowedTouchAction(gfx::Point(10, 10)));
  // The point is not in PanUp, so the result is Auto since the default touch
  // action is auto.
  EXPECT_EQ(TouchAction::kAuto,
            touch_action_region.GetAllowedTouchAction(gfx::Point(60, 60)));
}

}  // namespace
}  // namespace cc
