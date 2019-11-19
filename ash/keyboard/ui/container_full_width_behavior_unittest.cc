// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/keyboard/ui/container_full_width_behavior.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

namespace keyboard {

TEST(ContainerFullWidthBehaviorTest, AdjustSetBoundsRequest) {
  ContainerFullWidthBehavior full_width_behavior(nullptr);

  // workspace is not always necessarily positioned at the origin (e.g.
  // secondary display).
  gfx::Rect workspace(20, -30, 300, 200);
  gfx::Rect requested_bounds(0, 0, 10, 10);
  gfx::Rect result;

  // Ignore width. Stretch the bounds across the bottom of the screen.
  result =
      full_width_behavior.AdjustSetBoundsRequest(workspace, requested_bounds);
  ASSERT_EQ(gfx::Rect(20, 160, 300, 10), result);

  // Even if the coordinates are non-zero, ignore them. Only use height.
  requested_bounds = gfx::Rect(30, 80, 20, 100);
  result =
      full_width_behavior.AdjustSetBoundsRequest(workspace, requested_bounds);
  ASSERT_EQ(gfx::Rect(20, 70, 300, 100), result);
}

}  // namespace keyboard
