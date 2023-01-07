// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/clear_for_opaque_raster.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace cc {

TEST(ClearForOpaqueRasterTest, NoTransform) {
  const gfx::Vector2dF translation;
  const gfx::Vector2dF scale(1, 1);
  const gfx::Size content_size(100, 100);
  const gfx::Rect bitmap_rect(content_size);
  gfx::Rect inner_rect;
  gfx::Rect outer_rect;
  EXPECT_FALSE(CalculateClearForOpaqueRasterRects(
      translation, scale, content_size, bitmap_rect, bitmap_rect, outer_rect,
      inner_rect));
  EXPECT_FALSE(CalculateClearForOpaqueRasterRects(
      translation, scale, content_size, bitmap_rect, gfx::Rect(25, 25, 50, 50),
      outer_rect, inner_rect));
}

TEST(ClearForOpaqueRasterTest, WithTranslation) {
  const gfx::Vector2dF translation(0.3f, 0.7f);
  const gfx::Vector2dF scale(1, 1);
  const gfx::Size content_size(100, 100);
  const gfx::Rect bitmap_rect(content_size);
  gfx::Rect inner_rect;
  gfx::Rect outer_rect;

  // Full playback (touching all edges).
  EXPECT_TRUE(CalculateClearForOpaqueRasterRects(
      translation, scale, content_size, bitmap_rect, bitmap_rect, outer_rect,
      inner_rect));
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100), outer_rect);
  EXPECT_EQ(gfx::Rect(1, 1, 99, 99), inner_rect);

  // Touches the left edge only.
  EXPECT_TRUE(CalculateClearForOpaqueRasterRects(
      translation, scale, content_size, bitmap_rect, gfx::Rect(0, 25, 50, 50),
      outer_rect, inner_rect));
  EXPECT_EQ(gfx::Rect(0, 25, 50, 50), outer_rect);
  EXPECT_EQ(gfx::Rect(1, 25, 49, 50), inner_rect);

  // Touches the top edge only.
  EXPECT_TRUE(CalculateClearForOpaqueRasterRects(
      translation, scale, content_size, bitmap_rect, gfx::Rect(25, 0, 50, 50),
      outer_rect, inner_rect));
  EXPECT_EQ(gfx::Rect(25, 0, 50, 50), outer_rect);
  EXPECT_EQ(gfx::Rect(25, 1, 50, 49), inner_rect);

  // Touches the right edge only.
  EXPECT_FALSE(CalculateClearForOpaqueRasterRects(
      translation, scale, content_size, bitmap_rect, gfx::Rect(50, 25, 50, 50),
      outer_rect, inner_rect));

  // Touches the bottom edge only.
  EXPECT_FALSE(CalculateClearForOpaqueRasterRects(
      translation, scale, content_size, bitmap_rect, gfx::Rect(25, 50, 50, 50),
      outer_rect, inner_rect));

  // Touches no edges.
  EXPECT_FALSE(CalculateClearForOpaqueRasterRects(
      translation, scale, content_size, bitmap_rect, gfx::Rect(1, 1, 98, 98),
      outer_rect, inner_rect));
}

TEST(ClearForOpaqueRasterTest, WithScale) {
  const gfx::Vector2dF translation;
  const gfx::Vector2dF scale(1.5f, 1.5f);
  const gfx::Size content_size(100, 100);
  const gfx::Rect bitmap_rect(content_size);
  gfx::Rect inner_rect;
  gfx::Rect outer_rect;

  // Full playback (touching all edges).
  EXPECT_TRUE(CalculateClearForOpaqueRasterRects(
      translation, scale, content_size, bitmap_rect, bitmap_rect, outer_rect,
      inner_rect));
  EXPECT_EQ(gfx::Rect(0, 0, 101, 101), outer_rect);
  EXPECT_EQ(gfx::Rect(0, 0, 99, 99), inner_rect);

  // Touches the left edge only.
  EXPECT_FALSE(CalculateClearForOpaqueRasterRects(
      translation, scale, content_size, bitmap_rect, gfx::Rect(0, 25, 50, 50),
      outer_rect, inner_rect));

  // Touches the top edge only.
  EXPECT_FALSE(CalculateClearForOpaqueRasterRects(
      translation, scale, content_size, bitmap_rect, gfx::Rect(25, 0, 50, 50),
      outer_rect, inner_rect));

  // Touches the right edge only.
  EXPECT_TRUE(CalculateClearForOpaqueRasterRects(
      translation, scale, content_size, bitmap_rect, gfx::Rect(50, 25, 50, 50),
      outer_rect, inner_rect));
  EXPECT_EQ(gfx::Rect(50, 25, 51, 50), outer_rect);
  EXPECT_EQ(gfx::Rect(50, 25, 49, 50), inner_rect);

  // Touches the bottom edge only.
  EXPECT_TRUE(CalculateClearForOpaqueRasterRects(
      translation, scale, content_size, bitmap_rect, gfx::Rect(25, 50, 50, 50),
      outer_rect, inner_rect));
  EXPECT_EQ(gfx::Rect(25, 50, 50, 51), outer_rect);
  EXPECT_EQ(gfx::Rect(25, 50, 50, 49), inner_rect);

  // Touches no edges.
  EXPECT_FALSE(CalculateClearForOpaqueRasterRects(
      translation, scale, content_size, bitmap_rect, gfx::Rect(1, 1, 98, 98),
      outer_rect, inner_rect));
}

TEST(ClearForOpaqueRasterTest, WithTranslationAndScale) {
  const gfx::Vector2dF translation(0.3f, 0.7f);
  const gfx::Vector2dF scale(1.5f, 1.5f);
  const gfx::Size content_size(100, 100);
  const gfx::Rect bitmap_rect(content_size);
  gfx::Rect inner_rect;
  gfx::Rect outer_rect;

  // Full playback (touching all edges).
  EXPECT_TRUE(CalculateClearForOpaqueRasterRects(
      translation, scale, content_size, bitmap_rect, bitmap_rect, outer_rect,
      inner_rect));
  EXPECT_EQ(gfx::Rect(0, 0, 101, 101), outer_rect);
  EXPECT_EQ(gfx::Rect(1, 1, 98, 98), inner_rect);

  // Touches the left edge only.
  EXPECT_TRUE(CalculateClearForOpaqueRasterRects(
      translation, scale, content_size, bitmap_rect, gfx::Rect(0, 25, 50, 50),
      outer_rect, inner_rect));
  EXPECT_EQ(gfx::Rect(0, 25, 50, 50), outer_rect);
  EXPECT_EQ(gfx::Rect(1, 25, 49, 50), inner_rect);

  // Touches the top edge only.
  EXPECT_TRUE(CalculateClearForOpaqueRasterRects(
      translation, scale, content_size, bitmap_rect, gfx::Rect(25, 0, 50, 50),
      outer_rect, inner_rect));
  EXPECT_EQ(gfx::Rect(25, 0, 50, 50), outer_rect);
  EXPECT_EQ(gfx::Rect(25, 1, 50, 49), inner_rect);

  // Touches the right edge only.
  EXPECT_TRUE(CalculateClearForOpaqueRasterRects(
      translation, scale, content_size, bitmap_rect, gfx::Rect(50, 25, 50, 50),
      outer_rect, inner_rect));
  EXPECT_EQ(gfx::Rect(50, 25, 51, 50), outer_rect);
  EXPECT_EQ(gfx::Rect(50, 25, 49, 50), inner_rect);

  // Touches the bottom edge only.
  EXPECT_TRUE(CalculateClearForOpaqueRasterRects(
      translation, scale, content_size, bitmap_rect, gfx::Rect(25, 50, 50, 50),
      outer_rect, inner_rect));
  EXPECT_EQ(gfx::Rect(25, 50, 50, 51), outer_rect);
  EXPECT_EQ(gfx::Rect(25, 50, 50, 49), inner_rect);

  // Touches no edges.
  EXPECT_FALSE(CalculateClearForOpaqueRasterRects(
      translation, scale, content_size, bitmap_rect, gfx::Rect(1, 1, 98, 98),
      outer_rect, inner_rect));

  // With bitmap_rect non-zero offset.
  EXPECT_TRUE(CalculateClearForOpaqueRasterRects(
      translation, scale, content_size, gfx::Rect(25, 25, 75, 75),
      gfx::Rect(50, 50, 50, 50), outer_rect, inner_rect));
  EXPECT_EQ(gfx::Rect(25, 25, 51, 51), outer_rect);
  EXPECT_EQ(gfx::Rect(25, 25, 49, 49), inner_rect);
}

TEST(ClearForOpaqueRasterTest, PlaybackRectBelowContentRect) {
  const gfx::Vector2dF translation(0.0f, 0.1f);
  const gfx::Vector2dF scale(1.0f, 1.0f);
  const gfx::Size content_size(100, 100);
  const gfx::Rect bitmap_rect(50, 50, 100, 100);
  const gfx::Rect playback_rect(50, 100, 100, 3);
  gfx::Rect inner_rect;
  gfx::Rect outer_rect;

  EXPECT_FALSE(CalculateClearForOpaqueRasterRects(
      translation, scale, content_size, bitmap_rect, playback_rect, outer_rect,
      inner_rect));
}

}  // namespace cc
