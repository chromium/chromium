// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/keyboard/ui/container_floating_behavior.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"

namespace keyboard {

TEST(ContainerFloatingBehaviorTest, AdjustSetBoundsRequest) {
  ContainerFloatingBehavior floating_behavior(nullptr);

  const int keyboard_width = 600;
  const int keyboard_height = 70;

  gfx::Rect workspace(0, 0, 1000, 600);
  gfx::Rect center(100, 100, keyboard_width, keyboard_height);
  gfx::Rect top_left_overlap(-30, -30, keyboard_width, keyboard_height);
  gfx::Rect bottom_right_overlap(workspace.width() - 30,
                                 workspace.height() - 30, keyboard_width,
                                 keyboard_height);

  gfx::Rect result =
      floating_behavior.AdjustSetBoundsRequest(workspace, center);
  ASSERT_EQ(center, result);
  result =
      floating_behavior.AdjustSetBoundsRequest(workspace, top_left_overlap);
  ASSERT_EQ(gfx::Rect(0, 0, keyboard_width, keyboard_height), result);

  result =
      floating_behavior.AdjustSetBoundsRequest(workspace, bottom_right_overlap);
  ASSERT_EQ(gfx::Rect(workspace.width() - keyboard_width,
                      workspace.height() - keyboard_height, keyboard_width,
                      keyboard_height),
            result);

  // Try to move the keyboard to the center of the primary display while it's
  // in a secondary display.
  gfx::Rect secondary_display(1000, -200, 1200, 800);
  result = floating_behavior.AdjustSetBoundsRequest(secondary_display, center);
  // It gets clipped to the far left of this display
  ASSERT_EQ(gfx::Rect(1000, 100, keyboard_width, keyboard_height), result);
}

TEST(ContainerFloatingBehaviorTest, AdjustSetBoundsRequestVariousSides) {
  ContainerFloatingBehavior floating_behavior(nullptr);

  const int keyboard_width = 100;
  const int keyboard_height = 100;
  gfx::Size keyboard_size = gfx::Size(keyboard_width, keyboard_height);
  gfx::Rect workspace_wide(0, 0, 1000, 500);
  gfx::Rect workspace_tall(0, 0, 500, 1000);
  gfx::Rect top_left(0, 0, keyboard_width, keyboard_height);
  gfx::Rect top_right(900, 0, keyboard_width, keyboard_height);
  gfx::Rect bottom_left(0, 400, keyboard_width, keyboard_height);
  gfx::Rect bottom_right(900, 400, keyboard_width, keyboard_height);
  gfx::Rect bottomish_center(450, 390, keyboard_width, keyboard_height);

  floating_behavior.AdjustSetBoundsRequest(workspace_wide, top_left);
  gfx::Point result = floating_behavior.GetPositionForShowingKeyboard(
      keyboard_size, workspace_wide);
  ASSERT_EQ(gfx::Point(0, 0), result);
  result = floating_behavior.GetPositionForShowingKeyboard(keyboard_size,
                                                           workspace_tall);
  ASSERT_EQ(gfx::Point(0, 0), result);

  floating_behavior.AdjustSetBoundsRequest(workspace_wide, top_right);
  result = floating_behavior.GetPositionForShowingKeyboard(keyboard_size,
                                                           workspace_wide);
  ASSERT_EQ(gfx::Point(900, 0), result);
  result = floating_behavior.GetPositionForShowingKeyboard(keyboard_size,
                                                           workspace_tall);
  ASSERT_EQ(gfx::Point(400, 0), result);

  floating_behavior.AdjustSetBoundsRequest(workspace_wide, bottom_left);
  result = floating_behavior.GetPositionForShowingKeyboard(keyboard_size,
                                                           workspace_wide);
  ASSERT_EQ(gfx::Point(0, 400), result);
  result = floating_behavior.GetPositionForShowingKeyboard(keyboard_size,
                                                           workspace_tall);
  ASSERT_EQ(gfx::Point(0, 900), result);

  floating_behavior.AdjustSetBoundsRequest(workspace_wide, bottom_right);
  result = floating_behavior.GetPositionForShowingKeyboard(keyboard_size,
                                                           workspace_wide);
  ASSERT_EQ(gfx::Point(900, 400), result);
  result = floating_behavior.GetPositionForShowingKeyboard(keyboard_size,
                                                           workspace_tall);
  ASSERT_EQ(gfx::Point(400, 900), result);

  floating_behavior.AdjustSetBoundsRequest(workspace_wide, bottomish_center);
  result = floating_behavior.GetPositionForShowingKeyboard(keyboard_size,
                                                           workspace_wide);
  ASSERT_EQ(gfx::Point(450, 390), result);
  result = floating_behavior.GetPositionForShowingKeyboard(keyboard_size,
                                                           workspace_tall);

  // rather than 400:0 for the vertical padding, use 390:10
  // with 900 pixels available this ratio results in 877.5, which is truncated.
  // 390 / 400 * 900 = 877.5
  ASSERT_EQ(gfx::Point(200, 877), result);
}

TEST(ContainerFloatingBehaviorTest, AdjustSetBoundsRequestOverlapLeftEdge) {
  ContainerFloatingBehavior floating_behavior(nullptr);

  const int keyboard_width = 600;
  const int keyboard_height = 70;

  gfx::Rect area_to_remain_on_screen(10, 10, keyboard_width - 20,
                                     keyboard_height - 20);
  gfx::Rect workspace(0, 0, 1000, 600);
  gfx::Rect left_edge_overlap(-150, 30, keyboard_width, keyboard_height);

  floating_behavior.SetAreaToRemainOnScreen(area_to_remain_on_screen);

  // Left edge of keyboard should go offscreen up to leftmost bound of
  // area_to_remain_on_screen.
  gfx::Rect result =
      floating_behavior.AdjustSetBoundsRequest(workspace, left_edge_overlap);
  ASSERT_EQ(gfx::Rect(area_to_remain_on_screen.x() * -1, left_edge_overlap.y(),
                      keyboard_width, keyboard_height),
            result);
}

TEST(ContainerFloatingBehaviorTest, AdjustSetBoundsRequestOverlapTopEdge) {
  ContainerFloatingBehavior floating_behavior(nullptr);

  const int keyboard_width = 600;
  const int keyboard_height = 70;

  gfx::Rect area_to_remain_on_screen(10, 10, keyboard_width - 20,
                                     keyboard_height - 20);
  gfx::Rect workspace(0, 0, 1000, 600);
  gfx::Rect top_edge_overlap(30, -40, keyboard_width, keyboard_height);

  floating_behavior.SetAreaToRemainOnScreen(area_to_remain_on_screen);

  // Top edge of keyboard should go offscreen up to uppermost bound of
  // area_to_remain_on_screen.
  gfx::Rect result =
      floating_behavior.AdjustSetBoundsRequest(workspace, top_edge_overlap);
  ASSERT_EQ(gfx::Rect(top_edge_overlap.x(), area_to_remain_on_screen.y() * -1,
                      keyboard_width, keyboard_height),
            result);
}

TEST(ContainerFloatingBehaviorTest, AdjustSetBoundsRequestOverlapRightEdge) {
  ContainerFloatingBehavior floating_behavior(nullptr);

  const int keyboard_width = 600;
  const int keyboard_height = 70;
  const int inner_offset_x = 10;

  gfx::Rect area_to_remain_on_screen(inner_offset_x, 10, keyboard_width - 20,
                                     keyboard_height - 20);
  gfx::Rect workspace(0, 0, 1000, 600);
  gfx::Rect right_edge_overlap(workspace.width() + 30 - keyboard_width, 0,
                               keyboard_width, keyboard_height);

  floating_behavior.SetAreaToRemainOnScreen(area_to_remain_on_screen);

  // Right edge of keyboard should go offscreen up to rightmost bound of
  // area_to_remain_on_screen.
  gfx::Rect result =
      floating_behavior.AdjustSetBoundsRequest(workspace, right_edge_overlap);
  ASSERT_EQ(gfx::Rect((workspace.width() + inner_offset_x) - keyboard_width,
                      right_edge_overlap.y(), keyboard_width, keyboard_height),
            result);
}

TEST(ContainerFloatingBehaviorTest, AdjustSetBoundsRequestBottomEdge) {
  ContainerFloatingBehavior floating_behavior(nullptr);

  const int keyboard_width = 600;
  const int keyboard_height = 70;
  const int inner_offset_y = 10;

  gfx::Rect area_to_remain_on_screen(10, inner_offset_y, keyboard_width - 20,
                                     keyboard_height - 20);
  gfx::Rect workspace(0, 0, 1000, 600);
  gfx::Rect bottom_edge_overlap(0, workspace.height() + 30 - keyboard_height,
                                keyboard_width, keyboard_height);

  floating_behavior.SetAreaToRemainOnScreen(area_to_remain_on_screen);

  // Bottom edge of keyboard should go offscreen up to bottommost bound of
  // area_to_remain_on_screen.
  gfx::Rect result =
      floating_behavior.AdjustSetBoundsRequest(workspace, bottom_edge_overlap);
  ASSERT_EQ(gfx::Rect(bottom_edge_overlap.x(),
                      (workspace.height() + inner_offset_y) - keyboard_height,
                      keyboard_width, keyboard_height),
            result);
}

}  // namespace keyboard
