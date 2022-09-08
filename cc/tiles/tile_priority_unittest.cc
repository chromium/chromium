// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/tile_priority.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

TEST(TilePriorityTest, IsHigherPriorityThan) {
  TilePriority now(HIGH_RESOLUTION, TilePriority::NOW, 0);
  TilePriority close_soon(HIGH_RESOLUTION, TilePriority::SOON, 1);
  TilePriority far_soon(HIGH_RESOLUTION, TilePriority::SOON, 500);
  TilePriority close_eventually(HIGH_RESOLUTION, TilePriority::EVENTUALLY, 2);
  TilePriority far_eventually(HIGH_RESOLUTION, TilePriority::EVENTUALLY, 1000);
  TilePriority non_ideal_now(NON_IDEAL_RESOLUTION, TilePriority::NOW, 0);

  EXPECT_FALSE(now.IsHigherPriorityThan(now));
  EXPECT_FALSE(now.IsHigherPriorityThan(non_ideal_now));

  EXPECT_TRUE(now.IsHigherPriorityThan(close_soon));
  EXPECT_TRUE(now.IsHigherPriorityThan(far_soon));
  EXPECT_TRUE(now.IsHigherPriorityThan(close_eventually));
  EXPECT_TRUE(now.IsHigherPriorityThan(far_eventually));
  EXPECT_TRUE(close_soon.IsHigherPriorityThan(far_soon));
  EXPECT_TRUE(close_soon.IsHigherPriorityThan(close_eventually));
  EXPECT_TRUE(close_soon.IsHigherPriorityThan(far_eventually));
  EXPECT_TRUE(far_soon.IsHigherPriorityThan(close_eventually));
  EXPECT_TRUE(far_soon.IsHigherPriorityThan(far_eventually));
  EXPECT_TRUE(close_eventually.IsHigherPriorityThan(far_eventually));

  EXPECT_FALSE(far_eventually.IsHigherPriorityThan(close_eventually));
  EXPECT_FALSE(far_eventually.IsHigherPriorityThan(far_soon));
  EXPECT_FALSE(far_eventually.IsHigherPriorityThan(close_soon));
  EXPECT_FALSE(far_eventually.IsHigherPriorityThan(now));
  EXPECT_FALSE(far_eventually.IsHigherPriorityThan(non_ideal_now));
  EXPECT_FALSE(close_eventually.IsHigherPriorityThan(far_soon));
  EXPECT_FALSE(close_eventually.IsHigherPriorityThan(close_soon));
  EXPECT_FALSE(close_eventually.IsHigherPriorityThan(now));
  EXPECT_FALSE(far_soon.IsHigherPriorityThan(close_soon));
  EXPECT_FALSE(far_soon.IsHigherPriorityThan(now));
  EXPECT_FALSE(close_soon.IsHigherPriorityThan(now));
}

}  // namespace cc
