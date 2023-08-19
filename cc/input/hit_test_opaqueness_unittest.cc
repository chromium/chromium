// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/hit_test_opaqueness.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

constexpr auto kTransparent = HitTestOpaqueness::kTransparent;
constexpr auto kMixed = HitTestOpaqueness::kMixed;
constexpr auto kOpaque = HitTestOpaqueness::kOpaque;

#define EXPECT_OPAQUENESS(expected, r1, o1, r2, o2)              \
  do {                                                           \
    EXPECT_EQ(expected, UnionHitTestOpaqueness(r1, o1, r2, o2)); \
    EXPECT_EQ(expected, UnionHitTestOpaqueness(r2, o2, r1, o1)); \
  } while (false)

TEST(HitTestOpaquenessTest, BothTransparent) {
  EXPECT_OPAQUENESS(kTransparent, gfx::Rect(), kTransparent, gfx::Rect(),
                    kTransparent);
  EXPECT_OPAQUENESS(kTransparent, gfx::Rect(0, 0, 100, 100), kTransparent,
                    gfx::Rect(0, 0, 100, 100), kTransparent);
  EXPECT_OPAQUENESS(kTransparent, gfx::Rect(0, 0, 100, 100), kTransparent,
                    gfx::Rect(0, 100, 100, 100), kTransparent);
  EXPECT_OPAQUENESS(kTransparent, gfx::Rect(0, 0, 100, 100), kTransparent,
                    gfx::Rect(0, 0, 50, 100), kTransparent);
  EXPECT_OPAQUENESS(kTransparent, gfx::Rect(0, 0, 100, 100), kTransparent,
                    gfx::Rect(0, 200, 100, 100), kTransparent);
  EXPECT_OPAQUENESS(kTransparent, gfx::Rect(0, 0, 100, 100), kTransparent,
                    gfx::Rect(50, 50, 100, 100), kTransparent);
}

TEST(HitTestOpaquenessTest, BothOpaque) {
  EXPECT_OPAQUENESS(kOpaque, gfx::Rect(), kOpaque, gfx::Rect(), kOpaque);
  EXPECT_OPAQUENESS(kOpaque, gfx::Rect(0, 0, 100, 100), kOpaque,
                    gfx::Rect(0, 0, 100, 100), kOpaque);
  EXPECT_OPAQUENESS(kOpaque, gfx::Rect(0, 0, 100, 100), kOpaque,
                    gfx::Rect(0, 100, 100, 100), kOpaque);
  EXPECT_OPAQUENESS(kOpaque, gfx::Rect(0, 0, 100, 100), kOpaque,
                    gfx::Rect(0, 0, 50, 100), kOpaque);
  EXPECT_OPAQUENESS(kMixed, gfx::Rect(0, 0, 100, 100), kOpaque,
                    gfx::Rect(0, 200, 100, 100), kOpaque);
  EXPECT_OPAQUENESS(kMixed, gfx::Rect(0, 0, 100, 100), kOpaque,
                    gfx::Rect(50, 50, 100, 100), kOpaque);
}

TEST(HitTestOpaquenessTest, BothMixed) {
  EXPECT_OPAQUENESS(kMixed, gfx::Rect(), kMixed, gfx::Rect(), kMixed);
  EXPECT_OPAQUENESS(kMixed, gfx::Rect(0, 0, 100, 100), kMixed,
                    gfx::Rect(0, 0, 100, 100), kMixed);
  EXPECT_OPAQUENESS(kMixed, gfx::Rect(0, 0, 100, 100), kMixed,
                    gfx::Rect(0, 100, 100, 100), kMixed);
  EXPECT_OPAQUENESS(kMixed, gfx::Rect(0, 0, 100, 100), kMixed,
                    gfx::Rect(0, 0, 50, 100), kMixed);
  EXPECT_OPAQUENESS(kMixed, gfx::Rect(0, 0, 100, 100), kMixed,
                    gfx::Rect(0, 200, 100, 100), kMixed);
  EXPECT_OPAQUENESS(kMixed, gfx::Rect(0, 0, 100, 100), kMixed,
                    gfx::Rect(50, 50, 100, 100), kMixed);
}

TEST(HitTestOpaquenessTest, MixedAndTransparent) {
  EXPECT_OPAQUENESS(kTransparent, gfx::Rect(), kMixed,
                    gfx::Rect(0, 0, 100, 100), kTransparent);
  EXPECT_OPAQUENESS(kTransparent, gfx::Rect(), kMixed,
                    gfx::Rect(0, 0, 100, 100), kTransparent);
  EXPECT_OPAQUENESS(kMixed, gfx::Rect(0, 0, 100, 100), kMixed,
                    gfx::Rect(0, 0, 100, 100), kTransparent);
  EXPECT_OPAQUENESS(kMixed, gfx::Rect(0, 0, 100, 100), kMixed,
                    gfx::Rect(0, 100, 100, 100), kTransparent);
  EXPECT_OPAQUENESS(kMixed, gfx::Rect(0, 0, 100, 100), kMixed,
                    gfx::Rect(0, 100, 100, 100), kTransparent);
  EXPECT_OPAQUENESS(kMixed, gfx::Rect(0, 0, 100, 100), kMixed,
                    gfx::Rect(50, 50, 100, 100), kTransparent);
}

TEST(HitTestOpaquenessTest, MixedAndOpaque) {
  EXPECT_OPAQUENESS(kMixed, gfx::Rect(0, 0, 100, 100), kMixed, gfx::Rect(),
                    kOpaque);
  EXPECT_OPAQUENESS(kOpaque, gfx::Rect(), kMixed, gfx::Rect(0, 0, 100, 100),
                    kOpaque);
  EXPECT_OPAQUENESS(kOpaque, gfx::Rect(0, 0, 100, 100), kMixed,
                    gfx::Rect(0, 0, 100, 100), kOpaque);
  EXPECT_OPAQUENESS(kMixed, gfx::Rect(0, 0, 100, 100), kMixed,
                    gfx::Rect(0, 100, 100, 100), kOpaque);
  EXPECT_OPAQUENESS(kMixed, gfx::Rect(0, 0, 100, 100), kMixed,
                    gfx::Rect(0, 100, 100, 100), kOpaque);
  EXPECT_OPAQUENESS(kMixed, gfx::Rect(0, 0, 100, 100), kMixed,
                    gfx::Rect(50, 50, 100, 100), kOpaque);
}

TEST(HitTestOpaquenessTest, TransparentAndOpaque) {
  EXPECT_OPAQUENESS(kTransparent, gfx::Rect(0, 0, 100, 100), kTransparent,
                    gfx::Rect(), kOpaque);
  EXPECT_OPAQUENESS(kOpaque, gfx::Rect(), kTransparent,
                    gfx::Rect(0, 0, 100, 100), kOpaque);
  EXPECT_OPAQUENESS(kOpaque, gfx::Rect(0, 0, 100, 100), kTransparent,
                    gfx::Rect(0, 0, 100, 100), kOpaque);
  EXPECT_OPAQUENESS(kMixed, gfx::Rect(0, 0, 100, 100), kTransparent,
                    gfx::Rect(0, 100, 100, 100), kOpaque);
  EXPECT_OPAQUENESS(kMixed, gfx::Rect(0, 0, 100, 100), kTransparent,
                    gfx::Rect(0, 100, 100, 100), kOpaque);
  EXPECT_OPAQUENESS(kMixed, gfx::Rect(0, 0, 100, 100), kTransparent,
                    gfx::Rect(50, 50, 100, 100), kOpaque);
}

}  // anonymous namespace
}  // namespace cc
