// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/picture_layer_tiling_set.h"

#include <map>
#include <vector>

#include "build/build_config.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/fake_picture_layer_tiling_client.h"
#include "cc/test/fake_raster_source.h"
#include "cc/trees/layer_tree_settings.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/test/fake_output_surface.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace cc {
namespace {

class TestablePictureLayerTilingSet : public PictureLayerTilingSet {
 public:
  TestablePictureLayerTilingSet(
      WhichTree tree,
      PictureLayerTilingClient* client,
      int tiling_interest_area_padding,
      float skewport_target_time_in_seconds,
      int skewport_extrapolation_limit_in_screen_pixels,
      float max_preraster_distance)
      : PictureLayerTilingSet(tree,
                              client,
                              tiling_interest_area_padding,
                              skewport_target_time_in_seconds,
                              skewport_extrapolation_limit_in_screen_pixels,
                              max_preraster_distance) {}

  using PictureLayerTilingSet::ComputeSkewport;
  using PictureLayerTilingSet::ComputeSoonBorderRect;
  using PictureLayerTilingSet::TilingsNeedUpdate;
};

std::unique_ptr<TestablePictureLayerTilingSet> CreateTilingSetWithSettings(
    PictureLayerTilingClient* client,
    const LayerTreeSettings& settings) {
  return std::make_unique<TestablePictureLayerTilingSet>(
      ACTIVE_TREE, client, settings.tiling_interest_area_padding,
      settings.skewport_target_time_in_seconds,
      settings.skewport_extrapolation_limit_in_screen_pixels,
      settings.max_preraster_distance_in_screen_pixels);
}

std::unique_ptr<TestablePictureLayerTilingSet> CreateTilingSet(
    PictureLayerTilingClient* client) {
  return CreateTilingSetWithSettings(client, LayerTreeSettings());
}

TEST(PictureLayerTilingSetTest, TilingRange) {
  FakePictureLayerTilingClient client;
  gfx::Size layer_bounds(10, 10);
  PictureLayerTilingSet::TilingRange higher_than_high_res_range(0, 0);
  PictureLayerTilingSet::TilingRange high_res_range(0, 0);
  PictureLayerTilingSet::TilingRange between_high_and_low_res_range(0, 0);
  PictureLayerTilingSet::TilingRange low_res_range(0, 0);
  PictureLayerTilingSet::TilingRange lower_than_low_res_range(0, 0);
  PictureLayerTiling* high_res_tiling;
  PictureLayerTiling* low_res_tiling;

  scoped_refptr<FakeRasterSource> raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);

  std::unique_ptr<TestablePictureLayerTilingSet> set = CreateTilingSet(&client);
  set->AddTiling(gfx::AxisTransform2d(2.0, gfx::Vector2dF()), raster_source);
  high_res_tiling = set->AddTiling(gfx::AxisTransform2d(), raster_source);
  high_res_tiling->set_resolution(HIGH_RESOLUTION);
  set->AddTiling(gfx::AxisTransform2d(0.5, gfx::Vector2dF()), raster_source);
  low_res_tiling = set->AddTiling(gfx::AxisTransform2d(0.25, gfx::Vector2dF()),
                                  raster_source);
  low_res_tiling->set_resolution(LOW_RESOLUTION);
  set->AddTiling(gfx::AxisTransform2d(0.125, gfx::Vector2dF()), raster_source);

  higher_than_high_res_range =
      set->GetTilingRange(PictureLayerTilingSet::HIGHER_THAN_HIGH_RES);
  EXPECT_EQ(0u, higher_than_high_res_range.start);
  EXPECT_EQ(1u, higher_than_high_res_range.end);

  high_res_range = set->GetTilingRange(PictureLayerTilingSet::HIGH_RES);
  EXPECT_EQ(1u, high_res_range.start);
  EXPECT_EQ(2u, high_res_range.end);

  between_high_and_low_res_range =
      set->GetTilingRange(PictureLayerTilingSet::BETWEEN_HIGH_AND_LOW_RES);
  EXPECT_EQ(2u, between_high_and_low_res_range.start);
  EXPECT_EQ(3u, between_high_and_low_res_range.end);

  low_res_range = set->GetTilingRange(PictureLayerTilingSet::LOW_RES);
  EXPECT_EQ(3u, low_res_range.start);
  EXPECT_EQ(4u, low_res_range.end);

  lower_than_low_res_range =
      set->GetTilingRange(PictureLayerTilingSet::LOWER_THAN_LOW_RES);
  EXPECT_EQ(4u, lower_than_low_res_range.start);
  EXPECT_EQ(5u, lower_than_low_res_range.end);

  std::unique_ptr<TestablePictureLayerTilingSet> set_without_low_res =
      CreateTilingSet(&client);
  set_without_low_res->AddTiling(gfx::AxisTransform2d(2.0, gfx::Vector2dF()),
                                 raster_source);
  high_res_tiling =
      set_without_low_res->AddTiling(gfx::AxisTransform2d(), raster_source);
  high_res_tiling->set_resolution(HIGH_RESOLUTION);
  set_without_low_res->AddTiling(gfx::AxisTransform2d(0.5, gfx::Vector2dF()),
                                 raster_source);
  set_without_low_res->AddTiling(gfx::AxisTransform2d(0.25, gfx::Vector2dF()),
                                 raster_source);

  higher_than_high_res_range = set_without_low_res->GetTilingRange(
      PictureLayerTilingSet::HIGHER_THAN_HIGH_RES);
  EXPECT_EQ(0u, higher_than_high_res_range.start);
  EXPECT_EQ(1u, higher_than_high_res_range.end);

  high_res_range =
      set_without_low_res->GetTilingRange(PictureLayerTilingSet::HIGH_RES);
  EXPECT_EQ(1u, high_res_range.start);
  EXPECT_EQ(2u, high_res_range.end);

  between_high_and_low_res_range = set_without_low_res->GetTilingRange(
      PictureLayerTilingSet::BETWEEN_HIGH_AND_LOW_RES);
  EXPECT_EQ(2u, between_high_and_low_res_range.start);
  EXPECT_EQ(4u, between_high_and_low_res_range.end);

  low_res_range =
      set_without_low_res->GetTilingRange(PictureLayerTilingSet::LOW_RES);
  EXPECT_EQ(0u, low_res_range.end - low_res_range.start);

  lower_than_low_res_range = set_without_low_res->GetTilingRange(
      PictureLayerTilingSet::LOWER_THAN_LOW_RES);
  EXPECT_EQ(0u, lower_than_low_res_range.end - lower_than_low_res_range.start);

  std::unique_ptr<TestablePictureLayerTilingSet>
      set_with_only_high_and_low_res = CreateTilingSet(&client);
  high_res_tiling = set_with_only_high_and_low_res->AddTiling(
      gfx::AxisTransform2d(), raster_source);
  high_res_tiling->set_resolution(HIGH_RESOLUTION);
  low_res_tiling = set_with_only_high_and_low_res->AddTiling(
      gfx::AxisTransform2d(0.5, gfx::Vector2dF()), raster_source);
  low_res_tiling->set_resolution(LOW_RESOLUTION);

  higher_than_high_res_range = set_with_only_high_and_low_res->GetTilingRange(
      PictureLayerTilingSet::HIGHER_THAN_HIGH_RES);
  EXPECT_EQ(0u,
            higher_than_high_res_range.end - higher_than_high_res_range.start);

  high_res_range = set_with_only_high_and_low_res->GetTilingRange(
      PictureLayerTilingSet::HIGH_RES);
  EXPECT_EQ(0u, high_res_range.start);
  EXPECT_EQ(1u, high_res_range.end);

  between_high_and_low_res_range =
      set_with_only_high_and_low_res->GetTilingRange(
          PictureLayerTilingSet::BETWEEN_HIGH_AND_LOW_RES);
  EXPECT_EQ(0u, between_high_and_low_res_range.end -
                    between_high_and_low_res_range.start);

  low_res_range = set_with_only_high_and_low_res->GetTilingRange(
      PictureLayerTilingSet::LOW_RES);
  EXPECT_EQ(1u, low_res_range.start);
  EXPECT_EQ(2u, low_res_range.end);

  lower_than_low_res_range = set_with_only_high_and_low_res->GetTilingRange(
      PictureLayerTilingSet::LOWER_THAN_LOW_RES);
  EXPECT_EQ(0u, lower_than_low_res_range.end - lower_than_low_res_range.start);

  std::unique_ptr<TestablePictureLayerTilingSet> set_with_only_high_res =
      CreateTilingSet(&client);
  high_res_tiling =
      set_with_only_high_res->AddTiling(gfx::AxisTransform2d(), raster_source);
  high_res_tiling->set_resolution(HIGH_RESOLUTION);

  higher_than_high_res_range = set_with_only_high_res->GetTilingRange(
      PictureLayerTilingSet::HIGHER_THAN_HIGH_RES);
  EXPECT_EQ(0u,
            higher_than_high_res_range.end - higher_than_high_res_range.start);

  high_res_range =
      set_with_only_high_res->GetTilingRange(PictureLayerTilingSet::HIGH_RES);
  EXPECT_EQ(0u, high_res_range.start);
  EXPECT_EQ(1u, high_res_range.end);

  between_high_and_low_res_range = set_with_only_high_res->GetTilingRange(
      PictureLayerTilingSet::BETWEEN_HIGH_AND_LOW_RES);
  EXPECT_EQ(0u, between_high_and_low_res_range.end -
                    between_high_and_low_res_range.start);

  low_res_range =
      set_with_only_high_res->GetTilingRange(PictureLayerTilingSet::LOW_RES);
  EXPECT_EQ(0u, low_res_range.end - low_res_range.start);

  lower_than_low_res_range = set_with_only_high_res->GetTilingRange(
      PictureLayerTilingSet::LOWER_THAN_LOW_RES);
  EXPECT_EQ(0u, lower_than_low_res_range.end - lower_than_low_res_range.start);
}

class PictureLayerTilingSetTestWithResources : public testing::Test {
 public:
  void RunTest(int num_tilings,
               float min_scale,
               float scale_increment,
               float ideal_contents_scale,
               float expected_scale) {
    scoped_refptr<viz::TestContextProvider> context_provider =
        viz::TestContextProvider::CreateRaster();
    ASSERT_EQ(context_provider->BindToCurrentSequence(),
              gpu::ContextResult::kSuccess);
    std::unique_ptr<viz::ClientResourceProvider> resource_provider =
        std::make_unique<viz::ClientResourceProvider>();

    FakePictureLayerTilingClient client(resource_provider.get(),
                                        context_provider.get());
    client.SetTileSize(gfx::Size(256, 256));
    gfx::Size layer_bounds(1000, 800);
    std::unique_ptr<TestablePictureLayerTilingSet> set =
        CreateTilingSet(&client);
    scoped_refptr<FakeRasterSource> raster_source =
        FakeRasterSource::CreateFilled(layer_bounds);

    float scale = min_scale;
    for (int i = 0; i < num_tilings; ++i, scale += scale_increment) {
      PictureLayerTiling* tiling = set->AddTiling(
          gfx::AxisTransform2d(scale, gfx::Vector2dF()), raster_source);
      tiling->set_resolution(HIGH_RESOLUTION);
      tiling->CreateAllTilesForTesting();
      std::vector<Tile*> tiles = tiling->AllTilesForTesting();
      client.tile_manager()->InitializeTilesWithResourcesForTesting(tiles);
    }

    float max_contents_scale = scale;
    gfx::Size content_bounds(
        gfx::ScaleToCeiledSize(layer_bounds, max_contents_scale));
    gfx::Rect content_rect(content_bounds);

    Region remaining(content_rect);
    for (auto iter =
             set->Cover(content_rect, max_contents_scale, ideal_contents_scale);
         iter; ++iter) {
      gfx::Rect geometry_rect = iter.geometry_rect();
      EXPECT_TRUE(content_rect.Contains(geometry_rect));
      ASSERT_TRUE(remaining.Contains(geometry_rect));
      remaining.Subtract(geometry_rect);

      EXPECT_EQ(expected_scale, iter.CurrentTiling()->contents_scale_key());

      if (num_tilings)
        EXPECT_TRUE(*iter);
      else
        EXPECT_FALSE(*iter);
    }
    EXPECT_TRUE(remaining.IsEmpty());
  }
};

TEST_F(PictureLayerTilingSetTestWithResources, NoTilings) {
  RunTest(0, 0.f, 0.f, 2.f, 0.f);
}
TEST_F(PictureLayerTilingSetTestWithResources, OneTiling_Smaller) {
  RunTest(1, 1.f, 0.f, 2.f, 1.f);
}
TEST_F(PictureLayerTilingSetTestWithResources, OneTiling_Larger) {
  RunTest(1, 3.f, 0.f, 2.f, 3.f);
}
TEST_F(PictureLayerTilingSetTestWithResources, TwoTilings_Smaller) {
  RunTest(2, 1.f, 1.f, 3.f, 2.f);
}

TEST_F(PictureLayerTilingSetTestWithResources, TwoTilings_SmallerEqual) {
  RunTest(2, 1.f, 1.f, 2.f, 2.f);
}

TEST_F(PictureLayerTilingSetTestWithResources, TwoTilings_LargerEqual) {
  RunTest(2, 1.f, 1.f, 1.f, 1.f);
}

TEST_F(PictureLayerTilingSetTestWithResources, TwoTilings_Larger) {
  RunTest(2, 2.f, 8.f, 1.f, 2.f);
}

// Test is flaky: https://crbug.com/1056828.
#if BUILDFLAG(IS_LINUX) && defined(THREAD_SANITIZER)
#define MAYBE_ManyTilings_Equal DISABLED_ManyTilings_Equal
#else
#define MAYBE_ManyTilings_Equal ManyTilings_Equal
#endif
TEST_F(PictureLayerTilingSetTestWithResources, MAYBE_ManyTilings_Equal) {
  RunTest(10, 1.f, 1.f, 5.f, 5.f);
}

// Test is flaky: https://crbug.com/1056828.
#if BUILDFLAG(IS_LINUX) && defined(THREAD_SANITIZER)
#define MAYBE_ManyTilings_NotEqual DISABLED_ManyTilings_NotEqual
#else
#define MAYBE_ManyTilings_NotEqual ManyTilings_NotEqual
#endif
TEST_F(PictureLayerTilingSetTestWithResources, MAYBE_ManyTilings_NotEqual) {
  RunTest(10, 1.f, 1.f, 4.5f, 5.f);
}

TEST(PictureLayerTilingSetTest, TileSizeChange) {
  FakePictureLayerTilingClient pending_client;
  FakePictureLayerTilingClient active_client;
  std::unique_ptr<PictureLayerTilingSet> pending_set =
      PictureLayerTilingSet::Create(PENDING_TREE, &pending_client, 1000, 1.f,
                                    1000, 1000.f);
  std::unique_ptr<PictureLayerTilingSet> active_set =
      PictureLayerTilingSet::Create(ACTIVE_TREE, &active_client, 1000, 1.f,
                                    1000, 1000.f);

  gfx::Size layer_bounds(100, 100);
  scoped_refptr<FakeRasterSource> raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);

  gfx::Size tile_size1(10, 10);
  gfx::Size tile_size2(30, 30);
  gfx::Size tile_size3(20, 20);

  pending_client.SetTileSize(tile_size1);
  pending_set->AddTiling(gfx::AxisTransform2d(), raster_source);
  // New tilings get the correct tile size.
  EXPECT_EQ(tile_size1, pending_set->tiling_at(0)->tile_size());

  // Set some expected things for the tiling set to function.
  pending_set->tiling_at(0)->set_resolution(HIGH_RESOLUTION);
  active_client.set_twin_tiling_set(pending_set.get());

  // Set a priority rect so we get tiles.
  pending_set->UpdateTilePriorities(gfx::Rect(layer_bounds), 1.f, 1.0,
                                    Occlusion(), false);
  EXPECT_EQ(tile_size1, pending_set->tiling_at(0)->tile_size());

  // The tiles should get the correct size.
  std::vector<Tile*> pending_tiles =
      pending_set->tiling_at(0)->AllTilesForTesting();
  EXPECT_GT(pending_tiles.size(), 0u);
  for (auto* tile : pending_tiles) {
    EXPECT_EQ(tile_size1, tile->content_rect().size());
  }

  // Update to a new source frame with a new tile size.
  // Note that setting a new raster source can typically only happen after
  // activation, since we can't set the raster source twice on the pending tree
  // without activating. For test, just remove and add a new tiling instead.
  pending_set->RemoveAllTilings();
  pending_set->AddTiling(gfx::AxisTransform2d(), raster_source);
  pending_set->tiling_at(0)->set_resolution(HIGH_RESOLUTION);
  pending_client.SetTileSize(tile_size2);
  pending_set->UpdateTilingsToCurrentRasterSourceForCommit(raster_source.get(),
                                                           Region(), 1.f, 1.f);
  // The tiling should get the correct tile size.
  EXPECT_EQ(tile_size2, pending_set->tiling_at(0)->tile_size());

  // Set a priority rect so we get tiles.
  pending_set->UpdateTilePriorities(gfx::Rect(layer_bounds), 1.f, 2.0,
                                    Occlusion(), false);
  EXPECT_EQ(tile_size2, pending_set->tiling_at(0)->tile_size());

  // Tiles should have the new correct size.
  pending_tiles = pending_set->tiling_at(0)->AllTilesForTesting();
  EXPECT_GT(pending_tiles.size(), 0u);
  for (auto* tile : pending_tiles) {
    EXPECT_EQ(tile_size2, tile->content_rect().size());
  }

  // Clone from the pending to the active tree.
  active_client.SetTileSize(tile_size2);
  active_set->UpdateTilingsToCurrentRasterSourceForActivation(
      raster_source.get(), pending_set.get(), Region(), 1.f, 1.f);
  // The active tiling should get the right tile size.
  EXPECT_EQ(tile_size2, active_set->tiling_at(0)->tile_size());

  // Cloned tiles should have the right size.
  std::vector<Tile*> active_tiles =
      active_set->tiling_at(0)->AllTilesForTesting();
  EXPECT_GT(active_tiles.size(), 0u);
  for (auto* tile : active_tiles) {
    EXPECT_EQ(tile_size2, tile->content_rect().size());
  }

  // A new source frame with a new tile size.
  pending_client.SetTileSize(tile_size3);
  pending_set->UpdateTilingsToCurrentRasterSourceForCommit(raster_source.get(),
                                                           Region(), 1.f, 1.f);
  // The tiling gets the new size correctly.
  EXPECT_EQ(tile_size3, pending_set->tiling_at(0)->tile_size());

  // Set a priority rect so we get tiles.
  pending_set->UpdateTilePriorities(gfx::Rect(layer_bounds), 1.f, 3.0,
                                    Occlusion(), false);
  EXPECT_EQ(tile_size3, pending_set->tiling_at(0)->tile_size());

  // Tiles are resized for the new size.
  pending_tiles = pending_set->tiling_at(0)->AllTilesForTesting();
  EXPECT_GT(pending_tiles.size(), 0u);
  for (auto* tile : pending_tiles) {
    EXPECT_EQ(tile_size3, tile->content_rect().size());
  }

  // Now we activate with a different tile size for the active tiling.
  active_client.SetTileSize(tile_size3);
  active_set->UpdateTilingsToCurrentRasterSourceForActivation(
      raster_source.get(), pending_set.get(), Region(), 1.f, 1.f);
  // The active tiling changes its tile size.
  EXPECT_EQ(tile_size3, active_set->tiling_at(0)->tile_size());

  // And its tiles are resized.
  active_tiles = active_set->tiling_at(0)->AllTilesForTesting();
  EXPECT_GT(active_tiles.size(), 0u);
  for (auto* tile : active_tiles) {
    EXPECT_EQ(tile_size3, tile->content_rect().size());
  }
}

TEST(PictureLayerTilingSetTest, ModifyPendingTilingSetTwiceInOneVsync) {
  FakePictureLayerTilingClient pending_client;
  FakePictureLayerTilingClient active_client;
  std::unique_ptr<PictureLayerTilingSet> pending_set =
      PictureLayerTilingSet::Create(PENDING_TREE, &pending_client, 1000, 1.f,
                                    1000, 1000.f);
  std::unique_ptr<PictureLayerTilingSet> active_set =
      PictureLayerTilingSet::Create(ACTIVE_TREE, &active_client, 1000, 1.f,
                                    1000, 1000.f);

  gfx::Size layer_bounds(100, 100);
  scoped_refptr<FakeRasterSource> raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);

  gfx::Size tile_size1(10, 10);
  gfx::Size tile_size2(30, 30);

  // Initialize a tiling
  pending_client.SetTileSize(tile_size1);
  pending_set->AddTiling(gfx::AxisTransform2d(), raster_source);
  // New tilings get the correct tile size.
  EXPECT_EQ(tile_size1, pending_set->tiling_at(0)->tile_size());

  // Set some expected things for the tiling set to function.
  pending_set->tiling_at(0)->set_resolution(HIGH_RESOLUTION);
  active_client.set_twin_tiling_set(pending_set.get());

  // Set a priority rect so we get tiles.
  // Note that the current_frame_time_in_seconds parameter is 1.0.
  pending_set->UpdateTilePriorities(gfx::Rect(layer_bounds), 1.f, 1.0,
                                    Occlusion(), false);
  // The pending tiling should get the right tile size.
  EXPECT_EQ(tile_size1, pending_set->tiling_at(0)->tile_size());
  // The pending tiling should have tiles.
  EXPECT_TRUE(pending_set->tiling_at(0)->has_tiles());

  // Clone from the pending to the active tree.
  active_client.SetTileSize(tile_size1);
  active_set->UpdateTilingsToCurrentRasterSourceForActivation(
      raster_source.get(), pending_set.get(), Region(), 1.f, 1.f);
  // The active tiling should get the right tile size.
  EXPECT_EQ(tile_size1, active_set->tiling_at(0)->tile_size());
  // The active tiling should have tiles.
  EXPECT_TRUE(active_set->tiling_at(0)->has_tiles());

  // A new source frame with a new tile size.
  pending_client.SetTileSize(tile_size2);
  pending_set->UpdateTilingsToCurrentRasterSourceForCommit(raster_source.get(),
                                                           Region(), 1.f, 1.f);
  // The tiling gets the new size correctly.
  EXPECT_EQ(tile_size2, pending_set->tiling_at(0)->tile_size());

  // Re-update priority rect so we get new tiles.
  // Note that the current_frame_time_in_seconds parameter is still 1.0.
  pending_set->UpdateTilePriorities(gfx::Rect(layer_bounds), 1.f, 1.0,
                                    Occlusion(), false);
  // The pending tiling should get the new tile size.
  EXPECT_EQ(tile_size2, pending_set->tiling_at(0)->tile_size());
  // The pending tiling should have tiles.
  EXPECT_TRUE(pending_set->tiling_at(0)->has_tiles());
}

TEST(PictureLayerTilingSetTest, MaxContentScale) {
  FakePictureLayerTilingClient pending_client;
  FakePictureLayerTilingClient active_client;
  std::unique_ptr<PictureLayerTilingSet> pending_set =
      PictureLayerTilingSet::Create(PENDING_TREE, &pending_client, 1000, 1.f,
                                    1000, 1000.f);
  std::unique_ptr<PictureLayerTilingSet> active_set =
      PictureLayerTilingSet::Create(ACTIVE_TREE, &active_client, 1000, 1.f,
                                    1000, 1000.f);

  gfx::Size layer_bounds(100, 105);
  scoped_refptr<FakeRasterSource> raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);

  // Tilings can be added of any scale, the tiling client can controls this.
  pending_set->AddTiling(gfx::AxisTransform2d(), raster_source);
  pending_set->AddTiling(gfx::AxisTransform2d(2.f, gfx::Vector2dF()),
                         raster_source);
  pending_set->AddTiling(gfx::AxisTransform2d(3.f, gfx::Vector2dF()),
                         raster_source);

  // Set some expected things for the tiling set to function.
  pending_set->tiling_at(0)->set_resolution(HIGH_RESOLUTION);
  active_client.set_twin_tiling_set(pending_set.get());

  // Update to a new source frame with a max content scale that is larger than
  // everything.
  float max_content_scale = 3.f;
  pending_set->UpdateTilingsToCurrentRasterSourceForCommit(
      raster_source.get(), Region(), 1.f, max_content_scale);

  // All the tilings are there still.
  EXPECT_EQ(3u, pending_set->num_tilings());

  // Clone from the pending to the active tree with the same max content size.
  active_set->UpdateTilingsToCurrentRasterSourceForActivation(
      raster_source.get(), pending_set.get(), Region(), 1.f, max_content_scale);
  // All the tilings are on the active tree.
  EXPECT_EQ(3u, active_set->num_tilings());

  // Update to a new source frame with a max content scale that will drop one
  // tiling.
  max_content_scale = 2.9f;
  pending_set->UpdateTilingsToCurrentRasterSourceForCommit(
      raster_source.get(), Region(), 1.f, max_content_scale);
  // All the tilings are there still.
  EXPECT_EQ(2u, pending_set->num_tilings());

  pending_set->tiling_at(0)->set_resolution(HIGH_RESOLUTION);

  // Clone from the pending to the active tree with the same max content size.
  active_set->UpdateTilingsToCurrentRasterSourceForActivation(
      raster_source.get(), pending_set.get(), Region(), 1.f, max_content_scale);
  // All the tilings are on the active tree.
  EXPECT_EQ(2u, active_set->num_tilings());
}

TEST(PictureLayerTilingSetTest, SkewportLimits) {
  FakePictureLayerTilingClient client;

  gfx::Rect viewport(0, 0, 100, 100);
  gfx::Size layer_bounds(200, 200);

  client.SetTileSize(gfx::Size(100, 100));
  LayerTreeSettings settings;
  settings.skewport_extrapolation_limit_in_screen_pixels = 75;
  settings.tiling_interest_area_padding = 1000000;

  scoped_refptr<FakeRasterSource> raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);
  std::unique_ptr<TestablePictureLayerTilingSet> tiling_set =
      CreateTilingSetWithSettings(&client, settings);

  EXPECT_FALSE(tiling_set->TilingsNeedUpdate(viewport, 1.0));
  tiling_set->AddTiling(gfx::AxisTransform2d(), raster_source);
  EXPECT_TRUE(tiling_set->TilingsNeedUpdate(viewport, 1.0));

  tiling_set->UpdateTilePriorities(viewport, 1.f, 1.0, Occlusion(), true);

  // Move viewport down 50 pixels in 0.5 seconds.
  gfx::Rect down_skewport =
      tiling_set->ComputeSkewport(gfx::Rect(0, 50, 100, 100), 1.5, 1.f);

  EXPECT_EQ(0, down_skewport.x());
  EXPECT_EQ(50, down_skewport.y());
  EXPECT_EQ(100, down_skewport.width());
  EXPECT_EQ(175, down_skewport.height());
  EXPECT_TRUE(down_skewport.Contains(gfx::Rect(0, 50, 100, 100)));

  // Move viewport down 50 and right 10 pixels.
  gfx::Rect down_right_skewport =
      tiling_set->ComputeSkewport(gfx::Rect(10, 50, 100, 100), 1.5, 1.f);

  EXPECT_EQ(10, down_right_skewport.x());
  EXPECT_EQ(50, down_right_skewport.y());
  EXPECT_EQ(120, down_right_skewport.width());
  EXPECT_EQ(175, down_right_skewport.height());
  EXPECT_TRUE(down_right_skewport.Contains(gfx::Rect(10, 50, 100, 100)));

  // Move viewport left.
  gfx::Rect left_skewport =
      tiling_set->ComputeSkewport(gfx::Rect(-50, 0, 100, 100), 1.5, 1.f);

  EXPECT_EQ(-125, left_skewport.x());
  EXPECT_EQ(0, left_skewport.y());
  EXPECT_EQ(175, left_skewport.width());
  EXPECT_EQ(100, left_skewport.height());
  EXPECT_TRUE(left_skewport.Contains(gfx::Rect(-50, 0, 100, 100)));

  // Expand viewport.
  gfx::Rect expand_skewport =
      tiling_set->ComputeSkewport(gfx::Rect(-50, -50, 200, 200), 1.5, 1.f);

  // x and y moved by -75 (-50 - 75 = -125).
  // right side and bottom side moved by 75 [(350 - 125) - (200 - 50) = 75].
  EXPECT_EQ(-125, expand_skewport.x());
  EXPECT_EQ(-125, expand_skewport.y());
  EXPECT_EQ(350, expand_skewport.width());
  EXPECT_EQ(350, expand_skewport.height());
  EXPECT_TRUE(expand_skewport.Contains(gfx::Rect(-50, -50, 200, 200)));

  // Expand the viewport past the limit in all directions.
  gfx::Rect big_expand_skewport =
      tiling_set->ComputeSkewport(gfx::Rect(-500, -500, 1500, 1500), 1.5, 1.f);

  EXPECT_EQ(-575, big_expand_skewport.x());
  EXPECT_EQ(-575, big_expand_skewport.y());
  EXPECT_EQ(1650, big_expand_skewport.width());
  EXPECT_EQ(1650, big_expand_skewport.height());
  EXPECT_TRUE(big_expand_skewport.Contains(gfx::Rect(-500, -500, 1500, 1500)));

  // Shrink the skewport in all directions.
  gfx::Rect shrink_viewport =
      tiling_set->ComputeSkewport(gfx::Rect(0, 0, 100, 100), 1.5, 1.f);
  EXPECT_EQ(0, shrink_viewport.x());
  EXPECT_EQ(0, shrink_viewport.y());
  EXPECT_EQ(100, shrink_viewport.width());
  EXPECT_EQ(100, shrink_viewport.height());

  // Move the skewport really far in one direction.
  gfx::Rect move_skewport_far =
      tiling_set->ComputeSkewport(gfx::Rect(0, 5000, 100, 100), 1.5, 1.f);
  EXPECT_EQ(0, move_skewport_far.x());
  EXPECT_EQ(5000, move_skewport_far.y());
  EXPECT_EQ(100, move_skewport_far.width());
  EXPECT_EQ(175, move_skewport_far.height());
  EXPECT_TRUE(move_skewport_far.Contains(gfx::Rect(0, 5000, 100, 100)));
}

TEST(PictureLayerTilingSetTest, ComputeSkewportExtremeCases) {
  FakePictureLayerTilingClient client;

  gfx::Size layer_bounds(200, 200);
  client.SetTileSize(gfx::Size(100, 100));
  LayerTreeSettings settings;
  settings.tiling_interest_area_padding = 1000000000;
  scoped_refptr<FakeRasterSource> raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);
  std::unique_ptr<TestablePictureLayerTilingSet> tiling_set =
      CreateTilingSetWithSettings(&client, settings);
  tiling_set->AddTiling(gfx::AxisTransform2d(), raster_source);

  gfx::Rect viewport1(-1918, 255860, 4010, 2356);
  gfx::Rect viewport2(-7088, -91738, 14212, 8350);
  gfx::Rect viewport3(-12730024, -158883296, 24607540, 14454512);
  double time = 1.0;
  tiling_set->UpdateTilePriorities(viewport1, 1.f, time, Occlusion(), true);
  time += 0.016;
  EXPECT_TRUE(
      tiling_set->ComputeSkewport(viewport2, time, 1.f).Contains(viewport2));
  tiling_set->UpdateTilePriorities(viewport2, 1.f, time, Occlusion(), true);
  time += 0.016;
  EXPECT_TRUE(
      tiling_set->ComputeSkewport(viewport3, time, 1.f).Contains(viewport3));

  // Use a tiling with a large scale, so the viewport times the scale no longer
  // fits into integers, and the viewport is not anywhere close to the tiling.
  PictureLayerTiling* tiling = tiling_set->AddTiling(
      gfx::AxisTransform2d(1000.f, gfx::Vector2dF()), raster_source);
  EXPECT_TRUE(tiling_set->TilingsNeedUpdate(viewport3, time));
  tiling_set->UpdateTilePriorities(viewport3, 1.f, time, Occlusion(), true);
  EXPECT_TRUE(tiling->GetCurrentVisibleRectForTesting().IsEmpty());
}

TEST(PictureLayerTilingSetTest, ComputeSkewport) {
  FakePictureLayerTilingClient client;

  gfx::Rect viewport(0, 0, 100, 100);
  gfx::Size layer_bounds(200, 200);

  client.SetTileSize(gfx::Size(100, 100));

  scoped_refptr<FakeRasterSource> raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);
  std::unique_ptr<TestablePictureLayerTilingSet> tiling_set =
      CreateTilingSet(&client);
  tiling_set->AddTiling(gfx::AxisTransform2d(), raster_source);

  tiling_set->UpdateTilePriorities(viewport, 1.f, 1.0, Occlusion(), true);

  // Move viewport down 50 pixels in 0.5 seconds.
  gfx::Rect down_skewport =
      tiling_set->ComputeSkewport(gfx::Rect(0, 50, 100, 100), 1.5, 1.f);

  EXPECT_EQ(0, down_skewport.x());
  EXPECT_EQ(50, down_skewport.y());
  EXPECT_EQ(100, down_skewport.width());
  EXPECT_EQ(200, down_skewport.height());

  // Shrink viewport.
  gfx::Rect shrink_skewport =
      tiling_set->ComputeSkewport(gfx::Rect(25, 25, 50, 50), 1.5, 1.f);

  EXPECT_EQ(25, shrink_skewport.x());
  EXPECT_EQ(25, shrink_skewport.y());
  EXPECT_EQ(50, shrink_skewport.width());
  EXPECT_EQ(50, shrink_skewport.height());

  // Move viewport down 50 and right 10 pixels.
  gfx::Rect down_right_skewport =
      tiling_set->ComputeSkewport(gfx::Rect(10, 50, 100, 100), 1.5, 1.f);

  EXPECT_EQ(10, down_right_skewport.x());
  EXPECT_EQ(50, down_right_skewport.y());
  EXPECT_EQ(120, down_right_skewport.width());
  EXPECT_EQ(200, down_right_skewport.height());

  // Move viewport left.
  gfx::Rect left_skewport =
      tiling_set->ComputeSkewport(gfx::Rect(-20, 0, 100, 100), 1.5, 1.f);

  EXPECT_EQ(-60, left_skewport.x());
  EXPECT_EQ(0, left_skewport.y());
  EXPECT_EQ(140, left_skewport.width());
  EXPECT_EQ(100, left_skewport.height());

  // Expand viewport in 0.2 seconds.
  gfx::Rect expanded_skewport =
      tiling_set->ComputeSkewport(gfx::Rect(-5, -5, 110, 110), 1.2, 1.f);

  EXPECT_EQ(-30, expanded_skewport.x());
  EXPECT_EQ(-30, expanded_skewport.y());
  EXPECT_EQ(160, expanded_skewport.width());
  EXPECT_EQ(160, expanded_skewport.height());
}

TEST(PictureLayerTilingSetTest, SkewportThroughUpdateTilePriorities) {
  FakePictureLayerTilingClient client;

  gfx::Rect viewport(0, 0, 100, 100);
  gfx::Size layer_bounds(200, 200);

  client.SetTileSize(gfx::Size(100, 100));

  scoped_refptr<FakeRasterSource> raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);
  std::unique_ptr<TestablePictureLayerTilingSet> tiling_set =
      CreateTilingSet(&client);
  tiling_set->AddTiling(gfx::AxisTransform2d(), raster_source);

  tiling_set->UpdateTilePriorities(viewport, 1.f, 1.0, Occlusion(), true);

  // Move viewport down 50 pixels in 0.5 seconds.
  gfx::Rect viewport_50 = gfx::Rect(0, 50, 100, 100);
  gfx::Rect skewport_50 = tiling_set->ComputeSkewport(viewport_50, 1.5, 1.f);

  EXPECT_EQ(gfx::Rect(0, 50, 100, 200), skewport_50);
  tiling_set->UpdateTilePriorities(viewport_50, 1.f, 1.5, Occlusion(), true);

  gfx::Rect viewport_100 = gfx::Rect(0, 100, 100, 100);
  gfx::Rect skewport_100 = tiling_set->ComputeSkewport(viewport_100, 2.0, 1.f);

  EXPECT_EQ(gfx::Rect(0, 100, 100, 200), skewport_100);
  tiling_set->UpdateTilePriorities(viewport_100, 1.f, 2.0, Occlusion(), true);

  // Advance time, but not the viewport.
  gfx::Rect result = tiling_set->ComputeSkewport(viewport_100, 2.5, 1.f);
  // Since the history did advance, we should still get a skewport but a smaller
  // one.
  EXPECT_EQ(gfx::Rect(0, 100, 100, 150), result);
  tiling_set->UpdateTilePriorities(viewport_100, 1.f, 2.5, Occlusion(), true);

  // Advance time again.
  result = tiling_set->ComputeSkewport(viewport_100, 3.0, 1.f);
  EXPECT_EQ(viewport_100, result);
  tiling_set->UpdateTilePriorities(viewport_100, 1.f, 3.0, Occlusion(), true);

  // Ensure we have a skewport.
  gfx::Rect viewport_150 = gfx::Rect(0, 150, 100, 100);
  gfx::Rect skewport_150 = tiling_set->ComputeSkewport(viewport_150, 3.5, 1.f);
  EXPECT_EQ(gfx::Rect(0, 150, 100, 150), skewport_150);
  tiling_set->UpdateTilePriorities(viewport_150, 1.f, 3.5, Occlusion(), true);

  // Advance the viewport, but not the time.
  gfx::Rect viewport_200 = gfx::Rect(0, 200, 100, 100);
  gfx::Rect skewport_200 = tiling_set->ComputeSkewport(viewport_200, 3.5, 1.f);
  EXPECT_EQ(gfx::Rect(0, 200, 100, 300), skewport_200);

  // Ensure that continued calls with the same value, produce the same skewport.
  tiling_set->UpdateTilePriorities(viewport_150, 1.f, 3.5, Occlusion(), true);
  EXPECT_EQ(gfx::Rect(0, 200, 100, 300), skewport_200);
  tiling_set->UpdateTilePriorities(viewport_150, 1.f, 3.5, Occlusion(), true);
  EXPECT_EQ(gfx::Rect(0, 200, 100, 300), skewport_200);

  tiling_set->UpdateTilePriorities(viewport_200, 1.f, 3.5, Occlusion(), true);

  // This should never happen, but advance the viewport yet again keeping the
  // time the same.
  gfx::Rect viewport_250 = gfx::Rect(0, 250, 100, 100);
  gfx::Rect skewport_250 = tiling_set->ComputeSkewport(viewport_250, 3.5, 1.f);
  EXPECT_EQ(viewport_250, skewport_250);
  tiling_set->UpdateTilePriorities(viewport_250, 1.f, 3.5, Occlusion(), true);
}

TEST(PictureLayerTilingTest, ViewportDistanceWithScale) {
  FakePictureLayerTilingClient client;

  gfx::Rect viewport(0, 0, 100, 100);
  gfx::Size layer_bounds(1500, 1500);

  client.SetTileSize(gfx::Size(10, 10));
  LayerTreeSettings settings;

  // Tiling at 0.25 scale: this should create 47x47 tiles of size 10x10.
  // The reason is that each tile has a one pixel border, so tile at (1, 2)
  // for instance begins at (8, 16) pixels. So tile at (46, 46) will begin at
  // (368, 368) and extend to the end of 1500 * 0.25 = 375 edge of the
  // tiling.
  scoped_refptr<FakeRasterSource> raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);
  std::unique_ptr<TestablePictureLayerTilingSet> tiling_set =
      CreateTilingSet(&client);
  auto* tiling = tiling_set->AddTiling(
      gfx::AxisTransform2d(0.25f, gfx::Vector2dF()), raster_source);
  tiling->set_resolution(HIGH_RESOLUTION);
  gfx::Rect viewport_in_content_space =
      gfx::ScaleToEnclosedRect(viewport, 0.25f);

  tiling_set->UpdateTilePriorities(viewport, 1.f, 1.0, Occlusion(), true);
  auto prioritized_tiles = tiling->UpdateAndGetAllPrioritizedTilesForTesting();

  // Compute the soon border.
  gfx::Rect soon_border_rect_in_content_space =
      tiling_set->ComputeSoonBorderRect(viewport, 1.f);
  soon_border_rect_in_content_space =
      gfx::ScaleToEnclosedRect(soon_border_rect_in_content_space, 0.25f);

  // Sanity checks.
  for (int i = 0; i < 47; ++i) {
    for (int j = 0; j < 47; ++j) {
      EXPECT_TRUE(tiling->TileAt(i, j)) << "i: " << i << " j: " << j;
    }
  }
  for (int i = 0; i < 47; ++i) {
    EXPECT_FALSE(tiling->TileAt(i, 47)) << "i: " << i;
    EXPECT_FALSE(tiling->TileAt(47, i)) << "i: " << i;
  }

  // No movement in the viewport implies that tiles will either be NOW
  // or EVENTUALLY, with the exception of tiles that are between 0 and 312
  // pixels away from the viewport, which will be in the SOON bin.
  bool have_now = false;
  bool have_eventually = false;
  bool have_soon = false;
  for (int i = 0; i < 47; ++i) {
    for (int j = 0; j < 47; ++j) {
      Tile* tile = tiling->TileAt(i, j);
      PrioritizedTile prioritized_tile = prioritized_tiles[tile];
      TilePriority priority = prioritized_tile.priority();

      gfx::Rect tile_rect = tiling->TilingDataForTesting().TileBounds(i, j);
      if (viewport_in_content_space.Intersects(tile_rect)) {
        EXPECT_EQ(TilePriority::NOW, priority.priority_bin);
        EXPECT_FLOAT_EQ(0.f, priority.distance_to_visible);
        have_now = true;
      } else if (soon_border_rect_in_content_space.Intersects(tile_rect)) {
        EXPECT_EQ(TilePriority::SOON, priority.priority_bin);
        have_soon = true;
      } else {
        EXPECT_EQ(TilePriority::EVENTUALLY, priority.priority_bin);
        EXPECT_GT(priority.distance_to_visible, 0.f);
        have_eventually = true;
      }
    }
  }

  EXPECT_TRUE(have_now);
  EXPECT_TRUE(have_soon);
  EXPECT_TRUE(have_eventually);

  // Spot check some distances.
  // Tile at 5, 1 should begin at 41x9 in content space (without borders),
  // so the distance to a viewport that ends at 25x25 in content space
  // should be 17 (41 - 25 + 1). In layer space, then that should be
  // 17 / 0.25 = 68 pixels.

  // We can verify that the content rect (with borders) is one pixel off
  // 41,9 8x8 on all sides.
  EXPECT_EQ(tiling->TileAt(5, 1)->content_rect().ToString(), "40,8 10x10");
  TilePriority priority = prioritized_tiles[tiling->TileAt(5, 1)].priority();
  EXPECT_FLOAT_EQ(68.f, priority.distance_to_visible);

  priority = prioritized_tiles[tiling->TileAt(2, 5)].priority();
  EXPECT_FLOAT_EQ(68.f, priority.distance_to_visible);

  priority = prioritized_tiles[tiling->TileAt(3, 4)].priority();
  EXPECT_FLOAT_EQ(40.f, priority.distance_to_visible);

  // Move the viewport down 40 pixels.
  viewport = gfx::Rect(0, 40, 100, 100);
  viewport_in_content_space = gfx::ScaleToEnclosedRect(viewport, 0.25f);
  gfx::Rect skewport_in_content_space =
      tiling_set->ComputeSkewport(viewport, 2.0, 1.f);
  skewport_in_content_space =
      gfx::ScaleToEnclosedRect(skewport_in_content_space, 0.25f);

  // Compute the soon border.
  soon_border_rect_in_content_space =
      tiling_set->ComputeSoonBorderRect(viewport, 1.f);
  soon_border_rect_in_content_space =
      gfx::ScaleToEnclosedRect(soon_border_rect_in_content_space, 0.25f);

  EXPECT_EQ(0, skewport_in_content_space.x());
  EXPECT_EQ(10, skewport_in_content_space.y());
  EXPECT_EQ(25, skewport_in_content_space.width());
  EXPECT_EQ(35, skewport_in_content_space.height());

  EXPECT_TRUE(tiling_set->TilingsNeedUpdate(viewport, 2.0));
  tiling_set->UpdateTilePriorities(viewport, 1.f, 2.0, Occlusion(), true);
  prioritized_tiles = tiling->UpdateAndGetAllPrioritizedTilesForTesting();

  have_now = false;
  have_eventually = false;
  have_soon = false;

  // Viewport moved, so we expect to find some NOW tiles, some SOON tiles and
  // some EVENTUALLY tiles.
  for (int i = 0; i < 47; ++i) {
    for (int j = 0; j < 47; ++j) {
      Tile* tile = tiling->TileAt(i, j);
      priority = prioritized_tiles[tile].priority();

      gfx::Rect tile_rect = tiling->TilingDataForTesting().TileBounds(i, j);
      if (viewport_in_content_space.Intersects(tile_rect)) {
        EXPECT_EQ(TilePriority::NOW, priority.priority_bin) << "i: " << i
                                                            << " j: " << j;
        EXPECT_FLOAT_EQ(0.f, priority.distance_to_visible) << "i: " << i
                                                           << " j: " << j;
        have_now = true;
      } else if (skewport_in_content_space.Intersects(tile_rect) ||
                 soon_border_rect_in_content_space.Intersects(tile_rect)) {
        EXPECT_EQ(TilePriority::SOON, priority.priority_bin) << "i: " << i
                                                             << " j: " << j;
        EXPECT_GT(priority.distance_to_visible, 0.f) << "i: " << i
                                                     << " j: " << j;
        have_soon = true;
      } else {
        EXPECT_EQ(TilePriority::EVENTUALLY, priority.priority_bin)
            << "i: " << i << " j: " << j;
        EXPECT_GT(priority.distance_to_visible, 0.f) << "i: " << i
                                                     << " j: " << j;
        have_eventually = true;
      }
    }
  }

  EXPECT_TRUE(have_now);
  EXPECT_TRUE(have_soon);
  EXPECT_TRUE(have_eventually);

  priority = prioritized_tiles[tiling->TileAt(5, 1)].priority();
  EXPECT_FLOAT_EQ(68.f, priority.distance_to_visible);

  priority = prioritized_tiles[tiling->TileAt(2, 5)].priority();
  EXPECT_FLOAT_EQ(28.f, priority.distance_to_visible);

  priority = prioritized_tiles[tiling->TileAt(3, 4)].priority();
  EXPECT_FLOAT_EQ(4.f, priority.distance_to_visible);

  // Change the underlying layer scale.
  tiling_set->UpdateTilePriorities(viewport, 2.0f, 3.0, Occlusion(), true);
  prioritized_tiles = tiling->UpdateAndGetAllPrioritizedTilesForTesting();

  priority = prioritized_tiles[tiling->TileAt(5, 1)].priority();
  EXPECT_FLOAT_EQ(136.f, priority.distance_to_visible);

  priority = prioritized_tiles[tiling->TileAt(2, 5)].priority();
  EXPECT_FLOAT_EQ(56.f, priority.distance_to_visible);

  priority = prioritized_tiles[tiling->TileAt(3, 4)].priority();
  EXPECT_FLOAT_EQ(8.f, priority.distance_to_visible);

  // Test additional scales.
  tiling = tiling_set->AddTiling(gfx::AxisTransform2d(0.2f, gfx::Vector2dF()),
                                 raster_source);
  tiling->set_resolution(HIGH_RESOLUTION);
  tiling_set->UpdateTilePriorities(viewport, 1.0f, 4.0, Occlusion(), true);
  prioritized_tiles = tiling->UpdateAndGetAllPrioritizedTilesForTesting();

  priority = prioritized_tiles[tiling->TileAt(5, 1)].priority();
  EXPECT_FLOAT_EQ(110.f, priority.distance_to_visible);

  priority = prioritized_tiles[tiling->TileAt(2, 5)].priority();
  EXPECT_FLOAT_EQ(70.f, priority.distance_to_visible);

  priority = prioritized_tiles[tiling->TileAt(3, 4)].priority();
  EXPECT_FLOAT_EQ(60.f, priority.distance_to_visible);

  tiling_set->UpdateTilePriorities(viewport, 0.5f, 5.0, Occlusion(), true);
  prioritized_tiles = tiling->UpdateAndGetAllPrioritizedTilesForTesting();

  priority = prioritized_tiles[tiling->TileAt(5, 1)].priority();
  EXPECT_FLOAT_EQ(55.f, priority.distance_to_visible);

  priority = prioritized_tiles[tiling->TileAt(2, 5)].priority();
  EXPECT_FLOAT_EQ(35.f, priority.distance_to_visible);

  priority = prioritized_tiles[tiling->TileAt(3, 4)].priority();
  EXPECT_FLOAT_EQ(30.f, priority.distance_to_visible);
}

TEST(PictureLayerTilingTest, InvalidateAfterComputeTilePriorityRects) {
  FakePictureLayerTilingClient pending_client;
  pending_client.SetTileSize(gfx::Size(100, 100));

  scoped_refptr<FakeRasterSource> raster_source =
      FakeRasterSource::CreateFilled(gfx::Size(100, 100));
  std::unique_ptr<TestablePictureLayerTilingSet> tiling_set =
      CreateTilingSet(&pending_client);
  auto* pending_tiling =
      tiling_set->AddTiling(gfx::AxisTransform2d(), raster_source);
  pending_tiling->set_resolution(HIGH_RESOLUTION);

  // Ensure that we can compute tile priority rects, invalidate, and compute the
  // rects again. It is important that the second compute tile priority rects
  // return true, indicating that things have changed (since invalidation has
  // changed things). This causes PrepareTiles to be properly scheduled. If the
  // second ComputeTilePriorityRects returns false, then we assume that
  // PrepareTiles isn't needed and we signal that we're ready to draw
  // immediately, which can cause visual glitches.
  //
  // This can happen we if we process an impl frame deadline before processing a
  // commit. That is, when we draw we ComputeTilePriorityRects. If we process
  // the commit afterwards, it would use the same timestamp and sometimes would
  // use the same viewport to compute tile priority rects again.
  double time = 1.;
  gfx::Rect viewport(0, 0, 100, 100);
  EXPECT_TRUE(
      tiling_set->UpdateTilePriorities(viewport, 1.f, time, Occlusion(), true));
  EXPECT_FALSE(
      tiling_set->UpdateTilePriorities(viewport, 1.f, time, Occlusion(), true));

  // This will invalidate tilings.
  tiling_set->Invalidate(Region());

  EXPECT_TRUE(
      tiling_set->UpdateTilePriorities(viewport, 1.f, time, Occlusion(), true));
}

TEST(PictureLayerTilingTest, InvalidateAfterUpdateRasterSourceForCommit) {
  FakePictureLayerTilingClient pending_client;
  FakePictureLayerTilingClient active_client;
  std::unique_ptr<PictureLayerTilingSet> pending_set =
      PictureLayerTilingSet::Create(PENDING_TREE, &pending_client, 1000, 1.f,
                                    1000, 1000.f);
  std::unique_ptr<PictureLayerTilingSet> active_set =
      PictureLayerTilingSet::Create(ACTIVE_TREE, &active_client, 1000, 1.f,
                                    1000, 1000.f);

  gfx::Size layer_bounds(100, 100);
  scoped_refptr<FakeRasterSource> raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);

  auto* pending_tiling =
      pending_set->AddTiling(gfx::AxisTransform2d(), raster_source);
  pending_tiling->set_resolution(HIGH_RESOLUTION);
  active_client.set_twin_tiling_set(pending_set.get());

  double time = 1.;
  gfx::Rect viewport(0, 0, 100, 100);

  // The first commit will update the raster source for pending tilings.
  pending_set->UpdateTilingsToCurrentRasterSourceForCommit(raster_source,
                                                           Region(), 1.f, 1.f);
  // UpdateTilePriorities for pending set gets called during UDP in commit.
  EXPECT_TRUE(pending_set->UpdateTilePriorities(viewport, 1.f, time,
                                                Occlusion(), true));
  // The active set doesn't have tilings yet.
  EXPECT_FALSE(
      active_set->UpdateTilePriorities(viewport, 1.f, time, Occlusion(), true));

  // On activation tilings are copied from pending set to active set.
  active_set->UpdateTilingsToCurrentRasterSourceForActivation(
      raster_source, pending_set.get(), Region(), 1.f, 1.f);
  // Pending set doesn't have any tilings now.
  EXPECT_FALSE(pending_set->UpdateTilePriorities(viewport, 1.f, time,
                                                 Occlusion(), true));
  // UpdateTilePriorities for active set gets called during UDP in draw.
  EXPECT_TRUE(
      active_set->UpdateTilePriorities(viewport, 1.f, time, Occlusion(), true));

  // Even though frame time and viewport haven't changed since last commit we
  // update tile priorities because of potential invalidations.
  pending_set->UpdateTilingsToCurrentRasterSourceForCommit(raster_source,
                                                           Region(), 1.f, 1.f);
  // UpdateTilePriorities for pending set gets called during UDP in commit.
  EXPECT_TRUE(pending_set->UpdateTilePriorities(viewport, 1.f, time,
                                                Occlusion(), true));
  // No changes for active set until activation.
  EXPECT_FALSE(
      active_set->UpdateTilePriorities(viewport, 1.f, time, Occlusion(), true));
}

TEST(PictureLayerTilingSetTest, TilingTranslationChanges) {
  gfx::Size tile_size(64, 64);
  FakePictureLayerTilingClient pending_client;
  FakePictureLayerTilingClient active_client;
  pending_client.SetTileSize(tile_size);
  active_client.SetTileSize(tile_size);
  std::unique_ptr<PictureLayerTilingSet> pending_set =
      PictureLayerTilingSet::Create(PENDING_TREE, &pending_client, 0, 1.f, 0,
                                    0.f);
  std::unique_ptr<PictureLayerTilingSet> active_set =
      PictureLayerTilingSet::Create(ACTIVE_TREE, &active_client, 0, 1.f, 0,
                                    0.f);
  active_client.set_twin_tiling_set(pending_set.get());

  gfx::Size layer_bounds(100, 100);
  scoped_refptr<FakeRasterSource> raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);

  gfx::AxisTransform2d raster_transform1(1.f, gfx::Vector2dF(0.25f, 0.25f));
  pending_set->AddTiling(raster_transform1, raster_source);
  pending_set->tiling_at(0)->set_resolution(HIGH_RESOLUTION);

  // Set a priority rect so we get tiles.
  pending_set->UpdateTilePriorities(gfx::Rect(layer_bounds), 1.f, 1.0,
                                    Occlusion(), false);

  // Make sure all tiles are generated.
  EXPECT_EQ(4u, pending_set->tiling_at(0)->AllTilesForTesting().size());

  // Clone from the pending to the active tree.
  active_set->UpdateTilingsToCurrentRasterSourceForActivation(
      raster_source.get(), pending_set.get(), Region(), 1.f, 1.f);

  // Verifies active tree cloned the tiling correctly.
  ASSERT_EQ(1u, active_set->num_tilings());
  EXPECT_EQ(active_set->tiling_at(0)->raster_transform(), raster_transform1);
  EXPECT_EQ(4u, active_set->tiling_at(0)->AllTilesForTesting().size());

  // Change raster translation on the pending set.
  gfx::AxisTransform2d raster_transform2(1.f, gfx::Vector2dF(0.75f, 0.75f));
  pending_set->RemoveAllTilings();
  pending_set->AddTiling(raster_transform2, raster_source);
  pending_set->tiling_at(0)->set_resolution(HIGH_RESOLUTION);

  // Set a different priority rect to get one tile.
  pending_set->UpdateTilePriorities(gfx::Rect(1, 1), 1.f, 1.0, Occlusion(),
                                    false);
  EXPECT_EQ(1u, pending_set->tiling_at(0)->AllTilesForTesting().size());

  // Commit the pending to the active tree again.
  active_set->UpdateTilingsToCurrentRasterSourceForActivation(
      raster_source.get(), pending_set.get(), Region(), 1.f, 1.f);

  // Verifies the old tiling with a different translation is dropped.
  ASSERT_EQ(1u, active_set->num_tilings());
  EXPECT_EQ(active_set->tiling_at(0)->raster_transform(), raster_transform2);
  EXPECT_EQ(1u, active_set->tiling_at(0)->AllTilesForTesting().size());
}

TEST(PictureLayerTilingSetTest, LcdChanges) {
  gfx::Size tile_size(64, 64);
  FakePictureLayerTilingClient pending_client;
  FakePictureLayerTilingClient active_client;
  pending_client.SetTileSize(tile_size);
  active_client.SetTileSize(tile_size);
  std::unique_ptr<PictureLayerTilingSet> pending_set =
      PictureLayerTilingSet::Create(PENDING_TREE, &pending_client, 0, 1.f, 0,
                                    0.f);
  std::unique_ptr<PictureLayerTilingSet> active_set =
      PictureLayerTilingSet::Create(ACTIVE_TREE, &active_client, 0, 1.f, 0,
                                    0.f);
  active_client.set_twin_tiling_set(pending_set.get());
  pending_client.set_twin_tiling_set(active_set.get());

  gfx::Size layer_bounds(100, 100);
  scoped_refptr<FakeRasterSource> raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);

  const bool with_lcd_text = true;
  const bool without_lcd_text = false;

  gfx::AxisTransform2d raster_transform(1.f, gfx::Vector2dF());
  pending_set->AddTiling(raster_transform, raster_source, with_lcd_text);
  pending_set->tiling_at(0)->set_resolution(HIGH_RESOLUTION);

  // Set a priority rect so we get tiles.
  pending_set->UpdateTilePriorities(gfx::Rect(layer_bounds), 1.f, 1.0,
                                    Occlusion(), false);

  // Make sure all tiles are generated.
  EXPECT_EQ(4u, pending_set->tiling_at(0)->AllTilesForTesting().size());

  // Clone from the pending to the active tree.
  active_set->UpdateTilingsToCurrentRasterSourceForActivation(
      raster_source.get(), pending_set.get(), Region(), 1.f, 1.f);

  // Verifies active tree cloned the tiling correctly.
  ASSERT_EQ(1u, active_set->num_tilings());
  EXPECT_EQ(4u, active_set->tiling_at(0)->AllTilesForTesting().size());

  // Change LCD state on the pending tree
  pending_set->RemoveAllTilings();
  pending_set->AddTiling(raster_transform, raster_source, without_lcd_text);
  pending_set->tiling_at(0)->set_resolution(HIGH_RESOLUTION);

  // Set a priority rect so we get tiles.
  pending_set->UpdateTilePriorities(gfx::Rect(layer_bounds), 1.f, 1.0,
                                    Occlusion(), false);
  // We should have created all tiles because lcd state changed.
  EXPECT_EQ(4u, pending_set->tiling_at(0)->AllTilesForTesting().size());
}

}  // namespace
}  // namespace cc
