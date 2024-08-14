// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/tiling_data.h"

#include <algorithm>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

int NumTiles(const gfx::Size& max_texture_size,
             const gfx::Rect& tiling_rect,
             int border_texels) {
  TilingData tiling(max_texture_size, tiling_rect, border_texels);
  int num_tiles = tiling.num_tiles_x() * tiling.num_tiles_y();

  // Assert no overflow.
  EXPECT_GE(num_tiles, 0);
  if (num_tiles > 0)
    EXPECT_EQ(num_tiles / tiling.num_tiles_x(), tiling.num_tiles_y());

  return num_tiles;
}

int XIndex(const gfx::Size& max_texture_size,
           const gfx::Rect& tiling_rect,
           int border_texels,
           int x_coord) {
  TilingData tiling(max_texture_size, tiling_rect, border_texels);
  return tiling.TileXIndexFromSrcCoord(x_coord);
}

int YIndex(const gfx::Size& max_texture_size,
           const gfx::Rect& tiling_rect,
           int border_texels,
           int y_coord) {
  TilingData tiling(max_texture_size, tiling_rect, border_texels);
  return tiling.TileYIndexFromSrcCoord(y_coord);
}

int MinBorderXIndex(const gfx::Size& max_texture_size,
                    const gfx::Rect& tiling_rect,
                    int border_texels,
                    int x_coord) {
  TilingData tiling(max_texture_size, tiling_rect, border_texels);
  return tiling.FirstBorderTileXIndexFromSrcCoord(x_coord);
}

int MinBorderYIndex(const gfx::Size& max_texture_size,
                    const gfx::Rect& tiling_rect,
                    int border_texels,
                    int y_coord) {
  TilingData tiling(max_texture_size, tiling_rect, border_texels);
  return tiling.FirstBorderTileYIndexFromSrcCoord(y_coord);
}

int MaxBorderXIndex(const gfx::Size& max_texture_size,
                    const gfx::Rect& tiling_rect,
                    int border_texels,
                    int x_coord) {
  TilingData tiling(max_texture_size, tiling_rect, border_texels);
  return tiling.LastBorderTileXIndexFromSrcCoord(x_coord);
}

int MaxBorderYIndex(const gfx::Size& max_texture_size,
                    const gfx::Rect& tiling_rect,
                    int border_texels,
                    int y_coord) {
  TilingData tiling(max_texture_size, tiling_rect, border_texels);
  return tiling.LastBorderTileYIndexFromSrcCoord(y_coord);
}

int PosX(const gfx::Size& max_texture_size,
         const gfx::Rect& tiling_rect,
         int border_texels,
         int x_index) {
  TilingData tiling(max_texture_size, tiling_rect, border_texels);
  return tiling.TilePositionX(x_index);
}

int PosY(const gfx::Size& max_texture_size,
         const gfx::Rect& tiling_rect,
         int border_texels,
         int y_index) {
  TilingData tiling(max_texture_size, tiling_rect, border_texels);
  return tiling.TilePositionY(y_index);
}

int SizeX(const gfx::Size& max_texture_size,
          const gfx::Rect& tiling_rect,
          int border_texels,
          int x_index) {
  TilingData tiling(max_texture_size, tiling_rect, border_texels);
  return tiling.TileSizeX(x_index);
}

int SizeY(const gfx::Size& max_texture_size,
          const gfx::Rect& tiling_rect,
          int border_texels,
          int y_index) {
  TilingData tiling(max_texture_size, tiling_rect, border_texels);
  return tiling.TileSizeY(y_index);
}

class TilingDataTest : public ::testing::TestWithParam<gfx::Vector2d> {
 public:
  static gfx::Vector2d offset() { return GetParam(); }
  static int OffsetX(int x) { return x + offset().x(); }
  static int OffsetY(int y) { return y + offset().y(); }
  static gfx::Rect OffsetRect(int w, int h) { return OffsetRect(0, 0, w, h); }
  static gfx::Rect OffsetRect(int x, int y, int w, int h) {
    return gfx::Rect(OffsetX(x), OffsetY(y), w, h);
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         TilingDataTest,
                         ::testing::Values(gfx::Vector2d(0, 0),
                                           gfx::Vector2d(50, 100),
                                           gfx::Vector2d(17, 31),
                                           gfx::Vector2d(10000, 15000)));

TEST_P(TilingDataTest, NumTiles_NoTiling) {
  EXPECT_EQ(1, NumTiles(gfx::Size(16, 16), OffsetRect(16, 16), 0));
  EXPECT_EQ(1, NumTiles(gfx::Size(16, 16), OffsetRect(15, 15), 1));
  EXPECT_EQ(1, NumTiles(gfx::Size(16, 16), OffsetRect(16, 16), 1));
  EXPECT_EQ(1, NumTiles(gfx::Size(16, 16), OffsetRect(1, 16), 0));
  EXPECT_EQ(1, NumTiles(gfx::Size(15, 15), OffsetRect(15, 15), 1));
  EXPECT_EQ(1, NumTiles(gfx::Size(32, 16), OffsetRect(32, 16), 0));
  EXPECT_EQ(1, NumTiles(gfx::Size(32, 16), OffsetRect(32, 16), 1));
}

TEST_P(TilingDataTest, NumTiles_TilingNoBorders) {
  EXPECT_EQ(0, NumTiles(gfx::Size(0, 0), OffsetRect(0, 0), 0));
  EXPECT_EQ(0, NumTiles(gfx::Size(0, 0), OffsetRect(4, 0), 0));
  EXPECT_EQ(0, NumTiles(gfx::Size(0, 0), OffsetRect(0, 4), 0));
  EXPECT_EQ(0, NumTiles(gfx::Size(4, 4), OffsetRect(4, 0), 0));
  EXPECT_EQ(0, NumTiles(gfx::Size(4, 4), OffsetRect(0, 4), 0));
  EXPECT_EQ(0, NumTiles(gfx::Size(0, 0), OffsetRect(1, 1), 0));

  EXPECT_EQ(1, NumTiles(gfx::Size(1, 1), OffsetRect(1, 1), 0));
  EXPECT_EQ(2, NumTiles(gfx::Size(1, 1), OffsetRect(1, 2), 0));
  EXPECT_EQ(2, NumTiles(gfx::Size(1, 1), OffsetRect(2, 1), 0));
  EXPECT_EQ(1, NumTiles(gfx::Size(2, 2), OffsetRect(1, 1), 0));
  EXPECT_EQ(1, NumTiles(gfx::Size(2, 2), OffsetRect(1, 2), 0));
  EXPECT_EQ(1, NumTiles(gfx::Size(2, 2), OffsetRect(2, 1), 0));
  EXPECT_EQ(1, NumTiles(gfx::Size(2, 2), OffsetRect(2, 2), 0));
  EXPECT_EQ(1, NumTiles(gfx::Size(3, 3), OffsetRect(3, 3), 0));

  EXPECT_EQ(1, NumTiles(gfx::Size(4, 4), OffsetRect(1, 4), 0));
  EXPECT_EQ(1, NumTiles(gfx::Size(4, 4), OffsetRect(2, 4), 0));
  EXPECT_EQ(1, NumTiles(gfx::Size(4, 4), OffsetRect(3, 4), 0));
  EXPECT_EQ(1, NumTiles(gfx::Size(4, 4), OffsetRect(4, 4), 0));
  EXPECT_EQ(2, NumTiles(gfx::Size(4, 4), OffsetRect(5, 4), 0));
  EXPECT_EQ(2, NumTiles(gfx::Size(4, 4), OffsetRect(6, 4), 0));
  EXPECT_EQ(2, NumTiles(gfx::Size(4, 4), OffsetRect(7, 4), 0));
  EXPECT_EQ(2, NumTiles(gfx::Size(4, 4), OffsetRect(8, 4), 0));
  EXPECT_EQ(3, NumTiles(gfx::Size(4, 4), OffsetRect(9, 4), 0));
  EXPECT_EQ(3, NumTiles(gfx::Size(4, 4), OffsetRect(10, 4), 0));
  EXPECT_EQ(3, NumTiles(gfx::Size(4, 4), OffsetRect(11, 4), 0));

  EXPECT_EQ(1, NumTiles(gfx::Size(5, 5), OffsetRect(1, 5), 0));
  EXPECT_EQ(1, NumTiles(gfx::Size(5, 5), OffsetRect(2, 5), 0));
  EXPECT_EQ(1, NumTiles(gfx::Size(5, 5), OffsetRect(3, 5), 0));
  EXPECT_EQ(1, NumTiles(gfx::Size(5, 5), OffsetRect(4, 5), 0));
  EXPECT_EQ(1, NumTiles(gfx::Size(5, 5), OffsetRect(5, 5), 0));
  EXPECT_EQ(2, NumTiles(gfx::Size(5, 5), OffsetRect(6, 5), 0));
  EXPECT_EQ(2, NumTiles(gfx::Size(5, 5), OffsetRect(7, 5), 0));
  EXPECT_EQ(2, NumTiles(gfx::Size(5, 5), OffsetRect(8, 5), 0));
  EXPECT_EQ(2, NumTiles(gfx::Size(5, 5), OffsetRect(9, 5), 0));
  EXPECT_EQ(2, NumTiles(gfx::Size(5, 5), OffsetRect(10, 5), 0));
  EXPECT_EQ(3, NumTiles(gfx::Size(5, 5), OffsetRect(11, 5), 0));

  EXPECT_EQ(1, NumTiles(gfx::Size(16, 16), OffsetRect(16, 16), 0));
  EXPECT_EQ(1, NumTiles(gfx::Size(17, 17), OffsetRect(16, 16), 0));
  EXPECT_EQ(4, NumTiles(gfx::Size(15, 15), OffsetRect(16, 16), 0));
  EXPECT_EQ(4, NumTiles(gfx::Size(8, 8), OffsetRect(16, 16), 0));
  EXPECT_EQ(6, NumTiles(gfx::Size(8, 8), OffsetRect(17, 16), 0));

  EXPECT_EQ(8, NumTiles(gfx::Size(5, 8), OffsetRect(17, 16), 0));
}

TEST_P(TilingDataTest, NumTiles_TilingWithBorders) {
  EXPECT_EQ(0, NumTiles(gfx::Size(0, 0), OffsetRect(0, 0), 1));
  EXPECT_EQ(0, NumTiles(gfx::Size(0, 0), OffsetRect(4, 0), 1));
  EXPECT_EQ(0, NumTiles(gfx::Size(0, 0), OffsetRect(0, 4), 1));
  EXPECT_EQ(0, NumTiles(gfx::Size(4, 4), OffsetRect(4, 0), 1));
  EXPECT_EQ(0, NumTiles(gfx::Size(4, 4), OffsetRect(0, 4), 1));
  EXPECT_EQ(0, NumTiles(gfx::Size(0, 0), OffsetRect(1, 1), 1));

  EXPECT_EQ(1, NumTiles(gfx::Size(1, 1), OffsetRect(1, 1), 1));
  EXPECT_EQ(0, NumTiles(gfx::Size(1, 1), OffsetRect(1, 2), 1));
  EXPECT_EQ(0, NumTiles(gfx::Size(1, 1), OffsetRect(2, 1), 1));
  EXPECT_EQ(1, NumTiles(gfx::Size(2, 2), OffsetRect(1, 1), 1));
  EXPECT_EQ(1, NumTiles(gfx::Size(2, 2), OffsetRect(1, 2), 1));
  EXPECT_EQ(1, NumTiles(gfx::Size(2, 2), OffsetRect(2, 1), 1));
  EXPECT_EQ(1, NumTiles(gfx::Size(2, 2), OffsetRect(2, 2), 1));

  EXPECT_EQ(1, NumTiles(gfx::Size(3, 3), OffsetRect(1, 3), 1));
  EXPECT_EQ(1, NumTiles(gfx::Size(3, 3), OffsetRect(2, 3), 1));
  EXPECT_EQ(1, NumTiles(gfx::Size(3, 3), OffsetRect(3, 3), 1));
  EXPECT_EQ(2, NumTiles(gfx::Size(3, 3), OffsetRect(4, 3), 1));
  EXPECT_EQ(3, NumTiles(gfx::Size(3, 3), OffsetRect(5, 3), 1));
  EXPECT_EQ(4, NumTiles(gfx::Size(3, 3), OffsetRect(6, 3), 1));
  EXPECT_EQ(5, NumTiles(gfx::Size(3, 3), OffsetRect(7, 3), 1));

  EXPECT_EQ(1, NumTiles(gfx::Size(4, 4), OffsetRect(1, 4), 1));
  EXPECT_EQ(1, NumTiles(gfx::Size(4, 4), OffsetRect(2, 4), 1));
  EXPECT_EQ(1, NumTiles(gfx::Size(4, 4), OffsetRect(3, 4), 1));
  EXPECT_EQ(1, NumTiles(gfx::Size(4, 4), OffsetRect(4, 4), 1));
  EXPECT_EQ(2, NumTiles(gfx::Size(4, 4), OffsetRect(5, 4), 1));
  EXPECT_EQ(2, NumTiles(gfx::Size(4, 4), OffsetRect(6, 4), 1));
  EXPECT_EQ(3, NumTiles(gfx::Size(4, 4), OffsetRect(7, 4), 1));
  EXPECT_EQ(3, NumTiles(gfx::Size(4, 4), OffsetRect(8, 4), 1));
  EXPECT_EQ(4, NumTiles(gfx::Size(4, 4), OffsetRect(9, 4), 1));
  EXPECT_EQ(4, NumTiles(gfx::Size(4, 4), OffsetRect(10, 4), 1));
  EXPECT_EQ(5, NumTiles(gfx::Size(4, 4), OffsetRect(11, 4), 1));

  EXPECT_EQ(1, NumTiles(gfx::Size(5, 5), OffsetRect(1, 5), 1));
  EXPECT_EQ(1, NumTiles(gfx::Size(5, 5), OffsetRect(2, 5), 1));
  EXPECT_EQ(1, NumTiles(gfx::Size(5, 5), OffsetRect(3, 5), 1));
  EXPECT_EQ(1, NumTiles(gfx::Size(5, 5), OffsetRect(4, 5), 1));
  EXPECT_EQ(1, NumTiles(gfx::Size(5, 5), OffsetRect(5, 5), 1));
  EXPECT_EQ(2, NumTiles(gfx::Size(5, 5), OffsetRect(6, 5), 1));
  EXPECT_EQ(2, NumTiles(gfx::Size(5, 5), OffsetRect(7, 5), 1));
  EXPECT_EQ(2, NumTiles(gfx::Size(5, 5), OffsetRect(8, 5), 1));
  EXPECT_EQ(3, NumTiles(gfx::Size(5, 5), OffsetRect(9, 5), 1));
  EXPECT_EQ(3, NumTiles(gfx::Size(5, 5), OffsetRect(10, 5), 1));
  EXPECT_EQ(3, NumTiles(gfx::Size(5, 5), OffsetRect(11, 5), 1));

  EXPECT_EQ(30, NumTiles(gfx::Size(8, 5), OffsetRect(16, 32), 1));
}

TEST_P(TilingDataTest, TileXIndexFromSrcCoord) {
  EXPECT_EQ(0, XIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetX(0)));
  EXPECT_EQ(0, XIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetX(1)));
  EXPECT_EQ(0, XIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetX(2)));
  EXPECT_EQ(1, XIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetX(3)));
  EXPECT_EQ(1, XIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetX(4)));
  EXPECT_EQ(1, XIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetX(5)));
  EXPECT_EQ(2, XIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetX(6)));
  EXPECT_EQ(2, XIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetX(7)));
  EXPECT_EQ(2, XIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetX(8)));
  EXPECT_EQ(3, XIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetX(9)));
  EXPECT_EQ(3, XIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetX(10)));
  EXPECT_EQ(3, XIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetX(11)));

  EXPECT_EQ(0, XIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetX(0)));
  EXPECT_EQ(0, XIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetX(1)));
  EXPECT_EQ(1, XIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetX(2)));
  EXPECT_EQ(2, XIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetX(3)));
  EXPECT_EQ(3, XIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetX(4)));
  EXPECT_EQ(4, XIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetX(5)));
  EXPECT_EQ(5, XIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetX(6)));
  EXPECT_EQ(6, XIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetX(7)));
  EXPECT_EQ(7, XIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetX(8)));
  EXPECT_EQ(7, XIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetX(9)));
  EXPECT_EQ(7, XIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetX(10)));
  EXPECT_EQ(7, XIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetX(11)));

  EXPECT_EQ(0, XIndex(gfx::Size(1, 1), OffsetRect(1, 1), 0, OffsetX(0)));
  EXPECT_EQ(0, XIndex(gfx::Size(2, 2), OffsetRect(2, 2), 0, OffsetX(0)));
  EXPECT_EQ(0, XIndex(gfx::Size(2, 2), OffsetRect(2, 2), 0, OffsetX(1)));
  EXPECT_EQ(0, XIndex(gfx::Size(3, 3), OffsetRect(3, 3), 0, OffsetX(0)));
  EXPECT_EQ(0, XIndex(gfx::Size(3, 3), OffsetRect(3, 3), 0, OffsetX(1)));
  EXPECT_EQ(0, XIndex(gfx::Size(3, 3), OffsetRect(3, 3), 0, OffsetX(2)));

  EXPECT_EQ(0, XIndex(gfx::Size(3, 3), OffsetRect(4, 3), 0, OffsetX(0)));
  EXPECT_EQ(0, XIndex(gfx::Size(3, 3), OffsetRect(4, 3), 0, OffsetX(1)));
  EXPECT_EQ(0, XIndex(gfx::Size(3, 3), OffsetRect(4, 3), 0, OffsetX(2)));
  EXPECT_EQ(1, XIndex(gfx::Size(3, 3), OffsetRect(4, 3), 0, OffsetX(3)));

  EXPECT_EQ(0, XIndex(gfx::Size(1, 1), OffsetRect(1, 1), 1, OffsetX(0)));
  EXPECT_EQ(0, XIndex(gfx::Size(2, 2), OffsetRect(2, 2), 1, OffsetX(0)));
  EXPECT_EQ(0, XIndex(gfx::Size(2, 2), OffsetRect(2, 2), 1, OffsetX(1)));
  EXPECT_EQ(0, XIndex(gfx::Size(3, 3), OffsetRect(3, 3), 1, OffsetX(0)));
  EXPECT_EQ(0, XIndex(gfx::Size(3, 3), OffsetRect(3, 3), 1, OffsetX(1)));
  EXPECT_EQ(0, XIndex(gfx::Size(3, 3), OffsetRect(3, 3), 1, OffsetX(2)));

  EXPECT_EQ(0, XIndex(gfx::Size(3, 3), OffsetRect(4, 3), 1, OffsetX(0)));
  EXPECT_EQ(0, XIndex(gfx::Size(3, 3), OffsetRect(4, 3), 1, OffsetX(1)));
  EXPECT_EQ(1, XIndex(gfx::Size(3, 3), OffsetRect(4, 3), 1, OffsetX(2)));
  EXPECT_EQ(1, XIndex(gfx::Size(3, 3), OffsetRect(4, 3), 1, OffsetX(3)));
}

TEST_P(TilingDataTest, FirstBorderTileXIndexFromSrcCoord) {
  EXPECT_EQ(
      0, MinBorderXIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetX(0)));
  EXPECT_EQ(
      0, MinBorderXIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetX(1)));
  EXPECT_EQ(
      0, MinBorderXIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetX(2)));
  EXPECT_EQ(
      1, MinBorderXIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetX(3)));
  EXPECT_EQ(
      1, MinBorderXIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetX(4)));
  EXPECT_EQ(
      1, MinBorderXIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetX(5)));
  EXPECT_EQ(
      2, MinBorderXIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetX(6)));
  EXPECT_EQ(
      2, MinBorderXIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetX(7)));
  EXPECT_EQ(
      2, MinBorderXIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetX(8)));
  EXPECT_EQ(
      3, MinBorderXIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetX(9)));
  EXPECT_EQ(
      3, MinBorderXIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetX(10)));
  EXPECT_EQ(
      3, MinBorderXIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetX(11)));

  EXPECT_EQ(
      0, MinBorderXIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetX(0)));
  EXPECT_EQ(
      0, MinBorderXIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetX(1)));
  EXPECT_EQ(
      0, MinBorderXIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetX(2)));
  EXPECT_EQ(
      1, MinBorderXIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetX(3)));
  EXPECT_EQ(
      2, MinBorderXIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetX(4)));
  EXPECT_EQ(
      3, MinBorderXIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetX(5)));
  EXPECT_EQ(
      4, MinBorderXIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetX(6)));
  EXPECT_EQ(
      5, MinBorderXIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetX(7)));
  EXPECT_EQ(
      6, MinBorderXIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetX(8)));
  EXPECT_EQ(
      7, MinBorderXIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetX(9)));
  EXPECT_EQ(
      7, MinBorderXIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetX(10)));
  EXPECT_EQ(
      7, MinBorderXIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetX(11)));

  EXPECT_EQ(0,
            MinBorderXIndex(gfx::Size(1, 1), OffsetRect(1, 1), 0, OffsetX(0)));
  EXPECT_EQ(0,
            MinBorderXIndex(gfx::Size(2, 2), OffsetRect(2, 2), 0, OffsetX(0)));
  EXPECT_EQ(0,
            MinBorderXIndex(gfx::Size(2, 2), OffsetRect(2, 2), 0, OffsetX(1)));
  EXPECT_EQ(0,
            MinBorderXIndex(gfx::Size(3, 3), OffsetRect(3, 3), 0, OffsetX(0)));
  EXPECT_EQ(0,
            MinBorderXIndex(gfx::Size(3, 3), OffsetRect(3, 3), 0, OffsetX(1)));
  EXPECT_EQ(0,
            MinBorderXIndex(gfx::Size(3, 3), OffsetRect(3, 3), 0, OffsetX(2)));

  EXPECT_EQ(0,
            MinBorderXIndex(gfx::Size(3, 3), OffsetRect(4, 3), 0, OffsetX(0)));
  EXPECT_EQ(0,
            MinBorderXIndex(gfx::Size(3, 3), OffsetRect(4, 3), 0, OffsetX(1)));
  EXPECT_EQ(0,
            MinBorderXIndex(gfx::Size(3, 3), OffsetRect(4, 3), 0, OffsetX(2)));
  EXPECT_EQ(1,
            MinBorderXIndex(gfx::Size(3, 3), OffsetRect(4, 3), 0, OffsetX(3)));

  EXPECT_EQ(0,
            MinBorderXIndex(gfx::Size(1, 1), OffsetRect(1, 1), 1, OffsetX(0)));
  EXPECT_EQ(0,
            MinBorderXIndex(gfx::Size(2, 2), OffsetRect(2, 2), 1, OffsetX(0)));
  EXPECT_EQ(0,
            MinBorderXIndex(gfx::Size(2, 2), OffsetRect(2, 2), 1, OffsetX(1)));
  EXPECT_EQ(0,
            MinBorderXIndex(gfx::Size(3, 3), OffsetRect(3, 3), 1, OffsetX(0)));
  EXPECT_EQ(0,
            MinBorderXIndex(gfx::Size(3, 3), OffsetRect(3, 3), 1, OffsetX(1)));
  EXPECT_EQ(0,
            MinBorderXIndex(gfx::Size(3, 3), OffsetRect(3, 3), 1, OffsetX(2)));

  EXPECT_EQ(0,
            MinBorderXIndex(gfx::Size(3, 3), OffsetRect(4, 3), 1, OffsetX(0)));
  EXPECT_EQ(0,
            MinBorderXIndex(gfx::Size(3, 3), OffsetRect(4, 3), 1, OffsetX(1)));
  EXPECT_EQ(0,
            MinBorderXIndex(gfx::Size(3, 3), OffsetRect(4, 3), 1, OffsetX(2)));
  EXPECT_EQ(1,
            MinBorderXIndex(gfx::Size(3, 3), OffsetRect(4, 3), 1, OffsetX(3)));
}

TEST_P(TilingDataTest, LastBorderTileXIndexFromSrcCoord) {
  EXPECT_EQ(
      0, MaxBorderXIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetX(0)));
  EXPECT_EQ(
      0, MaxBorderXIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetX(1)));
  EXPECT_EQ(
      0, MaxBorderXIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetX(2)));
  EXPECT_EQ(
      1, MaxBorderXIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetX(3)));
  EXPECT_EQ(
      1, MaxBorderXIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetX(4)));
  EXPECT_EQ(
      1, MaxBorderXIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetX(5)));
  EXPECT_EQ(
      2, MaxBorderXIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetX(6)));
  EXPECT_EQ(
      2, MaxBorderXIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetX(7)));
  EXPECT_EQ(
      2, MaxBorderXIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetX(8)));
  EXPECT_EQ(
      3, MaxBorderXIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetX(9)));
  EXPECT_EQ(
      3, MaxBorderXIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetX(10)));
  EXPECT_EQ(
      3, MaxBorderXIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetX(11)));

  EXPECT_EQ(
      0, MaxBorderXIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetX(0)));
  EXPECT_EQ(
      1, MaxBorderXIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetX(1)));
  EXPECT_EQ(
      2, MaxBorderXIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetX(2)));
  EXPECT_EQ(
      3, MaxBorderXIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetX(3)));
  EXPECT_EQ(
      4, MaxBorderXIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetX(4)));
  EXPECT_EQ(
      5, MaxBorderXIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetX(5)));
  EXPECT_EQ(
      6, MaxBorderXIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetX(6)));
  EXPECT_EQ(
      7, MaxBorderXIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetX(7)));
  EXPECT_EQ(
      7, MaxBorderXIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetX(8)));
  EXPECT_EQ(
      7, MaxBorderXIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetX(9)));
  EXPECT_EQ(
      7, MaxBorderXIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetX(10)));
  EXPECT_EQ(
      7, MaxBorderXIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetX(11)));

  EXPECT_EQ(0,
            MaxBorderXIndex(gfx::Size(1, 1), OffsetRect(1, 1), 0, OffsetX(0)));
  EXPECT_EQ(0,
            MaxBorderXIndex(gfx::Size(2, 2), OffsetRect(2, 2), 0, OffsetX(0)));
  EXPECT_EQ(0,
            MaxBorderXIndex(gfx::Size(2, 2), OffsetRect(2, 2), 0, OffsetX(1)));
  EXPECT_EQ(0,
            MaxBorderXIndex(gfx::Size(3, 3), OffsetRect(3, 3), 0, OffsetX(0)));
  EXPECT_EQ(0,
            MaxBorderXIndex(gfx::Size(3, 3), OffsetRect(3, 3), 0, OffsetX(1)));
  EXPECT_EQ(0,
            MaxBorderXIndex(gfx::Size(3, 3), OffsetRect(3, 3), 0, OffsetX(2)));

  EXPECT_EQ(0,
            MaxBorderXIndex(gfx::Size(3, 3), OffsetRect(4, 3), 0, OffsetX(0)));
  EXPECT_EQ(0,
            MaxBorderXIndex(gfx::Size(3, 3), OffsetRect(4, 3), 0, OffsetX(1)));
  EXPECT_EQ(0,
            MaxBorderXIndex(gfx::Size(3, 3), OffsetRect(4, 3), 0, OffsetX(2)));
  EXPECT_EQ(1,
            MaxBorderXIndex(gfx::Size(3, 3), OffsetRect(4, 3), 0, OffsetX(3)));

  EXPECT_EQ(0,
            MaxBorderXIndex(gfx::Size(1, 1), OffsetRect(1, 1), 1, OffsetX(0)));
  EXPECT_EQ(0,
            MaxBorderXIndex(gfx::Size(2, 2), OffsetRect(2, 2), 1, OffsetX(0)));
  EXPECT_EQ(0,
            MaxBorderXIndex(gfx::Size(2, 2), OffsetRect(2, 2), 1, OffsetX(1)));
  EXPECT_EQ(0,
            MaxBorderXIndex(gfx::Size(3, 3), OffsetRect(3, 3), 1, OffsetX(0)));
  EXPECT_EQ(0,
            MaxBorderXIndex(gfx::Size(3, 3), OffsetRect(3, 3), 1, OffsetX(1)));
  EXPECT_EQ(0,
            MaxBorderXIndex(gfx::Size(3, 3), OffsetRect(3, 3), 1, OffsetX(2)));

  EXPECT_EQ(0,
            MaxBorderXIndex(gfx::Size(3, 3), OffsetRect(4, 3), 1, OffsetX(0)));
  EXPECT_EQ(1,
            MaxBorderXIndex(gfx::Size(3, 3), OffsetRect(4, 3), 1, OffsetX(1)));
  EXPECT_EQ(1,
            MaxBorderXIndex(gfx::Size(3, 3), OffsetRect(4, 3), 1, OffsetX(2)));
  EXPECT_EQ(1,
            MaxBorderXIndex(gfx::Size(3, 3), OffsetRect(4, 3), 1, OffsetX(3)));
}

TEST_P(TilingDataTest, TileYIndexFromSrcCoord) {
  EXPECT_EQ(0, YIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetY(0)));
  EXPECT_EQ(0, YIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetY(1)));
  EXPECT_EQ(0, YIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetY(2)));
  EXPECT_EQ(1, YIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetY(3)));
  EXPECT_EQ(1, YIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetY(4)));
  EXPECT_EQ(1, YIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetY(5)));
  EXPECT_EQ(2, YIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetY(6)));
  EXPECT_EQ(2, YIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetY(7)));
  EXPECT_EQ(2, YIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetY(8)));
  EXPECT_EQ(3, YIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetY(9)));
  EXPECT_EQ(3, YIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetY(10)));
  EXPECT_EQ(3, YIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetY(11)));

  EXPECT_EQ(0, YIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetY(0)));
  EXPECT_EQ(0, YIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetY(1)));
  EXPECT_EQ(1, YIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetY(2)));
  EXPECT_EQ(2, YIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetY(3)));
  EXPECT_EQ(3, YIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetY(4)));
  EXPECT_EQ(4, YIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetY(5)));
  EXPECT_EQ(5, YIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetY(6)));
  EXPECT_EQ(6, YIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetY(7)));
  EXPECT_EQ(7, YIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetY(8)));
  EXPECT_EQ(7, YIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetY(9)));
  EXPECT_EQ(7, YIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetY(10)));
  EXPECT_EQ(7, YIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetY(11)));

  EXPECT_EQ(0, YIndex(gfx::Size(1, 1), OffsetRect(1, 1), 0, OffsetY(0)));
  EXPECT_EQ(0, YIndex(gfx::Size(2, 2), OffsetRect(2, 2), 0, OffsetY(0)));
  EXPECT_EQ(0, YIndex(gfx::Size(2, 2), OffsetRect(2, 2), 0, OffsetY(1)));
  EXPECT_EQ(0, YIndex(gfx::Size(3, 3), OffsetRect(3, 3), 0, OffsetY(0)));
  EXPECT_EQ(0, YIndex(gfx::Size(3, 3), OffsetRect(3, 3), 0, OffsetY(1)));
  EXPECT_EQ(0, YIndex(gfx::Size(3, 3), OffsetRect(3, 3), 0, OffsetY(2)));

  EXPECT_EQ(0, YIndex(gfx::Size(3, 3), OffsetRect(3, 4), 0, OffsetY(0)));
  EXPECT_EQ(0, YIndex(gfx::Size(3, 3), OffsetRect(3, 4), 0, OffsetY(1)));
  EXPECT_EQ(0, YIndex(gfx::Size(3, 3), OffsetRect(3, 4), 0, OffsetY(2)));
  EXPECT_EQ(1, YIndex(gfx::Size(3, 3), OffsetRect(3, 4), 0, OffsetY(3)));

  EXPECT_EQ(0, YIndex(gfx::Size(1, 1), OffsetRect(1, 1), 1, OffsetY(0)));
  EXPECT_EQ(0, YIndex(gfx::Size(2, 2), OffsetRect(2, 2), 1, OffsetY(0)));
  EXPECT_EQ(0, YIndex(gfx::Size(2, 2), OffsetRect(2, 2), 1, OffsetY(1)));
  EXPECT_EQ(0, YIndex(gfx::Size(3, 3), OffsetRect(3, 3), 1, OffsetY(0)));
  EXPECT_EQ(0, YIndex(gfx::Size(3, 3), OffsetRect(3, 3), 1, OffsetY(1)));
  EXPECT_EQ(0, YIndex(gfx::Size(3, 3), OffsetRect(3, 3), 1, OffsetY(2)));

  EXPECT_EQ(0, YIndex(gfx::Size(3, 3), OffsetRect(3, 4), 1, OffsetY(0)));
  EXPECT_EQ(0, YIndex(gfx::Size(3, 3), OffsetRect(3, 4), 1, OffsetY(1)));
  EXPECT_EQ(1, YIndex(gfx::Size(3, 3), OffsetRect(3, 4), 1, OffsetY(2)));
  EXPECT_EQ(1, YIndex(gfx::Size(3, 3), OffsetRect(3, 4), 1, OffsetY(3)));
}

TEST_P(TilingDataTest, FirstBorderTileYIndexFromSrcCoord) {
  EXPECT_EQ(
      0, MinBorderYIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetY(0)));
  EXPECT_EQ(
      0, MinBorderYIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetY(1)));
  EXPECT_EQ(
      0, MinBorderYIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetY(2)));
  EXPECT_EQ(
      1, MinBorderYIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetY(3)));
  EXPECT_EQ(
      1, MinBorderYIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetY(4)));
  EXPECT_EQ(
      1, MinBorderYIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetY(5)));
  EXPECT_EQ(
      2, MinBorderYIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetY(6)));
  EXPECT_EQ(
      2, MinBorderYIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetY(7)));
  EXPECT_EQ(
      2, MinBorderYIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetY(8)));
  EXPECT_EQ(
      3, MinBorderYIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetY(9)));
  EXPECT_EQ(
      3, MinBorderYIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetY(10)));
  EXPECT_EQ(
      3, MinBorderYIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetY(11)));

  EXPECT_EQ(
      0, MinBorderYIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetY(0)));
  EXPECT_EQ(
      0, MinBorderYIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetY(1)));
  EXPECT_EQ(
      0, MinBorderYIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetY(2)));
  EXPECT_EQ(
      1, MinBorderYIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetY(3)));
  EXPECT_EQ(
      2, MinBorderYIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetY(4)));
  EXPECT_EQ(
      3, MinBorderYIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetY(5)));
  EXPECT_EQ(
      4, MinBorderYIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetY(6)));
  EXPECT_EQ(
      5, MinBorderYIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetY(7)));
  EXPECT_EQ(
      6, MinBorderYIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetY(8)));
  EXPECT_EQ(
      7, MinBorderYIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetY(9)));
  EXPECT_EQ(
      7, MinBorderYIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetY(10)));
  EXPECT_EQ(
      7, MinBorderYIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetY(11)));

  EXPECT_EQ(0,
            MinBorderYIndex(gfx::Size(1, 1), OffsetRect(1, 1), 0, OffsetY(0)));
  EXPECT_EQ(0,
            MinBorderYIndex(gfx::Size(2, 2), OffsetRect(2, 2), 0, OffsetY(0)));
  EXPECT_EQ(0,
            MinBorderYIndex(gfx::Size(2, 2), OffsetRect(2, 2), 0, OffsetY(1)));
  EXPECT_EQ(0,
            MinBorderYIndex(gfx::Size(3, 3), OffsetRect(3, 3), 0, OffsetY(0)));
  EXPECT_EQ(0,
            MinBorderYIndex(gfx::Size(3, 3), OffsetRect(3, 3), 0, OffsetY(1)));
  EXPECT_EQ(0,
            MinBorderYIndex(gfx::Size(3, 3), OffsetRect(3, 3), 0, OffsetY(2)));

  EXPECT_EQ(0,
            MinBorderYIndex(gfx::Size(3, 3), OffsetRect(3, 4), 0, OffsetY(0)));
  EXPECT_EQ(0,
            MinBorderYIndex(gfx::Size(3, 3), OffsetRect(3, 4), 0, OffsetY(1)));
  EXPECT_EQ(0,
            MinBorderYIndex(gfx::Size(3, 3), OffsetRect(3, 4), 0, OffsetY(2)));
  EXPECT_EQ(1,
            MinBorderYIndex(gfx::Size(3, 3), OffsetRect(3, 4), 0, OffsetY(3)));

  EXPECT_EQ(0,
            MinBorderYIndex(gfx::Size(1, 1), OffsetRect(1, 1), 1, OffsetY(0)));
  EXPECT_EQ(0,
            MinBorderYIndex(gfx::Size(2, 2), OffsetRect(2, 2), 1, OffsetY(0)));
  EXPECT_EQ(0,
            MinBorderYIndex(gfx::Size(2, 2), OffsetRect(2, 2), 1, OffsetY(1)));
  EXPECT_EQ(0,
            MinBorderYIndex(gfx::Size(3, 3), OffsetRect(3, 3), 1, OffsetY(0)));
  EXPECT_EQ(0,
            MinBorderYIndex(gfx::Size(3, 3), OffsetRect(3, 3), 1, OffsetY(1)));
  EXPECT_EQ(0,
            MinBorderYIndex(gfx::Size(3, 3), OffsetRect(3, 3), 1, OffsetY(2)));

  EXPECT_EQ(0,
            MinBorderYIndex(gfx::Size(3, 3), OffsetRect(3, 4), 1, OffsetY(0)));
  EXPECT_EQ(0,
            MinBorderYIndex(gfx::Size(3, 3), OffsetRect(3, 4), 1, OffsetY(1)));
  EXPECT_EQ(0,
            MinBorderYIndex(gfx::Size(3, 3), OffsetRect(3, 4), 1, OffsetY(2)));
  EXPECT_EQ(1,
            MinBorderYIndex(gfx::Size(3, 3), OffsetRect(3, 4), 1, OffsetY(3)));
}

TEST_P(TilingDataTest, LastBorderTileYIndexFromSrcCoord) {
  EXPECT_EQ(
      0, MaxBorderYIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetY(0)));
  EXPECT_EQ(
      0, MaxBorderYIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetY(1)));
  EXPECT_EQ(
      0, MaxBorderYIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetY(2)));
  EXPECT_EQ(
      1, MaxBorderYIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetY(3)));
  EXPECT_EQ(
      1, MaxBorderYIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetY(4)));
  EXPECT_EQ(
      1, MaxBorderYIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetY(5)));
  EXPECT_EQ(
      2, MaxBorderYIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetY(6)));
  EXPECT_EQ(
      2, MaxBorderYIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetY(7)));
  EXPECT_EQ(
      2, MaxBorderYIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetY(8)));
  EXPECT_EQ(
      3, MaxBorderYIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetY(9)));
  EXPECT_EQ(
      3, MaxBorderYIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetY(10)));
  EXPECT_EQ(
      3, MaxBorderYIndex(gfx::Size(3, 3), OffsetRect(10, 10), 0, OffsetY(11)));

  EXPECT_EQ(
      0, MaxBorderYIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetY(0)));
  EXPECT_EQ(
      1, MaxBorderYIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetY(1)));
  EXPECT_EQ(
      2, MaxBorderYIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetY(2)));
  EXPECT_EQ(
      3, MaxBorderYIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetY(3)));
  EXPECT_EQ(
      4, MaxBorderYIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetY(4)));
  EXPECT_EQ(
      5, MaxBorderYIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetY(5)));
  EXPECT_EQ(
      6, MaxBorderYIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetY(6)));
  EXPECT_EQ(
      7, MaxBorderYIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetY(7)));
  EXPECT_EQ(
      7, MaxBorderYIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetY(8)));
  EXPECT_EQ(
      7, MaxBorderYIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetY(9)));
  EXPECT_EQ(
      7, MaxBorderYIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetY(10)));
  EXPECT_EQ(
      7, MaxBorderYIndex(gfx::Size(3, 3), OffsetRect(10, 10), 1, OffsetY(11)));

  EXPECT_EQ(0,
            MaxBorderYIndex(gfx::Size(1, 1), OffsetRect(1, 1), 0, OffsetY(0)));
  EXPECT_EQ(0,
            MaxBorderYIndex(gfx::Size(2, 2), OffsetRect(2, 2), 0, OffsetY(0)));
  EXPECT_EQ(0,
            MaxBorderYIndex(gfx::Size(2, 2), OffsetRect(2, 2), 0, OffsetY(1)));
  EXPECT_EQ(0,
            MaxBorderYIndex(gfx::Size(3, 3), OffsetRect(3, 3), 0, OffsetY(0)));
  EXPECT_EQ(0,
            MaxBorderYIndex(gfx::Size(3, 3), OffsetRect(3, 3), 0, OffsetY(1)));
  EXPECT_EQ(0,
            MaxBorderYIndex(gfx::Size(3, 3), OffsetRect(3, 3), 0, OffsetY(2)));

  EXPECT_EQ(0,
            MaxBorderYIndex(gfx::Size(3, 3), OffsetRect(3, 4), 0, OffsetY(0)));
  EXPECT_EQ(0,
            MaxBorderYIndex(gfx::Size(3, 3), OffsetRect(3, 4), 0, OffsetY(1)));
  EXPECT_EQ(0,
            MaxBorderYIndex(gfx::Size(3, 3), OffsetRect(3, 4), 0, OffsetY(2)));
  EXPECT_EQ(1,
            MaxBorderYIndex(gfx::Size(3, 3), OffsetRect(3, 4), 0, OffsetY(3)));

  EXPECT_EQ(0,
            MaxBorderYIndex(gfx::Size(1, 1), OffsetRect(1, 1), 1, OffsetY(0)));
  EXPECT_EQ(0,
            MaxBorderYIndex(gfx::Size(2, 2), OffsetRect(2, 2), 1, OffsetY(0)));
  EXPECT_EQ(0,
            MaxBorderYIndex(gfx::Size(2, 2), OffsetRect(2, 2), 1, OffsetY(1)));
  EXPECT_EQ(0,
            MaxBorderYIndex(gfx::Size(3, 3), OffsetRect(3, 3), 1, OffsetY(0)));
  EXPECT_EQ(0,
            MaxBorderYIndex(gfx::Size(3, 3), OffsetRect(3, 3), 1, OffsetY(1)));
  EXPECT_EQ(0,
            MaxBorderYIndex(gfx::Size(3, 3), OffsetRect(3, 3), 1, OffsetY(2)));

  EXPECT_EQ(0,
            MaxBorderYIndex(gfx::Size(3, 3), OffsetRect(3, 4), 1, OffsetY(0)));
  EXPECT_EQ(1,
            MaxBorderYIndex(gfx::Size(3, 3), OffsetRect(3, 4), 1, OffsetY(1)));
  EXPECT_EQ(1,
            MaxBorderYIndex(gfx::Size(3, 3), OffsetRect(3, 4), 1, OffsetY(2)));
  EXPECT_EQ(1,
            MaxBorderYIndex(gfx::Size(3, 3), OffsetRect(3, 4), 1, OffsetY(3)));
}

TEST_P(TilingDataTest, TileSizeX) {
  EXPECT_EQ(5, SizeX(gfx::Size(5, 5), OffsetRect(5, 5), 0, 0));
  EXPECT_EQ(5, SizeX(gfx::Size(5, 5), OffsetRect(5, 5), 1, 0));

  EXPECT_EQ(5, SizeX(gfx::Size(5, 5), OffsetRect(6, 6), 0, 0));
  EXPECT_EQ(1, SizeX(gfx::Size(5, 5), OffsetRect(6, 6), 0, 1));
  EXPECT_EQ(4, SizeX(gfx::Size(5, 5), OffsetRect(6, 6), 1, 0));
  EXPECT_EQ(2, SizeX(gfx::Size(5, 5), OffsetRect(6, 6), 1, 1));

  EXPECT_EQ(5, SizeX(gfx::Size(5, 5), OffsetRect(8, 8), 0, 0));
  EXPECT_EQ(3, SizeX(gfx::Size(5, 5), OffsetRect(8, 8), 0, 1));
  EXPECT_EQ(4, SizeX(gfx::Size(5, 5), OffsetRect(8, 8), 1, 0));
  EXPECT_EQ(4, SizeX(gfx::Size(5, 5), OffsetRect(8, 8), 1, 1));

  EXPECT_EQ(5, SizeX(gfx::Size(5, 5), OffsetRect(10, 10), 0, 0));
  EXPECT_EQ(5, SizeX(gfx::Size(5, 5), OffsetRect(10, 10), 0, 1));
  EXPECT_EQ(4, SizeX(gfx::Size(5, 5), OffsetRect(10, 10), 1, 0));
  EXPECT_EQ(3, SizeX(gfx::Size(5, 5), OffsetRect(10, 10), 1, 1));
  EXPECT_EQ(3, SizeX(gfx::Size(5, 5), OffsetRect(10, 10), 1, 2));

  EXPECT_EQ(4, SizeX(gfx::Size(5, 5), OffsetRect(11, 11), 1, 2));
  EXPECT_EQ(3, SizeX(gfx::Size(5, 5), OffsetRect(12, 12), 1, 2));

  EXPECT_EQ(3, SizeX(gfx::Size(5, 9), OffsetRect(12, 17), 1, 2));
}

TEST_P(TilingDataTest, TileSizeY) {
  EXPECT_EQ(5, SizeY(gfx::Size(5, 5), OffsetRect(5, 5), 0, 0));
  EXPECT_EQ(5, SizeY(gfx::Size(5, 5), OffsetRect(5, 5), 1, 0));

  EXPECT_EQ(5, SizeY(gfx::Size(5, 5), OffsetRect(6, 6), 0, 0));
  EXPECT_EQ(1, SizeY(gfx::Size(5, 5), OffsetRect(6, 6), 0, 1));
  EXPECT_EQ(4, SizeY(gfx::Size(5, 5), OffsetRect(6, 6), 1, 0));
  EXPECT_EQ(2, SizeY(gfx::Size(5, 5), OffsetRect(6, 6), 1, 1));

  EXPECT_EQ(5, SizeY(gfx::Size(5, 5), OffsetRect(8, 8), 0, 0));
  EXPECT_EQ(3, SizeY(gfx::Size(5, 5), OffsetRect(8, 8), 0, 1));
  EXPECT_EQ(4, SizeY(gfx::Size(5, 5), OffsetRect(8, 8), 1, 0));
  EXPECT_EQ(4, SizeY(gfx::Size(5, 5), OffsetRect(8, 8), 1, 1));

  EXPECT_EQ(5, SizeY(gfx::Size(5, 5), OffsetRect(10, 10), 0, 0));
  EXPECT_EQ(5, SizeY(gfx::Size(5, 5), OffsetRect(10, 10), 0, 1));
  EXPECT_EQ(4, SizeY(gfx::Size(5, 5), OffsetRect(10, 10), 1, 0));
  EXPECT_EQ(3, SizeY(gfx::Size(5, 5), OffsetRect(10, 10), 1, 1));
  EXPECT_EQ(3, SizeY(gfx::Size(5, 5), OffsetRect(10, 10), 1, 2));

  EXPECT_EQ(4, SizeY(gfx::Size(5, 5), OffsetRect(11, 11), 1, 2));
  EXPECT_EQ(3, SizeY(gfx::Size(5, 5), OffsetRect(12, 12), 1, 2));

  EXPECT_EQ(3, SizeY(gfx::Size(9, 5), OffsetRect(17, 12), 1, 2));
}

TEST_P(TilingDataTest, TileSizeX_and_TilePositionX) {
  // Single tile cases:
  EXPECT_EQ(1, SizeX(gfx::Size(3, 3), OffsetRect(1, 1), 0, 0));
  EXPECT_EQ(OffsetX(0), PosX(gfx::Size(3, 3), OffsetRect(1, 1), 0, 0));
  EXPECT_EQ(1, SizeX(gfx::Size(3, 3), OffsetRect(1, 100), 0, 0));
  EXPECT_EQ(OffsetX(0), PosX(gfx::Size(3, 3), OffsetRect(1, 100), 0, 0));
  EXPECT_EQ(3, SizeX(gfx::Size(3, 3), OffsetRect(3, 1), 0, 0));
  EXPECT_EQ(OffsetX(0), PosX(gfx::Size(3, 3), OffsetRect(3, 1), 0, 0));
  EXPECT_EQ(3, SizeX(gfx::Size(3, 3), OffsetRect(3, 100), 0, 0));
  EXPECT_EQ(OffsetX(0), PosX(gfx::Size(3, 3), OffsetRect(3, 100), 0, 0));
  EXPECT_EQ(1, SizeX(gfx::Size(3, 3), OffsetRect(1, 1), 1, 0));
  EXPECT_EQ(OffsetX(0), PosX(gfx::Size(3, 3), OffsetRect(1, 1), 1, 0));
  EXPECT_EQ(1, SizeX(gfx::Size(3, 3), OffsetRect(1, 100), 1, 0));
  EXPECT_EQ(OffsetX(0), PosX(gfx::Size(3, 3), OffsetRect(1, 100), 1, 0));
  EXPECT_EQ(3, SizeX(gfx::Size(3, 3), OffsetRect(3, 1), 1, 0));
  EXPECT_EQ(OffsetX(0), PosX(gfx::Size(3, 3), OffsetRect(3, 1), 1, 0));
  EXPECT_EQ(3, SizeX(gfx::Size(3, 3), OffsetRect(3, 100), 1, 0));
  EXPECT_EQ(OffsetX(0), PosX(gfx::Size(3, 3), OffsetRect(3, 100), 1, 0));

  // Multiple tiles:
  // no border
  // positions 0, 3
  EXPECT_EQ(2, NumTiles(gfx::Size(3, 3), OffsetRect(6, 1), 0));
  EXPECT_EQ(3, SizeX(gfx::Size(3, 3), OffsetRect(6, 1), 0, 0));
  EXPECT_EQ(3, SizeX(gfx::Size(3, 3), OffsetRect(6, 1), 0, 1));
  EXPECT_EQ(OffsetX(0), PosX(gfx::Size(3, 3), OffsetRect(6, 1), 0, 0));
  EXPECT_EQ(OffsetX(3), PosX(gfx::Size(3, 3), OffsetRect(6, 1), 0, 1));
  EXPECT_EQ(3, SizeX(gfx::Size(3, 3), OffsetRect(6, 100), 0, 0));
  EXPECT_EQ(3, SizeX(gfx::Size(3, 3), OffsetRect(6, 100), 0, 1));
  EXPECT_EQ(OffsetX(0), PosX(gfx::Size(3, 3), OffsetRect(6, 100), 0, 0));
  EXPECT_EQ(OffsetX(3), PosX(gfx::Size(3, 3), OffsetRect(6, 100), 0, 1));

  // Multiple tiles:
  // with border
  // positions 0, 2, 3, 4
  EXPECT_EQ(4, NumTiles(gfx::Size(3, 3), OffsetRect(6, 1), 1));
  EXPECT_EQ(2, SizeX(gfx::Size(3, 3), OffsetRect(6, 1), 1, 0));
  EXPECT_EQ(1, SizeX(gfx::Size(3, 3), OffsetRect(6, 1), 1, 1));
  EXPECT_EQ(1, SizeX(gfx::Size(3, 3), OffsetRect(6, 1), 1, 2));
  EXPECT_EQ(2, SizeX(gfx::Size(3, 3), OffsetRect(6, 1), 1, 3));
  EXPECT_EQ(OffsetX(0), PosX(gfx::Size(3, 3), OffsetRect(6, 1), 1, 0));
  EXPECT_EQ(OffsetX(2), PosX(gfx::Size(3, 3), OffsetRect(6, 1), 1, 1));
  EXPECT_EQ(OffsetX(3), PosX(gfx::Size(3, 3), OffsetRect(6, 1), 1, 2));
  EXPECT_EQ(OffsetX(4), PosX(gfx::Size(3, 3), OffsetRect(6, 1), 1, 3));
  EXPECT_EQ(2, SizeX(gfx::Size(3, 7), OffsetRect(6, 100), 1, 0));
  EXPECT_EQ(1, SizeX(gfx::Size(3, 7), OffsetRect(6, 100), 1, 1));
  EXPECT_EQ(1, SizeX(gfx::Size(3, 7), OffsetRect(6, 100), 1, 2));
  EXPECT_EQ(2, SizeX(gfx::Size(3, 7), OffsetRect(6, 100), 1, 3));
  EXPECT_EQ(OffsetX(0), PosX(gfx::Size(3, 7), OffsetRect(6, 100), 1, 0));
  EXPECT_EQ(OffsetX(2), PosX(gfx::Size(3, 7), OffsetRect(6, 100), 1, 1));
  EXPECT_EQ(OffsetX(3), PosX(gfx::Size(3, 7), OffsetRect(6, 100), 1, 2));
  EXPECT_EQ(OffsetX(4), PosX(gfx::Size(3, 7), OffsetRect(6, 100), 1, 3));
}

TEST_P(TilingDataTest, TileSizeY_and_TilePositionY) {
  // Single tile cases:
  EXPECT_EQ(1, SizeY(gfx::Size(3, 3), OffsetRect(1, 1), 0, 0));
  EXPECT_EQ(OffsetY(0), PosY(gfx::Size(3, 3), OffsetRect(1, 1), 0, 0));
  EXPECT_EQ(1, SizeY(gfx::Size(3, 3), OffsetRect(100, 1), 0, 0));
  EXPECT_EQ(OffsetY(0), PosY(gfx::Size(3, 3), OffsetRect(100, 1), 0, 0));
  EXPECT_EQ(3, SizeY(gfx::Size(3, 3), OffsetRect(1, 3), 0, 0));
  EXPECT_EQ(OffsetY(0), PosY(gfx::Size(3, 3), OffsetRect(1, 3), 0, 0));
  EXPECT_EQ(3, SizeY(gfx::Size(3, 3), OffsetRect(100, 3), 0, 0));
  EXPECT_EQ(OffsetY(0), PosY(gfx::Size(3, 3), OffsetRect(100, 3), 0, 0));
  EXPECT_EQ(1, SizeY(gfx::Size(3, 3), OffsetRect(1, 1), 1, 0));
  EXPECT_EQ(OffsetY(0), PosY(gfx::Size(3, 3), OffsetRect(1, 1), 1, 0));
  EXPECT_EQ(1, SizeY(gfx::Size(3, 3), OffsetRect(100, 1), 1, 0));
  EXPECT_EQ(OffsetY(0), PosY(gfx::Size(3, 3), OffsetRect(100, 1), 1, 0));
  EXPECT_EQ(3, SizeY(gfx::Size(3, 3), OffsetRect(1, 3), 1, 0));
  EXPECT_EQ(OffsetY(0), PosY(gfx::Size(3, 3), OffsetRect(1, 3), 1, 0));
  EXPECT_EQ(3, SizeY(gfx::Size(3, 3), OffsetRect(100, 3), 1, 0));
  EXPECT_EQ(OffsetY(0), PosY(gfx::Size(3, 3), OffsetRect(100, 3), 1, 0));

  // Multiple tiles:
  // no border
  // positions 0, 3
  EXPECT_EQ(2, NumTiles(gfx::Size(3, 3), OffsetRect(1, 6), 0));
  EXPECT_EQ(3, SizeY(gfx::Size(3, 3), OffsetRect(1, 6), 0, 0));
  EXPECT_EQ(3, SizeY(gfx::Size(3, 3), OffsetRect(1, 6), 0, 1));
  EXPECT_EQ(OffsetY(0), PosY(gfx::Size(3, 3), OffsetRect(1, 6), 0, 0));
  EXPECT_EQ(OffsetY(3), PosY(gfx::Size(3, 3), OffsetRect(1, 6), 0, 1));
  EXPECT_EQ(3, SizeY(gfx::Size(3, 3), OffsetRect(100, 6), 0, 0));
  EXPECT_EQ(3, SizeY(gfx::Size(3, 3), OffsetRect(100, 6), 0, 1));
  EXPECT_EQ(OffsetY(0), PosY(gfx::Size(3, 3), OffsetRect(100, 6), 0, 0));
  EXPECT_EQ(OffsetY(3), PosY(gfx::Size(3, 3), OffsetRect(100, 6), 0, 1));

  // Multiple tiles:
  // with border
  // positions 0, 2, 3, 4
  EXPECT_EQ(4, NumTiles(gfx::Size(3, 3), OffsetRect(1, 6), 1));
  EXPECT_EQ(2, SizeY(gfx::Size(3, 3), OffsetRect(1, 6), 1, 0));
  EXPECT_EQ(1, SizeY(gfx::Size(3, 3), OffsetRect(1, 6), 1, 1));
  EXPECT_EQ(1, SizeY(gfx::Size(3, 3), OffsetRect(1, 6), 1, 2));
  EXPECT_EQ(2, SizeY(gfx::Size(3, 3), OffsetRect(1, 6), 1, 3));
  EXPECT_EQ(OffsetY(0), PosY(gfx::Size(3, 3), OffsetRect(1, 6), 1, 0));
  EXPECT_EQ(OffsetY(2), PosY(gfx::Size(3, 3), OffsetRect(1, 6), 1, 1));
  EXPECT_EQ(OffsetY(3), PosY(gfx::Size(3, 3), OffsetRect(1, 6), 1, 2));
  EXPECT_EQ(OffsetY(4), PosY(gfx::Size(3, 3), OffsetRect(1, 6), 1, 3));
  EXPECT_EQ(2, SizeY(gfx::Size(7, 3), OffsetRect(100, 6), 1, 0));
  EXPECT_EQ(1, SizeY(gfx::Size(7, 3), OffsetRect(100, 6), 1, 1));
  EXPECT_EQ(1, SizeY(gfx::Size(7, 3), OffsetRect(100, 6), 1, 2));
  EXPECT_EQ(2, SizeY(gfx::Size(7, 3), OffsetRect(100, 6), 1, 3));
  EXPECT_EQ(OffsetY(0), PosY(gfx::Size(7, 3), OffsetRect(100, 6), 1, 0));
  EXPECT_EQ(OffsetY(2), PosY(gfx::Size(7, 3), OffsetRect(100, 6), 1, 1));
  EXPECT_EQ(OffsetY(3), PosY(gfx::Size(7, 3), OffsetRect(100, 6), 1, 2));
  EXPECT_EQ(OffsetY(4), PosY(gfx::Size(7, 3), OffsetRect(100, 6), 1, 3));
}

TEST_P(TilingDataTest, SetTilingSize) {
  TilingData data(gfx::Size(5, 5), OffsetRect(5, 5), 0);
  EXPECT_EQ(OffsetRect(5, 5), data.tiling_rect());
  EXPECT_EQ(1, data.num_tiles_x());
  EXPECT_EQ(5, data.TileSizeX(0));
  EXPECT_EQ(1, data.num_tiles_y());
  EXPECT_EQ(5, data.TileSizeY(0));

  data.SetTilingRect(OffsetRect(6, 5));
  EXPECT_EQ(OffsetRect(6, 5), data.tiling_rect());
  EXPECT_EQ(2, data.num_tiles_x());
  EXPECT_EQ(5, data.TileSizeX(0));
  EXPECT_EQ(1, data.TileSizeX(1));
  EXPECT_EQ(1, data.num_tiles_y());
  EXPECT_EQ(5, data.TileSizeY(0));

  data.SetTilingRect(OffsetRect(5, 12));
  EXPECT_EQ(OffsetRect(5, 12), data.tiling_rect());
  EXPECT_EQ(1, data.num_tiles_x());
  EXPECT_EQ(5, data.TileSizeX(0));
  EXPECT_EQ(3, data.num_tiles_y());
  EXPECT_EQ(5, data.TileSizeY(0));
  EXPECT_EQ(5, data.TileSizeY(1));
  EXPECT_EQ(2, data.TileSizeY(2));
}

TEST_P(TilingDataTest, SetMaxTextureSizeNoBorders) {
  TilingData data(gfx::Size(8, 8), OffsetRect(16, 32), 0);
  EXPECT_EQ(2, data.num_tiles_x());
  EXPECT_EQ(4, data.num_tiles_y());

  data.SetMaxTextureSize(gfx::Size(32, 32));
  EXPECT_EQ(gfx::Size(32, 32), data.max_texture_size());
  EXPECT_EQ(1, data.num_tiles_x());
  EXPECT_EQ(1, data.num_tiles_y());

  data.SetMaxTextureSize(gfx::Size(2, 2));
  EXPECT_EQ(gfx::Size(2, 2), data.max_texture_size());
  EXPECT_EQ(8, data.num_tiles_x());
  EXPECT_EQ(16, data.num_tiles_y());

  data.SetMaxTextureSize(gfx::Size(5, 5));
  EXPECT_EQ(gfx::Size(5, 5), data.max_texture_size());
  EXPECT_EQ(4, data.num_tiles_x());
  EXPECT_EQ(7, data.num_tiles_y());

  data.SetMaxTextureSize(gfx::Size(8, 5));
  EXPECT_EQ(gfx::Size(8, 5), data.max_texture_size());
  EXPECT_EQ(2, data.num_tiles_x());
  EXPECT_EQ(7, data.num_tiles_y());
}

TEST_P(TilingDataTest, SetMaxTextureSizeBorders) {
  TilingData data(gfx::Size(8, 8), OffsetRect(16, 32), 1);
  EXPECT_EQ(3, data.num_tiles_x());
  EXPECT_EQ(5, data.num_tiles_y());

  data.SetMaxTextureSize(gfx::Size(32, 32));
  EXPECT_EQ(gfx::Size(32, 32), data.max_texture_size());
  EXPECT_EQ(1, data.num_tiles_x());
  EXPECT_EQ(1, data.num_tiles_y());

  data.SetMaxTextureSize(gfx::Size(2, 2));
  EXPECT_EQ(gfx::Size(2, 2), data.max_texture_size());
  EXPECT_EQ(0, data.num_tiles_x());
  EXPECT_EQ(0, data.num_tiles_y());

  data.SetMaxTextureSize(gfx::Size(5, 5));
  EXPECT_EQ(gfx::Size(5, 5), data.max_texture_size());
  EXPECT_EQ(5, data.num_tiles_x());
  EXPECT_EQ(10, data.num_tiles_y());

  data.SetMaxTextureSize(gfx::Size(8, 5));
  EXPECT_EQ(gfx::Size(8, 5), data.max_texture_size());
  EXPECT_EQ(3, data.num_tiles_x());
  EXPECT_EQ(10, data.num_tiles_y());
}

TEST_P(TilingDataTest, ExpandRectToTileBounds) {
  TilingData data(gfx::Size(4, 4), OffsetRect(16, 32), 1);

  // Small rect at tiling rect origin rounds up to tile 0, 0.
  gfx::Rect at_origin_src = OffsetRect(1, 1);
  gfx::Rect at_origin_result(data.TileBounds(0, 0));
  EXPECT_NE(at_origin_src, at_origin_result);
  EXPECT_EQ(at_origin_result, data.ExpandRectToTileBounds(at_origin_src));

  // Arbitrary internal rect.
  gfx::Rect rect_src = OffsetRect(6, 6, 1, 3);
  // Tile 2, 2 => gfx::Rect(4, 4, 4, 4)
  // Tile 3, 4 => gfx::Rect(6, 8, 4, 4)
  gfx::Rect rect_result(
      gfx::UnionRects(data.TileBounds(2, 2), data.TileBounds(3, 4)));
  EXPECT_NE(rect_src, rect_result);
  EXPECT_EQ(rect_result, data.ExpandRectToTileBounds(rect_src));

  // On tile bounds rounds up to next tile (since border overlaps).
  gfx::Rect border_rect_src(
      gfx::UnionRects(data.TileBounds(1, 2), data.TileBounds(3, 4)));
  gfx::Rect border_rect_result(
      gfx::UnionRects(data.TileBounds(0, 1), data.TileBounds(4, 5)));
  EXPECT_EQ(border_rect_result, data.ExpandRectToTileBounds(border_rect_src));

  // Equal to tiling rect.
  EXPECT_EQ(data.tiling_rect(),
            data.ExpandRectToTileBounds(data.tiling_rect()));

  // Containing, but larger than tiling rect.
  EXPECT_EQ(data.tiling_rect(),
            data.ExpandRectToTileBounds(OffsetRect(100, 100)));

  // Non-intersecting with tiling rect.
  gfx::Rect non_intersect = OffsetRect(200, 200, 100, 100);
  EXPECT_FALSE(non_intersect.Intersects(data.tiling_rect()));
  EXPECT_EQ(gfx::Rect(), data.ExpandRectToTileBounds(non_intersect));

  TilingData data2(gfx::Size(8, 8), OffsetRect(32, 64), 1);

  // Inside other tile border texels doesn't include other tiles.
  gfx::Rect inner_rect_src(data2.TileBounds(1, 1));
  inner_rect_src.Inset(
      gfx::Insets::VH(data.border_texels(), data2.border_texels()));
  gfx::Rect inner_rect_result(data2.TileBounds(1, 1));
  gfx::Rect expanded = data2.ExpandRectToTileBounds(inner_rect_src);
  EXPECT_EQ(inner_rect_result.ToString(), expanded.ToString());
}

TEST_P(TilingDataTest, Assignment) {
  {
    TilingData source(gfx::Size(8, 8), OffsetRect(16, 32), 1);
    TilingData dest = source;
    EXPECT_EQ(source.border_texels(), dest.border_texels());
    EXPECT_EQ(source.max_texture_size(), dest.max_texture_size());
    EXPECT_EQ(source.num_tiles_x(), dest.num_tiles_x());
    EXPECT_EQ(source.num_tiles_y(), dest.num_tiles_y());
    EXPECT_EQ(source.tiling_rect(), dest.tiling_rect());
  }
  {
    TilingData source(gfx::Size(7, 3), OffsetRect(6, 100), 0);
    TilingData dest(source);
    EXPECT_EQ(source.border_texels(), dest.border_texels());
    EXPECT_EQ(source.max_texture_size(), dest.max_texture_size());
    EXPECT_EQ(source.num_tiles_x(), dest.num_tiles_x());
    EXPECT_EQ(source.num_tiles_y(), dest.num_tiles_y());
    EXPECT_EQ(source.tiling_rect(), dest.tiling_rect());
  }
}

TEST_P(TilingDataTest, LargeBorders) {
  TilingData data(gfx::Size(100, 80), OffsetRect(200, 145), 30);
  EXPECT_EQ(30, data.border_texels());

  EXPECT_EQ(70, data.TileSizeX(0));
  EXPECT_EQ(40, data.TileSizeX(1));
  EXPECT_EQ(40, data.TileSizeX(2));
  EXPECT_EQ(50, data.TileSizeX(3));
  EXPECT_EQ(4, data.num_tiles_x());

  EXPECT_EQ(50, data.TileSizeY(0));
  EXPECT_EQ(20, data.TileSizeY(1));
  EXPECT_EQ(20, data.TileSizeY(2));
  EXPECT_EQ(20, data.TileSizeY(3));
  EXPECT_EQ(35, data.TileSizeY(4));
  EXPECT_EQ(5, data.num_tiles_y());

  EXPECT_EQ(OffsetRect(70, 50), data.TileBounds(0, 0));
  EXPECT_EQ(OffsetRect(70, 50, 40, 20), data.TileBounds(1, 1));
  EXPECT_EQ(OffsetRect(110, 110, 40, 35), data.TileBounds(2, 4));
  EXPECT_EQ(OffsetRect(150, 70, 50, 20), data.TileBounds(3, 2));
  EXPECT_EQ(OffsetRect(150, 110, 50, 35), data.TileBounds(3, 4));

  EXPECT_EQ(OffsetRect(100, 80), data.TileBoundsWithBorder(0, 0));
  EXPECT_EQ(OffsetRect(40, 20, 100, 80), data.TileBoundsWithBorder(1, 1));
  EXPECT_EQ(OffsetRect(80, 80, 100, 65), data.TileBoundsWithBorder(2, 4));
  EXPECT_EQ(OffsetRect(120, 40, 80, 80), data.TileBoundsWithBorder(3, 2));
  EXPECT_EQ(OffsetRect(120, 80, 80, 65), data.TileBoundsWithBorder(3, 4));

  EXPECT_EQ(0, data.TileXIndexFromSrcCoord(OffsetX(0)));
  EXPECT_EQ(0, data.TileXIndexFromSrcCoord(OffsetX(69)));
  EXPECT_EQ(1, data.TileXIndexFromSrcCoord(OffsetX(70)));
  EXPECT_EQ(1, data.TileXIndexFromSrcCoord(OffsetX(109)));
  EXPECT_EQ(2, data.TileXIndexFromSrcCoord(OffsetX(110)));
  EXPECT_EQ(2, data.TileXIndexFromSrcCoord(OffsetX(149)));
  EXPECT_EQ(3, data.TileXIndexFromSrcCoord(OffsetX(150)));
  EXPECT_EQ(3, data.TileXIndexFromSrcCoord(OffsetX(199)));

  EXPECT_EQ(0, data.TileYIndexFromSrcCoord(OffsetY(0)));
  EXPECT_EQ(0, data.TileYIndexFromSrcCoord(OffsetY(49)));
  EXPECT_EQ(1, data.TileYIndexFromSrcCoord(OffsetY(50)));
  EXPECT_EQ(1, data.TileYIndexFromSrcCoord(OffsetY(69)));
  EXPECT_EQ(2, data.TileYIndexFromSrcCoord(OffsetY(70)));
  EXPECT_EQ(2, data.TileYIndexFromSrcCoord(OffsetY(89)));
  EXPECT_EQ(3, data.TileYIndexFromSrcCoord(OffsetY(90)));
  EXPECT_EQ(3, data.TileYIndexFromSrcCoord(OffsetY(109)));
  EXPECT_EQ(4, data.TileYIndexFromSrcCoord(OffsetY(110)));
  EXPECT_EQ(4, data.TileYIndexFromSrcCoord(OffsetY(144)));

  EXPECT_EQ(0, data.FirstBorderTileXIndexFromSrcCoord(OffsetX(0)));
  EXPECT_EQ(0, data.FirstBorderTileXIndexFromSrcCoord(OffsetX(99)));
  EXPECT_EQ(1, data.FirstBorderTileXIndexFromSrcCoord(OffsetX(100)));
  EXPECT_EQ(1, data.FirstBorderTileXIndexFromSrcCoord(OffsetX(139)));
  EXPECT_EQ(2, data.FirstBorderTileXIndexFromSrcCoord(OffsetX(140)));
  EXPECT_EQ(2, data.FirstBorderTileXIndexFromSrcCoord(OffsetX(179)));
  EXPECT_EQ(3, data.FirstBorderTileXIndexFromSrcCoord(OffsetX(180)));
  EXPECT_EQ(3, data.FirstBorderTileXIndexFromSrcCoord(OffsetX(199)));

  EXPECT_EQ(0, data.FirstBorderTileYIndexFromSrcCoord(OffsetY(0)));
  EXPECT_EQ(0, data.FirstBorderTileYIndexFromSrcCoord(OffsetY(79)));
  EXPECT_EQ(1, data.FirstBorderTileYIndexFromSrcCoord(OffsetY(80)));
  EXPECT_EQ(1, data.FirstBorderTileYIndexFromSrcCoord(OffsetY(99)));
  EXPECT_EQ(2, data.FirstBorderTileYIndexFromSrcCoord(OffsetY(100)));
  EXPECT_EQ(2, data.FirstBorderTileYIndexFromSrcCoord(OffsetY(119)));
  EXPECT_EQ(3, data.FirstBorderTileYIndexFromSrcCoord(OffsetY(120)));
  EXPECT_EQ(3, data.FirstBorderTileYIndexFromSrcCoord(OffsetY(139)));
  EXPECT_EQ(4, data.FirstBorderTileYIndexFromSrcCoord(OffsetY(140)));
  EXPECT_EQ(4, data.FirstBorderTileYIndexFromSrcCoord(OffsetY(144)));

  EXPECT_EQ(0, data.LastBorderTileXIndexFromSrcCoord(OffsetX(0)));
  EXPECT_EQ(0, data.LastBorderTileXIndexFromSrcCoord(OffsetX(39)));
  EXPECT_EQ(1, data.LastBorderTileXIndexFromSrcCoord(OffsetX(40)));
  EXPECT_EQ(1, data.LastBorderTileXIndexFromSrcCoord(OffsetX(79)));
  EXPECT_EQ(2, data.LastBorderTileXIndexFromSrcCoord(OffsetX(80)));
  EXPECT_EQ(2, data.LastBorderTileXIndexFromSrcCoord(OffsetX(119)));
  EXPECT_EQ(3, data.LastBorderTileXIndexFromSrcCoord(OffsetX(120)));
  EXPECT_EQ(3, data.LastBorderTileXIndexFromSrcCoord(OffsetX(199)));

  EXPECT_EQ(0, data.LastBorderTileYIndexFromSrcCoord(OffsetY(0)));
  EXPECT_EQ(0, data.LastBorderTileYIndexFromSrcCoord(OffsetY(19)));
  EXPECT_EQ(1, data.LastBorderTileYIndexFromSrcCoord(OffsetY(20)));
  EXPECT_EQ(1, data.LastBorderTileYIndexFromSrcCoord(OffsetY(39)));
  EXPECT_EQ(2, data.LastBorderTileYIndexFromSrcCoord(OffsetY(40)));
  EXPECT_EQ(2, data.LastBorderTileYIndexFromSrcCoord(OffsetY(59)));
  EXPECT_EQ(3, data.LastBorderTileYIndexFromSrcCoord(OffsetY(60)));
  EXPECT_EQ(3, data.LastBorderTileYIndexFromSrcCoord(OffsetY(79)));
  EXPECT_EQ(4, data.LastBorderTileYIndexFromSrcCoord(OffsetY(80)));
  EXPECT_EQ(4, data.LastBorderTileYIndexFromSrcCoord(OffsetY(144)));
}

void TestIterate(const TilingData& data,
                 gfx::Rect rect,
                 int expect_left,
                 int expect_top,
                 int expect_right,
                 int expect_bottom,
                 bool include_borders) {
  EXPECT_GE(expect_left, 0);
  EXPECT_GE(expect_top, 0);
  EXPECT_LT(expect_right, data.num_tiles_x());
  EXPECT_LT(expect_bottom, data.num_tiles_y());

  SCOPED_TRACE(base::StringPrintf(
      "TilingData: max_texture_size=%s, tiling_rect=%s, border_texels=%d; "
      "rect: %s; expected(left,top,right,bottom): %d,%d,%d,%d; "
      "include_borders: %d",
      data.max_texture_size().ToString().c_str(),
      data.tiling_rect().ToString().c_str(), data.border_texels(),
      rect.ToString().c_str(), expect_left, expect_top, expect_right,
      expect_bottom, include_borders));

  std::vector<std::pair<int, int>> original_expected;
  for (int x = 0; x < data.num_tiles_x(); ++x) {
    for (int y = 0; y < data.num_tiles_y(); ++y) {
      gfx::Rect bounds;
      if (include_borders) {
        bounds = data.TileBoundsWithBorder(x, y);
      } else {
        bounds = data.TileBounds(x, y);
      }
      SCOPED_TRACE(base::StringPrintf("x:%d y:%d bounds:%s", x, y,
                                      bounds.ToString().c_str()));
      if (x >= expect_left && x <= expect_right &&
          y >= expect_top && y <= expect_bottom) {
        EXPECT_TRUE(bounds.Intersects(rect));
        original_expected.push_back(std::make_pair(x, y));
      } else {
        EXPECT_FALSE(bounds.Intersects(rect));
      }
    }
  }

  // Verify with vanilla iterator.
  {
    std::vector<std::pair<int, int>> expected = original_expected;
    for (TilingData::Iterator iter(&data, rect, include_borders); iter;
         ++iter) {
      bool found = false;
      for (size_t i = 0; i < expected.size(); ++i) {
        if (expected[i] == iter.index()) {
          expected[i] = expected.back();
          expected.pop_back();
          found = true;
          break;
        }
      }
      EXPECT_TRUE(found);
    }
    EXPECT_EQ(0u, expected.size());
  }

  // Make sure this also works with a difference iterator and an empty ignore.
  // The difference iterator never includes borders, so ignore it otherwise.
  if (!include_borders) {
    std::vector<std::pair<int, int>> expected = original_expected;
    for (TilingData::DifferenceIterator iter(&data, rect, gfx::Rect()); iter;
         ++iter) {
      bool found = false;
      for (size_t i = 0; i < expected.size(); ++i) {
        if (expected[i] == iter.index()) {
          expected[i] = expected.back();
          expected.pop_back();
          found = true;
          break;
        }
      }
      EXPECT_TRUE(found);
    }
    EXPECT_EQ(0u, expected.size());
  }
}

void TestIterateBorders(const TilingData& data,
                        gfx::Rect rect,
                        int expect_left,
                        int expect_top,
                        int expect_right,
                        int expect_bottom) {
  bool include_borders = true;
  TestIterate(data,
              rect,
              expect_left,
              expect_top,
              expect_right,
              expect_bottom,
              include_borders);
}

void TestIterateNoBorders(const TilingData& data,
                          gfx::Rect rect,
                          int expect_left,
                          int expect_top,
                          int expect_right,
                          int expect_bottom) {
  bool include_borders = false;
  TestIterate(data,
              rect,
              expect_left,
              expect_top,
              expect_right,
              expect_bottom,
              include_borders);
}

void TestIterateAll(const TilingData& data,
                    gfx::Rect rect,
                    int expect_left,
                    int expect_top,
                    int expect_right,
                    int expect_bottom) {
  TestIterateBorders(
      data, rect, expect_left, expect_top, expect_right, expect_bottom);
  TestIterateNoBorders(
      data, rect, expect_left, expect_top, expect_right, expect_bottom);
}

TEST_P(TilingDataTest, IteratorNoBorderTexels) {
  TilingData data(gfx::Size(10, 10), OffsetRect(40, 25), 0);
  // X border index by src coord: [0-10), [10-20), [20, 30), [30, 40)
  // Y border index by src coord: [0-10), [10-20), [20, 25)
  TestIterateAll(data, OffsetRect(40, 25), 0, 0, 3, 2);
  TestIterateAll(data, OffsetRect(15, 15, 8, 8), 1, 1, 2, 2);

  // Oversized.
  TestIterateAll(data, OffsetRect(-100, -100, 1000, 1000), 0, 0, 3, 2);
  TestIterateAll(data, OffsetRect(-100, 20, 1000, 1), 0, 2, 3, 2);
  TestIterateAll(data, OffsetRect(29, -100, 31, 1000), 2, 0, 3, 2);
  // Nonintersecting.
  TestIterateAll(data, OffsetRect(60, 80, 100, 100), 0, 0, -1, -1);
}

TEST_P(TilingDataTest, BordersIteratorOneBorderTexel) {
  TilingData data(gfx::Size(10, 20), OffsetRect(25, 45), 1);
  // X border index by src coord: [0-10), [8-18), [16-25)
  // Y border index by src coord: [0-20), [18-38), [36-45)
  TestIterateBorders(data, OffsetRect(25, 45), 0, 0, 2, 2);
  TestIterateBorders(data, OffsetRect(18, 19, 3, 17), 2, 0, 2, 1);
  TestIterateBorders(data, OffsetRect(10, 20, 6, 16), 1, 1, 1, 1);
  TestIterateBorders(data, OffsetRect(9, 19, 8, 18), 0, 0, 2, 2);
  // Oversized.
  TestIterateBorders(data, OffsetRect(-100, -100, 1000, 1000), 0, 0, 2, 2);
  TestIterateBorders(data, OffsetRect(-100, 20, 1000, 1), 0, 1, 2, 1);
  TestIterateBorders(data, OffsetRect(18, -100, 6, 1000), 2, 0, 2, 2);
  // Nonintersecting.
  TestIterateBorders(data, OffsetRect(60, 80, 100, 100), 0, 0, -1, -1);
}

TEST_P(TilingDataTest, NoBordersIteratorOneBorderTexel) {
  TilingData data(gfx::Size(10, 20), OffsetRect(25, 45), 1);
  // X index by src coord: [0-9), [9-17), [17-25)
  // Y index by src coord: [0-19), [19-37), [37-45)
  TestIterateNoBorders(data, OffsetRect(25, 45), 0, 0, 2, 2);
  TestIterateNoBorders(data, OffsetRect(17, 19, 3, 18), 2, 1, 2, 1);
  TestIterateNoBorders(data, OffsetRect(17, 19, 3, 19), 2, 1, 2, 2);
  TestIterateNoBorders(data, OffsetRect(8, 18, 9, 19), 0, 0, 1, 1);
  TestIterateNoBorders(data, OffsetRect(9, 19, 9, 19), 1, 1, 2, 2);
  // Oversized.
  TestIterateNoBorders(data, OffsetRect(-100, -100, 1000, 1000), 0, 0, 2, 2);
  TestIterateNoBorders(data, OffsetRect(-100, 20, 1000, 1), 0, 1, 2, 1);
  TestIterateNoBorders(data, OffsetRect(18, -100, 6, 1000), 2, 0, 2, 2);
  // Nonintersecting.
  TestIterateNoBorders(data, OffsetRect(60, 80, 100, 100), 0, 0, -1, -1);
}

TEST_P(TilingDataTest, BordersIteratorManyBorderTexels) {
  TilingData data(gfx::Size(50, 60), OffsetRect(65, 110), 20);
  // X border index by src coord: [0-50), [10-60), [20-65)
  // Y border index by src coord: [0-60), [20-80), [40-100), [60-110)
  TestIterateBorders(data, OffsetRect(65, 110), 0, 0, 2, 3);
  TestIterateBorders(data, OffsetRect(50, 60, 15, 65), 1, 1, 2, 3);
  TestIterateBorders(data, OffsetRect(60, 30, 2, 10), 2, 0, 2, 1);
  // Oversized.
  TestIterateBorders(data, OffsetRect(-100, -100, 1000, 1000), 0, 0, 2, 3);
  TestIterateBorders(data, OffsetRect(-100, 10, 1000, 10), 0, 0, 2, 0);
  TestIterateBorders(data, OffsetRect(10, -100, 10, 1000), 0, 0, 1, 3);
  // Nonintersecting.
  TestIterateBorders(data, OffsetRect(65, 110, 100, 100), 0, 0, -1, -1);
}

TEST_P(TilingDataTest, NoBordersIteratorManyBorderTexels) {
  TilingData data(gfx::Size(50, 60), OffsetRect(65, 110), 20);
  // X index by src coord: [0-30), [30-40), [40, 65)
  // Y index by src coord: [0-40), [40-60), [60, 80), [80-110)
  TestIterateNoBorders(data, OffsetRect(65, 110), 0, 0, 2, 3);
  TestIterateNoBorders(data, OffsetRect(30, 40, 15, 65), 1, 1, 2, 3);
  TestIterateNoBorders(data, OffsetRect(60, 20, 2, 21), 2, 0, 2, 1);
  // Oversized.
  TestIterateNoBorders(data, OffsetRect(-100, -100, 1000, 1000), 0, 0, 2, 3);
  TestIterateNoBorders(data, OffsetRect(-100, 10, 1000, 10), 0, 0, 2, 0);
  TestIterateNoBorders(data, OffsetRect(10, -100, 10, 1000), 0, 0, 0, 3);
  // Nonintersecting.
  TestIterateNoBorders(data, OffsetRect(65, 110, 100, 100), 0, 0, -1, -1);
}

TEST_P(TilingDataTest, IteratorOneTile) {
  TilingData no_border(gfx::Size(1000, 1000), OffsetRect(30, 40), 0);
  TestIterateAll(no_border, OffsetRect(30, 40), 0, 0, 0, 0);
  TestIterateAll(no_border, OffsetRect(10, 10, 20, 20), 0, 0, 0, 0);
  TestIterateAll(no_border, OffsetRect(30, 40, 100, 100), 0, 0, -1, -1);

  TilingData one_border(gfx::Size(1000, 1000), OffsetRect(30, 40), 1);
  TestIterateAll(one_border, OffsetRect(30, 40), 0, 0, 0, 0);
  TestIterateAll(one_border, OffsetRect(10, 10, 20, 20), 0, 0, 0, 0);
  TestIterateAll(one_border, OffsetRect(30, 40, 100, 100), 0, 0, -1, -1);

  TilingData big_border(gfx::Size(1000, 1000), OffsetRect(30, 40), 50);
  TestIterateAll(big_border, OffsetRect(30, 40), 0, 0, 0, 0);
  TestIterateAll(big_border, OffsetRect(10, 10, 20, 20), 0, 0, 0, 0);
  TestIterateAll(big_border, OffsetRect(30, 40, 100, 100), 0, 0, -1, -1);
}

TEST_P(TilingDataTest, IteratorNoTiles) {
  TilingData data(gfx::Size(100, 100), OffsetRect(0, 0), 0);
  TestIterateAll(data, OffsetRect(100, 100), 0, 0, -1, -1);
}

void TestDiff(const TilingData& data,
              gfx::Rect consider,
              gfx::Rect ignore,
              size_t num_tiles) {
  std::vector<std::pair<int, int>> expected;
  for (int y = 0; y < data.num_tiles_y(); ++y) {
    for (int x = 0; x < data.num_tiles_x(); ++x) {
      gfx::Rect bounds = data.TileBounds(x, y);
      if (bounds.Intersects(consider) && !bounds.Intersects(ignore))
        expected.push_back(std::make_pair(x, y));
    }
  }

  // Sanity check the test.
  EXPECT_EQ(num_tiles, expected.size());

  for (TilingData::DifferenceIterator iter(&data, consider, ignore); iter;
       ++iter) {
    bool found = false;
    for (size_t i = 0; i < expected.size(); ++i) {
      if (expected[i] == iter.index()) {
        expected[i] = expected.back();
        expected.pop_back();
        found = true;
        break;
      }
    }
    EXPECT_TRUE(found);
  }
  EXPECT_EQ(0u, expected.size());
}

TEST_P(TilingDataTest, DifferenceIteratorIgnoreGeometry) {
  // This test is checking that the iterator can handle different geometries of
  // ignore rects relative to the consider rect.  The consider rect indices
  // themselves are mostly tested by the non-difference iterator tests, so the
  // full rect is mostly used here for simplicity.

  // X border index by src coord: [0-10), [10-20), [20, 30), [30, 40)
  // Y border index by src coord: [0-10), [10-20), [20, 25)
  TilingData data(gfx::Size(10, 10), OffsetRect(40, 25), 0);

  // Fully ignored
  TestDiff(data, OffsetRect(40, 25), OffsetRect(40, 25), 0);
  TestDiff(data, OffsetRect(40, 25), OffsetRect(-100, -100, 200, 200), 0);
  TestDiff(data, OffsetRect(40, 25), OffsetRect(9, 9, 30, 15), 0);
  TestDiff(data, OffsetRect(15, 15, 8, 8), OffsetRect(15, 15, 8, 8), 0);

  // Fully un-ignored
  TestDiff(data, OffsetRect(40, 25), OffsetRect(-30, -20, 8, 8), 12);
  TestDiff(data, OffsetRect(40, 25), OffsetRect(0, 0), 12);

  // Top left, remove 2x2 tiles
  TestDiff(data, OffsetRect(40, 25), OffsetRect(20, 19), 8);
  // Bottom right, remove 2x2 tiles
  TestDiff(data, OffsetRect(40, 25), OffsetRect(20, 15, 20, 6), 8);
  // Bottom left, remove 2x2 tiles
  TestDiff(data, OffsetRect(40, 25), OffsetRect(0, 15, 20, 6), 8);
  // Top right, remove 2x2 tiles
  TestDiff(data, OffsetRect(40, 25), OffsetRect(20, 0, 20, 19), 8);
  // Center, remove only one tile
  TestDiff(data, OffsetRect(40, 25), OffsetRect(10, 10, 5, 5), 11);

  // Left column, flush left, removing two columns
  TestDiff(data, OffsetRect(40, 25), OffsetRect(11, 25), 6);
  // Middle column, removing two columns
  TestDiff(data, OffsetRect(40, 25), OffsetRect(11, 0, 11, 25), 6);
  // Right column, flush right, removing one column
  TestDiff(data, OffsetRect(40, 25), OffsetRect(30, 0, 2, 25), 9);

  // Top row, flush top, removing one row
  TestDiff(data, OffsetRect(40, 25), OffsetRect(0, 5, 40, 5), 8);
  // Middle row, removing one row
  TestDiff(data, OffsetRect(40, 25), OffsetRect(0, 13, 40, 5), 8);
  // Bottom row, flush bottom, removing two rows
  TestDiff(data, OffsetRect(40, 25), OffsetRect(0, 13, 40, 12), 4);

  // Non-intersecting, but still touching two of the same tiles.
  TestDiff(data, OffsetRect(8, 0, 32, 25), OffsetRect(0, 12, 5, 12), 10);

  // Intersecting, but neither contains the other. 2x3 with one overlap.
  TestDiff(data, OffsetRect(5, 2, 20, 10), OffsetRect(25, 15, 5, 10), 5);
}

TEST_P(TilingDataTest, DifferenceIteratorManyBorderTexels) {
  // X border index by src coord: [0-50), [10-60), [20-65)
  // Y border index by src coord: [0-60), [20-80), [40-100), [60-110)
  // X tile bounds by src coord: [0-30), [30-40), [40-65)
  // Y tile bounds by src coord: [0-40), [40-60), [60-80), [80-110)
  TilingData data(gfx::Size(50, 60), OffsetRect(65, 110), 20);

  // Knock out two rows, but not the left column.
  TestDiff(data, OffsetRect(10, 30, 55, 80), OffsetRect(30, 59, 20, 2), 8);

  // Knock out one row.
  TestDiff(data, OffsetRect(10, 30, 55, 80), OffsetRect(29, 59, 20, 1), 9);

  // Overlap all tiles with ignore rect.
  TestDiff(data, OffsetRect(65, 110), OffsetRect(29, 39, 12, 42), 0);

  gfx::Rect tile = data.TileBounds(1, 1);

  // Ignore one tile.
  TestDiff(data, OffsetRect(20, 30, 45, 80), tile, 11);

  // Include one tile.
  TestDiff(data, tile, OffsetRect(0, 0), 1);
}

TEST_P(TilingDataTest, DifferenceIteratorOneTile) {
  TilingData no_border(gfx::Size(1000, 1000), OffsetRect(30, 40), 0);
  TestDiff(no_border, OffsetRect(30, 40), OffsetRect(0, 0), 1);
  TestDiff(no_border, OffsetRect(5, 5, 100, 100), OffsetRect(5, 5, 1, 1), 0);

  TilingData one_border(gfx::Size(1000, 1000), OffsetRect(30, 40), 1);
  TestDiff(one_border, OffsetRect(30, 40), OffsetRect(0, 0), 1);
  TestDiff(one_border, OffsetRect(5, 5, 100, 100), OffsetRect(5, 5, 1, 1), 0);

  TilingData big_border(gfx::Size(1000, 1000), OffsetRect(30, 40), 50);
  TestDiff(big_border, OffsetRect(30, 40), OffsetRect(0, 0), 1);
  TestDiff(big_border, OffsetRect(5, 5, 100, 100), OffsetRect(5, 5, 1, 1), 0);
}

TEST_P(TilingDataTest, DifferenceIteratorNoTiles) {
  TilingData data(gfx::Size(100, 100), OffsetRect(0, 0), 0);
  TestDiff(data, OffsetRect(100, 100), OffsetRect(5, 5), 0);
}

}  // namespace

}  // namespace cc
