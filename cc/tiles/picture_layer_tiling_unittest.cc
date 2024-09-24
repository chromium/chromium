// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/picture_layer_tiling.h"

#include <stddef.h>

#include <limits>
#include <set>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "cc/base/math_util.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/fake_picture_layer_tiling_client.h"
#include "cc/test/fake_raster_source.h"
#include "cc/tiles/picture_layer_tiling_set.h"
#include "cc/trees/layer_tree_settings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace cc {
namespace {

static gfx::Rect ViewportInLayerSpace(
    const gfx::Transform& transform,
    const gfx::Size& device_viewport) {

  gfx::Transform inverse;
  if (!transform.GetInverse(&inverse))
    return gfx::Rect();

  return MathUtil::ProjectEnclosingClippedRect(inverse,
                                               gfx::Rect(device_viewport));
}

class TestablePictureLayerTiling : public PictureLayerTiling {
 public:
  using PictureLayerTiling::SetLiveTilesRect;
  using PictureLayerTiling::TileAt;

  static std::unique_ptr<TestablePictureLayerTiling> Create(
      WhichTree tree,
      const gfx::AxisTransform2d& raster_transform,
      scoped_refptr<RasterSource> raster_source,
      PictureLayerTilingClient* client,
      const LayerTreeSettings& settings) {
    return base::WrapUnique(new TestablePictureLayerTiling(
        tree, raster_transform, raster_source, client,
        settings.tiling_interest_area_padding,
        settings.skewport_target_time_in_seconds,
        settings.skewport_extrapolation_limit_in_screen_pixels,
        312.f, /* min_preraster_distance */
        settings.max_preraster_distance_in_screen_pixels));
  }

  gfx::Rect live_tiles_rect() const { return live_tiles_rect_; }
  PriorityRectType visible_rect_type() const {
    return PriorityRectType::VISIBLE_RECT;
  }

  void SetEventuallyRect(const gfx::Rect& rect) {
    SetPriorityRect(EnclosingLayerRectFromContentsRect(rect),
                    PriorityRectType::EVENTUALLY_RECT,
                    /*evicts_tiles=*/true);
  }

  using PictureLayerTiling::has_eventually_rect_tiles;
  using PictureLayerTiling::has_skewport_rect_tiles;
  using PictureLayerTiling::has_soon_border_rect_tiles;
  using PictureLayerTiling::has_visible_rect_tiles;

  using PictureLayerTiling::RemoveTilesInRegion;
  using PictureLayerTiling::ComputePriorityRectTypeForTile;

 protected:
  TestablePictureLayerTiling(WhichTree tree,
                             const gfx::AxisTransform2d& raster_transform,
                             scoped_refptr<RasterSource> raster_source,
                             PictureLayerTilingClient* client,
                             size_t tiling_interest_area_padding,
                             float skewport_target_time,
                             int skewport_extrapolation_limit,
                             float min_preraster_distance,
                             float max_preraster_distance)
      : PictureLayerTiling(tree,
                           raster_transform,
                           raster_source,
                           client,
                           min_preraster_distance,
                           max_preraster_distance,
                           /*can_use_lcd_text*/ false) {}
};

class PictureLayerTilingIteratorTest : public testing::Test {
 public:
  using VerifyTilesCallback =
      base::RepeatingCallback<void(Tile* tile, const gfx::Rect& geometry_rect)>;

  PictureLayerTilingIteratorTest() = default;
  PictureLayerTilingIteratorTest(const PictureLayerTilingIteratorTest&) =
      delete;
  ~PictureLayerTilingIteratorTest() override = default;

  PictureLayerTilingIteratorTest& operator=(
      const PictureLayerTilingIteratorTest&) = delete;

  void Initialize(
      const gfx::Size& tile_size,
      scoped_refptr<FakeRasterSource> raster_source,
      const gfx::AxisTransform2d& raster_transform = gfx::AxisTransform2d(),
      WhichTree tree = PENDING_TREE) {
    client_.SetTileSize(tile_size);
    tiling_ = TestablePictureLayerTiling::Create(tree, raster_transform,
                                                 std::move(raster_source),
                                                 &client_, LayerTreeSettings());
    tiling_->set_resolution(HIGH_RESOLUTION);
  }

  void InitializeFilled(const gfx::Size& tile_size,
                        float contents_scale,
                        const gfx::Size& layer_bounds,
                        WhichTree tree = PENDING_TREE) {
    Initialize(tile_size, FakeRasterSource::CreateFilled(layer_bounds),
               gfx::AxisTransform2d(contents_scale, gfx::Vector2dF()), tree);
  }

  void InitializePartiallyFilled(const gfx::Size& tile_size,
                                 float contents_scale,
                                 const gfx::Size& layer_bounds,
                                 const gfx::Rect& recorded_bounds,
                                 WhichTree tree = PENDING_TREE) {
    Initialize(
        tile_size,
        FakeRasterSource::CreatePartiallyFilled(layer_bounds, recorded_bounds),
        gfx::AxisTransform2d(contents_scale, gfx::Vector2dF()), tree);
  }

  void InitializeWithTransform(const gfx::Size& tile_size,
                               const gfx::AxisTransform2d& raster_transform,
                               const gfx::Size& layer_bounds,
                               WhichTree tree = PENDING_TREE) {
    Initialize(tile_size, FakeRasterSource::CreateFilled(layer_bounds),
               raster_transform, tree);
  }

  void InitializeActive(const gfx::Size& tile_size,
                        float contents_scale,
                        const gfx::Size& layer_bounds) {
    InitializeFilled(tile_size, contents_scale, layer_bounds, ACTIVE_TREE);
  }

  void SetLiveRectAndVerifyTiles(const gfx::Rect& live_tiles_rect) {
    tiling_->CreateAllTilesForTesting(live_tiles_rect);

    std::vector<Tile*> tiles = tiling_->AllTilesForTesting();
    for (auto iter = tiles.begin(); iter != tiles.end(); ++iter) {
      EXPECT_TRUE(live_tiles_rect.Intersects((*iter)->content_rect()));
    }
  }

  void VerifyTilesExactlyCoverRect(
      float rect_scale,
      const gfx::Rect& request_rect,
      const gfx::Rect& expect_rect) {
    EXPECT_TRUE(request_rect.Contains(expect_rect));

    // Iterators are not valid if the destination scale is smaller than the
    // tiling scale. This is because coverage computation is done in integer
    // grids in the dest space, and the overlap between tiles may not guarantee
    // to enclose an integer grid line to round to if scaled down.
    ASSERT_GE(rect_scale, tiling_->contents_scale_key());

    Region remaining = expect_rect;
    for (PictureLayerTiling::CoverageIterator
             iter(tiling_.get(), rect_scale, request_rect);
         iter;
         ++iter) {
      // Geometry cannot overlap previous geometry at all
      gfx::Rect geometry = iter.geometry_rect();
      EXPECT_TRUE(expect_rect.Contains(geometry));
      EXPECT_TRUE(remaining.Contains(geometry));
      remaining.Subtract(geometry);

      // Sanity check that texture coords are within the texture rect.
      // Skip check for external edges because they do overhang.
      // For internal edges there is an inset of 0.5 texels because the sample
      // points are at the center of the texels. An extra 1/1024 tolerance
      // is allowed for numerical errors.
      // Refer to picture_layer_tiling.cc for detailed analysis.
      const float inset = loose_texel_extent_check_ ? 0 : (0.5f - 1.f / 1024.f);
      gfx::RectF texture_rect = iter.texture_rect();
      if (iter.i())
        EXPECT_GE(texture_rect.x(), inset);
      if (iter.j())
        EXPECT_GE(texture_rect.y(), inset);
      if (iter.i() != tiling_->tiling_data()->num_tiles_x() - 1)
        EXPECT_LE(texture_rect.right(), client_.TileSize().width() - inset);
      if (iter.j() != tiling_->tiling_data()->num_tiles_y() - 1)
        EXPECT_LE(texture_rect.bottom(), client_.TileSize().height() - inset);
    }

    // The entire rect must be filled by geometry from the tiling.
    EXPECT_TRUE(remaining.IsEmpty());
  }

  void VerifyTilesExactlyCoverRect(float rect_scale, const gfx::Rect& rect) {
    VerifyTilesExactlyCoverRect(rect_scale, rect, rect);
  }

  void VerifyTiles(float rect_scale,
                   const gfx::Rect& rect,
                   VerifyTilesCallback callback) {
    VerifyTiles(tiling_.get(), rect_scale, rect, callback);
  }

  void VerifyTiles(PictureLayerTiling* tiling,
                   float rect_scale,
                   const gfx::Rect& rect,
                   VerifyTilesCallback callback) {
    Region remaining = rect;
    for (PictureLayerTiling::CoverageIterator iter(tiling, rect_scale, rect);
         iter; ++iter) {
      remaining.Subtract(iter.geometry_rect());
      callback.Run(*iter, iter.geometry_rect());
    }
    EXPECT_TRUE(remaining.IsEmpty());
  }

  void VerifyTilesCoverNonContainedRect(float rect_scale,
                                        const gfx::Rect& dest_rect) {
    float dest_to_contents_scale = tiling_->contents_scale_key() / rect_scale;
    gfx::Rect clamped_rect = gfx::ScaleToEnclosingRect(
        tiling_->tiling_rect(), 1.f / dest_to_contents_scale);
    clamped_rect.Intersect(dest_rect);
    VerifyTilesExactlyCoverRect(rect_scale, dest_rect, clamped_rect);
  }

 protected:
  FakePictureLayerTilingClient client_;
  std::unique_ptr<TestablePictureLayerTiling> tiling_;
  bool loose_texel_extent_check_ = false;
};

TEST_F(PictureLayerTilingIteratorTest, ResizeLayer) {
  // Verifies that a resize with invalidation for newly exposed pixels will
  // recreated tiles that intersect that invalidation.
  gfx::Size tile_size(100, 100);
  gfx::Size original_layer_size(10, 10);
  InitializeActive(tile_size, 1.f, original_layer_size);
  SetLiveRectAndVerifyTiles(gfx::Rect(original_layer_size));

  // Tiling only has one tile, since its total size is less than one.
  ASSERT_TRUE(tiling_->TileAt(0, 0));
  auto tile_id = tiling_->TileAt(0, 0)->id();

  gfx::Size new_layer_size(200, 200);
  scoped_refptr<FakeRasterSource> raster_source =
      FakeRasterSource::CreateFilled(new_layer_size);

  Region invalidation =
      SubtractRegions(gfx::Rect(tile_size), gfx::Rect(original_layer_size));
  tiling_->SetRasterSourceAndResize(raster_source);
  ASSERT_TRUE(tiling_->TileAt(0, 0));
  EXPECT_EQ(tile_id, tiling_->TileAt(0, 0)->id());
  tiling_->Invalidate(invalidation);
  // The invalidated tile should be deleted and recreated.
  ASSERT_TRUE(tiling_->TileAt(0, 0));
  EXPECT_NE(tile_id, tiling_->TileAt(0, 0)->id());
}

TEST_F(PictureLayerTilingIteratorTest, RecordedBoundsChange) {
  // Verifies that a resize with invalidation for newly exposed pixels will
  // recreated tiles that intersect that invalidation.
  gfx::Size tile_size(100, 100);
  gfx::Size layer_size(1000, 1000);
  gfx::Rect old_recorded_bounds(310, 310, 150, 150);
  InitializePartiallyFilled(tile_size, 1.f, layer_size, old_recorded_bounds);
  SetLiveRectAndVerifyTiles(old_recorded_bounds);
  // Tiling rect origin is snapped.
  EXPECT_EQ(gfx::Rect(256, 256, 204, 204), tiling_->tiling_rect());
  EXPECT_EQ(9u, tiling_->AllTilesForTesting().size());
  auto tile_id00 = tiling_->TileAt(0, 0)->id();

  gfx::Rect new_recorded_bounds1(300, 300, 160, 160);
  tiling_->SetRasterSourceAndResize(FakeRasterSource::CreatePartiallyFilled(
      layer_size, new_recorded_bounds1));
  // Small change of origin within the snap distance won't cause change of
  // the tiling rect.
  EXPECT_EQ(gfx::Rect(256, 256, 204, 204), tiling_->tiling_rect());
  EXPECT_EQ(9u, tiling_->AllTilesForTesting().size());
  // This checks we won't invalidate all tiles. In real world, we'll invalidate
  // raster for the changed recording.
  EXPECT_EQ(tile_id00, tiling_->TileAt(0, 0)->id());

  gfx::Rect new_recorded_bounds2(310, 310, 200, 20);
  // The previous SetRasterSourceAndResize() clamped the live tiles rect to
  // old_recorded_bounds, and this one will clamp it again to
  // new_recorded_bounds, so it only drops disappeared tiles but won't create
  // newly exposed tiles.
  tiling_->SetRasterSourceAndResize(FakeRasterSource::CreatePartiallyFilled(
      layer_size, new_recorded_bounds2));
  EXPECT_EQ(3u, tiling_->AllTilesForTesting().size());
  EXPECT_EQ(gfx::Rect(256, 256, 254, 74), tiling_->tiling_rect());
  EXPECT_EQ(tile_id00, tiling_->TileAt(0, 0)->id());
  // Setting the live tiles rect will create new tiles.
  SetLiveRectAndVerifyTiles(new_recorded_bounds2);
  EXPECT_EQ(gfx::Rect(256, 256, 254, 74), tiling_->tiling_rect());
  EXPECT_EQ(3u, tiling_->AllTilesForTesting().size());
  EXPECT_EQ(tile_id00, tiling_->TileAt(0, 0)->id());

  gfx::Rect new_recorded_bounds3(400, 400, 200, 50);
  tiling_->SetRasterSourceAndResize(FakeRasterSource::CreatePartiallyFilled(
      layer_size, new_recorded_bounds3));
  // All tiles are invalidated when the origin of the tiling rect changes.
  EXPECT_EQ(gfx::Rect(384, 384, 216, 66), tiling_->tiling_rect());
  EXPECT_EQ(0u, tiling_->AllTilesForTesting().size());
  // Setting the live tiles rect will create new tiles.
  SetLiveRectAndVerifyTiles(new_recorded_bounds3);
  EXPECT_EQ(3u, tiling_->AllTilesForTesting().size());
  EXPECT_NE(tile_id00, tiling_->TileAt(0, 0)->id());
}

TEST_F(PictureLayerTilingIteratorTest, CreateMissingTilesStaysInsideLiveRect) {
  // The tiling has three rows and columns.
  InitializeFilled(gfx::Size(100, 100), 1.f, gfx::Size(250, 250));
  EXPECT_EQ(3, tiling_->TilingDataForTesting().num_tiles_x());
  EXPECT_EQ(3, tiling_->TilingDataForTesting().num_tiles_y());

  // The live tiles rect is at the very edge of the right-most and
  // bottom-most tiles. Their border pixels would still be inside the live
  // tiles rect, but the tiles should not exist just for that.
  int right = tiling_->TilingDataForTesting().TileBounds(2, 2).x();
  int bottom = tiling_->TilingDataForTesting().TileBounds(2, 2).y();

  SetLiveRectAndVerifyTiles(gfx::Rect(right, bottom));
  EXPECT_FALSE(tiling_->TileAt(2, 0));
  EXPECT_FALSE(tiling_->TileAt(2, 1));
  EXPECT_FALSE(tiling_->TileAt(2, 2));
  EXPECT_FALSE(tiling_->TileAt(1, 2));
  EXPECT_FALSE(tiling_->TileAt(0, 2));

  // Verify CreateMissingTilesInLiveTilesRect respects this.
  tiling_->CreateMissingTilesInLiveTilesRect();
  EXPECT_FALSE(tiling_->TileAt(2, 0));
  EXPECT_FALSE(tiling_->TileAt(2, 1));
  EXPECT_FALSE(tiling_->TileAt(2, 2));
  EXPECT_FALSE(tiling_->TileAt(1, 2));
  EXPECT_FALSE(tiling_->TileAt(0, 2));
}

TEST_F(PictureLayerTilingIteratorTest, ResizeTilingOverTileBorders) {
  // The tiling has four rows and three columns.
  InitializeFilled(gfx::Size(100, 100), 1.f, gfx::Size(250, 350));
  EXPECT_EQ(3, tiling_->TilingDataForTesting().num_tiles_x());
  EXPECT_EQ(4, tiling_->TilingDataForTesting().num_tiles_y());

  // The live tiles rect covers the whole tiling.
  SetLiveRectAndVerifyTiles(gfx::Rect(250, 350));

  // Tiles in the bottom row and right column exist.
  EXPECT_TRUE(tiling_->TileAt(2, 0));
  EXPECT_TRUE(tiling_->TileAt(2, 1));
  EXPECT_TRUE(tiling_->TileAt(2, 2));
  EXPECT_TRUE(tiling_->TileAt(2, 3));
  EXPECT_TRUE(tiling_->TileAt(1, 3));
  EXPECT_TRUE(tiling_->TileAt(0, 3));

  int right = tiling_->TilingDataForTesting().TileBounds(2, 2).x();
  int bottom = tiling_->TilingDataForTesting().TileBounds(2, 3).y();

  // Shrink the tiling so that the last tile row/column is entirely in the
  // border pixels of the interior tiles. That row/column is removed.
  scoped_refptr<FakeRasterSource> raster_source =
      FakeRasterSource::CreateFilled(gfx::Size(right + 1, bottom + 1));
  tiling_->SetRasterSourceAndResize(raster_source);
  EXPECT_EQ(2, tiling_->TilingDataForTesting().num_tiles_x());
  EXPECT_EQ(3, tiling_->TilingDataForTesting().num_tiles_y());

  // The live tiles rect was clamped to the raster source size.
  EXPECT_EQ(gfx::Rect(right + 1, bottom + 1), tiling_->live_tiles_rect());

  // Since the row/column is gone, the tiles should be gone too.
  EXPECT_FALSE(tiling_->TileAt(2, 0));
  EXPECT_FALSE(tiling_->TileAt(2, 1));
  EXPECT_FALSE(tiling_->TileAt(2, 2));
  EXPECT_FALSE(tiling_->TileAt(2, 3));
  EXPECT_FALSE(tiling_->TileAt(1, 3));
  EXPECT_FALSE(tiling_->TileAt(0, 3));

  // Growing outside the current right/bottom tiles border pixels should create
  // the tiles again, even though the live rect has not changed size.
  raster_source =
      FakeRasterSource::CreateFilled(gfx::Size(right + 2, bottom + 2));
  tiling_->SetRasterSourceAndResize(raster_source);
  EXPECT_EQ(3, tiling_->TilingDataForTesting().num_tiles_x());
  EXPECT_EQ(4, tiling_->TilingDataForTesting().num_tiles_y());

  // Not changed.
  EXPECT_EQ(gfx::Rect(right + 1, bottom + 1), tiling_->live_tiles_rect());

  // The last row/column tiles are inside the live tiles rect.
  EXPECT_TRUE(gfx::Rect(right + 1, bottom + 1).Intersects(
      tiling_->TilingDataForTesting().TileBounds(2, 0)));
  EXPECT_TRUE(gfx::Rect(right + 1, bottom + 1).Intersects(
      tiling_->TilingDataForTesting().TileBounds(0, 3)));

  EXPECT_TRUE(tiling_->TileAt(2, 0));
  EXPECT_TRUE(tiling_->TileAt(2, 1));
  EXPECT_TRUE(tiling_->TileAt(2, 2));
  EXPECT_TRUE(tiling_->TileAt(2, 3));
  EXPECT_TRUE(tiling_->TileAt(1, 3));
  EXPECT_TRUE(tiling_->TileAt(0, 3));
}

TEST_F(PictureLayerTilingIteratorTest, ResizeLiveTileRectOverTileBorders) {
  // The tiling has three rows and columns.
  InitializeFilled(gfx::Size(100, 100), 1.f, gfx::Size(250, 350));
  EXPECT_EQ(3, tiling_->TilingDataForTesting().num_tiles_x());
  EXPECT_EQ(4, tiling_->TilingDataForTesting().num_tiles_y());

  // The live tiles rect covers the whole tiling.
  SetLiveRectAndVerifyTiles(gfx::Rect(250, 350));

  // Tiles in the bottom row and right column exist.
  EXPECT_TRUE(tiling_->TileAt(2, 0));
  EXPECT_TRUE(tiling_->TileAt(2, 1));
  EXPECT_TRUE(tiling_->TileAt(2, 2));
  EXPECT_TRUE(tiling_->TileAt(2, 3));
  EXPECT_TRUE(tiling_->TileAt(1, 3));
  EXPECT_TRUE(tiling_->TileAt(0, 3));

  // Shrink the live tiles rect to the very edge of the right-most and
  // bottom-most tiles. Their border pixels would still be inside the live
  // tiles rect, but the tiles should not exist just for that.
  int right = tiling_->TilingDataForTesting().TileBounds(2, 3).x();
  int bottom = tiling_->TilingDataForTesting().TileBounds(2, 3).y();

  SetLiveRectAndVerifyTiles(gfx::Rect(right, bottom));
  EXPECT_FALSE(tiling_->TileAt(2, 0));
  EXPECT_FALSE(tiling_->TileAt(2, 1));
  EXPECT_FALSE(tiling_->TileAt(2, 2));
  EXPECT_FALSE(tiling_->TileAt(2, 3));
  EXPECT_FALSE(tiling_->TileAt(1, 3));
  EXPECT_FALSE(tiling_->TileAt(0, 3));

  // Including the bottom row and right column again, should create the tiles.
  SetLiveRectAndVerifyTiles(gfx::Rect(right + 1, bottom + 1));
  EXPECT_TRUE(tiling_->TileAt(2, 0));
  EXPECT_TRUE(tiling_->TileAt(2, 1));
  EXPECT_TRUE(tiling_->TileAt(2, 2));
  EXPECT_TRUE(tiling_->TileAt(2, 3));
  EXPECT_TRUE(tiling_->TileAt(1, 2));
  EXPECT_TRUE(tiling_->TileAt(0, 2));

  // Shrink the live tiles rect to the very edge of the left-most and
  // top-most tiles. Their border pixels would still be inside the live
  // tiles rect, but the tiles should not exist just for that.
  int left = tiling_->TilingDataForTesting().TileBounds(0, 0).right();
  int top = tiling_->TilingDataForTesting().TileBounds(0, 0).bottom();

  SetLiveRectAndVerifyTiles(gfx::Rect(left, top, 250 - left, 350 - top));
  EXPECT_FALSE(tiling_->TileAt(0, 3));
  EXPECT_FALSE(tiling_->TileAt(0, 2));
  EXPECT_FALSE(tiling_->TileAt(0, 1));
  EXPECT_FALSE(tiling_->TileAt(0, 0));
  EXPECT_FALSE(tiling_->TileAt(1, 0));
  EXPECT_FALSE(tiling_->TileAt(2, 0));

  // Including the top row and left column again, should create the tiles.
  SetLiveRectAndVerifyTiles(
      gfx::Rect(left - 1, top - 1, 250 - left, 350 - top));
  EXPECT_TRUE(tiling_->TileAt(0, 3));
  EXPECT_TRUE(tiling_->TileAt(0, 2));
  EXPECT_TRUE(tiling_->TileAt(0, 1));
  EXPECT_TRUE(tiling_->TileAt(0, 0));
  EXPECT_TRUE(tiling_->TileAt(1, 0));
  EXPECT_TRUE(tiling_->TileAt(2, 0));
}

TEST_F(PictureLayerTilingIteratorTest, ShrinkWidthExpandHeightTilingRect) {
  InitializeFilled(gfx::Size(100, 100), 1.f, gfx::Size(450, 296));
  EXPECT_EQ(5, tiling_->TilingDataForTesting().num_tiles_x());
  EXPECT_EQ(3, tiling_->TilingDataForTesting().num_tiles_y());

  SetLiveRectAndVerifyTiles(gfx::Rect(450, 296));

  // Tiles in the rightmost column exist and tiles in the third row does
  // not exist yet.
  EXPECT_TRUE(tiling_->TileAt(4, 0));
  EXPECT_TRUE(tiling_->TileAt(4, 1));
  EXPECT_TRUE(tiling_->TileAt(4, 2));
  EXPECT_FALSE(tiling_->TileAt(0, 3));
  EXPECT_FALSE(tiling_->TileAt(1, 3));
  EXPECT_FALSE(tiling_->TileAt(2, 3));
  EXPECT_FALSE(tiling_->TileAt(3, 3));

  scoped_refptr<FakeRasterSource> raster_source =
      FakeRasterSource::CreateFilled(gfx::Size(310, 310));
  tiling_->SetRasterSourceAndResize(raster_source);
  EXPECT_EQ(4, tiling_->TilingDataForTesting().num_tiles_x());
  EXPECT_EQ(4, tiling_->TilingDataForTesting().num_tiles_y());

  // Tiles in the rightmost column for the original size was removed and
  // tiles in the bottom row was created.
  EXPECT_FALSE(tiling_->TileAt(4, 0));
  EXPECT_FALSE(tiling_->TileAt(4, 1));
  EXPECT_FALSE(tiling_->TileAt(4, 2));
  EXPECT_TRUE(tiling_->TileAt(0, 3));
  EXPECT_TRUE(tiling_->TileAt(1, 3));
  EXPECT_TRUE(tiling_->TileAt(2, 3));
  EXPECT_TRUE(tiling_->TileAt(3, 3));
}

TEST_F(PictureLayerTilingIteratorTest, ResizeLiveTileRectOverSameTiles) {
  // The tiling has four rows and three columns.
  InitializeFilled(gfx::Size(100, 100), 1.f, gfx::Size(250, 350));
  EXPECT_EQ(3, tiling_->TilingDataForTesting().num_tiles_x());
  EXPECT_EQ(4, tiling_->TilingDataForTesting().num_tiles_y());

  // The live tiles rect covers the whole tiling.
  SetLiveRectAndVerifyTiles(gfx::Rect(250, 350));

  // All tiles exist.
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 4; ++j)
      EXPECT_TRUE(tiling_->TileAt(i, j)) << i << "," << j;
  }

  // Shrink the live tiles rect, but still cover all the tiles.
  SetLiveRectAndVerifyTiles(gfx::Rect(1, 1, 249, 349));

  // All tiles still exist.
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 4; ++j)
      EXPECT_TRUE(tiling_->TileAt(i, j)) << i << "," << j;
  }

  // Grow the live tiles rect, but still cover all the same tiles.
  SetLiveRectAndVerifyTiles(gfx::Rect(0, 0, 250, 350));

  // All tiles still exist.
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 4; ++j)
      EXPECT_TRUE(tiling_->TileAt(i, j)) << i << "," << j;
  }
}

TEST_F(PictureLayerTilingIteratorTest, ResizeLayerOverBorderPixels) {
  // Verifies that a resize with invalidation for newly exposed pixels will
  // recreate tiles that intersect that invalidation.
  gfx::Size tile_size(100, 100);
  gfx::Size original_layer_size(99, 99);
  InitializeActive(tile_size, 1.f, original_layer_size);
  SetLiveRectAndVerifyTiles(gfx::Rect(original_layer_size));

  // Tiling only has one tile, since its total size is less than one.
  ASSERT_TRUE(tiling_->TileAt(0, 0));
  auto tile_id = tiling_->TileAt(0, 0)->id();

  gfx::Size new_layer_size(200, 200);
  scoped_refptr<FakeRasterSource> raster_source =
      FakeRasterSource::CreateFilled(new_layer_size);

  Region invalidation =
      SubtractRegions(gfx::Rect(tile_size), gfx::Rect(original_layer_size));
  tiling_->SetRasterSourceAndResize(raster_source);
  ASSERT_TRUE(tiling_->TileAt(0, 0));
  EXPECT_EQ(tile_id, tiling_->TileAt(0, 0)->id());
  tiling_->Invalidate(invalidation);
  // The invalidated tile should be deleted and recreated.
  ASSERT_TRUE(tiling_->TileAt(0, 0));
  EXPECT_NE(tile_id, tiling_->TileAt(0, 0)->id());
}

TEST_F(PictureLayerTilingIteratorTest, RemoveOutsideLayerKeepsTiles) {
  gfx::Size tile_size(100, 100);
  gfx::Size layer_size(100, 100);
  InitializeActive(tile_size, 1.f, layer_size);
  SetLiveRectAndVerifyTiles(gfx::Rect(layer_size));

  // In all cases here, the tiling should remain with one tile, since the remove
  // region doesn't intersect it.

  bool recreate_tiles = false;
  // Top
  tiling_->RemoveTilesInRegion(gfx::Rect(50, -1, 1, 1), recreate_tiles);
  EXPECT_TRUE(tiling_->TileAt(0, 0));
  // Bottom
  tiling_->RemoveTilesInRegion(gfx::Rect(50, 100, 1, 1), recreate_tiles);
  EXPECT_TRUE(tiling_->TileAt(0, 0));
  // Left
  tiling_->RemoveTilesInRegion(gfx::Rect(-1, 50, 1, 1), recreate_tiles);
  EXPECT_TRUE(tiling_->TileAt(0, 0));
  // Right
  tiling_->RemoveTilesInRegion(gfx::Rect(100, 50, 1, 1), recreate_tiles);
  EXPECT_TRUE(tiling_->TileAt(0, 0));
}

TEST_F(PictureLayerTilingIteratorTest, CreateTileJustCoverBorderUp) {
  float content_scale = 1.2000000476837158f;
  gfx::Size tile_size(512, 512);
  gfx::Size layer_size(1440, 4560);
  FakePictureLayerTilingClient active_client;

  active_client.SetTileSize(tile_size);
  scoped_refptr<FakeRasterSource> raster_source =
      FakeRasterSource::CreateFilled(layer_size);
  std::unique_ptr<TestablePictureLayerTiling> active_tiling =
      TestablePictureLayerTiling::Create(
          ACTIVE_TREE, gfx::AxisTransform2d(content_scale, gfx::Vector2dF()),
          raster_source, &active_client, LayerTreeSettings());
  active_tiling->set_resolution(HIGH_RESOLUTION);

  gfx::Rect invalid_rect(0, 750, 220, 100);
  InitializeFilled(tile_size, content_scale, layer_size);
  client_.set_twin_tiling(active_tiling.get());
  client_.set_invalidation(invalid_rect);
  SetLiveRectAndVerifyTiles(gfx::Rect(layer_size));
  // When it creates a tile in pending tree, verify that tiles are invalidated
  // even if only their border pixels intersect the invalidation rect
  EXPECT_TRUE(tiling_->TileAt(0, 1));
  gfx::Rect scaled_invalid_rect =
      gfx::ScaleToEnclosingRect(invalid_rect, content_scale);
  EXPECT_FALSE(scaled_invalid_rect.Intersects(
      tiling_->TilingDataForTesting().TileBounds(0, 2)));
  EXPECT_TRUE(scaled_invalid_rect.Intersects(
      tiling_->TilingDataForTesting().TileBoundsWithBorder(0, 2)));
  EXPECT_TRUE(tiling_->TileAt(0, 2));

  bool recreate_tiles = false;
  active_tiling->RemoveTilesInRegion(invalid_rect, recreate_tiles);
  // Even though a tile just touch border area of invalid region, verify that
  // RemoveTilesInRegion behaves the same as SetLiveRectAndVerifyTiles with
  // respect to the tiles that it invalidates
  EXPECT_FALSE(active_tiling->TileAt(0, 1));
  EXPECT_FALSE(active_tiling->TileAt(0, 2));
}

TEST_F(PictureLayerTilingIteratorTest, LiveTilesExactlyCoverLiveTileRect) {
  InitializeFilled(gfx::Size(100, 100), 1.f, gfx::Size(1099, 801));
  SetLiveRectAndVerifyTiles(gfx::Rect(100, 100));
  SetLiveRectAndVerifyTiles(gfx::Rect(101, 99));
  SetLiveRectAndVerifyTiles(gfx::Rect(1099, 1));
  SetLiveRectAndVerifyTiles(gfx::Rect(1, 801));
  SetLiveRectAndVerifyTiles(gfx::Rect(1099, 1));
  SetLiveRectAndVerifyTiles(gfx::Rect(201, 800));
}

TEST_F(PictureLayerTilingIteratorTest, IteratorCoversLayerBoundsNoScale) {
  InitializeFilled(gfx::Size(100, 100), 1.f, gfx::Size(1099, 801));
  VerifyTilesExactlyCoverRect(1, gfx::Rect());
  VerifyTilesExactlyCoverRect(1, gfx::Rect(0, 0, 1099, 801));
  VerifyTilesExactlyCoverRect(1, gfx::Rect(52, 83, 789, 412));

  // With borders, a size of 3x3 = 1 pixel of content.
  InitializeFilled(gfx::Size(3, 3), 1.f, gfx::Size(10, 10));
  VerifyTilesExactlyCoverRect(1, gfx::Rect(0, 0, 1, 1));
  VerifyTilesExactlyCoverRect(1, gfx::Rect(0, 0, 2, 2));
  VerifyTilesExactlyCoverRect(1, gfx::Rect(1, 1, 2, 2));
  VerifyTilesExactlyCoverRect(1, gfx::Rect(3, 2, 5, 2));
}

TEST_F(PictureLayerTilingIteratorTest, IteratorCoversLayerBoundsTilingScale) {
  InitializeFilled(gfx::Size(200, 100), 2.0f, gfx::Size(1005, 2010));
  VerifyTilesExactlyCoverRect(2, gfx::Rect());
  VerifyTilesExactlyCoverRect(2, gfx::Rect(0, 0, 2010, 4020));
  VerifyTilesExactlyCoverRect(2, gfx::Rect(100, 224, 1024, 762));

  InitializeFilled(gfx::Size(3, 3), 2.0f, gfx::Size(10, 10));
  VerifyTilesExactlyCoverRect(2, gfx::Rect());
  VerifyTilesExactlyCoverRect(2, gfx::Rect(0, 0, 1, 1));
  VerifyTilesExactlyCoverRect(2, gfx::Rect(0, 0, 2, 2));
  VerifyTilesExactlyCoverRect(2, gfx::Rect(1, 1, 2, 2));
  VerifyTilesExactlyCoverRect(2, gfx::Rect(3, 2, 5, 2));

  InitializeFilled(gfx::Size(100, 200), 0.5f, gfx::Size(1005, 2010));
  VerifyTilesExactlyCoverRect(1, gfx::Rect(0, 0, 1005, 2010));
  VerifyTilesExactlyCoverRect(1, gfx::Rect(50, 112, 512, 381));

  InitializeFilled(gfx::Size(150, 250), 0.37f, gfx::Size(1005, 2010));
  VerifyTilesExactlyCoverRect(1, gfx::Rect(0, 0, 1005, 2010));
  VerifyTilesExactlyCoverRect(1, gfx::Rect(50, 112, 512, 381));

  InitializeFilled(gfx::Size(312, 123), 0.01f, gfx::Size(1005, 2010));
  VerifyTilesExactlyCoverRect(1, gfx::Rect(0, 0, 1005, 2010));
  VerifyTilesExactlyCoverRect(1, gfx::Rect(50, 112, 512, 381));
}

TEST_F(PictureLayerTilingIteratorTest, IteratorCoversLayerBoundsBothScale) {
  InitializeFilled(gfx::Size(50, 50), 4.0f, gfx::Size(800, 600));
  VerifyTilesExactlyCoverRect(4.0f, gfx::Rect());
  VerifyTilesExactlyCoverRect(4.0f, gfx::Rect(0, 0, 3200, 2400));
  VerifyTilesExactlyCoverRect(4.0f, gfx::Rect(1024, 730, 506, 364));

  float scale = 6.7f;
  gfx::Size bounds(800, 600);
  gfx::Rect full_rect(gfx::ScaleToCeiledSize(bounds, scale));
  InitializeFilled(gfx::Size(256, 512), 5.2f, bounds);
  VerifyTilesExactlyCoverRect(scale, full_rect);
  VerifyTilesExactlyCoverRect(scale, gfx::Rect(2014, 1579, 867, 1033));
}

TEST_F(PictureLayerTilingIteratorTest, IteratorEmptyRect) {
  InitializeFilled(gfx::Size(100, 100), 1.0f, gfx::Size(800, 600));

  gfx::Rect empty;
  PictureLayerTiling::CoverageIterator iter(tiling_.get(), 1.0f, empty);
  EXPECT_FALSE(iter);
}

TEST_F(PictureLayerTilingIteratorTest, NonIntersectingRect) {
  InitializeFilled(gfx::Size(100, 100), 1.0f, gfx::Size(800, 600));
  gfx::Rect non_intersecting(1000, 1000, 50, 50);
  PictureLayerTiling::CoverageIterator iter(tiling_.get(), 1, non_intersecting);
  EXPECT_FALSE(iter);
}

TEST_F(PictureLayerTilingIteratorTest, LayerEdgeTextureCoordinates) {
  InitializeFilled(gfx::Size(300, 300), 1.0f, gfx::Size(256, 256));
  // All of these sizes are 256x256, scaled and ceiled.
  VerifyTilesExactlyCoverRect(1.0f, gfx::Rect(0, 0, 256, 256));
  VerifyTilesExactlyCoverRect(1.2f, gfx::Rect(0, 0, 308, 308));
}

TEST_F(PictureLayerTilingIteratorTest, NonContainedDestRect) {
  InitializeFilled(gfx::Size(100, 100), 1.0f, gfx::Size(400, 400));

  // Too large in all dimensions
  VerifyTilesCoverNonContainedRect(1.0f, gfx::Rect(-1000, -1000, 2000, 2000));
  VerifyTilesCoverNonContainedRect(1.5f, gfx::Rect(-1000, -1000, 2000, 2000));

  // Partially covering content, but too large
  VerifyTilesCoverNonContainedRect(1.0f, gfx::Rect(-1000, 100, 2000, 100));
  VerifyTilesCoverNonContainedRect(1.5f, gfx::Rect(-1000, 100, 2000, 100));
}

static void TileExists(bool exists, Tile* tile,
                       const gfx::Rect& geometry_rect) {
  EXPECT_EQ(exists, tile != nullptr) << geometry_rect.ToString();
}

TEST_F(PictureLayerTilingIteratorTest, TilesExist) {
  gfx::Size layer_bounds(1099, 801);
  InitializeFilled(gfx::Size(100, 100), 1.f, layer_bounds);
  VerifyTilesExactlyCoverRect(1.f, gfx::Rect(layer_bounds));
  VerifyTiles(1.f, gfx::Rect(layer_bounds),
              base::BindRepeating(&TileExists, false));

  tiling_->ComputeTilePriorityRects(
      gfx::Rect(layer_bounds),  // visible rect
      gfx::Rect(layer_bounds),  // skewport
      gfx::Rect(layer_bounds),  // soon border rect
      gfx::Rect(layer_bounds),  // eventually rect
      1.f,                      // current contents scale
      Occlusion());
  VerifyTiles(1.f, gfx::Rect(layer_bounds),
              base::BindRepeating(&TileExists, true));

  // Make the viewport rect empty. All tiles are killed and become zombies.
  tiling_->ComputeTilePriorityRects(gfx::Rect(), gfx::Rect(), gfx::Rect(),
                                    gfx::Rect(), 1.f, Occlusion());
  VerifyTiles(1.f, gfx::Rect(layer_bounds),
              base::BindRepeating(&TileExists, false));
}

TEST_F(PictureLayerTilingIteratorTest, TilesExistGiantViewport) {
  gfx::Size layer_bounds(1099, 801);
  InitializeFilled(gfx::Size(100, 100), 1.f, layer_bounds);
  VerifyTilesExactlyCoverRect(1.f, gfx::Rect(layer_bounds));
  VerifyTiles(1.f, gfx::Rect(layer_bounds),
              base::BindRepeating(&TileExists, false));

  gfx::Rect giant_rect(-10000000, -10000000, 1000000000, 1000000000);

  tiling_->ComputeTilePriorityRects(
      gfx::Rect(layer_bounds),  // visible rect
      gfx::Rect(layer_bounds),  // skewport
      gfx::Rect(layer_bounds),  // soon border rect
      gfx::Rect(layer_bounds),  // eventually rect
      1.f,                      // current contents scale
      Occlusion());
  VerifyTiles(1.f, gfx::Rect(layer_bounds),
              base::BindRepeating(&TileExists, true));

  // If the visible content rect is huge, we should still have live tiles.
  tiling_->ComputeTilePriorityRects(giant_rect, giant_rect, giant_rect,
                                    giant_rect, 1.f, Occlusion());
  VerifyTiles(1.f, gfx::Rect(layer_bounds),
              base::BindRepeating(&TileExists, true));
}

TEST_F(PictureLayerTilingIteratorTest, TilesExistOutsideViewport) {
  gfx::Size layer_bounds(1099, 801);
  InitializeFilled(gfx::Size(100, 100), 1.f, layer_bounds);
  VerifyTilesExactlyCoverRect(1.f, gfx::Rect(layer_bounds));
  VerifyTiles(1.f, gfx::Rect(layer_bounds),
              base::BindRepeating(&TileExists, false));

  // This rect does not intersect with the layer, as the layer is outside the
  // viewport.
  gfx::Rect viewport_rect(1100, 0, 1000, 1000);
  EXPECT_FALSE(viewport_rect.Intersects(gfx::Rect(layer_bounds)));

  LayerTreeSettings settings;
  gfx::Rect eventually_rect = viewport_rect;
  eventually_rect.Inset(-settings.tiling_interest_area_padding);
  tiling_->ComputeTilePriorityRects(viewport_rect, viewport_rect, viewport_rect,
                                    eventually_rect, 1.f, Occlusion());
  VerifyTiles(1.f, gfx::Rect(layer_bounds),
              base::BindRepeating(&TileExists, true));
}

static void TilesIntersectingRectExist(const gfx::Rect& rect,
                                       bool intersect_exists,
                                       Tile* tile,
                                       const gfx::Rect& geometry_rect) {
  bool intersects = rect.Intersects(geometry_rect);
  bool expected_exists = intersect_exists ? intersects : !intersects;
  EXPECT_EQ(expected_exists, tile != nullptr)
      << "Rects intersecting " << rect.ToString() << " should exist. "
      << "Current tile rect is " << geometry_rect.ToString();
}

TEST_F(PictureLayerTilingIteratorTest,
       TilesExistLargeViewportAndLayerWithSmallVisibleArea) {
  gfx::Size layer_bounds(10000, 10000);
  client_.SetTileSize(gfx::Size(100, 100));
  LayerTreeSettings settings;
  settings.tiling_interest_area_padding = 1;

  scoped_refptr<FakeRasterSource> raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);
  tiling_ = TestablePictureLayerTiling::Create(
      PENDING_TREE, gfx::AxisTransform2d(), raster_source, &client_, settings);
  tiling_->set_resolution(HIGH_RESOLUTION);
  VerifyTilesExactlyCoverRect(1.f, gfx::Rect(layer_bounds));
  VerifyTiles(1.f, gfx::Rect(layer_bounds),
              base::BindRepeating(&TileExists, false));

  gfx::Rect visible_rect(8000, 8000, 50, 50);

  tiling_->ComputeTilePriorityRects(visible_rect,  // visible rect
                                    visible_rect,  // skewport
                                    visible_rect,  // soon border rect
                                    visible_rect,  // eventually rect
                                    1.f,           // current contents scale
                                    Occlusion());
  VerifyTiles(
      1.f, gfx::Rect(layer_bounds),
      base::BindRepeating(&TilesIntersectingRectExist, visible_rect, true));
}

TEST(ComputeTilePriorityRectsTest, VisibleTiles) {
  // The TilePriority of visible tiles should have zero distance_to_visible
  // and time_to_visible.
  FakePictureLayerTilingClient client;

  gfx::Size device_viewport(800, 600);
  gfx::Size current_layer_bounds(200, 200);
  float current_layer_contents_scale = 1.f;
  gfx::Transform current_screen_transform;

  gfx::Rect viewport_in_layer_space = ViewportInLayerSpace(
      current_screen_transform, device_viewport);

  client.SetTileSize(gfx::Size(100, 100));

  scoped_refptr<FakeRasterSource> raster_source =
      FakeRasterSource::CreateFilled(current_layer_bounds);
  std::unique_ptr<TestablePictureLayerTiling> tiling =
      TestablePictureLayerTiling::Create(ACTIVE_TREE, gfx::AxisTransform2d(),
                                         raster_source, &client,
                                         LayerTreeSettings());
  tiling->set_resolution(HIGH_RESOLUTION);

  LayerTreeSettings settings;
  gfx::Rect eventually_rect = viewport_in_layer_space;
  eventually_rect.Inset(-settings.tiling_interest_area_padding);
  tiling->ComputeTilePriorityRects(
      viewport_in_layer_space, viewport_in_layer_space, viewport_in_layer_space,
      eventually_rect, current_layer_contents_scale, Occlusion());
  auto prioritized_tiles = tiling->UpdateAndGetAllPrioritizedTilesForTesting();

  ASSERT_TRUE(tiling->TileAt(0, 0));
  ASSERT_TRUE(tiling->TileAt(0, 1));
  ASSERT_TRUE(tiling->TileAt(1, 0));
  ASSERT_TRUE(tiling->TileAt(1, 1));

  TilePriority priority = prioritized_tiles[tiling->TileAt(0, 0)].priority();
  EXPECT_FLOAT_EQ(0.f, priority.distance_to_visible);
  EXPECT_FLOAT_EQ(TilePriority::NOW, priority.priority_bin);

  priority = prioritized_tiles[tiling->TileAt(0, 1)].priority();
  EXPECT_FLOAT_EQ(0.f, priority.distance_to_visible);
  EXPECT_FLOAT_EQ(TilePriority::NOW, priority.priority_bin);

  priority = prioritized_tiles[tiling->TileAt(1, 0)].priority();
  EXPECT_FLOAT_EQ(0.f, priority.distance_to_visible);
  EXPECT_FLOAT_EQ(TilePriority::NOW, priority.priority_bin);

  priority = prioritized_tiles[tiling->TileAt(1, 1)].priority();
  EXPECT_FLOAT_EQ(0.f, priority.distance_to_visible);
  EXPECT_FLOAT_EQ(TilePriority::NOW, priority.priority_bin);
}

TEST(ComputeTilePriorityRectsTest, OffscreenTiles) {
  // The TilePriority of offscreen tiles (without movement) should have nonzero
  // distance_to_visible and infinite time_to_visible.
  FakePictureLayerTilingClient client;

  gfx::Size device_viewport(800, 600);
  gfx::Size current_layer_bounds(200, 200);
  float current_layer_contents_scale = 1.f;
  gfx::Transform last_screen_transform;
  gfx::Transform current_screen_transform;

  current_screen_transform.Translate(850, 0);
  last_screen_transform = current_screen_transform;

  gfx::Rect viewport_in_layer_space = ViewportInLayerSpace(
      current_screen_transform, device_viewport);

  client.SetTileSize(gfx::Size(100, 100));

  scoped_refptr<FakeRasterSource> raster_source =
      FakeRasterSource::CreateFilled(current_layer_bounds);
  std::unique_ptr<TestablePictureLayerTiling> tiling =
      TestablePictureLayerTiling::Create(ACTIVE_TREE, gfx::AxisTransform2d(),
                                         raster_source, &client,
                                         LayerTreeSettings());
  tiling->set_resolution(HIGH_RESOLUTION);

  LayerTreeSettings settings;
  gfx::Rect eventually_rect = viewport_in_layer_space;
  eventually_rect.Inset(-settings.tiling_interest_area_padding);
  tiling->ComputeTilePriorityRects(
      viewport_in_layer_space, viewport_in_layer_space, viewport_in_layer_space,
      eventually_rect, current_layer_contents_scale, Occlusion());
  auto prioritized_tiles = tiling->UpdateAndGetAllPrioritizedTilesForTesting();

  ASSERT_TRUE(tiling->TileAt(0, 0));
  ASSERT_TRUE(tiling->TileAt(0, 1));
  ASSERT_TRUE(tiling->TileAt(1, 0));
  ASSERT_TRUE(tiling->TileAt(1, 1));

  TilePriority priority = prioritized_tiles[tiling->TileAt(0, 0)].priority();
  EXPECT_GT(priority.distance_to_visible, 0.f);
  EXPECT_NE(TilePriority::NOW, priority.priority_bin);

  priority = prioritized_tiles[tiling->TileAt(0, 1)].priority();
  EXPECT_GT(priority.distance_to_visible, 0.f);
  EXPECT_NE(TilePriority::NOW, priority.priority_bin);

  priority = prioritized_tiles[tiling->TileAt(1, 0)].priority();
  EXPECT_GT(priority.distance_to_visible, 0.f);
  EXPECT_NE(TilePriority::NOW, priority.priority_bin);

  priority = prioritized_tiles[tiling->TileAt(1, 1)].priority();
  EXPECT_GT(priority.distance_to_visible, 0.f);
  EXPECT_NE(TilePriority::NOW, priority.priority_bin);

  // Furthermore, in this scenario tiles on the right hand side should have a
  // larger distance to visible.
  TilePriority left = prioritized_tiles[tiling->TileAt(0, 0)].priority();
  TilePriority right = prioritized_tiles[tiling->TileAt(1, 0)].priority();
  EXPECT_GT(right.distance_to_visible, left.distance_to_visible);

  left = prioritized_tiles[tiling->TileAt(0, 1)].priority();
  right = prioritized_tiles[tiling->TileAt(1, 1)].priority();
  EXPECT_GT(right.distance_to_visible, left.distance_to_visible);
}

TEST(ComputeTilePriorityRectsTest, PartiallyOffscreenLayer) {
  // Sanity check that a layer with some tiles visible and others offscreen has
  // correct TilePriorities for each tile.
  FakePictureLayerTilingClient client;

  gfx::Size device_viewport(800, 600);
  gfx::Size current_layer_bounds(200, 200);
  float current_layer_contents_scale = 1.f;
  gfx::Transform last_screen_transform;
  gfx::Transform current_screen_transform;

  current_screen_transform.Translate(705, 505);
  last_screen_transform = current_screen_transform;

  gfx::Rect viewport_in_layer_space = ViewportInLayerSpace(
      current_screen_transform, device_viewport);

  client.SetTileSize(gfx::Size(100, 100));

  scoped_refptr<FakeRasterSource> raster_source =
      FakeRasterSource::CreateFilled(current_layer_bounds);
  std::unique_ptr<TestablePictureLayerTiling> tiling =
      TestablePictureLayerTiling::Create(ACTIVE_TREE, gfx::AxisTransform2d(),
                                         raster_source, &client,
                                         LayerTreeSettings());
  tiling->set_resolution(HIGH_RESOLUTION);

  LayerTreeSettings settings;
  gfx::Rect eventually_rect = viewport_in_layer_space;
  eventually_rect.Inset(-settings.tiling_interest_area_padding);
  tiling->ComputeTilePriorityRects(
      viewport_in_layer_space, viewport_in_layer_space, viewport_in_layer_space,
      eventually_rect, current_layer_contents_scale, Occlusion());
  auto prioritized_tiles = tiling->UpdateAndGetAllPrioritizedTilesForTesting();

  ASSERT_TRUE(tiling->TileAt(0, 0));
  ASSERT_TRUE(tiling->TileAt(0, 1));
  ASSERT_TRUE(tiling->TileAt(1, 0));
  ASSERT_TRUE(tiling->TileAt(1, 1));

  TilePriority priority = prioritized_tiles[tiling->TileAt(0, 0)].priority();
  EXPECT_FLOAT_EQ(0.f, priority.distance_to_visible);
  EXPECT_FLOAT_EQ(TilePriority::NOW, priority.priority_bin);

  priority = prioritized_tiles[tiling->TileAt(0, 1)].priority();
  EXPECT_GT(priority.distance_to_visible, 0.f);
  EXPECT_NE(TilePriority::NOW, priority.priority_bin);

  priority = prioritized_tiles[tiling->TileAt(1, 0)].priority();
  EXPECT_GT(priority.distance_to_visible, 0.f);
  EXPECT_NE(TilePriority::NOW, priority.priority_bin);

  priority = prioritized_tiles[tiling->TileAt(1, 1)].priority();
  EXPECT_GT(priority.distance_to_visible, 0.f);
  EXPECT_NE(TilePriority::NOW, priority.priority_bin);
}

TEST(PictureLayerTilingTest, RecycledTilesClearedOnReset) {
  FakePictureLayerTilingClient active_client;
  active_client.SetTileSize(gfx::Size(100, 100));

  scoped_refptr<FakeRasterSource> raster_source =
      FakeRasterSource::CreateFilled(gfx::Size(100, 100));
  std::unique_ptr<TestablePictureLayerTiling> active_tiling =
      TestablePictureLayerTiling::Create(ACTIVE_TREE, gfx::AxisTransform2d(),
                                         raster_source, &active_client,
                                         LayerTreeSettings());
  active_tiling->set_resolution(HIGH_RESOLUTION);
  // Create all tiles on this tiling.
  gfx::Rect visible_rect = gfx::Rect(0, 0, 100, 100);
  active_tiling->ComputeTilePriorityRects(
      visible_rect, visible_rect, visible_rect, visible_rect, 1.f, Occlusion());

  FakePictureLayerTilingClient recycle_client;
  recycle_client.SetTileSize(gfx::Size(100, 100));
  recycle_client.set_twin_tiling(active_tiling.get());

  LayerTreeSettings settings;

  raster_source = FakeRasterSource::CreateFilled(gfx::Size(100, 100));
  std::unique_ptr<TestablePictureLayerTiling> recycle_tiling =
      TestablePictureLayerTiling::Create(PENDING_TREE, gfx::AxisTransform2d(),
                                         raster_source, &recycle_client,
                                         settings);
  recycle_tiling->set_resolution(HIGH_RESOLUTION);

  // Create all tiles on the recycle tiling.
  recycle_tiling->ComputeTilePriorityRects(visible_rect, visible_rect,
                                           visible_rect, visible_rect, 1.0f,
                                           Occlusion());

  // Set the second tiling as recycled.
  active_client.set_twin_tiling(nullptr);
  recycle_client.set_twin_tiling(nullptr);

  EXPECT_TRUE(active_tiling->TileAt(0, 0));
  EXPECT_FALSE(recycle_tiling->TileAt(0, 0));

  // Reset the active tiling. The recycle tiles should be released too.
  active_tiling->Reset();
  EXPECT_FALSE(active_tiling->TileAt(0, 0));
  EXPECT_FALSE(recycle_tiling->TileAt(0, 0));
}

TEST(PictureLayerTilingTest, EdgeCaseTileNowAndRequired) {
  FakePictureLayerTilingClient pending_client;
  pending_client.SetTileSize(gfx::Size(100, 100));

  scoped_refptr<FakeRasterSource> raster_source =
      FakeRasterSource::CreateFilled(gfx::Size(500, 500));
  std::unique_ptr<TestablePictureLayerTiling> pending_tiling =
      TestablePictureLayerTiling::Create(PENDING_TREE, gfx::AxisTransform2d(),
                                         raster_source, &pending_client,
                                         LayerTreeSettings());
  pending_tiling->set_resolution(HIGH_RESOLUTION);
  pending_tiling->set_can_require_tiles_for_activation(true);

  // The tile at (1, 0) should be touching the visible rect, but not
  // intersecting it.
  gfx::Rect visible_rect = gfx::Rect(0, 0, 99, 99);
  gfx::Rect eventually_rect = gfx::Rect(0, 0, 500, 500);
  pending_tiling->ComputeTilePriorityRects(visible_rect, visible_rect,
                                           visible_rect, eventually_rect, 1.f,
                                           Occlusion());

  Tile* tile = pending_tiling->TileAt(1, 0);
  EXPECT_NE(pending_tiling->visible_rect_type(),
            pending_tiling->ComputePriorityRectTypeForTile(tile));
  EXPECT_FALSE(pending_tiling->IsTileRequiredForActivation(tile));
  EXPECT_TRUE(tile->content_rect().Intersects(visible_rect));
  EXPECT_FALSE(pending_tiling->tiling_data()
                   ->TileBounds(tile->tiling_i_index(), tile->tiling_j_index())
                   .Intersects(visible_rect));

  // Now the tile at (1, 0) should be intersecting the visible rect.
  visible_rect = gfx::Rect(0, 0, 100, 100);
  pending_tiling->ComputeTilePriorityRects(visible_rect, visible_rect,
                                           visible_rect, eventually_rect, 1.f,
                                           Occlusion());
  EXPECT_EQ(pending_tiling->visible_rect_type(),
            pending_tiling->ComputePriorityRectTypeForTile(tile));
  EXPECT_TRUE(pending_tiling->IsTileRequiredForActivation(tile));
  EXPECT_TRUE(tile->content_rect().Intersects(visible_rect));
  EXPECT_TRUE(pending_tiling->tiling_data()
                  ->TileBounds(tile->tiling_i_index(), tile->tiling_j_index())
                  .Intersects(visible_rect));
}

TEST_F(PictureLayerTilingIteratorTest, ResizeTilesAndUpdateToCurrent) {
  // The tiling has four rows and three columns.
  InitializeFilled(gfx::Size(150, 100), 1.f, gfx::Size(250, 150));
  tiling_->CreateAllTilesForTesting();
  EXPECT_EQ(150, tiling_->TilingDataForTesting().max_texture_size().width());
  EXPECT_EQ(100, tiling_->TilingDataForTesting().max_texture_size().height());
  EXPECT_EQ(4u, tiling_->AllTilesForTesting().size());

  client_.SetTileSize(gfx::Size(250, 200));

  // Tile size in the tiling should still be 150x100.
  EXPECT_EQ(150, tiling_->TilingDataForTesting().max_texture_size().width());
  EXPECT_EQ(100, tiling_->TilingDataForTesting().max_texture_size().height());

  // The layer's size isn't changed, but the tile size was.
  scoped_refptr<FakeRasterSource> raster_source =
      FakeRasterSource::CreateFilled(gfx::Size(250, 150));
  tiling_->SetRasterSourceAndResize(raster_source);

  // Tile size in the tiling should be resized to 250x200.
  EXPECT_EQ(250, tiling_->TilingDataForTesting().max_texture_size().width());
  EXPECT_EQ(200, tiling_->TilingDataForTesting().max_texture_size().height());
  EXPECT_EQ(0u, tiling_->AllTilesForTesting().size());
}

// This test runs into floating point issues because of big numbers.
TEST_F(PictureLayerTilingIteratorTest, GiantRect) {
  loose_texel_extent_check_ = true;

  gfx::Size tile_size(256, 256);
  gfx::Size layer_size(33554432, 33554432);
  float contents_scale = 1.f;

  client_.SetTileSize(tile_size);
  scoped_refptr<FakeRasterSource> raster_source =
      FakeRasterSource::CreateFilled(layer_size);
  tiling_ = TestablePictureLayerTiling::Create(
      PENDING_TREE, gfx::AxisTransform2d(contents_scale, gfx::Vector2dF()),
      raster_source, &client_, LayerTreeSettings());

  gfx::Rect content_rect(25554432, 25554432, 950, 860);
  VerifyTilesExactlyCoverRect(contents_scale, content_rect);
}

TEST_F(PictureLayerTilingIteratorTest, QuadShouldNotUseLastHalfTexel) {
  InitializeFilled(gfx::Size(100, 100), 1.f, gfx::Size(198, 198));
  // Creates a situation that tile bounds get rounded up by almost 1px in the
  // dest space. This test verifies that even in such situation the coverage
  // iterator won't generate a texture rect that can potentially get clamped.
  VerifyTilesExactlyCoverRect(1.000005f, gfx::Rect(199, 199));
}

static void TileHasGeometryRect(const gfx::Rect& expected_rect,
                                Tile* tile,
                                const gfx::Rect& geometry_rect) {
  EXPECT_EQ(expected_rect, geometry_rect);
}

TEST_F(PictureLayerTilingIteratorTest, UseLeastTilesToCover) {
  // This test verifies that when a dest pixel can be covered by more than
  // one tiles, least number of tiles gets emitted.
  InitializeFilled(gfx::Size(100, 100), 1.f, gfx::Size(1000, 1000));
  gfx::RectF overlaped =
      gfx::ScaleRect(gfx::RectF(198.f, 198.f, 1.f, 1.f), 1.f / 2.f);
  ASSERT_TRUE(tiling_->tiling_data()->TexelExtent(0, 0).Contains(overlaped));
  ASSERT_TRUE(tiling_->tiling_data()->TexelExtent(1, 0).Contains(overlaped));
  ASSERT_TRUE(tiling_->tiling_data()->TexelExtent(0, 1).Contains(overlaped));
  ASSERT_TRUE(tiling_->tiling_data()->TexelExtent(1, 1).Contains(overlaped));
  VerifyTilesExactlyCoverRect(2.f, gfx::Rect(199, 199));
  VerifyTiles(2.f, gfx::Rect(199, 199),
              base::BindRepeating(&TileHasGeometryRect, gfx::Rect(199, 199)));
}

TEST_F(PictureLayerTilingIteratorTest, UseLeastTilesToCover2) {
  // Similar to above test, but with an internal tile.
  InitializeFilled(gfx::Size(100, 100), 1.f, gfx::Size(1000, 1000));
  gfx::RectF overlaped =
      gfx::ScaleRect(gfx::RectF(197.f, 393.f, 1.f, 1.f), 1.f / 2.f);
  ASSERT_TRUE(tiling_->tiling_data()->TexelExtent(0, 1).Contains(overlaped));
  ASSERT_TRUE(tiling_->tiling_data()->TexelExtent(1, 1).Contains(overlaped));
  ASSERT_TRUE(tiling_->tiling_data()->TexelExtent(0, 2).Contains(overlaped));
  ASSERT_TRUE(tiling_->tiling_data()->TexelExtent(1, 2).Contains(overlaped));
  gfx::Rect dest_rect(197, 393, 198, 198);
  VerifyTilesExactlyCoverRect(2.f, dest_rect);
  VerifyTiles(2.f, dest_rect,
              base::BindRepeating(&TileHasGeometryRect, dest_rect));
}

TEST_F(PictureLayerTilingIteratorTest, TightCover) {
  // This test verifies that the whole dest rect is still fully covered when
  // numerical condition is tight.
  // In this test, the right edge of tile #37 almost (but failed to) covered
  // grid line x = 9654. Tile #38 needs to reach hard to x = 9653 to make up
  // for this.
  InitializeFilled(gfx::Size(256, 256), 1.f, gfx::Size(10000, 1));
  float dest_scale = 16778082.f / 16777216.f;  // 0b1.00000000 00000011 01100010
  VerifyTilesExactlyCoverRect(dest_scale, gfx::Rect(10001, 2));
}

TEST_F(PictureLayerTilingIteratorTest, TightCover2) {
  // In this test, the left edge of tile #38 almost (but failed to) covered
  // grid line x = 9653. Tile #37 needs to reach hard to x = 9654 to make up
  // for this.
  InitializeFilled(gfx::Size(256, 256), 1.f, gfx::Size(10000, 1));
  float dest_scale = 16778088.f / 16777216.f;  // 0b1.00000000 00000011 01101000
  VerifyTilesExactlyCoverRect(dest_scale, gfx::Rect(10001, 2));
}

TEST_F(PictureLayerTilingIteratorTest, TilesStoreTilings) {
  gfx::Size bounds(200, 200);
  InitializeFilled(gfx::Size(100, 100), 1.f, bounds);
  gfx::Rect rect(bounds);
  SetLiveRectAndVerifyTiles(rect);
  tiling_->SetTilePriorityRectsForTesting(rect, rect, rect, rect);

  // Get all tiles and ensure they are associated with |tiling_|.
  std::vector<Tile*> tiles = tiling_->AllTilesForTesting();
  EXPECT_TRUE(tiles.size());
  for (const auto* tile : tiles) {
    EXPECT_EQ(tile->tiling(), tiling_.get());
  }

  // Create an active tiling, transfer tiles to that tiling, and ensure that
  // the tiles have their tiling updated.
  scoped_refptr<FakeRasterSource> raster_source =
      FakeRasterSource::CreateFilled(bounds);
  auto active_tiling = TestablePictureLayerTiling::Create(
      ACTIVE_TREE, gfx::AxisTransform2d(), raster_source, &client_,
      LayerTreeSettings());
  active_tiling->set_resolution(HIGH_RESOLUTION);

  active_tiling->TakeTilesAndPropertiesFrom(tiling_.get(),
                                            Region(gfx::Rect(bounds)));
  for (const auto* tile : tiles) {
    EXPECT_EQ(tile->tiling(), active_tiling.get());
  }
}

TEST_F(PictureLayerTilingIteratorTest, FractionalTranslatedTiling) {
  InitializeWithTransform(
      gfx::Size(256, 256),
      gfx::AxisTransform2d(1.f, gfx::Vector2dF(0.125f, 0.125f)),
      gfx::Size(1000, 1));
  EXPECT_EQ(tiling_->tiling_rect(), gfx::Rect(1001, 2));
  SetLiveRectAndVerifyTiles(gfx::Rect(1000, 1));

  // Verifies the texture coordinate is correctly translated.
  for (PictureLayerTiling::CoverageIterator iter(tiling_.get(), 1.f,
                                                 gfx::Rect(1000, 1));
       iter; ++iter) {
    gfx::Rect geometry_rect = iter.geometry_rect();
    gfx::RectF texture_rect = iter.texture_rect();
    if (geometry_rect == gfx::Rect(0, 0, 255, 1)) {
      EXPECT_EQ(gfx::RectF(0.125f, 0.125f, 255.f, 1.f), texture_rect);
    } else if (geometry_rect == gfx::Rect(255, 0, 254, 1)) {
      EXPECT_EQ(gfx::RectF(1.125f, 0.125f, 254.f, 1.f), texture_rect);
    } else if (geometry_rect == gfx::Rect(509, 0, 254, 1)) {
      EXPECT_EQ(gfx::RectF(1.125f, 0.125f, 254.f, 1.f), texture_rect);
    } else {
      EXPECT_EQ(gfx::Rect(763, 0, 237, 1), geometry_rect);
      EXPECT_EQ(gfx::RectF(1.125f, 0.125f, 237.f, 1.f), texture_rect);
    }
  }

  // Similar to above, with a different coverage scale.
  for (PictureLayerTiling::CoverageIterator iter(tiling_.get(), 1.375f,
                                                 gfx::Rect(1375, 2));
       iter; ++iter) {
    gfx::Rect geometry_rect = iter.geometry_rect();
    gfx::RectF texture_rect = iter.texture_rect();
    if (geometry_rect == gfx::Rect(0, 0, 351, 2)) {
      gfx::RectF expectation(geometry_rect);
      expectation.InvScale(1.375f);
      expectation.Offset(0.125f, 0.125f);
      EXPECT_FLOAT_EQ(expectation.x(), texture_rect.x());
      EXPECT_FLOAT_EQ(expectation.y(), texture_rect.y());
      EXPECT_FLOAT_EQ(expectation.width(), texture_rect.width());
      EXPECT_FLOAT_EQ(expectation.height(), texture_rect.height());
    } else if (geometry_rect == gfx::Rect(351, 0, 349, 2)) {
      gfx::RectF expectation(geometry_rect);
      expectation.InvScale(1.375f);
      expectation.Offset(0.125f - 254.f, 0.125f);
      EXPECT_NEAR(expectation.x(), texture_rect.x(), 1e-4);
      EXPECT_FLOAT_EQ(expectation.y(), texture_rect.y());
      EXPECT_FLOAT_EQ(expectation.width(), texture_rect.width());
      EXPECT_FLOAT_EQ(expectation.height(), texture_rect.height());
    } else if (geometry_rect == gfx::Rect(700, 0, 349, 2)) {
      gfx::RectF expectation(geometry_rect);
      expectation.InvScale(1.375f);
      expectation.Offset(0.125f - 254.f * 2.f, 0.125f);
      EXPECT_FLOAT_EQ(expectation.x(), texture_rect.x());
      EXPECT_FLOAT_EQ(expectation.y(), texture_rect.y());
      EXPECT_FLOAT_EQ(expectation.width(), texture_rect.width());
      EXPECT_FLOAT_EQ(expectation.height(), texture_rect.height());
    } else {
      EXPECT_EQ(gfx::Rect(1049, 0, 326, 2), geometry_rect);
      gfx::RectF expectation(geometry_rect);
      expectation.InvScale(1.375f);
      expectation.Offset(0.125f - 254.f * 3.f, 0.125f);
      EXPECT_FLOAT_EQ(expectation.x(), texture_rect.x());
      EXPECT_FLOAT_EQ(expectation.y(), texture_rect.y());
      EXPECT_FLOAT_EQ(expectation.width(), texture_rect.width());
      EXPECT_FLOAT_EQ(expectation.height(), texture_rect.height());
    }
  }
}

TEST_F(PictureLayerTilingIteratorTest,
       FractionalTranslatedTilingPartiallyFilled) {
  Initialize(gfx::Size(256, 256),
             FakeRasterSource::CreatePartiallyFilled(gfx::Size(2000, 3),
                                                     gfx::Rect(320, 0, 600, 1)),
             gfx::AxisTransform2d(1.f, gfx::Vector2dF(0.125f, 0.125f)));
  EXPECT_EQ(tiling_->tiling_rect(), gfx::Rect(256, 0, 665, 4));
  SetLiveRectAndVerifyTiles(gfx::Rect(350, 0, 500, 1));
  EXPECT_EQ(3u, tiling_->AllTilesForTesting().size());

  // Verifies the texture coordinate is correctly translated.
  int i = 0;
  for (PictureLayerTiling::CoverageIterator iter(tiling_.get(), 1.f,
                                                 gfx::Rect(2000, 6));
       iter; ++iter, ++i) {
    gfx::Rect geometry_rect = iter.geometry_rect();
    gfx::RectF texture_rect = iter.texture_rect();
    switch (i) {
      case 0:
        EXPECT_EQ(gfx::Rect(255, 0, 256, 3), geometry_rect);
        EXPECT_EQ(gfx::RectF(-0.875f, 0.125f, 256, 3), texture_rect);
        break;
      case 1:
        EXPECT_EQ(gfx::Rect(511, 0, 254, 3), geometry_rect);
        EXPECT_EQ(gfx::RectF(1.125f, 0.125f, 254, 3), texture_rect);
        break;
      case 2:
        EXPECT_EQ(gfx::Rect(765, 0, 156, 3), geometry_rect);
        EXPECT_EQ(gfx::RectF(1.125f, 0.125f, 156, 3), texture_rect);
        break;
      default:
        NOTREACHED();
    }
  }
  EXPECT_EQ(3, i);
}

TEST_F(PictureLayerTilingIteratorTest, FractionalTranslatedTilingOverflow) {
  // This tests a corner case where the coverage rect is slightly greater
  // than the layer rect due to rounding up, and the bottom right edge of
  // the tiling coincide with actual layer bound. That is, the requested
  // coverage rect slightly exceed the valid extent, but we still return
  // full coverage as a special case for external edges.

  // The layer bounds is (9, 9), which after scale and translation
  // becomes (l=0.5, t=0.5, r=14, b=14) in the contents space.
  InitializeWithTransform(
      gfx::Size(256, 256),
      gfx::AxisTransform2d(1.5f, gfx::Vector2dF(0.5f, 0.5f)), gfx::Size(9, 9));
  EXPECT_EQ(tiling_->tiling_rect(), gfx::Rect(14, 14));
  SetLiveRectAndVerifyTiles(gfx::Rect(9, 9));

  PictureLayerTiling::CoverageIterator iter(tiling_.get(), 1.56f,
                                            gfx::Rect(15, 15));
  ASSERT_TRUE(iter);
  gfx::Rect geometry_rect = iter.geometry_rect();
  gfx::RectF texture_rect = iter.texture_rect();
  EXPECT_EQ(gfx::Rect(0, 0, 15, 15), geometry_rect);
  gfx::RectF expectation(geometry_rect);
  expectation.Scale(1.5f / 1.56f);
  expectation.Offset(0.5f, 0.5f);
  EXPECT_FLOAT_EQ(expectation.x(), texture_rect.x());
  EXPECT_FLOAT_EQ(expectation.y(), texture_rect.y());
  EXPECT_FLOAT_EQ(expectation.width(), texture_rect.width());
  EXPECT_FLOAT_EQ(expectation.height(), texture_rect.height());

  EXPECT_FALSE(++iter);
}

TEST_F(PictureLayerTilingIteratorTest, EdgeCaseLargeIntBounds) {
  gfx::Size tile_size(256, 256);
  float scale = 7352.331055f;
  gfx::Size layer_bounds(292082, 26910);
  gfx::Rect coverage_rect(2104641536, 522015, 29440, 66172);
  InitializeFilled(tile_size, scale, layer_bounds);
  int count = 0;
  for (PictureLayerTiling::CoverageIterator
           iter(tiling_.get(), scale, coverage_rect);
       iter && count < 200; ++count, ++iter) {
    EXPECT_FALSE(iter.geometry_rect().IsEmpty());
  }
}

TEST_F(PictureLayerTilingIteratorTest, EdgeCaseLargeIntBounds2) {
  gfx::Size tile_size(256, 256);
  float scale = 7352.331055f;
  gfx::Size layer_bounds(292082, 26910);
  gfx::Rect coverage_rect(2104670720, 522015, 192, 1);
  InitializeFilled(tile_size, scale, layer_bounds);
  for (PictureLayerTiling::CoverageIterator iter(tiling_.get(), scale,
                                                 coverage_rect);
       iter; ++iter) {
    EXPECT_FALSE(iter.geometry_rect().IsEmpty());
  }
}

TEST_F(PictureLayerTilingIteratorTest, SmallRasterTransforms) {
  gfx::Size tile_size(1, 1);
  gfx::Size layer_bounds(4357, 4357);
  float scale = 1.f / layer_bounds.width();
  InitializeFilled(tile_size, scale, layer_bounds);
  EXPECT_EQ(tiling_->tiling_rect(), gfx::Rect(tile_size));

  layer_bounds = {378, 378};
  scale = 1.f / layer_bounds.width();
  InitializeFilled(tile_size, scale, layer_bounds);
  EXPECT_EQ(tiling_->tiling_rect(), gfx::Rect(tile_size));
}

TEST_F(PictureLayerTilingIteratorTest, TilingSizeChange) {
  gfx::Size tile_size(2940, 478);
  gfx::Size original_layer_size(2940, 5518);

  InitializeFilled(tile_size, 1.f, original_layer_size);

  gfx::Rect visible_rect(0, 5520, 2940, 1840);
  gfx::Rect skewport_rect(0, 5520, 2940, 1840);
  gfx::Rect soon_border_rect(-312, 5208, 3564, 2464);
  gfx::Rect eventually_rect(0, 2391, 2940, 3127);
  tiling_->ComputeTilePriorityRects(
      gfx::Rect(visible_rect),      // visible rect
      gfx::Rect(skewport_rect),     // skewport
      gfx::Rect(soon_border_rect),  // soon border rect
      gfx::Rect(eventually_rect),   // eventually rect
      1.f,                          // current contents scale
      Occlusion());

  EXPECT_FALSE(tiling_->has_visible_rect_tiles());
  EXPECT_FALSE(tiling_->has_skewport_rect_tiles());
  EXPECT_TRUE(tiling_->has_soon_border_rect_tiles());
  EXPECT_TRUE(tiling_->has_eventually_rect_tiles());

  // |PictureLayerTilingSet::UpdateTilePriorities| may exit prematurely,
  // resulting in |current_xxx_rect| not being updated and |has_xxx_rect_|
  // not being recalculated.
  // |tiling_->ComputeTilePriorityRects| not run, because
  // |PictureLayerTilingSet::UpdateTilePriorities| early out.

  // |PictureLayer::PushPropertiesTo| will not exit early and will
  // update tiling_rect.
  gfx::Size new_layer_size(2940, 12880);
  scoped_refptr<FakeRasterSource> raster_source =
      FakeRasterSource::CreateFilled(new_layer_size);
  tiling_->SetRasterSourceAndResize(raster_source);

  // |has_xxx_rect_tiles_| refers to whether current_rect and
  // tiling_rect overlap. Once tiling_rect changes, it also needs to be
  // recalculated.
  EXPECT_TRUE(tiling_->has_visible_rect_tiles());
  EXPECT_TRUE(tiling_->has_skewport_rect_tiles());
  EXPECT_TRUE(tiling_->has_soon_border_rect_tiles());
  EXPECT_TRUE(tiling_->has_eventually_rect_tiles());
}

TEST_F(PictureLayerTilingIteratorTest, LiveTilesRectChange) {
  EXPECT_EQ(TileMemoryLimitPolicy::ALLOW_ANYTHING,
            client_.global_tile_state().memory_limit_policy);
  gfx::Size tile_size(100, 100);
  gfx::Size layer_size(800, 600);

  InitializeFilled(tile_size, 1.f, layer_size);

  gfx::Rect visible_rect(0, 0, 100, 100);
  gfx::Rect skewport_rect(0, 0, 200, 200);
  gfx::Rect soon_border_rect(0, 0, 300, 300);
  gfx::Rect eventually_rect(0, 0, 400, 400);
  tiling_->ComputeTilePriorityRects(
      gfx::Rect(visible_rect),      // visible rect
      gfx::Rect(skewport_rect),     // skewport
      gfx::Rect(soon_border_rect),  // soon border rect
      gfx::Rect(eventually_rect),   // eventually rect
      1.f,                          // current contents scale
      Occlusion());

  // memory limit policy is ALLOW_ANYTHING.
  EXPECT_EQ(eventually_rect, tiling_->live_tiles_rect());
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      Tile* tile = tiling_->TileAt(i, j);
      EXPECT_TRUE(tile);
      EXPECT_FALSE(tile->IsReadyToDraw());
    }
  }

  gfx::Rect new_live_tiles_rect(0, 0, 200, 200);
  tiling_->SetLiveTilesRect(new_live_tiles_rect);
  // Changing live_tiles_rect does not evict tiles.
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      Tile* tile = tiling_->TileAt(i, j);
      EXPECT_TRUE(tile);
    }
  }

  EXPECT_FALSE(tiling_->all_tiles_done());
  tiling_->set_all_tiles_done(true);
  EXPECT_TRUE(tiling_->all_tiles_done());

  gfx::Rect expanded_live_tiles_rect(0, 0, 300, 200);
  tiling_->SetLiveTilesRect(expanded_live_tiles_rect);
  // Now we expand the live_tiles_rect, and a tile that was outside
  // the previous live_tiles_rect is now included. That tile is
  // not ready to draw, thus set clearing all_tiles_done.
  EXPECT_FALSE(tiling_->all_tiles_done());

  gfx::Rect new_eventually_rect(0, 0, 400, 250);
  tiling_->SetEventuallyRect(new_eventually_rect);
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      Tile* tile = tiling_->TileAt(i, j);
      if (j == 3) {
        EXPECT_FALSE(tile);
      } else {
        EXPECT_TRUE(tile);
      }
    }
  }
}

TEST_F(PictureLayerTilingIteratorTest,
       ComputeTilePriorityRectsAllowPrepaintOnly) {
  client_.set_memory_limit_policy(TileMemoryLimitPolicy::ALLOW_PREPAINT_ONLY);
  EXPECT_EQ(TileMemoryLimitPolicy::ALLOW_PREPAINT_ONLY,
            client_.global_tile_state().memory_limit_policy);
  gfx::Size tile_size(100, 100);
  gfx::Size layer_size(800, 600);

  InitializeFilled(tile_size, 1.f, layer_size);

  gfx::Rect visible_rect(0, 0, 100, 100);
  gfx::Rect skewport_rect(0, 0, 200, 200);
  gfx::Rect soon_border_rect(0, 0, 400, 280);
  gfx::Rect eventually_rect(0, 0, 400, 400);
  tiling_->ComputeTilePriorityRects(
      gfx::Rect(visible_rect),      // visible rect
      gfx::Rect(skewport_rect),     // skewport
      gfx::Rect(soon_border_rect),  // soon border rect
      gfx::Rect(eventually_rect),   // eventually rect
      1.f,                          // current contents scale
      Occlusion());
  if (features::IsCCSlimmingEnabled()) {
    EXPECT_EQ(soon_border_rect, tiling_->live_tiles_rect());
  } else {
    EXPECT_EQ(eventually_rect, tiling_->live_tiles_rect());
  }

  // The difference between soon_border_rect and eventually_rect is the last
  // column.
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      Tile* tile = tiling_->TileAt(i, j);
      if (j == 3) {
        if (features::IsCCSlimmingEnabled()) {
          EXPECT_FALSE(tile);
        } else {
          EXPECT_TRUE(tile);
        }
      } else {
        EXPECT_TRUE(tile);
      }
    }
  }
}

}  // namespace
}  // namespace cc
