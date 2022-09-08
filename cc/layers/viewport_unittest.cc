// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/viewport.h"

#include <stddef.h>

#include "cc/test/layer_test_common.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

TEST(ViewportTest, ShouldAnimateViewport) {
  EXPECT_TRUE(Viewport::ShouldAnimateViewport(gfx::Vector2dF(10, 1),
                                              gfx::Vector2dF(8, 5)));
  EXPECT_TRUE(Viewport::ShouldAnimateViewport(gfx::Vector2dF(0, 10),
                                              gfx::Vector2dF(4, 8)));
  EXPECT_TRUE(Viewport::ShouldAnimateViewport(gfx::Vector2dF(10, 10),
                                              gfx::Vector2dF(8, 9)));

  EXPECT_FALSE(Viewport::ShouldAnimateViewport(gfx::Vector2dF(8, 10),
                                               gfx::Vector2dF(8, 10)));
  EXPECT_FALSE(Viewport::ShouldAnimateViewport(gfx::Vector2dF(8, 10),
                                               gfx::Vector2dF(10, 0)));
  EXPECT_FALSE(Viewport::ShouldAnimateViewport(gfx::Vector2dF(10, 10),
                                               gfx::Vector2dF(10, 18)));
}

}  // namespace cc
