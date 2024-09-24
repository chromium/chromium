// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "cc/layers/picture_layer_impl.h"

#include <stddef.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <set>
#include <utility>

#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "cc/animation/animation_host.h"
#include "cc/base/features.h"
#include "cc/base/math_util.h"
#include "cc/layers/append_quads_data.h"
#include "cc/layers/picture_layer.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/fake_impl_task_runner_provider.h"
#include "cc/test/fake_layer_tree_frame_sink.h"
#include "cc/test/fake_layer_tree_host.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_picture_layer_impl.h"
#include "cc/test/fake_raster_source.h"
#include "cc/test/fake_recording_source.h"
#include "cc/test/layer_tree_impl_test_base.h"
#include "cc/test/skia_common.h"
#include "cc/test/test_layer_tree_host_base.h"
#include "cc/test/test_paint_worklet_input.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/tiles/tiling_set_raster_queue_all.h"
#include "cc/tiles/tiling_set_raster_queue_required.h"
#include "cc/trees/layer_tree_impl.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/quads/tile_draw_quad.h"
#include "components/viz/test/begin_frame_args_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/test/geometry_util.h"

namespace cc {
namespace {

#define EXPECT_BOTH_EQ(expression, x)          \
  do {                                         \
    EXPECT_EQ(x, pending_layer()->expression); \
    EXPECT_EQ(x, active_layer()->expression);  \
  } while (false)

#define EXPECT_BOTH_NE(expression, x)          \
  do {                                         \
    EXPECT_NE(x, pending_layer()->expression); \
    EXPECT_NE(x, active_layer()->expression);  \
  } while (false)

#define EXPECT_BOTH_TRUE(expression)          \
  do {                                        \
    EXPECT_TRUE(pending_layer()->expression); \
    EXPECT_TRUE(active_layer()->expression);  \
  } while (false)

#define EXPECT_BOTH_FALSE(expression)          \
  do {                                         \
    EXPECT_FALSE(pending_layer()->expression); \
    EXPECT_FALSE(active_layer()->expression);  \
  } while (false)

class PictureLayerImplTest : public TestLayerTreeHostBase {
 public:
  void SetUp() override {
    TestLayerTreeHostBase::SetUp();
    host_impl()->active_tree()->SetDeviceViewportRect(gfx::Rect(10000, 10000));
  }

  LayerTreeSettings CreateSettings() override {
    auto settings = TestLayerTreeHostBase::CreateSettings();
    settings.commit_to_active_tree = false;
    settings.create_low_res_tiling = true;
    return settings;
  }

  std::unique_ptr<LayerTreeFrameSink> CreateLayerTreeFrameSink() override {
    return FakeLayerTreeFrameSink::Create3dForGpuRasterization();
  }

  void SetupDefaultTreesWithFixedTileSize(const gfx::Size& layer_bounds,
                                          const gfx::Size& tile_size,
                                          const Region& invalidation) {
    scoped_refptr<FakeRasterSource> pending_raster_source =
        FakeRasterSource::CreateFilled(layer_bounds);
    scoped_refptr<FakeRasterSource> active_raster_source =
        FakeRasterSource::CreateFilled(layer_bounds);

    SetupTreesWithFixedTileSize(std::move(pending_raster_source),
                                std::move(active_raster_source), tile_size,
                                invalidation);
  }

  void SetupTreesWithFixedTileSize(
      scoped_refptr<RasterSource> pending_raster_source,
      scoped_refptr<RasterSource> active_raster_source,
      const gfx::Size& tile_size,
      const Region& pending_invalidation) {
    SetupPendingTree(std::move(active_raster_source), tile_size, Region());
    ActivateTree();
    SetupPendingTree(std::move(pending_raster_source), tile_size,
                     pending_invalidation);
  }

  void SetupDefaultTreesWithInvalidation(const gfx::Size& layer_bounds,
                                         const Region& invalidation) {
    scoped_refptr<FakeRasterSource> pending_raster_source =
        FakeRasterSource::CreateFilled(layer_bounds);
    scoped_refptr<FakeRasterSource> active_raster_source =
        FakeRasterSource::CreateFilled(layer_bounds);

    SetupTreesWithInvalidation(std::move(pending_raster_source),
                               std::move(active_raster_source), invalidation);
  }

  void SetupTreesWithInvalidation(
      scoped_refptr<RasterSource> pending_raster_source,
      scoped_refptr<RasterSource> active_raster_source,
      const Region& pending_invalidation) {
    SetupPendingTree(std::move(active_raster_source), gfx::Size(), Region());
    ActivateTree();
    SetupPendingTree(std::move(pending_raster_source), gfx::Size(),
                     pending_invalidation);
  }

  void SetupPendingTreeWithInvalidation(
      scoped_refptr<RasterSource> raster_source,
      const Region& invalidation) {
    SetupPendingTree(std::move(raster_source), gfx::Size(), invalidation);
  }

  void SetupPendingTreeWithFixedTileSize(
      scoped_refptr<RasterSource> raster_source,
      const gfx::Size& tile_size,
      const Region& invalidation) {
    SetupPendingTree(std::move(raster_source), tile_size, invalidation);
  }

  void SetupDrawProperties(FakePictureLayerImpl* layer,
                           const gfx::Vector2dF& ideal_contents_scale,
                           float device_scale_factor,
                           float page_scale_factor) {
    layer->layer_tree_impl()->SetDeviceScaleFactor(device_scale_factor);
    host_impl()->active_tree()->SetPageScaleOnActiveTree(page_scale_factor);

    gfx::Transform scale_transform;
    scale_transform.Scale(ideal_contents_scale.x(), ideal_contents_scale.y());
    layer->draw_properties().screen_space_transform = scale_transform;
    layer->draw_properties().target_space_transform = scale_transform;
    layer->set_contributes_to_drawn_render_surface(true);
    DCHECK_EQ(layer->GetIdealContentsScale(), ideal_contents_scale);
  }

  void SetupDrawProperties(FakePictureLayerImpl* layer,
                           float ideal_contents_scale,
                           float device_scale_factor,
                           float page_scale_factor) {
    SetupDrawProperties(
        layer, gfx::Vector2dF(ideal_contents_scale, ideal_contents_scale),
        device_scale_factor, page_scale_factor);
  }

  void SetupDrawPropertiesAndUpdateTiles(
      FakePictureLayerImpl* layer,
      const gfx::Vector2dF& ideal_contents_scale,
      float device_scale_factor,
      float page_scale_factor) {
    SetupDrawProperties(layer, ideal_contents_scale, device_scale_factor,
                        page_scale_factor);
    layer->UpdateTiles();
  }

  void SetupDrawPropertiesAndUpdateTiles(FakePictureLayerImpl* layer,
                                         float ideal_contents_scale,
                                         float device_scale_factor,
                                         float page_scale_factor) {
    SetupDrawPropertiesAndUpdateTiles(
        layer, gfx::Vector2dF(ideal_contents_scale, ideal_contents_scale),
        device_scale_factor, page_scale_factor);
  }

  static void VerifyAllPrioritizedTilesExistAndHaveRasterSource(
      const PictureLayerTiling* tiling,
      RasterSource* raster_source) {
    auto prioritized_tiles =
        tiling->UpdateAndGetAllPrioritizedTilesForTesting();
    for (PictureLayerTiling::CoverageIterator iter(
             tiling, tiling->contents_scale_key(), tiling->tiling_rect());
         iter; ++iter) {
      EXPECT_TRUE(*iter);
      EXPECT_EQ(raster_source, prioritized_tiles[*iter].raster_source());
    }
  }

  void SetMaximumAnimationToScreenScale(FakePictureLayerImpl* layer,
                                        float maximum_animation_to_screen_scale,
                                        bool affected_by_invalid_scale) {
    layer->layer_tree_impl()
        ->property_trees()
        ->SetMaximumAnimationToScreenScaleForTesting(
            layer->transform_tree_index(), maximum_animation_to_screen_scale,
            affected_by_invalid_scale);
    layer->draw_properties().screen_space_transform_is_animating = true;
  }

  void SetContentsScaleOnPendingLayer(float contents_scale,
                                      float device_scale_factor,
                                      float page_scale_factor) {
    // Sets arbitrary animation scale to ensure it's not used in any way.
    constexpr float kArbitraryScale = 12345.0f;
    SetMaximumAnimationToScreenScale(pending_layer(), kArbitraryScale, false);
    pending_layer()->draw_properties().screen_space_transform_is_animating =
        false;
    SetupDrawPropertiesAndUpdateTiles(pending_layer(), contents_scale,
                                      device_scale_factor, page_scale_factor);
  }

  void SetContentsScaleOnActiveLayer(float contents_scale,
                                     float device_scale_factor,
                                     float page_scale_factor) {
    // Sets arbitrary animation scale to ensure it's not used in any way.
    constexpr float kArbitraryScale = 34567.0f;
    SetMaximumAnimationToScreenScale(active_layer(), kArbitraryScale, false);
    active_layer()->draw_properties().screen_space_transform_is_animating =
        false;
    SetupDrawPropertiesAndUpdateTiles(active_layer(), contents_scale,
                                      device_scale_factor, page_scale_factor);
  }

  void SetContentsScaleOnBothLayers(float contents_scale,
                                    float device_scale_factor,
                                    float page_scale_factor) {
    SetContentsScaleOnPendingLayer(contents_scale, device_scale_factor,
                                   page_scale_factor);
    SetContentsScaleOnActiveLayer(contents_scale, device_scale_factor,
                                  page_scale_factor);
  }

  void SetContentsAndAnimationScalesOnBothLayers(
      float contents_scale,
      float device_scale_factor,
      float page_scale_factor,
      float maximum_animation_scale,
      bool affected_by_invalid_scale) {
    SetMaximumAnimationToScreenScale(pending_layer(), maximum_animation_scale,
                                     affected_by_invalid_scale);
    SetupDrawPropertiesAndUpdateTiles(pending_layer(), contents_scale,
                                      device_scale_factor, page_scale_factor);
    SetMaximumAnimationToScreenScale(active_layer(), maximum_animation_scale,
                                     affected_by_invalid_scale);
    SetupDrawPropertiesAndUpdateTiles(active_layer(), contents_scale,
                                      device_scale_factor, page_scale_factor);
  }

  void ResetTilingsAndRasterScales() {
    if (pending_layer()) {
      pending_layer()->ReleaseTileResources();
      EXPECT_TRUE(pending_layer()->tilings());
      EXPECT_EQ(0u, pending_layer()->num_tilings());
      pending_layer()->RecreateTileResources();
      EXPECT_EQ(0u, pending_layer()->num_tilings());
    }

    if (active_layer()) {
      active_layer()->ReleaseTileResources();
      EXPECT_TRUE(active_layer()->tilings());
      EXPECT_EQ(0u, pending_layer()->num_tilings());
      active_layer()->RecreateTileResources();
      EXPECT_EQ(0u, active_layer()->tilings()->num_tilings());
    }
  }

  size_t NumberOfTilesRequired(PictureLayerTiling* tiling) {
    size_t num_required = 0;
    std::vector<Tile*> tiles = tiling->AllTilesForTesting();
    for (size_t i = 0; i < tiles.size(); ++i) {
      if (tiles[i]->required_for_activation())
        num_required++;
    }
    return num_required;
  }

  void AssertAllTilesRequired(PictureLayerTiling* tiling) {
    std::vector<Tile*> tiles = tiling->AllTilesForTesting();
    for (size_t i = 0; i < tiles.size(); ++i)
      EXPECT_TRUE(tiles[i]->required_for_activation()) << "i: " << i;
    EXPECT_GT(tiles.size(), 0u);
  }

  void AssertNoTilesRequired(PictureLayerTiling* tiling) {
    std::vector<Tile*> tiles = tiling->AllTilesForTesting();
    for (size_t i = 0; i < tiles.size(); ++i)
      EXPECT_FALSE(tiles[i]->required_for_activation()) << "i: " << i;
    EXPECT_GT(tiles.size(), 0u);
  }

  void SetInitialDeviceScaleFactor(float device_scale_factor) {
    // Device scale factor is a per-tree property. However, tests can't directly
    // set the pending tree's device scale factor before the pending tree is
    // created, and setting it after SetupPendingTree is too late, since
    // draw properties will already have been updated on the tree. To handle
    // this, we initially set only the active tree's device scale factor, and we
    // copy this over to the pending tree inside SetupPendingTree.
    host_impl()->active_tree()->SetDeviceScaleFactor(device_scale_factor);
  }

  void TestQuadsForSolidColor(bool test_for_solid, bool partial_opaque);
};

// Legacy PictureLayerImplTest which forces SW rasterization. New tests should
// default to the more common GPU rasterization path.
class LegacySWPictureLayerImplTest : public PictureLayerImplTest {
 public:
  LayerTreeSettings CreateSettings() override {
    auto settings = PictureLayerImplTest::CreateSettings();
    settings.gpu_rasterization_disabled = true;
    return settings;
  }
};

class CommitToActiveTreePictureLayerImplTest : public PictureLayerImplTest {
 public:
  LayerTreeSettings CreateSettings() override {
    LayerTreeSettings settings = PictureLayerImplTest::CreateSettings();
    settings.commit_to_active_tree = true;
    return settings;
  }
};

class NoLowResPictureLayerImplTest : public LegacySWPictureLayerImplTest {
 public:
  LayerTreeSettings CreateSettings() override {
    LayerTreeSettings settings = LegacySWPictureLayerImplTest::CreateSettings();
    settings.create_low_res_tiling = false;
    return settings;
  }
};

TEST_F(LegacySWPictureLayerImplTest, CloneNoInvalidation) {
  gfx::Size layer_bounds(400, 400);
  SetupDefaultTrees(layer_bounds);

  EXPECT_EQ(pending_layer()->tilings()->num_tilings(),
            active_layer()->tilings()->num_tilings());

  const PictureLayerTilingSet* tilings = pending_layer()->tilings();
  EXPECT_GT(tilings->num_tilings(), 0u);
  for (size_t i = 0; i < tilings->num_tilings(); ++i)
    EXPECT_TRUE(tilings->tiling_at(i)->AllTilesForTesting().empty());
}

TEST_F(LegacySWPictureLayerImplTest, ExternalViewportRectForPrioritizingTiles) {
  host_impl()->AdvanceToNextFrame(base::Milliseconds(1));
  gfx::Size layer_bounds(400, 400);
  SetupDefaultTrees(layer_bounds);

  SetupDrawPropertiesAndUpdateTiles(active_layer(), 1.f, 1.f, 1.f);

  host_impl()->AdvanceToNextFrame(base::Milliseconds(200));

  // Update tiles with viewport for tile priority as (0, 0, 100, 100) and the
  // identify transform for tile priority.
  gfx::Rect viewport_rect_for_tile_priority = gfx::Rect(0, 0, 100, 100);
  gfx::Transform transform_for_tile_priority;

  host_impl()->SetExternalTilePriorityConstraints(
      viewport_rect_for_tile_priority, transform_for_tile_priority);
  UpdateDrawProperties(host_impl()->active_tree());

  // Verify the viewport rect for tile priority is used in picture layer tiling.
  EXPECT_EQ(viewport_rect_for_tile_priority,
            active_layer()->viewport_rect_for_tile_priority_in_content_space());
  PictureLayerTilingSet* tilings = active_layer()->tilings();
  for (size_t i = 0; i < tilings->num_tilings(); i++) {
    PictureLayerTiling* tiling = tilings->tiling_at(i);
    EXPECT_EQ(tiling->GetCurrentVisibleRectForTesting(),
              gfx::ScaleToEnclosingRect(viewport_rect_for_tile_priority,
                                        tiling->contents_scale_key()));
  }

  // Update tiles with viewport for tile priority as (200, 200, 100, 100) in
  // root layer space and the transform for tile priority is translated and
  // rotated.
  host_impl()->AdvanceToNextFrame(base::Milliseconds(200));

  viewport_rect_for_tile_priority = gfx::Rect(200, 200, 100, 100);
  transform_for_tile_priority.Translate(100, 100);
  transform_for_tile_priority.Rotate(45);
  host_impl()->SetExternalTilePriorityConstraints(
      viewport_rect_for_tile_priority, transform_for_tile_priority);
  UpdateDrawProperties(host_impl()->active_tree());

  EXPECT_EQ(viewport_rect_for_tile_priority,
            active_layer()->viewport_rect_for_tile_priority_in_content_space());
  tilings = active_layer()->tilings();
  for (size_t i = 0; i < tilings->num_tilings(); i++) {
    PictureLayerTiling* tiling = tilings->tiling_at(i);
    EXPECT_EQ(tiling->GetCurrentVisibleRectForTesting(),
              gfx::ScaleToEnclosingRect(viewport_rect_for_tile_priority,
                                        tiling->contents_scale_key()));
  }
}

TEST_F(LegacySWPictureLayerImplTest, ViewportRectForTilePriorityIsCached) {
  host_impl()->AdvanceToNextFrame(base::Milliseconds(1));
  gfx::Size layer_bounds(400, 400);
  SetupDefaultTrees(layer_bounds);

  SetupDrawPropertiesAndUpdateTiles(active_layer(), 1.f, 1.f, 1.f);

  host_impl()->AdvanceToNextFrame(base::Milliseconds(200));

  gfx::Rect viewport_rect_for_tile_priority(0, 0, 100, 100);
  gfx::Transform transform_for_tile_priority;

  host_impl()->SetExternalTilePriorityConstraints(
      viewport_rect_for_tile_priority, transform_for_tile_priority);
  UpdateDrawProperties(host_impl()->active_tree());

  EXPECT_EQ(viewport_rect_for_tile_priority,
            active_layer()->viewport_rect_for_tile_priority_in_content_space());

  host_impl()->AdvanceToNextFrame(base::Milliseconds(200));

  gfx::Rect another_viewport_rect_for_tile_priority(11, 11, 50, 50);
  host_impl()->SetExternalTilePriorityConstraints(
      another_viewport_rect_for_tile_priority, transform_for_tile_priority);

  // Didn't call UpdateDrawProperties yet. The viewport rect for tile priority
  // should remain to be the previously cached value.
  EXPECT_EQ(viewport_rect_for_tile_priority,
            active_layer()->viewport_rect_for_tile_priority_in_content_space());
  UpdateDrawProperties(host_impl()->active_tree());

  // Now the UpdateDrawProperties is called. The viewport rect for tile
  // priority should be the latest value.
  EXPECT_EQ(another_viewport_rect_for_tile_priority,
            active_layer()->viewport_rect_for_tile_priority_in_content_space());
}

TEST_F(LegacySWPictureLayerImplTest, ClonePartialInvalidation) {
  gfx::Size layer_bounds(400, 400);
  gfx::Rect layer_invalidation(150, 200, 30, 180);

  scoped_refptr<FakeRasterSource> pending_raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);
  scoped_refptr<FakeRasterSource> active_raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);
  scoped_refptr<FakeRasterSource> lost_raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);

  SetupPendingTreeWithFixedTileSize(lost_raster_source, gfx::Size(50, 50),
                                    Region());
  ActivateTree();
  {
    // Add a unique tiling on the active tree.
    PictureLayerTiling* tiling =
        active_layer()->AddTiling(gfx::AxisTransform2d(3.f, gfx::Vector2dF()));
    tiling->set_resolution(HIGH_RESOLUTION);
    tiling->CreateAllTilesForTesting();
  }

  // Ensure UpdateTiles won't remove any tilings.
  active_layer()->MarkAllTilingsUsed();

  // Then setup a new pending tree and activate it.
  SetupTreesWithFixedTileSize(pending_raster_source, active_raster_source,
                              gfx::Size(50, 50), layer_invalidation);

  EXPECT_EQ(1u, pending_layer()->num_tilings());
  EXPECT_EQ(3u, active_layer()->num_tilings());

  const PictureLayerTilingSet* tilings = pending_layer()->tilings();
  EXPECT_GT(tilings->num_tilings(), 0u);
  for (size_t i = 0; i < tilings->num_tilings(); ++i) {
    const PictureLayerTiling* tiling = tilings->tiling_at(i);
    gfx::Rect content_invalidation = gfx::ScaleToEnclosingRect(
        layer_invalidation, tiling->contents_scale_key());
    auto prioritized_tiles =
        tiling->UpdateAndGetAllPrioritizedTilesForTesting();
    for (PictureLayerTiling::CoverageIterator iter(
             tiling, tiling->contents_scale_key(), tiling->tiling_rect());
         iter; ++iter) {
      // We don't always have a tile, but when we do it's because it was
      // invalidated and it has the latest raster source.
      if (*iter) {
        EXPECT_FALSE(iter.geometry_rect().IsEmpty());
        EXPECT_EQ(pending_raster_source.get(),
                  prioritized_tiles[*iter].raster_source());
        EXPECT_TRUE(iter.geometry_rect().Intersects(content_invalidation));
      } else {
        // We don't create tiles in non-invalidated regions.
        EXPECT_FALSE(iter.geometry_rect().Intersects(content_invalidation));
      }
    }
  }

  tilings = active_layer()->tilings();
  EXPECT_GT(tilings->num_tilings(), 0u);
  for (size_t i = 0; i < tilings->num_tilings(); ++i) {
    const PictureLayerTiling* tiling = tilings->tiling_at(i);
    auto prioritized_tiles =
        tiling->UpdateAndGetAllPrioritizedTilesForTesting();
    for (PictureLayerTiling::CoverageIterator iter(
             tiling, tiling->contents_scale_key(), tiling->tiling_rect());
         iter; ++iter) {
      EXPECT_TRUE(*iter);
      EXPECT_FALSE(iter.geometry_rect().IsEmpty());
      // Raster source will be updated upon activation.
      EXPECT_EQ(active_raster_source.get(),
                prioritized_tiles[*iter].raster_source());
    }
  }
}

TEST_F(LegacySWPictureLayerImplTest, CloneFullInvalidation) {
  gfx::Size layer_bounds(300, 500);

  scoped_refptr<FakeRasterSource> pending_raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);
  scoped_refptr<FakeRasterSource> active_raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);

  SetupTreesWithInvalidation(pending_raster_source, active_raster_source,
                             gfx::Rect(layer_bounds));

  EXPECT_EQ(pending_layer()->tilings()->num_tilings(),
            active_layer()->tilings()->num_tilings());

  const PictureLayerTilingSet* tilings = pending_layer()->tilings();
  EXPECT_GT(tilings->num_tilings(), 0u);
  for (size_t i = 0; i < tilings->num_tilings(); ++i) {
    VerifyAllPrioritizedTilesExistAndHaveRasterSource(
        tilings->tiling_at(i), pending_raster_source.get());
  }
}

TEST_F(LegacySWPictureLayerImplTest, UpdateTilesCreatesTilings) {
  gfx::Size layer_bounds(1300, 1900);
  SetupDefaultTrees(layer_bounds);

  float low_res_factor = host_impl()->settings().low_res_contents_scale_factor;
  EXPECT_LT(low_res_factor, 1.f);

  active_layer()->ReleaseTileResources();
  EXPECT_TRUE(active_layer()->tilings());
  EXPECT_EQ(0u, active_layer()->num_tilings());
  active_layer()->RecreateTileResources();
  EXPECT_EQ(0u, active_layer()->num_tilings());

  SetupDrawPropertiesAndUpdateTiles(active_layer(),
                                    6.f,   // ideal contents scale
                                    3.f,   // device scale
                                    2.f);  // page scale
  ASSERT_EQ(2u, active_layer()->tilings()->num_tilings());
  EXPECT_FLOAT_EQ(
      6.f, active_layer()->tilings()->tiling_at(0)->contents_scale_key());
  EXPECT_FLOAT_EQ(
      6.f * low_res_factor,
      active_layer()->tilings()->tiling_at(1)->contents_scale_key());

  // If we change the page scale factor, then we should get new tilings.
  SetupDrawPropertiesAndUpdateTiles(active_layer(),
                                    6.6f,   // ideal contents scale
                                    3.f,    // device scale
                                    2.2f);  // page scale
  ASSERT_EQ(4u, active_layer()->tilings()->num_tilings());
  EXPECT_FLOAT_EQ(
      6.6f, active_layer()->tilings()->tiling_at(0)->contents_scale_key());
  EXPECT_FLOAT_EQ(
      6.6f * low_res_factor,
      active_layer()->tilings()->tiling_at(2)->contents_scale_key());

  // If we change the device scale factor, then we should get new tilings.
  SetupDrawPropertiesAndUpdateTiles(active_layer(),
                                    7.26f,  // ideal contents scale
                                    3.3f,   // device scale
                                    2.2f);  // page scale
  ASSERT_EQ(6u, active_layer()->tilings()->num_tilings());
  EXPECT_FLOAT_EQ(
      7.26f, active_layer()->tilings()->tiling_at(0)->contents_scale_key());
  EXPECT_FLOAT_EQ(
      7.26f * low_res_factor,
      active_layer()->tilings()->tiling_at(3)->contents_scale_key());

  // If we change the device scale factor, but end up at the same total scale
  // factor somehow, then we don't get new tilings.
  SetupDrawPropertiesAndUpdateTiles(active_layer(),
                                    7.26f,  // ideal contents scale
                                    2.2f,   // device scale
                                    3.3f);  // page scale
  ASSERT_EQ(6u, active_layer()->tilings()->num_tilings());
  EXPECT_FLOAT_EQ(
      7.26f, active_layer()->tilings()->tiling_at(0)->contents_scale_key());
  EXPECT_FLOAT_EQ(
      7.26f * low_res_factor,
      active_layer()->tilings()->tiling_at(3)->contents_scale_key());
}

TEST_F(LegacySWPictureLayerImplTest, PendingLayerOnlyHasHighResTiling) {
  gfx::Size layer_bounds(1300, 1900);
  SetupDefaultTrees(layer_bounds);

  float low_res_factor = host_impl()->settings().low_res_contents_scale_factor;
  EXPECT_LT(low_res_factor, 1.f);

  pending_layer()->ReleaseTileResources();
  pending_layer()->RecreateTileResources();
  EXPECT_EQ(0u, pending_layer()->tilings()->num_tilings());

  SetupDrawPropertiesAndUpdateTiles(pending_layer(),
                                    6.f,   // ideal contents scale
                                    3.f,   // device scale
                                    2.f);  // page scale
  ASSERT_EQ(1u, pending_layer()->tilings()->num_tilings());
  EXPECT_FLOAT_EQ(
      6.f, pending_layer()->tilings()->tiling_at(0)->contents_scale_key());

  // If we change the page scale factor, then we should get new tilings.
  SetupDrawPropertiesAndUpdateTiles(pending_layer(),
                                    6.6f,   // ideal contents scale
                                    3.f,    // device scale
                                    2.2f);  // page scale
  ASSERT_EQ(1u, pending_layer()->tilings()->num_tilings());
  EXPECT_FLOAT_EQ(
      6.6f, pending_layer()->tilings()->tiling_at(0)->contents_scale_key());

  // If we change the device scale factor, then we should get new tilings.
  SetupDrawPropertiesAndUpdateTiles(pending_layer(),
                                    7.26f,  // ideal contents scale
                                    3.3f,   // device scale
                                    2.2f);  // page scale
  ASSERT_EQ(1u, pending_layer()->tilings()->num_tilings());
  EXPECT_FLOAT_EQ(
      7.26f, pending_layer()->tilings()->tiling_at(0)->contents_scale_key());

  // If we change the device scale factor, but end up at the same total scale
  // factor somehow, then we don't get new tilings.
  SetupDrawPropertiesAndUpdateTiles(pending_layer(),
                                    7.26f,  // ideal contents scale
                                    2.2f,   // device scale
                                    3.3f);  // page scale
  ASSERT_EQ(1u, pending_layer()->tilings()->num_tilings());
  EXPECT_FLOAT_EQ(
      7.26f, pending_layer()->tilings()->tiling_at(0)->contents_scale_key());
}

TEST_F(LegacySWPictureLayerImplTest, CreateTilingsEvenIfTwinHasNone) {
  // This test makes sure that if a layer can have tilings, then a commit makes
  // it not able to have tilings (empty size), and then a future commit that
  // makes it valid again should be able to create tilings.
  gfx::Size layer_bounds(1300, 1900);

  scoped_refptr<FakeRasterSource> empty_raster_source =
      FakeRasterSource::CreateEmpty(layer_bounds);
  scoped_refptr<FakeRasterSource> valid_raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);

  SetupPendingTree(valid_raster_source);
  ASSERT_EQ(1u, pending_layer()->tilings()->num_tilings());

  ActivateTree();
  SetupPendingTree(empty_raster_source);
  EXPECT_FALSE(pending_layer()->CanHaveTilings());
  ASSERT_EQ(2u, active_layer()->tilings()->num_tilings());
  ASSERT_EQ(0u, pending_layer()->tilings()->num_tilings());

  ActivateTree();
  EXPECT_FALSE(active_layer()->CanHaveTilings());
  ASSERT_EQ(0u, active_layer()->tilings()->num_tilings());

  SetupPendingTree(valid_raster_source);
  ASSERT_EQ(1u, pending_layer()->tilings()->num_tilings());
  ASSERT_EQ(0u, active_layer()->tilings()->num_tilings());
}

TEST_F(LegacySWPictureLayerImplTest, LowResTilingStaysOnActiveTree) {
  gfx::Size layer_bounds(1300, 1900);

  scoped_refptr<FakeRasterSource> valid_raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);
  scoped_refptr<FakeRasterSource> other_valid_raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);

  SetupPendingTree(valid_raster_source);
  ASSERT_EQ(1u, pending_layer()->tilings()->num_tilings());

  ActivateTree();
  SetupPendingTree(other_valid_raster_source);
  ASSERT_EQ(2u, active_layer()->tilings()->num_tilings());
  ASSERT_EQ(1u, pending_layer()->tilings()->num_tilings());
  auto* low_res_tiling =
      active_layer()->tilings()->FindTilingWithResolution(LOW_RESOLUTION);
  EXPECT_TRUE(low_res_tiling);

  ActivateTree();
  ASSERT_EQ(2u, active_layer()->tilings()->num_tilings());
  auto* other_low_res_tiling =
      active_layer()->tilings()->FindTilingWithResolution(LOW_RESOLUTION);
  EXPECT_TRUE(other_low_res_tiling);
  EXPECT_EQ(low_res_tiling, other_low_res_tiling);
}

TEST_F(LegacySWPictureLayerImplTest, ZoomOutCrash) {
  gfx::Size layer_bounds(1300, 1900);

  // Set up the high and low res tilings before pinch zoom.
  SetupDefaultTrees(layer_bounds);
  ResetTilingsAndRasterScales();
  EXPECT_EQ(0u, active_layer()->tilings()->num_tilings());
  SetContentsScaleOnBothLayers(32.0f, 1.0f, 32.0f);
  EXPECT_EQ(32.f, active_layer()->HighResTiling()->contents_scale_key());

  // Since this test simulates a pinch it needs an input handler.
  // TODO(bokan): This is a raster unit test, it shouldn't be using a real
  // input handler.
  InputHandler::Create(static_cast<CompositorDelegateForInput&>(*host_impl()));

  host_impl()->GetInputHandler().PinchGestureBegin(
      gfx::Point(), ui::ScrollInputType::kTouchscreen);
  SetContentsScaleOnBothLayers(1.0f, 1.0f, 1.0f);
  SetContentsScaleOnBothLayers(1.0f, 1.0f, 1.0f);
  EXPECT_EQ(active_layer()->tilings()->NumHighResTilings(), 1);
}

TEST_F(LegacySWPictureLayerImplTest, ScaledBoundsOverflowInt) {
  // Limit visible size.
  gfx::Size viewport_size(1, 1);
  host_impl()->active_tree()->SetDeviceViewportRect(gfx::Rect(viewport_size));

  gfx::Size layer_bounds(600000, 60);

  // Set up the high and low res tilings before pinch zoom.
  SetupDefaultTrees(layer_bounds);
  ResetTilingsAndRasterScales();
  EXPECT_EQ(0u, active_layer()->tilings()->num_tilings());
  float scale = 8000.f;

  // Verify this will overflow an int.
  EXPECT_GT(static_cast<float>(layer_bounds.width()) * scale,
            static_cast<float>(std::numeric_limits<int>::max()));

  SetContentsScaleOnBothLayers(scale, 1.0f, scale);
  float adjusted_scale = active_layer()->HighResTiling()->contents_scale_key();
  EXPECT_LT(adjusted_scale, scale);

  // PopulateSharedQuadState CHECKs for overflows.
  // See http://crbug.com/679035
  active_layer()->draw_properties().visible_layer_rect =
      gfx::Rect(layer_bounds);
  viz::SharedQuadState state;
  active_layer()->PopulateScaledSharedQuadState(
      &state, adjusted_scale, active_layer()->contents_opaque());
}

TEST_F(LegacySWPictureLayerImplTest, PinchGestureTilings) {
  gfx::Size layer_bounds(1300, 1900);

  float low_res_factor = host_impl()->settings().low_res_contents_scale_factor;
  // Set up the high and low res tilings before pinch zoom.
  SetupDefaultTrees(layer_bounds);
  ResetTilingsAndRasterScales();

  SetContentsScaleOnBothLayers(2.f, 1.0f, 2.f);
  ASSERT_EQ(active_layer()->num_tilings(), 2u);
  ASSERT_EQ(pending_layer()->num_tilings(), 1u);
  EXPECT_EQ(active_layer()->tilings()->tiling_at(0)->contents_scale_key(), 2.f);
  EXPECT_EQ(active_layer()->tilings()->tiling_at(1)->contents_scale_key(),
            2.f * low_res_factor);
  // One of the tilings has to be a low resolution one.
  EXPECT_EQ(LOW_RESOLUTION,
            active_layer()->tilings()->tiling_at(1)->resolution());

  // Ensure UpdateTiles won't remove any tilings.
  active_layer()->MarkAllTilingsUsed();

  // Since this test simulates a pinch it needs an input handler.
  // TODO(bokan): This is a raster unit test, it shouldn't be using a real
  // input handler.
  InputHandler::Create(static_cast<CompositorDelegateForInput&>(*host_impl()));

  // Start a pinch gesture.
  host_impl()->GetInputHandler().PinchGestureBegin(
      gfx::Point(), ui::ScrollInputType::kTouchscreen);

  // Zoom out by a small amount. We should create a tiling at half
  // the scale (2/kMaxScaleRatioDuringPinch).
  SetContentsScaleOnBothLayers(1.8f, 1.0f, 1.8f);
  ASSERT_EQ(3u, active_layer()->tilings()->num_tilings());
  EXPECT_FLOAT_EQ(
      2.0f, active_layer()->tilings()->tiling_at(0)->contents_scale_key());
  EXPECT_FLOAT_EQ(
      1.0f, active_layer()->tilings()->tiling_at(1)->contents_scale_key());
  EXPECT_FLOAT_EQ(
      2.0f * low_res_factor,
      active_layer()->tilings()->tiling_at(2)->contents_scale_key());
  // Since we're pinching, we shouldn't create a low resolution tiling.
  EXPECT_FALSE(
      active_layer()->tilings()->FindTilingWithResolution(LOW_RESOLUTION));

  // Ensure UpdateTiles won't remove any tilings.
  active_layer()->MarkAllTilingsUsed();

  // Zoom out further, close to our low-res scale factor. We should
  // use that tiling as high-res, and not create a new tiling.
  SetContentsScaleOnBothLayers(low_res_factor * 2.1f, 1.0f,
                               low_res_factor * 2.1f);
  ASSERT_EQ(3u, active_layer()->tilings()->num_tilings());
  EXPECT_FALSE(
      active_layer()->tilings()->FindTilingWithResolution(LOW_RESOLUTION));

  // Zoom in a lot now. Since we increase by increments of
  // kMaxScaleRatioDuringPinch, this will create a new tiling at 4.0.
  SetContentsScaleOnBothLayers(3.8f, 1.0f, 3.8f);
  ASSERT_EQ(4u, active_layer()->tilings()->num_tilings());
  EXPECT_FLOAT_EQ(
      4.0f, active_layer()->tilings()->tiling_at(0)->contents_scale_key());
  // Although one of the tilings matches the low resolution scale, it still
  // shouldn't be marked as low resolution since we're pinching.
  auto* low_res_tiling =
      active_layer()->tilings()->FindTilingWithScaleKey(4.f * low_res_factor);
  EXPECT_TRUE(low_res_tiling);
  EXPECT_NE(LOW_RESOLUTION, low_res_tiling->resolution());

  // Stop a pinch gesture.
  host_impl()->GetInputHandler().PinchGestureEnd(gfx::Point());

  // Ensure UpdateTiles won't remove any tilings.
  active_layer()->MarkAllTilingsUsed();

  // After pinch ends, set the scale to what the raster scale was updated to
  // (checked above).
  SetContentsScaleOnBothLayers(4.0f, 1.0f, 4.0f);
  ASSERT_EQ(4u, active_layer()->tilings()->num_tilings());
  EXPECT_FLOAT_EQ(
      4.0f, active_layer()->tilings()->tiling_at(0)->contents_scale_key());
  // Now that we stopped pinching, the low resolution tiling that existed should
  // now be marked as low resolution.
  low_res_tiling =
      active_layer()->tilings()->FindTilingWithScaleKey(4.f * low_res_factor);
  EXPECT_TRUE(low_res_tiling);
  EXPECT_EQ(LOW_RESOLUTION, low_res_tiling->resolution());
}

TEST_F(LegacySWPictureLayerImplTest, SnappedTilingDuringZoom) {
  gfx::Size layer_bounds(2600, 3800);
  SetupDefaultTrees(layer_bounds);

  ResetTilingsAndRasterScales();
  EXPECT_EQ(0u, active_layer()->tilings()->num_tilings());

  // Set up the high and low res tilings before pinch zoom.
  SetContentsScaleOnBothLayers(0.24f, 1.0f, 0.24f);
  ASSERT_EQ(2u, active_layer()->tilings()->num_tilings());
  EXPECT_FLOAT_EQ(
      0.24f, active_layer()->tilings()->tiling_at(0)->contents_scale_key());
  EXPECT_FLOAT_EQ(
      0.06f, active_layer()->tilings()->tiling_at(1)->contents_scale_key());

  // Ensure UpdateTiles won't remove any tilings.
  active_layer()->MarkAllTilingsUsed();

  // Since this test simulates a pinch it needs an input handler.
  // TODO(bokan): This is a raster unit test, it shouldn't be using a real
  // input handler.
  InputHandler::Create(static_cast<CompositorDelegateForInput&>(*host_impl()));

  // Start a pinch gesture.
  host_impl()->GetInputHandler().PinchGestureBegin(
      gfx::Point(), ui::ScrollInputType::kTouchscreen);

  // Zoom out by a small amount. We should create a tiling at half
  // the scale (1/kMaxScaleRatioDuringPinch).
  SetContentsScaleOnBothLayers(0.2f, 1.0f, 0.2f);
  ASSERT_EQ(3u, active_layer()->tilings()->num_tilings());
  EXPECT_FLOAT_EQ(
      0.24f, active_layer()->tilings()->tiling_at(0)->contents_scale_key());
  EXPECT_FLOAT_EQ(
      0.12f, active_layer()->tilings()->tiling_at(1)->contents_scale_key());
  EXPECT_FLOAT_EQ(
      0.06f, active_layer()->tilings()->tiling_at(2)->contents_scale_key());

  // Ensure UpdateTiles won't remove any tilings.
  active_layer()->MarkAllTilingsUsed();

  // Zoom out further, close to our low-res scale factor. We should
  // use that tiling as high-res, and not create a new tiling.
  SetContentsScaleOnBothLayers(0.1f, 1.0f, 0.1f);
  ASSERT_EQ(3u, active_layer()->tilings()->num_tilings());

  // Zoom in. 0.22(desired_scale) should be snapped to 0.24 during zoom-in
  // because 0.22(desired_scale) is within the ratio(1.2).
  SetContentsScaleOnBothLayers(0.22f, 1.0f, 0.22f);
  ASSERT_EQ(3u, active_layer()->tilings()->num_tilings());
  EXPECT_FLOAT_EQ(
      0.24f, active_layer()->tilings()->tiling_at(0)->contents_scale_key());

  // Zoom in a lot. Since we move in factors of two, we should get a scale that
  // is a power of 2 times 0.24.
  SetContentsScaleOnBothLayers(1.f, 1.0f, 1.f);
  ASSERT_EQ(4u, active_layer()->tilings()->num_tilings());
  EXPECT_FLOAT_EQ(
      1.92f, active_layer()->tilings()->tiling_at(0)->contents_scale_key());
}

TEST_F(LegacySWPictureLayerImplTest, CleanUpTilings) {
  gfx::Size layer_bounds(1300, 1900);

  std::vector<raw_ptr<PictureLayerTiling, VectorExperimental>> used_tilings;

  float low_res_factor = host_impl()->settings().low_res_contents_scale_factor;
  EXPECT_LT(low_res_factor, 1.f);

  float scale = 1.f;
  float page_scale = 1.f;

  SetupDefaultTrees(layer_bounds);
  SetWillChangeTransform(active_layer(), true);
  EXPECT_FLOAT_EQ(2u, active_layer()->tilings()->num_tilings());
  EXPECT_FLOAT_EQ(
      1.f, active_layer()->tilings()->tiling_at(0)->contents_scale_key());
  EXPECT_FLOAT_EQ(
      1.f * low_res_factor,
      active_layer()->tilings()->tiling_at(1)->contents_scale_key());

  // Ensure UpdateTiles won't remove any tilings. Note this is unrelated to
  // |used_tilings| variable, and it's here only to ensure that active_layer()
  // won't remove tilings before the test has a chance to verify behavior.
  active_layer()->MarkAllTilingsUsed();

  // We only have ideal tilings, so they aren't removed.
  used_tilings.clear();
  active_layer()->CleanUpTilingsOnActiveLayer(used_tilings);
  EXPECT_FLOAT_EQ(2u, active_layer()->tilings()->num_tilings());
  EXPECT_FLOAT_EQ(
      1.f, active_layer()->tilings()->tiling_at(0)->contents_scale_key());
  EXPECT_FLOAT_EQ(
      1.f * low_res_factor,
      active_layer()->tilings()->tiling_at(1)->contents_scale_key());

  // Since this test simulates a pinch it needs an input handler.
  // TODO(bokan): This is a raster unit test, it shouldn't be using a real
  // input handler.
  InputHandler::Create(static_cast<CompositorDelegateForInput&>(*host_impl()));

  host_impl()->GetInputHandler().PinchGestureBegin(
      gfx::Point(), ui::ScrollInputType::kTouchscreen);

  // Changing the ideal but not creating new tilings.
  scale = 1.5f;
  page_scale = 1.5f;
  SetContentsScaleOnBothLayers(scale, 1.f, page_scale);
  EXPECT_FLOAT_EQ(2u, active_layer()->tilings()->num_tilings());
  EXPECT_FLOAT_EQ(
      1.f, active_layer()->tilings()->tiling_at(0)->contents_scale_key());
  EXPECT_FLOAT_EQ(
      1.f * low_res_factor,
      active_layer()->tilings()->tiling_at(1)->contents_scale_key());

  // The tilings are still our target scale, so they aren't removed.
  used_tilings.clear();
  active_layer()->CleanUpTilingsOnActiveLayer(used_tilings);
  ASSERT_EQ(2u, active_layer()->tilings()->num_tilings());
  EXPECT_FLOAT_EQ(
      1.f, active_layer()->tilings()->tiling_at(0)->contents_scale_key());
  EXPECT_FLOAT_EQ(
      1.f * low_res_factor,
      active_layer()->tilings()->tiling_at(1)->contents_scale_key());

  host_impl()->GetInputHandler().PinchGestureEnd(gfx::Point());

  // Create a 1.2 scale tiling. Now we have 1.0 and 1.2 tilings. Ideal = 1.2.
  scale = 1.2f;
  page_scale = 1.2f;
  SetContentsScaleOnBothLayers(1.2f, 1.f, page_scale);
  ASSERT_EQ(4u, active_layer()->tilings()->num_tilings());
  EXPECT_FLOAT_EQ(
      1.2f, active_layer()->tilings()->tiling_at(0)->contents_scale_key());
  EXPECT_FLOAT_EQ(
      1.f, active_layer()->tilings()->tiling_at(1)->contents_scale_key());
  EXPECT_FLOAT_EQ(
      1.2f * low_res_factor,
      active_layer()->tilings()->tiling_at(2)->contents_scale_key());
  EXPECT_FLOAT_EQ(
      1.f * low_res_factor,
      active_layer()->tilings()->tiling_at(3)->contents_scale_key());

  // Ensure UpdateTiles won't remove any tilings.
  active_layer()->MarkAllTilingsUsed();

  // Mark the non-ideal tilings as used. They won't be removed.
  used_tilings.clear();
  used_tilings.push_back(active_layer()->tilings()->tiling_at(1));
  used_tilings.push_back(active_layer()->tilings()->tiling_at(3));
  active_layer()->CleanUpTilingsOnActiveLayer(used_tilings);
  ASSERT_EQ(4u, active_layer()->tilings()->num_tilings());
  EXPECT_FLOAT_EQ(
      1.2f, active_layer()->tilings()->tiling_at(0)->contents_scale_key());
  EXPECT_FLOAT_EQ(
      1.f, active_layer()->tilings()->tiling_at(1)->contents_scale_key());
  EXPECT_FLOAT_EQ(
      1.2f * low_res_factor,
      active_layer()->tilings()->tiling_at(2)->contents_scale_key());
  EXPECT_FLOAT_EQ(
      1.f * low_res_factor,
      active_layer()->tilings()->tiling_at(3)->contents_scale_key());

  // Now move the ideal scale to 0.5. Our target stays 1.2.
  SetContentsScaleOnBothLayers(0.5f, 1.f, page_scale);

  // The high resolution tiling is between target and ideal, so is not
  // removed.  The low res tiling for the old ideal=1.0 scale is removed.
  used_tilings.clear();
  active_layer()->CleanUpTilingsOnActiveLayer(used_tilings);
  ASSERT_EQ(3u, active_layer()->tilings()->num_tilings());
  EXPECT_FLOAT_EQ(
      1.2f, active_layer()->tilings()->tiling_at(0)->contents_scale_key());
  EXPECT_FLOAT_EQ(
      1.f, active_layer()->tilings()->tiling_at(1)->contents_scale_key());
  EXPECT_FLOAT_EQ(
      1.2f * low_res_factor,
      active_layer()->tilings()->tiling_at(2)->contents_scale_key());

  // Now move the ideal scale to 1.0. Our target stays 1.2.
  SetContentsScaleOnBothLayers(1.f, 1.f, page_scale);

  // All the tilings are between are target and the ideal, so they are not
  // removed.
  used_tilings.clear();
  active_layer()->CleanUpTilingsOnActiveLayer(used_tilings);
  ASSERT_EQ(3u, active_layer()->tilings()->num_tilings());
  EXPECT_FLOAT_EQ(
      1.2f, active_layer()->tilings()->tiling_at(0)->contents_scale_key());
  EXPECT_FLOAT_EQ(
      1.f, active_layer()->tilings()->tiling_at(1)->contents_scale_key());
  EXPECT_FLOAT_EQ(
      1.2f * low_res_factor,
      active_layer()->tilings()->tiling_at(2)->contents_scale_key());

  // Now move the ideal scale to 1.1 on the active layer. Our target stays 1.2.
  SetupDrawPropertiesAndUpdateTiles(active_layer(), 1.1f, 1.f, page_scale);

  // Because the pending layer's ideal scale is still 1.0, our tilings fall
  // in the range [1.0,1.2] and are kept.
  used_tilings.clear();
  active_layer()->CleanUpTilingsOnActiveLayer(used_tilings);
  ASSERT_EQ(3u, active_layer()->tilings()->num_tilings());
  EXPECT_FLOAT_EQ(
      1.2f, active_layer()->tilings()->tiling_at(0)->contents_scale_key());
  EXPECT_FLOAT_EQ(
      1.f, active_layer()->tilings()->tiling_at(1)->contents_scale_key());
  EXPECT_FLOAT_EQ(
      1.2f * low_res_factor,
      active_layer()->tilings()->tiling_at(2)->contents_scale_key());

  // Move the ideal scale on the pending layer to 1.1 as well. Our target stays
  // 1.2 still.
  SetupDrawPropertiesAndUpdateTiles(pending_layer(), 1.1f, 1.f, page_scale);

  // Our 1.0 tiling now falls outside the range between our ideal scale and our
  // target raster scale. But it is in our used tilings set, so nothing is
  // deleted.
  used_tilings.clear();
  used_tilings.push_back(active_layer()->tilings()->tiling_at(1));
  active_layer()->CleanUpTilingsOnActiveLayer(used_tilings);
  ASSERT_EQ(3u, active_layer()->tilings()->num_tilings());
  EXPECT_FLOAT_EQ(
      1.2f, active_layer()->tilings()->tiling_at(0)->contents_scale_key());
  EXPECT_FLOAT_EQ(
      1.f, active_layer()->tilings()->tiling_at(1)->contents_scale_key());
  EXPECT_FLOAT_EQ(
      1.2f * low_res_factor,
      active_layer()->tilings()->tiling_at(2)->contents_scale_key());

  // If we remove it from our used tilings set, it is outside the range to keep
  // so it is deleted.
  used_tilings.clear();
  active_layer()->CleanUpTilingsOnActiveLayer(used_tilings);
  ASSERT_EQ(2u, active_layer()->tilings()->num_tilings());
  EXPECT_FLOAT_EQ(
      1.2f, active_layer()->tilings()->tiling_at(0)->contents_scale_key());
  EXPECT_FLOAT_EQ(
      1.2 * low_res_factor,
      active_layer()->tilings()->tiling_at(1)->contents_scale_key());
}

TEST_F(LegacySWPictureLayerImplTest, DontAddLowResDuringAnimation) {
  // Make sure this layer covers multiple tiles, since otherwise low
  // res won't get created because it is too small.
  gfx::Size tile_size(host_impl()->settings().default_tile_size);
  // Avoid max untiled layer size heuristics via fixed tile size.
  gfx::Size layer_bounds(tile_size.width() + 1, tile_size.height() + 1);
  SetupDefaultTreesWithFixedTileSize(layer_bounds, tile_size, Region());

  float low_res_factor = host_impl()->settings().low_res_contents_scale_factor;
  float contents_scale = 1.f;
  float device_scale = 1.f;
  float page_scale = 1.f;
  float maximum_animation_scale = 1.f;
  bool affected_by_invalid_scale = false;

  ResetTilingsAndRasterScales();

  // Animating, so don't create low res even if there isn't one already.
  SetContentsAndAnimationScalesOnBothLayers(contents_scale, device_scale,
                                            page_scale, maximum_animation_scale,
                                            affected_by_invalid_scale);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), 1.f);
  EXPECT_BOTH_EQ(num_tilings(), 1u);

  // Stop animating, low res gets created.
  SetContentsScaleOnBothLayers(contents_scale, device_scale, page_scale);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), 1.f);
  EXPECT_EQ(active_layer()->LowResTiling()->contents_scale_key(),
            low_res_factor);
  EXPECT_EQ(active_layer()->num_tilings(), 2u);
  EXPECT_EQ(pending_layer()->num_tilings(), 1u);

  // Ensure UpdateTiles won't remove any tilings.
  active_layer()->MarkAllTilingsUsed();

  // Page scale animation, new high res, but no low res. We still have
  // a tiling at the previous scale, it's just not marked as low res on the
  // active layer. The pending layer drops non-ideal tilings.
  contents_scale = 2.f;
  page_scale = 2.f;
  maximum_animation_scale = 2.f;
  SetContentsAndAnimationScalesOnBothLayers(contents_scale, device_scale,
                                            page_scale, maximum_animation_scale,
                                            affected_by_invalid_scale);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), 2.f);
  EXPECT_FALSE(active_layer()->LowResTiling());
  EXPECT_FALSE(pending_layer()->LowResTiling());
  EXPECT_EQ(3u, active_layer()->num_tilings());
  EXPECT_EQ(1u, pending_layer()->num_tilings());

  // Stop animating, new low res gets created for final page scale.
  SetContentsScaleOnBothLayers(contents_scale, device_scale, page_scale);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), 2.f);
  EXPECT_EQ(active_layer()->LowResTiling()->contents_scale_key(),
            2.f * low_res_factor);
  EXPECT_EQ(4u, active_layer()->num_tilings());
  EXPECT_EQ(1u, pending_layer()->num_tilings());
}

TEST_F(LegacySWPictureLayerImplTest, DontAddLowResForSmallLayers) {
  gfx::Size layer_bounds(host_impl()->settings().default_tile_size);

  scoped_refptr<FakeRasterSource> pending_raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);
  scoped_refptr<FakeRasterSource> active_raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);

  SetupTrees(pending_raster_source, active_raster_source);

  float low_res_factor = host_impl()->settings().low_res_contents_scale_factor;
  float device_scale = 1.f;
  float page_scale = 1.f;

  // Contents exactly fit on one tile at scale 1, no low res.
  float contents_scale = 1.f;
  SetContentsScaleOnBothLayers(contents_scale, device_scale, page_scale);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), contents_scale);
  EXPECT_BOTH_EQ(num_tilings(), 1u);

  ResetTilingsAndRasterScales();

  // Contents that are smaller than one tile, no low res.
  contents_scale = 0.123f;
  SetContentsScaleOnBothLayers(contents_scale, device_scale, page_scale);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), contents_scale);
  EXPECT_BOTH_EQ(num_tilings(), 1u);

  // TODO(danakj): Remove these when raster scale doesn't get fixed?
  ResetTilingsAndRasterScales();

  // Any content bounds that would create more than one tile will
  // generate a low res tiling.
  contents_scale = 2.5f;
  SetContentsScaleOnBothLayers(contents_scale, device_scale, page_scale);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), contents_scale);
  EXPECT_EQ(active_layer()->LowResTiling()->contents_scale_key(),
            contents_scale * low_res_factor);
  EXPECT_FALSE(pending_layer()->LowResTiling());
  EXPECT_EQ(active_layer()->num_tilings(), 2u);
  EXPECT_EQ(pending_layer()->num_tilings(), 1u);

  // Mask layers dont create low res since they always fit on one tile.
  CreateEffectNode(pending_layer());
  auto* mask = AddLayer<FakePictureLayerImpl>(host_impl()->pending_tree(),
                                              pending_raster_source);
  SetupMaskProperties(pending_layer(), mask);

  UpdateDrawProperties(host_impl()->pending_tree());

  // We did an UpdateDrawProperties above, which will set a contents scale on
  // the mask layer, so allow us to reset the contents scale.
  mask->ReleaseTileResources();
  mask->RecreateTileResources();

  SetupDrawPropertiesAndUpdateTiles(mask, contents_scale, device_scale,
                                    page_scale);
  EXPECT_EQ(mask->HighResTiling()->contents_scale_key(), contents_scale);
  EXPECT_EQ(mask->num_tilings(), 1u);
}

TEST_F(LegacySWPictureLayerImplTest, HugeBackdropFilterMasksGetScaledDown) {
  host_impl()->AdvanceToNextFrame(base::Milliseconds(1));

  gfx::Size layer_bounds(1000, 1000);

  scoped_refptr<FakeRasterSource> valid_raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);
  SetupPendingTree(valid_raster_source);

  CreateEffectNode(pending_layer())
      .backdrop_filters.Append(FilterOperation::CreateInvertFilter(1.0));
  auto* pending_mask = AddLayer<FakePictureLayerImpl>(
      host_impl()->pending_tree(), valid_raster_source);
  SetupMaskProperties(pending_layer(), pending_mask);
  ASSERT_TRUE(pending_mask->is_backdrop_filter_mask());

  host_impl()->AdvanceToNextFrame(base::Milliseconds(1));
  UpdateDrawProperties(host_impl()->pending_tree());

  EXPECT_EQ(1.f, pending_mask->HighResTiling()->contents_scale_key());
  EXPECT_EQ(1u, pending_mask->num_tilings());

  host_impl()->tile_manager()->InitializeTilesWithResourcesForTesting(
      pending_mask->HighResTiling()->AllTilesForTesting());

  ActivateTree();

  FakePictureLayerImpl* active_mask = static_cast<FakePictureLayerImpl*>(
      host_impl()->active_tree()->LayerById(pending_mask->id()));

  // Mask layers have a tiling with a single tile in it.
  EXPECT_EQ(1u, active_mask->HighResTiling()->AllTilesForTesting().size());
  // The mask resource exists.
  viz::ResourceId mask_resource_id;
  gfx::Size mask_texture_size;
  gfx::SizeF mask_uv_size;
  active_mask->GetContentsResourceId(&mask_resource_id, &mask_texture_size,
                                     &mask_uv_size);
  EXPECT_NE(viz::kInvalidResourceId, mask_resource_id);
  EXPECT_EQ(active_mask->bounds(), mask_texture_size);
  EXPECT_EQ(gfx::SizeF(1.0f, 1.0f), mask_uv_size);

  // Drop resources and recreate them, still the same.
  pending_mask->ReleaseTileResources();
  active_mask->ReleaseTileResources();
  pending_mask->RecreateTileResources();
  active_mask->RecreateTileResources();
  SetupDrawPropertiesAndUpdateTiles(active_mask, 1.f, 1.f, 1.f);
  active_mask->HighResTiling()->CreateAllTilesForTesting();
  EXPECT_EQ(1u, active_mask->HighResTiling()->AllTilesForTesting().size());

  // Resize larger than the max texture size.
  int max_texture_size = host_impl()->max_texture_size();
  gfx::Size huge_bounds(max_texture_size + 1, 10);
  scoped_refptr<FakeRasterSource> huge_raster_source =
      FakeRasterSource::CreateFilled(huge_bounds);

  SetupPendingTree(huge_raster_source);
  pending_mask->SetBounds(huge_bounds);
  pending_mask->SetRasterSource(huge_raster_source, Region());

  host_impl()->AdvanceToNextFrame(base::Milliseconds(1));
  UpdateDrawProperties(host_impl()->pending_tree());

  // The mask tiling gets scaled down.
  EXPECT_LT(pending_mask->HighResTiling()->contents_scale_key(), 1.f);
  EXPECT_EQ(1u, pending_mask->num_tilings());

  host_impl()->tile_manager()->InitializeTilesWithResourcesForTesting(
      pending_mask->HighResTiling()->AllTilesForTesting());

  ActivateTree();

  // Mask layers have a tiling with a single tile in it.
  EXPECT_EQ(1u, active_mask->HighResTiling()->AllTilesForTesting().size());
  // The mask resource exists.
  active_mask->GetContentsResourceId(&mask_resource_id, &mask_texture_size,
                                     &mask_uv_size);
  EXPECT_NE(viz::kInvalidResourceId, mask_resource_id);
  gfx::Size expected_size = active_mask->bounds();
  expected_size.SetToMin(gfx::Size(max_texture_size, max_texture_size));
  EXPECT_EQ(expected_size, mask_texture_size);
  EXPECT_EQ(gfx::SizeF(1.0f, 1.0f), mask_uv_size);

  // Drop resources and recreate them, still the same.
  pending_mask->ReleaseTileResources();
  active_mask->ReleaseTileResources();
  pending_mask->RecreateTileResources();
  active_mask->RecreateTileResources();
  SetupDrawPropertiesAndUpdateTiles(active_mask, 1.f, 1.f, 1.f);
  active_mask->HighResTiling()->CreateAllTilesForTesting();
  EXPECT_EQ(1u, active_mask->HighResTiling()->AllTilesForTesting().size());
  EXPECT_NE(viz::kInvalidResourceId, mask_resource_id);
  EXPECT_EQ(expected_size, mask_texture_size);

  // Do another activate, the same holds.
  SetupPendingTree(huge_raster_source);
  ActivateTree();
  EXPECT_EQ(1u, active_mask->HighResTiling()->AllTilesForTesting().size());
  active_layer()->GetContentsResourceId(&mask_resource_id, &mask_texture_size,
                                        &mask_uv_size);
  EXPECT_EQ(expected_size, mask_texture_size);
  EXPECT_EQ(viz::kInvalidResourceId, mask_resource_id);
  EXPECT_EQ(gfx::SizeF(1.0f, 1.0f), mask_uv_size);

  // Resize even larger, so that the scale would be smaller than the minimum
  // contents scale. Then the layer should no longer have any tiling.
  gfx::Size extra_huge_bounds(max_texture_size * 10 + 1, 10);
  scoped_refptr<FakeRasterSource> extra_huge_raster_source =
      FakeRasterSource::CreateFilled(extra_huge_bounds);

  SetupPendingTree(extra_huge_raster_source);
  pending_mask->SetBounds(extra_huge_bounds);
  pending_mask->SetRasterSource(extra_huge_raster_source, Region());

  EXPECT_FALSE(pending_mask->CanHaveTilings());

  host_impl()->AdvanceToNextFrame(base::Milliseconds(1));
  UpdateDrawProperties(host_impl()->pending_tree());

  EXPECT_EQ(0u, pending_mask->num_tilings());
}

TEST_F(LegacySWPictureLayerImplTest, ScaledBackdropFilterMaskLayer) {
  host_impl()->AdvanceToNextFrame(base::Milliseconds(1));

  gfx::Size layer_bounds(1000, 1000);

  SetInitialDeviceScaleFactor(1.3f);

  scoped_refptr<FakeRasterSource> valid_raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);
  SetupPendingTree(valid_raster_source);

  CreateEffectNode(pending_layer())
      .backdrop_filters.Append(FilterOperation::CreateInvertFilter(1.0));
  auto* pending_mask = AddLayer<FakePictureLayerImpl>(
      host_impl()->pending_tree(), valid_raster_source);
  SetupMaskProperties(pending_layer(), pending_mask);
  ASSERT_TRUE(pending_mask->is_backdrop_filter_mask());

  host_impl()->AdvanceToNextFrame(base::Milliseconds(1));
  UpdateDrawProperties(host_impl()->pending_tree());

  // Masks are scaled, and do not have a low res tiling.
  EXPECT_EQ(1.3f, pending_mask->HighResTiling()->contents_scale_key());
  EXPECT_EQ(1u, pending_mask->num_tilings());

  host_impl()->tile_manager()->InitializeTilesWithResourcesForTesting(
      pending_mask->HighResTiling()->AllTilesForTesting());

  ActivateTree();

  FakePictureLayerImpl* active_mask = static_cast<FakePictureLayerImpl*>(
      host_impl()->active_tree()->LayerById(pending_mask->id()));

  // Mask layers have a tiling with a single tile in it.
  EXPECT_EQ(1u, active_mask->HighResTiling()->AllTilesForTesting().size());
  // The mask resource exists.
  viz::ResourceId mask_resource_id;
  gfx::Size mask_texture_size;
  gfx::SizeF mask_uv_size;
  active_mask->GetContentsResourceId(&mask_resource_id, &mask_texture_size,
                                     &mask_uv_size);
  EXPECT_NE(viz::kInvalidResourceId, mask_resource_id);
  gfx::Size expected_mask_texture_size =
      gfx::ScaleToCeiledSize(active_mask->bounds(), 1.3f);
  EXPECT_EQ(mask_texture_size, expected_mask_texture_size);
  EXPECT_EQ(gfx::SizeF(1.0f, 1.0f), mask_uv_size);
}

TEST_F(LegacySWPictureLayerImplTest, ScaledMaskLayer) {
  host_impl()->AdvanceToNextFrame(base::Milliseconds(1));

  gfx::Size layer_bounds(1000, 1000);

  SetInitialDeviceScaleFactor(1.3f);

  scoped_refptr<FakeRasterSource> valid_raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);
  SetupPendingTree(valid_raster_source);

  CreateEffectNode(pending_layer());
  auto* pending_mask = AddLayer<FakePictureLayerImpl>(
      host_impl()->pending_tree(), valid_raster_source);
  SetupMaskProperties(pending_layer(), pending_mask);
  ASSERT_FALSE(pending_mask->is_backdrop_filter_mask());

  host_impl()->AdvanceToNextFrame(base::Milliseconds(1));
  UpdateDrawProperties(host_impl()->pending_tree());

  // Masks are scaled, and do not have a low res tiling.
  EXPECT_EQ(1.3f, pending_mask->HighResTiling()->contents_scale_key());
  EXPECT_EQ(1u, pending_mask->num_tilings());

  host_impl()->tile_manager()->InitializeTilesWithResourcesForTesting(
      pending_mask->HighResTiling()->AllTilesForTesting());

  ActivateTree();

  FakePictureLayerImpl* active_mask = static_cast<FakePictureLayerImpl*>(
      host_impl()->active_tree()->LayerById(pending_mask->id()));

  // Non-backdrop-filter mask layers are tiled normally.
  EXPECT_EQ(36u, active_mask->HighResTiling()->AllTilesForTesting().size());
  // And don't have mask resources.
  viz::ResourceId mask_resource_id;
  gfx::Size mask_texture_size;
  gfx::SizeF mask_uv_size;
  active_mask->GetContentsResourceId(&mask_resource_id, &mask_texture_size,
                                     &mask_uv_size);
  EXPECT_EQ(viz::kInvalidResourceId, mask_resource_id);
  EXPECT_EQ(gfx::Size(), mask_texture_size);
  EXPECT_EQ(gfx::SizeF(), mask_uv_size);
}

TEST_F(LegacySWPictureLayerImplTest, ReleaseTileResources) {
  gfx::Size layer_bounds(1300, 1900);
  SetupDefaultTrees(layer_bounds);
  EXPECT_EQ(1u, pending_layer()->tilings()->num_tilings());

  // All tilings should be removed when losing output surface.
  active_layer()->ReleaseTileResources();
  active_layer()->RecreateTileResources();
  EXPECT_EQ(0u, active_layer()->num_tilings());
  pending_layer()->ReleaseTileResources();
  pending_layer()->RecreateTileResources();
  EXPECT_EQ(0u, pending_layer()->num_tilings());

  // This should create new tilings.
  SetupDrawPropertiesAndUpdateTiles(pending_layer(),
                                    1.f,   // ideal contents scale
                                    1.f,   // device scale
                                    1.f);  // page scale
  EXPECT_EQ(1u, pending_layer()->tilings()->num_tilings());
}

// ReleaseResources should behave identically to ReleaseTileResources.
TEST_F(LegacySWPictureLayerImplTest, ReleaseResources) {
  gfx::Size layer_bounds(1300, 1900);
  SetupDefaultTrees(layer_bounds);
  EXPECT_EQ(1u, pending_layer()->tilings()->num_tilings());

  // All tilings should be removed when losing output surface.
  active_layer()->ReleaseResources();
  EXPECT_TRUE(active_layer()->tilings());
  EXPECT_EQ(0u, active_layer()->num_tilings());
  active_layer()->RecreateTileResources();
  EXPECT_EQ(0u, active_layer()->num_tilings());

  pending_layer()->ReleaseResources();
  EXPECT_TRUE(pending_layer()->tilings());
  EXPECT_EQ(0u, pending_layer()->num_tilings());
  pending_layer()->RecreateTileResources();
  EXPECT_EQ(0u, pending_layer()->num_tilings());
}

TEST_F(LegacySWPictureLayerImplTest, ClampTilesToMaxTileSize) {
  gfx::Size layer_bounds(5000, 5000);

  scoped_refptr<FakeRasterSource> pending_raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);

  SetupPendingTree(pending_raster_source);
  EXPECT_GE(pending_layer()->tilings()->num_tilings(), 1u);

  pending_layer()->tilings()->tiling_at(0)->CreateAllTilesForTesting();

  // The default value.
  EXPECT_EQ(gfx::Size(256, 256).ToString(),
            host_impl()->settings().default_tile_size.ToString());

  Tile* tile =
      pending_layer()->tilings()->tiling_at(0)->AllTilesForTesting()[0];
  EXPECT_EQ(gfx::Size(256, 256).ToString(),
            tile->content_rect().size().ToString());

  ResetTilingsAndRasterScales();

  // Change the max texture size on the output surface context.
  auto worker_context_provider = viz::TestContextProvider::CreateWorker();
  {
    viz::RasterContextProvider::ScopedRasterContextLock scoped_context(
        worker_context_provider.get());
    worker_context_provider->GetTestRasterInterface()->set_max_texture_size(
        140);
  }
  ResetLayerTreeFrameSink(
      FakeLayerTreeFrameSink::Create3d(viz::TestContextProvider::CreateRaster(),
                                       std::move(worker_context_provider)));

  SetupDrawPropertiesAndUpdateTiles(pending_layer(), 1.f, 1.f, 1.f);
  ASSERT_EQ(1u, pending_layer()->tilings()->num_tilings());

  pending_layer()->tilings()->tiling_at(0)->CreateAllTilesForTesting();

  // Verify the tiles are not larger than the context's max texture size.
  tile = pending_layer()->tilings()->tiling_at(0)->AllTilesForTesting()[0];
  EXPECT_GE(140, tile->content_rect().width());
  EXPECT_GE(140, tile->content_rect().height());
}

TEST_F(LegacySWPictureLayerImplTest, ClampSingleTileToToMaxTileSize) {
  gfx::Size layer_bounds(500, 500);
  SetupDefaultTrees(layer_bounds);
  EXPECT_GE(active_layer()->tilings()->num_tilings(), 1u);

  active_layer()->tilings()->tiling_at(0)->CreateAllTilesForTesting();

  // The default value. The layer is smaller than this.
  EXPECT_EQ(gfx::Size(512, 512).ToString(),
            host_impl()->settings().max_untiled_layer_size.ToString());

  // There should be a single tile since the layer is small.
  PictureLayerTiling* high_res_tiling = active_layer()->tilings()->tiling_at(0);
  EXPECT_EQ(1u, high_res_tiling->AllTilesForTesting().size());

  ResetTilingsAndRasterScales();

  // Change the max texture size on the output surface context.
  auto worker_context_provider = viz::TestContextProvider::CreateWorker();
  {
    viz::RasterContextProvider::ScopedRasterContextLock scoped_context(
        worker_context_provider.get());
    worker_context_provider->GetTestRasterInterface()->set_max_texture_size(
        140);
  }
  ResetLayerTreeFrameSink(
      FakeLayerTreeFrameSink::Create3d(viz::TestContextProvider::CreateRaster(),
                                       std::move(worker_context_provider)));

  SetupDrawPropertiesAndUpdateTiles(active_layer(), 1.f, 1.f, 1.f);
  ASSERT_LE(1u, active_layer()->tilings()->num_tilings());

  active_layer()->tilings()->tiling_at(0)->CreateAllTilesForTesting();

  // There should be more than one tile since the max texture size won't cover
  // the layer.
  high_res_tiling = active_layer()->tilings()->tiling_at(0);
  EXPECT_LT(1u, high_res_tiling->AllTilesForTesting().size());

  // Verify the tiles are not larger than the context's max texture size.
  Tile* tile = active_layer()->tilings()->tiling_at(0)->AllTilesForTesting()[0];
  EXPECT_GE(140, tile->content_rect().width());
  EXPECT_GE(140, tile->content_rect().height());
}

TEST_F(LegacySWPictureLayerImplTest, DisallowTileDrawQuads) {
  auto render_pass = viz::CompositorRenderPass::Create();

  gfx::Size layer_bounds(1300, 1900);
  gfx::Rect layer_rect(layer_bounds);

  gfx::Rect layer_invalidation(150, 200, 30, 180);
  SetupDefaultTreesWithInvalidation(layer_bounds, layer_invalidation);

  active_layer()->SetContentsOpaque(true);
  active_layer()->draw_properties().visible_layer_rect =
      gfx::Rect(layer_bounds);

  AppendQuadsData data;
  active_layer()->WillDraw(DRAW_MODE_RESOURCELESS_SOFTWARE, nullptr);
  active_layer()->AppendQuads(render_pass.get(), &data);
  active_layer()->DidDraw(nullptr);

  ASSERT_EQ(1u, render_pass->quad_list.size());
  EXPECT_EQ(viz::DrawQuad::Material::kPictureContent,
            render_pass->quad_list.front()->material);
  EXPECT_EQ(render_pass->quad_list.front()->rect, layer_rect);
  EXPECT_FALSE(render_pass->quad_list.front()->needs_blending);
  EXPECT_TRUE(
      render_pass->quad_list.front()->shared_quad_state->are_contents_opaque);
  EXPECT_EQ(render_pass->quad_list.front()->visible_rect, layer_rect);
}

TEST_F(LegacySWPictureLayerImplTest, ResourcelessPartialRecording) {
  auto render_pass = viz::CompositorRenderPass::Create();

  gfx::Size layer_bounds(700, 650);
  gfx::Rect layer_rect(layer_bounds);
  SetInitialDeviceScaleFactor(2.f);

  gfx::Rect recorded_viewport(20, 30, 40, 50);
  scoped_refptr<FakeRasterSource> active_raster_source =
      FakeRasterSource::CreatePartiallyFilled(layer_bounds, recorded_viewport);

  SetupPendingTree(active_raster_source);
  ActivateTree();

  active_layer()->SetContentsOpaque(true);
  gfx::Rect visible_rect(30, 35, 10, 5);
  active_layer()->draw_properties().visible_layer_rect = visible_rect;

  AppendQuadsData data;
  active_layer()->WillDraw(DRAW_MODE_RESOURCELESS_SOFTWARE, nullptr);
  active_layer()->AppendQuads(render_pass.get(), &data);
  active_layer()->DidDraw(nullptr);

  gfx::Rect scaled_visible = gfx::ScaleToEnclosingRect(visible_rect, 2.f);
  gfx::Rect scaled_recorded = gfx::ScaleToEnclosingRect(recorded_viewport, 2.f);
  gfx::Rect quad_visible = gfx::IntersectRects(scaled_visible, scaled_recorded);

  ASSERT_EQ(1U, render_pass->quad_list.size());
  EXPECT_EQ(viz::DrawQuad::Material::kPictureContent,
            render_pass->quad_list.front()->material);
  const viz::DrawQuad* quad = render_pass->quad_list.front();
  EXPECT_EQ(quad_visible, quad->rect);
  EXPECT_TRUE(quad->shared_quad_state->are_contents_opaque);
  EXPECT_EQ(quad_visible, quad->visible_rect);
  EXPECT_FALSE(quad->needs_blending);
}

TEST_F(LegacySWPictureLayerImplTest, ResourcelessRecordingNotVisible) {
  auto render_pass = viz::CompositorRenderPass::Create();

  gfx::Size layer_bounds(1000, 1000);
  scoped_refptr<FakeRasterSource> active_raster_source =
      FakeRasterSource::CreatePartiallyFilled(layer_bounds,
                                              gfx::Rect(0, 0, 300, 300));
  SetupPendingTree(active_raster_source);
  ActivateTree();

  active_layer()->SetContentsOpaque(true);
  active_layer()->draw_properties().visible_layer_rect =
      gfx::Rect(500, 0, 1000, 500);

  AppendQuadsData data;
  active_layer()->WillDraw(DRAW_MODE_RESOURCELESS_SOFTWARE, nullptr);
  active_layer()->AppendQuads(render_pass.get(), &data);
  active_layer()->DidDraw(nullptr);

  EXPECT_EQ(0U, render_pass->quad_list.size());
}

TEST_F(LegacySWPictureLayerImplTest, FarScrolledQuadsShifted) {
  auto render_pass = viz::CompositorRenderPass::Create();

  gfx::Size layer_bounds(1000, 10000);
  scoped_refptr<FakeRasterSource> active_raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);
  SetupPendingTree(active_raster_source);
  ActivateTree();

  active_layer()->SetContentsOpaque(true);
  active_layer()->draw_properties().visible_layer_rect =
      gfx::Rect(0, 5000, 1000, 1000);
  active_layer()->UpdateTiles();

  auto* high_res_tiling = active_layer()->HighResTiling();
  ASSERT_TRUE(high_res_tiling);
  const std::vector<Tile*>& tiles = high_res_tiling->AllTilesForTesting();
  ASSERT_GT(tiles.size(), 0u);

  host_impl()->tile_manager()->InitializeTilesWithResourcesForTesting(tiles);

  AppendQuadsData data;
  active_layer()->WillDraw(DRAW_MODE_HARDWARE, nullptr);
  active_layer()->AppendQuads(render_pass.get(), &data);
  active_layer()->DidDraw(nullptr);

  EXPECT_EQ(20u, render_pass->quad_list.size());
  int last_y = -1;
  int last_height = -1;
  int min_y = std::numeric_limits<int>::max();
  float min_transformed_y = std::numeric_limits<float>::max();
  float max_transformed_y = -1;
  for (auto* draw_quad : render_pass->quad_list) {
    if (last_y == -1) {
      last_y = draw_quad->rect.y();
      min_y = last_y;
      last_height = draw_quad->rect.height();
    }

    if (last_y != draw_quad->rect.y()) {
      EXPECT_EQ(last_y + last_height, draw_quad->rect.y());
      last_y = draw_quad->rect.y();
      min_y = std::min(min_y, last_y);
      last_height = draw_quad->rect.height();
    }
    EXPECT_LT(last_y, 5000);
    EXPECT_EQ(draw_quad->material, viz::DrawQuad::Material::kTiledContent);

    auto transform = [draw_quad](const gfx::Rect& rect) {
      return draw_quad->shared_quad_state->quad_to_target_transform.MapRect(
          gfx::RectF(rect));
    };

    gfx::RectF transformed_rect = transform(draw_quad->rect);
    EXPECT_GT(transformed_rect.y(), 0);
    if (min_transformed_y < 0 || transformed_rect.y() < min_transformed_y)
      min_transformed_y = transformed_rect.y();
    if (transformed_rect.bottom() > max_transformed_y)
      max_transformed_y = transformed_rect.bottom();

    gfx::RectF transformed_quad_layer_rect =
        transform(draw_quad->shared_quad_state->quad_layer_rect);
    EXPECT_RECTF_EQ(transformed_quad_layer_rect,
                    gfx::RectF(0.f, 0.f, 1000.f, 10000.f));

    gfx::RectF transformed_visible_quad_layer_rect =
        transform(draw_quad->shared_quad_state->visible_quad_layer_rect);
    EXPECT_RECTF_EQ(transformed_visible_quad_layer_rect,
                    gfx::RectF(0.f, 5000.f, 1000.f, 1000.f));
  }
  EXPECT_EQ(min_y, 0);
  EXPECT_FLOAT_EQ(min_transformed_y, 5000.f);
  EXPECT_FLOAT_EQ(max_transformed_y, 6000.f);
}

TEST_F(LegacySWPictureLayerImplTest, FarScrolledSolidColorQuadsShifted) {
  auto render_pass = viz::CompositorRenderPass::Create();

  gfx::Size layer_bounds(1000, 10000);
  scoped_refptr<FakeRasterSource> active_raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);
  SetupPendingTree(active_raster_source);
  ActivateTree();

  active_layer()->SetContentsOpaque(true);
  active_layer()->draw_properties().visible_layer_rect =
      gfx::Rect(0, 9000, 1000, 1000);
  active_layer()->UpdateTiles();

  auto* high_res_tiling = active_layer()->HighResTiling();
  ASSERT_TRUE(high_res_tiling);
  const std::vector<Tile*>& tiles = high_res_tiling->AllTilesForTesting();
  ASSERT_GT(tiles.size(), 0u);

  for (auto* tile : tiles)
    tile->draw_info().SetSolidColorForTesting(SkColors::kBlue);

  AppendQuadsData data;
  active_layer()->WillDraw(DRAW_MODE_HARDWARE, nullptr);
  active_layer()->AppendQuads(render_pass.get(), &data);
  active_layer()->DidDraw(nullptr);

  EXPECT_EQ(20u, render_pass->quad_list.size());
  int last_y = -1;
  int last_height = -1;
  int min_y = std::numeric_limits<int>::max();
  float min_transformed_y = std::numeric_limits<float>::max();
  float max_transformed_y = -1;
  for (auto* draw_quad : render_pass->quad_list) {
    if (last_y == -1) {
      last_y = draw_quad->rect.y();
      min_y = last_y;
      last_height = draw_quad->rect.height();
    }

    if (last_y != draw_quad->rect.y()) {
      EXPECT_EQ(last_y + last_height, draw_quad->rect.y());
      last_y = draw_quad->rect.y();
      min_y = std::min(min_y, last_y);
      last_height = draw_quad->rect.height();
    }
    EXPECT_LT(last_y, 5000);
    EXPECT_EQ(draw_quad->material, viz::DrawQuad::Material::kSolidColor);

    auto transform = [draw_quad](const gfx::Rect& rect) {
      return draw_quad->shared_quad_state->quad_to_target_transform.MapRect(
          gfx::RectF(rect));
    };

    gfx::RectF transformed_rect = transform(draw_quad->rect);
    EXPECT_GT(transformed_rect.y(), 0);
    if (transformed_rect.y() < min_transformed_y)
      min_transformed_y = transformed_rect.y();
    if (transformed_rect.bottom() > max_transformed_y)
      max_transformed_y = transformed_rect.bottom();

    gfx::RectF transformed_quad_layer_rect =
        transform(draw_quad->shared_quad_state->quad_layer_rect);
    EXPECT_RECTF_EQ(transformed_quad_layer_rect,
                    gfx::RectF(0.f, 0.f, 1000.f, 10000.f));

    gfx::RectF transformed_visible_quad_layer_rect =
        transform(draw_quad->shared_quad_state->visible_quad_layer_rect);
    EXPECT_RECTF_EQ(transformed_visible_quad_layer_rect,
                    gfx::RectF(0.f, 9000.f, 1000.f, 1000.f));
  }
  EXPECT_EQ(min_y, 0);
  EXPECT_FLOAT_EQ(min_transformed_y, 9000.f);
  EXPECT_FLOAT_EQ(max_transformed_y, 10000.f);
}

TEST_F(LegacySWPictureLayerImplTest, SolidColorLayerHasVisibleFullCoverage) {
  auto render_pass = viz::CompositorRenderPass::Create();

  gfx::Size layer_bounds(1500, 1500);
  gfx::Rect visible_rect(250, 250, 1000, 1000);

  scoped_refptr<FakeRasterSource> pending_raster_source =
      FakeRasterSource::CreateFilledSolidColor(layer_bounds);
  scoped_refptr<FakeRasterSource> active_raster_source =
      FakeRasterSource::CreateFilledSolidColor(layer_bounds);

  SetupTrees(pending_raster_source, active_raster_source);

  active_layer()->draw_properties().visible_layer_rect = visible_rect;

  AppendQuadsData data;
  active_layer()->WillDraw(DRAW_MODE_SOFTWARE, nullptr);
  active_layer()->AppendQuads(render_pass.get(), &data);
  active_layer()->DidDraw(nullptr);

  Region remaining = visible_rect;
  for (auto* quad : render_pass->quad_list) {
    EXPECT_TRUE(visible_rect.Contains(quad->rect));
    EXPECT_TRUE(remaining.Contains(quad->rect));
    remaining.Subtract(quad->rect);
  }

  EXPECT_TRUE(remaining.IsEmpty());
}

TEST_F(LegacySWPictureLayerImplTest, TileScalesWithSolidColorRasterSource) {
  gfx::Size layer_bounds(200, 200);
  scoped_refptr<FakeRasterSource> pending_raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);
  scoped_refptr<FakeRasterSource> active_raster_source =
      FakeRasterSource::CreateFilledSolidColor(layer_bounds);

  SetupTrees(pending_raster_source, active_raster_source);
  // Solid color raster source should not allow tilings at any scale.
  EXPECT_FALSE(active_layer()->CanHaveTilings());
  EXPECT_EQ(0.f, active_layer()->ideal_contents_scale_key());

  // Activate non-solid-color pending raster source makes active layer can have
  // tilings.
  ActivateTree();
  EXPECT_TRUE(active_layer()->CanHaveTilings());
  EXPECT_GT(active_layer()->ideal_contents_scale_key(), 0.f);
}

TEST_F(NoLowResPictureLayerImplTest, MarkRequiredOffscreenTiles) {
  gfx::Size layer_bounds(200, 200);

  gfx::Transform transform;
  gfx::Rect viewport(0, 0, 100, 200);
  host_impl()->SetExternalTilePriorityConstraints(viewport, transform);

  scoped_refptr<FakeRasterSource> pending_raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);
  SetupPendingTreeWithFixedTileSize(pending_raster_source, gfx::Size(100, 100),
                                    Region());

  EXPECT_EQ(1u, pending_layer()->num_tilings());
  EXPECT_EQ(
      viewport,
      pending_layer()->viewport_rect_for_tile_priority_in_content_space());

  host_impl()->AdvanceToNextFrame(base::Milliseconds(1));
  pending_layer()->UpdateTiles();

  int num_visible = 0;
  int num_offscreen = 0;

  std::unique_ptr<TilingSetRasterQueueAll> queue =
      TilingSetRasterQueueAll::Create(
          pending_layer()->picture_layer_tiling_set(), false, false);
  EXPECT_TRUE(queue);
  for (; !queue->IsEmpty(); queue->Pop()) {
    const PrioritizedTile& prioritized_tile = queue->Top();
    DCHECK(prioritized_tile.tile());
    if (prioritized_tile.priority().distance_to_visible == 0.f) {
      EXPECT_TRUE(prioritized_tile.tile()->required_for_activation());
      num_visible++;
    } else {
      EXPECT_FALSE(prioritized_tile.tile()->required_for_activation());
      num_offscreen++;
    }
  }

  EXPECT_GT(num_visible, 0);
  EXPECT_GT(num_offscreen, 0);
}

TEST_F(NoLowResPictureLayerImplTest,
       TileOutsideOfViewportForTilePriorityNotRequired) {
  host_impl()->AdvanceToNextFrame(base::Milliseconds(1));

  gfx::Size tile_size(100, 100);
  gfx::Size layer_bounds(400, 400);
  gfx::Rect external_viewport_for_tile_priority(400, 200);
  gfx::Rect visible_layer_rect(200, 400);

  SetupDefaultTreesWithFixedTileSize(layer_bounds, tile_size, Region());

  ASSERT_EQ(1u, pending_layer()->num_tilings());
  ASSERT_EQ(1.f, pending_layer()->HighResTiling()->contents_scale_key());

  // Set external viewport for tile priority.
  gfx::Transform transform_for_tile_priority;
  host_impl()->SetExternalTilePriorityConstraints(
      external_viewport_for_tile_priority, transform_for_tile_priority);
  host_impl()->AdvanceToNextFrame(base::Milliseconds(1));
  UpdateDrawProperties(host_impl()->pending_tree());

  // Set visible content rect that is different from
  // external_viewport_for_tile_priority.
  pending_layer()->draw_properties().visible_layer_rect = visible_layer_rect;
  host_impl()->AdvanceToNextFrame(base::Milliseconds(200));
  pending_layer()->UpdateTiles();

  // Intersect the two rects. Any tile outside should not be required for
  // activation.
  gfx::Rect viewport_for_tile_priority =
      pending_layer()->viewport_rect_for_tile_priority_in_content_space();
  viewport_for_tile_priority.Intersect(pending_layer()->visible_layer_rect());

  EXPECT_TRUE(pending_layer()->HighResTiling()->AllTilesForTesting().empty());

  int num_inside = 0;
  int num_outside = 0;
  for (PictureLayerTiling::CoverageIterator iter(
           active_layer()->HighResTiling(), 1.f, gfx::Rect(layer_bounds));
       iter; ++iter) {
    if (!*iter)
      continue;
    Tile* tile = *iter;
    if (viewport_for_tile_priority.Intersects(iter.geometry_rect())) {
      num_inside++;
      // Mark everything in viewport for tile priority as ready to draw.
      TileDrawInfo& draw_info = tile->draw_info();
      draw_info.SetSolidColorForTesting(SkColors::kRed);
    } else {
      num_outside++;
      EXPECT_FALSE(tile->required_for_activation());
    }
  }

  EXPECT_GT(num_inside, 0);
  EXPECT_GT(num_outside, 0);

  // Activate and draw active layer.
  ActivateTree();
  active_layer()->draw_properties().visible_layer_rect = visible_layer_rect;

  auto render_pass = viz::CompositorRenderPass::Create();
  AppendQuadsData data;
  active_layer()->WillDraw(DRAW_MODE_SOFTWARE, nullptr);
  active_layer()->AppendQuads(render_pass.get(), &data);
  active_layer()->DidDraw(nullptr);

  // All tiles in activation rect is ready to draw.
  EXPECT_EQ(0, data.num_missing_tiles);
  EXPECT_EQ(0, data.num_incompletely_rastered_tiles);
  EXPECT_EQ(0, data.num_incompletely_recorded_tiles);
  EXPECT_FALSE(active_layer()->only_used_low_res_last_append_quads());
}

TEST_F(LegacySWPictureLayerImplTest, HighResTileIsComplete) {
  host_impl()->AdvanceToNextFrame(base::Milliseconds(1));

  gfx::Size tile_size(100, 100);
  gfx::Size layer_bounds(200, 200);

  scoped_refptr<FakeRasterSource> pending_raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);

  SetupPendingTreeWithFixedTileSize(pending_raster_source, tile_size, Region());
  ActivateTree();

  // All high res tiles have resources.
  std::vector<Tile*> tiles =
      active_layer()->tilings()->tiling_at(0)->AllTilesForTesting();
  host_impl()->tile_manager()->InitializeTilesWithResourcesForTesting(tiles);

  auto render_pass = viz::CompositorRenderPass::Create();
  AppendQuadsData data;
  active_layer()->WillDraw(DRAW_MODE_SOFTWARE, nullptr);
  active_layer()->AppendQuads(render_pass.get(), &data);
  active_layer()->DidDraw(nullptr);

  // All high res tiles drew, nothing was incomplete.
  EXPECT_EQ(9u, render_pass->quad_list.size());
  EXPECT_EQ(0, data.num_missing_tiles);
  EXPECT_EQ(0, data.num_incompletely_rastered_tiles);
  EXPECT_EQ(0, data.num_incompletely_recorded_tiles);
  EXPECT_FALSE(active_layer()->only_used_low_res_last_append_quads());
}

TEST_F(LegacySWPictureLayerImplTest, HighResTileIsIncomplete) {
  host_impl()->AdvanceToNextFrame(base::Milliseconds(1));

  gfx::Size tile_size(100, 100);
  gfx::Size layer_bounds(200, 200);

  scoped_refptr<FakeRasterSource> pending_raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);
  SetupPendingTreeWithFixedTileSize(pending_raster_source, tile_size, Region());
  ActivateTree();

  auto render_pass = viz::CompositorRenderPass::Create();
  AppendQuadsData data;
  active_layer()->WillDraw(DRAW_MODE_SOFTWARE, nullptr);
  active_layer()->AppendQuads(render_pass.get(), &data);
  active_layer()->DidDraw(nullptr);

  EXPECT_EQ(1u, render_pass->quad_list.size());
  EXPECT_EQ(1, data.num_missing_tiles);
  EXPECT_EQ(0, data.num_incompletely_rastered_tiles);
  EXPECT_EQ(0, data.num_incompletely_recorded_tiles);
  EXPECT_TRUE(active_layer()->only_used_low_res_last_append_quads());
}

TEST_F(LegacySWPictureLayerImplTest, HighResTileIsIncompleteLowResComplete) {
  host_impl()->AdvanceToNextFrame(base::Milliseconds(1));

  gfx::Size tile_size(100, 100);
  gfx::Size layer_bounds(200, 200);

  scoped_refptr<FakeRasterSource> pending_raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);
  SetupPendingTreeWithFixedTileSize(pending_raster_source, tile_size, Region());
  ActivateTree();

  std::vector<Tile*> low_tiles =
      active_layer()->tilings()->tiling_at(1)->AllTilesForTesting();
  host_impl()->tile_manager()->InitializeTilesWithResourcesForTesting(
      low_tiles);

  auto render_pass = viz::CompositorRenderPass::Create();
  AppendQuadsData data;
  active_layer()->WillDraw(DRAW_MODE_SOFTWARE, nullptr);
  active_layer()->AppendQuads(render_pass.get(), &data);
  active_layer()->DidDraw(nullptr);

  EXPECT_EQ(1u, render_pass->quad_list.size());
  EXPECT_EQ(0, data.num_missing_tiles);
  EXPECT_EQ(1, data.num_incompletely_rastered_tiles);
  EXPECT_EQ(0, data.num_incompletely_recorded_tiles);
  EXPECT_TRUE(active_layer()->only_used_low_res_last_append_quads());
}

TEST_F(LegacySWPictureLayerImplTest, LowResTileIsIncomplete) {
  host_impl()->AdvanceToNextFrame(base::Milliseconds(1));

  gfx::Size tile_size(100, 100);
  gfx::Size layer_bounds(200, 200);

  scoped_refptr<FakeRasterSource> pending_raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);
  SetupPendingTreeWithFixedTileSize(pending_raster_source, tile_size, Region());
  ActivateTree();

  // All high res tiles have resources except one.
  std::vector<Tile*> high_tiles =
      active_layer()->tilings()->tiling_at(0)->AllTilesForTesting();
  high_tiles.erase(high_tiles.begin());
  host_impl()->tile_manager()->InitializeTilesWithResourcesForTesting(
      high_tiles);

  // All low res tiles have resources.
  std::vector<Tile*> low_tiles =
      active_layer()->tilings()->tiling_at(1)->AllTilesForTesting();
  host_impl()->tile_manager()->InitializeTilesWithResourcesForTesting(
      low_tiles);

  auto render_pass = viz::CompositorRenderPass::Create();
  AppendQuadsData data;
  active_layer()->WillDraw(DRAW_MODE_SOFTWARE, nullptr);
  active_layer()->AppendQuads(render_pass.get(), &data);
  active_layer()->DidDraw(nullptr);

  // The missing high res tile was replaced by a low res tile.
  EXPECT_EQ(9u, render_pass->quad_list.size());
  EXPECT_EQ(0, data.num_missing_tiles);
  EXPECT_EQ(1, data.num_incompletely_rastered_tiles);
  EXPECT_EQ(0, data.num_incompletely_recorded_tiles);
  EXPECT_FALSE(active_layer()->only_used_low_res_last_append_quads());
}

TEST_F(LegacySWPictureLayerImplTest,
       HighResAndIdealResTileIsCompleteWhenRasterScaleIsNotIdeal) {
  host_impl()->AdvanceToNextFrame(base::Milliseconds(1));

  gfx::Size tile_size(100, 100);
  gfx::Size layer_bounds(200, 200);
  gfx::Size viewport_size(400, 400);

  host_impl()->active_tree()->SetDeviceViewportRect(gfx::Rect(viewport_size));
  SetInitialDeviceScaleFactor(2.f);

  SetupDefaultTreesWithFixedTileSize(layer_bounds, tile_size, Region());
  SetWillChangeTransform(active_layer(), true);

  // One ideal tile exists, this will get used when drawing.
  std::vector<Tile*> ideal_tiles;
  EXPECT_EQ(2.f, active_layer()->HighResTiling()->contents_scale_key());
  ideal_tiles.push_back(active_layer()->HighResTiling()->TileAt(0, 0));
  host_impl()->tile_manager()->InitializeTilesWithResourcesForTesting(
      ideal_tiles);

  // Due to layer scale throttling, the raster contents scale is changed to 1,
  // while the ideal is still 2.
  SetupDrawPropertiesAndUpdateTiles(active_layer(), 1.f, 1.f, 1.f);
  SetupDrawPropertiesAndUpdateTiles(active_layer(), 2.f, 1.f, 1.f);

  EXPECT_EQ(1.f, active_layer()->HighResTiling()->contents_scale_key());
  EXPECT_EQ(1.f, active_layer()->raster_contents_scale_key());
  EXPECT_EQ(2.f, active_layer()->ideal_contents_scale_key());

  // Both tilings still exist.
  EXPECT_EQ(2.f, active_layer()->tilings()->tiling_at(0)->contents_scale_key());
  EXPECT_EQ(1.f, active_layer()->tilings()->tiling_at(1)->contents_scale_key());

  // All high res tiles have resources.
  std::vector<Tile*> high_tiles =
      active_layer()->HighResTiling()->AllTilesForTesting();
  host_impl()->tile_manager()->InitializeTilesWithResourcesForTesting(
      high_tiles);

  auto render_pass = viz::CompositorRenderPass::Create();
  AppendQuadsData data;
  active_layer()->WillDraw(DRAW_MODE_SOFTWARE, nullptr);
  active_layer()->AppendQuads(render_pass.get(), &data);
  active_layer()->DidDraw(nullptr);

  // All high res tiles drew, and the one ideal res tile drew.
  ASSERT_GT(render_pass->quad_list.size(), 9u);
  EXPECT_EQ(gfx::Rect(0, 0, 99, 99), render_pass->quad_list.front()->rect);
  EXPECT_EQ(gfx::RectF(0.f, 0.f, 99.f, 99.f),
            viz::TileDrawQuad::MaterialCast(render_pass->quad_list.front())
                ->tex_coord_rect);
  EXPECT_EQ(gfx::Rect(99, 0, 100, 99),
            render_pass->quad_list.ElementAt(1)->rect);
  EXPECT_EQ(gfx::RectF(49.5f, 0.f, 50.f, 49.5f),
            viz::TileDrawQuad::MaterialCast(render_pass->quad_list.ElementAt(1))
                ->tex_coord_rect);

  // Neither the high res nor the ideal tiles were considered as incomplete.
  EXPECT_EQ(0, data.num_missing_tiles);
  EXPECT_EQ(0, data.num_incompletely_rastered_tiles);
  EXPECT_EQ(0, data.num_incompletely_recorded_tiles);
  EXPECT_FALSE(active_layer()->only_used_low_res_last_append_quads());
}

TEST_F(LegacySWPictureLayerImplTest, AppendQuadsDataForCheckerboard) {
  host_impl()->AdvanceToNextFrame(base::Milliseconds(1));

  gfx::Size tile_size(100, 100);
  gfx::Size layer_bounds(500, 500);
  gfx::Rect recorded_bounds(0, 0, 150, 150);

  scoped_refptr<FakeRasterSource> pending_raster_source =
      FakeRasterSource::CreatePartiallyFilled(layer_bounds, recorded_bounds);
  SetupPendingTreeWithFixedTileSize(pending_raster_source, tile_size, Region());
  host_impl()
      ->pending_tree()
      ->property_trees()
      ->scroll_tree_mutable()
      .SetScrollingContentsCullRect(pending_layer()->element_id(),
                                    gfx::Rect(0, 0, 120, 120));
  ActivateTree();

  auto render_pass = viz::CompositorRenderPass::Create();
  AppendQuadsData data;
  active_layer()->WillDraw(DRAW_MODE_SOFTWARE, nullptr);
  active_layer()->AppendQuads(render_pass.get(), &data);
  active_layer()->DidDraw(nullptr);

  EXPECT_EQ(recorded_bounds, active_layer()->HighResTiling()->tiling_rect());
  EXPECT_EQ(1u, render_pass->quad_list.size());
  EXPECT_EQ(1, data.num_missing_tiles);
  EXPECT_EQ(0, data.num_incompletely_rastered_tiles);
  EXPECT_EQ(1, data.num_incompletely_recorded_tiles);
  EXPECT_EQ(22500, data.checkerboarded_visible_content_area);
  EXPECT_EQ(8100, data.checkerboarded_needs_record_content_area);
  EXPECT_EQ(22500, data.checkerboarded_needs_raster_content_area);
  EXPECT_TRUE(active_layer()->only_used_low_res_last_append_quads());

  recorded_bounds = gfx::Rect(30, 30, 150, 150);
  SetupPendingTree(
      FakeRasterSource::CreatePartiallyFilled(layer_bounds, recorded_bounds));
  ActivateTree();

  render_pass = viz::CompositorRenderPass::Create();
  active_layer()->WillDraw(DRAW_MODE_SOFTWARE, nullptr);
  data = AppendQuadsData();
  active_layer()->AppendQuads(render_pass.get(), &data);
  active_layer()->DidDraw(nullptr);

  // Tiling rect origin is snapped.
  EXPECT_EQ(gfx::Rect(0, 0, 180, 180),
            active_layer()->HighResTiling()->tiling_rect());
  EXPECT_EQ(1u, render_pass->quad_list.size());
  EXPECT_EQ(1, data.num_missing_tiles);
  EXPECT_EQ(0, data.num_incompletely_rastered_tiles);
  EXPECT_EQ(1, data.num_incompletely_recorded_tiles);
  EXPECT_EQ(32400, data.checkerboarded_visible_content_area);
  EXPECT_EQ(18000, data.checkerboarded_needs_record_content_area);
  EXPECT_EQ(22500, data.checkerboarded_needs_raster_content_area);
  EXPECT_TRUE(active_layer()->only_used_low_res_last_append_quads());

  // Initialize all tiles with resources.
  for (size_t i = 0; i < active_layer()->tilings()->num_tilings(); i++) {
    host_impl()->tile_manager()->InitializeTilesWithResourcesForTesting(
        active_layer()->tilings()->tiling_at(i)->AllTilesForTesting());
  }

  render_pass = viz::CompositorRenderPass::Create();
  active_layer()->WillDraw(DRAW_MODE_SOFTWARE, nullptr);
  data = AppendQuadsData();
  active_layer()->AppendQuads(render_pass.get(), &data);
  active_layer()->DidDraw(nullptr);
  EXPECT_EQ(4u, render_pass->quad_list.size());
  EXPECT_EQ(0, data.num_missing_tiles);
  EXPECT_EQ(0, data.num_incompletely_rastered_tiles);
  EXPECT_EQ(3, data.num_incompletely_recorded_tiles);
  EXPECT_EQ(18000, data.checkerboarded_visible_content_area);
  EXPECT_EQ(18000, data.checkerboarded_needs_record_content_area);
  EXPECT_EQ(0, data.checkerboarded_needs_raster_content_area);
  EXPECT_FALSE(active_layer()->only_used_low_res_last_append_quads());

  // Now the layer is fully recorded.
  host_impl()
      ->active_tree()
      ->property_trees()
      ->scroll_tree_mutable()
      .SetScrollingContentsCullRect(active_layer()->element_id(),
                                    gfx::Rect(layer_bounds));

  render_pass = viz::CompositorRenderPass::Create();
  active_layer()->WillDraw(DRAW_MODE_SOFTWARE, nullptr);
  data = AppendQuadsData();
  active_layer()->AppendQuads(render_pass.get(), &data);
  active_layer()->DidDraw(nullptr);
  EXPECT_EQ(4u, render_pass->quad_list.size());
  EXPECT_EQ(0, data.num_missing_tiles);
  EXPECT_EQ(0, data.num_incompletely_rastered_tiles);
  EXPECT_EQ(0, data.num_incompletely_recorded_tiles);
  EXPECT_EQ(0, data.checkerboarded_visible_content_area);
  EXPECT_EQ(0, data.checkerboarded_needs_record_content_area);
  EXPECT_EQ(0, data.checkerboarded_needs_raster_content_area);
  EXPECT_FALSE(active_layer()->only_used_low_res_last_append_quads());
}

TEST_F(LegacySWPictureLayerImplTest, HighResRequiredWhenActiveAllReady) {
  gfx::Size layer_bounds(400, 400);
  gfx::Size tile_size(100, 100);

  SetupDefaultTreesWithFixedTileSize(layer_bounds, tile_size,
                                     gfx::Rect(layer_bounds));

  active_layer()->SetAllTilesReady();

  // All active tiles ready, so pending can only activate with all high res
  // tiles.
  pending_layer()->HighResTiling()->UpdateAllRequiredStateForTesting();
  EXPECT_FALSE(pending_layer()->LowResTiling());

  AssertAllTilesRequired(pending_layer()->HighResTiling());
}

TEST_F(LegacySWPictureLayerImplTest, HighResRequiredWhenMissingHighResFlagOn) {
  gfx::Size layer_bounds(400, 400);
  gfx::Size tile_size(100, 100);

  // No invalidation.
  SetupDefaultTreesWithFixedTileSize(layer_bounds, tile_size, Region());

  // Verify active tree not ready.
  Tile* some_active_tile =
      active_layer()->HighResTiling()->AllTilesForTesting()[0];
  EXPECT_FALSE(some_active_tile->draw_info().IsReadyToDraw());

  // When high res are required, all tiles in active high res tiling should be
  // required for activation.
  host_impl()->SetRequiresHighResToDraw();

  pending_layer()->HighResTiling()->UpdateAllRequiredStateForTesting();
  EXPECT_FALSE(pending_layer()->LowResTiling());
  active_layer()->HighResTiling()->UpdateAllRequiredStateForTesting();
  active_layer()->LowResTiling()->UpdateAllRequiredStateForTesting();

  EXPECT_TRUE(pending_layer()->HighResTiling()->AllTilesForTesting().empty());
  AssertAllTilesRequired(active_layer()->HighResTiling());
  AssertNoTilesRequired(active_layer()->LowResTiling());
}

TEST_F(LegacySWPictureLayerImplTest, AllHighResRequiredEvenIfNotChanged) {
  gfx::Size layer_bounds(400, 400);
  gfx::Size tile_size(100, 100);

  SetupDefaultTreesWithFixedTileSize(layer_bounds, tile_size, Region());

  Tile* some_active_tile =
      active_layer()->HighResTiling()->AllTilesForTesting()[0];
  EXPECT_FALSE(some_active_tile->draw_info().IsReadyToDraw());

  // Since there are no invalidations, pending tree should have no tiles.
  EXPECT_TRUE(pending_layer()->HighResTiling()->AllTilesForTesting().empty());
  EXPECT_FALSE(pending_layer()->LowResTiling());

  active_layer()->HighResTiling()->UpdateAllRequiredStateForTesting();
  active_layer()->LowResTiling()->UpdateAllRequiredStateForTesting();

  AssertAllTilesRequired(active_layer()->HighResTiling());
  AssertNoTilesRequired(active_layer()->LowResTiling());
}

TEST_F(LegacySWPictureLayerImplTest, DisallowRequiredForActivation) {
  gfx::Size layer_bounds(400, 400);
  gfx::Size tile_size(100, 100);

  SetupDefaultTreesWithFixedTileSize(layer_bounds, tile_size, Region());

  Tile* some_active_tile =
      active_layer()->HighResTiling()->AllTilesForTesting()[0];
  EXPECT_FALSE(some_active_tile->draw_info().IsReadyToDraw());

  EXPECT_TRUE(pending_layer()->HighResTiling()->AllTilesForTesting().empty());
  EXPECT_FALSE(pending_layer()->LowResTiling());
  active_layer()->HighResTiling()->set_can_require_tiles_for_activation(false);
  active_layer()->LowResTiling()->set_can_require_tiles_for_activation(false);
  pending_layer()->HighResTiling()->set_can_require_tiles_for_activation(false);

  // If we disallow required for activation, no tiles can be required.
  active_layer()->HighResTiling()->UpdateAllRequiredStateForTesting();
  active_layer()->LowResTiling()->UpdateAllRequiredStateForTesting();

  AssertNoTilesRequired(active_layer()->HighResTiling());
  AssertNoTilesRequired(active_layer()->LowResTiling());
}

TEST_F(LegacySWPictureLayerImplTest, NothingRequiredIfActiveMissingTiles) {
  gfx::Size layer_bounds(1000, 4000);
  gfx::Size tile_size(100, 100);

  scoped_refptr<FakeRasterSource> pending_raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);
  scoped_refptr<FakeRasterSource> active_raster_source =
      FakeRasterSource::CreatePartiallyFilled(layer_bounds,
                                              gfx::Rect(500, 500));

  SetupTreesWithFixedTileSize(pending_raster_source, active_raster_source,
                              tile_size, Region());

  // Active layer has tilings.
  EXPECT_TRUE(active_layer()->CanHaveTilings());
  EXPECT_EQ(active_layer()->tilings()->num_tilings(), 2u);
  // Simulate scrolling past the end of recorded content on the active layer,
  // where the recordings are so far away that no tiles are created.
  gfx::Rect visible_rect(0, 3000, 1000, 1000);
  active_layer()->HighResTiling()->ComputeTilePriorityRects(
      visible_rect, visible_rect, visible_rect, visible_rect, 1, Occlusion());
  EXPECT_EQ(active_layer()->HighResTiling()->AllTilesForTesting().size(), 0u);

  // Since the active layer has no tiles at all, the pending layer doesn't
  // need content in order to activate.
  pending_layer()->HighResTiling()->UpdateAllRequiredStateForTesting();
  EXPECT_FALSE(pending_layer()->LowResTiling());
  EXPECT_EQ(pending_layer()->HighResTiling()->AllTilesForTesting().size(), 0u);
}

TEST_F(LegacySWPictureLayerImplTest, HighResRequiredIfActiveCantHaveTiles) {
  gfx::Size layer_bounds(400, 400);
  gfx::Size tile_size(100, 100);

  scoped_refptr<FakeRasterSource> pending_raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);
  scoped_refptr<FakeRasterSource> active_raster_source =
      FakeRasterSource::CreateEmpty(layer_bounds);
  SetupTreesWithFixedTileSize(pending_raster_source, active_raster_source,
                              tile_size, Region());

  // Active layer can't have tiles.
  EXPECT_FALSE(active_layer()->CanHaveTilings());

  // All high res tiles required.  This should be considered identical
  // to the case where there is no active layer, to avoid flashing content.
  // This can happen if a layer exists for a while and switches from
  // not being able to have content to having content.
  pending_layer()->HighResTiling()->UpdateAllRequiredStateForTesting();
  EXPECT_FALSE(pending_layer()->LowResTiling());

  AssertAllTilesRequired(pending_layer()->HighResTiling());
}

TEST_F(LegacySWPictureLayerImplTest,
       HighResRequiredWhenActiveHasDifferentBounds) {
  gfx::Size pending_layer_bounds(400, 400);
  gfx::Size active_layer_bounds(200, 200);
  gfx::Size tile_size(100, 100);

  scoped_refptr<FakeRasterSource> pending_raster_source =
      FakeRasterSource::CreateFilled(pending_layer_bounds);
  scoped_refptr<FakeRasterSource> active_raster_source =
      FakeRasterSource::CreateFilled(active_layer_bounds);

  SetupTreesWithFixedTileSize(pending_raster_source, active_raster_source,
                              tile_size, Region());

  // Since the active layer has different bounds, the pending layer needs all
  // high res tiles in order to activate.
  pending_layer()->HighResTiling()->UpdateAllRequiredStateForTesting();
  EXPECT_FALSE(pending_layer()->LowResTiling());
  active_layer()->HighResTiling()->UpdateAllRequiredStateForTesting();
  active_layer()->LowResTiling()->UpdateAllRequiredStateForTesting();

  AssertAllTilesRequired(pending_layer()->HighResTiling());
  AssertAllTilesRequired(active_layer()->HighResTiling());
  AssertNoTilesRequired(active_layer()->LowResTiling());
}

TEST_F(LegacySWPictureLayerImplTest, ActivateUninitializedLayer) {
  gfx::Size layer_bounds(400, 400);
  scoped_refptr<FakeRasterSource> pending_raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);

  host_impl()->CreatePendingTree();
  LayerTreeImpl* pending_tree = host_impl()->pending_tree();

  int kLayerId = 2;
  std::unique_ptr<FakePictureLayerImpl> pending_layer =
      FakePictureLayerImpl::Create(pending_tree, kLayerId,
                                   pending_raster_source);
  pending_layer->SetDrawsContent(true);
  auto* raw_pending_layer = pending_layer.get();
  SetupRootProperties(raw_pending_layer);
  pending_tree->SetRootLayerForTesting(std::move(pending_layer));
  PrepareForUpdateDrawProperties(pending_tree);

  // Set some state on the pending layer, make sure it is not clobbered
  // by a sync from the active layer.  This could happen because if the
  // pending layer has not been post-commit initialized it will attempt
  // to sync from the active layer.
  float raster_page_scale = 10.f * raw_pending_layer->raster_page_scale();
  raw_pending_layer->set_raster_page_scale(raster_page_scale);

  host_impl()->ActivateSyncTree();

  FakePictureLayerImpl* raw_active_layer = static_cast<FakePictureLayerImpl*>(
      host_impl()->active_tree()->LayerById(kLayerId));

  EXPECT_EQ(0u, raw_active_layer->num_tilings());
  EXPECT_EQ(raster_page_scale, raw_active_layer->raster_page_scale());
}

TEST_F(LegacySWPictureLayerImplTest, ShareTilesOnNextFrame) {
  gfx::Size layer_bounds(1500, 1500);

  scoped_refptr<FakeRasterSource> pending_raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);

  SetupPendingTree(pending_raster_source);

  PictureLayerTiling* tiling = pending_layer()->HighResTiling();
  gfx::Rect first_invalidate = tiling->TilingDataForTesting().TileBounds(0, 0);
  first_invalidate.Inset(tiling->TilingDataForTesting().border_texels());
  gfx::Rect second_invalidate = tiling->TilingDataForTesting().TileBounds(1, 1);
  second_invalidate.Inset(tiling->TilingDataForTesting().border_texels());

  ActivateTree();

  // Make a pending tree with an invalidated raster tile 0,0.
  SetupPendingTreeWithInvalidation(pending_raster_source, first_invalidate);

  // Activate and make a pending tree with an invalidated raster tile 1,1.
  ActivateTree();

  SetupPendingTreeWithInvalidation(pending_raster_source, second_invalidate);

  PictureLayerTiling* pending_tiling = pending_layer()->tilings()->tiling_at(0);
  PictureLayerTiling* active_tiling = active_layer()->tilings()->tiling_at(0);

  // Tile 0,0 not exist on pending, but tile 1,1 should.
  EXPECT_TRUE(active_tiling->TileAt(0, 0));
  EXPECT_TRUE(active_tiling->TileAt(1, 0));
  EXPECT_TRUE(active_tiling->TileAt(0, 1));
  EXPECT_FALSE(pending_tiling->TileAt(0, 0));
  EXPECT_FALSE(pending_tiling->TileAt(1, 0));
  EXPECT_FALSE(pending_tiling->TileAt(0, 1));
  EXPECT_NE(active_tiling->TileAt(1, 1), pending_tiling->TileAt(1, 1));
  EXPECT_TRUE(active_tiling->TileAt(1, 1));
  EXPECT_TRUE(pending_tiling->TileAt(1, 1));

  // Drop the tiles on the active tree and recreate them.
  active_layer()->tilings()->UpdateTilePriorities(gfx::Rect(), 1.f, 1.0,
                                                  Occlusion(), true);
  EXPECT_TRUE(active_tiling->AllTilesForTesting().empty());
  active_tiling->CreateAllTilesForTesting();

  // Tile 0,0 not exist on pending, but tile 1,1 should.
  EXPECT_TRUE(active_tiling->TileAt(0, 0));
  EXPECT_TRUE(active_tiling->TileAt(1, 0));
  EXPECT_TRUE(active_tiling->TileAt(0, 1));
  EXPECT_FALSE(pending_tiling->TileAt(0, 0));
  EXPECT_FALSE(pending_tiling->TileAt(1, 0));
  EXPECT_FALSE(pending_tiling->TileAt(0, 1));
  EXPECT_NE(active_tiling->TileAt(1, 1), pending_tiling->TileAt(1, 1));
  EXPECT_TRUE(active_tiling->TileAt(1, 1));
  EXPECT_TRUE(pending_tiling->TileAt(1, 1));
}

TEST_F(LegacySWPictureLayerImplTest, PendingHasNoTilesWithNoInvalidation) {
  SetupDefaultTrees(gfx::Size(1500, 1500));

  EXPECT_GE(active_layer()->num_tilings(), 1u);
  EXPECT_GE(pending_layer()->num_tilings(), 1u);

  // No invalidation.
  PictureLayerTiling* active_tiling = active_layer()->tilings()->tiling_at(0);
  PictureLayerTiling* pending_tiling = pending_layer()->tilings()->tiling_at(0);
  ASSERT_TRUE(active_tiling);
  ASSERT_TRUE(pending_tiling);

  EXPECT_TRUE(active_tiling->TileAt(0, 0));
  EXPECT_TRUE(active_tiling->TileAt(1, 0));
  EXPECT_TRUE(active_tiling->TileAt(0, 1));
  EXPECT_TRUE(active_tiling->TileAt(1, 1));

  EXPECT_FALSE(pending_tiling->TileAt(0, 0));
  EXPECT_FALSE(pending_tiling->TileAt(1, 0));
  EXPECT_FALSE(pending_tiling->TileAt(0, 1));
  EXPECT_FALSE(pending_tiling->TileAt(1, 1));
}

TEST_F(LegacySWPictureLayerImplTest, ShareInvalidActiveTreeTiles) {
  gfx::Size layer_bounds(1500, 1500);

  scoped_refptr<FakeRasterSource> pending_raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);
  scoped_refptr<FakeRasterSource> active_raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);
  SetupTreesWithInvalidation(pending_raster_source, active_raster_source,
                             gfx::Rect(1, 1));
  // Activate the invalidation.
  ActivateTree();
  // Make another pending tree without any invalidation in it.
  scoped_refptr<FakeRasterSource> pending_raster_source2 =
      FakeRasterSource::CreateFilled(layer_bounds);
  SetupPendingTree(pending_raster_source2);

  EXPECT_GE(active_layer()->num_tilings(), 1u);
  EXPECT_GE(pending_layer()->num_tilings(), 1u);

  // The active tree invalidation was handled by the active tiles.
  PictureLayerTiling* active_tiling = active_layer()->tilings()->tiling_at(0);
  PictureLayerTiling* pending_tiling = pending_layer()->tilings()->tiling_at(0);
  ASSERT_TRUE(active_tiling);
  ASSERT_TRUE(pending_tiling);

  EXPECT_TRUE(active_tiling->TileAt(0, 0));
  EXPECT_TRUE(active_tiling->TileAt(1, 0));
  EXPECT_TRUE(active_tiling->TileAt(0, 1));
  EXPECT_TRUE(active_tiling->TileAt(1, 1));

  EXPECT_FALSE(pending_tiling->TileAt(0, 0));
  EXPECT_FALSE(pending_tiling->TileAt(1, 0));
  EXPECT_FALSE(pending_tiling->TileAt(0, 1));
  EXPECT_FALSE(pending_tiling->TileAt(1, 1));
}

TEST_F(LegacySWPictureLayerImplTest, RecreateInvalidPendingTreeTiles) {
  // Set some invalidation on the pending tree. We should replace raster tiles
  // that touch this.
  SetupDefaultTreesWithInvalidation(gfx::Size(1500, 1500), gfx::Rect(1, 1));

  EXPECT_GE(active_layer()->num_tilings(), 1u);
  EXPECT_GE(pending_layer()->num_tilings(), 1u);

  // The pending tree invalidation creates tiles on the pending tree.
  PictureLayerTiling* active_tiling = active_layer()->tilings()->tiling_at(0);
  PictureLayerTiling* pending_tiling = pending_layer()->tilings()->tiling_at(0);
  ASSERT_TRUE(active_tiling);
  ASSERT_TRUE(pending_tiling);

  EXPECT_TRUE(active_tiling->TileAt(0, 0));
  EXPECT_TRUE(active_tiling->TileAt(1, 0));
  EXPECT_TRUE(active_tiling->TileAt(0, 1));
  EXPECT_TRUE(active_tiling->TileAt(1, 1));

  EXPECT_TRUE(pending_tiling->TileAt(0, 0));
  EXPECT_FALSE(pending_tiling->TileAt(1, 0));
  EXPECT_FALSE(pending_tiling->TileAt(0, 1));
  EXPECT_FALSE(pending_tiling->TileAt(1, 1));

  EXPECT_NE(active_tiling->TileAt(0, 0), pending_tiling->TileAt(0, 0));
}

TEST_F(LegacySWPictureLayerImplTest, HighResCreatedWhenBoundsShrink) {
  // Put 0.5 as high res.
  SetInitialDeviceScaleFactor(0.5f);

  scoped_refptr<FakeRasterSource> pending_raster_source =
      FakeRasterSource::CreateFilled(gfx::Size(10, 10));
  SetupPendingTree(pending_raster_source);

  EXPECT_EQ(1u, pending_layer()->tilings()->num_tilings());
  EXPECT_EQ(1, pending_layer()->tilings()->NumHighResTilings());
  auto* high_res = pending_layer()->tilings()->FindTilingWithScaleKey(0.5f);
  ASSERT_TRUE(high_res);
  EXPECT_EQ(HIGH_RESOLUTION, high_res->resolution());

  ActivateTree();

  EXPECT_EQ(1u, active_layer()->tilings()->num_tilings());
  EXPECT_EQ(1, active_layer()->tilings()->NumHighResTilings());
  high_res = active_layer()->tilings()->FindTilingWithScaleKey(0.5f);
  ASSERT_TRUE(high_res);
  EXPECT_EQ(HIGH_RESOLUTION, high_res->resolution());

  // Now, set the bounds to be 1x1, so that minimum contents scale becomes 1.
  pending_raster_source = FakeRasterSource::CreateFilled(gfx::Size(1, 1));
  SetupPendingTree(pending_raster_source);

  EXPECT_EQ(1.f, pending_layer()->MinimumContentsScale());

  // Since the MinContentsScale is 1, the 0.5 tiling should have been replaced
  // by a 1.0 tiling during the UDP in SetupPendingTree.
  EXPECT_EQ(1u, pending_layer()->tilings()->num_tilings());
  high_res = pending_layer()->tilings()->FindTilingWithScaleKey(1.0f);
  ASSERT_TRUE(high_res);
  EXPECT_EQ(HIGH_RESOLUTION, high_res->resolution());

  ActivateTree();
  EXPECT_EQ(1u, active_layer()->tilings()->num_tilings());
  high_res = active_layer()->tilings()->FindTilingWithScaleKey(1.0f);
  ASSERT_TRUE(high_res);
  EXPECT_EQ(HIGH_RESOLUTION, high_res->resolution());
}

// Tests when directly committing to the active tree, a small raster source
// makes the minimum scale bigger than the the previous high res tiling scale,
// causing the high res tiling (if not previously used for quads) to be removed.
// See crbug.com/1160003.
TEST_F(LegacySWPictureLayerImplTest,
       HighResCreatedWhenBoundsShrinkOnActiveLayerWithUsedNonIdealScaleTiling) {
  // Put 0.5 as high res.
  SetInitialDeviceScaleFactor(0.5f);

  SetupPendingTree(FakeRasterSource::CreateFilled(gfx::Size(10, 10)));
  ActivateTree();
  active_layer()
      ->AddTiling(gfx::AxisTransform2d(1.0f, gfx::Vector2dF()))
      ->set_resolution(NON_IDEAL_RESOLUTION);

  EXPECT_EQ(2u, active_layer()->tilings()->num_tilings());
  EXPECT_EQ(1, active_layer()->tilings()->NumHighResTilings());
  auto* high_res = active_layer()->tilings()->FindTilingWithScaleKey(0.5f);
  ASSERT_TRUE(high_res);
  EXPECT_EQ(HIGH_RESOLUTION, high_res->resolution());
  EXPECT_TRUE(active_layer()->tilings()->FindTilingWithScaleKey(1.0f));

  // Now, set the bounds to be 1x1, so that minimum contents scale becomes 1.
  SetupPendingTree(FakeRasterSource::CreateFilled(gfx::Size(1, 1)));
  ActivateTree();
  active_layer()->AddLastAppendQuadsTilingForTesting(
      active_layer()->tilings()->FindTilingWithScaleKey(1.0f));
  active_layer()->UpdateTiles();

  EXPECT_EQ(1.f, active_layer()->MinimumContentsScale());

  // Since the MinContentsScale is 1, the 0.5 tiling should have been replaced
  // by a 1.0 tiling during UpdateRasterSource() and UpdateTiles().
  EXPECT_EQ(1u, active_layer()->tilings()->num_tilings());
  high_res = active_layer()->tilings()->FindTilingWithScaleKey(1.0f);
  ASSERT_TRUE(high_res);
  EXPECT_EQ(HIGH_RESOLUTION, high_res->resolution());
}

TEST_F(LegacySWPictureLayerImplTest, LowResTilingWithoutGpuRasterization) {
  gfx::Size default_tile_size(host_impl()->settings().default_tile_size);
  gfx::Size layer_bounds(default_tile_size.width() * 4,
                         default_tile_size.height() * 4);

  SetupDefaultTrees(layer_bounds);
  EXPECT_FALSE(host_impl()->use_gpu_rasterization());
  // Should have only a high-res tiling.
  EXPECT_EQ(1u, pending_layer()->tilings()->num_tilings());
  ActivateTree();
  // Should add a high and a low res for active tree.
  EXPECT_EQ(2u, active_layer()->tilings()->num_tilings());
}

TEST_F(PictureLayerImplTest, NoLowResTilingWithGpuRasterization) {
  gfx::Size default_tile_size(host_impl()->settings().default_tile_size);
  gfx::Size layer_bounds(default_tile_size.width() * 4,
                         default_tile_size.height() * 4);

  SetupDefaultTrees(layer_bounds);
  EXPECT_TRUE(host_impl()->use_gpu_rasterization());
  // Should only have the high-res tiling.
  EXPECT_EQ(1u, pending_layer()->tilings()->num_tilings());
  ActivateTree();
  // Should only have the high-res tiling.
  EXPECT_EQ(1u, active_layer()->tilings()->num_tilings());
}

TEST_F(PictureLayerImplTest, RequiredTilesWithGpuRasterization) {
  gfx::Size viewport_size(1000, 1000);
  host_impl()->active_tree()->SetDeviceViewportRect(gfx::Rect(viewport_size));

  gfx::Size layer_bounds(4000, 4000);
  SetupDefaultTrees(layer_bounds);
  EXPECT_TRUE(host_impl()->use_gpu_rasterization());

  // Should only have the high-res tiling.
  EXPECT_EQ(1u, active_layer()->tilings()->num_tilings());

  active_layer()->HighResTiling()->UpdateAllRequiredStateForTesting();

  EXPECT_EQ(ALLOW_PREPAINT_ONLY,
            host_impl()->global_tile_state().memory_limit_policy);
  if (!features::IsCCSlimmingEnabled()) {
    // High res tiling should have 128 tiles (4x16 tile grid, plus another
    // factor of 2 for half-width tiles).
    EXPECT_EQ(128u,
              active_layer()->HighResTiling()->AllTilesForTesting().size());
  } else {
    // Due to optimization in PictureLayerTiling::ComputeTilePriorityRects(),
    // since the memory limit policy is ALLOW_PREPAINT_ONLY, |live_tiles_rect|
    // does not include |eventually_rect|.
    EXPECT_EQ(15u,
              active_layer()->HighResTiling()->AllTilesForTesting().size());
  }

  // Visible viewport should be covered by 8 tiles (4 high, half-width.
  // No other tiles should be required for activation.
  EXPECT_EQ(8u, NumberOfTilesRequired(active_layer()->HighResTiling()));
}

TEST_F(CommitToActiveTreePictureLayerImplTest,
       RequiredTilesWithGpuRasterizationAndFractionalDsf) {
  host_impl()->CommitComplete();

  gfx::Size viewport_size(1502, 2560);
  host_impl()->active_tree()->SetDeviceViewportRect(gfx::Rect(viewport_size));

  float dsf = 3.5f;
  gfx::Size layer_bounds = gfx::ScaleToCeiledSize(viewport_size, 1.0f / dsf);
  SetupDefaultTrees(layer_bounds);
  EXPECT_TRUE(host_impl()->use_gpu_rasterization());

  SetContentsScaleOnActiveLayer(dsf /* contents_scale */,
                                dsf /* device_scale_factor */,
                                1.0f /* page_scale_factor */);

  active_layer()->HighResTiling()->UpdateAllRequiredStateForTesting();

  // High res tiling should have 4 tiles (1x4 tile grid).
  EXPECT_EQ(4u, active_layer()->HighResTiling()->AllTilesForTesting().size());
}

TEST_F(LegacySWPictureLayerImplTest, NoTilingIfDoesNotDrawContent) {
  // Set up layers with tilings.
  SetupDefaultTrees(gfx::Size(10, 10));
  SetContentsScaleOnBothLayers(1.f, 1.f, 1.f);
  pending_layer()->PushPropertiesTo(active_layer());
  EXPECT_TRUE(pending_layer()->draws_content());
  EXPECT_TRUE(pending_layer()->CanHaveTilings());
  EXPECT_GE(pending_layer()->num_tilings(), 0u);
  EXPECT_GE(active_layer()->num_tilings(), 0u);

  // Set content to false, which should make CanHaveTilings return false.
  pending_layer()->SetDrawsContent(false);
  EXPECT_FALSE(pending_layer()->draws_content());
  EXPECT_FALSE(pending_layer()->CanHaveTilings());

  // No tilings should be pushed to active layer.
  pending_layer()->PushPropertiesTo(active_layer());
  EXPECT_EQ(0u, active_layer()->num_tilings());
}

TEST_F(LegacySWPictureLayerImplTest, FirstTilingDuringPinch) {
  SetupDefaultTrees(gfx::Size(10, 10));

  // We start with a tiling at scale 1.
  EXPECT_EQ(1.f, pending_layer()->HighResTiling()->contents_scale_key());

  // Since this test simulates a pinch it needs an input handler.
  // TODO(bokan): This is a raster unit test, it shouldn't be using a real
  // input handler.
  InputHandler::Create(static_cast<CompositorDelegateForInput&>(*host_impl()));

  // When we page scale up by 2.3, we get a new tiling that is a power of 2, in
  // this case 4.
  host_impl()->GetInputHandler().PinchGestureBegin(
      gfx::Point(), ui::ScrollInputType::kTouchscreen);
  float high_res_scale = 2.3f;
  SetContentsScaleOnBothLayers(high_res_scale, 1.f, high_res_scale);
  EXPECT_EQ(4.f, pending_layer()->HighResTiling()->contents_scale_key());
}

TEST_F(LegacySWPictureLayerImplTest, PinchingTooSmall) {
  SetupDefaultTrees(gfx::Size(10, 10));

  // We start with a tiling at scale 1.
  EXPECT_EQ(1.f, pending_layer()->HighResTiling()->contents_scale_key());

  // Since this test simulates a pinch it needs an input handler.
  // TODO(bokan): This is a raster unit test, it shouldn't be using a real
  // input handler.
  InputHandler::Create(static_cast<CompositorDelegateForInput&>(*host_impl()));

  host_impl()->GetInputHandler().PinchGestureBegin(
      gfx::Point(), ui::ScrollInputType::kTouchscreen);
  float high_res_scale = 0.0001f;
  EXPECT_LT(high_res_scale, pending_layer()->MinimumContentsScale());

  SetContentsScaleOnBothLayers(high_res_scale, 1.f, high_res_scale);
  EXPECT_FLOAT_EQ(pending_layer()->MinimumContentsScale(),
                  pending_layer()->HighResTiling()->contents_scale_key());
}

TEST_F(LegacySWPictureLayerImplTest, PinchingTooSmallWithContentsScale) {
  SetupDefaultTrees(gfx::Size(10, 10));

  ResetTilingsAndRasterScales();

  float contents_scale = 0.15f;
  SetContentsScaleOnBothLayers(contents_scale, 1.f, 1.f);

  ASSERT_GE(pending_layer()->num_tilings(), 0u);
  EXPECT_FLOAT_EQ(contents_scale,
                  pending_layer()->HighResTiling()->contents_scale_key());

  // Since this test simulates a pinch it needs an input handler.
  // TODO(bokan): This is a raster unit test, it shouldn't be using a real
  // input handler.
  InputHandler::Create(static_cast<CompositorDelegateForInput&>(*host_impl()));

  host_impl()->GetInputHandler().PinchGestureBegin(
      gfx::Point(), ui::ScrollInputType::kTouchscreen);

  float page_scale = 0.0001f;
  EXPECT_LT(page_scale * contents_scale,
            pending_layer()->MinimumContentsScale());

  SetContentsScaleOnBothLayers(contents_scale * page_scale, 1.f, page_scale);
  ASSERT_GE(pending_layer()->num_tilings(), 0u);
  EXPECT_FLOAT_EQ(pending_layer()->MinimumContentsScale(),
                  pending_layer()->HighResTiling()->contents_scale_key());
}

TEST_F(LegacySWPictureLayerImplTest, HighResTilingDuringAnimation) {
  gfx::Size viewport_size(1000, 1000);
  host_impl()->active_tree()->SetDeviceViewportRect(gfx::Rect(viewport_size));

  gfx::Size layer_bounds(100, 100);
  SetupDefaultTrees(layer_bounds);

  float contents_scale = 1.f;
  float device_scale = 1.f;
  float page_scale = 1.f;
  float maximum_animation_scale = 1.f;
  bool affected_by_invalid_scale = false;

  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), 1.f);

  // Starting an animation should cause tiling resolution to get set to the
  // maximum animation scale factor.
  maximum_animation_scale = 3.f;
  contents_scale = 2.f;

  SetContentsAndAnimationScalesOnBothLayers(contents_scale, device_scale,
                                            page_scale, maximum_animation_scale,
                                            affected_by_invalid_scale);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), 3.f);

  // Further changes to scale during the animation should not cause a new
  // high-res tiling to get created.
  contents_scale = 4.f;
  maximum_animation_scale = 5.f;

  SetContentsAndAnimationScalesOnBothLayers(contents_scale, device_scale,
                                            page_scale, maximum_animation_scale,
                                            affected_by_invalid_scale);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), 3.f);

  // Once we stop animating, a new high-res tiling should be created.
  SetContentsScaleOnBothLayers(contents_scale, device_scale, page_scale);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), 4.f);

  // When animating with a maxmium animation scale factor that is so large
  // that the layer grows larger than the viewport at this scale, a new
  // high-res tiling should get created at a source scale that the rasterized
  // layer will not be larger than the viewport, not at its maximum scale.
  contents_scale = 2.f;
  maximum_animation_scale = 11.f;

  SetContentsAndAnimationScalesOnBothLayers(contents_scale, device_scale,
                                            page_scale, maximum_animation_scale,
                                            affected_by_invalid_scale);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), 10.f);

  // Once we stop animating, a new high-res tiling should be created.
  contents_scale = 11.f;

  SetContentsScaleOnBothLayers(contents_scale, device_scale, page_scale);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), 11.f);

  // When animating with a maxmium animation scale factor that is so large
  // that the layer grows larger than the viewport at this scale, and where
  // the initial source scale is < 1, a new high-res tiling should get created
  // at a source scale that the rasterized layer will not be larger than the
  // viewport.
  contents_scale = 0.1f;
  maximum_animation_scale = 12.f;

  SetContentsAndAnimationScalesOnBothLayers(contents_scale, device_scale,
                                            page_scale, maximum_animation_scale,
                                            affected_by_invalid_scale);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), 10.f);

  // Once we stop animating, a new high-res tiling should be created.
  contents_scale = 12.f;

  SetContentsScaleOnBothLayers(contents_scale, device_scale, page_scale);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), 12.f);

  // When animating toward a smaller scale, but that is still so large that the
  // layer grows larger than the viewport at this scale, a new high-res tiling
  // should get created at a source scale that the rasterized layer is not
  // larger than the viewport.
  contents_scale = 11.f;
  maximum_animation_scale = 11.f;

  SetContentsAndAnimationScalesOnBothLayers(contents_scale, device_scale,
                                            page_scale, maximum_animation_scale,
                                            affected_by_invalid_scale);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), 10.f);

  // Once we stop animating, a new high-res tiling should be created.
  contents_scale = 11.f;

  SetContentsScaleOnBothLayers(contents_scale, device_scale, page_scale);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), 11.f);
}

TEST_F(LegacySWPictureLayerImplTest, HighResTilingDuringAnimationWideLayer) {
  gfx::Size viewport_size(2048, 2048);
  host_impl()->active_tree()->SetDeviceViewportRect(gfx::Rect(viewport_size));

  gfx::Size layer_bounds(1000000, 32);
  SetupDefaultTrees(layer_bounds);

  float contents_scale = 1.f;
  float device_scale = 1.f;
  float page_scale = 1.f;
  float maximum_animation_scale = 1.f;
  bool affected_by_invalid_scale = false;

  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), 1.f);

  // Starting an animation should cause tiling resolution to get set to the
  // maximum animation scale factor.
  maximum_animation_scale = 3.f;
  contents_scale = 2.f;

  SetContentsAndAnimationScalesOnBothLayers(contents_scale, device_scale,
                                            page_scale, maximum_animation_scale,
                                            affected_by_invalid_scale);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), 3.f);

  // Further changes to scale during the animation should not cause a new
  // high-res tiling to get created.
  contents_scale = 4.f;
  maximum_animation_scale = 5.f;

  SetContentsAndAnimationScalesOnBothLayers(contents_scale, device_scale,
                                            page_scale, maximum_animation_scale,
                                            affected_by_invalid_scale);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), 3.f);

  // Once we stop animating, a new high-res tiling should be created.
  SetContentsScaleOnBothLayers(contents_scale, device_scale, page_scale);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), 4.f);

  // When animating with a maxmium animation scale factor that is so large
  // that the layer grows larger than the viewport at this scale, a new high-res
  // tiling should get created at a source scale that the rasterized visible
  // rect will not be larger than the viewport, not at its maximum scale.
  contents_scale = 2.f;
  maximum_animation_scale = 11.f;

  SetContentsAndAnimationScalesOnBothLayers(contents_scale, device_scale,
                                            page_scale, maximum_animation_scale,
                                            affected_by_invalid_scale);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), 8.f);

  // Once we stop animating, a new high-res tiling should be created.
  contents_scale = 11.f;

  SetContentsScaleOnBothLayers(contents_scale, device_scale, page_scale);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), 11.f);

  // When animating with a maxmium animation scale factor that is so large
  // that the layer grows larger than the viewport at this scale, and where
  // the initial source scale is < 1, a new high-res tiling should get created
  // at a source scale that the rasterized visible rect will not be larger than
  // the viewport.
  contents_scale = 0.1f;
  maximum_animation_scale = 12.f;

  SetContentsAndAnimationScalesOnBothLayers(contents_scale, device_scale,
                                            page_scale, maximum_animation_scale,
                                            affected_by_invalid_scale);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), 8.f);

  // Once we stop animating, a new high-res tiling should be created.
  contents_scale = 12.f;

  SetContentsScaleOnBothLayers(contents_scale, device_scale, page_scale);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), 12.f);

  // When animating toward a smaller scale, but that is still so large that the
  // layer grows larger than the viewport at this scale, a new high-res tiling
  // should get created at a source scale that the rasterized visible rect is
  // not larger than the viewport.
  contents_scale = 11.f;
  maximum_animation_scale = 11.f;

  SetContentsAndAnimationScalesOnBothLayers(contents_scale, device_scale,
                                            page_scale, maximum_animation_scale,
                                            affected_by_invalid_scale);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), 8.f);

  // Once we stop animating, a new high-res tiling should be created.
  contents_scale = 11.f;

  SetContentsScaleOnBothLayers(contents_scale, device_scale, page_scale);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), 11.f);
}

TEST_F(LegacySWPictureLayerImplTest,
       HighResTilingDuringAnimationSmallerAnimationScale) {
  gfx::Size viewport_size(1000, 1000);
  host_impl()->active_tree()->SetDeviceViewportRect(gfx::Rect(viewport_size));

  gfx::Size layer_bounds(100, 100);
  SetupDefaultTrees(layer_bounds);

  float contents_scale = 1.f;
  float device_scale = 1.f;
  float page_scale = 1.f;
  float maximum_animation_scale = 1.f;
  bool affected_by_invalid_scale = false;

  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), 1.f);

  // When animating with smaller animation scale factors (i.e. not accurately
  // calculated because some limitations, e.g. nested scales), a new high-res
  // tiling should be created at the current contents scale.
  contents_scale = 0.5f;
  maximum_animation_scale = 0.4f;

  SetContentsAndAnimationScalesOnBothLayers(contents_scale, device_scale,
                                            page_scale, maximum_animation_scale,
                                            affected_by_invalid_scale);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), 0.5f);

  // Increase the contents scale a bit, and the current high-res tiling should
  // still be used.
  contents_scale = 0.6f;
  SetContentsAndAnimationScalesOnBothLayers(contents_scale, device_scale,
                                            page_scale, maximum_animation_scale,
                                            affected_by_invalid_scale);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), 0.5f);

  // A new high-res tiling should be created at the current contents scale if
  // its >1.5x of the current high-res scale.
  contents_scale = 0.8f;
  SetContentsAndAnimationScalesOnBothLayers(contents_scale, device_scale,
                                            page_scale, maximum_animation_scale,
                                            affected_by_invalid_scale);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), 0.8f);

  // Reduce the contents scale, and the current high-res tiling should still be
  // used.
  contents_scale = 0.4f;
  SetContentsAndAnimationScalesOnBothLayers(contents_scale, device_scale,
                                            page_scale, maximum_animation_scale,
                                            affected_by_invalid_scale);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), 0.8f);
}

TEST_F(LegacySWPictureLayerImplTest,
       HighResTilingDuringAnimationSmallerAnimationScaleWithInvalidScale) {
  gfx::Size viewport_size(1000, 1000);
  host_impl()->active_tree()->SetDeviceViewportRect(gfx::Rect(viewport_size));

  gfx::Size layer_bounds(100, 100);
  SetupDefaultTrees(layer_bounds);

  float contents_scale = 1.f;
  float device_scale = 1.f;
  float page_scale = 1.f;
  float maximum_animation_scale = 1.f;
  bool affected_by_invalid_scale = true;

  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), 1.f);

  // When animating with smaller animation scale factors (i.e. not accurately
  // calculated because some limitations, e.g. nested scales), a new high-res
  // tiling should be created at the current contents scale.
  contents_scale = 0.5f;
  maximum_animation_scale = 0.4f;

  SetContentsAndAnimationScalesOnBothLayers(contents_scale, device_scale,
                                            page_scale, maximum_animation_scale,
                                            affected_by_invalid_scale);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), 0.5f);

  // Increase the contents scale a bit, and the current high-res tiling should
  // still be used.
  contents_scale = 0.6f;
  SetContentsAndAnimationScalesOnBothLayers(contents_scale, device_scale,
                                            page_scale, maximum_animation_scale,
                                            affected_by_invalid_scale);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), 0.5f);

  // Raster scale change during animation should be avoided.
  contents_scale = 1.2f;
  SetContentsAndAnimationScalesOnBothLayers(contents_scale, device_scale,
                                            page_scale, maximum_animation_scale,
                                            affected_by_invalid_scale);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), 0.5f);

  // Raster scale change during animation should be avoided.
  contents_scale = 0.4f;
  SetContentsAndAnimationScalesOnBothLayers(contents_scale, device_scale,
                                            page_scale, maximum_animation_scale,
                                            affected_by_invalid_scale);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), 0.5f);

  // Should update raster scale if the cache is invalidated (simulating that a
  // new property tree is pushed).
  contents_scale = 1.2f;
  SetMaximumAnimationToScreenScale(pending_layer(), maximum_animation_scale,
                                   affected_by_invalid_scale);
  host_impl()->pending_tree()->property_trees()->ResetCachedData();
  SetupDrawPropertiesAndUpdateTiles(pending_layer(), contents_scale,
                                    device_scale, page_scale);
  SetMaximumAnimationToScreenScale(active_layer(), maximum_animation_scale,
                                   affected_by_invalid_scale);
  host_impl()->active_tree()->property_trees()->ResetCachedData();
  SetupDrawPropertiesAndUpdateTiles(active_layer(), contents_scale,
                                    device_scale, page_scale);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), 1.2f);
}

TEST_F(LegacySWPictureLayerImplTest, ViewportSizeChangeDuringAnimation) {
  gfx::Size layer_bounds(100, 100);
  SetupDefaultTrees(layer_bounds);

  host_impl()->pending_tree()->SetDeviceViewportRect(gfx::Rect());

  EXPECT_EQ(pending_layer()->HighResTiling()->contents_scale_key(), 1.f);

  float maximum_animation_scale = 20.f;
  // This flag should be ignored on viewport size change.
  bool affected_by_invalid_scale = true;

  // Starting an animation should cause tiling resolution to get set to the
  // maximum animation scale factor, clamped by the viewport size (using default
  // minimum 500x500 as the viewport is empty for now).
  SetMaximumAnimationToScreenScale(pending_layer(), maximum_animation_scale,
                                   affected_by_invalid_scale);
  pending_layer()->UpdateTiles();
  EXPECT_EQ(pending_layer()->HighResTiling()->contents_scale_key(), 5.f);

  // Setting viewport rect smaller than the minimum won't change raster scale.
  host_impl()->pending_tree()->SetDeviceViewportRect(gfx::Rect(400, 400));
  SetMaximumAnimationToScreenScale(pending_layer(), maximum_animation_scale,
                                   affected_by_invalid_scale);
  pending_layer()->UpdateTiles();
  EXPECT_EQ(pending_layer()->HighResTiling()->contents_scale_key(), 5.f);

  // For a larger viewport size, the clamped scale is also larger.
  host_impl()->pending_tree()->SetDeviceViewportRect(gfx::Rect(1000, 200));
  SetMaximumAnimationToScreenScale(pending_layer(), maximum_animation_scale,
                                   affected_by_invalid_scale);
  pending_layer()->UpdateTiles();
  EXPECT_EQ(pending_layer()->HighResTiling()->contents_scale_key(), 10.f);
}

TEST_F(LegacySWPictureLayerImplTest,
       AnimationTilingChangesWithWillChangeTransformHint) {
  gfx::Size viewport_size(1000, 1000);
  host_impl()->active_tree()->SetDeviceViewportRect(gfx::Rect(viewport_size));

  gfx::Size layer_bounds(100, 100);
  SetupDefaultTrees(layer_bounds);

  float contents_scale = 1.f;
  float device_scale = 1.f;
  float page_scale = 1.f;
  float maximum_animation_scale = 1.f;
  bool affected_by_invalid_scale = false;

  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), 1.f);

  SetWillChangeTransform(active_layer(), true);
  SetWillChangeTransform(pending_layer(), true);

  // Starting an animation should cause tiling resolution to get set to the
  // maximum animation scale factor.
  maximum_animation_scale = 2.f;
  contents_scale = 1.f;

  SetContentsAndAnimationScalesOnBothLayers(contents_scale, device_scale,
                                            page_scale, maximum_animation_scale,
                                            affected_by_invalid_scale);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), 2.f);

  // Once we stop animating, because we have a will-change: transform hint
  // we should not reset the scale factor.
  SetContentsScaleOnBothLayers(contents_scale, device_scale, page_scale);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), 2.f);

  // Starting an animation with a different maximum animation scale should
  // not cause a change either.
  maximum_animation_scale = 1.5f;

  SetContentsAndAnimationScalesOnBothLayers(contents_scale, device_scale,
                                            page_scale, maximum_animation_scale,
                                            affected_by_invalid_scale);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), 2.f);

  // Again, stop animating, because we have a will-change: transform hint
  // we should not reset the scale factor.
  SetContentsScaleOnBothLayers(contents_scale, device_scale, page_scale);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), 2.f);

  // Test that will-change:transform on an ancestor has the same
  // effects.  We happen to have the page scale layer as an ancestor, so
  // just use that.
  maximum_animation_scale = 2.f;
  LayerImpl* active_page_scale_layer =
      host_impl()->active_tree()->LayerById(active_layer()->id() - 1);
  LayerImpl* pending_page_scale_layer =
      host_impl()->pending_tree()->LayerById(pending_layer()->id() - 1);
  DCHECK_EQ(GetTransformNode(active_page_scale_layer)->id,
            GetTransformNode(active_layer())->parent_id);
  DCHECK_EQ(GetTransformNode(pending_page_scale_layer)->id,
            GetTransformNode(pending_layer())->parent_id);
  SetWillChangeTransform(active_layer(), false);
  SetWillChangeTransform(pending_layer(), false);
  SetContentsScaleOnBothLayers(contents_scale * 2.f, device_scale, page_scale);
  SetContentsScaleOnBothLayers(contents_scale, device_scale, page_scale);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), 1.f);
  SetContentsAndAnimationScalesOnBothLayers(contents_scale, device_scale,
                                            page_scale, maximum_animation_scale,
                                            affected_by_invalid_scale);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), 2.f);
  SetContentsScaleOnBothLayers(contents_scale, device_scale, page_scale);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), 1.f);
  SetWillChangeTransform(active_page_scale_layer, true);
  SetWillChangeTransform(pending_page_scale_layer, true);
  // re-set the false so node_or_ancestors_will_change_transform is recomputed
  SetWillChangeTransform(active_layer(), false);
  SetWillChangeTransform(pending_layer(), false);
  SetContentsAndAnimationScalesOnBothLayers(contents_scale, device_scale,
                                            page_scale, maximum_animation_scale,
                                            affected_by_invalid_scale);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), 2.f);
  SetContentsScaleOnBothLayers(contents_scale, device_scale, page_scale);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), 2.f);
}

TEST_F(LegacySWPictureLayerImplTest, HighResTilingDuringAnimationAspectRatio) {
  gfx::Size viewport_size(2000, 1000);
  host_impl()->active_tree()->SetDeviceViewportRect(gfx::Rect(viewport_size));

  gfx::Size layer_bounds(100, 100);
  SetupDefaultTrees(layer_bounds);

  float contents_scale = 1.f;
  float device_scale = 1.f;
  float page_scale = 1.f;
  float maximum_animation_scale = 1.f;
  bool affected_by_invalid_scale = false;

  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), 1.f);

  // Allow rastering at maximum scale if the animation size is smaller than
  // the square of the maximum viewport dimension.
  contents_scale = 2.f;
  maximum_animation_scale = 15.f;

  SetContentsAndAnimationScalesOnBothLayers(contents_scale, device_scale,
                                            page_scale, maximum_animation_scale,
                                            affected_by_invalid_scale);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), 15.f);
}

TEST_F(LegacySWPictureLayerImplTest,
       HighResTilingDuringAnimationAspectRatioTooLarge) {
  gfx::Size viewport_size(2000, 1000);
  host_impl()->active_tree()->SetDeviceViewportRect(gfx::Rect(viewport_size));

  gfx::Size layer_bounds(100, 100);
  SetupDefaultTrees(layer_bounds);

  float contents_scale = 1.f;
  float device_scale = 1.f;
  float page_scale = 1.f;
  float maximum_animation_scale = 1.f;
  bool affected_by_invalid_scale = false;

  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), 1.f);

  // The maximum animation scale exceeds the squared size of the maximum
  // viewport dimension, so raster scale should be shrunk to make the
  // rasterized layer not larger than the viewport.
  maximum_animation_scale = 21.f;

  SetContentsAndAnimationScalesOnBothLayers(contents_scale, device_scale,
                                            page_scale, maximum_animation_scale,
                                            affected_by_invalid_scale);
  EXPECT_NEAR(20.f, pending_layer()->HighResTiling()->contents_scale_key(),
              0.001f);
  EXPECT_NEAR(20.f, active_layer()->HighResTiling()->contents_scale_key(),
              0.001f);

  // The clamping logic still works with will-change:transform.
  // Raster source size change forces adjustment of raster scale.
  SetWillChangeTransform(active_layer(), true);
  SetWillChangeTransform(pending_layer(), true);
  layer_bounds = gfx::Size(200, 200);
  Region invalidation;
  // UpdateRasterSource() requires that the pending tree doesn't have tiles.
  pending_layer()->picture_layer_tiling_set()->RemoveAllTiles();
  pending_layer()->SetBounds(layer_bounds);
  pending_layer()->UpdateRasterSource(
      FakeRasterSource::CreateFilled(layer_bounds), &invalidation);
  pending_layer()->PushPropertiesTo(active_layer());
  SetContentsAndAnimationScalesOnBothLayers(contents_scale, device_scale,
                                            page_scale, maximum_animation_scale,
                                            affected_by_invalid_scale);
  EXPECT_NEAR(10.f, pending_layer()->HighResTiling()->contents_scale_key(),
              0.001f);
  EXPECT_NEAR(10.f, active_layer()->HighResTiling()->contents_scale_key(),
              0.001f);
}

TEST_F(LegacySWPictureLayerImplTest, TilingSetRasterQueue) {
  host_impl()->AdvanceToNextFrame(base::Milliseconds(1));

  host_impl()->active_tree()->SetDeviceViewportRect(gfx::Rect(500, 500));

  gfx::Size layer_bounds(1000, 1000);

  scoped_refptr<FakeRasterSource> pending_raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);

  SetupPendingTree(pending_raster_source);
  EXPECT_EQ(1u, pending_layer()->num_tilings());

  std::set<Tile*> unique_tiles;
  bool reached_prepaint = false;
  int non_ideal_tile_count = 0u;
  int low_res_tile_count = 0u;
  int high_res_tile_count = 0u;
  int high_res_now_tiles = 0u;
  std::unique_ptr<TilingSetRasterQueueAll> queue =
      TilingSetRasterQueueAll::Create(
          pending_layer()->picture_layer_tiling_set(), false, false);
  EXPECT_TRUE(queue);
  while (!queue->IsEmpty()) {
    PrioritizedTile prioritized_tile = queue->Top();
    TilePriority priority = prioritized_tile.priority();

    EXPECT_TRUE(prioritized_tile.tile());

    // Non-high res tiles only get visible tiles. Also, prepaint should only
    // come at the end of the iteration.
    if (priority.resolution != HIGH_RESOLUTION) {
      EXPECT_EQ(TilePriority::NOW, priority.priority_bin);
    } else if (reached_prepaint) {
      EXPECT_NE(TilePriority::NOW, priority.priority_bin);
    } else {
      reached_prepaint = priority.priority_bin != TilePriority::NOW;
      if (!reached_prepaint)
        ++high_res_now_tiles;
    }

    non_ideal_tile_count += priority.resolution == NON_IDEAL_RESOLUTION;
    low_res_tile_count += priority.resolution == LOW_RESOLUTION;
    high_res_tile_count += priority.resolution == HIGH_RESOLUTION;

    unique_tiles.insert(prioritized_tile.tile());
    queue->Pop();
  }

  EXPECT_TRUE(reached_prepaint);
  EXPECT_EQ(0, non_ideal_tile_count);
  EXPECT_EQ(0, low_res_tile_count);

  // With layer size being 1000x1000 and default tile size 256x256, we expect to
  // see 4 now tiles out of 16 total high res tiles.
  EXPECT_EQ(16, high_res_tile_count);
  EXPECT_EQ(4, high_res_now_tiles);
  EXPECT_EQ(low_res_tile_count + high_res_tile_count + non_ideal_tile_count,
            static_cast<int>(unique_tiles.size()));

  std::unique_ptr<TilingSetRasterQueueRequired> required_queue =
      TilingSetRasterQueueRequired::Create(
          pending_layer()->picture_layer_tiling_set(),
          RasterTilePriorityQueue::Type::REQUIRED_FOR_DRAW);
  EXPECT_TRUE(required_queue);
  EXPECT_TRUE(required_queue->IsEmpty());

  required_queue = TilingSetRasterQueueRequired::Create(
      pending_layer()->picture_layer_tiling_set(),
      RasterTilePriorityQueue::Type::REQUIRED_FOR_ACTIVATION);
  EXPECT_TRUE(required_queue);
  EXPECT_FALSE(required_queue->IsEmpty());
  int required_for_activation_count = 0;
  while (!required_queue->IsEmpty()) {
    PrioritizedTile prioritized_tile = required_queue->Top();
    EXPECT_TRUE(prioritized_tile.tile()->required_for_activation());
    EXPECT_FALSE(prioritized_tile.tile()->draw_info().IsReadyToDraw());
    ++required_for_activation_count;
    required_queue->Pop();
  }

  // All of the high res tiles should be required for activation, since there is
  // no active twin.
  EXPECT_EQ(high_res_now_tiles, required_for_activation_count);

  // No NOW tiles.
  host_impl()->AdvanceToNextFrame(base::Milliseconds(200));

  pending_layer()->draw_properties().visible_layer_rect =
      gfx::Rect(1100, 1100, 500, 500);
  pending_layer()->UpdateTiles();

  unique_tiles.clear();
  high_res_tile_count = 0u;
  queue = TilingSetRasterQueueAll::Create(
      pending_layer()->picture_layer_tiling_set(), false, false);
  EXPECT_TRUE(queue);
  while (!queue->IsEmpty()) {
    PrioritizedTile prioritized_tile = queue->Top();
    TilePriority priority = prioritized_tile.priority();

    EXPECT_TRUE(prioritized_tile.tile());

    // Non-high res tiles only get visible tiles.
    EXPECT_EQ(HIGH_RESOLUTION, priority.resolution);
    EXPECT_NE(TilePriority::NOW, priority.priority_bin);

    high_res_tile_count += priority.resolution == HIGH_RESOLUTION;

    unique_tiles.insert(prioritized_tile.tile());
    queue->Pop();
  }

  EXPECT_EQ(16, high_res_tile_count);
  EXPECT_EQ(high_res_tile_count, static_cast<int>(unique_tiles.size()));

  host_impl()->AdvanceToNextFrame(base::Milliseconds(200));

  pending_layer()->draw_properties().visible_layer_rect =
      gfx::Rect(0, 0, 500, 500);
  pending_layer()->UpdateTiles();

  std::vector<Tile*> high_res_tiles =
      pending_layer()->HighResTiling()->AllTilesForTesting();
  for (auto tile_it = high_res_tiles.begin(); tile_it != high_res_tiles.end();
       ++tile_it) {
    Tile* tile = *tile_it;
    TileDrawInfo& draw_info = tile->draw_info();
    draw_info.SetSolidColorForTesting(SkColors::kRed);
  }

  queue = TilingSetRasterQueueAll::Create(
      pending_layer()->picture_layer_tiling_set(), true, false);
  EXPECT_TRUE(queue);
  EXPECT_TRUE(queue->IsEmpty());
}

TEST_F(LegacySWPictureLayerImplTest, TilingSetRasterQueueActiveTree) {
  host_impl()->AdvanceToNextFrame(base::Milliseconds(1));

  host_impl()->active_tree()->SetDeviceViewportRect(gfx::Rect(500, 500));

  gfx::Size layer_bounds(1000, 1000);

  scoped_refptr<FakeRasterSource> pending_raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);

  SetupPendingTree(pending_raster_source);
  ActivateTree();
  EXPECT_EQ(2u, active_layer()->num_tilings());

  std::unique_ptr<TilingSetRasterQueueRequired> queue =
      TilingSetRasterQueueRequired::Create(
          active_layer()->picture_layer_tiling_set(),
          RasterTilePriorityQueue::Type::REQUIRED_FOR_DRAW);
  EXPECT_TRUE(queue);
  EXPECT_FALSE(queue->IsEmpty());
  while (!queue->IsEmpty()) {
    PrioritizedTile prioritized_tile = queue->Top();
    EXPECT_TRUE(prioritized_tile.tile()->required_for_draw());
    EXPECT_FALSE(prioritized_tile.tile()->draw_info().IsReadyToDraw());
    queue->Pop();
  }

  queue = TilingSetRasterQueueRequired::Create(
      active_layer()->picture_layer_tiling_set(),
      RasterTilePriorityQueue::Type::REQUIRED_FOR_ACTIVATION);
  if (features::IsCCSlimmingEnabled()) {
    EXPECT_FALSE(queue);
  } else {
    EXPECT_TRUE(queue);
    EXPECT_TRUE(queue->IsEmpty());
  }
}

TEST_F(LegacySWPictureLayerImplTest, TilingSetRasterQueueRequiredNoHighRes) {
  scoped_refptr<FakeRasterSource> pending_raster_source =
      FakeRasterSource::CreateFilledSolidColor(gfx::Size(1024, 1024));

  SetupPendingTree(pending_raster_source);
  EXPECT_FALSE(
      pending_layer()->picture_layer_tiling_set()->FindTilingWithResolution(
          HIGH_RESOLUTION));

  std::unique_ptr<TilingSetRasterQueueRequired> queue =
      TilingSetRasterQueueRequired::Create(
          pending_layer()->picture_layer_tiling_set(),
          RasterTilePriorityQueue::Type::REQUIRED_FOR_ACTIVATION);
  if (features::IsCCSlimmingEnabled()) {
    EXPECT_FALSE(queue);
  } else {
    EXPECT_TRUE(queue);
    EXPECT_TRUE(queue->IsEmpty());
  }
}

TEST_F(LegacySWPictureLayerImplTest, TilingSetEvictionQueue) {
  gfx::Size layer_bounds(1000, 1000);
  float low_res_factor = host_impl()->settings().low_res_contents_scale_factor;

  host_impl()->active_tree()->SetDeviceViewportRect(gfx::Rect(500, 500));

  scoped_refptr<FakeRasterSource> pending_raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);

  // TODO(vmpstr): Add a test with tilings other than high res on the active
  // tree (crbug.com/519607).
  SetupPendingTree(pending_raster_source);
  EXPECT_EQ(1u, pending_layer()->num_tilings());

  std::vector<Tile*> all_tiles;
  for (size_t i = 0; i < pending_layer()->num_tilings(); ++i) {
    PictureLayerTiling* tiling = pending_layer()->tilings()->tiling_at(i);
    std::vector<Tile*> tiles = tiling->AllTilesForTesting();
    all_tiles.insert(all_tiles.end(), tiles.begin(), tiles.end());
  }

  std::set<Tile*> all_tiles_set(all_tiles.begin(), all_tiles.end());

  bool mark_required = false;
  size_t number_of_marked_tiles = 0u;
  size_t number_of_unmarked_tiles = 0u;
  for (size_t i = 0; i < pending_layer()->num_tilings(); ++i) {
    PictureLayerTiling* tiling = pending_layer()->tilings()->tiling_at(i);
    for (PictureLayerTiling::CoverageIterator iter(
             tiling, 1.f, pending_layer()->visible_layer_rect());
         iter; ++iter) {
      if (mark_required) {
        number_of_marked_tiles++;
        iter->set_required_for_activation(true);
      } else {
        number_of_unmarked_tiles++;
      }
      mark_required = !mark_required;
    }
  }

  // Sanity checks.
  EXPECT_EQ(16u, all_tiles.size());
  EXPECT_EQ(16u, all_tiles_set.size());
  EXPECT_GT(number_of_marked_tiles, 1u);
  EXPECT_GT(number_of_unmarked_tiles, 1u);

  // Tiles don't have resources yet.
  std::unique_ptr<TilingSetEvictionQueue> queue(new TilingSetEvictionQueue(
      pending_layer()->picture_layer_tiling_set(),
      pending_layer()->contributes_to_drawn_render_surface()));
  EXPECT_TRUE(queue->IsEmpty());

  host_impl()->tile_manager()->InitializeTilesWithResourcesForTesting(
      all_tiles);

  std::set<Tile*> unique_tiles;
  float expected_scales[] = {low_res_factor, 1.f};
  size_t scale_index = 0;
  bool reached_visible = false;
  PrioritizedTile last_tile;
  size_t distance_decreasing = 0;
  size_t distance_increasing = 0;
  queue = std::make_unique<TilingSetEvictionQueue>(
      pending_layer()->picture_layer_tiling_set(),
      pending_layer()->contributes_to_drawn_render_surface());
  while (!queue->IsEmpty()) {
    PrioritizedTile prioritized_tile = queue->Top();
    Tile* tile = prioritized_tile.tile();
    if (!last_tile.tile())
      last_tile = prioritized_tile;

    EXPECT_TRUE(tile);

    TilePriority priority = prioritized_tile.priority();

    if (priority.priority_bin == TilePriority::NOW) {
      reached_visible = true;
      last_tile = prioritized_tile;
      break;
    }

    EXPECT_FALSE(tile->required_for_activation());

    while (std::abs(tile->contents_scale_key() - expected_scales[scale_index]) >
           std::numeric_limits<float>::epsilon()) {
      ++scale_index;
      ASSERT_LT(scale_index, std::size(expected_scales));
    }

    EXPECT_FLOAT_EQ(tile->contents_scale_key(), expected_scales[scale_index]);
    unique_tiles.insert(tile);

    if (tile->required_for_activation() ==
            last_tile.tile()->required_for_activation() &&
        std::abs(tile->contents_scale_key() -
                 last_tile.tile()->contents_scale_key()) <
            std::numeric_limits<float>::epsilon()) {
      if (priority.distance_to_visible <=
          last_tile.priority().distance_to_visible)
        ++distance_decreasing;
      else
        ++distance_increasing;
    }

    last_tile = prioritized_tile;
    queue->Pop();
  }

  // 4 high res tiles are inside the viewport, the rest are evicted.
  EXPECT_TRUE(reached_visible);
  EXPECT_EQ(12u, unique_tiles.size());
  EXPECT_EQ(1u, distance_increasing);
  EXPECT_EQ(11u, distance_decreasing);

  scale_index = 0;
  bool reached_required = false;
  while (!queue->IsEmpty()) {
    PrioritizedTile prioritized_tile = queue->Top();
    Tile* tile = prioritized_tile.tile();
    EXPECT_TRUE(tile);

    TilePriority priority = prioritized_tile.priority();
    EXPECT_EQ(TilePriority::NOW, priority.priority_bin);

    if (reached_required) {
      EXPECT_TRUE(tile->required_for_activation());
    } else if (tile->required_for_activation()) {
      reached_required = true;
      scale_index = 0;
    }

    while (std::abs(tile->contents_scale_key() - expected_scales[scale_index]) >
           std::numeric_limits<float>::epsilon()) {
      ++scale_index;
      ASSERT_LT(scale_index, std::size(expected_scales));
    }

    EXPECT_FLOAT_EQ(tile->contents_scale_key(), expected_scales[scale_index]);
    unique_tiles.insert(tile);
    queue->Pop();
  }

  EXPECT_TRUE(reached_required);
  EXPECT_EQ(all_tiles_set.size(), unique_tiles.size());
}

TEST_F(LegacySWPictureLayerImplTest, Occlusion) {
  gfx::Size tile_size(102, 102);
  gfx::Size layer_bounds(1000, 1000);
  gfx::Size viewport_size(1000, 1000);

  LayerTreeImplTestBase impl;
  host_impl()->active_tree()->SetDeviceViewportRect(gfx::Rect(viewport_size));

  scoped_refptr<FakeRasterSource> pending_raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);
  SetupPendingTreeWithFixedTileSize(pending_raster_source, tile_size, Region());
  ActivateTree();

  std::vector<Tile*> tiles =
      active_layer()->HighResTiling()->AllTilesForTesting();
  host_impl()->tile_manager()->InitializeTilesWithResourcesForTesting(tiles);

  {
    SCOPED_TRACE("No occlusion");
    gfx::Rect occluded;
    impl.AppendQuadsWithOcclusion(active_layer(), occluded);

    VerifyQuadsExactlyCoverRect(impl.quad_list(), gfx::Rect(layer_bounds));
    EXPECT_EQ(100u, impl.quad_list().size());
  }

  {
    SCOPED_TRACE("Full occlusion");
    gfx::Rect occluded(active_layer()->visible_layer_rect());
    impl.AppendQuadsWithOcclusion(active_layer(), occluded);

    VerifyQuadsExactlyCoverRect(impl.quad_list(), gfx::Rect());
    EXPECT_EQ(impl.quad_list().size(), 0u);
  }

  {
    SCOPED_TRACE("Partial occlusion");
    gfx::Rect occluded(150, 0, 200, 1000);
    impl.AppendQuadsWithOcclusion(active_layer(), occluded);

    size_t partially_occluded_count = 0;
    VerifyQuadsAreOccluded(impl.quad_list(), occluded,
                           &partially_occluded_count);
    // The layer outputs one quad, which is partially occluded.
    EXPECT_EQ(100u - 10u, impl.quad_list().size());
    EXPECT_EQ(10u + 10u, partially_occluded_count);
  }
}

TEST_F(LegacySWPictureLayerImplTest, OcclusionOnSolidColorPictureLayer) {
  gfx::Size layer_bounds(1000, 1000);
  gfx::Size viewport_size(1000, 1000);

  LayerTreeImplTestBase impl;
  host_impl()->active_tree()->SetDeviceViewportRect(gfx::Rect(viewport_size));

  scoped_refptr<FakeRasterSource> pending_raster_source =
      FakeRasterSource::CreateFilledSolidColor(layer_bounds);
  SetupPendingTree(std::move(pending_raster_source), gfx::Size(), Region());
  // Device scale factor should not affect a non-mask solid color layer.
  host_impl()->pending_tree()->SetDeviceScaleFactor(2.f);
  ActivateTree();

  {
    SCOPED_TRACE("Scaled occlusion");
    gfx::Rect occluded(300, 0, 2000, 2000);
    impl.AppendQuadsWithOcclusion(active_layer(), occluded);

    size_t partial_occluded_count = 0;
    VerifyQuadsAreOccluded(impl.quad_list(), occluded, &partial_occluded_count);
    // Because of the implementation of test helper AppendQuadsWithOcclusion,
    // the occlusion will have a scale transform resulted from the device scale
    // factor. A single partially overlapped DrawQuad of 500x500 will be added.
    EXPECT_EQ(1u, impl.quad_list().size());
    EXPECT_EQ(1u, partial_occluded_count);
  }
}

TEST_F(LegacySWPictureLayerImplTest, IgnoreOcclusionOnSolidColorMask) {
  gfx::Size layer_bounds(1000, 1000);
  gfx::Size viewport_size(1000, 1000);

  LayerTreeImplTestBase impl;
  host_impl()->active_tree()->SetDeviceViewportRect(gfx::Rect(viewport_size));

  scoped_refptr<FakeRasterSource> pending_raster_source =
      FakeRasterSource::CreateFilledSolidColor(layer_bounds);
  SetupPendingTree(std::move(pending_raster_source), gfx::Size(), Region());

  host_impl()->pending_tree()->SetDeviceScaleFactor(2.f);
  ActivateTree();

  {
    SCOPED_TRACE("Scaled occlusion");
    gfx::Rect occluded(150, 0, 200, 1000);
    impl.AppendQuadsWithOcclusion(active_layer(), occluded);

    size_t partial_occluded_count = 0;
    VerifyQuadsAreOccluded(impl.quad_list(), gfx::Rect(),
                           &partial_occluded_count);
    // None of the quads shall be occluded because mask layers ignores
    // occlusion.
    EXPECT_EQ(1u, impl.quad_list().size());
    EXPECT_EQ(0u, partial_occluded_count);
  }
}

TEST_F(LegacySWPictureLayerImplTest, RasterScaleChangeWithoutAnimation) {
  gfx::Size tile_size(host_impl()->settings().default_tile_size);
  SetupDefaultTrees(tile_size);

  ResetTilingsAndRasterScales();

  float contents_scale = 2.f;
  float device_scale = 1.5f;
  float page_scale = 1.f;

  SetContentsScaleOnBothLayers(contents_scale, device_scale, page_scale);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), 2.f);

  // Changing the source scale without being in an animation will cause
  // the layer to change scale.
  contents_scale = 3.f;

  SetContentsScaleOnBothLayers(contents_scale, device_scale, page_scale);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), 3.f);

  contents_scale = 0.5f;

  SetContentsScaleOnBothLayers(contents_scale, device_scale, page_scale);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), 0.5f);

  // If we change the layer contents scale after setting will change
  // will, then it will be updated if it's below the minimum scale (page scale *
  // device scale).
  SetWillChangeTransform(active_layer(), true);
  SetWillChangeTransform(pending_layer(), true);

  contents_scale = 0.75f;

  SetContentsScaleOnBothLayers(contents_scale, device_scale, page_scale);
  // The scale is clamped to the native scale.
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), 1.5f);

  // Further changes to the source scale will no longer be reflected in the
  // contents scale.
  contents_scale = 2.f;

  SetContentsScaleOnBothLayers(contents_scale, device_scale, page_scale);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), 1.5f);

  // Ditto.
  contents_scale = 20.f;

  SetContentsScaleOnBothLayers(contents_scale, device_scale, page_scale);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), 1.5f);

  // Disabling the will-change hint will once again make the raster scale update
  // with the ideal scale.
  SetWillChangeTransform(active_layer(), false);
  SetWillChangeTransform(pending_layer(), false);

  contents_scale = 3.f;

  SetContentsScaleOnBothLayers(contents_scale, device_scale, page_scale);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), 3.f);
}

TEST_F(LegacySWPictureLayerImplTest, TinyRasterScale) {
  gfx::Size tile_size(host_impl()->settings().default_tile_size);
  SetupDefaultTrees(tile_size);

  ResetTilingsAndRasterScales();

  float contents_scale = 0.01f;
  float device_scale = 1.5f;
  float page_scale = 1.f;

  SetContentsScaleOnBothLayers(contents_scale, device_scale, page_scale);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), 0.01f);

  // If we change the layer contents scale after setting will change
  // will, then it will be updated if it's below the minimum scale (page scale *
  // device scale).
  SetWillChangeTransform(active_layer(), true);
  SetWillChangeTransform(pending_layer(), true);

  SetContentsScaleOnBothLayers(contents_scale, device_scale, page_scale);
  // The scale is clamped to the native scale.
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), 0.01f);

  // Further changes to the source scale will no longer be reflected in the
  // contents scale.
  contents_scale = 0.02f;

  SetContentsScaleOnBothLayers(contents_scale, device_scale, page_scale);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), 0.01f);

  // ... unless the difference is very big.
  contents_scale = 0.12f;

  SetContentsScaleOnBothLayers(contents_scale, device_scale, page_scale);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), 0.12f);

  // Bigger scale will be clamped to the native scale.
  contents_scale = 0.5f;

  SetContentsScaleOnBothLayers(contents_scale, device_scale, page_scale);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), 1.5f);
}

TEST_F(LegacySWPictureLayerImplTest,
       ForceAdjustRasterScaleWillChangeTransform) {
  gfx::Size layer_bounds(100, 100);
  SetupDefaultTrees(layer_bounds);

  ResetTilingsAndRasterScales();

  float contents_scale = 1.f;
  float device_scale = 1.f;
  float page_scale = 1.f;
  SetWillChangeTransform(active_layer(), true);
  SetWillChangeTransform(pending_layer(), true);

  SetContentsScaleOnBothLayers(contents_scale, device_scale, page_scale);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), 1.f);

  contents_scale = 2.f;
  SetContentsScaleOnBothLayers(contents_scale, device_scale, page_scale);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), 1.f);

  // Raster source size change forces adjustment of raster scale.
  layer_bounds = gfx::Size(200, 200);
  Region invalidation;
  // UpdateRasterSource() requires that the pending tree doesn't have tiles.
  pending_layer()->picture_layer_tiling_set()->RemoveAllTiles();
  pending_layer()->SetBounds(layer_bounds);
  pending_layer()->UpdateRasterSource(
      FakeRasterSource::CreateFilled(layer_bounds), &invalidation);
  pending_layer()->PushPropertiesTo(active_layer());
  SetContentsScaleOnBothLayers(contents_scale, device_scale, page_scale);
  EXPECT_BOTH_EQ(HighResTiling()->contents_scale_key(), 2.f);
}

TEST_F(LegacySWPictureLayerImplTest, LowResReadyToDrawNotEnoughToActivate) {
  gfx::Size tile_size(100, 100);
  gfx::Size layer_bounds(1000, 1000);

  // Make sure pending tree has tiles.
  gfx::Rect invalidation(gfx::Point(50, 50), tile_size);
  SetupDefaultTreesWithFixedTileSize(layer_bounds, tile_size, invalidation);

  // All pending layer tiles required are not ready.
  EXPECT_FALSE(host_impl()->tile_manager()->IsReadyToActivate());

  // Initialize all low-res tiles.
  EXPECT_FALSE(pending_layer()->LowResTiling());
  pending_layer()->SetAllTilesReadyInTiling(active_layer()->LowResTiling());

  // Low-res tiles should not be enough.
  EXPECT_FALSE(host_impl()->tile_manager()->IsReadyToActivate());

  // Initialize remaining tiles.
  pending_layer()->SetAllTilesReady();
  active_layer()->SetAllTilesReady();

  EXPECT_TRUE(host_impl()->tile_manager()->IsReadyToActivate());
}

TEST_F(LegacySWPictureLayerImplTest, HighResReadyToDrawEnoughToActivate) {
  gfx::Size tile_size(100, 100);
  gfx::Size layer_bounds(1000, 1000);

  // Make sure pending tree has tiles.
  gfx::Rect invalidation(gfx::Point(50, 50), tile_size);
  SetupDefaultTreesWithFixedTileSize(layer_bounds, tile_size, invalidation);

  // All pending layer tiles required are not ready.
  EXPECT_FALSE(host_impl()->tile_manager()->IsReadyToActivate());

  // Initialize all high-res tiles.
  pending_layer()->SetAllTilesReadyInTiling(pending_layer()->HighResTiling());
  active_layer()->SetAllTilesReadyInTiling(active_layer()->HighResTiling());

  // High-res tiles should be enough, since they cover everything visible.
  EXPECT_TRUE(host_impl()->tile_manager()->IsReadyToActivate());
}

TEST_F(LegacySWPictureLayerImplTest, ActiveHighResReadyNotEnoughToActivate) {
  gfx::Size tile_size(100, 100);
  gfx::Size layer_bounds(1000, 1000);

  // Make sure pending tree has tiles.
  gfx::Rect invalidation(gfx::Point(50, 50), tile_size);
  SetupDefaultTreesWithFixedTileSize(layer_bounds, tile_size, invalidation);

  // Initialize all high-res tiles in the active layer.
  active_layer()->SetAllTilesReadyInTiling(active_layer()->HighResTiling());

  // The pending high-res tiles are not ready, so we cannot activate.
  EXPECT_FALSE(host_impl()->tile_manager()->IsReadyToActivate());

  // When the pending high-res tiles are ready, we can activate.
  pending_layer()->SetAllTilesReadyInTiling(pending_layer()->HighResTiling());
  EXPECT_TRUE(host_impl()->tile_manager()->IsReadyToActivate());
}

TEST_F(NoLowResPictureLayerImplTest, ManageTilingsCreatesTilings) {
  gfx::Size layer_bounds(1300, 1900);
  SetupDefaultTrees(layer_bounds);

  float low_res_factor = host_impl()->settings().low_res_contents_scale_factor;
  EXPECT_LT(low_res_factor, 1.f);

  ResetTilingsAndRasterScales();

  SetupDrawPropertiesAndUpdateTiles(active_layer(),
                                    6.f,   // ideal contents scale
                                    3.f,   // device scale
                                    2.f);  // page scale
  ASSERT_EQ(1u, active_layer()->tilings()->num_tilings());
  EXPECT_FLOAT_EQ(
      6.f, active_layer()->tilings()->tiling_at(0)->contents_scale_key());

  // If we change the page scale factor, then we should get new tilings.
  SetupDrawPropertiesAndUpdateTiles(active_layer(),
                                    6.6f,   // ideal contents scale
                                    3.f,    // device scale
                                    2.2f);  // page scale
  ASSERT_EQ(2u, active_layer()->tilings()->num_tilings());
  EXPECT_FLOAT_EQ(
      6.6f, active_layer()->tilings()->tiling_at(0)->contents_scale_key());

  // If we change the device scale factor, then we should get new tilings.
  SetupDrawPropertiesAndUpdateTiles(active_layer(),
                                    7.26f,  // ideal contents scale
                                    3.3f,   // device scale
                                    2.2f);  // page scale
  ASSERT_EQ(3u, active_layer()->tilings()->num_tilings());
  EXPECT_FLOAT_EQ(
      7.26f, active_layer()->tilings()->tiling_at(0)->contents_scale_key());

  // If we change the device scale factor, but end up at the same total scale
  // factor somehow, then we don't get new tilings.
  SetupDrawPropertiesAndUpdateTiles(active_layer(),
                                    7.26f,  // ideal contents scale
                                    2.2f,   // device scale
                                    3.3f);  // page scale
  ASSERT_EQ(3u, active_layer()->tilings()->num_tilings());
  EXPECT_FLOAT_EQ(
      7.26f, active_layer()->tilings()->tiling_at(0)->contents_scale_key());
}

TEST_F(NoLowResPictureLayerImplTest, PendingLayerOnlyHasHighResTiling) {
  gfx::Size layer_bounds(1300, 1900);
  SetupDefaultTrees(layer_bounds);

  float low_res_factor = host_impl()->settings().low_res_contents_scale_factor;
  EXPECT_LT(low_res_factor, 1.f);

  ResetTilingsAndRasterScales();

  SetupDrawPropertiesAndUpdateTiles(pending_layer(),
                                    6.f,   // ideal contents scale
                                    3.f,   // device scale
                                    2.f);  // page scale
  ASSERT_EQ(1u, pending_layer()->tilings()->num_tilings());
  EXPECT_FLOAT_EQ(
      6.f, pending_layer()->tilings()->tiling_at(0)->contents_scale_key());

  // If we change the page scale factor, then we should get new tilings.
  SetupDrawPropertiesAndUpdateTiles(pending_layer(),
                                    6.6f,   // ideal contents scale
                                    3.f,    // device scale
                                    2.2f);  // page scale
  ASSERT_EQ(1u, pending_layer()->tilings()->num_tilings());
  EXPECT_FLOAT_EQ(
      6.6f, pending_layer()->tilings()->tiling_at(0)->contents_scale_key());

  // If we change the device scale factor, then we should get new tilings.
  SetupDrawPropertiesAndUpdateTiles(pending_layer(),
                                    7.26f,  // ideal contents scale
                                    3.3f,   // device scale
                                    2.2f);  // page scale
  ASSERT_EQ(1u, pending_layer()->tilings()->num_tilings());
  EXPECT_FLOAT_EQ(
      7.26f, pending_layer()->tilings()->tiling_at(0)->contents_scale_key());

  // If we change the device scale factor, but end up at the same total scale
  // factor somehow, then we don't get new tilings.
  SetupDrawPropertiesAndUpdateTiles(pending_layer(),
                                    7.26f,  // ideal contents scale
                                    2.2f,   // device scale
                                    3.3f);  // page scale
  ASSERT_EQ(1u, pending_layer()->tilings()->num_tilings());
  EXPECT_FLOAT_EQ(
      7.26f, pending_layer()->tilings()->tiling_at(0)->contents_scale_key());
}

TEST_F(NoLowResPictureLayerImplTest, AllHighResRequiredEvenIfNotChanged) {
  gfx::Size layer_bounds(400, 400);
  gfx::Size tile_size(100, 100);

  SetupDefaultTreesWithFixedTileSize(layer_bounds, tile_size, Region());

  Tile* some_active_tile =
      active_layer()->HighResTiling()->AllTilesForTesting()[0];
  EXPECT_FALSE(some_active_tile->draw_info().IsReadyToDraw());

  // Since there is no invalidation, pending tree should have no tiles.
  EXPECT_TRUE(pending_layer()->HighResTiling()->AllTilesForTesting().empty());
  if (host_impl()->settings().create_low_res_tiling)
    EXPECT_TRUE(pending_layer()->LowResTiling()->AllTilesForTesting().empty());

  active_layer()->HighResTiling()->UpdateAllRequiredStateForTesting();
  if (host_impl()->settings().create_low_res_tiling)
    active_layer()->LowResTiling()->UpdateAllRequiredStateForTesting();

  AssertAllTilesRequired(active_layer()->HighResTiling());
  if (host_impl()->settings().create_low_res_tiling)
    AssertNoTilesRequired(active_layer()->LowResTiling());
}

TEST_F(NoLowResPictureLayerImplTest, NothingRequiredIfActiveMissingTiles) {
  gfx::Size layer_bounds(1000, 4000);
  gfx::Size tile_size(100, 100);

  scoped_refptr<FakeRasterSource> pending_raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);
  scoped_refptr<FakeRasterSource> active_raster_source =
      FakeRasterSource::CreatePartiallyFilled(layer_bounds,
                                              gfx::Rect(500, 500));

  SetupTreesWithFixedTileSize(pending_raster_source, active_raster_source,
                              tile_size, Region());

  // Active layer has tilings.
  EXPECT_TRUE(active_layer()->CanHaveTilings());
  EXPECT_EQ(active_layer()->tilings()->num_tilings(),
            host_impl()->settings().create_low_res_tiling ? 2u : 1u);
  // Simulate scrolling past the end of recorded content on the active layer,
  // where the recordings are so far away that no tiles are created.
  gfx::Rect visible_rect(0, 3000, 1000, 1000);
  active_layer()->HighResTiling()->ComputeTilePriorityRects(
      visible_rect, visible_rect, visible_rect, visible_rect, 1, Occlusion());
  EXPECT_EQ(active_layer()->HighResTiling()->AllTilesForTesting().size(), 0u);

  // Since the active layer has no tiles at all, the pending layer doesn't
  // need content in order to activate.
  pending_layer()->HighResTiling()->UpdateAllRequiredStateForTesting();
  if (host_impl()->settings().create_low_res_tiling)
    pending_layer()->LowResTiling()->UpdateAllRequiredStateForTesting();

  EXPECT_EQ(pending_layer()->HighResTiling()->AllTilesForTesting().size(), 0u);
  if (host_impl()->settings().create_low_res_tiling) {
    EXPECT_EQ(pending_layer()->LowResTiling()->AllTilesForTesting().size(), 0u);
  }
}

TEST_F(NoLowResPictureLayerImplTest, CleanUpTilings) {
  gfx::Size layer_bounds(1300, 1900);
  std::vector<raw_ptr<PictureLayerTiling, VectorExperimental>> used_tilings;
  SetupDefaultTrees(layer_bounds);

  float low_res_factor = host_impl()->settings().low_res_contents_scale_factor;
  EXPECT_LT(low_res_factor, 1.f);

  // Set the device scale and page scale so that the minimum that we would clamp
  // to is small. This test isn't testing the clamping. See
  // RasterScaleChangeWithoutAnimation for this test.
  float device_scale = 0.5f;
  float page_scale = 1.f;
  float scale = 1.f;

  SetWillChangeTransform(active_layer(), true);
  ResetTilingsAndRasterScales();

  SetContentsScaleOnBothLayers(scale, device_scale, page_scale);
  ASSERT_EQ(1u, active_layer()->tilings()->num_tilings());

  // Ensure UpdateTiles won't remove any tilings. Note this is unrelated to
  // |used_tilings| variable, and it's here only to ensure that active_layer()
  // won't remove tilings before the test has a chance to verify behavior.
  active_layer()->MarkAllTilingsUsed();

  // We only have ideal tilings, so they aren't removed.
  used_tilings.clear();
  active_layer()->CleanUpTilingsOnActiveLayer(used_tilings);
  ASSERT_EQ(1u, active_layer()->tilings()->num_tilings());

  // Since this test simulates a pinch it needs an input handler.
  // TODO(bokan): This is a raster unit test, it shouldn't be using a real
  // input handler.
  InputHandler::Create(static_cast<CompositorDelegateForInput&>(*host_impl()));

  host_impl()->GetInputHandler().PinchGestureBegin(
      gfx::Point(), ui::ScrollInputType::kTouchscreen);

  // Changing the ideal but not creating new tilings.
  scale *= 1.5f;
  page_scale *= 1.5f;
  SetContentsScaleOnBothLayers(scale, device_scale, page_scale);
  ASSERT_EQ(1u, active_layer()->tilings()->num_tilings());

  // The tilings are still our target scale, so they aren't removed.
  used_tilings.clear();
  active_layer()->CleanUpTilingsOnActiveLayer(used_tilings);
  ASSERT_EQ(1u, active_layer()->tilings()->num_tilings());

  host_impl()->GetInputHandler().PinchGestureEnd(gfx::Point());

  // Create a 1.2 scale tiling. Now we have 1.0 and 1.2 tilings. Ideal = 1.2.
  scale /= 4.f;
  page_scale /= 4.f;
  SetContentsScaleOnBothLayers(1.2f, device_scale, page_scale);
  ASSERT_EQ(2u, active_layer()->tilings()->num_tilings());
  EXPECT_FLOAT_EQ(
      1.f, active_layer()->tilings()->tiling_at(1)->contents_scale_key());

  // Ensure UpdateTiles won't remove any tilings.
  active_layer()->MarkAllTilingsUsed();

  // Mark the non-ideal tilings as used. They won't be removed.
  used_tilings.clear();
  used_tilings.push_back(active_layer()->tilings()->tiling_at(1));
  active_layer()->CleanUpTilingsOnActiveLayer(used_tilings);
  ASSERT_EQ(2u, active_layer()->tilings()->num_tilings());

  // Now move the ideal scale to 0.5. Our target stays 1.2.
  SetContentsScaleOnBothLayers(0.5f, device_scale, page_scale);

  // The high resolution tiling is between target and ideal, so is not
  // removed.  The low res tiling for the old ideal=1.0 scale is removed.
  used_tilings.clear();
  active_layer()->CleanUpTilingsOnActiveLayer(used_tilings);
  ASSERT_EQ(2u, active_layer()->tilings()->num_tilings());

  // Now move the ideal scale to 1.0. Our target stays 1.2.
  SetContentsScaleOnBothLayers(1.f, device_scale, page_scale);

  // All the tilings are between are target and the ideal, so they are not
  // removed.
  used_tilings.clear();
  active_layer()->CleanUpTilingsOnActiveLayer(used_tilings);
  ASSERT_EQ(2u, active_layer()->tilings()->num_tilings());

  // Now move the ideal scale to 1.1 on the active layer. Our target stays 1.2.
  SetupDrawPropertiesAndUpdateTiles(active_layer(), 1.1f, device_scale,
                                    page_scale);

  // Because the pending layer's ideal scale is still 1.0, our tilings fall
  // in the range [1.0,1.2] and are kept.
  used_tilings.clear();
  active_layer()->CleanUpTilingsOnActiveLayer(used_tilings);
  ASSERT_EQ(2u, active_layer()->tilings()->num_tilings());

  // Move the ideal scale on the pending layer to 1.1 as well. Our target stays
  // 1.2 still.
  SetupDrawPropertiesAndUpdateTiles(pending_layer(), 1.1f, device_scale,
                                    page_scale);

  // Our 1.0 tiling now falls outside the range between our ideal scale and our
  // target raster scale. But it is in our used tilings set, so nothing is
  // deleted.
  used_tilings.clear();
  used_tilings.push_back(active_layer()->tilings()->tiling_at(1));
  active_layer()->CleanUpTilingsOnActiveLayer(used_tilings);
  ASSERT_EQ(2u, active_layer()->tilings()->num_tilings());

  // If we remove it from our used tilings set, it is outside the range to keep
  // so it is deleted.
  used_tilings.clear();
  active_layer()->CleanUpTilingsOnActiveLayer(used_tilings);
  ASSERT_EQ(1u, active_layer()->tilings()->num_tilings());
}

TEST_F(NoLowResPictureLayerImplTest, ReleaseTileResources) {
  gfx::Size layer_bounds(1300, 1900);
  SetupDefaultTrees(layer_bounds);
  EXPECT_EQ(1u, pending_layer()->tilings()->num_tilings());
  EXPECT_EQ(1u, active_layer()->tilings()->num_tilings());

  // All tilings should be removed when losing output surface.
  active_layer()->ReleaseTileResources();
  EXPECT_TRUE(active_layer()->tilings());
  EXPECT_EQ(0u, active_layer()->num_tilings());
  active_layer()->RecreateTileResources();
  EXPECT_EQ(0u, active_layer()->num_tilings());
  pending_layer()->ReleaseTileResources();
  EXPECT_TRUE(pending_layer()->tilings());
  EXPECT_EQ(0u, pending_layer()->num_tilings());
  pending_layer()->RecreateTileResources();
  EXPECT_EQ(0u, pending_layer()->num_tilings());

  // This should create new tilings.
  SetupDrawPropertiesAndUpdateTiles(pending_layer(),
                                    1.3f,   // ideal contents scale
                                    2.7f,   // device scale
                                    3.2f);  // page scale
  EXPECT_EQ(1u, pending_layer()->tilings()->num_tilings());
}

TEST_F(LegacySWPictureLayerImplTest, SharedQuadStateContainsMaxTilingScale) {
  auto render_pass = viz::CompositorRenderPass::Create();

  gfx::Size layer_bounds(1000, 2000);
  host_impl()->active_tree()->SetDeviceViewportRect(gfx::Rect(10000, 20000));
  SetupDefaultTrees(layer_bounds);

  ResetTilingsAndRasterScales();
  SetupDrawPropertiesAndUpdateTiles(active_layer(), 2.5f, 1.f, 1.f);

  float max_contents_scale = active_layer()->MaximumTilingContentsScale();
  EXPECT_EQ(2.5f, max_contents_scale);

  gfx::Transform scaled_draw_transform = active_layer()->DrawTransform();
  scaled_draw_transform.Scale(SK_Scalar1 / max_contents_scale,
                              SK_Scalar1 / max_contents_scale);

  AppendQuadsData data;
  active_layer()->AppendQuads(render_pass.get(), &data);

  // SharedQuadState should have be of size 1, as we are doing AppenQuad once.
  EXPECT_EQ(1u, render_pass->shared_quad_state_list.size());
  // The quad_to_target_transform should be scaled by the
  // MaximumTilingContentsScale on the layer.
  EXPECT_EQ(scaled_draw_transform.ToString(),
            render_pass->shared_quad_state_list.front()
                ->quad_to_target_transform.ToString());
  // The content_bounds should be scaled by the
  // MaximumTilingContentsScale on the layer.
  EXPECT_EQ(
      gfx::Rect(2500u, 5000u).ToString(),
      render_pass->shared_quad_state_list.front()->quad_layer_rect.ToString());
  // The visible_layer_rect should be scaled by the
  // MaximumTilingContentsScale on the layer.
  EXPECT_EQ(gfx::Rect(0u, 0u, 2500u, 5000u).ToString(),
            render_pass->shared_quad_state_list.front()
                ->visible_quad_layer_rect.ToString());
}

class PictureLayerImplTestWithDelegatingRenderer
    : public LegacySWPictureLayerImplTest {
 public:
  std::unique_ptr<LayerTreeFrameSink> CreateLayerTreeFrameSink() override {
    return FakeLayerTreeFrameSink::Create3d();
  }
};

TEST_F(PictureLayerImplTestWithDelegatingRenderer,
       DelegatingRendererWithTileOOM) {
  // This test is added for crbug.com/402321, where quad should be produced when
  // raster on demand is not allowed and tile is OOM.
  gfx::Size layer_bounds(1000, 1000);

  // Create tiles.
  scoped_refptr<FakeRasterSource> pending_raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);
  SetupPendingTree(pending_raster_source);
  pending_layer()->SetBounds(layer_bounds);
  ActivateTree();
  UpdateDrawProperties(host_impl()->active_tree());
  std::vector<Tile*> tiles =
      active_layer()->HighResTiling()->AllTilesForTesting();
  host_impl()->tile_manager()->InitializeTilesWithResourcesForTesting(tiles);

  // Force tiles after max_tiles to be OOM. TileManager uses
  // GlobalStateThatImpactsTilesPriority from LayerTreeHostImpl, and we cannot
  // directly set state to host_impl_, so we set policy that would change the
  // state. We also need to update tree priority separately.
  GlobalStateThatImpactsTilePriority state;
  size_t max_tiles = 1;
  gfx::Size tile_size(host_impl()->settings().default_tile_size);
  size_t memory_limit = max_tiles * 4 * tile_size.width() * tile_size.height();
  size_t resource_limit = max_tiles;
  ManagedMemoryPolicy policy(memory_limit,
                             gpu::MemoryAllocation::CUTOFF_ALLOW_EVERYTHING,
                             resource_limit);
  host_impl()->SetMemoryPolicy(policy);
  host_impl()->SetTreePriority(SAME_PRIORITY_FOR_BOTH_TREES);
  host_impl()->PrepareTiles();

  auto render_pass = viz::CompositorRenderPass::Create();
  AppendQuadsData data;
  active_layer()->WillDraw(DRAW_MODE_HARDWARE, nullptr);
  active_layer()->AppendQuads(render_pass.get(), &data);
  active_layer()->DidDraw(nullptr);

  // Even when OOM, quads should be produced, and should be different material
  // from quads with resource.
  EXPECT_LT(max_tiles, render_pass->quad_list.size());
  EXPECT_EQ(viz::DrawQuad::Material::kTiledContent,
            render_pass->quad_list.front()->material);
  EXPECT_EQ(viz::DrawQuad::Material::kSolidColor,
            render_pass->quad_list.back()->material);
}

class OcclusionTrackingPictureLayerImplTest
    : public LegacySWPictureLayerImplTest {
 public:
  LayerTreeSettings CreateSettings() override {
    LayerTreeSettings settings = LegacySWPictureLayerImplTest::CreateSettings();
    settings.use_occlusion_for_tile_prioritization = true;
    return settings;
  }

  void VerifyEvictionConsidersOcclusion(FakePictureLayerImpl* layer,
                                        WhichTree tree,
                                        size_t expected_occluded_tile_count,
                                        int source_line) {
    size_t occluded_tile_count = 0u;
    PrioritizedTile last_tile;

    std::unique_ptr<TilingSetEvictionQueue> queue(new TilingSetEvictionQueue(
        layer->picture_layer_tiling_set(),
        layer->contributes_to_drawn_render_surface()));
    while (!queue->IsEmpty()) {
      PrioritizedTile prioritized_tile = queue->Top();
      Tile* tile = prioritized_tile.tile();
      if (!last_tile.tile())
        last_tile = prioritized_tile;

      // The only way we will encounter an occluded tile after an unoccluded
      // tile is if the priorty bin decreased, the tile is required for
      // activation, or the scale changed.
      bool tile_is_occluded = prioritized_tile.is_occluded();
      if (tile_is_occluded) {
        occluded_tile_count++;

        bool last_tile_is_occluded = last_tile.is_occluded();
        if (!last_tile_is_occluded) {
          TilePriority::PriorityBin tile_priority_bin =
              prioritized_tile.priority().priority_bin;
          TilePriority::PriorityBin last_tile_priority_bin =
              last_tile.priority().priority_bin;

          EXPECT_TRUE(tile_priority_bin < last_tile_priority_bin ||
                      tile->required_for_activation() ||
                      tile->contents_scale_key() !=
                          last_tile.tile()->contents_scale_key())
              << "line: " << source_line;
        }
      }
      last_tile = prioritized_tile;
      queue->Pop();
    }
    EXPECT_EQ(expected_occluded_tile_count, occluded_tile_count)
        << "line: " << source_line;
  }

  void TestOcclusionForScale(float scale, int expected_occluded_count);
};

TEST_F(OcclusionTrackingPictureLayerImplTest,
       OccludedTilesSkippedDuringRasterization) {
  host_impl()->AdvanceToNextFrame(base::Milliseconds(1));

  gfx::Size tile_size(102, 102);
  gfx::Size layer_bounds(1000, 1000);
  gfx::Size viewport_size(500, 500);
  gfx::Vector2dF occluding_layer_position(310.f, 0.f);

  host_impl()->active_tree()->SetDeviceViewportRect(gfx::Rect(viewport_size));

  scoped_refptr<FakeRasterSource> pending_raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);
  SetupPendingTreeWithFixedTileSize(pending_raster_source, tile_size, Region());

  // No occlusion.
  int unoccluded_tile_count = 0;
  std::unique_ptr<TilingSetRasterQueueAll> queue =
      TilingSetRasterQueueAll::Create(
          pending_layer()->picture_layer_tiling_set(), false, false);
  EXPECT_TRUE(queue);
  while (!queue->IsEmpty()) {
    PrioritizedTile prioritized_tile = queue->Top();
    Tile* tile = prioritized_tile.tile();

    // Occluded tiles should not be iterated over.
    EXPECT_FALSE(prioritized_tile.is_occluded());

    // Some tiles may not be visible (i.e. outside the viewport). The rest are
    // visible and at least partially unoccluded, verified by the above expect.
    bool tile_is_visible =
        tile->content_rect().Intersects(pending_layer()->visible_layer_rect());
    if (tile_is_visible)
      unoccluded_tile_count++;
    queue->Pop();
  }
  EXPECT_EQ(unoccluded_tile_count, 25);

  // Partial occlusion.
  LayerImpl* layer1 = AddLayer<LayerImpl>(host_impl()->pending_tree());
  layer1->SetBounds(layer_bounds);
  layer1->SetDrawsContent(true);
  layer1->SetContentsOpaque(true);
  CopyProperties(pending_layer(), layer1);
  layer1->SetOffsetToTransformParent(occluding_layer_position);

  host_impl()->AdvanceToNextFrame(base::Milliseconds(200));
  UpdateDrawProperties(host_impl()->pending_tree());

  unoccluded_tile_count = 0;
  queue = TilingSetRasterQueueAll::Create(
      pending_layer()->picture_layer_tiling_set(), false, false);
  EXPECT_TRUE(queue);
  while (!queue->IsEmpty()) {
    PrioritizedTile prioritized_tile = queue->Top();
    Tile* tile = prioritized_tile.tile();

    EXPECT_FALSE(prioritized_tile.is_occluded());

    bool tile_is_visible =
        tile->content_rect().Intersects(pending_layer()->visible_layer_rect());
    if (tile_is_visible)
      unoccluded_tile_count++;
    queue->Pop();
  }
  EXPECT_EQ(20, unoccluded_tile_count);

  // Full occlusion.
  layer1->SetOffsetToTransformParent(gfx::Vector2dF());
  layer1->NoteLayerPropertyChanged();

  host_impl()->AdvanceToNextFrame(base::Milliseconds(200));
  UpdateDrawProperties(host_impl()->pending_tree());

  unoccluded_tile_count = 0;
  queue = TilingSetRasterQueueAll::Create(
      pending_layer()->picture_layer_tiling_set(), false, false);
  EXPECT_TRUE(queue);
  while (!queue->IsEmpty()) {
    PrioritizedTile prioritized_tile = queue->Top();
    Tile* tile = prioritized_tile.tile();

    EXPECT_FALSE(prioritized_tile.is_occluded());

    bool tile_is_visible =
        tile->content_rect().Intersects(pending_layer()->visible_layer_rect());
    if (tile_is_visible)
      unoccluded_tile_count++;
    queue->Pop();
  }
  EXPECT_EQ(unoccluded_tile_count, 0);
}

TEST_F(OcclusionTrackingPictureLayerImplTest,
       OccludedTilesNotMarkedAsRequired) {
  host_impl()->AdvanceToNextFrame(base::Milliseconds(1));

  gfx::Size tile_size(102, 102);
  gfx::Size layer_bounds(1000, 1000);
  gfx::Size viewport_size(500, 500);
  gfx::Vector2dF occluding_layer_position(310.f, 0.f);

  host_impl()->active_tree()->SetDeviceViewportRect(gfx::Rect(viewport_size));

  scoped_refptr<FakeRasterSource> pending_raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);
  SetupPendingTreeWithFixedTileSize(pending_raster_source, tile_size, Region());

  // No occlusion.
  int occluded_tile_count = 0;
  for (size_t i = 0; i < pending_layer()->num_tilings(); ++i) {
    PictureLayerTiling* tiling = pending_layer()->tilings()->tiling_at(i);
    auto prioritized_tiles =
        tiling->UpdateAndGetAllPrioritizedTilesForTesting();

    occluded_tile_count = 0;
    for (PictureLayerTiling::CoverageIterator iter(tiling, 1.f,
                                                   gfx::Rect(layer_bounds));
         iter; ++iter) {
      if (!*iter)
        continue;
      const Tile* tile = *iter;

      // Fully occluded tiles are not required for activation.
      if (prioritized_tiles[tile].is_occluded()) {
        EXPECT_FALSE(tile->required_for_activation());
        occluded_tile_count++;
      }
    }
    EXPECT_EQ(occluded_tile_count, 0);
  }

  // Partial occlusion.
  LayerImpl* layer1 = AddLayer<LayerImpl>(host_impl()->pending_tree());
  layer1->SetBounds(layer_bounds);
  layer1->SetDrawsContent(true);
  layer1->SetContentsOpaque(true);
  CopyProperties(pending_layer(), layer1);
  layer1->SetOffsetToTransformParent(occluding_layer_position);

  host_impl()->AdvanceToNextFrame(base::Milliseconds(200));
  UpdateDrawProperties(host_impl()->pending_tree());

  for (size_t i = 0; i < pending_layer()->num_tilings(); ++i) {
    PictureLayerTiling* tiling = pending_layer()->tilings()->tiling_at(i);
    auto prioritized_tiles =
        tiling->UpdateAndGetAllPrioritizedTilesForTesting();

    occluded_tile_count = 0;
    for (PictureLayerTiling::CoverageIterator iter(tiling, 1.f,
                                                   gfx::Rect(layer_bounds));
         iter; ++iter) {
      if (!*iter)
        continue;
      const Tile* tile = *iter;

      if (prioritized_tiles[tile].is_occluded()) {
        EXPECT_FALSE(tile->required_for_activation());
        occluded_tile_count++;
      }
    }
    switch (i) {
      case 0:
        EXPECT_EQ(occluded_tile_count, 5);
        break;
      case 1:
        EXPECT_EQ(occluded_tile_count, 2);
        break;
      default:
        NOTREACHED();
    }
  }

  // Full occlusion.
  layer1->SetOffsetToTransformParent(gfx::Vector2dF());
  layer1->NoteLayerPropertyChanged();

  host_impl()->AdvanceToNextFrame(base::Milliseconds(200));
  UpdateDrawProperties(host_impl()->pending_tree());

  for (size_t i = 0; i < pending_layer()->num_tilings(); ++i) {
    PictureLayerTiling* tiling = pending_layer()->tilings()->tiling_at(i);
    auto prioritized_tiles =
        tiling->UpdateAndGetAllPrioritizedTilesForTesting();

    occluded_tile_count = 0;
    for (PictureLayerTiling::CoverageIterator iter(tiling, 1.f,
                                                   gfx::Rect(layer_bounds));
         iter; ++iter) {
      if (!*iter)
        continue;
      const Tile* tile = *iter;

      if (prioritized_tiles[tile].is_occluded()) {
        EXPECT_FALSE(tile->required_for_activation());
        occluded_tile_count++;
      }
    }
    switch (i) {
      case 0:
        EXPECT_EQ(25, occluded_tile_count);
        break;
      case 1:
        EXPECT_EQ(4, occluded_tile_count);
        break;
      default:
        NOTREACHED();
    }
  }
}

void OcclusionTrackingPictureLayerImplTest::TestOcclusionForScale(
    float scale,
    int expected_occluded_count) {
  host_impl()->AdvanceToNextFrame(base::Milliseconds(1));

  gfx::Size tile_size(102, 102);
  gfx::Size layer_bounds(1000, 1000);
  gfx::Size viewport_size(500, 500);
  gfx::Vector2dF occluding_layer_position(310.f, 0.f);

  scoped_refptr<FakeRasterSource> pending_raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);

  host_impl()->active_tree()->SetDeviceViewportRect(gfx::Rect(viewport_size));

  SetupPendingTreeWithFixedTileSize(pending_raster_source, tile_size, Region());

  ActivateTree();

  LayerImpl* layer1 = AddLayer<LayerImpl>(host_impl()->active_tree());
  layer1->SetBounds(layer_bounds);
  layer1->SetDrawsContent(true);
  layer1->SetContentsOpaque(true);
  CopyProperties(active_layer(), layer1);
  layer1->SetOffsetToTransformParent(occluding_layer_position);

  ASSERT_TRUE(active_layer()->CanHaveTilings());
  active_layer()->SetContentsScaleForTesting(scale);
  active_layer()->tilings()->RemoveAllTilings();
  float low_res_factor = host_impl()->settings().low_res_contents_scale_factor;
  active_layer()
      ->AddTiling(gfx::AxisTransform2d(low_res_factor, gfx::Vector2dF()))
      ->set_resolution(LOW_RESOLUTION);
  active_layer()
      ->AddTiling(gfx::AxisTransform2d(scale, gfx::Vector2dF()))
      ->set_resolution(HIGH_RESOLUTION);

  ASSERT_EQ(2u, active_layer()->num_tilings());

  host_impl()->AdvanceToNextFrame(base::Milliseconds(1));
  // UpdateDrawProperties with the occluding layer.
  UpdateDrawProperties(host_impl()->active_tree());

  ASSERT_EQ(2u, active_layer()->num_tilings());

  int occluded_tile_count = 0;
  for (size_t i = 0; i < active_layer()->num_tilings(); ++i) {
    PictureLayerTiling* tiling = active_layer()->tilings()->tiling_at(i);
    auto prioritized_tiles =
        tiling->UpdateAndGetAllPrioritizedTilesForTesting();
    std::vector<Tile*> tiles = tiling->AllTilesForTesting();

    occluded_tile_count = 0;
    for (size_t j = 0; j < tiles.size(); ++j) {
      if (prioritized_tiles[tiles[j]].is_occluded()) {
        gfx::Rect scaled_content_rect = ScaleToEnclosingRect(
            tiles[j]->content_rect(), 1.f / tiles[j]->contents_scale_key());
        EXPECT_GE(scaled_content_rect.x(), occluding_layer_position.x());
        occluded_tile_count++;
      }
    }

    if (i == 0) {
      EXPECT_EQ(scale, tiling->contents_scale_key());
      EXPECT_EQ(occluded_tile_count, expected_occluded_count);
    } else {
      ASSERT_EQ(1u, i);
      EXPECT_EQ(occluded_tile_count, 2);
    }
  }
}

TEST_F(OcclusionTrackingPictureLayerImplTest, OcclusionForScale0_3) {
  TestOcclusionForScale(0.3f, 2);
}

TEST_F(OcclusionTrackingPictureLayerImplTest, OcclusionForScale0_7) {
  TestOcclusionForScale(0.7f, 4);
}

TEST_F(OcclusionTrackingPictureLayerImplTest, OcclusionForScale1) {
  TestOcclusionForScale(1.0f, 5);
}

TEST_F(OcclusionTrackingPictureLayerImplTest, OcclusionForScale2) {
  TestOcclusionForScale(2.0f, 30);
}

TEST_F(OcclusionTrackingPictureLayerImplTest, DifferentOcclusionOnTrees) {
  gfx::Size layer_bounds(1000, 1000);
  gfx::Size viewport_size(1000, 1000);
  gfx::Vector2dF occluding_layer_position(310.f, 0.f);
  gfx::Rect invalidation_rect(230, 230, 102, 102);

  scoped_refptr<FakeRasterSource> pending_raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);
  scoped_refptr<FakeRasterSource> active_raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);

  host_impl()->active_tree()->SetDeviceViewportRect(gfx::Rect(viewport_size));
  SetupPendingTree(active_raster_source);

  // Partially occlude the active layer.
  LayerImpl* layer1 = AddLayer<LayerImpl>(host_impl()->pending_tree());
  layer1->SetBounds(layer_bounds);
  layer1->SetDrawsContent(true);
  layer1->SetContentsOpaque(true);
  CopyProperties(pending_layer(), layer1);
  layer1->SetOffsetToTransformParent(occluding_layer_position);

  ActivateTree();

  for (size_t i = 0; i < active_layer()->num_tilings(); ++i) {
    PictureLayerTiling* tiling = active_layer()->tilings()->tiling_at(i);
    auto prioritized_tiles =
        tiling->UpdateAndGetAllPrioritizedTilesForTesting();

    for (PictureLayerTiling::CoverageIterator iter(tiling, 1.f,
                                                   gfx::Rect(layer_bounds));
         iter; ++iter) {
      if (!*iter)
        continue;
      const Tile* tile = *iter;

      gfx::Rect scaled_content_rect = ScaleToEnclosingRect(
          tile->content_rect(), 1.f / tile->contents_scale_key());
      // Tiles are occluded on the active tree iff they lie beneath the
      // occluding layer.
      EXPECT_EQ(prioritized_tiles[tile].is_occluded(),
                scaled_content_rect.x() >= occluding_layer_position.x());
    }
  }

  // Partially invalidate the pending layer.
  SetupPendingTreeWithInvalidation(pending_raster_source, invalidation_rect);

  for (size_t i = 0; i < active_layer()->num_tilings(); ++i) {
    PictureLayerTiling* tiling = active_layer()->tilings()->tiling_at(i);
    auto prioritized_tiles =
        tiling->UpdateAndGetAllPrioritizedTilesForTesting();

    for (PictureLayerTiling::CoverageIterator iter(tiling, 1.f,
                                                   gfx::Rect(layer_bounds));
         iter; ++iter) {
      if (!*iter)
        continue;
      const Tile* tile = *iter;
      EXPECT_TRUE(tile);

      // All tiles are unoccluded, because the pending tree has no occlusion.
      EXPECT_FALSE(prioritized_tiles[tile].is_occluded());

      if (tiling->resolution() == LOW_RESOLUTION) {
        EXPECT_FALSE(active_layer()->GetPendingOrActiveTwinTiling(tiling));
        continue;
      }

      Tile* twin_tile =
          active_layer()->GetPendingOrActiveTwinTiling(tiling)->TileAt(
              iter.i(), iter.j());
      gfx::Rect scaled_content_rect = ScaleToEnclosingRect(
          tile->content_rect(), 1.f / tile->contents_scale_key());

      if (scaled_content_rect.Intersects(invalidation_rect)) {
        // Tiles inside the invalidation rect exist on both trees.
        EXPECT_TRUE(twin_tile);
        EXPECT_NE(tile, twin_tile);
      } else {
        // Tiles outside the invalidation rect only exist on the active tree.
        EXPECT_FALSE(twin_tile);
      }
    }
  }
}

TEST_F(OcclusionTrackingPictureLayerImplTest,
       OccludedTilesConsideredDuringEviction) {
  host_impl()->AdvanceToNextFrame(base::Milliseconds(1));

  gfx::Size tile_size(102, 102);
  gfx::Size layer_bounds(1000, 1000);
  gfx::Size viewport_size(1000, 1000);
  gfx::Vector2dF pending_occluding_layer_position(310.f, 0.f);
  gfx::Vector2dF active_occluding_layer_position(0.f, 310.f);
  gfx::Rect invalidation_rect(230, 230, 152, 152);

  host_impl()->active_tree()->SetDeviceViewportRect(gfx::Rect(viewport_size));
  SetInitialDeviceScaleFactor(2.f);

  scoped_refptr<FakeRasterSource> pending_raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);
  scoped_refptr<FakeRasterSource> active_raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);

  SetupPendingTreeWithFixedTileSize(active_raster_source, tile_size, Region());

  // Partially occlude the active layer.
  LayerImpl* active_occluding_layer =
      AddLayer<LayerImpl>(host_impl()->pending_tree());
  active_occluding_layer->SetBounds(layer_bounds);
  active_occluding_layer->SetDrawsContent(true);
  active_occluding_layer->SetContentsOpaque(true);
  CopyProperties(pending_layer(), active_occluding_layer);
  active_occluding_layer->SetOffsetToTransformParent(
      active_occluding_layer_position);
  ActivateTree();

  // Partially invalidate the pending layer. Tiles inside the invalidation rect
  // are created.
  SetupPendingTreeWithFixedTileSize(pending_raster_source, tile_size,
                                    invalidation_rect);

  // Partially occlude the pending layer in a different way.
  LayerImpl* pending_occluding_layer =
      host_impl()->pending_tree()->LayerById(active_occluding_layer->id());
  ASSERT_EQ(active_occluding_layer->bounds(),
            pending_occluding_layer->bounds());
  ASSERT_TRUE(pending_occluding_layer->draws_content());
  ASSERT_TRUE(pending_occluding_layer->contents_opaque());
  pending_occluding_layer->SetOffsetToTransformParent(
      pending_occluding_layer_position);
  pending_occluding_layer->NoteLayerPropertyChanged();

  EXPECT_EQ(1u, pending_layer()->num_tilings());
  EXPECT_EQ(2u, active_layer()->num_tilings());

  host_impl()->AdvanceToNextFrame(base::Milliseconds(1));
  // UpdateDrawProperties with the occluding layer.
  UpdateDrawProperties(host_impl()->pending_tree());

  float dest_scale = std::max(active_layer()->MaximumTilingContentsScale(),
                              pending_layer()->MaximumTilingContentsScale());
  gfx::Rect dest_layer_bounds =
      gfx::ScaleToEnclosingRect(gfx::Rect(layer_bounds), dest_scale);
  gfx::Rect dest_invalidation_rect =
      gfx::ScaleToEnclosingRect(invalidation_rect, dest_scale);

  // The expected number of occluded tiles on each of the 2 tilings for each of
  // the 3 tree priorities.
  size_t expected_occluded_tile_count_on_pending[] = {4u, 0u};
  size_t expected_occluded_tile_count_on_active[] = {12u, 3u};
  size_t total_expected_occluded_tile_count_on_trees[] = {15u, 4u};

  // Verify number of occluded tiles on the pending layer for each tiling.
  for (size_t i = 0; i < pending_layer()->num_tilings(); ++i) {
    PictureLayerTiling* tiling = pending_layer()->tilings()->tiling_at(i);
    auto prioritized_tiles =
        tiling->UpdateAndGetAllPrioritizedTilesForTesting();

    size_t occluded_tile_count_on_pending = 0u;
    for (PictureLayerTiling::CoverageIterator iter(tiling, dest_scale,
                                                   dest_layer_bounds);
         iter; ++iter) {
      Tile* tile = *iter;

      if (dest_invalidation_rect.Intersects(iter.geometry_rect()))
        EXPECT_TRUE(tile);
      else
        EXPECT_FALSE(tile);

      if (!tile)
        continue;
      if (prioritized_tiles[tile].is_occluded())
        occluded_tile_count_on_pending++;
    }
    EXPECT_EQ(expected_occluded_tile_count_on_pending[i],
              occluded_tile_count_on_pending)
        << tiling->contents_scale_key();
  }

  // Verify number of occluded tiles on the active layer for each tiling.
  for (size_t i = 0; i < active_layer()->num_tilings(); ++i) {
    PictureLayerTiling* tiling = active_layer()->tilings()->tiling_at(i);
    auto prioritized_tiles =
        tiling->UpdateAndGetAllPrioritizedTilesForTesting();

    size_t occluded_tile_count_on_active = 0u;
    for (PictureLayerTiling::CoverageIterator iter(tiling, dest_scale,
                                                   dest_layer_bounds);
         iter; ++iter) {
      Tile* tile = *iter;

      if (!tile)
        continue;
      if (prioritized_tiles[tile].is_occluded())
        occluded_tile_count_on_active++;
    }
    EXPECT_EQ(expected_occluded_tile_count_on_active[i],
              occluded_tile_count_on_active)
        << i;
  }

  std::vector<Tile*> all_tiles;
  for (size_t i = 0; i < pending_layer()->num_tilings(); ++i) {
    PictureLayerTiling* tiling = pending_layer()->tilings()->tiling_at(i);
    std::vector<Tile*> tiles = tiling->AllTilesForTesting();
    all_tiles.insert(all_tiles.end(), tiles.begin(), tiles.end());
  }
  for (size_t i = 0; i < active_layer()->num_tilings(); ++i) {
    PictureLayerTiling* tiling = active_layer()->tilings()->tiling_at(i);
    std::vector<Tile*> tiles = tiling->AllTilesForTesting();
    all_tiles.insert(all_tiles.end(), tiles.begin(), tiles.end());
  }

  host_impl()->tile_manager()->InitializeTilesWithResourcesForTesting(
      all_tiles);

  VerifyEvictionConsidersOcclusion(
      pending_layer(), PENDING_TREE,
      total_expected_occluded_tile_count_on_trees[PENDING_TREE], __LINE__);
  VerifyEvictionConsidersOcclusion(
      active_layer(), ACTIVE_TREE,
      total_expected_occluded_tile_count_on_trees[ACTIVE_TREE], __LINE__);

  // Repeat the tests without valid active tree priorities.
  active_layer()->set_has_valid_tile_priorities(false);
  VerifyEvictionConsidersOcclusion(
      pending_layer(), PENDING_TREE,
      total_expected_occluded_tile_count_on_trees[PENDING_TREE], __LINE__);
  VerifyEvictionConsidersOcclusion(
      active_layer(), ACTIVE_TREE,
      total_expected_occluded_tile_count_on_trees[ACTIVE_TREE], __LINE__);
  active_layer()->set_has_valid_tile_priorities(true);

  // Repeat the tests without valid pending tree priorities.
  pending_layer()->set_has_valid_tile_priorities(false);
  VerifyEvictionConsidersOcclusion(
      active_layer(), ACTIVE_TREE,
      total_expected_occluded_tile_count_on_trees[ACTIVE_TREE], __LINE__);
  VerifyEvictionConsidersOcclusion(
      pending_layer(), PENDING_TREE,
      total_expected_occluded_tile_count_on_trees[PENDING_TREE], __LINE__);
  pending_layer()->set_has_valid_tile_priorities(true);
}

TEST_F(LegacySWPictureLayerImplTest, PendingOrActiveTwinLayer) {
  gfx::Size layer_bounds(1000, 1000);

  scoped_refptr<FakeRasterSource> raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);
  SetupPendingTree(raster_source);
  EXPECT_FALSE(pending_layer()->GetPendingOrActiveTwinLayer());

  ActivateTree();
  EXPECT_FALSE(active_layer()->GetPendingOrActiveTwinLayer());

  SetupPendingTree(raster_source);
  EXPECT_TRUE(pending_layer()->GetPendingOrActiveTwinLayer());
  EXPECT_TRUE(active_layer()->GetPendingOrActiveTwinLayer());
  EXPECT_EQ(pending_layer(), active_layer()->GetPendingOrActiveTwinLayer());
  EXPECT_EQ(active_layer(), pending_layer()->GetPendingOrActiveTwinLayer());

  ActivateTree();
  EXPECT_FALSE(active_layer()->GetPendingOrActiveTwinLayer());

  // Make an empty pending tree.
  host_impl()->CreatePendingTree();
  pending_layer_ = old_pending_layer_ = nullptr;
  host_impl()->pending_tree()->DetachLayers();
  EXPECT_FALSE(active_layer()->GetPendingOrActiveTwinLayer());
}

void GetClientDataAndUpdateInvalidation(RecordingSource& recording_source,
                                        FakeContentLayerClient* client,
                                        gfx::Size layer_bounds) {
  Region invalidation;
  recording_source.Update(layer_bounds, /*recording_scale_factor=*/1.f, *client,
                          invalidation);
}

void PictureLayerImplTest::TestQuadsForSolidColor(bool test_for_solid,
                                                  bool partial_opaque) {
  host_impl()->AdvanceToNextFrame(base::Milliseconds(1));

  gfx::Size tile_size(100, 100);
  gfx::Size layer_bounds(200, 200);
  gfx::Rect layer_rect(layer_bounds);

  FakeContentLayerClient client;
  client.set_bounds(layer_bounds);
  scoped_refptr<PictureLayer> layer = PictureLayer::Create(&client);
  FakeLayerTreeHostClient host_client;
  TestTaskGraphRunner task_graph_runner;
  auto animation_host = AnimationHost::CreateForTesting(ThreadInstance::kMain);
  std::unique_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create(
      &host_client, &task_graph_runner, animation_host.get());
  host->SetRootLayer(layer);
  RecordingSource& recording_source = layer->GetRecordingSourceForTesting();

  client.set_fill_with_nonsolid_color(!test_for_solid);
  PaintFlags flags;
  flags.setColor(SK_ColorRED);
  if (test_for_solid)
    client.add_draw_rect(layer_rect, flags);

  GetClientDataAndUpdateInvalidation(recording_source, &client, layer_bounds);

  scoped_refptr<RasterSource> pending_raster_source =
      recording_source.CreateRasterSource();

  SetupPendingTreeWithFixedTileSize(pending_raster_source, tile_size, Region());
  ActivateTree();

  if (test_for_solid) {
    EXPECT_EQ(0u, active_layer()->tilings()->num_tilings());
  } else {
    ASSERT_TRUE(active_layer()->tilings());
    ASSERT_GT(active_layer()->tilings()->num_tilings(), 0u);
    std::vector<Tile*> tiles =
        active_layer()->HighResTiling()->AllTilesForTesting();
    EXPECT_FALSE(tiles.empty());

    std::vector<Tile*> resource_tiles;
    if (!partial_opaque) {
      resource_tiles = tiles;
    } else {
      size_t i = 0;
      for (auto it = tiles.begin(); it != tiles.end(); ++it, ++i) {
        if (i < 5) {
          TileDrawInfo& draw_info = (*it)->draw_info();
          draw_info.SetSolidColorForTesting(SkColors::kTransparent);
        } else {
          resource_tiles.push_back(*it);
        }
      }
    }
    host_impl()->tile_manager()->InitializeTilesWithResourcesForTesting(
        resource_tiles);
  }

  auto render_pass = viz::CompositorRenderPass::Create();
  AppendQuadsData data;
  active_layer()->WillDraw(DRAW_MODE_SOFTWARE, nullptr);
  active_layer()->AppendQuads(render_pass.get(), &data);
  active_layer()->DidDraw(nullptr);

  // Transparent quads should be eliminated.
  if (partial_opaque)
    EXPECT_EQ(4u, render_pass->quad_list.size());

  viz::DrawQuad::Material expected =
      test_for_solid ? viz::DrawQuad::Material::kSolidColor
                     : viz::DrawQuad::Material::kTiledContent;
  EXPECT_EQ(expected, render_pass->quad_list.front()->material);
}

TEST_F(LegacySWPictureLayerImplTest, DrawSolidQuads) {
  TestQuadsForSolidColor(true, false);
}

TEST_F(LegacySWPictureLayerImplTest, DrawNonSolidQuads) {
  TestQuadsForSolidColor(false, false);
}

TEST_F(LegacySWPictureLayerImplTest, DrawTransparentQuads) {
  TestQuadsForSolidColor(false, true);
}

TEST_F(LegacySWPictureLayerImplTest, NonSolidToSolidNoTilings) {
  host_impl()->AdvanceToNextFrame(base::Milliseconds(1));

  gfx::Size layer_bounds(200, 200);
  gfx::Rect layer_rect(layer_bounds);

  FakeContentLayerClient client;
  client.set_bounds(layer_bounds);
  scoped_refptr<PictureLayer> layer = PictureLayer::Create(&client);
  FakeLayerTreeHostClient host_client;
  TestTaskGraphRunner task_graph_runner;
  auto animation_host = AnimationHost::CreateForTesting(ThreadInstance::kMain);
  std::unique_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create(
      &host_client, &task_graph_runner, animation_host.get());
  host->SetRootLayer(layer);
  RecordingSource& recording_source = layer->GetRecordingSourceForTesting();

  client.set_fill_with_nonsolid_color(true);

  recording_source.SetNeedsDisplayRect(layer_rect);
  GetClientDataAndUpdateInvalidation(recording_source, &client, layer_bounds);

  scoped_refptr<RasterSource> raster_source1 =
      recording_source.CreateRasterSource();

  SetupPendingTree(raster_source1);
  ActivateTree();

  // We've started with a solid layer that contains some tilings.
  ASSERT_TRUE(active_layer()->tilings());
  EXPECT_NE(0u, active_layer()->tilings()->num_tilings());

  client.set_fill_with_nonsolid_color(false);

  recording_source.SetNeedsDisplayRect(layer_rect);
  GetClientDataAndUpdateInvalidation(recording_source, &client, layer_bounds);

  scoped_refptr<RasterSource> raster_source2 =
      recording_source.CreateRasterSource();

  SetupPendingTree(raster_source2);
  ActivateTree();

  // We've switched to a solid color, so we should end up with no tilings.
  ASSERT_TRUE(active_layer()->tilings());
  EXPECT_EQ(0u, active_layer()->tilings()->num_tilings());
}

TEST_F(LegacySWPictureLayerImplTest, ChangeInViewportAllowsTilingUpdates) {
  host_impl()->AdvanceToNextFrame(base::Milliseconds(1));

  gfx::Size layer_bounds(400, 4000);
  SetupDefaultTrees(layer_bounds);

  Region invalidation;
  gfx::Rect viewport = gfx::Rect(0, 0, 100, 100);
  gfx::Transform transform;

  host_impl()->SetRequiresHighResToDraw();

  // Update tiles.
  pending_layer()->draw_properties().visible_layer_rect = viewport;
  pending_layer()->draw_properties().screen_space_transform = transform;
  SetupDrawPropertiesAndUpdateTiles(pending_layer(), 1.f, 1.f, 1.f);
  pending_layer()->HighResTiling()->UpdateAllRequiredStateForTesting();

  // Ensure we can't activate.
  EXPECT_FALSE(host_impl()->tile_manager()->IsReadyToActivate());

  // Now in the same frame, move the viewport (this can happen during
  // animation).
  viewport = gfx::Rect(0, 2000, 100, 100);

  // Update tiles.
  pending_layer()->draw_properties().visible_layer_rect = viewport;
  pending_layer()->draw_properties().screen_space_transform = transform;
  SetupDrawPropertiesAndUpdateTiles(pending_layer(), 1.f, 1.f, 1.f);
  pending_layer()->HighResTiling()->UpdateAllRequiredStateForTesting();

  // Make sure all viewport tiles (viewport from the tiling) are ready to draw.
  std::vector<Tile*> tiles;
  for (PictureLayerTiling::CoverageIterator iter(
           pending_layer()->HighResTiling(), 1.f,
           pending_layer()->HighResTiling()->GetCurrentVisibleRectForTesting());
       iter; ++iter) {
    if (*iter)
      tiles.push_back(*iter);
  }
  for (PictureLayerTiling::CoverageIterator iter(
           active_layer()->HighResTiling(), 1.f,
           active_layer()->HighResTiling()->GetCurrentVisibleRectForTesting());
       iter; ++iter) {
    if (*iter)
      tiles.push_back(*iter);
  }

  host_impl()->tile_manager()->InitializeTilesWithResourcesForTesting(tiles);

  // Ensure we can activate.
  EXPECT_TRUE(host_impl()->tile_manager()->IsReadyToActivate());
}

TEST_F(LegacySWPictureLayerImplTest, CloneMissingRecordings) {
  gfx::Size tile_size(100, 100);
  gfx::Size layer_bounds(400, 400);

  scoped_refptr<FakeRasterSource> filled_raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);

  scoped_refptr<FakeRasterSource> partial_raster_source =
      FakeRasterSource::CreatePartiallyFilled(layer_bounds,
                                              gfx::Rect(100, 100, 300, 300));

  SetupPendingTreeWithFixedTileSize(filled_raster_source, tile_size, Region());
  ActivateTree();

  PictureLayerTiling* pending_tiling = old_pending_layer()->HighResTiling();
  PictureLayerTiling* active_tiling = active_layer()->HighResTiling();

  // We should have all tiles on active, and none on pending.
  EXPECT_EQ(0u, pending_tiling->AllTilesForTesting().size());
  EXPECT_EQ(5u * 5u, active_tiling->AllTilesForTesting().size());

  // Now put a partially-recorded raster source on the pending tree (and
  // invalidate everything, since the main thread recording will invalidate
  // dropped recordings). This will cause us to be missing some tiles.
  SetupPendingTreeWithFixedTileSize(partial_raster_source, tile_size,
                                    Region(gfx::Rect(layer_bounds)));
  EXPECT_EQ(4u * 4u, pending_tiling->AllTilesForTesting().size());

  // Active is not affected yet.
  EXPECT_EQ(5u * 5u, active_tiling->AllTilesForTesting().size());

  // Activate the tree. The same tiles go missing on the active tree.
  ActivateTree();
  EXPECT_EQ(4u * 4u, active_tiling->AllTilesForTesting().size());

  // Now put a full recording on the pending tree again. We'll get all our tiles
  // back.
  SetupPendingTreeWithFixedTileSize(filled_raster_source, tile_size,
                                    Region(gfx::Rect(layer_bounds)));
  EXPECT_EQ(5u * 5u, pending_tiling->AllTilesForTesting().size());
  Tile::Id tile00 = pending_tiling->TileAt(0, 0)->id();
  Tile::Id tile11 = pending_tiling->TileAt(1, 1)->id();
  Tile::Id tile22 = pending_tiling->TileAt(2, 2)->id();

  // Active is not affected yet.
  EXPECT_EQ(4u * 4u, active_tiling->AllTilesForTesting().size());

  // Activate the tree. The tiles are moved to the active tree.
  ActivateTree();
  EXPECT_EQ(5u * 5u, active_tiling->AllTilesForTesting().size());
  EXPECT_EQ(tile00, active_tiling->TileAt(0, 0)->id());
  EXPECT_EQ(tile11, active_tiling->TileAt(1, 1)->id());
  EXPECT_EQ(tile22, active_tiling->TileAt(2, 2)->id());
}

TEST_F(LegacySWPictureLayerImplTest,
       DirectlyCompositedImageRasterSourceCoverage) {
  gfx::Size tile_size(100, 100);
  gfx::Size layer_bounds(400, 400);

  scoped_refptr<FakeRasterSource> filled_raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);
  filled_raster_source->SetDirectlyCompositedImageDefaultRasterScale(
      gfx::Vector2dF(1, 1));

  scoped_refptr<FakeRasterSource> partial_raster_source =
      FakeRasterSource::CreatePartiallyFilled(layer_bounds,
                                              gfx::Rect(150, 150, 100, 100));
  partial_raster_source->SetDirectlyCompositedImageDefaultRasterScale(
      gfx::Vector2dF(1, 1));

  SetupPendingTreeWithFixedTileSize(filled_raster_source, tile_size, Region());
  ASSERT_TRUE(pending_layer()->IsDirectlyCompositedImage());
  ActivateTree();
  ASSERT_TRUE(active_layer()->IsDirectlyCompositedImage());

  PictureLayerTiling* pending_tiling = old_pending_layer()->HighResTiling();
  PictureLayerTiling* active_tiling = active_layer()->HighResTiling();

  // We should have all tiles on active, and none on pending.
  EXPECT_EQ(0u, pending_tiling->AllTilesForTesting().size());
  EXPECT_EQ(5u * 5u, active_tiling->AllTilesForTesting().size());

  // Now put a partially-recorded raster source on the pending tree (and
  // invalidate everything, since the main thread recording will invalidate
  // dropped recordings). Because the layer is a directly composited image, all
  // tiles should be created.
  SetupPendingTreeWithFixedTileSize(partial_raster_source, tile_size,
                                    Region(gfx::Rect(layer_bounds)));
  ASSERT_TRUE(pending_layer()->IsDirectlyCompositedImage());

  EXPECT_EQ(5u * 5u, pending_tiling->AllTilesForTesting().size());

  // Activate the tree. The same tiles should have been moved to active tree.
  EXPECT_EQ(5u * 5u, pending_tiling->AllTilesForTesting().size());
  Tile::Id tile00 = pending_tiling->TileAt(0, 0)->id();
  Tile::Id tile11 = pending_tiling->TileAt(1, 1)->id();
  Tile::Id tile22 = pending_tiling->TileAt(2, 2)->id();

  // Activate the tree. The tiles are moved to the active tree.
  ActivateTree();
  EXPECT_EQ(5u * 5u, active_tiling->AllTilesForTesting().size());
  EXPECT_EQ(tile00, active_tiling->TileAt(0, 0)->id());
  EXPECT_EQ(tile11, active_tiling->TileAt(1, 1)->id());
  EXPECT_EQ(tile22, active_tiling->TileAt(2, 2)->id());
}

TEST_F(LegacySWPictureLayerImplTest, ScrollPastLiveTilesRectAndBack) {
  host_impl()->AdvanceToNextFrame(base::Milliseconds(1));

  gfx::Size tile_size(102, 102);
  gfx::Size layer_bounds(100, 100);
  gfx::Size viewport_size(100, 100);

  host_impl()->active_tree()->SetDeviceViewportRect(gfx::Rect(viewport_size));
  SetInitialDeviceScaleFactor(1.f);

  scoped_refptr<FakeRasterSource> pending_raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);
  scoped_refptr<FakeRasterSource> active_raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);

  SetupPendingTreeWithFixedTileSize(active_raster_source, tile_size, Region());

  ActivateTree();
  EXPECT_TRUE(active_layer()->HighResTiling()->has_tiles());

  host_impl()->SetExternalTilePriorityConstraints(gfx::Rect(0, 5000, 100, 100),
                                                  gfx::Transform());

  SetupPendingTreeWithFixedTileSize(pending_raster_source, tile_size,
                                    gfx::Rect());

  EXPECT_FALSE(pending_layer()->HighResTiling()->has_tiles());
  EXPECT_TRUE(pending_layer()->HighResTiling()->live_tiles_rect().IsEmpty());
  ActivateTree();
  EXPECT_FALSE(active_layer()->HighResTiling()->has_tiles());
  EXPECT_TRUE(active_layer()->HighResTiling()->live_tiles_rect().IsEmpty());

  host_impl()->SetExternalTilePriorityConstraints(gfx::Rect(0, 110, 100, 100),
                                                  gfx::Transform());

  SetupPendingTreeWithFixedTileSize(pending_raster_source, tile_size,
                                    gfx::Rect());

  EXPECT_FALSE(pending_layer()->HighResTiling()->has_tiles());
  EXPECT_FALSE(pending_layer()->HighResTiling()->live_tiles_rect().IsEmpty());
  ActivateTree();
  EXPECT_TRUE(active_layer()->HighResTiling()->has_tiles());
  EXPECT_FALSE(active_layer()->HighResTiling()->live_tiles_rect().IsEmpty());
}

TEST_F(LegacySWPictureLayerImplTest, ScrollPropagatesToPending) {
  host_impl()->AdvanceToNextFrame(base::Milliseconds(1));

  gfx::Size layer_bounds(1000, 1000);
  gfx::Size viewport_size(100, 100);

  host_impl()->active_tree()->SetDeviceViewportRect(gfx::Rect(viewport_size));
  SetInitialDeviceScaleFactor(1.f);

  SetupDefaultTrees(layer_bounds);

  active_layer()->SetCurrentScrollOffset(gfx::PointF(0.0, 50.0));
  UpdateDrawProperties(host_impl()->active_tree());
  EXPECT_EQ("0,50 100x100", active_layer()
                                ->HighResTiling()
                                ->GetCurrentVisibleRectForTesting()
                                .ToString());

  EXPECT_EQ("0,0 100x100", pending_layer()
                               ->HighResTiling()
                               ->GetCurrentVisibleRectForTesting()
                               .ToString());
  // Scroll offset in property trees is not propagated from the active tree to
  // the pending tree.
  SetScrollOffset(pending_layer(), CurrentScrollOffset(active_layer()));
  UpdateDrawProperties(host_impl()->pending_tree());
  EXPECT_EQ("0,50 100x100", pending_layer()
                                ->HighResTiling()
                                ->GetCurrentVisibleRectForTesting()
                                .ToString());
}

TEST_F(LegacySWPictureLayerImplTest, UpdateLCDTextInvalidatesPendingTree) {
  gfx::Size layer_bounds(100, 100);
  SetupPendingTree(FakeRasterSource::CreateFilledWithText(layer_bounds));
  // LCD text is disallowed before SetContentsOpaque(true).
  EXPECT_FALSE(pending_layer()->can_use_lcd_text());
  pending_layer()->SetContentsOpaque(true);
  pending_layer()->UpdateTiles();

  EXPECT_TRUE(pending_layer()->can_use_lcd_text());
  EXPECT_TRUE(pending_layer()->HighResTiling()->has_tiles());
  std::vector<Tile*> tiles =
      pending_layer()->HighResTiling()->AllTilesForTesting();
  for (Tile* tile : tiles)
    EXPECT_TRUE(tile->can_use_lcd_text());

  pending_layer()->SetContentsOpaque(false);
  pending_layer()->UpdateTiles();

  EXPECT_FALSE(pending_layer()->can_use_lcd_text());
  EXPECT_TRUE(pending_layer()->HighResTiling()->has_tiles());
  for (Tile* tile : pending_layer()->HighResTiling()->AllTilesForTesting())
    EXPECT_FALSE(tile->can_use_lcd_text());

  // Change of the specific LCD text disallowed reason should not invalidate
  // tilings.
  pending_layer()->SetContentsOpaque(true);
  FilterOperations blur_filter;
  blur_filter.Append(FilterOperation::CreateBlurFilter(4.0f));
  SetFilter(pending_layer(), blur_filter);
  UpdateDrawProperties(host_impl()->pending_tree());
  EXPECT_FALSE(pending_layer()->can_use_lcd_text());
  EXPECT_TRUE(pending_layer()->HighResTiling()->has_tiles());
  for (Tile* tile : pending_layer()->HighResTiling()->AllTilesForTesting())
    EXPECT_FALSE(tile->can_use_lcd_text());

  SetFilter(pending_layer(), FilterOperations());
  UpdateDrawProperties(host_impl()->pending_tree());
  EXPECT_TRUE(pending_layer()->can_use_lcd_text());
  EXPECT_TRUE(pending_layer()->HighResTiling()->has_tiles());
  for (Tile* tile : pending_layer()->HighResTiling()->AllTilesForTesting())
    EXPECT_TRUE(tile->can_use_lcd_text());
}

TEST_F(LegacySWPictureLayerImplTest, UpdateLCDTextPushToActiveTree) {
  SetupPendingTree(FakeRasterSource::CreateFilledWithText(gfx::Size(200, 200)));
  float page_scale = 4.f;
  SetupDrawPropertiesAndUpdateTiles(pending_layer(), page_scale, 1.0f,
                                    page_scale);
  // LCD text is disallowed before SetContentsOpaque(true).
  EXPECT_FALSE(pending_layer()->can_use_lcd_text());
  pending_layer()->SetContentsOpaque(true);
  pending_layer()->UpdateTiles();
  EXPECT_TRUE(pending_layer()->can_use_lcd_text());
  EXPECT_TRUE(pending_layer()->HighResTiling()->can_use_lcd_text());
  ActivateTree();

  EXPECT_TRUE(active_layer()->can_use_lcd_text());
  ASSERT_EQ(2u, active_layer()->tilings()->num_tilings());
  ASSERT_TRUE(active_layer()->HighResTiling()->has_tiles());
  std::vector<Tile*> tiles =
      active_layer()->HighResTiling()->AllTilesForTesting();
  for (Tile* tile : tiles)
    EXPECT_TRUE(tile->can_use_lcd_text());
  for (Tile* tile : active_layer()->LowResTiling()->AllTilesForTesting())
    EXPECT_FALSE(tile->can_use_lcd_text());

  SetupPendingTree(FakeRasterSource::CreateFilledWithText(gfx::Size(200, 200)));
  SetupDrawPropertiesAndUpdateTiles(pending_layer(), page_scale, 1.0f,
                                    page_scale);
  pending_layer()->SetContentsOpaque(false);
  pending_layer()->UpdateTiles();
  EXPECT_FALSE(pending_layer()->can_use_lcd_text());
  EXPECT_FALSE(pending_layer()->HighResTiling()->can_use_lcd_text());
  ActivateTree();

  EXPECT_FALSE(active_layer()->can_use_lcd_text());
  ASSERT_EQ(2u, active_layer()->tilings()->num_tilings());
  ASSERT_TRUE(active_layer()->HighResTiling()->has_tiles());
  for (Tile* tile : active_layer()->HighResTiling()->AllTilesForTesting())
    EXPECT_FALSE(tile->can_use_lcd_text());
  for (Tile* tile : active_layer()->LowResTiling()->AllTilesForTesting())
    EXPECT_FALSE(tile->can_use_lcd_text());
}

TEST_F(LegacySWPictureLayerImplTest, UpdateLCDTextPushToActiveTreeWith2dScale) {
  SetupPendingTree(FakeRasterSource::CreateFilledWithText(gfx::Size(200, 200)));
  gfx::Vector2dF ideal_scale(4.f, 2.f);
  float page_scale = 4.f;
  SetupDrawPropertiesAndUpdateTiles(pending_layer(), ideal_scale, 1.0f,
                                    page_scale);
  // LCD text is disallowed before SetContentsOpaque(true).
  EXPECT_FALSE(pending_layer()->can_use_lcd_text());
  pending_layer()->SetContentsOpaque(true);
  pending_layer()->UpdateTiles();
  EXPECT_TRUE(pending_layer()->can_use_lcd_text());
  EXPECT_TRUE(pending_layer()->HighResTiling()->can_use_lcd_text());
  ActivateTree();

  EXPECT_TRUE(active_layer()->can_use_lcd_text());
  ASSERT_EQ(2u, active_layer()->tilings()->num_tilings());
  ASSERT_TRUE(active_layer()->HighResTiling()->has_tiles());
  std::vector<Tile*> tiles =
      active_layer()->HighResTiling()->AllTilesForTesting();
  for (Tile* tile : tiles)
    EXPECT_TRUE(tile->can_use_lcd_text());
  for (Tile* tile : active_layer()->LowResTiling()->AllTilesForTesting())
    EXPECT_FALSE(tile->can_use_lcd_text());

  SetupPendingTree(FakeRasterSource::CreateFilledWithText(gfx::Size(200, 200)));
  SetupDrawPropertiesAndUpdateTiles(pending_layer(), ideal_scale, 1.0f,
                                    page_scale);
  pending_layer()->SetContentsOpaque(false);
  pending_layer()->UpdateTiles();
  EXPECT_FALSE(pending_layer()->can_use_lcd_text());
  EXPECT_FALSE(pending_layer()->HighResTiling()->can_use_lcd_text());
  ActivateTree();

  EXPECT_FALSE(active_layer()->can_use_lcd_text());
  ASSERT_EQ(2u, active_layer()->tilings()->num_tilings());
  ASSERT_TRUE(active_layer()->HighResTiling()->has_tiles());
  for (Tile* tile : active_layer()->HighResTiling()->AllTilesForTesting())
    EXPECT_FALSE(tile->can_use_lcd_text());
  for (Tile* tile : active_layer()->LowResTiling()->AllTilesForTesting())
    EXPECT_FALSE(tile->can_use_lcd_text());
}

TEST_F(LegacySWPictureLayerImplTest, TilingAllTilesDone) {
  gfx::Size tile_size = host_impl()->settings().default_tile_size;
  size_t tile_mem = 4 * tile_size.width() * tile_size.height();
  gfx::Size layer_bounds(1000, 1000);

  // Create tiles.
  scoped_refptr<FakeRasterSource> pending_raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);
  SetupPendingTree(pending_raster_source);
  pending_layer()->SetBounds(layer_bounds);
  ActivateTree();
  host_impl()->tile_manager()->InitializeTilesWithResourcesForTesting(
      active_layer()->HighResTiling()->AllTilesForTesting());
  host_impl()->SetTreePriority(SAME_PRIORITY_FOR_BOTH_TREES);

  EXPECT_FALSE(active_layer()->HighResTiling()->all_tiles_done());

  {
    // Set a memory policy that will fit all tiles.
    size_t max_tiles = 16;
    size_t memory_limit = max_tiles * tile_mem;
    ManagedMemoryPolicy policy(memory_limit,
                               gpu::MemoryAllocation::CUTOFF_ALLOW_EVERYTHING,
                               max_tiles);
    host_impl()->SetMemoryPolicy(policy);
    host_impl()->PrepareTiles();

    EXPECT_TRUE(active_layer()->HighResTiling()->all_tiles_done());
  }

  {
    // Set a memory policy that will cause tile eviction.
    size_t max_tiles = 1;
    size_t memory_limit = max_tiles * tile_mem;
    ManagedMemoryPolicy policy(memory_limit,
                               gpu::MemoryAllocation::CUTOFF_ALLOW_EVERYTHING,
                               max_tiles);
    host_impl()->SetMemoryPolicy(policy);
    host_impl()->PrepareTiles();

    EXPECT_FALSE(active_layer()->HighResTiling()->all_tiles_done());
  }
}

class TileSizeTest : public PictureLayerImplTest {
 public:
  LayerTreeSettings CreateSettings() override {
    LayerTreeSettings settings = PictureLayerImplTest::CreateSettings();
    settings.default_tile_size = gfx::Size(100, 100);
    settings.max_untiled_layer_size = gfx::Size(200, 200);
    return settings;
  }
};

class LegacySWTileSizeTest : public TileSizeTest {
 public:
  LayerTreeSettings CreateSettings() override {
    LayerTreeSettings settings = TileSizeTest::CreateSettings();
    settings.gpu_rasterization_disabled = true;
    return settings;
  }
};

TEST_F(LegacySWTileSizeTest, SWTileSizes) {
  SetupPendingTree();
  auto* layer = pending_layer();

  host_impl()->active_tree()->SetDeviceViewportRect(gfx::Rect(1000, 1000));
  gfx::Size result;

  host_impl()->CommitComplete();
  EXPECT_FALSE(host_impl()->use_gpu_rasterization());
  host_impl()->NotifyReadyToActivate();

  // Default tile-size for large layers.
  result = layer->CalculateTileSize(gfx::Size(10000, 10000));
  EXPECT_EQ(result.width(), 100);
  EXPECT_EQ(result.height(), 100);
  // Don't tile and round-up, when under max_untiled_layer_size.
  result = layer->CalculateTileSize(gfx::Size(42, 42));
  EXPECT_EQ(result.width(), 64);
  EXPECT_EQ(result.height(), 64);
  result = layer->CalculateTileSize(gfx::Size(191, 191));
  EXPECT_EQ(result.width(), 192);
  EXPECT_EQ(result.height(), 192);
  result = layer->CalculateTileSize(gfx::Size(199, 199));
  EXPECT_EQ(result.width(), 200);
  EXPECT_EQ(result.height(), 200);
}

TEST_F(TileSizeTest, GPUTileSizes) {
  SetupPendingTree();
  auto* layer = pending_layer();

  host_impl()->active_tree()->SetDeviceViewportRect(gfx::Rect(1000, 1000));
  gfx::Size result;

  host_impl()->CommitComplete();

  // Gpu-rasterization uses 25% viewport-height tiles.
  // The +2's below are for border texels.
  EXPECT_TRUE(host_impl()->use_gpu_rasterization());
  host_impl()->active_tree()->SetDeviceViewportRect(gfx::Rect(2000, 2000));
  host_impl()->NotifyReadyToActivate();

  layer->set_gpu_raster_max_texture_size(
      host_impl()->active_tree()->GetDeviceViewport().size());
  result = layer->CalculateTileSize(gfx::Size(10000, 10000));
  EXPECT_EQ(result.width(),
            MathUtil::UncheckedRoundUp(
                1000 + 2 * PictureLayerTiling::kBorderTexels, 32));
  EXPECT_EQ(result.height(), 512);  // 500 + 2, 32-byte aligned.

  // Clamp and round-up, when smaller than viewport.
  // Tile-height doubles to 50% when width shrinks to <= 50%.
  host_impl()->active_tree()->SetDeviceViewportRect(gfx::Rect(1000, 1000));
  layer->set_gpu_raster_max_texture_size(
      host_impl()->active_tree()->GetDeviceViewport().size());
  result = layer->CalculateTileSize(gfx::Size(447, 10000));
  EXPECT_EQ(result.width(), 448);
  EXPECT_EQ(result.height(), 512);  // 500 + 2, 32-byte aliged.

  // Largest layer is 50% of viewport width (rounded up), and
  // 50% of viewport in height.
  result = layer->CalculateTileSize(gfx::Size(447, 400));
  EXPECT_EQ(result.width(), 448);
  EXPECT_EQ(result.height(), 448);
  result = layer->CalculateTileSize(gfx::Size(500, 499));
  EXPECT_EQ(result.width(), 512);
  EXPECT_EQ(result.height(), 512);  // 500 + 2, 32-byte aligned.
}

TEST_F(TileSizeTest, RawDrawTileSizes) {
  base::test::ScopedFeatureList scoped_feature_list(features::kRawDraw);
  SetupPendingTree();
  auto* layer = pending_layer();

  host_impl()->active_tree()->SetDeviceViewportRect(gfx::Rect(1000, 1000));
  gfx::Size result;

  host_impl()->CommitComplete();

  constexpr int kMaxTextureSize = 2048;
  // Gpu-RawDraw-rasterization uses 100%
  // std::max({viewport-width, viewport-height, 2280}) tiles.
  EXPECT_TRUE(host_impl()->use_gpu_rasterization());
  gfx::Rect viewport_rect(2000, 2000);
  host_impl()->active_tree()->SetDeviceViewportRect(viewport_rect);
  layer->set_gpu_raster_max_texture_size(viewport_rect.size());
  host_impl()->NotifyReadyToActivate();

  gfx::Size content_bounds(10000, 10000);
  result = layer->CalculateTileSize(content_bounds);
  EXPECT_EQ(result, gfx::Size(kMaxTextureSize, kMaxTextureSize));

  viewport_rect = gfx::Rect(1000, 1000);
  host_impl()->active_tree()->SetDeviceViewportRect(viewport_rect);
  layer->set_gpu_raster_max_texture_size(gfx::Size());
  host_impl()->NotifyReadyToActivate();
  content_bounds = gfx::Size(1600, 100);

  result = layer->CalculateTileSize(content_bounds);
  EXPECT_EQ(result, gfx::Size(1600, 128));

  viewport_rect = gfx::Rect(1000, 1000);
  host_impl()->active_tree()->SetDeviceViewportRect(viewport_rect);
  layer->set_gpu_raster_max_texture_size(gfx::Size());
  host_impl()->NotifyReadyToActivate();
  content_bounds = gfx::Size(std::numeric_limits<int>::max(),
                             std::numeric_limits<int>::max());

  result = layer->CalculateTileSize(content_bounds);
  EXPECT_EQ(result, gfx::Size(kMaxTextureSize, kMaxTextureSize));
}

class HalfWidthTileTest : public PictureLayerImplTest {};

TEST_F(HalfWidthTileTest, TileSizes) {
  SetupPendingTree();
  auto* layer = pending_layer();

  gfx::Size result;
  host_impl()->CommitComplete();
  EXPECT_TRUE(host_impl()->use_gpu_rasterization());
  host_impl()->active_tree()->SetDeviceViewportRect(gfx::Rect(2000, 2000));
  host_impl()->NotifyReadyToActivate();

  // Basic test.
  layer->set_gpu_raster_max_texture_size(
      host_impl()->active_tree()->GetDeviceViewport().size());
  result = layer->CalculateTileSize(gfx::Size(10000, 10000));
  EXPECT_EQ(result.width(),
            MathUtil::UncheckedRoundUp(
                2000 / 2 + 2 * PictureLayerTiling::kBorderTexels, 32));
  EXPECT_EQ(result.height(), 512);

  // When using odd sized viewport bounds, we should round up.
  host_impl()->active_tree()->SetDeviceViewportRect(gfx::Rect(509, 1000));
  layer->set_gpu_raster_max_texture_size(
      host_impl()->active_tree()->GetDeviceViewport().size());
  result = layer->CalculateTileSize(gfx::Size(10000, 10000));
  EXPECT_EQ(result.width(), 288);
  EXPECT_EQ(result.height(), 256);

  // If content would fit in a single tile after rounding, we shouldn't halve
  // the tile width.
  host_impl()->active_tree()->SetDeviceViewportRect(gfx::Rect(511, 1000));
  layer->set_gpu_raster_max_texture_size(
      host_impl()->active_tree()->GetDeviceViewport().size());
  result = layer->CalculateTileSize(gfx::Size(530, 10000));
  EXPECT_EQ(result.width(), 544);
  EXPECT_EQ(result.height(), 256);
}

TEST_F(NoLowResPictureLayerImplTest, LowResWasHighResCollision) {
  gfx::Size layer_bounds(1300, 1900);

  float low_res_factor = host_impl()->settings().low_res_contents_scale_factor;
  SetupDefaultTrees(layer_bounds);
  ResetTilingsAndRasterScales();

  float page_scale = 2.f;
  SetContentsScaleOnBothLayers(page_scale, 1.0f, page_scale);
  EXPECT_BOTH_EQ(num_tilings(), 1u);
  EXPECT_BOTH_EQ(tilings()->tiling_at(0)->contents_scale_key(), page_scale);

  // Since this test simulates a pinch it needs an input handler.
  // TODO(bokan): This is a raster unit test, it shouldn't be using a real
  // input handler.
  InputHandler::Create(static_cast<CompositorDelegateForInput&>(*host_impl()));

  host_impl()->GetInputHandler().PinchGestureBegin(
      gfx::Point(), ui::ScrollInputType::kTouchscreen);

  // Zoom out to exactly the low res factor so that the previous high res
  // would be equal to the current low res (if it were possible to have one).
  float zoomed = page_scale / low_res_factor;
  SetContentsScaleOnBothLayers(zoomed, 1.0f, zoomed);
  EXPECT_EQ(1u, pending_layer()->num_tilings());
  EXPECT_EQ(zoomed,
            pending_layer()->tilings()->tiling_at(0)->contents_scale_key());
}

TEST_F(LegacySWPictureLayerImplTest, HighResWasLowResCollision) {
  gfx::Size layer_bounds(1300, 1900);

  float low_res_factor = host_impl()->settings().low_res_contents_scale_factor;

  SetupDefaultTrees(layer_bounds);
  ResetTilingsAndRasterScales();

  float page_scale = 4.f;
  float low_res = page_scale * low_res_factor;
  float extra_low_res = low_res * low_res_factor;
  SetupDrawPropertiesAndUpdateTiles(active_layer(), page_scale, 1.0f,
                                    page_scale);
  EXPECT_EQ(2u, active_layer()->tilings()->num_tilings());
  EXPECT_EQ(page_scale,
            active_layer()->tilings()->tiling_at(0)->contents_scale_key());
  EXPECT_EQ(low_res,
            active_layer()->tilings()->tiling_at(1)->contents_scale_key());

  // Grab a current low res tile.
  PictureLayerTiling* old_low_res_tiling =
      active_layer()->tilings()->tiling_at(1);
  Tile::Id old_low_res_tile_id =
      active_layer()->tilings()->tiling_at(1)->TileAt(0, 0)->id();

  // The tiling knows it has low res content.
  EXPECT_TRUE(active_layer()
                  ->tilings()
                  ->tiling_at(1)
                  ->may_contain_low_resolution_tiles());

  host_impl()->AdvanceToNextFrame(base::Milliseconds(1));

  // Zoom in to exactly the low res factor so that the previous low res
  // would be equal to the current high res.
  SetupDrawPropertiesAndUpdateTiles(active_layer(), low_res, 1.0f, low_res);
  // 3 tilings. The old high res, the new high res (old low res) and the new low
  // res.
  EXPECT_EQ(3u, active_layer()->num_tilings());

  PictureLayerTilingSet* tilings = active_layer()->tilings();
  EXPECT_EQ(page_scale, tilings->tiling_at(0)->contents_scale_key());
  EXPECT_EQ(low_res, tilings->tiling_at(1)->contents_scale_key());
  EXPECT_EQ(extra_low_res, tilings->tiling_at(2)->contents_scale_key());

  EXPECT_EQ(NON_IDEAL_RESOLUTION, tilings->tiling_at(0)->resolution());
  EXPECT_EQ(HIGH_RESOLUTION, tilings->tiling_at(1)->resolution());
  EXPECT_EQ(LOW_RESOLUTION, tilings->tiling_at(2)->resolution());

  // The old low res tile was destroyed and replaced.
  EXPECT_EQ(old_low_res_tiling, tilings->tiling_at(1));
  EXPECT_NE(old_low_res_tile_id, tilings->tiling_at(1)->TileAt(0, 0)->id());
  EXPECT_TRUE(tilings->tiling_at(1)->TileAt(0, 0));

  // New high res tiling.
  EXPECT_FALSE(tilings->tiling_at(0)->may_contain_low_resolution_tiles());
  // New low res tiling.
  EXPECT_TRUE(tilings->tiling_at(2)->may_contain_low_resolution_tiles());

  // This tiling will be high res now, it won't contain low res content since it
  // was all destroyed.
  EXPECT_FALSE(tilings->tiling_at(1)->may_contain_low_resolution_tiles());
}

TEST_F(LegacySWPictureLayerImplTest, CompositedImageCalculateContentsScale) {
  gfx::Size layer_bounds(400, 400);
  gfx::Rect layer_rect(layer_bounds);

  host_impl()->active_tree()->SetDeviceViewportRect(layer_rect);

  scoped_refptr<FakeRasterSource> pending_raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);
  pending_raster_source->SetDirectlyCompositedImageDefaultRasterScale(
      gfx::Vector2dF(1, 1));
  SetupPendingTree(pending_raster_source);
  ASSERT_TRUE(pending_layer()->IsDirectlyCompositedImage());

  SetupDrawPropertiesAndUpdateTiles(pending_layer(), 2.f, 3.f, 4.f);
  EXPECT_FLOAT_EQ(1.f, pending_layer()->MaximumTilingContentsScale());
}

TEST_F(LegacySWPictureLayerImplTest, CompositedImageIgnoreIdealContentsScale) {
  gfx::Size layer_bounds(400, 400);
  gfx::Rect layer_rect(layer_bounds);

  host_impl()->active_tree()->SetDeviceViewportRect(layer_rect);

  scoped_refptr<FakeRasterSource> pending_raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);
  pending_raster_source->SetDirectlyCompositedImageDefaultRasterScale(
      gfx::Vector2dF(1, 1));
  SetupPendingTree(pending_raster_source);

  LayerTreeImpl* pending_tree = host_impl()->pending_tree();
  const int kLayerId = 100;

  std::unique_ptr<FakePictureLayerImpl> pending_layer =
      FakePictureLayerImpl::Create(pending_tree, kLayerId,
                                   pending_raster_source);
  pending_layer->SetDrawsContent(true);
  // This is needed after SetDrawsContent(true) which changes CanHaveTilings().
  pending_layer->UpdateDirectlyCompositedImageFromRasterSource();
  ASSERT_TRUE(pending_layer->IsDirectlyCompositedImage());
  FakePictureLayerImpl* pending_layer_ptr = pending_layer.get();
  pending_tree->AddLayer(std::move(pending_layer));
  CopyProperties(pending_tree->root_layer(), pending_layer_ptr);

  UpdateDrawProperties(pending_tree);

  // Set PictureLayerImpl::ideal_contents_scale_ to 2.f.
  const float suggested_ideal_contents_scale = 2.f;
  const float device_scale_factor = 3.f;
  const float page_scale_factor = 4.f;
  SetupDrawPropertiesAndUpdateTiles(pending_layer_ptr,
                                    suggested_ideal_contents_scale,
                                    device_scale_factor, page_scale_factor);
  EXPECT_EQ(1.f,
            pending_layer_ptr->tilings()->tiling_at(0)->contents_scale_key());

  // Push to active layer.
  host_impl()->ActivateSyncTree();

  FakePictureLayerImpl* active_layer = static_cast<FakePictureLayerImpl*>(
      host_impl()->active_tree()->LayerById(kLayerId));
  SetupDrawPropertiesAndUpdateTiles(active_layer,
                                    suggested_ideal_contents_scale,
                                    device_scale_factor, page_scale_factor);
  EXPECT_EQ(1.f, active_layer->tilings()->tiling_at(0)->contents_scale_key());
  active_layer->set_visible_layer_rect(layer_rect);

  // Create resources for the tiles.
  host_impl()->tile_manager()->InitializeTilesWithResourcesForTesting(
      active_layer->tilings()->tiling_at(0)->AllTilesForTesting());

  // Draw.
  auto render_pass = viz::CompositorRenderPass::Create();
  AppendQuadsData data;
  active_layer->WillDraw(DRAW_MODE_SOFTWARE, nullptr);
  active_layer->AppendQuads(render_pass.get(), &data);
  active_layer->DidDraw(nullptr);

  ASSERT_FALSE(render_pass->quad_list.empty());
  EXPECT_EQ(viz::DrawQuad::Material::kTiledContent,
            render_pass->quad_list.front()->material);

  // Tiles are ready at correct scale, so should not set had_incomplete_tile.
  EXPECT_EQ(0, data.num_incompletely_rastered_tiles);
}

TEST_F(LegacySWPictureLayerImplTest, CompositedImageRasterScaleChanges) {
  gfx::Size layer_bounds(400, 400);
  scoped_refptr<FakeRasterSource> pending_raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);
  pending_raster_source->SetDirectlyCompositedImageDefaultRasterScale(
      gfx::Vector2dF(1, 1));

  SetupPendingTree(pending_raster_source);
  ASSERT_TRUE(pending_layer()->IsDirectlyCompositedImage());

  float expected_contents_scale = 0.125f;
  for (int i = 1; i < 30; ++i) {
    float ideal_contents_scale = 0.1f * i - 1e-6;
    switch (i) {
      // Scale 0.2.
      case 2:
        expected_contents_scale = 0.25f;
        break;
      // Scale 0.3.
      case 3:
        expected_contents_scale = 0.5f;
        break;
      // Scale 0.6.
      case 6:
        expected_contents_scale = 1.f;
        break;
    }
    SetupDrawPropertiesAndUpdateTiles(pending_layer(), ideal_contents_scale,
                                      1.f, 1.f);
    EXPECT_FLOAT_EQ(expected_contents_scale,
                    pending_layer()
                        ->picture_layer_tiling_set()
                        ->FindTilingWithResolution(HIGH_RESOLUTION)
                        ->contents_scale_key())
        << "ideal_contents_scale: " << ideal_contents_scale;
  }

  expected_contents_scale = 1.f;
  for (int i = 30; i >= 1; --i) {
    float ideal_contents_scale = 0.1f * i - 1e-6;
    switch (i) {
      // Scale 0.2.
      case 2:
        expected_contents_scale = 0.25f;
        break;
    }
    SetupDrawPropertiesAndUpdateTiles(pending_layer(), ideal_contents_scale,
                                      1.f, 1.f);
    EXPECT_FLOAT_EQ(expected_contents_scale,
                    pending_layer()
                        ->picture_layer_tiling_set()
                        ->FindTilingWithResolution(HIGH_RESOLUTION)
                        ->contents_scale_key())
        << "ideal_contents_scale: " << ideal_contents_scale;
  }
}

TEST_F(LegacySWPictureLayerImplTest, CompositedImageRasterOnChange) {
  gfx::Size layer_bounds(400, 400);
  scoped_refptr<FakeRasterSource> pending_raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);
  pending_raster_source->SetDirectlyCompositedImageDefaultRasterScale(
      gfx::Vector2dF(0.5f, 0.5f));

  SetupPendingTree(pending_raster_source);
  ASSERT_TRUE(pending_layer()->IsDirectlyCompositedImage());

  // The image is smaller than the layer bounds.
  SetupDrawPropertiesAndUpdateTiles(pending_layer(), 1.f, 1.f, 1.f);
  EXPECT_FLOAT_EQ(0.5f, pending_layer()
                            ->picture_layer_tiling_set()
                            ->FindTilingWithResolution(HIGH_RESOLUTION)
                            ->contents_scale_key());

  // The image is much larger than the layer bounds (5x). Verify that the
  // scaling down code is triggered (we should halve the raster scale until it
  // is less than 2x the ideal source scale).
  pending_layer()->SetBounds(layer_bounds);
  pending_raster_source->SetDirectlyCompositedImageDefaultRasterScale(
      gfx::Vector2dF(5.f, 5.f));
  pending_layer()->UpdateDirectlyCompositedImageFromRasterSource();
  SetupDrawPropertiesAndUpdateTiles(pending_layer(), 1.f, 1.f, 1.f);
  EXPECT_FLOAT_EQ(1.25f, pending_layer()
                             ->picture_layer_tiling_set()
                             ->FindTilingWithResolution(HIGH_RESOLUTION)
                             ->contents_scale_key());

  // Update the bounds and and bump up the ideal scale so that the scale down
  // restriction is lifted.
  pending_layer()->SetBounds(layer_bounds);
  SetupDrawPropertiesAndUpdateTiles(pending_layer(), 4.f, 1.f, 1.f);
  EXPECT_FLOAT_EQ(5.f, pending_layer()
                           ->picture_layer_tiling_set()
                           ->FindTilingWithResolution(HIGH_RESOLUTION)
                           ->contents_scale_key());

  // Lower the ideal scale to see that the clamping still applied as it is
  // lowered.
  SetupDrawPropertiesAndUpdateTiles(pending_layer(), 0.5f, 1.f, 1.f);
  EXPECT_FLOAT_EQ(0.625f, pending_layer()
                              ->picture_layer_tiling_set()
                              ->FindTilingWithResolution(HIGH_RESOLUTION)
                              ->contents_scale_key());

  SetupDrawPropertiesAndUpdateTiles(pending_layer(), 0.25f, 1.f, 1.f);
  EXPECT_FLOAT_EQ(0.625f, pending_layer()
                              ->picture_layer_tiling_set()
                              ->FindTilingWithResolution(HIGH_RESOLUTION)
                              ->contents_scale_key());
}

TEST_F(LegacySWPictureLayerImplTest, CompositedImageTinyRasterScale) {
  gfx::Size layer_bounds(512, 512);
  scoped_refptr<FakeRasterSource> pending_raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);
  pending_raster_source->SetDirectlyCompositedImageDefaultRasterScale(
      gfx::Vector2dF(1e-20f, 1e-20f));

  SetupPendingTree(pending_raster_source);
  ASSERT_TRUE(pending_layer()->IsDirectlyCompositedImage());

  SetupDrawPropertiesAndUpdateTiles(pending_layer(), 1.f, 1.f, 1.f);
  EXPECT_FLOAT_EQ(1.f / 512, pending_layer()
                                 ->picture_layer_tiling_set()
                                 ->FindTilingWithResolution(HIGH_RESOLUTION)
                                 ->contents_scale_key());
}

TEST_F(LegacySWPictureLayerImplTest, CompositedImageHugeRasterScale) {
  gfx::Size layer_bounds(400, 400);
  scoped_refptr<FakeRasterSource> pending_raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);
  pending_raster_source->SetDirectlyCompositedImageDefaultRasterScale(
      gfx::Vector2dF(1e20f, 1e20f));

  SetupPendingTree(pending_raster_source);
  ASSERT_TRUE(pending_layer()->IsDirectlyCompositedImage());

  SetupDrawPropertiesAndUpdateTiles(pending_layer(), 1.f, 1.f, 1.f);
  EXPECT_FLOAT_EQ(1.0f, pending_layer()
                            ->picture_layer_tiling_set()
                            ->FindTilingWithResolution(HIGH_RESOLUTION)
                            ->contents_scale_key());
}

TEST_F(LegacySWPictureLayerImplTest, CompositedImageRasterHighResScreen) {
  gfx::Size layer_bounds(100, 100);
  scoped_refptr<FakeRasterSource> pending_raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);
  pending_raster_source->SetDirectlyCompositedImageDefaultRasterScale(
      gfx::Vector2dF(5, 5));

  SetupPendingTree(pending_raster_source);
  ASSERT_TRUE(pending_layer()->IsDirectlyCompositedImage());

  // Verify that device_scale=2.0 is handled correctly. The image is expected to
  // be downscaled not more than 4x, so content scale is above 1.0.
  SetupDrawPropertiesAndUpdateTiles(pending_layer(), 1.f, 2.f, 1.f);
  EXPECT_FLOAT_EQ(1.25f, pending_layer()
                             ->picture_layer_tiling_set()
                             ->FindTilingWithResolution(HIGH_RESOLUTION)
                             ->contents_scale_key());
}

TEST_F(LegacySWPictureLayerImplTest, CompositedImageRasterOptOutTransitions) {
  gfx::Size layer_bounds(6, 6);
  scoped_refptr<FakeRasterSource> pending_raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);
  pending_raster_source->SetDirectlyCompositedImageDefaultRasterScale(
      gfx::Vector2dF(1, 1));

  SetupPendingTree(pending_raster_source);
  ASSERT_TRUE(pending_layer()->IsDirectlyCompositedImage());

  // Start with image and bounds matching to have this layer initially opted
  // in to directly composited images.
  EXPECT_EQ(layer_bounds, pending_layer()->bounds());
  EXPECT_EQ(gfx::Vector2dF(1, 1), pending_layer()->GetIdealContentsScale());
  EXPECT_FLOAT_EQ(1, pending_layer()
                         ->picture_layer_tiling_set()
                         ->FindTilingWithResolution(HIGH_RESOLUTION)
                         ->contents_scale_key());

  // Change the image and bounds to values that make the layer not eligible for
  // direct compositing. This must be reflected by a |contents_scale_key()| of
  // 0.2f (matching the ideal source scale).
  pending_raster_source->SetDirectlyCompositedImageDefaultRasterScale(
      gfx::Vector2dF(50, 50));
  pending_layer()->UpdateDirectlyCompositedImageFromRasterSource();
  SetupDrawPropertiesAndUpdateTiles(pending_layer(), 0.2f, 1.f, 1.f);
  EXPECT_FLOAT_EQ(0.2f, pending_layer()
                            ->picture_layer_tiling_set()
                            ->FindTilingWithResolution(HIGH_RESOLUTION)
                            ->contents_scale_key());

  // Ensure we get back to a directly composited image if the input values
  // change such that the optimization should apply.
  pending_layer()->SetBounds(ScaleToFlooredSize(layer_bounds, 2));
  pending_raster_source->SetDirectlyCompositedImageDefaultRasterScale(
      gfx::Vector2dF(2, 2));
  pending_layer()->UpdateDirectlyCompositedImageFromRasterSource();
  SetupDrawPropertiesAndUpdateTiles(pending_layer(), 0.2f, 1.f, 1.f);
  EXPECT_FLOAT_EQ(0.25f, pending_layer()
                             ->picture_layer_tiling_set()
                             ->FindTilingWithResolution(HIGH_RESOLUTION)
                             ->contents_scale_key());
}

TEST_F(LegacySWPictureLayerImplTest,
       ChangeRasterTranslationNukePendingLayerTiles) {
  gfx::Size layer_bounds(200, 200);
  gfx::Size tile_size(256, 256);
  auto raster_source = FakeRasterSource::CreateFilledWithText(layer_bounds);
  SetupTreesWithFixedTileSize(raster_source, raster_source, tile_size,
                              Region());

  // Start with scale & translation of * 2.25 + (0.25, 0.5).
  SetupDrawProperties(pending_layer(), 2.25f, 1.5f, 1.f);
  gfx::Transform translate1;
  translate1.Translate(0.25f, 0.5f);
  pending_layer()->draw_properties().screen_space_transform.PostConcat(
      translate1);
  pending_layer()->draw_properties().target_space_transform =
      pending_layer()->draw_properties().screen_space_transform;
  pending_layer()->UpdateTiles();
  ASSERT_EQ(1u, pending_layer()->tilings()->num_tilings());
  {
    PictureLayerTiling* tiling = pending_layer()->tilings()->tiling_at(0);
    EXPECT_EQ(gfx::AxisTransform2d(2.25f, gfx::Vector2dF(0.25f, 0.5f)),
              tiling->raster_transform());
    EXPECT_EQ(4u, tiling->AllTilesForTesting().size());
    for (auto* tile : tiling->AllTilesForTesting())
      EXPECT_EQ(tile->raster_transform(), tiling->raster_transform());
  }

  // Change to scale & translation of * 2.25 + (0.75, 0.25).
  // Verifies that layer movement updates raster translation.
  SetupDrawProperties(pending_layer(), 2.25f, 1.5f, 1.f);
  gfx::Transform translate2;
  translate2.Translate(0.75f, 0.25f);
  pending_layer()->draw_properties().screen_space_transform.PostConcat(
      translate2);
  pending_layer()->draw_properties().target_space_transform =
      pending_layer()->draw_properties().screen_space_transform;
  pending_layer()->UpdateTiles();
  ASSERT_EQ(1u, pending_layer()->tilings()->num_tilings());
  {
    PictureLayerTiling* tiling = pending_layer()->tilings()->tiling_at(0);
    EXPECT_EQ(gfx::AxisTransform2d(2.25f, gfx::Vector2dF(0.75f, 0.25f)),
              tiling->raster_transform());
    EXPECT_EQ(4u, tiling->AllTilesForTesting().size());
    for (auto* tile : tiling->AllTilesForTesting())
      EXPECT_EQ(tile->raster_transform(), tiling->raster_transform());
  }

  // Now change the device scale factor but keep the same total scale. Old tiles
  // with the same scale would become non-ideal and deleted on pending layers.
  SetupDrawProperties(pending_layer(), 2.25f, 1.0f, 1.f);
  pending_layer()->draw_properties().screen_space_transform.PostConcat(
      translate2);
  pending_layer()->draw_properties().target_space_transform =
      pending_layer()->draw_properties().screen_space_transform;
  pending_layer()->UpdateTiles();
  ASSERT_EQ(1u, pending_layer()->tilings()->num_tilings());
  {
    PictureLayerTiling* tiling = pending_layer()->tilings()->tiling_at(0);
    EXPECT_EQ(gfx::AxisTransform2d(2.25f, gfx::Vector2dF(0.75f, 0.25f)),
              tiling->raster_transform());
    EXPECT_EQ(4u, tiling->AllTilesForTesting().size());
    for (auto* tile : tiling->AllTilesForTesting())
      EXPECT_EQ(tile->raster_transform(), tiling->raster_transform());
  }
}

TEST_F(LegacySWPictureLayerImplTest,
       ChangeRasterTranslationNukeActiveLayerTiles) {
  gfx::Size layer_bounds(200, 200);
  gfx::Size tile_size(256, 256);
  auto raster_source = FakeRasterSource::CreateFilledWithText(layer_bounds);
  SetupTreesWithFixedTileSize(raster_source, raster_source, tile_size,
                              Region());

  // Start with scale & translation of * 2.25 + (0.25, 0.5) on the active layer.
  SetupDrawProperties(active_layer(), 2.25f, 1.5f, 1.f);
  gfx::Transform translate1;
  translate1.Translate(0.25f, 0.5f);
  active_layer()->draw_properties().screen_space_transform.PostConcat(
      translate1);
  active_layer()->draw_properties().target_space_transform =
      active_layer()->draw_properties().screen_space_transform;
  active_layer()->UpdateTiles();
  ASSERT_EQ(3u, active_layer()->tilings()->num_tilings());
  {
    PictureLayerTiling* tiling =
        active_layer()->tilings()->FindTilingWithScaleKey(2.25f);
    EXPECT_EQ(gfx::AxisTransform2d(2.25f, gfx::Vector2dF(0.25f, 0.5f)),
              tiling->raster_transform());
    EXPECT_EQ(4u, tiling->AllTilesForTesting().size());
    for (auto* tile : tiling->AllTilesForTesting())
      EXPECT_EQ(tile->raster_transform(), tiling->raster_transform());
  }

  // Create a pending layer with the same scale but different translation.
  SetupDrawProperties(pending_layer(), 2.25f, 1.5f, 1.f);
  gfx::Transform translate2;
  translate2.Translate(0.75f, 0.25f);
  pending_layer()->draw_properties().screen_space_transform.PostConcat(
      translate2);
  pending_layer()->draw_properties().target_space_transform =
      pending_layer()->draw_properties().screen_space_transform;
  pending_layer()->UpdateTiles();
  ASSERT_EQ(1u, pending_layer()->tilings()->num_tilings());
  {
    PictureLayerTiling* tiling = pending_layer()->tilings()->tiling_at(0);
    EXPECT_EQ(gfx::AxisTransform2d(2.25f, gfx::Vector2dF(0.75f, 0.25f)),
              tiling->raster_transform());
    EXPECT_EQ(4u, tiling->AllTilesForTesting().size());
    for (auto* tile : tiling->AllTilesForTesting())
      EXPECT_EQ(tile->raster_transform(), tiling->raster_transform());
  }

  // Now push to the active layer.
  // Verifies the active tiles get evicted due to slot conflict.
  host_impl()->ActivateSyncTree();
  ASSERT_EQ(3u, active_layer()->tilings()->num_tilings());
  {
    PictureLayerTiling* tiling =
        active_layer()->tilings()->FindTilingWithScaleKey(2.25f);
    EXPECT_EQ(gfx::AxisTransform2d(2.25f, gfx::Vector2dF(0.75f, 0.25f)),
              tiling->raster_transform());
    EXPECT_EQ(4u, tiling->AllTilesForTesting().size());
    for (auto* tile : tiling->AllTilesForTesting())
      EXPECT_EQ(tile->raster_transform(), tiling->raster_transform());
  }
}

TEST_F(LegacySWPictureLayerImplTest, AnimatedImages) {
  gfx::Size layer_bounds(1000, 1000);

  // Set up a raster source with 2 animated images.
  FakeRecordingSource recording_source(layer_bounds);
  std::vector<FrameMetadata> frames = {
      FrameMetadata(true, base::Milliseconds(1)),
      FrameMetadata(true, base::Milliseconds(1))};
  PaintImage image1 = CreateAnimatedImage(gfx::Size(200, 200), frames);
  PaintImage image2 = CreateAnimatedImage(gfx::Size(200, 200), frames);
  recording_source.add_draw_image(image1, gfx::Point(100, 100));
  recording_source.add_draw_image(image2, gfx::Point(500, 500));
  recording_source.Rerecord();
  scoped_refptr<RasterSource> raster_source =
      recording_source.CreateRasterSource();

  // All images should be registered on the pending layer.
  SetupPendingTree(raster_source, gfx::Size(), Region(gfx::Rect(layer_bounds)));
  auto* controller = host_impl()->image_animation_controller();
  EXPECT_EQ(controller->GetDriversForTesting(image1.stable_id())
                .count(pending_layer()),
            1u);
  EXPECT_EQ(controller->GetDriversForTesting(image2.stable_id())
                .count(pending_layer()),
            1u);

  // Make only the first image visible and verify that only this image is
  // animated.
  gfx::Rect visible_rect(0, 0, 300, 300);
  pending_layer()->set_visible_layer_rect(visible_rect);
  EXPECT_TRUE(pending_layer()->ShouldAnimate(image1.stable_id()));
  EXPECT_FALSE(pending_layer()->ShouldAnimate(image2.stable_id()));

  // Now activate and make sure the active layer is registered as well.
  ActivateTree();
  active_layer()->set_visible_layer_rect(visible_rect);
  EXPECT_EQ(controller->GetDriversForTesting(image1.stable_id())
                .count(active_layer()),
            1u);
  EXPECT_EQ(controller->GetDriversForTesting(image2.stable_id())
                .count(active_layer()),
            1u);

  // Once activated, only the active layer should drive animations for these
  // images. Since DrawProperties are not updated on the recycle tree, it has
  // stale state for visibility of images.
  ASSERT_EQ(old_pending_layer()->visible_layer_rect(), visible_rect);
  EXPECT_FALSE(old_pending_layer()->ShouldAnimate(image1.stable_id()));
  EXPECT_FALSE(old_pending_layer()->ShouldAnimate(image2.stable_id()));
  EXPECT_TRUE(active_layer()->ShouldAnimate(image1.stable_id()));
  EXPECT_FALSE(active_layer()->ShouldAnimate(image2.stable_id()));
}

TEST_F(LegacySWPictureLayerImplTest, PaintWorkletInputPaintRecordInvalidation) {
  gfx::Size layer_bounds(1000, 1000);

  PaintWorkletInput::PropertyKey key(
      PaintWorkletInput::NativePropertyType::kClipPath, ElementId());

  // Set up a raster source with a PaintWorkletInput.
  FakeRecordingSource recording_source(layer_bounds);
  scoped_refptr<TestPaintWorkletInput> input1 =
      base::MakeRefCounted<TestPaintWorkletInput>(key, gfx::SizeF(100, 100));
  PaintImage image1 = CreatePaintWorkletPaintImage(input1);
  recording_source.add_draw_image(image1, gfx::Point(100, 100));
  recording_source.Rerecord();
  scoped_refptr<RasterSource> raster_source =
      recording_source.CreateRasterSource();

  // Ensure the input is registered
  SetupPendingTree(raster_source, gfx::Size(), Region(gfx::Rect(layer_bounds)));
  EXPECT_EQ(pending_layer()->GetPaintWorkletRecordMap().size(), 1u);
  EXPECT_TRUE(pending_layer()->GetPaintWorkletRecordMap().contains(input1));

  // Add a paint record
  PaintRecord record1;
  pending_layer()->SetPaintWorkletRecord(input1, record1);

  // Should be immediately accessible
  auto it = pending_layer()->GetPaintWorkletRecordMap().find(input1);
  ASSERT_NE(it, pending_layer()->GetPaintWorkletRecordMap().end());
  EXPECT_TRUE(it->second.second.has_value());
  EXPECT_TRUE(it->second.second->EqualsForTesting(record1));

  PaintWorkletInput::PropertyValue val1;
  val1.float_value = 0.1f;
  pending_layer()->InvalidatePaintWorklets(key, val1, val1);

  // Paint record should not be invalidated with the same value
  it = pending_layer()->GetPaintWorkletRecordMap().find(input1);
  ASSERT_NE(it, pending_layer()->GetPaintWorkletRecordMap().end());
  EXPECT_TRUE(it->second.second.has_value());
  EXPECT_TRUE(it->second.second->EqualsForTesting(record1));

  PaintWorkletInput::PropertyValue val2;
  val2.float_value = 0.2f;
  pending_layer()->InvalidatePaintWorklets(key, val1, val2);

  // Paint record should have been invalidated
  it = pending_layer()->GetPaintWorkletRecordMap().find(input1);
  ASSERT_NE(it, pending_layer()->GetPaintWorkletRecordMap().end());
  EXPECT_FALSE(it->second.second.has_value());
}

TEST_F(LegacySWPictureLayerImplTest, PaintWorkletInputs) {
  gfx::Size layer_bounds(1000, 1000);

  // Set up a raster source with 2 PaintWorkletInputs.
  FakeRecordingSource recording_source(layer_bounds);
  scoped_refptr<TestPaintWorkletInput> input1 =
      base::MakeRefCounted<TestPaintWorkletInput>(gfx::SizeF(100, 100));
  PaintImage image1 = CreatePaintWorkletPaintImage(input1);
  scoped_refptr<TestPaintWorkletInput> input2 =
      base::MakeRefCounted<TestPaintWorkletInput>(gfx::SizeF(50, 50));
  PaintImage image2 = CreatePaintWorkletPaintImage(input2);
  recording_source.add_draw_image(image1, gfx::Point(100, 100));
  recording_source.add_draw_image(image2, gfx::Point(500, 500));
  recording_source.Rerecord();
  scoped_refptr<RasterSource> raster_source =
      recording_source.CreateRasterSource();

  // All inputs should be registered on the pending layer.
  SetupPendingTree(raster_source, gfx::Size(), Region(gfx::Rect(layer_bounds)));
  EXPECT_EQ(pending_layer()->GetPaintWorkletRecordMap().size(), 2u);
  EXPECT_TRUE(pending_layer()->GetPaintWorkletRecordMap().contains(input1));
  EXPECT_TRUE(pending_layer()->GetPaintWorkletRecordMap().contains(input2));

  // Specify a record for one of the inputs.
  PaintRecord record1;
  pending_layer()->SetPaintWorkletRecord(input1, record1);

  // Now activate and make sure the active layer is registered as well, with the
  // appropriate record.
  ActivateTree();
  EXPECT_EQ(active_layer()->GetPaintWorkletRecordMap().size(), 2u);
  auto it = active_layer()->GetPaintWorkletRecordMap().find(input1);
  ASSERT_NE(it, active_layer()->GetPaintWorkletRecordMap().end());
  EXPECT_TRUE(it->second.second->EqualsForTesting(record1));
  EXPECT_TRUE(active_layer()->GetPaintWorkletRecordMap().contains(input2));

  // Committing new PaintWorkletInputs (in a new raster source) should replace
  // the previous ones.
  FakeRecordingSource recording_source2(layer_bounds);
  scoped_refptr<TestPaintWorkletInput> input3 =
      base::MakeRefCounted<TestPaintWorkletInput>(gfx::SizeF(12, 12));
  PaintImage image3 = CreatePaintWorkletPaintImage(input3);
  recording_source2.add_draw_image(image3, gfx::Point(10, 10));
  recording_source2.Rerecord();
  raster_source = recording_source2.CreateRasterSource();

  SetupPendingTree(raster_source, gfx::Size(), Region(gfx::Rect(layer_bounds)));
  EXPECT_EQ(pending_layer()->GetPaintWorkletRecordMap().size(), 1u);
  EXPECT_TRUE(pending_layer()->GetPaintWorkletRecordMap().contains(input3));
}

TEST_F(LegacySWPictureLayerImplTest, PaintWorkletInputsIdenticalEntries) {
  gfx::Size layer_bounds(1000, 1000);

  FakeRecordingSource recording_source(layer_bounds);
  scoped_refptr<TestPaintWorkletInput> input =
      base::MakeRefCounted<TestPaintWorkletInput>(gfx::SizeF(100, 100));
  PaintImage image = CreatePaintWorkletPaintImage(input);
  recording_source.add_draw_image(image, gfx::Point(100, 100));
  recording_source.add_draw_image(image, gfx::Point(100, 100));
  recording_source.Rerecord();

  // All inputs should be registered on the pending layer.
  SetupPendingTree(recording_source.CreateRasterSource(), gfx::Size(),
                   Region(gfx::Rect(layer_bounds)));
  EXPECT_EQ(pending_layer()->GetPaintWorkletRecordMap().size(), 1u);
  EXPECT_TRUE(pending_layer()->GetPaintWorkletRecordMap().contains(input));

  PaintRecord record;
  pending_layer()->SetPaintWorkletRecord(input, record);
  pending_layer()->picture_layer_tiling_set()->RemoveAllTiles();
  recording_source.Rerecord();
  pending_layer()->SetRasterSource(recording_source.CreateRasterSource(),
                                   Region());
  EXPECT_EQ(pending_layer()->GetPaintWorkletRecordMap().size(), 1u);
  auto it = pending_layer()->GetPaintWorkletRecordMap().find(input);
  ASSERT_NE(it, pending_layer()->GetPaintWorkletRecordMap().end());
  // For now the paint record is cleared for multiple identical inputs.
  EXPECT_FALSE(it->second.second.has_value());
}

TEST_F(LegacySWPictureLayerImplTest, NoTilingsUsesScaleOne) {
  auto render_pass = viz::CompositorRenderPass::Create();

  gfx::Size layer_bounds(1000, 10000);
  scoped_refptr<FakeRasterSource> active_raster_source =
      FakeRasterSource::CreateEmpty(layer_bounds);
  SetupPendingTree(active_raster_source);
  ActivateTree();

  EXPECT_FALSE(active_raster_source->HasRecordings());
  EXPECT_FALSE(active_layer()->CanHaveTilings());
  active_layer()->SetContentsOpaque(true);
  active_layer()->SetSafeOpaqueBackgroundColor(SkColors::kWhite);
  active_layer()->draw_properties().visible_layer_rect =
      gfx::Rect(0, 0, 1000, 1000);
  active_layer()->UpdateTiles();

  ASSERT_EQ(0u, active_layer()->tilings()->num_tilings());
  ASSERT_FALSE(active_layer()->HighResTiling());

  AppendQuadsData data;
  active_layer()->WillDraw(DRAW_MODE_HARDWARE, nullptr);
  active_layer()->AppendQuads(render_pass.get(), &data);
  active_layer()->DidDraw(nullptr);

  // No checkerboard quads.
  EXPECT_EQ(0u, render_pass->quad_list.size());
  EXPECT_EQ(1u, render_pass->shared_quad_state_list.size());

  auto* shared_quad_state = render_pass->shared_quad_state_list.front();
  // We should use scale 1 here, so the layer rect should be full layer bounds
  // and the transform should be identity.
  EXPECT_EQ(gfx::Rect(1000, 10000), shared_quad_state->quad_layer_rect);
  EXPECT_TRUE(shared_quad_state->quad_to_target_transform.IsIdentity());
}

TEST_F(LegacySWPictureLayerImplTest,
       TransformedRasterizationAndContentsOpaqueAndLCDText) {
  auto raster_source =
      FakeRasterSource::CreateFilledWithText(gfx::Size(200, 200));
  SetupTreesWithInvalidation(raster_source, raster_source, Region());

  pending_layer()->SetBackgroundColor(SkColors::kWhite);
  pending_layer()->SetContentsOpaque(true);
  pending_layer()->SetOffsetToTransformParent(gfx::Vector2dF(0.2, 0.3));
  host_impl()->pending_tree()->set_needs_update_draw_properties();
  UpdateDrawProperties(host_impl()->pending_tree());
  EXPECT_TRUE(pending_layer()->contents_opaque());
  EXPECT_TRUE(pending_layer()->contents_opaque_for_text());
  EXPECT_EQ(LCDTextDisallowedReason::kNone,
            pending_layer()->ComputeLCDTextDisallowedReasonForTesting());
  ASSERT_TRUE(pending_layer()->HighResTiling());
  EXPECT_EQ(gfx::Vector2dF(0.2, 0.3),
            pending_layer()->HighResTiling()->raster_transform().translation());

  // Adding will-change:transform will keep the current raster translation.
  SetWillChangeTransform(pending_layer(), true);
  host_impl()->pending_tree()->set_needs_update_draw_properties();
  UpdateDrawProperties(host_impl()->pending_tree());
  EXPECT_TRUE(pending_layer()->contents_opaque());
  EXPECT_TRUE(pending_layer()->contents_opaque_for_text());
  EXPECT_EQ(LCDTextDisallowedReason::kWillChangeTransform,
            pending_layer()->ComputeLCDTextDisallowedReasonForTesting());
  ASSERT_TRUE(pending_layer()->HighResTiling());
  EXPECT_EQ(gfx::Vector2dF(0.2, 0.3),
            pending_layer()->HighResTiling()->raster_transform().translation());

  // We should not update raster translation when there is
  // will-change:transform.
  pending_layer()->SetOffsetToTransformParent(gfx::Vector2dF(0.4, 0.5));
  host_impl()->pending_tree()->set_needs_update_draw_properties();
  UpdateDrawProperties(host_impl()->pending_tree());
  EXPECT_TRUE(pending_layer()->contents_opaque());
  EXPECT_TRUE(pending_layer()->contents_opaque_for_text());
  EXPECT_EQ(LCDTextDisallowedReason::kWillChangeTransform,
            pending_layer()->ComputeLCDTextDisallowedReasonForTesting());
  ASSERT_TRUE(pending_layer()->HighResTiling());
  EXPECT_EQ(gfx::Vector2dF(0.2, 0.3),
            pending_layer()->HighResTiling()->raster_transform().translation());

  // Removing will-change:transform will update raster translation.
  SetWillChangeTransform(pending_layer(), false);
  host_impl()->pending_tree()->set_needs_update_draw_properties();
  UpdateDrawProperties(host_impl()->pending_tree());
  EXPECT_TRUE(pending_layer()->contents_opaque());
  EXPECT_TRUE(pending_layer()->contents_opaque_for_text());
  EXPECT_EQ(LCDTextDisallowedReason::kNone,
            pending_layer()->ComputeLCDTextDisallowedReasonForTesting());
  ASSERT_TRUE(pending_layer()->HighResTiling());
  EXPECT_EQ(gfx::Vector2dF(0.4, 0.5),
            pending_layer()->HighResTiling()->raster_transform().translation());
}

TEST_F(LegacySWPictureLayerImplTest,
       TransformedRasterizationAndLCDTextWithoutText) {
  auto raster_source = FakeRasterSource::CreateFilled(gfx::Size(200, 200));
  SetupTreesWithInvalidation(raster_source, raster_source, Region());

  pending_layer()->SetBackgroundColor(SkColors::kWhite);
  pending_layer()->SetContentsOpaque(true);
  pending_layer()->SetOffsetToTransformParent(gfx::Vector2dF(0.2, 0.3));
  host_impl()->pending_tree()->set_needs_update_draw_properties();
  UpdateDrawProperties(host_impl()->pending_tree());
  EXPECT_TRUE(pending_layer()->contents_opaque());
  EXPECT_TRUE(pending_layer()->contents_opaque_for_text());
  EXPECT_EQ(LCDTextDisallowedReason::kNoText,
            pending_layer()->ComputeLCDTextDisallowedReasonForTesting());
  ASSERT_TRUE(pending_layer()->HighResTiling());
  EXPECT_EQ(gfx::Vector2dF(),
            pending_layer()->HighResTiling()->raster_transform().translation());
}

TEST_F(LegacySWPictureLayerImplTest, InvalidateRasterInducingScrolls) {
  auto scroll_list1 = base::MakeRefCounted<DisplayItemList>();
  PaintImage image = CreateDiscardablePaintImage(gfx::Size(300, 200));
  scroll_list1->StartPaint();
  scroll_list1->push<DrawImageOp>(image, 0.f, 0.f);
  scroll_list1->EndPaintOfUnpaired(gfx::Rect(300, 200));
  scroll_list1->Finalize();

  auto scroll_list2 = base::MakeRefCounted<DisplayItemList>();
  scroll_list2->StartPaint();
  scroll_list2->push<DrawColorOp>(SkColors::kBlack, SkBlendMode::kSrcOver);
  scroll_list2->EndPaintOfUnpaired(gfx::Rect(1000, 1000));
  scroll_list2->Finalize();

  ElementId scroll_element_id1(123);
  ElementId scroll_element_id2(456);
  auto display_list = base::MakeRefCounted<DisplayItemList>();
  // Draw scrolling contents op under a clip.
  display_list->StartPaint();
  display_list->push<SaveOp>();
  display_list->push<ClipRectOp>(SkRect::MakeWH(200, 200), SkClipOp::kIntersect,
                                 false);
  display_list->EndPaintOfPairedBegin();
  display_list->PushDrawScrollingContentsOp(scroll_element_id1, scroll_list1,
                                            gfx::Rect(200, 200));
  display_list->StartPaint();
  display_list->push<RestoreOp>();
  display_list->EndPaintOfPairedEnd();
  // Draw another scrolling contents op under a translate and a clip.
  display_list->StartPaint();
  display_list->push<SaveOp>();
  display_list->push<TranslateOp>(100.f, 300.f);
  display_list->push<ClipRectOp>(SkRect::MakeWH(200, 200), SkClipOp::kIntersect,
                                 false);
  display_list->EndPaintOfPairedBegin();
  display_list->PushDrawScrollingContentsOp(scroll_element_id2, scroll_list2,
                                            gfx::Rect(100, 300, 200, 200));
  display_list->StartPaint();
  display_list->push<RestoreOp>();
  display_list->EndPaintOfPairedEnd();
  display_list->Finalize();

  EXPECT_EQ(2u, display_list->raster_inducing_scrolls().size());
  auto& info1 = display_list->raster_inducing_scrolls().at(scroll_element_id1);
  EXPECT_EQ(gfx::Rect(200, 200), info1.visual_rect);
  EXPECT_TRUE(info1.has_discardable_images);
  auto& info2 = display_list->raster_inducing_scrolls().at(scroll_element_id2);
  EXPECT_EQ(gfx::Rect(100, 300, 200, 200), info2.visual_rect);
  EXPECT_FALSE(info2.has_discardable_images);

  FakeContentLayerClient client;
  client.set_display_item_list(display_list);
  gfx::Size layer_bounds(500, 500);
  RecordingSource recording;
  Region invalidation;
  recording.Update(layer_bounds, 1, client, invalidation);
  auto raster = FakeRasterSource::CreateFromRecordingSource(recording);
  SetupTreesWithInvalidation(raster, raster, invalidation);
  pending_layer()->set_invalidation(Region());

  auto pending_image_map = pending_layer()->discardable_image_map();
  auto active_image_map = active_layer()->discardable_image_map();
  EXPECT_TRUE(pending_image_map);
  EXPECT_EQ(pending_image_map, active_image_map);

  // Invalidating scroll_element_id1 will invalidate both scroll visual rect
  // and the discardable image map.
  pending_layer()->InvalidateRasterInducingScrolls({scroll_element_id1});
  EXPECT_EQ(info1.visual_rect, pending_layer()->invalidation().bounds());
  EXPECT_TRUE(active_layer()->invalidation().IsEmpty());
  pending_image_map = pending_layer()->discardable_image_map();
  EXPECT_EQ(active_image_map, active_layer()->discardable_image_map());
  EXPECT_NE(pending_image_map, active_image_map);

  ActivateTree();
  SetupPendingTreeWithInvalidation(raster, Region());
  EXPECT_TRUE(pending_layer()->invalidation().IsEmpty());
  EXPECT_EQ(info1.visual_rect, active_layer()->invalidation().bounds());
  EXPECT_EQ(pending_image_map, pending_layer()->discardable_image_map());
  active_image_map = active_layer()->discardable_image_map();
  EXPECT_EQ(pending_image_map, active_image_map);

  // scroll_list2 doesn't contain discardable images. Invalidating
  // scroll_element_id2 will invalidate scroll visual rect only.
  pending_layer()->InvalidateRasterInducingScrolls({scroll_element_id2});
  EXPECT_EQ(info2.visual_rect, pending_layer()->invalidation().bounds());
  EXPECT_EQ(pending_image_map, pending_layer()->discardable_image_map());
  EXPECT_EQ(pending_image_map, active_layer()->discardable_image_map());

  ActivateTree();
  SetupPendingTreeWithInvalidation(raster, Region());
  EXPECT_TRUE(pending_layer()->invalidation().IsEmpty());
  EXPECT_EQ(info2.visual_rect, active_layer()->invalidation().bounds());
  EXPECT_EQ(pending_image_map, pending_layer()->discardable_image_map());
  EXPECT_EQ(pending_image_map, pending_layer()->discardable_image_map());
}

enum {
  kCanUseLCDText = 1 << 0,
  kLayersAlwaysAllowedLCDText = 1 << 1,
};

class LCDTextTest : public PictureLayerImplTest,
                    public testing::WithParamInterface<unsigned> {
 protected:
  LayerTreeSettings CreateSettings() override {
    auto settings = PictureLayerImplTest::CreateSettings();
    settings.can_use_lcd_text = GetParam() & kCanUseLCDText;
    settings.layers_always_allowed_lcd_text =
        GetParam() & kLayersAlwaysAllowedLCDText;
    return settings;
  }

  void SetUp() override {
    PictureLayerImplTest::SetUp();

    auto raster_source =
        FakeRasterSource::CreateFilledWithText(gfx::Size(200, 200));
    SetupPendingTree(raster_source);

    tree_ = host_impl()->pending_tree();
    layer_ = pending_layer();
    descendant_ = AddLayer<PictureLayerImpl>(tree_);

    layer_->SetContentsOpaque(true);
    layer_->SetDrawsContent(true);
    layer_->SetBounds(gfx::Size(200, 200));
    ASSERT_TRUE(layer_->CanHaveTilings());

    descendant_->SetContentsOpaque(true);
    descendant_->SetDrawsContent(true);
    descendant_->SetBounds(gfx::Size(200, 200));
    Region invalidation;
    descendant_->UpdateRasterSource(raster_source, &invalidation);
    ASSERT_TRUE(layer_->CanHaveTilings());

    CreateTransformNode(layer_);
    CreateEffectNode(layer_);
    CopyProperties(layer_, descendant_);
    CreateTransformNode(descendant_);
    CreateEffectNode(descendant_);
  }

  void TearDown() override {
    descendant_ = nullptr;
    layer_ = nullptr;
    tree_ = nullptr;
    PictureLayerImplTest::TearDown();
  }

  void CheckCanUseLCDText(LCDTextDisallowedReason expected_disallowed_reason,
                          const char* description,
                          PictureLayerImpl* layer = nullptr) {
    UpdateDrawProperties(tree_);

    if (GetParam() & kLayersAlwaysAllowedLCDText)
      expected_disallowed_reason = LCDTextDisallowedReason::kNone;
    else if (!(GetParam() & kCanUseLCDText))
      expected_disallowed_reason = LCDTextDisallowedReason::kSetting;

    SCOPED_TRACE(description);
    if (layer) {
      EXPECT_EQ(expected_disallowed_reason,
                layer->ComputeLCDTextDisallowedReasonForTesting());
    } else {
      EXPECT_EQ(expected_disallowed_reason,
                layer_->ComputeLCDTextDisallowedReasonForTesting());
      EXPECT_EQ(expected_disallowed_reason,
                descendant_->ComputeLCDTextDisallowedReasonForTesting());
    }
  }

  raw_ptr<LayerTreeImpl> tree_ = nullptr;
  raw_ptr<PictureLayerImpl> layer_ = nullptr;
  raw_ptr<PictureLayerImpl> descendant_ = nullptr;
};

INSTANTIATE_TEST_SUITE_P(All,
                         LCDTextTest,
                         testing::Values(0,
                                         kCanUseLCDText,
                                         kLayersAlwaysAllowedLCDText,
                                         kCanUseLCDText |
                                             kLayersAlwaysAllowedLCDText));

TEST_P(LCDTextTest, IdentityTransform) {
  CheckCanUseLCDText(LCDTextDisallowedReason::kNone, "identity transform");
}

TEST_P(LCDTextTest, IntegralTransform) {
  gfx::Transform integral_translation;
  integral_translation.Translate(1.0, 2.0);
  SetTransform(layer_.get(), integral_translation);

  CheckCanUseLCDText(LCDTextDisallowedReason::kNone, "integral transform");
}

TEST_P(LCDTextTest, NonIntegralTranslation) {
  // Non-integral translation.
  gfx::Transform non_integral_translation;
  non_integral_translation.Translate(1.5, 2.5);
  SetTransform(layer_.get(), non_integral_translation);
  // We can use LCD-text as raster translation can align the text to physical
  // pixels for fragtional transform in the render target.
  CheckCanUseLCDText(LCDTextDisallowedReason::kNone,
                     "non-integeral translation");

  SetTransform(layer_.get(), gfx::Transform());
  CheckCanUseLCDText(LCDTextDisallowedReason::kNone, "identity transform");
}

TEST_P(LCDTextTest, NonIntegralTranslationAboveRenderTarget) {
  // Non-integral translation above render target.
  gfx::Transform non_integral_translation;
  non_integral_translation.Translate(1.5, 2.5);
  SetTransform(layer_.get(), non_integral_translation);
  SetRenderSurfaceReason(layer_.get(), RenderSurfaceReason::kTest);
  // Raster translation can't handle fractional transform above the render
  // target, so LCD text is not allowed.
  CheckCanUseLCDText(LCDTextDisallowedReason::kNonIntegralTranslation,
                     "non-integeral translation above render target");
  SetTransform(layer_.get(), gfx::Transform());
  CheckCanUseLCDText(LCDTextDisallowedReason::kNone, "identity transform");
}

TEST_P(LCDTextTest, NonTranslation) {
  // Rotation.
  gfx::Transform rotation;
  rotation.Rotate(10.0);
  SetTransform(layer_.get(), rotation);
  CheckCanUseLCDText(LCDTextDisallowedReason::kNonIntegralTranslation,
                     "Rotation transform");

  // Scale. LCD text is allowed.
  gfx::Transform scale;
  scale.Scale(2.0, 2.0);
  SetTransform(layer_.get(), scale);
  CheckCanUseLCDText(LCDTextDisallowedReason::kNone, "Scale transform");

  // Skew.
  gfx::Transform skew;
  skew.Skew(10.0, 0.0);
  SetTransform(layer_.get(), skew);
  CheckCanUseLCDText(LCDTextDisallowedReason::kNonIntegralTranslation,
                     "Skew transform");

  SetTransform(layer_.get(), gfx::Transform());
  CheckCanUseLCDText(LCDTextDisallowedReason::kNone, "identity transform");
}

TEST_P(LCDTextTest, NonTranslationAboveRenderTarget) {
  SetRenderSurfaceReason(layer_.get(), RenderSurfaceReason::kTest);

  // Rotation.
  gfx::Transform rotation;
  rotation.Rotate(10.0);
  SetTransform(layer_.get(), rotation);
  CheckCanUseLCDText(LCDTextDisallowedReason::kNonIntegralTranslation,
                     "rotation transform above render target");

  // Scale. LCD-text is allowed.
  gfx::Transform scale;
  scale.Scale(2.0, 2.0);
  // Apply perspective and rotation to prevent the scale from applying
  // to the layers below the render target.
  scale.ApplyPerspectiveDepth(10);
  scale.RotateAboutXAxis(15.0);
  SetTransform(layer_.get(), scale);
  CheckCanUseLCDText(LCDTextDisallowedReason::kNonIntegralTranslation,
                     "scale transform above render target");

  // Skew.
  gfx::Transform skew;
  skew.Skew(10.0, 0.0);
  SetTransform(layer_.get(), skew);
  CheckCanUseLCDText(LCDTextDisallowedReason::kNonIntegralTranslation,
                     "skew transform above render target");

  SetTransform(layer_.get(), gfx::Transform());
  CheckCanUseLCDText(LCDTextDisallowedReason::kNone, "identity transform");
}

TEST_P(LCDTextTest, Opacity) {
  // LCD-text is allowed with opacity paint property.
  SetOpacity(layer_.get(), 0.5f);
  CheckCanUseLCDText(LCDTextDisallowedReason::kNone, "opacity: 0.5");
  SetOpacity(layer_.get(), 1.f);
  CheckCanUseLCDText(LCDTextDisallowedReason::kNone, "opacity: 1.0");
}

TEST_P(LCDTextTest, ContentsNotOpaque) {
  // Non-opaque content and opaque background.
  layer_->SetContentsOpaque(false);
  layer_->SetBackgroundColor(SkColors::kGreen);
  CheckCanUseLCDText(LCDTextDisallowedReason::kContentsNotOpaque,
                     "contents not opaque", layer_);
  CheckCanUseLCDText(LCDTextDisallowedReason::kNone,
                     "descedant of contents not opaque", descendant_);

  // Non-opaque content and non-opaque background.
  layer_->SetBackgroundColor({1.0f, 1.0f, 1.0f, 0.5f});
  CheckCanUseLCDText(LCDTextDisallowedReason::kBackgroundColorNotOpaque,
                     "background not opaque", layer_);
  CheckCanUseLCDText(LCDTextDisallowedReason::kNone,
                     "descendant of contents not opaque", descendant_);

  layer_->SetContentsOpaque(true);
  CheckCanUseLCDText(LCDTextDisallowedReason::kNone, "contents opaque");
}

TEST_P(LCDTextTest, WillChangeTransform) {
  SetWillChangeTransform(layer_.get(), true);
  CheckCanUseLCDText(LCDTextDisallowedReason::kWillChangeTransform,
                     "will-change:transform");

  SetWillChangeTransform(layer_.get(), false);
  CheckCanUseLCDText(LCDTextDisallowedReason::kNone,
                     "no will-change: transform");
}

TEST_P(LCDTextTest, Filter) {
  FilterOperations blur_filter;
  blur_filter.Append(FilterOperation::CreateBlurFilter(4.0f));
  SetFilter(layer_.get(), blur_filter);
  CheckCanUseLCDText(LCDTextDisallowedReason::kPixelOrColorEffect, "filter");

  SetFilter(layer_.get(), FilterOperations());
  CheckCanUseLCDText(LCDTextDisallowedReason::kNone, "no filter");
}

TEST_P(LCDTextTest, FilterAnimation) {
  FilterOperations blur_filter;
  blur_filter.Append(FilterOperation::CreateBlurFilter(4.0f));
  SetFilter(layer_.get(), blur_filter);
  CheckCanUseLCDText(LCDTextDisallowedReason::kPixelOrColorEffect, "filter");

  GetEffectNode(layer_.get())->has_potential_filter_animation = true;
  SetFilter(layer_.get(), FilterOperations());
  CheckCanUseLCDText(LCDTextDisallowedReason::kPixelOrColorEffect,
                     "filter animation");

  GetEffectNode(layer_.get())->has_potential_filter_animation = false;
  SetFilter(layer_.get(), FilterOperations());
  CheckCanUseLCDText(LCDTextDisallowedReason::kNone, "no filter");
}

TEST_P(LCDTextTest, BackdropFilter) {
  FilterOperations backdrop_filter;
  backdrop_filter.Append(FilterOperation::CreateBlurFilter(4.0f));
  SetBackdropFilter(descendant_.get(), backdrop_filter);
  UpdateDrawProperties(host_impl()->active_tree());
  CheckCanUseLCDText(LCDTextDisallowedReason::kPixelOrColorEffect,
                     "backdrop-filter", layer_);
  CheckCanUseLCDText(LCDTextDisallowedReason::kNone, "backdrop-filter",
                     descendant_);

  SetBackdropFilter(descendant_.get(), FilterOperations());
  UpdateDrawProperties(host_impl()->active_tree());
  CheckCanUseLCDText(LCDTextDisallowedReason::kNone, "no backdrop-filter",
                     layer_);
}

TEST_P(LCDTextTest, BackdropFilterAnimation) {
  FilterOperations backdrop_filter;
  backdrop_filter.Append(FilterOperation::CreateBlurFilter(4.0f));
  SetBackdropFilter(descendant_.get(), backdrop_filter);
  UpdateDrawProperties(host_impl()->active_tree());
  CheckCanUseLCDText(LCDTextDisallowedReason::kPixelOrColorEffect,
                     "backdrop-filter", layer_);
  CheckCanUseLCDText(LCDTextDisallowedReason::kNone, "backdrop-filter",
                     descendant_);

  GetEffectNode(descendant_.get())->has_potential_backdrop_filter_animation =
      true;
  SetBackdropFilter(descendant_.get(), FilterOperations());
  UpdateDrawProperties(host_impl()->active_tree());
  CheckCanUseLCDText(LCDTextDisallowedReason::kPixelOrColorEffect,
                     "backdrop-filter animation", layer_);

  GetEffectNode(descendant_.get())->has_potential_backdrop_filter_animation =
      false;
  UpdateDrawProperties(host_impl()->active_tree());
  CheckCanUseLCDText(LCDTextDisallowedReason::kNone, "no backdrop-filter",
                     layer_);
}

TEST_P(LCDTextTest, ContentsOpaqueForText) {
  layer_->SetContentsOpaque(false);
  layer_->SetBackgroundColor(SkColors::kGreen);
  layer_->SetContentsOpaqueForText(true);
  CheckCanUseLCDText(LCDTextDisallowedReason::kNone, "contents opaque for text",
                     layer_);

  layer_->SetContentsOpaqueForText(false);
  CheckCanUseLCDText(LCDTextDisallowedReason::kContentsNotOpaque,
                     "contents not opaque for text", layer_);
}

TEST_P(LCDTextTest, TransformAnimation) {
  GetTransformNode(layer_.get())->has_potential_animation = true;
  SetLocalTransformChanged(layer_.get());
  CheckCanUseLCDText(LCDTextDisallowedReason::kTransformAnimation,
                     "transform animation");

  GetTransformNode(layer_.get())->has_potential_animation = false;
  SetLocalTransformChanged(layer_.get());
  CheckCanUseLCDText(LCDTextDisallowedReason::kNone, "no transform animation");
}

}  // namespace
}  // namespace cc
