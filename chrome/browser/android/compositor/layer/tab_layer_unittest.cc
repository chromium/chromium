// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/layer/tab_layer.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"

using android::TabLayer;

TEST(ComputePaddingPositionsTest, NoSideOrBottomPadding) {
  gfx::Size content_size(100, 400);
  gfx::Size desired_size(100, 400);
  gfx::Rect side_padding_rect;
  gfx::Rect bottom_padding_rect;

  TabLayer::ComputePaddingPositions(content_size,
                                    desired_size,
                                    &side_padding_rect,
                                    &bottom_padding_rect);

  EXPECT_TRUE(side_padding_rect.IsEmpty());
  EXPECT_TRUE(bottom_padding_rect.IsEmpty());
}

TEST(ComputePaddingPositionsTest, OnlySidePadding) {
  gfx::Size content_size(90, 400);
  gfx::Size desired_size(100, 400);
  gfx::Rect side_padding_rect;
  gfx::Rect bottom_padding_rect;

  TabLayer::ComputePaddingPositions(content_size,
                                    desired_size,
                                    &side_padding_rect,
                                    &bottom_padding_rect);

  EXPECT_EQ(10, side_padding_rect.size().width());
  EXPECT_EQ(400, side_padding_rect.size().height());
  EXPECT_EQ(90, side_padding_rect.x());
  EXPECT_TRUE(bottom_padding_rect.IsEmpty());
}

TEST(ComputePaddingPositionsTest, OnlyBottomPadding) {
  gfx::Size content_size(100, 300);
  gfx::Size desired_size(100, 400);
  gfx::Rect side_padding_rect;
  gfx::Rect bottom_padding_rect;

  TabLayer::ComputePaddingPositions(content_size,
                                    desired_size,
                                    &side_padding_rect,
                                    &bottom_padding_rect);

  EXPECT_TRUE(side_padding_rect.IsEmpty());
  EXPECT_EQ(100, bottom_padding_rect.size().width());
  EXPECT_EQ(100, bottom_padding_rect.size().height());
  EXPECT_EQ(300, bottom_padding_rect.y());
}

TEST(ComputePaddingPositionsTest, SideAndBottomPadding) {
  gfx::Size content_size(75, 250);
  gfx::Size desired_size(100, 400);
  gfx::Rect side_padding_rect;
  gfx::Rect bottom_padding_rect;

  TabLayer::ComputePaddingPositions(content_size,
                                    desired_size,
                                    &side_padding_rect,
                                    &bottom_padding_rect);

  EXPECT_EQ(25, side_padding_rect.size().width());
  EXPECT_EQ(250, side_padding_rect.size().height());
  EXPECT_EQ(75, side_padding_rect.x());

  EXPECT_EQ(100, bottom_padding_rect.size().width());
  EXPECT_EQ(150, bottom_padding_rect.size().height());
  EXPECT_EQ(250, bottom_padding_rect.y());
}
