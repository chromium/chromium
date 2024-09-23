// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time_override.h"
#include "base/trace_event/process_memory_dump.h"
#include "cc/base/features.h"
#include "cc/layers/recording_source.h"
#include "cc/raster/raster_buffer.h"
#include "cc/raster/raster_source.h"
#include "cc/raster/synchronous_task_graph_runner.h"
#include "cc/resources/resource_pool.h"
#include "cc/test/fake_impl_task_runner_provider.h"
#include "cc/test/fake_layer_tree_frame_sink.h"
#include "cc/test/fake_layer_tree_frame_sink_client.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_paint_image_generator.h"
#include "cc/test/fake_picture_layer_impl.h"
#include "cc/test/fake_picture_layer_tiling_client.h"
#include "cc/test/fake_raster_query_queue.h"
#include "cc/test/fake_raster_source.h"
#include "cc/test/fake_recording_source.h"
#include "cc/test/fake_tile_manager.h"
#include "cc/test/fake_tile_task_manager.h"
#include "cc/test/layer_test_common.h"
#include "cc/test/skia_common.h"
#include "cc/test/test_layer_tree_host_base.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/test/test_tile_priorities.h"
#include "cc/tiles/eviction_tile_priority_queue.h"
#include "cc/tiles/raster_tile_priority_queue.h"
#include "cc/tiles/tile.h"
#include "cc/tiles/tile_priority.h"
#include "cc/tiles/tiling_set_raster_queue_all.h"
#include "cc/trees/layer_tree_impl.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "components/viz/test/begin_frame_args_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageGenerator.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSurface.h"

using testing::_;
using testing::Invoke;
using testing::Return;
using testing::StrictMock;

namespace cc {
namespace {

// A version of simple task runner that lets the user control if all tasks
// posted should run synchronously.
class SynchronousSimpleTaskRunner : public base::TestSimpleTaskRunner {
 public:
  bool PostDelayedTask(const base::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay) override {
    TestSimpleTaskRunner::PostDelayedTask(from_here, std::move(task), delay);
    if (run_tasks_synchronously_)
      RunUntilIdle();
    return true;
  }

  bool PostNonNestableDelayedTask(const base::Location& from_here,
                                  base::OnceClosure task,
                                  base::TimeDelta delay) override {
    return PostDelayedTask(from_here, std::move(task), delay);
  }

  void set_run_tasks_synchronously(bool run_tasks_synchronously) {
    run_tasks_synchronously_ = run_tasks_synchronously;
  }

 protected:
  ~SynchronousSimpleTaskRunner() override = default;

  bool run_tasks_synchronously_ = false;
};

class FakeRasterBuffer : public RasterBuffer {
 public:
  FakeRasterBuffer() = default;
  explicit FakeRasterBuffer(float expected_hdr_headroom)
      : expected_hdr_headroom_(expected_hdr_headroom) {}

  void Playback(const RasterSource* raster_source,
                const gfx::Rect& raster_full_rect,
                const gfx::Rect& raster_dirty_rect,
                uint64_t new_content_id,
                const gfx::AxisTransform2d& transform,
                const RasterSource::PlaybackSettings& playback_settings,
                const GURL& url) override {
    EXPECT_EQ(expected_hdr_headroom_, playback_settings.hdr_headroom);
  }

  bool SupportsBackgroundThreadPriority() const override { return true; }

 private:
  const float expected_hdr_headroom_ = 1.f;
};

class TileManagerTilePriorityQueueTest : public TestLayerTreeHostBase {
 public:
  LayerTreeSettings CreateSettings() override {
    auto settings = TestLayerTreeHostBase::CreateSettings();
    settings.create_low_res_tiling = true;
    return settings;
  }

  TileManager* tile_manager() { return host_impl()->tile_manager(); }

  class StubGpuBacking : public ResourcePool::GpuBacking {
   public:
    void OnMemoryDump(
        base::trace_event::ProcessMemoryDump* pmd,
        const base::trace_event::MemoryAllocatorDumpGuid& buffer_dump_guid,
        uint64_t tracing_process_id,
        int importance) const override {}
  };
};

TEST_F(TileManagerTilePriorityQueueTest, RasterTilePriorityQueue) {
  const gfx::Size layer_bounds(1000, 1000);
  host_impl()->active_tree()->SetDeviceViewportRect(gfx::Rect(layer_bounds));
  SetupDefaultTrees(layer_bounds);

  std::unique_ptr<RasterTilePriorityQueue> queue(host_impl()->BuildRasterQueue(
      SAME_PRIORITY_FOR_BOTH_TREES, RasterTilePriorityQueue::Type::ALL));
  EXPECT_FALSE(queue->IsEmpty());

  size_t tile_count = 0;
  std::set<Tile*> all_tiles;
  while (!queue->IsEmpty()) {
    EXPECT_TRUE(queue->Top().tile());
    all_tiles.insert(queue->Top().tile());
    ++tile_count;
    queue->Pop();
  }

  EXPECT_EQ(tile_count, all_tiles.size());
  EXPECT_EQ(16u, tile_count);

  // Sanity check, all tiles should be visible.
  std::set<Tile*> smoothness_tiles;
  queue = host_impl()->BuildRasterQueue(SMOOTHNESS_TAKES_PRIORITY,
                                        RasterTilePriorityQueue::Type::ALL);
  bool had_low_res = false;
  while (!queue->IsEmpty()) {
    PrioritizedTile prioritized_tile = queue->Top();
    EXPECT_TRUE(prioritized_tile.tile());
    EXPECT_EQ(TilePriority::NOW, prioritized_tile.priority().priority_bin);
    if (prioritized_tile.priority().resolution == LOW_RESOLUTION)
      had_low_res = true;
    else
      smoothness_tiles.insert(prioritized_tile.tile());
    queue->Pop();
  }
  EXPECT_EQ(all_tiles, smoothness_tiles);
  EXPECT_TRUE(had_low_res);

  // Check that everything is required for activation.
  queue = host_impl()->BuildRasterQueue(
      SMOOTHNESS_TAKES_PRIORITY,
      RasterTilePriorityQueue::Type::REQUIRED_FOR_ACTIVATION);
  std::set<Tile*> required_for_activation_tiles;
  while (!queue->IsEmpty()) {
    PrioritizedTile prioritized_tile = queue->Top();
    EXPECT_TRUE(prioritized_tile.tile()->required_for_activation());
    required_for_activation_tiles.insert(prioritized_tile.tile());
    queue->Pop();
  }
  EXPECT_EQ(all_tiles, required_for_activation_tiles);

  // Check that everything is required for draw.
  queue = host_impl()->BuildRasterQueue(
      SMOOTHNESS_TAKES_PRIORITY,
      RasterTilePriorityQueue::Type::REQUIRED_FOR_DRAW);
  std::set<Tile*> required_for_draw_tiles;
  while (!queue->IsEmpty()) {
    PrioritizedTile prioritized_tile = queue->Top();
    EXPECT_TRUE(prioritized_tile.tile()->required_for_draw());
    required_for_draw_tiles.insert(prioritized_tile.tile());
    queue->Pop();
  }
  EXPECT_EQ(all_tiles, required_for_draw_tiles);

  Region invalidation(gfx::Rect(0, 0, 500, 500));

  // Invalidate the pending tree.
  pending_layer()->set_invalidation(invalidation);
  pending_layer()->HighResTiling()->Invalidate(invalidation);

  // Renew all of the tile priorities.
  gfx::Rect viewport(50, 50, 100, 100);
  pending_layer()->picture_layer_tiling_set()->UpdateTilePriorities(
      viewport, 1.0f, 1.0, Occlusion(), true);
  active_layer()->picture_layer_tiling_set()->UpdateTilePriorities(
      viewport, 1.0f, 1.0, Occlusion(), true);

  // Populate all tiles directly from the tilings.
  all_tiles.clear();
  std::set<Tile*> high_res_tiles;
  std::vector<Tile*> pending_high_res_tiles =
      pending_layer()->HighResTiling()->AllTilesForTesting();
  for (size_t i = 0; i < pending_high_res_tiles.size(); ++i) {
    all_tiles.insert(pending_high_res_tiles[i]);
    high_res_tiles.insert(pending_high_res_tiles[i]);
  }

  std::vector<Tile*> active_high_res_tiles =
      active_layer()->HighResTiling()->AllTilesForTesting();
  for (size_t i = 0; i < active_high_res_tiles.size(); ++i) {
    all_tiles.insert(active_high_res_tiles[i]);
    high_res_tiles.insert(active_high_res_tiles[i]);
  }

  std::vector<Tile*> active_low_res_tiles =
      active_layer()->LowResTiling()->AllTilesForTesting();
  for (size_t i = 0; i < active_low_res_tiles.size(); ++i)
    all_tiles.insert(active_low_res_tiles[i]);

  PrioritizedTile last_tile;
  smoothness_tiles.clear();
  tile_count = 0;
  size_t correct_order_tiles = 0u;
  // Here we expect to get increasing ACTIVE_TREE priority_bin.
  queue = host_impl()->BuildRasterQueue(SMOOTHNESS_TAKES_PRIORITY,
                                        RasterTilePriorityQueue::Type::ALL);
  std::set<Tile*> expected_required_for_draw_tiles;
  std::set<Tile*> expected_required_for_activation_tiles;
  while (!queue->IsEmpty()) {
    PrioritizedTile prioritized_tile = queue->Top();
    EXPECT_TRUE(prioritized_tile.tile());

    if (!last_tile.tile())
      last_tile = prioritized_tile;

    EXPECT_LE(last_tile.priority().priority_bin,
              prioritized_tile.priority().priority_bin);
    bool skip_updating_last_tile = false;
    if (last_tile.priority().priority_bin ==
        prioritized_tile.priority().priority_bin) {
      correct_order_tiles += last_tile.priority().distance_to_visible <=
                             prioritized_tile.priority().distance_to_visible;
    } else if (prioritized_tile.priority().priority_bin == TilePriority::NOW) {
      // Since we'd return pending tree now tiles before the eventually tiles on
      // the active tree, update the value.
      ++correct_order_tiles;
      skip_updating_last_tile = true;
    }

    if (prioritized_tile.priority().priority_bin == TilePriority::NOW &&
        last_tile.priority().resolution !=
            prioritized_tile.priority().resolution) {
      // Low resolution should come first.
      EXPECT_EQ(LOW_RESOLUTION, last_tile.priority().resolution);
    }

    if (!skip_updating_last_tile)
      last_tile = prioritized_tile;
    ++tile_count;
    smoothness_tiles.insert(prioritized_tile.tile());
    if (prioritized_tile.tile()->required_for_draw())
      expected_required_for_draw_tiles.insert(prioritized_tile.tile());
    if (prioritized_tile.tile()->required_for_activation())
      expected_required_for_activation_tiles.insert(prioritized_tile.tile());
    queue->Pop();
  }

  EXPECT_EQ(tile_count, smoothness_tiles.size());
  EXPECT_EQ(all_tiles, smoothness_tiles);
  // Since we don't guarantee increasing distance due to spiral iterator, we
  // should check that we're _mostly_ right.
  EXPECT_GT(correct_order_tiles, 3 * tile_count / 4);

  // Check that we have consistent required_for_activation tiles.
  queue = host_impl()->BuildRasterQueue(
      SMOOTHNESS_TAKES_PRIORITY,
      RasterTilePriorityQueue::Type::REQUIRED_FOR_ACTIVATION);
  required_for_activation_tiles.clear();
  while (!queue->IsEmpty()) {
    PrioritizedTile prioritized_tile = queue->Top();
    EXPECT_TRUE(prioritized_tile.tile()->required_for_activation());
    required_for_activation_tiles.insert(prioritized_tile.tile());
    queue->Pop();
  }
  EXPECT_EQ(expected_required_for_activation_tiles,
            required_for_activation_tiles);
  EXPECT_NE(all_tiles, required_for_activation_tiles);

  // Check that we have consistent required_for_draw tiles.
  queue = host_impl()->BuildRasterQueue(
      SMOOTHNESS_TAKES_PRIORITY,
      RasterTilePriorityQueue::Type::REQUIRED_FOR_DRAW);
  required_for_draw_tiles.clear();
  while (!queue->IsEmpty()) {
    PrioritizedTile prioritized_tile = queue->Top();
    EXPECT_TRUE(prioritized_tile.tile()->required_for_draw());
    required_for_draw_tiles.insert(prioritized_tile.tile());
    queue->Pop();
  }
  EXPECT_EQ(expected_required_for_draw_tiles, required_for_draw_tiles);
  EXPECT_NE(all_tiles, required_for_draw_tiles);

  std::set<Tile*> new_content_tiles;
  last_tile = PrioritizedTile();
  size_t increasing_distance_tiles = 0u;
  // Here we expect to get increasing PENDING_TREE priority_bin.
  queue = host_impl()->BuildRasterQueue(NEW_CONTENT_TAKES_PRIORITY,
                                        RasterTilePriorityQueue::Type::ALL);
  tile_count = 0;
  while (!queue->IsEmpty()) {
    PrioritizedTile prioritized_tile = queue->Top();
    EXPECT_TRUE(prioritized_tile.tile());

    if (!last_tile.tile())
      last_tile = prioritized_tile;

    EXPECT_LE(last_tile.priority().priority_bin,
              prioritized_tile.priority().priority_bin);
    if (last_tile.priority().priority_bin ==
        prioritized_tile.priority().priority_bin) {
      increasing_distance_tiles +=
          last_tile.priority().distance_to_visible <=
          prioritized_tile.priority().distance_to_visible;
    }

    if (prioritized_tile.priority().priority_bin == TilePriority::NOW &&
        last_tile.priority().resolution !=
            prioritized_tile.priority().resolution) {
      // High resolution should come first.
      EXPECT_EQ(HIGH_RESOLUTION, last_tile.priority().resolution);
    }

    last_tile = prioritized_tile;
    new_content_tiles.insert(prioritized_tile.tile());
    ++tile_count;
    queue->Pop();
  }

  EXPECT_EQ(tile_count, new_content_tiles.size());
  EXPECT_EQ(high_res_tiles, new_content_tiles);
  // Since we don't guarantee increasing distance due to spiral iterator, we
  // should check that we're _mostly_ right.
  EXPECT_GE(increasing_distance_tiles, 3 * tile_count / 4);

  // Check that we have consistent required_for_activation tiles.
  queue = host_impl()->BuildRasterQueue(
      NEW_CONTENT_TAKES_PRIORITY,
      RasterTilePriorityQueue::Type::REQUIRED_FOR_ACTIVATION);
  required_for_activation_tiles.clear();
  while (!queue->IsEmpty()) {
    PrioritizedTile prioritized_tile = queue->Top();
    EXPECT_TRUE(prioritized_tile.tile()->required_for_activation());
    required_for_activation_tiles.insert(prioritized_tile.tile());
    queue->Pop();
  }
  EXPECT_EQ(expected_required_for_activation_tiles,
            required_for_activation_tiles);
  EXPECT_NE(new_content_tiles, required_for_activation_tiles);

  // Check that we have consistent required_for_draw tiles.
  queue = host_impl()->BuildRasterQueue(
      NEW_CONTENT_TAKES_PRIORITY,
      RasterTilePriorityQueue::Type::REQUIRED_FOR_DRAW);
  required_for_draw_tiles.clear();
  while (!queue->IsEmpty()) {
    PrioritizedTile prioritized_tile = queue->Top();
    EXPECT_TRUE(prioritized_tile.tile()->required_for_draw());
    required_for_draw_tiles.insert(prioritized_tile.tile());
    queue->Pop();
  }
  EXPECT_EQ(expected_required_for_draw_tiles, required_for_draw_tiles);
  EXPECT_NE(new_content_tiles, required_for_draw_tiles);
}

TEST_F(TileManagerTilePriorityQueueTest,
       RasterTilePriorityQueueAll_GetNextQueues) {
  host_impl()->AdvanceToNextFrame(base::Milliseconds(1));
  gfx::Size layer_bounds(1000, 1000);
  SetupDefaultTrees(layer_bounds);

  // Create a pending child layer.
  scoped_refptr<FakeRasterSource> pending_raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);
  auto* pending_child = AddLayer<FakePictureLayerImpl>(
      host_impl()->pending_tree(), pending_raster_source);
  pending_child->SetDrawsContent(true);
  CopyProperties(pending_layer(), pending_child);

  // Set a small viewport, so we have soon and eventually tiles.
  host_impl()->active_tree()->SetDeviceViewportRect(
      gfx::Rect(100, 100, 200, 200));
  host_impl()->AdvanceToNextFrame(base::Milliseconds(1));
  UpdateDrawProperties(host_impl()->active_tree());
  UpdateDrawProperties(host_impl()->pending_tree());
  host_impl()->SetRequiresHighResToDraw();

  // (1) SMOOTHNESS_TAKES_PRIORITY
  {
    std::unique_ptr<RasterTilePriorityQueue> queue(
        host_impl()->BuildRasterQueue(SMOOTHNESS_TAKES_PRIORITY,
                                      RasterTilePriorityQueue::Type::ALL));
    EXPECT_FALSE(queue->IsEmpty());

    TilePriority::PriorityBin last_bin = TilePriority::NOW;
    WhichTree las_tree = WhichTree::ACTIVE_TREE;
    float last_distance = 0.f;
    while (!queue->IsEmpty()) {
      PrioritizedTile prioritized_tile = queue->Top();
      WhichTree tree = prioritized_tile.source_tiling()->tree();
      TilePriority::PriorityBin priority_bin =
          prioritized_tile.priority().priority_bin;
      float distance_to_visible =
          prioritized_tile.priority().distance_to_visible;

      // Higher priority bin should come before lower priority bin regardless
      // of the tree.
      EXPECT_LE(last_bin, priority_bin);

      // If SMOOTHNESS_TAKES_PRIORITY, ACTIVE_TREE comes before PENDING_TREE for
      // NOW bin. The rest bins use the IsHigherPriorityThan condition.
      if (priority_bin == TilePriority::NOW) {
        EXPECT_LE(las_tree, tree);
      } else {
        // Reset the distance if it's in a different bin.
        if (last_bin != priority_bin)
          last_distance = 0;
        if (las_tree != tree)
          EXPECT_LE(last_distance, distance_to_visible);
      }

      last_bin = priority_bin;
      las_tree = tree;
      last_distance = distance_to_visible;
      queue->Pop();
    }
  }

  // (2) NEW_CONTENT_TAKES_PRIORITY
  {
    std::unique_ptr<RasterTilePriorityQueue> queue(
        host_impl()->BuildRasterQueue(NEW_CONTENT_TAKES_PRIORITY,
                                      RasterTilePriorityQueue::Type::ALL));
    EXPECT_FALSE(queue->IsEmpty());

    TilePriority::PriorityBin last_bin = TilePriority::NOW;
    WhichTree las_tree = WhichTree::PENDING_TREE;
    float last_distance = 0.f;
    while (!queue->IsEmpty()) {
      PrioritizedTile prioritized_tile = queue->Top();
      WhichTree tree = prioritized_tile.source_tiling()->tree();
      TilePriority::PriorityBin priority_bin =
          prioritized_tile.priority().priority_bin;
      float distance_to_visible =
          prioritized_tile.priority().distance_to_visible;

      // Higher priority bin should come before lower priority bin regardless
      // of the tree.
      EXPECT_LE(last_bin, priority_bin);

      // If SMOOTHNESS_TAKES_PRIORITY, PENDING_TREE comes before ACTIVE_TREE for
      // NOW bin. The rest bins use the IsHigherPriorityThan condition.
      if (priority_bin == TilePriority::NOW) {
        EXPECT_GE(las_tree, tree);
      } else {
        // Reset the distance if it's in a different bin.
        if (last_bin != priority_bin)
          last_distance = 0;
        if (las_tree != tree)
          EXPECT_LE(last_distance, distance_to_visible);
      }

      last_bin = priority_bin;
      las_tree = tree;
      last_distance = distance_to_visible;
      queue->Pop();
    }
  }

  // (3) SAME_PRIORITY_FOR_BOTH_TREES
  {
    std::unique_ptr<RasterTilePriorityQueue> queue(
        host_impl()->BuildRasterQueue(SAME_PRIORITY_FOR_BOTH_TREES,
                                      RasterTilePriorityQueue::Type::ALL));
    EXPECT_FALSE(queue->IsEmpty());

    TilePriority::PriorityBin last_bin = TilePriority::NOW;
    WhichTree las_tree = WhichTree::PENDING_TREE;
    float last_distance = 0.f;
    while (!queue->IsEmpty()) {
      PrioritizedTile prioritized_tile = queue->Top();
      WhichTree tree = prioritized_tile.source_tiling()->tree();
      TilePriority::PriorityBin priority_bin =
          prioritized_tile.priority().priority_bin;
      float distance_to_visible =
          prioritized_tile.priority().distance_to_visible;

      // Higher priority bin should come before lower priority bin regardless
      // of the tree.
      EXPECT_LE(last_bin, priority_bin);

      // Reset the distance if it's in a different bin.
      if (last_bin != priority_bin)
        last_distance = 0;

      // Use the IsHigherPriorityThan() condition.
      if (las_tree != tree)
        EXPECT_LE(last_distance, distance_to_visible);

      last_bin = priority_bin;
      las_tree = tree;
      last_distance = distance_to_visible;
      queue->Pop();
    }
  }
}

TEST_F(TileManagerTilePriorityQueueTest,
       RasterTilePriorityQueueHighNonIdealTilings) {
  const gfx::Size layer_bounds(1000, 1000);
  const gfx::Size viewport(800, 800);
  host_impl()->active_tree()->SetDeviceViewportRect(gfx::Rect(viewport));
  SetupDefaultTrees(layer_bounds);

  pending_layer()->tilings()->AddTiling(
      gfx::AxisTransform2d(1.5f, gfx::Vector2dF()),
      pending_layer()->raster_source());
  active_layer()->tilings()->AddTiling(
      gfx::AxisTransform2d(1.5f, gfx::Vector2dF()),
      active_layer()->raster_source());
  pending_layer()->tilings()->AddTiling(
      gfx::AxisTransform2d(1.7f, gfx::Vector2dF()),
      pending_layer()->raster_source());
  active_layer()->tilings()->AddTiling(
      gfx::AxisTransform2d(1.7f, gfx::Vector2dF()),
      active_layer()->raster_source());

  pending_layer()->tilings()->UpdateTilePriorities(gfx::Rect(viewport), 1.f,
                                                   5.0, Occlusion(), true);
  active_layer()->tilings()->UpdateTilePriorities(gfx::Rect(viewport), 1.f, 5.0,
                                                  Occlusion(), true);

  std::set<Tile*> all_expected_tiles;
  for (size_t i = 0; i < pending_layer()->num_tilings(); ++i) {
    PictureLayerTiling* tiling = pending_layer()->tilings()->tiling_at(i);
    if (tiling->contents_scale_key() == 1.f) {
      tiling->set_resolution(HIGH_RESOLUTION);
      const auto& all_tiles = tiling->AllTilesForTesting();
      all_expected_tiles.insert(all_tiles.begin(), all_tiles.end());
    } else {
      tiling->set_resolution(NON_IDEAL_RESOLUTION);
    }
  }

  for (size_t i = 0; i < active_layer()->num_tilings(); ++i) {
    PictureLayerTiling* tiling = active_layer()->tilings()->tiling_at(i);
    if (tiling->contents_scale_key() == 1.5f) {
      tiling->set_resolution(HIGH_RESOLUTION);
      const auto& all_tiles = tiling->AllTilesForTesting();
      all_expected_tiles.insert(all_tiles.begin(), all_tiles.end());
    } else {
      tiling->set_resolution(NON_IDEAL_RESOLUTION);
      // Non ideal tilings with a high res pending twin have to be processed
      // because of possible activation tiles.
      if (tiling->contents_scale_key() == 1.f) {
        tiling->UpdateAndGetAllPrioritizedTilesForTesting();
        const auto& all_tiles = tiling->AllTilesForTesting();
        for (auto* tile : all_tiles)
          EXPECT_TRUE(tile->required_for_activation());
        all_expected_tiles.insert(all_tiles.begin(), all_tiles.end());
      }
    }
  }

  std::unique_ptr<RasterTilePriorityQueue> queue(host_impl()->BuildRasterQueue(
      SMOOTHNESS_TAKES_PRIORITY, RasterTilePriorityQueue::Type::ALL));
  EXPECT_FALSE(queue->IsEmpty());

  size_t tile_count = 0;
  std::set<Tile*> all_actual_tiles;
  while (!queue->IsEmpty()) {
    EXPECT_TRUE(queue->Top().tile());
    all_actual_tiles.insert(queue->Top().tile());
    ++tile_count;
    queue->Pop();
  }

  EXPECT_EQ(tile_count, all_actual_tiles.size());
  EXPECT_EQ(all_expected_tiles.size(), all_actual_tiles.size());
  EXPECT_EQ(all_expected_tiles, all_actual_tiles);
}

TEST_F(TileManagerTilePriorityQueueTest,
       RasterTilePriorityQueueHighLowTilings) {
  const gfx::Size layer_bounds(1000, 1000);
  const gfx::Size viewport(800, 800);
  host_impl()->active_tree()->SetDeviceViewportRect(gfx::Rect(viewport));
  SetupDefaultTrees(layer_bounds);

  pending_layer()->tilings()->AddTiling(
      gfx::AxisTransform2d(1.5f, gfx::Vector2dF()),
      pending_layer()->raster_source());
  active_layer()->tilings()->AddTiling(
      gfx::AxisTransform2d(1.5f, gfx::Vector2dF()),
      active_layer()->raster_source());
  pending_layer()->tilings()->AddTiling(
      gfx::AxisTransform2d(1.7f, gfx::Vector2dF()),
      pending_layer()->raster_source());
  active_layer()->tilings()->AddTiling(
      gfx::AxisTransform2d(1.7f, gfx::Vector2dF()),
      active_layer()->raster_source());

  pending_layer()->tilings()->UpdateTilePriorities(gfx::Rect(viewport), 1.f,
                                                   5.0, Occlusion(), true);
  active_layer()->tilings()->UpdateTilePriorities(gfx::Rect(viewport), 1.f, 5.0,
                                                  Occlusion(), true);

  std::set<Tile*> all_expected_tiles;
  for (size_t i = 0; i < pending_layer()->num_tilings(); ++i) {
    PictureLayerTiling* tiling = pending_layer()->tilings()->tiling_at(i);
    if (tiling->contents_scale_key() == 1.f) {
      tiling->set_resolution(HIGH_RESOLUTION);
      const auto& all_tiles = tiling->AllTilesForTesting();
      all_expected_tiles.insert(all_tiles.begin(), all_tiles.end());
    } else {
      tiling->set_resolution(NON_IDEAL_RESOLUTION);
    }
  }

  for (size_t i = 0; i < active_layer()->num_tilings(); ++i) {
    PictureLayerTiling* tiling = active_layer()->tilings()->tiling_at(i);
    if (tiling->contents_scale_key() == 1.5f) {
      tiling->set_resolution(HIGH_RESOLUTION);
      const auto& all_tiles = tiling->AllTilesForTesting();
      all_expected_tiles.insert(all_tiles.begin(), all_tiles.end());
    } else {
      tiling->set_resolution(LOW_RESOLUTION);
      // Low res tilings with a high res pending twin have to be processed
      // because of possible activation tiles.
      if (tiling->contents_scale_key() == 1.f) {
        tiling->UpdateAndGetAllPrioritizedTilesForTesting();
        const auto& all_tiles = tiling->AllTilesForTesting();
        for (auto* tile : all_tiles)
          EXPECT_TRUE(tile->required_for_activation());
        all_expected_tiles.insert(all_tiles.begin(), all_tiles.end());
      }
    }
  }

  std::unique_ptr<RasterTilePriorityQueue> queue(host_impl()->BuildRasterQueue(
      SAME_PRIORITY_FOR_BOTH_TREES, RasterTilePriorityQueue::Type::ALL));
  EXPECT_FALSE(queue->IsEmpty());

  size_t tile_count = 0;
  std::set<Tile*> all_actual_tiles;
  while (!queue->IsEmpty()) {
    EXPECT_TRUE(queue->Top().tile());
    all_actual_tiles.insert(queue->Top().tile());
    ++tile_count;
    queue->Pop();
  }

  EXPECT_EQ(tile_count, all_actual_tiles.size());
  EXPECT_EQ(all_expected_tiles.size(), all_actual_tiles.size());
  EXPECT_EQ(all_expected_tiles, all_actual_tiles);
}

TEST_F(TileManagerTilePriorityQueueTest, RasterTilePriorityQueueInvalidation) {
  const gfx::Size layer_bounds(1000, 1000);
  host_impl()->active_tree()->SetDeviceViewportRect(gfx::Rect(500, 500));
  SetupDefaultTrees(layer_bounds);

  // Use a tile's content rect as an invalidation. We should inset it a bit to
  // ensure that border math doesn't invalidate neighbouring tiles.
  gfx::Rect invalidation =
      active_layer()->HighResTiling()->TileAt(1, 0)->content_rect();
  invalidation.Inset(2);

  pending_layer()->set_invalidation(invalidation);
  pending_layer()->HighResTiling()->Invalidate(invalidation);
  pending_layer()->HighResTiling()->CreateMissingTilesInLiveTilesRect();

  // Sanity checks: Tile at 0, 0 not exist on the pending tree (it's not
  // invalidated). Tile 1, 0 should exist on both.
  EXPECT_FALSE(pending_layer()->HighResTiling()->TileAt(0, 0));
  EXPECT_TRUE(active_layer()->HighResTiling()->TileAt(0, 0));
  EXPECT_TRUE(pending_layer()->HighResTiling()->TileAt(1, 0));
  EXPECT_TRUE(active_layer()->HighResTiling()->TileAt(1, 0));
  EXPECT_NE(pending_layer()->HighResTiling()->TileAt(1, 0),
            active_layer()->HighResTiling()->TileAt(1, 0));

  std::set<Tile*> expected_now_tiles;
  std::set<Tile*> expected_required_for_draw_tiles;
  std::set<Tile*> expected_required_for_activation_tiles;
  for (int i = 0; i <= 1; ++i) {
    for (int j = 0; j <= 1; ++j) {
      bool have_pending_tile = false;
      if (pending_layer()->HighResTiling()->TileAt(i, j)) {
        expected_now_tiles.insert(
            pending_layer()->HighResTiling()->TileAt(i, j));
        expected_required_for_activation_tiles.insert(
            pending_layer()->HighResTiling()->TileAt(i, j));
        have_pending_tile = true;
      }
      Tile* active_tile = active_layer()->HighResTiling()->TileAt(i, j);
      EXPECT_TRUE(active_tile);
      expected_now_tiles.insert(active_tile);
      expected_required_for_draw_tiles.insert(active_tile);
      if (!have_pending_tile)
        expected_required_for_activation_tiles.insert(active_tile);
    }
  }
  // Expect 3 shared tiles and 1 unshared tile in total.
  EXPECT_EQ(5u, expected_now_tiles.size());
  // Expect 4 tiles for each draw and activation, but not all the same.
  EXPECT_EQ(4u, expected_required_for_activation_tiles.size());
  EXPECT_EQ(4u, expected_required_for_draw_tiles.size());
  EXPECT_NE(expected_required_for_draw_tiles,
            expected_required_for_activation_tiles);

  std::set<Tile*> expected_all_tiles;
  for (int i = 0; i <= 3; ++i) {
    for (int j = 0; j <= 3; ++j) {
      if (pending_layer()->HighResTiling()->TileAt(i, j))
        expected_all_tiles.insert(
            pending_layer()->HighResTiling()->TileAt(i, j));
      EXPECT_TRUE(active_layer()->HighResTiling()->TileAt(i, j));
      expected_all_tiles.insert(active_layer()->HighResTiling()->TileAt(i, j));
    }
  }
  // Expect 15 shared tiles and 1 unshared tile.
  EXPECT_EQ(17u, expected_all_tiles.size());

  // The actual test will now build different queues and verify that the queues
  // return the same information as computed manually above.
  std::unique_ptr<RasterTilePriorityQueue> queue(host_impl()->BuildRasterQueue(
      SAME_PRIORITY_FOR_BOTH_TREES, RasterTilePriorityQueue::Type::ALL));
  std::set<Tile*> actual_now_tiles;
  std::set<Tile*> actual_all_tiles;
  while (!queue->IsEmpty()) {
    PrioritizedTile prioritized_tile = queue->Top();
    queue->Pop();
    if (prioritized_tile.priority().priority_bin == TilePriority::NOW)
      actual_now_tiles.insert(prioritized_tile.tile());
    actual_all_tiles.insert(prioritized_tile.tile());
  }
  EXPECT_EQ(expected_now_tiles, actual_now_tiles);
  EXPECT_EQ(expected_all_tiles, actual_all_tiles);

  queue = host_impl()->BuildRasterQueue(
      SAME_PRIORITY_FOR_BOTH_TREES,
      RasterTilePriorityQueue::Type::REQUIRED_FOR_DRAW);
  std::set<Tile*> actual_required_for_draw_tiles;
  while (!queue->IsEmpty()) {
    PrioritizedTile prioritized_tile = queue->Top();
    queue->Pop();
    actual_required_for_draw_tiles.insert(prioritized_tile.tile());
  }
  EXPECT_EQ(expected_required_for_draw_tiles, actual_required_for_draw_tiles);

  queue = host_impl()->BuildRasterQueue(
      SAME_PRIORITY_FOR_BOTH_TREES,
      RasterTilePriorityQueue::Type::REQUIRED_FOR_ACTIVATION);
  std::set<Tile*> actual_required_for_activation_tiles;
  while (!queue->IsEmpty()) {
    Tile* tile = queue->Top().tile();
    queue->Pop();
    actual_required_for_activation_tiles.insert(tile);
  }
  EXPECT_EQ(expected_required_for_activation_tiles,
            actual_required_for_activation_tiles);
}

TEST_F(TileManagerTilePriorityQueueTest, ActivationComesBeforeSoon) {
  host_impl()->AdvanceToNextFrame(base::Milliseconds(1));

  gfx::Size layer_bounds(1000, 1000);
  SetupDefaultTrees(layer_bounds);

  // Create a pending child layer.
  scoped_refptr<FakeRasterSource> pending_raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);
  auto* pending_child = AddLayer<FakePictureLayerImpl>(
      host_impl()->pending_tree(), pending_raster_source);
  pending_child->SetDrawsContent(true);
  CopyProperties(pending_layer(), pending_child);

  // Set a small viewport, so we have soon and eventually tiles.
  host_impl()->active_tree()->SetDeviceViewportRect(gfx::Rect(200, 200));
  host_impl()->AdvanceToNextFrame(base::Milliseconds(1));
  UpdateDrawProperties(host_impl()->pending_tree());

  host_impl()->SetRequiresHighResToDraw();
  std::unique_ptr<RasterTilePriorityQueue> queue(host_impl()->BuildRasterQueue(
      SMOOTHNESS_TAKES_PRIORITY, RasterTilePriorityQueue::Type::ALL));
  EXPECT_FALSE(queue->IsEmpty());

  // Get all the tiles that are NOW and make sure they are ready to draw.
  std::vector<Tile*> all_tiles;
  while (!queue->IsEmpty()) {
    PrioritizedTile prioritized_tile = queue->Top();
    if (prioritized_tile.priority().priority_bin >= TilePriority::SOON)
      break;

    all_tiles.push_back(prioritized_tile.tile());
    queue->Pop();
  }

  tile_manager()->InitializeTilesWithResourcesForTesting(
      std::vector<Tile*>(all_tiles.begin(), all_tiles.end()));

  // Ensure we can activate.
  EXPECT_TRUE(tile_manager()->IsReadyToActivate());
}

TEST_F(TileManagerTilePriorityQueueTest, EvictionTilePriorityQueue) {
  const gfx::Size layer_bounds(1000, 1000);
  host_impl()->active_tree()->SetDeviceViewportRect(gfx::Rect(layer_bounds));
  SetupDefaultTrees(layer_bounds);
  ASSERT_TRUE(active_layer()->HighResTiling());
  ASSERT_TRUE(active_layer()->LowResTiling());
  ASSERT_TRUE(pending_layer()->HighResTiling());
  EXPECT_FALSE(pending_layer()->LowResTiling());

  std::unique_ptr<EvictionTilePriorityQueue> empty_queue(
      host_impl()->BuildEvictionQueue(SAME_PRIORITY_FOR_BOTH_TREES));
  EXPECT_TRUE(empty_queue->IsEmpty());
  std::set<Tile*> all_tiles;
  size_t tile_count = 0;

  std::unique_ptr<RasterTilePriorityQueue> raster_queue(
      host_impl()->BuildRasterQueue(SAME_PRIORITY_FOR_BOTH_TREES,
                                    RasterTilePriorityQueue::Type::ALL));
  while (!raster_queue->IsEmpty()) {
    ++tile_count;
    EXPECT_TRUE(raster_queue->Top().tile());
    all_tiles.insert(raster_queue->Top().tile());
    raster_queue->Pop();
  }

  EXPECT_EQ(tile_count, all_tiles.size());
  EXPECT_EQ(16u, tile_count);

  tile_manager()->InitializeTilesWithResourcesForTesting(
      std::vector<Tile*>(all_tiles.begin(), all_tiles.end()));

  std::unique_ptr<EvictionTilePriorityQueue> queue(
      host_impl()->BuildEvictionQueue(SMOOTHNESS_TAKES_PRIORITY));
  EXPECT_FALSE(queue->IsEmpty());

  // Sanity check, all tiles should be visible.
  std::set<Tile*> smoothness_tiles;
  while (!queue->IsEmpty()) {
    PrioritizedTile prioritized_tile = queue->Top();
    EXPECT_TRUE(prioritized_tile.tile());
    EXPECT_EQ(TilePriority::NOW, prioritized_tile.priority().priority_bin);
    EXPECT_TRUE(prioritized_tile.tile()->draw_info().has_resource());
    smoothness_tiles.insert(prioritized_tile.tile());
    queue->Pop();
  }
  EXPECT_EQ(all_tiles, smoothness_tiles);

  tile_manager()->ReleaseTileResourcesForTesting(
      std::vector<Tile*>(all_tiles.begin(), all_tiles.end()));

  Region invalidation(gfx::Rect(0, 0, 500, 500));

  // Invalidate the pending tree.
  pending_layer()->set_invalidation(invalidation);
  pending_layer()->HighResTiling()->Invalidate(invalidation);
  pending_layer()->HighResTiling()->CreateMissingTilesInLiveTilesRect();
  EXPECT_FALSE(pending_layer()->LowResTiling());

  // Renew all of the tile priorities.
  gfx::Rect viewport(50, 50, 100, 100);
  pending_layer()->picture_layer_tiling_set()->UpdateTilePriorities(
      viewport, 1.0f, 1.0, Occlusion(), true);
  active_layer()->picture_layer_tiling_set()->UpdateTilePriorities(
      viewport, 1.0f, 1.0, Occlusion(), true);

  // Populate all tiles directly from the tilings.
  all_tiles.clear();
  std::vector<Tile*> pending_high_res_tiles =
      pending_layer()->HighResTiling()->AllTilesForTesting();
  for (size_t i = 0; i < pending_high_res_tiles.size(); ++i)
    all_tiles.insert(pending_high_res_tiles[i]);

  std::vector<Tile*> active_high_res_tiles =
      active_layer()->HighResTiling()->AllTilesForTesting();
  for (size_t i = 0; i < active_high_res_tiles.size(); ++i)
    all_tiles.insert(active_high_res_tiles[i]);

  std::vector<Tile*> active_low_res_tiles =
      active_layer()->LowResTiling()->AllTilesForTesting();
  for (size_t i = 0; i < active_low_res_tiles.size(); ++i)
    all_tiles.insert(active_low_res_tiles[i]);

  tile_manager()->InitializeTilesWithResourcesForTesting(
      std::vector<Tile*>(all_tiles.begin(), all_tiles.end()));

  PrioritizedTile last_tile;
  smoothness_tiles.clear();
  tile_count = 0;
  // Here we expect to get increasing combined priority_bin.
  queue = host_impl()->BuildEvictionQueue(SMOOTHNESS_TAKES_PRIORITY);
  int distance_increasing = 0;
  int distance_decreasing = 0;
  while (!queue->IsEmpty()) {
    PrioritizedTile prioritized_tile = queue->Top();
    Tile* tile = prioritized_tile.tile();
    EXPECT_TRUE(tile);
    EXPECT_TRUE(tile->draw_info().has_resource());

    if (!last_tile.tile())
      last_tile = prioritized_tile;

    const TilePriority& last_priority = last_tile.priority();
    const TilePriority& priority = prioritized_tile.priority();

    EXPECT_GE(last_priority.priority_bin, priority.priority_bin);
    if (last_priority.priority_bin == priority.priority_bin) {
      EXPECT_LE(last_tile.tile()->required_for_activation(),
                tile->required_for_activation());
      if (last_tile.tile()->required_for_activation() ==
          tile->required_for_activation()) {
        if (last_priority.distance_to_visible >= priority.distance_to_visible)
          ++distance_decreasing;
        else
          ++distance_increasing;
      }
    }

    last_tile = prioritized_tile;
    ++tile_count;
    smoothness_tiles.insert(tile);
    queue->Pop();
  }

  // Ensure that the distance is decreasing many more times than increasing.
  EXPECT_EQ(3, distance_increasing);
  EXPECT_EQ(16, distance_decreasing);
  EXPECT_EQ(tile_count, smoothness_tiles.size());
  EXPECT_EQ(all_tiles, smoothness_tiles);

  std::set<Tile*> new_content_tiles;
  last_tile = PrioritizedTile();
  // Again, we expect to get increasing combined priority_bin.
  queue = host_impl()->BuildEvictionQueue(NEW_CONTENT_TAKES_PRIORITY);
  distance_decreasing = 0;
  distance_increasing = 0;
  while (!queue->IsEmpty()) {
    PrioritizedTile prioritized_tile = queue->Top();
    Tile* tile = prioritized_tile.tile();
    EXPECT_TRUE(tile);

    if (!last_tile.tile())
      last_tile = prioritized_tile;

    const TilePriority& last_priority = last_tile.priority();
    const TilePriority& priority = prioritized_tile.priority();

    EXPECT_GE(last_priority.priority_bin, priority.priority_bin);
    if (last_priority.priority_bin == priority.priority_bin) {
      EXPECT_LE(last_tile.tile()->required_for_activation(),
                tile->required_for_activation());
      if (last_tile.tile()->required_for_activation() ==
          tile->required_for_activation()) {
        if (last_priority.distance_to_visible >= priority.distance_to_visible)
          ++distance_decreasing;
        else
          ++distance_increasing;
      }
    }

    last_tile = prioritized_tile;
    new_content_tiles.insert(tile);
    queue->Pop();
  }

  // Ensure that the distance is decreasing many more times than increasing.
  EXPECT_EQ(3, distance_increasing);
  EXPECT_EQ(16, distance_decreasing);
  EXPECT_EQ(tile_count, new_content_tiles.size());
  EXPECT_EQ(all_tiles, new_content_tiles);
}

// Verifies LayerDebugInfo::name ends up memory dumps.
TEST_F(TileManagerTilePriorityQueueTest, DebugNameAppearsInMemoryDump) {
  host_impl()->AdvanceToNextFrame(base::Milliseconds(1));

  gfx::Size layer_bounds(1000, 1000);

  host_impl()->active_tree()->SetDeviceViewportRect(gfx::Rect(layer_bounds));

  scoped_refptr<FakeRasterSource> pending_raster_source_1 =
      FakeRasterSource::CreateFilledWithText(layer_bounds);
  scoped_refptr<FakeRasterSource> pending_raster_source_2 =
      FakeRasterSource::CreateFilledWithText(layer_bounds);

  SetupPendingTree(pending_raster_source_1);

  auto* pending_child_layer =
      AddLayer<FakePictureLayerImpl>(host_impl()->pending_tree());
  LayerDebugInfo debug_info;
  debug_info.name = "debug-name";

  // The debug_info.name is copied to the raster source in SetRasterSource
  // so it must be set before.
  pending_child_layer->UpdateDebugInfo(&debug_info);
  pending_child_layer->SetBounds(pending_raster_source_2->size());
  pending_child_layer->SetRasterSource(pending_raster_source_2, Region());
  pending_child_layer->SetDrawsContent(true);
  CopyProperties(pending_layer(), pending_child_layer);

  ActivateTree();

  UpdateDrawProperties(host_impl()->active_tree());
  host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());

  base::trace_event::MemoryDumpArgs dump_args = {
      base::trace_event::MemoryDumpLevelOfDetail::kDetailed};
  base::trace_event::ProcessMemoryDump memory_dump(dump_args);
  host_impl()->resource_pool()->OnMemoryDump(dump_args, &memory_dump);
  bool found_debug_name = false;
  for (const auto& allocator_map_pair : memory_dump.allocator_dumps()) {
    if (base::Contains(allocator_map_pair.first, "debug-name")) {
      found_debug_name = true;
      break;
    }
  }
  EXPECT_TRUE(found_debug_name);
}

TEST_F(TileManagerTilePriorityQueueTest,
       EvictionTilePriorityQueueWithOcclusion) {
  host_impl()->AdvanceToNextFrame(base::Milliseconds(1));

  gfx::Size layer_bounds(1000, 1000);

  host_impl()->active_tree()->SetDeviceViewportRect(gfx::Rect(layer_bounds));

  scoped_refptr<FakeRasterSource> pending_raster_source =
      FakeRasterSource::CreateFilledWithText(layer_bounds);
  SetupPendingTree(pending_raster_source);

  auto* pending_child_layer = AddLayer<FakePictureLayerImpl>(
      host_impl()->pending_tree(), pending_raster_source);
  int child_id = pending_child_layer->id();
  pending_child_layer->SetDrawsContent(true);
  CopyProperties(pending_layer(), pending_child_layer);

  host_impl()->AdvanceToNextFrame(base::Milliseconds(1));
  UpdateDrawProperties(host_impl()->pending_tree());

  ActivateTree();
  SetupPendingTree(pending_raster_source);

  FakePictureLayerImpl* active_child_layer = static_cast<FakePictureLayerImpl*>(
      host_impl()->active_tree()->LayerById(child_id));

  std::set<Tile*> all_tiles;
  size_t tile_count = 0;
  std::unique_ptr<RasterTilePriorityQueue> raster_queue(
      host_impl()->BuildRasterQueue(SAME_PRIORITY_FOR_BOTH_TREES,
                                    RasterTilePriorityQueue::Type::ALL));
  while (!raster_queue->IsEmpty()) {
    ++tile_count;
    EXPECT_TRUE(raster_queue->Top().tile());
    all_tiles.insert(raster_queue->Top().tile());
    raster_queue->Pop();
  }
  EXPECT_EQ(tile_count, all_tiles.size());
  EXPECT_EQ(32u, tile_count);

  // Renew all of the tile priorities.
  gfx::Rect viewport(layer_bounds);
  pending_layer()->picture_layer_tiling_set()->UpdateTilePriorities(
      viewport, 1.0f, 1.0, Occlusion(), true);
  pending_child_layer->picture_layer_tiling_set()->UpdateTilePriorities(
      viewport, 1.0f, 1.0, Occlusion(), true);

  active_layer()->picture_layer_tiling_set()->UpdateTilePriorities(
      viewport, 1.0f, 1.0, Occlusion(), true);
  active_child_layer->picture_layer_tiling_set()->UpdateTilePriorities(
      viewport, 1.0f, 1.0, Occlusion(), true);

  // Populate all tiles directly from the tilings.
  all_tiles.clear();
  std::vector<Tile*> pending_high_res_tiles =
      pending_layer()->HighResTiling()->AllTilesForTesting();
  all_tiles.insert(pending_high_res_tiles.begin(),
                   pending_high_res_tiles.end());

  // Set all tiles on the pending_child_layer as occluded on the pending tree.
  std::vector<Tile*> pending_child_high_res_tiles =
      pending_child_layer->HighResTiling()->AllTilesForTesting();
  pending_child_layer->HighResTiling()->SetAllTilesOccludedForTesting();
  active_child_layer->HighResTiling()->SetAllTilesOccludedForTesting();
  active_child_layer->LowResTiling()->SetAllTilesOccludedForTesting();

  tile_manager()->InitializeTilesWithResourcesForTesting(
      std::vector<Tile*>(all_tiles.begin(), all_tiles.end()));

  // Verify occlusion is considered by EvictionTilePriorityQueue.
  TreePriority tree_priority = NEW_CONTENT_TAKES_PRIORITY;
  size_t occluded_count = 0u;
  PrioritizedTile last_tile;
  std::unique_ptr<EvictionTilePriorityQueue> queue(
      host_impl()->BuildEvictionQueue(tree_priority));
  while (!queue->IsEmpty()) {
    PrioritizedTile prioritized_tile = queue->Top();
    if (!last_tile.tile())
      last_tile = prioritized_tile;

    bool tile_is_occluded = prioritized_tile.is_occluded();

    // The only way we will encounter an occluded tile after an unoccluded
    // tile is if the priorty bin decreased, the tile is required for
    // activation, or the scale changed.
    if (tile_is_occluded) {
      occluded_count++;

      bool last_tile_is_occluded = last_tile.is_occluded();
      if (!last_tile_is_occluded) {
        TilePriority::PriorityBin tile_priority_bin =
            prioritized_tile.priority().priority_bin;
        TilePriority::PriorityBin last_tile_priority_bin =
            last_tile.priority().priority_bin;

        EXPECT_TRUE((tile_priority_bin < last_tile_priority_bin) ||
                    prioritized_tile.tile()->required_for_activation() ||
                    (prioritized_tile.tile()->raster_transform() !=
                     last_tile.tile()->raster_transform()));
      }
    }
    last_tile = prioritized_tile;
    queue->Pop();
  }
  size_t expected_occluded_count = pending_child_high_res_tiles.size();
  EXPECT_EQ(expected_occluded_count, occluded_count);
}

TEST_F(TileManagerTilePriorityQueueTest,
       EvictionTilePriorityQueueWithTransparentLayer) {
  host_impl()->AdvanceToNextFrame(base::Milliseconds(1));

  gfx::Size layer_bounds(1000, 1000);

  scoped_refptr<FakeRasterSource> pending_raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);
  SetupPendingTree(pending_raster_source);

  auto* pending_child_layer = AddLayer<FakePictureLayerImpl>(
      host_impl()->pending_tree(), pending_raster_source);
  pending_child_layer->SetElementId(
      LayerIdToElementIdForTesting(pending_child_layer->id()));
  CopyProperties(pending_layer(), pending_child_layer);

  // Create a fully transparent child layer so that its tile priorities are not
  // considered to be valid.
  pending_child_layer->SetDrawsContent(true);
  CreateEffectNode(pending_child_layer).render_surface_reason =
      RenderSurfaceReason::kTest;

  host_impl()->AdvanceToNextFrame(base::Milliseconds(1));
  UpdateDrawProperties(host_impl()->pending_tree());

  host_impl()->pending_tree()->SetOpacityMutated(
      pending_child_layer->element_id(), 0.0f);

  host_impl()->AdvanceToNextFrame(base::Milliseconds(1));
  host_impl()->pending_tree()->UpdateDrawProperties(
      /*update_tiles=*/true, /*update_image_animation_controller=*/true);

  // Renew all of the tile priorities.
  gfx::Rect viewport(layer_bounds);
  pending_layer()->picture_layer_tiling_set()->UpdateTilePriorities(
      viewport, 1.0f, 1.0, Occlusion(), true);
  pending_child_layer->picture_layer_tiling_set()->UpdateTilePriorities(
      viewport, 1.0f, 1.0, Occlusion(), true);

  // Populate all tiles directly from the tilings.
  std::set<Tile*> all_pending_tiles;
  std::vector<Tile*> pending_high_res_tiles =
      pending_layer()->HighResTiling()->AllTilesForTesting();
  all_pending_tiles.insert(pending_high_res_tiles.begin(),
                           pending_high_res_tiles.end());
  EXPECT_EQ(16u, pending_high_res_tiles.size());

  std::set<Tile*> all_pending_child_tiles;
  std::vector<Tile*> pending_child_high_res_tiles =
      pending_child_layer->HighResTiling()->AllTilesForTesting();
  all_pending_child_tiles.insert(pending_child_high_res_tiles.begin(),
                                 pending_child_high_res_tiles.end());
  EXPECT_EQ(16u, pending_child_high_res_tiles.size());

  std::set<Tile*> all_tiles = all_pending_tiles;
  all_tiles.insert(all_pending_child_tiles.begin(),
                   all_pending_child_tiles.end());

  tile_manager()->InitializeTilesWithResourcesForTesting(
      std::vector<Tile*>(all_tiles.begin(), all_tiles.end()));

  EXPECT_TRUE(pending_layer()->HasValidTilePriorities());
  EXPECT_FALSE(pending_child_layer->HasValidTilePriorities());

  // Verify that eviction queue returns tiles also from layers without valid
  // tile priorities and that the tile priority bin of those tiles is (at most)
  // EVENTUALLY.
  TreePriority tree_priority = NEW_CONTENT_TAKES_PRIORITY;
  std::set<Tile*> new_content_tiles;
  size_t tile_count = 0;
  std::unique_ptr<EvictionTilePriorityQueue> queue(
      host_impl()->BuildEvictionQueue(tree_priority));
  while (!queue->IsEmpty()) {
    PrioritizedTile prioritized_tile = queue->Top();
    Tile* tile = prioritized_tile.tile();
    const TilePriority& pending_priority = prioritized_tile.priority();
    EXPECT_NE(std::numeric_limits<float>::infinity(),
              pending_priority.distance_to_visible);
    if (base::Contains(all_pending_child_tiles, tile)) {
      EXPECT_EQ(TilePriority::EVENTUALLY, pending_priority.priority_bin);
    } else {
      EXPECT_EQ(TilePriority::NOW, pending_priority.priority_bin);
    }
    new_content_tiles.insert(tile);
    ++tile_count;
    queue->Pop();
  }
  EXPECT_EQ(tile_count, new_content_tiles.size());
  EXPECT_EQ(all_tiles, new_content_tiles);
}

TEST_F(TileManagerTilePriorityQueueTest, RasterTilePriorityQueueEmptyLayers) {
  const gfx::Size layer_bounds(1000, 1000);
  host_impl()->active_tree()->SetDeviceViewportRect(gfx::Rect(layer_bounds));
  SetupDefaultTrees(layer_bounds);

  std::unique_ptr<RasterTilePriorityQueue> queue(host_impl()->BuildRasterQueue(
      SAME_PRIORITY_FOR_BOTH_TREES, RasterTilePriorityQueue::Type::ALL));
  EXPECT_FALSE(queue->IsEmpty());

  size_t tile_count = 0;
  std::set<Tile*> all_tiles;
  while (!queue->IsEmpty()) {
    EXPECT_TRUE(queue->Top().tile());
    all_tiles.insert(queue->Top().tile());
    ++tile_count;
    queue->Pop();
  }

  EXPECT_EQ(tile_count, all_tiles.size());
  EXPECT_EQ(16u, tile_count);

  for (int i = 1; i < 10; ++i) {
    auto* pending_child_layer =
        AddLayer<FakePictureLayerImpl>(host_impl()->pending_tree());
    pending_child_layer->SetDrawsContent(true);
    pending_child_layer->set_has_valid_tile_priorities(true);
    CopyProperties(pending_layer(), pending_child_layer);
  }

  queue = host_impl()->BuildRasterQueue(SAME_PRIORITY_FOR_BOTH_TREES,
                                        RasterTilePriorityQueue::Type::ALL);
  EXPECT_FALSE(queue->IsEmpty());

  tile_count = 0;
  all_tiles.clear();
  while (!queue->IsEmpty()) {
    EXPECT_TRUE(queue->Top().tile());
    all_tiles.insert(queue->Top().tile());
    ++tile_count;
    queue->Pop();
  }
  EXPECT_EQ(tile_count, all_tiles.size());
  EXPECT_EQ(16u, tile_count);
}

TEST_F(TileManagerTilePriorityQueueTest, EvictionTilePriorityQueueEmptyLayers) {
  const gfx::Size layer_bounds(1000, 1000);
  host_impl()->active_tree()->SetDeviceViewportRect(gfx::Rect(layer_bounds));
  SetupDefaultTrees(layer_bounds);

  std::unique_ptr<RasterTilePriorityQueue> raster_queue(
      host_impl()->BuildRasterQueue(SAME_PRIORITY_FOR_BOTH_TREES,
                                    RasterTilePriorityQueue::Type::ALL));
  EXPECT_FALSE(raster_queue->IsEmpty());

  size_t tile_count = 0;
  std::set<Tile*> all_tiles;
  while (!raster_queue->IsEmpty()) {
    EXPECT_TRUE(raster_queue->Top().tile());
    all_tiles.insert(raster_queue->Top().tile());
    ++tile_count;
    raster_queue->Pop();
  }
  EXPECT_EQ(tile_count, all_tiles.size());
  EXPECT_EQ(16u, tile_count);

  std::vector<Tile*> tiles(all_tiles.begin(), all_tiles.end());
  host_impl()->tile_manager()->InitializeTilesWithResourcesForTesting(tiles);

  for (int i = 1; i < 10; ++i) {
    auto* pending_child_layer =
        AddLayer<FakePictureLayerImpl>(host_impl()->pending_tree());
    pending_child_layer->SetDrawsContent(true);
    pending_child_layer->set_has_valid_tile_priorities(true);
    CopyProperties(pending_layer(), pending_child_layer);
  }

  std::unique_ptr<EvictionTilePriorityQueue> queue(
      host_impl()->BuildEvictionQueue(SAME_PRIORITY_FOR_BOTH_TREES));
  EXPECT_FALSE(queue->IsEmpty());

  tile_count = 0;
  all_tiles.clear();
  while (!queue->IsEmpty()) {
    EXPECT_TRUE(queue->Top().tile());
    all_tiles.insert(queue->Top().tile());
    ++tile_count;
    queue->Pop();
  }
  EXPECT_EQ(tile_count, all_tiles.size());
  EXPECT_EQ(16u, tile_count);
}

TEST_F(TileManagerTilePriorityQueueTest,
       RasterTilePriorityQueueStaticViewport) {
  FakePictureLayerTilingClient client;

  gfx::Rect viewport(50, 50, 500, 500);
  gfx::Size layer_bounds(1600, 1600);

  const int soon_border_outset = 312;
  gfx::Rect soon_rect = viewport;
  soon_rect.Inset(-soon_border_outset);

  client.SetTileSize(gfx::Size(30, 30));
  LayerTreeSettings settings;

  std::unique_ptr<PictureLayerTilingSet> tiling_set =
      PictureLayerTilingSet::Create(
          ACTIVE_TREE, &client, settings.tiling_interest_area_padding,
          settings.skewport_target_time_in_seconds,
          settings.skewport_extrapolation_limit_in_screen_pixels,
          settings.max_preraster_distance_in_screen_pixels);

  scoped_refptr<FakeRasterSource> raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);
  PictureLayerTiling* tiling =
      tiling_set->AddTiling(gfx::AxisTransform2d(), raster_source);
  tiling->set_resolution(HIGH_RESOLUTION);

  tiling_set->UpdateTilePriorities(viewport, 1.0f, 1.0, Occlusion(), true);
  std::vector<Tile*> all_tiles = tiling->AllTilesForTesting();
  // Sanity check.
  EXPECT_EQ(3364u, all_tiles.size());

  // The explanation of each iteration is as follows:
  // 1. First iteration tests that we can get all of the tiles correctly.
  // 2. Second iteration ensures that we can get all of the tiles again (first
  //    iteration didn't change any tiles), as well set all tiles to be ready to
  //    draw.
  // 3. Third iteration ensures that no tiles are returned, since they were all
  //    marked as ready to draw.
  for (int i = 0; i < 3; ++i) {
    std::unique_ptr<TilingSetRasterQueueAll> queue =
        TilingSetRasterQueueAll::Create(tiling_set.get(), false, false);
    EXPECT_TRUE(queue);

    // There are 3 bins in TilePriority.
    bool have_tiles[3] = {};

    // On the third iteration, we should get no tiles since everything was
    // marked as ready to draw.
    if (i == 2) {
      EXPECT_TRUE(queue->IsEmpty());
      continue;
    }

    EXPECT_FALSE(queue->IsEmpty());
    std::set<Tile*> unique_tiles;
    unique_tiles.insert(queue->Top().tile());
    PrioritizedTile last_tile = queue->Top();
    have_tiles[last_tile.priority().priority_bin] = true;

    // On the second iteration, mark everything as ready to draw (solid color).
    if (i == 1) {
      TileDrawInfo& draw_info = last_tile.tile()->draw_info();
      draw_info.SetSolidColorForTesting(SkColors::kRed);
    }
    queue->Pop();
    int eventually_bin_order_correct_count = 0;
    int eventually_bin_order_incorrect_count = 0;
    while (!queue->IsEmpty()) {
      PrioritizedTile new_tile = queue->Top();
      queue->Pop();
      unique_tiles.insert(new_tile.tile());

      TilePriority last_priority = last_tile.priority();
      TilePriority new_priority = new_tile.priority();
      EXPECT_LE(last_priority.priority_bin, new_priority.priority_bin);
      if (last_priority.priority_bin == new_priority.priority_bin) {
        if (last_priority.priority_bin == TilePriority::EVENTUALLY) {
          bool order_correct = last_priority.distance_to_visible <=
                               new_priority.distance_to_visible;
          eventually_bin_order_correct_count += order_correct;
          eventually_bin_order_incorrect_count += !order_correct;
        } else if (!soon_rect.Intersects(new_tile.tile()->content_rect()) &&
                   !soon_rect.Intersects(last_tile.tile()->content_rect())) {
          EXPECT_LE(last_priority.distance_to_visible,
                    new_priority.distance_to_visible);
          EXPECT_EQ(TilePriority::NOW, new_priority.priority_bin);
        } else if (new_priority.distance_to_visible > 0.f) {
          EXPECT_EQ(TilePriority::SOON, new_priority.priority_bin);
        }
      }
      have_tiles[new_priority.priority_bin] = true;

      last_tile = new_tile;

      // On the second iteration, mark everything as ready to draw (solid
      // color).
      if (i == 1) {
        TileDrawInfo& draw_info = last_tile.tile()->draw_info();
        draw_info.SetSolidColorForTesting(SkColors::kRed);
      }
    }

    EXPECT_GT(eventually_bin_order_correct_count,
              eventually_bin_order_incorrect_count);

    // We should have now and eventually tiles, as well as soon tiles from
    // the border region.
    EXPECT_TRUE(have_tiles[TilePriority::NOW]);
    EXPECT_TRUE(have_tiles[TilePriority::SOON]);
    EXPECT_TRUE(have_tiles[TilePriority::EVENTUALLY]);

    EXPECT_EQ(unique_tiles.size(), all_tiles.size());
  }
}

TEST_F(TileManagerTilePriorityQueueTest,
       RasterTilePriorityQueueMovingViewport) {
  FakePictureLayerTilingClient client;

  gfx::Rect viewport(50, 0, 100, 100);
  gfx::Rect moved_viewport(50, 0, 100, 500);
  gfx::Size layer_bounds(1000, 1000);

  client.SetTileSize(gfx::Size(30, 30));
  LayerTreeSettings settings;

  std::unique_ptr<PictureLayerTilingSet> tiling_set =
      PictureLayerTilingSet::Create(
          ACTIVE_TREE, &client, settings.tiling_interest_area_padding,
          settings.skewport_target_time_in_seconds,
          settings.skewport_extrapolation_limit_in_screen_pixels,
          settings.max_preraster_distance_in_screen_pixels);

  scoped_refptr<FakeRasterSource> raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);
  PictureLayerTiling* tiling =
      tiling_set->AddTiling(gfx::AxisTransform2d(), raster_source);
  tiling->set_resolution(HIGH_RESOLUTION);

  tiling_set->UpdateTilePriorities(viewport, 1.0f, 1.0, Occlusion(), true);
  tiling_set->UpdateTilePriorities(moved_viewport, 1.0f, 2.0, Occlusion(),
                                   true);

  const int soon_border_outset = 312;
  gfx::Rect soon_rect = moved_viewport;
  soon_rect.Inset(-soon_border_outset);

  // There are 3 bins in TilePriority.
  bool have_tiles[3] = {};
  PrioritizedTile last_tile;
  int eventually_bin_order_correct_count = 0;
  int eventually_bin_order_incorrect_count = 0;
  std::unique_ptr<TilingSetRasterQueueAll> queue =
      TilingSetRasterQueueAll::Create(tiling_set.get(), false, false);
  EXPECT_TRUE(queue);
  for (; !queue->IsEmpty(); queue->Pop()) {
    if (!last_tile.tile())
      last_tile = queue->Top();

    const PrioritizedTile& new_tile = queue->Top();

    TilePriority last_priority = last_tile.priority();
    TilePriority new_priority = new_tile.priority();

    have_tiles[new_priority.priority_bin] = true;

    EXPECT_LE(last_priority.priority_bin, new_priority.priority_bin);
    if (last_priority.priority_bin == new_priority.priority_bin) {
      if (last_priority.priority_bin == TilePriority::EVENTUALLY) {
        bool order_correct = last_priority.distance_to_visible <=
                             new_priority.distance_to_visible;
        eventually_bin_order_correct_count += order_correct;
        eventually_bin_order_incorrect_count += !order_correct;
      } else if (!soon_rect.Intersects(new_tile.tile()->content_rect()) &&
                 !soon_rect.Intersects(last_tile.tile()->content_rect())) {
        EXPECT_LE(last_priority.distance_to_visible,
                  new_priority.distance_to_visible);
      } else if (new_priority.distance_to_visible > 0.f) {
        EXPECT_EQ(TilePriority::SOON, new_priority.priority_bin);
      }
    }
    last_tile = new_tile;
  }

  EXPECT_GT(eventually_bin_order_correct_count,
            eventually_bin_order_incorrect_count);

  EXPECT_TRUE(have_tiles[TilePriority::NOW]);
  EXPECT_TRUE(have_tiles[TilePriority::SOON]);
  EXPECT_TRUE(have_tiles[TilePriority::EVENTUALLY]);
}

TEST_F(TileManagerTilePriorityQueueTest, SetIsLikelyToRequireADraw) {
  const gfx::Size layer_bounds(1000, 1000);
  host_impl()->active_tree()->SetDeviceViewportRect(gfx::Rect(layer_bounds));
  SetupDefaultTrees(layer_bounds);

  // Verify that the queue has a required for draw tile at Top.
  std::unique_ptr<RasterTilePriorityQueue> queue(host_impl()->BuildRasterQueue(
      SAME_PRIORITY_FOR_BOTH_TREES, RasterTilePriorityQueue::Type::ALL));
  EXPECT_FALSE(queue->IsEmpty());
  EXPECT_TRUE(queue->Top().tile()->required_for_draw());

  EXPECT_FALSE(host_impl()->is_likely_to_require_a_draw());
  host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());
  EXPECT_TRUE(host_impl()->is_likely_to_require_a_draw());
}

TEST_F(TileManagerTilePriorityQueueTest,
       SetIsLikelyToRequireADrawOnZeroMemoryBudget) {
  const gfx::Size layer_bounds(1000, 1000);
  host_impl()->active_tree()->SetDeviceViewportRect(gfx::Rect(layer_bounds));
  SetupDefaultTrees(layer_bounds);

  // Verify that the queue has a required for draw tile at Top.
  std::unique_ptr<RasterTilePriorityQueue> queue(host_impl()->BuildRasterQueue(
      SAME_PRIORITY_FOR_BOTH_TREES, RasterTilePriorityQueue::Type::ALL));
  EXPECT_FALSE(queue->IsEmpty());
  EXPECT_TRUE(queue->Top().tile()->required_for_draw());

  ManagedMemoryPolicy policy = host_impl()->ActualManagedMemoryPolicy();
  policy.bytes_limit_when_visible = 0;
  host_impl()->SetMemoryPolicy(policy);

  EXPECT_FALSE(host_impl()->is_likely_to_require_a_draw());
  host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());
  EXPECT_FALSE(host_impl()->is_likely_to_require_a_draw());
}

TEST_F(TileManagerTilePriorityQueueTest,
       SetIsLikelyToRequireADrawOnLimitedMemoryBudget) {
  const gfx::Size layer_bounds(1000, 1000);
  host_impl()->active_tree()->SetDeviceViewportRect(gfx::Rect(layer_bounds));
  SetupDefaultTrees(layer_bounds);

  // Verify that the queue has a required for draw tile at Top.
  std::unique_ptr<RasterTilePriorityQueue> queue(host_impl()->BuildRasterQueue(
      SAME_PRIORITY_FOR_BOTH_TREES, RasterTilePriorityQueue::Type::ALL));
  EXPECT_FALSE(queue->IsEmpty());
  EXPECT_TRUE(queue->Top().tile()->required_for_draw());
  EXPECT_EQ(gfx::Size(256, 256), queue->Top().tile()->desired_texture_size());

  ManagedMemoryPolicy policy = host_impl()->ActualManagedMemoryPolicy();
  policy.bytes_limit_when_visible =
      viz::SinglePlaneFormat::kRGBA_8888.EstimatedSizeInBytes(
          gfx::Size(256, 256));
  host_impl()->SetMemoryPolicy(policy);

  EXPECT_FALSE(host_impl()->is_likely_to_require_a_draw());
  host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());
  EXPECT_TRUE(host_impl()->is_likely_to_require_a_draw());

  ResourcePool::InUsePoolResource resource =
      host_impl()->resource_pool()->AcquireResource(
          gfx::Size(256, 256), viz::SinglePlaneFormat::kRGBA_8888,
          gfx::ColorSpace());
  resource.set_gpu_backing(std::make_unique<StubGpuBacking>());

  host_impl()->tile_manager()->CheckIfMoreTilesNeedToBePreparedForTesting();
  EXPECT_FALSE(host_impl()->is_likely_to_require_a_draw());

  host_impl()->resource_pool()->ReleaseResource(std::move(resource));
}

TEST_F(TileManagerTilePriorityQueueTest, DefaultMemoryPolicy) {
  const gfx::Size layer_bounds(1000, 1000);
  host_impl()->active_tree()->SetDeviceViewportRect(gfx::Rect(layer_bounds));
  SetupDefaultTrees(layer_bounds);

  host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());

  // 64MB is the default mem limit.
  EXPECT_EQ(67108864u,
            host_impl()->global_tile_state().hard_memory_limit_in_bytes);
  EXPECT_EQ(TileMemoryLimitPolicy::ALLOW_ANYTHING,
            host_impl()->global_tile_state().memory_limit_policy);
  EXPECT_EQ(ManagedMemoryPolicy::kDefaultNumResourcesLimit,
            host_impl()->global_tile_state().num_resources_limit);
}

TEST_F(TileManagerTilePriorityQueueTest, RasterQueueAllUsesCorrectTileBounds) {
  // Verify that we use the real tile bounds when advancing phases during the
  // tile iteration.
  gfx::Size layer_bounds(1, 1);

  scoped_refptr<FakeRasterSource> raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);

  FakePictureLayerTilingClient pending_client;
  pending_client.SetTileSize(gfx::Size(64, 64));

  std::unique_ptr<PictureLayerTilingSet> tiling_set =
      PictureLayerTilingSet::Create(WhichTree::ACTIVE_TREE, &pending_client,
                                    1.0f, 1.0f, 1000, 1000.f);
  pending_client.set_twin_tiling_set(tiling_set.get());

  auto* tiling = tiling_set->AddTiling(gfx::AxisTransform2d(), raster_source);

  tiling->set_resolution(HIGH_RESOLUTION);
  tiling->CreateAllTilesForTesting();

  // The tile is (0, 0, 1, 1), create an intersecting and non-intersecting
  // rectangle to test the advance phase with. The tile size is (64, 64), so
  // both rectangles intersect the tile content size, but only one should
  // intersect the actual size.
  gfx::Rect non_intersecting_rect(2, 2, 10, 10);
  gfx::Rect intersecting_rect(0, 0, 10, 10);
  {
    tiling->SetTilePriorityRectsForTesting(
        non_intersecting_rect,  // Visible rect.
        intersecting_rect,      // Skewport rect.
        intersecting_rect,      // Soon rect.
        intersecting_rect);     // Eventually rect.
    std::unique_ptr<TilingSetRasterQueueAll> queue =
        TilingSetRasterQueueAll::Create(tiling_set.get(), false, false);
    if (features::IsCCSlimmingEnabled()) {
      EXPECT_FALSE(queue);
    } else {
      EXPECT_TRUE(queue);
    }
  }

  // This is the behavior difference between FakePictureLayerTilingClient
  // and the real PictureLayerTilingClient a.k.a. PictureLayerImpl.
  // The FakePictureLayerTilingClient doesn't clear PictureLayerTilingSet's
  // |all_tiles_done_| when AddTile() is called whereas PictureLayerImpl does.
  tiling_set->set_all_tiles_done(false);

  {
    tiling->SetTilePriorityRectsForTesting(
        non_intersecting_rect,  // Visible rect.
        intersecting_rect,      // Skewport rect.
        intersecting_rect,      // Soon rect.
        intersecting_rect);     // Eventually rect.
    std::unique_ptr<TilingSetRasterQueueAll> queue =
        TilingSetRasterQueueAll::Create(tiling_set.get(), false, false);
    EXPECT_TRUE(queue);
    EXPECT_FALSE(queue->IsEmpty());
  }

  {
    tiling->SetTilePriorityRectsForTesting(
        non_intersecting_rect,  // Visible rect.
        non_intersecting_rect,  // Skewport rect.
        intersecting_rect,      // Soon rect.
        intersecting_rect);     // Eventually rect.
    std::unique_ptr<TilingSetRasterQueueAll> queue =
        TilingSetRasterQueueAll::Create(tiling_set.get(), false, false);
    EXPECT_TRUE(queue);
    EXPECT_FALSE(queue->IsEmpty());
  }
  {
    tiling->SetTilePriorityRectsForTesting(
        non_intersecting_rect,  // Visible rect.
        non_intersecting_rect,  // Skewport rect.
        non_intersecting_rect,  // Soon rect.
        intersecting_rect);     // Eventually rect.
    std::unique_ptr<TilingSetRasterQueueAll> queue =
        TilingSetRasterQueueAll::Create(tiling_set.get(), false, false);
    EXPECT_TRUE(queue);
    EXPECT_FALSE(queue->IsEmpty());
  }
}

TEST_F(TileManagerTilePriorityQueueTest, NoRasterTasksforSolidColorTiles) {
  gfx::Size size(10, 10);
  const gfx::Size layer_bounds(1000, 1000);

  FakeRecordingSource recording_source(layer_bounds);

  PaintFlags solid_flags;
  SkColor4f solid_color{0.1f, 0.2f, 0.3f, 1.0f};
  solid_flags.setColor(solid_color);
  recording_source.add_draw_rect_with_flags(gfx::Rect(layer_bounds),
                                            solid_flags);

  // Create non solid tile as well, otherwise tilings wouldnt be created.
  SkColor4f non_solid_color{0.2f, 0.3f, 0.4f, 0.5f};
  PaintFlags non_solid_flags;
  non_solid_flags.setColor(non_solid_color);

  recording_source.add_draw_rect_with_flags(gfx::Rect(0, 0, 10, 10),
                                            non_solid_flags);
  recording_source.Rerecord();

  scoped_refptr<RasterSource> raster_source =
      recording_source.CreateRasterSource();

  FakePictureLayerTilingClient tiling_client;
  tiling_client.SetTileSize(size);

  SetupTrees(raster_source, raster_source);
  pending_layer_->set_contributes_to_drawn_render_surface(true);
  PictureLayerTilingSet* tiling_set =
      pending_layer_->picture_layer_tiling_set();
  tiling_set->RemoveAllTilings();

  PictureLayerTiling* tiling =
      tiling_set->AddTiling(gfx::AxisTransform2d(), raster_source);
  tiling->set_resolution(HIGH_RESOLUTION);
  tiling->CreateAllTilesForTesting(gfx::Rect(layer_bounds));

  host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());

  std::vector<Tile*> tiles = tiling->AllTilesForTesting();
  for (size_t tile_idx = 0; tile_idx < tiles.size(); ++tile_idx) {
    Tile* tile = tiles[tile_idx];
    if (tile->id() == 1) {
      // Non-solid tile.
      EXPECT_TRUE(tile->HasRasterTask());
      EXPECT_EQ(TileDrawInfo::RESOURCE_MODE, tile->draw_info().mode());
    } else {
      EXPECT_FALSE(tile->HasRasterTask());
      EXPECT_EQ(TileDrawInfo::SOLID_COLOR_MODE, tile->draw_info().mode());
      EXPECT_EQ(solid_color, tile->draw_info().solid_color());
    }
  }
}

class TestSoftwareBacking : public ResourcePool::SoftwareBacking {
 public:
  // No tracing is done during these tests.
  void OnMemoryDump(
      base::trace_event::ProcessMemoryDump* pmd,
      const base::trace_event::MemoryAllocatorDumpGuid& buffer_dump_guid,
      uint64_t tracing_process_id,
      int importance) const override {}

  std::unique_ptr<uint32_t[]> pixels;
};

// A RasterBufferProvider that allocates software backings with a standard
// array as the backing. Overrides Playback() on the RasterBuffer to raster
// into the pixels in the array.
class TestSoftwareRasterBufferProvider : public FakeRasterBufferProviderImpl {
 public:
  static constexpr bool kIsGpuCompositing = true;
  static constexpr viz::SharedImageFormat kSharedImageFormat =
      viz::SinglePlaneFormat::kRGBA_8888;

  std::unique_ptr<RasterBuffer> AcquireBufferForRaster(
      const ResourcePool::InUsePoolResource& resource,
      uint64_t resource_content_id,
      uint64_t previous_content_id,
      bool depends_on_at_raster_decodes,
      bool depends_on_hardware_accelerated_jpeg_candidates,
      bool depends_on_hardware_accelerated_webp_candidates) override {
    if (!resource.software_backing()) {
      auto backing = std::make_unique<TestSoftwareBacking>();
      backing->shared_bitmap_id = viz::SharedBitmap::GenerateId();
      backing->pixels = std::make_unique<uint32_t[]>(
          viz::ResourceSizes::CheckedSizeInBytes<size_t>(resource.size(),
                                                         kSharedImageFormat));
      resource.set_software_backing(std::move(backing));
    }
    auto* backing =
        static_cast<TestSoftwareBacking*>(resource.software_backing());
    return std::make_unique<TestRasterBuffer>(resource.size(),
                                              backing->pixels.get());
  }

 private:
  class TestRasterBuffer : public RasterBuffer {
   public:
    TestRasterBuffer(const gfx::Size& size, void* pixels)
        : size_(size), pixels_(pixels) {}

    void Playback(const RasterSource* raster_source,
                  const gfx::Rect& raster_full_rect,
                  const gfx::Rect& raster_dirty_rect,
                  uint64_t new_content_id,
                  const gfx::AxisTransform2d& transform,
                  const RasterSource::PlaybackSettings& playback_settings,
                  const GURL& url) override {
      RasterBufferProvider::PlaybackToMemory(
          pixels_, kSharedImageFormat, size_, /*stride=*/0, raster_source,
          raster_full_rect, /*canvas_playback_rect=*/raster_full_rect,
          transform, gfx::ColorSpace(), kIsGpuCompositing, playback_settings);
    }

    bool SupportsBackgroundThreadPriority() const override { return true; }

   private:
    gfx::Size size_;
    raw_ptr<void> pixels_;
  };
};

class TileManagerTest : public TestLayerTreeHostBase {
 public:
  LayerTreeSettings CreateSettings() override {
    LayerTreeSettings settings = TestLayerTreeHostBase::CreateSettings();
    CHECK(!settings.commit_to_active_tree);
    // This is required in commit-to-pending-tree mode to call
    // NotifyReadyToDraw().
    settings.wait_for_all_pipeline_stages_before_draw = true;
    return settings;
  }

  // MockLayerTreeHostImpl allows us to intercept tile manager callbacks.
  class MockLayerTreeHostImpl : public FakeLayerTreeHostImpl {
   public:
    MockLayerTreeHostImpl(const LayerTreeSettings& settings,
                          TaskRunnerProvider* task_runner_provider,
                          TaskGraphRunner* task_graph_runner)
        : FakeLayerTreeHostImpl(settings,
                                task_runner_provider,
                                task_graph_runner) {}

    MOCK_METHOD0(NotifyReadyToActivate, void());
    MOCK_METHOD0(NotifyReadyToDraw, void());
    MOCK_METHOD0(NotifyAllTileTasksCompleted, void());
  };

  std::unique_ptr<FakeLayerTreeHostImpl> CreateHostImpl(
      const LayerTreeSettings& settings,
      TaskRunnerProvider* task_runner_provider,
      TaskGraphRunner* task_graph_runner) override {
    return std::make_unique<testing::NiceMock<MockLayerTreeHostImpl>>(
        settings, task_runner_provider, task_graph_runner);
  }

  // By default use software compositing (no context provider).
  std::unique_ptr<LayerTreeFrameSink> CreateLayerTreeFrameSink() override {
    return FakeLayerTreeFrameSink::CreateSoftware();
  }

  MockLayerTreeHostImpl& MockHostImpl() {
    return *static_cast<MockLayerTreeHostImpl*>(host_impl());
  }
};

// Test to ensure that we call NotifyAllTileTasksCompleted when PrepareTiles is
// called.
TEST_F(TileManagerTest, AllWorkFinished) {
  // Check with no tile work enqueued.
  {
    base::RunLoop run_loop;
    EXPECT_FALSE(
        host_impl()->tile_manager()->HasScheduledTileTasksForTesting());
    EXPECT_CALL(MockHostImpl(), NotifyReadyToDraw());
    EXPECT_CALL(MockHostImpl(), NotifyAllTileTasksCompleted())
        .WillOnce(testing::Invoke([&run_loop]() { run_loop.Quit(); }));
    host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());
    EXPECT_TRUE(host_impl()->tile_manager()->HasScheduledTileTasksForTesting());
    run_loop.Run();
  }

  // Check that the "schedule more work" path also triggers the expected
  // callback.
  {
    base::RunLoop run_loop;
    EXPECT_FALSE(
        host_impl()->tile_manager()->HasScheduledTileTasksForTesting());
    EXPECT_CALL(MockHostImpl(), NotifyReadyToDraw());
    EXPECT_CALL(MockHostImpl(), NotifyAllTileTasksCompleted())
        .WillOnce(testing::Invoke([&run_loop]() { run_loop.Quit(); }));
    host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());
    host_impl()->tile_manager()->SetMoreTilesNeedToBeRasterizedForTesting();
    EXPECT_TRUE(host_impl()->tile_manager()->HasScheduledTileTasksForTesting());
    run_loop.Run();
  }

  // Check that if callbacks are called by CheckIfMoreTilesNeedToBePrepared if
  // they haven't been called already.
  {
    base::RunLoop run_loop;
    EXPECT_FALSE(
        host_impl()->tile_manager()->HasScheduledTileTasksForTesting());
    EXPECT_CALL(MockHostImpl(), NotifyReadyToDraw());
    EXPECT_CALL(MockHostImpl(), NotifyAllTileTasksCompleted())
        .WillOnce(testing::Invoke([&run_loop]() { run_loop.Quit(); }));
    host_impl()->tile_manager()->ResetSignalsForTesting();
    host_impl()->tile_manager()->SetMoreTilesNeedToBeRasterizedForTesting();
    host_impl()->tile_manager()->CheckIfMoreTilesNeedToBePreparedForTesting();
    run_loop.Run();
  }

  // Same test as above but with SMOOTHNESS_TAKES_PRIORITY.
  {
    base::RunLoop run_loop;
    EXPECT_FALSE(
        host_impl()->tile_manager()->HasScheduledTileTasksForTesting());
    EXPECT_CALL(MockHostImpl(), NotifyReadyToDraw());
    EXPECT_CALL(MockHostImpl(), NotifyAllTileTasksCompleted())
        .WillOnce(testing::Invoke([&run_loop]() { run_loop.Quit(); }));
    host_impl()->tile_manager()->ResetSignalsForTesting();
    auto global_state = host_impl()->global_tile_state();
    global_state.tree_priority = SMOOTHNESS_TAKES_PRIORITY;
    host_impl()->tile_manager()->SetGlobalStateForTesting(global_state);
    host_impl()->tile_manager()->SetMoreTilesNeedToBeRasterizedForTesting();
    host_impl()->tile_manager()->CheckIfMoreTilesNeedToBePreparedForTesting();
    run_loop.Run();
  }
}

TEST_F(TileManagerTest, ActivateAndDrawWhenOOM) {
  SetupDefaultTrees(gfx::Size(1000, 1000));

  auto global_state = host_impl()->global_tile_state();
  global_state.hard_memory_limit_in_bytes = 1u;
  global_state.soft_memory_limit_in_bytes = 1u;

  {
    base::RunLoop run_loop;
    EXPECT_FALSE(
        host_impl()->tile_manager()->HasScheduledTileTasksForTesting());
    EXPECT_CALL(MockHostImpl(), NotifyReadyToActivate());
    EXPECT_CALL(MockHostImpl(), NotifyReadyToDraw());
    EXPECT_CALL(MockHostImpl(), NotifyAllTileTasksCompleted())
        .WillOnce(testing::Invoke([&run_loop]() { run_loop.Quit(); }));
    host_impl()->tile_manager()->PrepareTiles(global_state);
    EXPECT_TRUE(host_impl()->tile_manager()->HasScheduledTileTasksForTesting());
    run_loop.Run();
  }

  EXPECT_TRUE(host_impl()->tile_manager()->IsReadyToDraw());
  EXPECT_TRUE(host_impl()->tile_manager()->IsReadyToActivate());
  EXPECT_TRUE(host_impl()->notify_tile_state_changed_called());

  // Next PrepareTiles should skip NotifyTileStateChanged since all tiles
  // are marked oom already.
  {
    base::RunLoop run_loop;
    host_impl()->set_notify_tile_state_changed_called(false);
    EXPECT_CALL(MockHostImpl(), NotifyReadyToActivate());
    EXPECT_CALL(MockHostImpl(), NotifyReadyToDraw());
    EXPECT_CALL(MockHostImpl(), NotifyAllTileTasksCompleted())
        .WillOnce(testing::Invoke([&run_loop]() { run_loop.Quit(); }));
    host_impl()->tile_manager()->PrepareTiles(global_state);
    run_loop.Run();
    EXPECT_FALSE(host_impl()->notify_tile_state_changed_called());
  }
}

class TileManagerOcclusionTest : public TileManagerTest {
 public:
  LayerTreeSettings CreateSettings() override {
    auto settings = TileManagerTest::CreateSettings();
    settings.create_low_res_tiling = true;
    settings.use_occlusion_for_tile_prioritization = true;
    return settings;
  }

  void PrepareTilesAndWaitUntilDone(
      const GlobalStateThatImpactsTilePriority& state) {
    base::RunLoop run_loop;
    EXPECT_CALL(MockHostImpl(), NotifyAllTileTasksCompleted())
        .WillOnce(testing::Invoke([&run_loop]() { run_loop.Quit(); }));
    tile_manager()->PrepareTiles(state);
    run_loop.Run();
    tile_manager()->PrepareToDraw();
  }

  TileManager* tile_manager() { return host_impl()->tile_manager(); }
};

TEST_F(TileManagerOcclusionTest, OccludedTileEvictedForVisibleTile) {
  gfx::Size layer_bounds(256, 256);
  host_impl()->active_tree()->SetDeviceViewportRect(gfx::Rect(layer_bounds));

  SetupPendingTree(FakeRasterSource::CreateFilledWithText(layer_bounds));
  const int initial_picture_id = pending_layer()->id();
  ActivateTree();

  GlobalStateThatImpactsTilePriority global_state =
      host_impl()->global_tile_state();
  const LayerTreeSettings settings = CreateSettings();
  global_state.hard_memory_limit_in_bytes =
      global_state.soft_memory_limit_in_bytes =
          settings.default_tile_size.GetArea() * 4 + 1;

  // Call PrepareTiles and wait for it to complete. It's necessary to wait for
  // completion so that the resource is pushed to the tile, which is used
  // in calculating eviction.
  PrepareTilesAndWaitUntilDone(global_state);

  // At this point the initial PictureLayerImpl should have a Tile with a
  // resource.
  {
    PictureLayerImpl* initial_layer = static_cast<PictureLayerImpl*>(
        host_impl()->active_tree()->LayerById(initial_picture_id));
    ASSERT_NE(initial_layer, nullptr);
    ASSERT_EQ(1u, initial_layer->picture_layer_tiling_set()->num_tilings());
    std::vector<Tile*> initial_layer_tiles =
        initial_layer->picture_layer_tiling_set()
            ->tiling_at(0)
            ->AllTilesForTesting();
    ASSERT_EQ(1u, initial_layer_tiles.size());
    EXPECT_TRUE(initial_layer_tiles[0]->draw_info().has_resource());
  }

  // Add another layer on top. As there is only enough memory for one tile,
  // the top most tile should get a raster task and the bottom layer should
  // not.
  SetupPendingTree(FakeRasterSource::CreateFilledWithText(layer_bounds));

  // Advance the frame to ensure PictureLayerTilingSet applies the occlusion to
  // the PictureLayerTiling. The amount of time advanced doesn't matter.
  host_impl()->AdvanceToNextFrame(base::Seconds(2));

  auto* picture_layer = AddLayer<FakePictureLayerImpl>(
      host_impl()->pending_tree(),
      FakeRasterSource::CreateFilledWithText(layer_bounds));
  const int top_most_layer_id = picture_layer->id();
  picture_layer->SetDrawsContent(true);
  picture_layer->SetContentsOpaque(true);
  CopyProperties(pending_layer(), picture_layer);
  ActivateTree();
  PrepareTilesAndWaitUntilDone(global_state);
  {
    PictureLayerImpl* initial_layer = static_cast<PictureLayerImpl*>(
        host_impl()->active_tree()->LayerById(initial_picture_id));
    ASSERT_NE(initial_layer, nullptr);
    ASSERT_EQ(1u, initial_layer->picture_layer_tiling_set()->num_tilings());
    std::vector<Tile*> initial_layer_tiles =
        initial_layer->picture_layer_tiling_set()
            ->tiling_at(0)
            ->AllTilesForTesting();
    ASSERT_EQ(1u, initial_layer_tiles.size());
    EXPECT_FALSE(initial_layer_tiles[0]->draw_info().has_resource());

    PictureLayerImpl* top_most_layer = static_cast<PictureLayerImpl*>(
        host_impl()->active_tree()->LayerById(top_most_layer_id));
    ASSERT_NE(top_most_layer, nullptr);
    ASSERT_EQ(1u, top_most_layer->picture_layer_tiling_set()->num_tilings());
    std::vector<Tile*> top_most_layer_tiles =
        top_most_layer->picture_layer_tiling_set()
            ->tiling_at(0)
            ->AllTilesForTesting();
    ASSERT_EQ(1u, top_most_layer_tiles.size());
    EXPECT_TRUE(top_most_layer_tiles[0]->draw_info().has_resource());
  }
}

class PixelInspectTileManagerTest : public TileManagerTest {
 public:
  void SetUp() override {
    TileManagerTest::SetUp();
    // Use a RasterBufferProvider that will let us inspect pixels.
    host_impl()->tile_manager()->SetRasterBufferProviderForTesting(
        &raster_buffer_provider_);
  }

 private:
  TestSoftwareRasterBufferProvider raster_buffer_provider_;
};

TEST_F(PixelInspectTileManagerTest, LowResHasNoImage) {
  gfx::Size size(10, 12);
  TileResolution resolutions[] = {HIGH_RESOLUTION, LOW_RESOLUTION};
  host_impl()->CreatePendingTree();

  for (size_t i = 0; i < std::size(resolutions); ++i) {
    SCOPED_TRACE(resolutions[i]);

    // Make a RasterSource that will draw a blue bitmap image.
    sk_sp<SkSurface> surface = SkSurfaces::Raster(
        SkImageInfo::MakeN32Premul(size.width(), size.height()));
    ASSERT_NE(surface, nullptr);
    surface->getCanvas()->clear(SK_ColorBLUE);
    sk_sp<SkImage> blue_image = surface->makeImageSnapshot();

    FakeRecordingSource recording_source(size);
    recording_source.SetBackgroundColor(SkColors::kTransparent);
    recording_source.SetRequiresClear(true);
    PaintFlags flags;
    flags.setColor(SK_ColorGREEN);
    recording_source.add_draw_rect_with_flags(gfx::Rect(size), flags);
    recording_source.add_draw_image(std::move(blue_image), gfx::Point());
    recording_source.Rerecord();
    scoped_refptr<RasterSource> raster = recording_source.CreateRasterSource();

    std::unique_ptr<PictureLayerImpl> layer =
        PictureLayerImpl::Create(host_impl()->pending_tree(), 1);
    layer->SetBounds(size);
    Region invalidation;
    layer->UpdateRasterSource(raster, &invalidation);
    layer->RegenerateDiscardableImageMapIfNeeded();
    PictureLayerTilingSet* tiling_set = layer->picture_layer_tiling_set();
    layer->set_contributes_to_drawn_render_surface(true);

    auto* tiling = tiling_set->AddTiling(gfx::AxisTransform2d(), raster);
    tiling->set_resolution(resolutions[i]);
    tiling->CreateAllTilesForTesting(gfx::Rect(size));

    // SMOOTHNESS_TAKES_PRIORITY ensures that we will actually raster
    // LOW_RESOLUTION tiles, otherwise they are skipped.
    host_impl()->SetTreePriority(SMOOTHNESS_TAKES_PRIORITY);

    // Call PrepareTiles and wait for it to complete.
    auto* tile_manager = host_impl()->tile_manager();
    base::RunLoop run_loop;
    EXPECT_CALL(MockHostImpl(), NotifyAllTileTasksCompleted())
        .WillOnce(testing::Invoke([&run_loop]() { run_loop.Quit(); }));
    tile_manager->PrepareTiles(host_impl()->global_tile_state());
    run_loop.Run();
    tile_manager->PrepareToDraw();

    Tile* tile = tiling->TileAt(0, 0);
    // The tile in the tiling was rastered.
    EXPECT_EQ(TileDrawInfo::RESOURCE_MODE, tile->draw_info().mode());
    EXPECT_TRUE(tile->draw_info().IsReadyToDraw());

    gfx::Size resource_size = tile->draw_info().resource_size();
    SkColorType ct = ToClosestSkColorType(
        TestSoftwareRasterBufferProvider::kIsGpuCompositing,
        TestSoftwareRasterBufferProvider::kSharedImageFormat);
    auto info = SkImageInfo::Make(resource_size.width(), resource_size.height(),
                                  ct, kPremul_SkAlphaType);
    // CreateLayerTreeFrameSink() sets up a software compositing, so the
    // tile resource will be a bitmap.
    auto* backing = static_cast<TestSoftwareBacking*>(
        tile->draw_info().GetResource().software_backing());
    SkBitmap bitmap;
    bitmap.installPixels(info, backing->pixels.get(), info.minRowBytes());

    for (int x = 0; x < size.width(); ++x) {
      for (int y = 0; y < size.height(); ++y) {
        SCOPED_TRACE(y);
        SCOPED_TRACE(x);
        if (resolutions[i] == LOW_RESOLUTION) {
          // Since it's low res, the bitmap was not drawn, and the background
          // (green) is visible instead.
          ASSERT_EQ(SK_ColorGREEN, bitmap.getColor(x, y));
        } else {
          EXPECT_EQ(HIGH_RESOLUTION, resolutions[i]);
          // Since it's high res, the bitmap (blue) was drawn, and the
          // background is not visible.
          ASSERT_EQ(SK_ColorBLUE, bitmap.getColor(x, y));
        }
      }
    }
  }
}

class ActivationTasksDoNotBlockReadyToDrawTest : public TileManagerTest {
 protected:
  std::unique_ptr<TaskGraphRunner> CreateTaskGraphRunner() override {
    return std::make_unique<SynchronousTaskGraphRunner>();
  }

  std::unique_ptr<LayerTreeFrameSink> CreateLayerTreeFrameSink() override {
    return FakeLayerTreeFrameSink::Create3dForGpuRasterization();
  }
};

TEST_F(ActivationTasksDoNotBlockReadyToDrawTest,
       ActivationTasksDoNotBlockReadyToDraw) {
  const gfx::Size layer_bounds(1000, 1000);

  EXPECT_TRUE(host_impl()->use_gpu_rasterization());

  // Active tree has no non-solid tiles, so it will generate no tile tasks.
  FakeRecordingSource active_tree_recording_source(layer_bounds);

  PaintFlags solid_flags;
  SkColor solid_color = SkColorSetARGB(255, 12, 23, 34);
  solid_flags.setColor(solid_color);
  active_tree_recording_source.add_draw_rect_with_flags(gfx::Rect(layer_bounds),
                                                        solid_flags);

  active_tree_recording_source.Rerecord();

  // Pending tree has non-solid tiles, so it will generate tile tasks.
  FakeRecordingSource pending_tree_recording_source(layer_bounds);
  SkColor non_solid_color = SkColorSetARGB(128, 45, 56, 67);
  PaintFlags non_solid_flags;
  non_solid_flags.setColor(non_solid_color);

  pending_tree_recording_source.add_draw_rect_with_flags(
      gfx::Rect(5, 5, 10, 10), non_solid_flags);
  pending_tree_recording_source.Rerecord();

  scoped_refptr<RasterSource> active_tree_raster_source =
      active_tree_recording_source.CreateRasterSource();
  scoped_refptr<RasterSource> pending_tree_raster_source =
      pending_tree_recording_source.CreateRasterSource();

  SetupTrees(pending_tree_raster_source, active_tree_raster_source);
  host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());

  // The first task to run should be ReadyToDraw (this should not be blocked by
  // the tasks required for activation).
  base::RunLoop run_loop;
  EXPECT_CALL(MockHostImpl(), NotifyReadyToDraw())
      .WillOnce(testing::Invoke([&run_loop]() { run_loop.Quit(); }));
  static_cast<SynchronousTaskGraphRunner*>(task_graph_runner())
      ->RunSingleTaskForTesting();
  run_loop.Run();
}

class PartialRasterTileManagerTest : public TileManagerTest {
 public:
  LayerTreeSettings CreateSettings() override {
    auto settings = TileManagerTest::CreateSettings();
    settings.use_partial_raster = true;
    return settings;
  }
};

// Ensures that if a raster task is cancelled, it gets returned to the resource
// pool with an invalid content ID, not with its invalidated content ID.
TEST_F(PartialRasterTileManagerTest, CancelledTasksHaveNoContentId) {
  // Create a FakeTileTaskManagerImpl and set it on the tile manager so that all
  // scheduled work is immediately cancelled.

  host_impl()->tile_manager()->SetTileTaskManagerForTesting(
      std::make_unique<FakeTileTaskManagerImpl>());

  // Pick arbitrary IDs - they don't really matter as long as they're constant.
  const int kLayerId = 7;
  const uint64_t kInvalidatedId = 43;
  const gfx::Size kTileSize(128, 128);

  scoped_refptr<FakeRasterSource> pending_raster_source =
      FakeRasterSource::CreateFilled(kTileSize);
  host_impl()->CreatePendingTree();
  LayerTreeImpl* pending_tree = host_impl()->pending_tree();
  pending_tree->SetDeviceViewportRect(
      host_impl()->active_tree()->GetDeviceViewport());

  // Steal from the recycled tree.
  std::unique_ptr<FakePictureLayerImpl> pending_layer =
      FakePictureLayerImpl::Create(pending_tree, kLayerId,
                                   pending_raster_source);
  pending_layer->SetDrawsContent(true);

  // The bounds() just mirror the raster source size.
  pending_layer->SetBounds(pending_layer->raster_source()->size());
  SetupRootProperties(pending_layer.get());
  pending_tree->SetRootLayerForTesting(std::move(pending_layer));

  // Add tilings/tiles for the layer.
  UpdateDrawProperties(host_impl()->pending_tree());

  // Build the raster queue and invalidate the top tile. The lifetime of the
  // queue can't outlive |host_impl| or the queue will contain dangling
  // pointers so wrap it in a scope.
  {
    std::unique_ptr<RasterTilePriorityQueue> queue(
        host_impl()->BuildRasterQueue(SAME_PRIORITY_FOR_BOTH_TREES,
                                      RasterTilePriorityQueue::Type::ALL));
    EXPECT_FALSE(queue->IsEmpty());
    queue->Top().tile()->SetInvalidated(gfx::Rect(), kInvalidatedId);
  }

  // PrepareTiles to schedule tasks. Due to the FakeTileTaskManagerImpl,
  // these tasks will immediately be canceled.
  host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());

  // Make sure that the tile we invalidated above was not returned to the pool
  // with its invalidated resource ID.
  gfx::Rect total_invalidated_rect;
  EXPECT_FALSE(host_impl()->resource_pool()->TryAcquireResourceForPartialRaster(
      kInvalidatedId + 1, gfx::Rect(), kInvalidatedId, &total_invalidated_rect,
      gfx::ColorSpace::CreateSRGB()));
  EXPECT_EQ(gfx::Rect(), total_invalidated_rect);

  // Free our host_impl_ before the tile_task_manager we passed it, as it
  // will use that class in clean up.
  ClearLayersAndHost();
}

// FakeRasterBufferProviderImpl that verifies the resource content ID of raster
// tasks.
class VerifyResourceContentIdRasterBufferProvider
    : public FakeRasterBufferProviderImpl {
 public:
  explicit VerifyResourceContentIdRasterBufferProvider(
      uint64_t expected_content_id)
      : expected_content_id_(expected_content_id) {}
  ~VerifyResourceContentIdRasterBufferProvider() override = default;

  // RasterBufferProvider methods.
  std::unique_ptr<RasterBuffer> AcquireBufferForRaster(
      const ResourcePool::InUsePoolResource& resource,
      uint64_t resource_content_id,
      uint64_t previous_content_id,
      bool depends_on_at_raster_decodes,
      bool depends_on_hardware_accelerated_jpeg_candidates,
      bool depends_on_hardware_accelerated_webp_candidates) override {
    EXPECT_EQ(expected_content_id_, resource_content_id);
    return nullptr;
  }

 private:
  uint64_t expected_content_id_;
};

// Runs a test to ensure that partial raster is either enabled or disabled,
// depending on |partial_raster_enabled|'s value. Takes ownership of host_impl
// so that cleanup order can be controlled.
void RunPartialRasterCheck(std::unique_ptr<LayerTreeHostImpl> host_impl,
                           bool partial_raster_enabled) {
  // Pick arbitrary IDs - they don't really matter as long as they're constant.
  const int kLayerId = 7;
  const uint64_t kInvalidatedId = 43;
  const uint64_t kExpectedId = partial_raster_enabled ? kInvalidatedId : 0u;
  const gfx::Size kTileSize(128, 128);

  // Create a VerifyResourceContentIdTileTaskManager to ensure that the
  // raster task we see is created with |kExpectedId|.
  host_impl->tile_manager()->SetTileTaskManagerForTesting(
      std::make_unique<FakeTileTaskManagerImpl>());

  VerifyResourceContentIdRasterBufferProvider raster_buffer_provider(
      kExpectedId);
  host_impl->tile_manager()->SetRasterBufferProviderForTesting(
      &raster_buffer_provider);

  // Ensure there's a resource with our |kInvalidatedId| in the resource pool.
  ResourcePool::InUsePoolResource resource =
      host_impl->resource_pool()->AcquireResource(
          kTileSize, viz::SinglePlaneFormat::kRGBA_8888,
          gfx::ColorSpace::CreateSRGB());

  resource.set_software_backing(std::make_unique<TestSoftwareBacking>());
  host_impl->resource_pool()->PrepareForExport(
      resource, viz::TransferableResource::ResourceSource::kTest);

  host_impl->resource_pool()->OnContentReplaced(resource, kInvalidatedId);
  host_impl->resource_pool()->ReleaseResource(std::move(resource));

  scoped_refptr<FakeRasterSource> pending_raster_source =
      FakeRasterSource::CreateFilled(kTileSize);
  host_impl->CreatePendingTree();
  LayerTreeImpl* pending_tree = host_impl->pending_tree();
  pending_tree->SetDeviceViewportRect(
      host_impl->active_tree()->GetDeviceViewport());

  std::unique_ptr<FakePictureLayerImpl> pending_layer =
      FakePictureLayerImpl::Create(pending_tree, kLayerId,
                                   pending_raster_source);
  pending_layer->SetDrawsContent(true);

  // The bounds() just mirror the raster source size.
  pending_layer->SetBounds(pending_layer->raster_source()->size());
  SetupRootProperties(pending_layer.get());
  pending_tree->SetRootLayerForTesting(std::move(pending_layer));

  // Add tilings/tiles for the layer.
  UpdateDrawProperties(pending_tree);

  // Build the raster queue and invalidate the top tile. The lifetime of the
  // queue can't outlive |host_impl| or the queue will contain dangling
  // pointers so wrap it in a scope.
  {
    std::unique_ptr<RasterTilePriorityQueue> queue(host_impl->BuildRasterQueue(
        SAME_PRIORITY_FOR_BOTH_TREES, RasterTilePriorityQueue::Type::ALL));
    EXPECT_FALSE(queue->IsEmpty());
    queue->Top().tile()->SetInvalidated(gfx::Rect(), kInvalidatedId);
  }

  // PrepareTiles to schedule tasks. Due to the
  // VerifyPreviousContentRasterBufferProvider, these tasks will verified and
  // cancelled.
  host_impl->tile_manager()->PrepareTiles(host_impl->global_tile_state());

  // Free our host_impl before the verifying_task_manager we passed it, as it
  // will use that class in clean up.
  host_impl = nullptr;
}

void RunPartialTileDecodeCheck(std::unique_ptr<LayerTreeHostImpl> host_impl,
                               bool partial_raster_enabled) {
  // Pick arbitrary IDs - they don't really matter as long as they're constant.
  const int kLayerId = 7;
  const uint64_t kInvalidatedId = 43;
  const uint64_t kExpectedId = partial_raster_enabled ? kInvalidatedId : 0u;
  const gfx::Size kTileSize(400, 400);

  host_impl->tile_manager()->SetTileTaskManagerForTesting(
      std::make_unique<FakeTileTaskManagerImpl>());

  // Create a VerifyResourceContentIdTileTaskManager to ensure that the
  // raster task we see is created with |kExpectedId|.
  VerifyResourceContentIdRasterBufferProvider raster_buffer_provider(
      kExpectedId);
  host_impl->tile_manager()->SetRasterBufferProviderForTesting(
      &raster_buffer_provider);

  // Ensure there's a resource with our |kInvalidatedId| in the resource pool.
  ResourcePool::InUsePoolResource resource =
      host_impl->resource_pool()->AcquireResource(
          kTileSize, viz::SinglePlaneFormat::kRGBA_8888,
          gfx::ColorSpace::CreateSRGB());
  host_impl->resource_pool()->OnContentReplaced(resource, kInvalidatedId);
  host_impl->resource_pool()->ReleaseResource(std::move(resource));

  const gfx::Size layer_bounds(500, 500);

  FakeRecordingSource recording_source(layer_bounds);
  recording_source.set_fill_with_nonsolid_color(true);

  int dimension = 250;
  PaintImage image1 =
      CreateDiscardablePaintImage(gfx::Size(dimension, dimension));
  PaintImage image2 =
      CreateDiscardablePaintImage(gfx::Size(dimension, dimension));
  PaintImage image3 =
      CreateDiscardablePaintImage(gfx::Size(dimension, dimension));
  PaintImage image4 =
      CreateDiscardablePaintImage(gfx::Size(dimension, dimension));
  recording_source.add_draw_image(image1, gfx::Point(0, 0));
  recording_source.add_draw_image(image2, gfx::Point(300, 0));
  recording_source.add_draw_image(image3, gfx::Point(0, 300));
  recording_source.add_draw_image(image4, gfx::Point(300, 300));

  recording_source.Rerecord();

  scoped_refptr<FakeRasterSource> pending_raster_source =
      FakeRasterSource::CreateFromRecordingSource(recording_source);

  host_impl->CreatePendingTree();
  LayerTreeImpl* pending_tree = host_impl->pending_tree();
  pending_tree->SetDeviceViewportRect(
      host_impl->active_tree()->GetDeviceViewport());

  // Steal from the recycled tree.
  std::unique_ptr<FakePictureLayerImpl> pending_layer =
      FakePictureLayerImpl::Create(pending_tree, kLayerId,
                                   pending_raster_source);
  pending_layer->SetDrawsContent(true);

  // The bounds() just mirror the raster source size.
  pending_layer->SetBounds(pending_layer->raster_source()->size());
  SetupRootProperties(pending_layer.get());
  pending_tree->SetRootLayerForTesting(std::move(pending_layer));

  // Add tilings/tiles for the layer.
  UpdateDrawProperties(pending_tree);

  // Build the raster queue and invalidate the top tile if partial raster is
  // enabled. The lifetime of the queue can't outlive |host_impl| or the
  // queue will contain dangling pointers, so wrap it in a scope.
  {
    std::unique_ptr<RasterTilePriorityQueue> queue(host_impl->BuildRasterQueue(
        SAME_PRIORITY_FOR_BOTH_TREES, RasterTilePriorityQueue::Type::ALL));
    ASSERT_FALSE(queue->IsEmpty());
    Tile* tile = queue->Top().tile();
    if (partial_raster_enabled) {
      tile->SetInvalidated(gfx::Rect(200, 200), kInvalidatedId);
    }

    // PrepareTiles to schedule tasks. Due to the
    // VerifyPreviousContentRasterBufferProvider, these tasks will verified and
    // cancelled.
    host_impl->tile_manager()->PrepareTiles(host_impl->global_tile_state());

    // Tile will have 1 dependent decode task if we decode images only in the
    // invalidated rect. Otherwise it will have 4.
    EXPECT_EQ(
        host_impl->tile_manager()->decode_tasks_for_testing(tile->id()).size(),
        partial_raster_enabled ? 1u : 4u);
  }

  // Free our host_impl before the verifying_task_manager we passed it, as it
  // will use that class in clean up.
  host_impl = nullptr;
}

// Ensures that the tile manager successfully reuses tiles when partial
// raster is enabled.
TEST_F(PartialRasterTileManagerTest, PartialRasterSuccessfullyEnabled) {
  RunPartialRasterCheck(TakeHostImpl(), true /* partial_raster_enabled */);
}

TEST_F(PartialRasterTileManagerTest, PartialTileImageDecode) {
  RunPartialTileDecodeCheck(TakeHostImpl(), true /* partial_raster_enabled */);
}

TEST_F(PartialRasterTileManagerTest, CompleteTileImageDecode) {
  RunPartialTileDecodeCheck(TakeHostImpl(),
                            false /* partial_raster_disabled */);
}

// Ensures that the tile manager does not attempt to reuse tiles when partial
// raster is disabled.
TEST_F(TileManagerTest, PartialRasterSuccessfullyDisabled) {
  RunPartialRasterCheck(TakeHostImpl(), false /* partial_raster_enabled */);
}

class InvalidResourceRasterBufferProvider
    : public FakeRasterBufferProviderImpl {
 public:
  std::unique_ptr<RasterBuffer> AcquireBufferForRaster(
      const ResourcePool::InUsePoolResource& resource,
      uint64_t resource_content_id,
      uint64_t previous_content_id,
      bool depends_on_at_raster_decodes,
      bool depends_on_hardware_accelerated_jpeg_candidates,
      bool depends_on_hardware_accelerated_webp_candidates) override {
    if (!resource.gpu_backing()) {
      auto backing = std::make_unique<StubGpuBacking>();
      // Don't set a mailbox to signal invalid resource.
      resource.set_gpu_backing(std::move(backing));
    }
    return std::make_unique<FakeRasterBuffer>();
  }

 private:
  class StubGpuBacking : public ResourcePool::GpuBacking {
   public:
    void OnMemoryDump(
        base::trace_event::ProcessMemoryDump* pmd,
        const base::trace_event::MemoryAllocatorDumpGuid& buffer_dump_guid,
        uint64_t tracing_process_id,
        int importance) const override {}
  };
};

class InvalidResourceTileManagerTest : public TileManagerTest {
 protected:
  std::unique_ptr<LayerTreeFrameSink> CreateLayerTreeFrameSink() override {
    return FakeLayerTreeFrameSink::Create3d();
  }
};

TEST_F(InvalidResourceTileManagerTest, InvalidResource) {
  auto* tile_manager = host_impl()->tile_manager();
  InvalidResourceRasterBufferProvider raster_buffer_provider;
  tile_manager->SetRasterBufferProviderForTesting(&raster_buffer_provider);

  gfx::Size size(10, 12);
  FakePictureLayerTilingClient tiling_client;
  tiling_client.SetTileSize(size);

  auto raster = FakeRasterSource::CreateFilled(size);
  SetupTrees(raster, raster);
  active_layer()->set_contributes_to_drawn_render_surface(true);

  active_layer()->picture_layer_tiling_set()->RemoveAllTilings();
  auto* tiling = active_layer()->picture_layer_tiling_set()->AddTiling(
      gfx::AxisTransform2d(), std::move(raster));
  tiling->set_resolution(HIGH_RESOLUTION);
  tiling->CreateAllTilesForTesting(gfx::Rect(size));

  base::RunLoop run_loop;
  EXPECT_CALL(MockHostImpl(), NotifyAllTileTasksCompleted())
      .WillOnce(testing::Invoke([&run_loop]() { run_loop.Quit(); }));
  tile_manager->PrepareTiles(host_impl()->global_tile_state());
  run_loop.Run();
  tile_manager->PrepareToDraw();

  Tile* tile = tiling->TileAt(0, 0);
  ASSERT_TRUE(tile);
  // The tile in the tiling was rastered, but didn't get a resource.
  EXPECT_TRUE(tile->draw_info().IsReadyToDraw());
  EXPECT_EQ(TileDrawInfo::OOM_MODE, tile->draw_info().mode());

  // Ensure that the host impl doesn't outlive |raster_buffer_provider|.
  ClearLayersAndHost();
}

// FakeRasterBufferProviderImpl that allows us to mock ready to draw
// functionality.
class MockReadyToDrawRasterBufferProviderImpl
    : public FakeRasterBufferProviderImpl {
 public:
  MOCK_METHOD1(IsResourceReadyToDraw,
               bool(const ResourcePool::InUsePoolResource& resource));
  MOCK_METHOD3(
      SetReadyToDrawCallback,
      uint64_t(
          const std::vector<const ResourcePool::InUsePoolResource*>& resources,
          base::OnceClosure callback,
          uint64_t pending_callback_id));

  void set_expected_hdr_headroom(float expected_hdr_headroom) {
    expected_hdr_headroom_ = expected_hdr_headroom;
  }

  std::unique_ptr<RasterBuffer> AcquireBufferForRaster(
      const ResourcePool::InUsePoolResource& resource,
      uint64_t resource_content_id,
      uint64_t previous_content_id,
      bool depends_on_at_raster_decodes,
      bool depends_on_hardware_accelerated_jpeg_candidates,
      bool depends_on_hardware_accelerated_webp_candidates) override {
    if (!resource.software_backing())
      resource.set_software_backing(std::make_unique<TestSoftwareBacking>());
    return std::make_unique<FakeRasterBuffer>(expected_hdr_headroom_);
  }

 private:
  float expected_hdr_headroom_ = 1.f;
};

class TileManagerReadyToDrawTest : public TileManagerTest {
 public:
  void SetUp() override {
    TileManagerTest::SetUp();
    host_impl()->tile_manager()->SetRasterBufferProviderForTesting(
        &mock_raster_buffer_provider_);

    PaintFlags solid_flags;
    SkColor solid_color = SkColorSetARGB(255, 12, 23, 34);
    solid_flags.setColor(solid_color);
    solid_color_recording_source_.add_draw_rect_with_flags(
        gfx::Rect(kLayerBounds), solid_flags);

    solid_color_recording_source_.Rerecord();

    SkColor non_solid_color = SkColorSetARGB(128, 45, 56, 67);
    PaintFlags non_solid_flags;
    non_solid_flags.setColor(non_solid_color);

    for (int i = 0; i < 100; ++i) {
      for (int j = 0; j < 100; ++j) {
        recording_source_.add_draw_rect_with_flags(
            gfx::Rect(10 * i, 10 * j, 5, 5), non_solid_flags);
      }
    }
    recording_source_.Rerecord();
  }

  void SetupTreesWithActiveTreeTiles() {
    scoped_refptr<RasterSource> active_tree_raster_source =
        recording_source_.CreateRasterSource();
    scoped_refptr<RasterSource> pending_tree_raster_source =
        solid_color_recording_source_.CreateRasterSource();

    SetupTrees(pending_tree_raster_source, active_tree_raster_source);
  }

  void SetupTreesWithPendingTreeTiles() {
    scoped_refptr<RasterSource> active_tree_raster_source =
        solid_color_recording_source_.CreateRasterSource();
    scoped_refptr<RasterSource> pending_tree_raster_source =
        recording_source_.CreateRasterSource();

    SetupTrees(pending_tree_raster_source, active_tree_raster_source);
  }

  TileManager* tile_manager() { return host_impl()->tile_manager(); }
  MockReadyToDrawRasterBufferProviderImpl* mock_raster_buffer_provider() {
    return &mock_raster_buffer_provider_;
  }

 private:
  StrictMock<MockReadyToDrawRasterBufferProviderImpl>
      mock_raster_buffer_provider_;
  const gfx::Size kLayerBounds{1000, 1000};
  FakeRecordingSource recording_source_{kLayerBounds};
  FakeRecordingSource solid_color_recording_source_{kLayerBounds};
};

TEST_F(TileManagerReadyToDrawTest, SmoothActivationWaitsOnCallback) {
  host_impl()->SetTreePriority(SMOOTHNESS_TAKES_PRIORITY);
  SetupTreesWithPendingTreeTiles();

  base::OnceClosure callback;
  {
    base::RunLoop run_loop;

    // Until we activate our ready to draw callback, treat all resources as not
    // ready to draw.
    EXPECT_CALL(*mock_raster_buffer_provider(),
                IsResourceReadyToDraw(testing::_))
        .WillRepeatedly(Return(false));

    EXPECT_CALL(*mock_raster_buffer_provider(), SetReadyToDrawCallback(_, _, 0))
        .WillOnce([&run_loop, &callback](
                      const std::vector<const ResourcePool::InUsePoolResource*>&
                          resources,
                      base::OnceClosure callback_in,
                      uint64_t pending_callback_id) {
          callback = std::move(callback_in);
          run_loop.Quit();
          return 1;
        });
    host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());
    run_loop.Run();
  }

  EXPECT_TRUE(host_impl()->tile_manager()->IsReadyToDraw());
  EXPECT_FALSE(host_impl()->tile_manager()->IsReadyToActivate());

  {
    base::RunLoop run_loop;
    EXPECT_CALL(MockHostImpl(), NotifyReadyToActivate())
        .WillOnce(Invoke([&run_loop]() { run_loop.Quit(); }));
    EXPECT_CALL(*mock_raster_buffer_provider(),
                IsResourceReadyToDraw(testing::_))
        .WillRepeatedly(Return(true));
    std::move(callback).Run();
    run_loop.Run();
  }

  EXPECT_TRUE(host_impl()->tile_manager()->IsReadyToDraw());
  EXPECT_TRUE(host_impl()->tile_manager()->IsReadyToActivate());
}

TEST_F(TileManagerReadyToDrawTest, NonSmoothActivationDoesNotWaitOnCallback) {
  SetupTreesWithPendingTreeTiles();

  // We're using a StrictMock on the RasterBufferProvider, so any function call
  // will cause a test failure.
  base::RunLoop run_loop;

  host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());
  EXPECT_CALL(MockHostImpl(), NotifyReadyToActivate())
      .WillOnce(Invoke([&run_loop]() { run_loop.Quit(); }));
  run_loop.Run();

  EXPECT_TRUE(host_impl()->tile_manager()->IsReadyToDraw());
  EXPECT_TRUE(host_impl()->tile_manager()->IsReadyToActivate());
}

TEST_F(TileManagerReadyToDrawTest, HdrHeadroomPropagated) {
  constexpr float kTestHdrHeadroom = 4.f;
  TargetColorParams target_color_params;
  target_color_params.hdr_max_luminance_relative = kTestHdrHeadroom;
  host_impl()->set_target_color_params(target_color_params);
  mock_raster_buffer_provider()->set_expected_hdr_headroom(kTestHdrHeadroom);

  SetupTreesWithPendingTreeTiles();
  base::RunLoop run_loop;

  host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());
  EXPECT_CALL(MockHostImpl(), NotifyReadyToActivate())
      .WillOnce(Invoke([&run_loop]() { run_loop.Quit(); }));
  run_loop.Run();

  EXPECT_TRUE(host_impl()->tile_manager()->IsReadyToDraw());
  EXPECT_TRUE(host_impl()->tile_manager()->IsReadyToActivate());
}

TEST_F(TileManagerReadyToDrawTest, SmoothDrawWaitsOnCallback) {
  host_impl()->SetTreePriority(SMOOTHNESS_TAKES_PRIORITY);
  SetupTreesWithActiveTreeTiles();

  base::OnceClosure callback;
  {
    base::RunLoop run_loop;

    // Until we activate our ready to draw callback, treat all resources as not
    // ready to draw.
    EXPECT_CALL(*mock_raster_buffer_provider(),
                IsResourceReadyToDraw(testing::_))
        .WillRepeatedly(Return(false));

    EXPECT_CALL(*mock_raster_buffer_provider(), SetReadyToDrawCallback(_, _, 0))
        .WillOnce([&run_loop, &callback](
                      const std::vector<const ResourcePool::InUsePoolResource*>&
                          resources,
                      base::OnceClosure callback_in,
                      uint64_t pending_callback_id) {
          callback = std::move(callback_in);
          run_loop.Quit();
          return 1;
        });
    host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());
    run_loop.Run();
  }

  EXPECT_FALSE(host_impl()->tile_manager()->IsReadyToDraw());
  EXPECT_TRUE(host_impl()->tile_manager()->IsReadyToActivate());

  {
    base::RunLoop run_loop;
    EXPECT_CALL(MockHostImpl(), NotifyReadyToDraw())
        .WillOnce(testing::Invoke([&run_loop]() { run_loop.Quit(); }));
    EXPECT_CALL(*mock_raster_buffer_provider(),
                IsResourceReadyToDraw(testing::_))
        .WillRepeatedly(Return(true));
    std::move(callback).Run();
    run_loop.Run();
  }

  EXPECT_TRUE(host_impl()->tile_manager()->IsReadyToDraw());
  EXPECT_TRUE(host_impl()->tile_manager()->IsReadyToActivate());
}

TEST_F(TileManagerReadyToDrawTest, NonSmoothDrawDoesNotWaitOnCallback) {
  SetupTreesWithActiveTreeTiles();

  // We're using a StrictMock on the RasterBufferProvider, so any function call
  // will cause a test failure.
  base::RunLoop run_loop;

  host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());
  EXPECT_CALL(MockHostImpl(), NotifyReadyToDraw())
      .WillOnce(Invoke([&run_loop]() { run_loop.Quit(); }));
  run_loop.Run();

  EXPECT_TRUE(host_impl()->tile_manager()->IsReadyToDraw());
  EXPECT_TRUE(host_impl()->tile_manager()->IsReadyToActivate());
}

TEST_F(TileManagerReadyToDrawTest, NoCallbackWhenAlreadyReadyToDraw) {
  host_impl()->SetTreePriority(SMOOTHNESS_TAKES_PRIORITY);
  SetupTreesWithPendingTreeTiles();

  base::RunLoop run_loop;
  host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());
  EXPECT_CALL(MockHostImpl(), NotifyReadyToActivate())
      .WillOnce(Invoke([&run_loop]() { run_loop.Quit(); }));
  EXPECT_CALL(*mock_raster_buffer_provider(), IsResourceReadyToDraw(_))
      .WillRepeatedly(Return(true));
  run_loop.Run();

  EXPECT_TRUE(host_impl()->tile_manager()->IsReadyToDraw());
  EXPECT_TRUE(host_impl()->tile_manager()->IsReadyToActivate());
}

TEST_F(TileManagerReadyToDrawTest, TilePrioritiesUpdated) {
  // Use smoothness as that's a mode in which we wait on resources to be
  // ready instead of marking them ready immediately.
  host_impl()->SetTreePriority(SMOOTHNESS_TAKES_PRIORITY);
  gfx::Size very_small(1, 1);
  host_impl()->active_tree()->SetDeviceViewportRect(gfx::Rect(very_small));

  gfx::Size layer_bounds(1000, 1000);
  SetupDefaultTrees(layer_bounds);

  // Run until all tile tasks are complete, but don't let any draw callbacks
  // finish.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(MockHostImpl(), NotifyAllTileTasksCompleted())
        .WillOnce(testing::Invoke([&run_loop]() { run_loop.Quit(); }));

    // Until we activate our ready to draw callback, treat all resources as not
    // ready to draw.
    EXPECT_CALL(*mock_raster_buffer_provider(),
                IsResourceReadyToDraw(testing::_))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*mock_raster_buffer_provider(), SetReadyToDrawCallback(_, _, _))
        .WillRepeatedly(Return(1));
    host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());
    run_loop.Run();
  }

  // Inspect the current state of tiles in this world of cpu done but gpu
  // not ready yet.
  size_t orig_num_required = 0;
  size_t orig_num_prepaint = 0;
  std::vector<Tile*> prepaint_tiles;
  for (auto* tile : host_impl()->tile_manager()->AllTilesForTesting()) {
    if (tile->draw_info().has_resource()) {
      if (tile->is_prepaint()) {
        orig_num_prepaint++;
        prepaint_tiles.push_back(tile);
      } else {
        orig_num_required++;
      }
    }
  }

  // Verify that there exist some prepaint tiles here.
  EXPECT_GT(orig_num_prepaint, 0u);
  EXPECT_GT(orig_num_required, 0u);

  host_impl()->active_tree()->SetDeviceViewportRect(gfx::Rect(layer_bounds));
  UpdateDrawProperties(host_impl()->active_tree());
  UpdateDrawProperties(host_impl()->pending_tree());

  // Rerun prepare tiles.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(MockHostImpl(), NotifyAllTileTasksCompleted())
        .WillOnce(testing::Invoke([&run_loop]() { run_loop.Quit(); }));
    host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());
    run_loop.Run();
  }

  // Make sure tiles priorities are updated.
  size_t final_num_required = 0;
  size_t final_num_prepaint = 0;
  bool found_one_prepaint_to_required_transition = false;
  for (auto* tile : host_impl()->tile_manager()->AllTilesForTesting()) {
    if (tile->draw_info().has_resource()) {
      if (tile->is_prepaint()) {
        final_num_prepaint++;
      } else {
        final_num_required++;
        if (base::Contains(prepaint_tiles, tile)) {
          found_one_prepaint_to_required_transition = true;
        }
      }
    }
  }

  // Tile priorities should be updated and we should have more required
  // and fewer prepaint now that the viewport has changed.
  EXPECT_GT(final_num_required, orig_num_required);
  EXPECT_LT(final_num_prepaint, orig_num_prepaint);
  EXPECT_TRUE(found_one_prepaint_to_required_transition);
}

TEST_F(TileManagerReadyToDrawTest, PrepaintTilesAreDroppedWhenIdle) {
  auto count_prepaint_tiles = [](const std::vector<Tile*>& tiles) {
    int count = 0;
    for (auto* tile : tiles) {
      if (tile->draw_info().has_resource() && tile->is_prepaint()) {
        count++;
      }
    }
    return count;
  };

  base::test::ScopedFeatureList scoped_feature_list{
      features::kReclaimPrepaintTilesWhenIdle};

  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  host_impl()->tile_manager()->SetOverridesForTesting(
      task_runner, task_runner->GetMockTickClock());

  gfx::Size very_small(1, 1);
  host_impl()->active_tree()->SetDeviceViewportRect(gfx::Rect(very_small));

  gfx::Size layer_bounds(1000, 1000);
  SetupDefaultTrees(layer_bounds);

  base::RunLoop run_loop;
  host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());
  EXPECT_CALL(MockHostImpl(), NotifyAllTileTasksCompleted())
      .WillOnce(Invoke([&run_loop]() { run_loop.Quit(); }));
  run_loop.Run();

  int prepaint_tiles =
      count_prepaint_tiles(host_impl()->tile_manager()->AllTilesForTesting());
  EXPECT_GT(prepaint_tiles, 0);
  EXPECT_TRUE(task_runner->HasPendingTask());

  // Too soon.
  task_runner->FastForwardBy(TileManager::kDelayBeforeTimeReclaim / 2);
  prepaint_tiles =
      count_prepaint_tiles(host_impl()->tile_manager()->AllTilesForTesting());
  EXPECT_GT(prepaint_tiles, 0);
  EXPECT_TRUE(task_runner->HasPendingTask());

  task_runner->FastForwardBy(TileManager::kDelayBeforeTimeReclaim / 2);

  // Prepaint tiles are dropped.
  prepaint_tiles =
      count_prepaint_tiles(host_impl()->tile_manager()->AllTilesForTesting());
  EXPECT_EQ(prepaint_tiles, 0);
  // Don't repost a task.
  EXPECT_FALSE(task_runner->HasPendingTask());
  host_impl()->tile_manager()->SetOverridesForTesting(nullptr, nullptr);
}

TEST_F(TileManagerReadyToDrawTest, PrepaintTilesAreNotDropped) {
  auto count_prepaint_tiles = [](const std::vector<Tile*>& tiles) {
    int count = 0;
    for (auto* tile : tiles) {
      if (tile->draw_info().has_resource() && tile->is_prepaint()) {
        count++;
      }
    }
    return count;
  };

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kReclaimPrepaintTilesWhenIdle);

  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  host_impl()->tile_manager()->SetOverridesForTesting(
      task_runner, task_runner->GetMockTickClock());

  gfx::Size very_small(1, 1);
  host_impl()->active_tree()->SetDeviceViewportRect(gfx::Rect(very_small));

  gfx::Size layer_bounds(1000, 1000);
  SetupDefaultTrees(layer_bounds);

  base::RunLoop run_loop;
  host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());
  EXPECT_CALL(MockHostImpl(), NotifyAllTileTasksCompleted())
      .WillOnce(Invoke([&run_loop]() { run_loop.Quit(); }));
  run_loop.Run();

  int before_prepaint_tiles =
      count_prepaint_tiles(host_impl()->tile_manager()->AllTilesForTesting());
  ASSERT_GT(before_prepaint_tiles, 0);

  // No release task.
  EXPECT_FALSE(task_runner->HasPendingTask());
  host_impl()->tile_manager()->SetOverridesForTesting(nullptr, nullptr);
}

TEST_F(TileManagerReadyToDrawTest, PrepaintTilesContinuousIdleTime) {
  auto count_prepaint_tiles = [](const std::vector<Tile*>& tiles) {
    int count = 0;
    for (auto* tile : tiles) {
      if (tile->draw_info().has_resource() && tile->is_prepaint()) {
        count++;
      }
    }
    return count;
  };

  auto prepare_tiles = [this]() {
    base::RunLoop run_loop;
    host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());
    EXPECT_CALL(MockHostImpl(), NotifyAllTileTasksCompleted())
        .WillOnce(Invoke([&run_loop]() { run_loop.Quit(); }));
    run_loop.Run();
  };

  base::test::ScopedFeatureList scoped_feature_list{
      features::kReclaimPrepaintTilesWhenIdle};

  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  host_impl()->tile_manager()->SetOverridesForTesting(
      task_runner, task_runner->GetMockTickClock());

  gfx::Size very_small(1, 1);
  host_impl()->active_tree()->SetDeviceViewportRect(gfx::Rect(very_small));

  gfx::Size layer_bounds(1000, 1000);
  SetupDefaultTrees(layer_bounds);

  prepare_tiles();
  int before_prepaint_tiles =
      count_prepaint_tiles(host_impl()->tile_manager()->AllTilesForTesting());
  ASSERT_GT(before_prepaint_tiles, 0);

  EXPECT_TRUE(task_runner->HasPendingTask());
  // Not enough continuous idle time.
  task_runner->FastForwardBy(TileManager::kDelayBeforeTimeReclaim / 2);
  prepare_tiles();
  task_runner->FastForwardBy(TileManager::kDelayBeforeTimeReclaim / 2);
  // A task has been re-posted.
  EXPECT_TRUE(task_runner->HasPendingTask());

  int after_prepaint_tiles =
      count_prepaint_tiles(host_impl()->tile_manager()->AllTilesForTesting());
  EXPECT_EQ(after_prepaint_tiles, before_prepaint_tiles);

  task_runner->FastForwardBy(TileManager::kDelayBeforeTimeReclaim / 2);
  EXPECT_EQ(0, count_prepaint_tiles(
                   host_impl()->tile_manager()->AllTilesForTesting()));
  // No task is posted when there is nothing to do.
  EXPECT_FALSE(task_runner->HasPendingTask());

  host_impl()->tile_manager()->SetOverridesForTesting(nullptr, nullptr);
}

void UpdateVisibleRect(FakePictureLayerImpl* layer,
                       const gfx::Rect visible_rect) {
  PictureLayerTilingSet* tiling_set = layer->tilings();
  for (size_t j = 0; j < tiling_set->num_tilings(); ++j) {
    PictureLayerTiling* tiling = tiling_set->tiling_at(j);
    tiling->SetTilePriorityRectsForTesting(
        visible_rect,                  // Visible rect.
        visible_rect,                  // Skewport rect.
        visible_rect,                  // Soon rect.
        gfx::Rect(0, 0, 1000, 1000));  // Eventually rect.
  }
}

TEST_F(TileManagerReadyToDrawTest, ReadyToDrawRespectsRequirementChange) {
  host_impl()->SetTreePriority(SMOOTHNESS_TAKES_PRIORITY);
  SetupTreesWithPendingTreeTiles();

  // Initially create a tiling with a visible rect of (0, 0, 100, 100) and
  // a soon rect of the rest of the layer.
  UpdateVisibleRect(pending_layer(), gfx::Rect(0, 0, 100, 100));

  // Mark all these tiles as ready to draw.
  {
    base::RunLoop run_loop;
    host_impl()->tile_manager()->DidModifyTilePriorities();
    host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());
    EXPECT_CALL(MockHostImpl(), NotifyReadyToActivate())
        .WillOnce(Invoke([&run_loop]() { run_loop.Quit(); }));
    EXPECT_CALL(*mock_raster_buffer_provider(), IsResourceReadyToDraw(_))
        .WillRepeatedly(Return(true));
    run_loop.Run();
  }

  EXPECT_TRUE(host_impl()->tile_manager()->IsReadyToDraw());
  EXPECT_TRUE(host_impl()->tile_manager()->IsReadyToActivate());

  // Move the viewport to (900, 900, 100, 100), so that we need a different set
  // of tilings.
  UpdateVisibleRect(pending_layer(), gfx::Rect(900, 900, 100, 100));

  EXPECT_CALL(*mock_raster_buffer_provider(), IsResourceReadyToDraw(testing::_))
      .WillRepeatedly(Return(false));

  base::OnceClosure callback;
  {
    base::RunLoop run_loop;

    EXPECT_CALL(*mock_raster_buffer_provider(), SetReadyToDrawCallback(_, _, 0))
        .WillOnce([&run_loop, &callback](
                      const std::vector<const ResourcePool::InUsePoolResource*>&
                          resources,
                      base::OnceClosure callback_in,
                      uint64_t pending_callback_id) {
          callback = std::move(callback_in);
          run_loop.Quit();
          return 1;
        });
    host_impl()->tile_manager()->DidModifyTilePriorities();
    host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());
    run_loop.Run();
  }

  EXPECT_TRUE(host_impl()->tile_manager()->IsReadyToDraw());
  EXPECT_FALSE(host_impl()->tile_manager()->IsReadyToActivate());

  // Now switch back to our original tiling. We should be immediately able to
  // activate, as we still have the original tile, and no longer need the
  // tiles from the previous callback.
  UpdateVisibleRect(pending_layer(), gfx::Rect(0, 0, 100, 100));

  {
    base::RunLoop run_loop;
    host_impl()->tile_manager()->DidModifyTilePriorities();
    host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());
    EXPECT_CALL(MockHostImpl(), NotifyReadyToActivate())
        .WillOnce(Invoke([&run_loop]() { run_loop.Quit(); }));
    run_loop.Run();
  }

  EXPECT_TRUE(host_impl()->tile_manager()->IsReadyToDraw());
  EXPECT_TRUE(host_impl()->tile_manager()->IsReadyToActivate());
}

TEST_F(TileManagerReadyToDrawTest, SetBackIsLikelyToRequireADrawToFalse) {
  const gfx::Size layer_bounds(1000, 1000);
  host_impl()->active_tree()->SetDeviceViewportRect(gfx::Rect(layer_bounds));
  SetupDefaultTrees(layer_bounds);

  // Verify that the queue has a required for draw tile
  std::unique_ptr<RasterTilePriorityQueue> queue(host_impl()->BuildRasterQueue(
      SAME_PRIORITY_FOR_BOTH_TREES, RasterTilePriorityQueue::Type::ALL));
  EXPECT_FALSE(queue->IsEmpty());
  EXPECT_TRUE(queue->Top().tile()->required_for_draw());
  EXPECT_FALSE(host_impl()->is_likely_to_require_a_draw());

  // Mark all these tiles as ready to draw.
  {
    base::RunLoop run_loop;
    host_impl()->tile_manager()->DidModifyTilePriorities();
    host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());
    EXPECT_CALL(MockHostImpl(), NotifyReadyToActivate())
        .WillOnce(Invoke([&run_loop]() { run_loop.Quit(); }));
    EXPECT_CALL(*mock_raster_buffer_provider(), IsResourceReadyToDraw(_))
        .WillRepeatedly(Return(true));
    run_loop.Run();
  }

  EXPECT_TRUE(host_impl()->is_likely_to_require_a_draw());

  // Check is_likely_to_require_a_draw returns FALSE after PrepareToDraw.
  EXPECT_TRUE(host_impl()->tile_manager()->IsReadyToDraw());
  host_impl()->tile_manager()->PrepareToDraw();
  EXPECT_FALSE(host_impl()->is_likely_to_require_a_draw());
}

PaintImage MakeCheckerablePaintImage(const gfx::Size& size) {
  auto image = CreateDiscardablePaintImage(size);
  return PaintImageBuilder::WithCopy(image)
      .set_decoding_mode(PaintImage::DecodingMode::kAsync)
      .TakePaintImage();
}

class CheckerImagingTileManagerTest : public TestLayerTreeHostBase {
 public:
  class MockImageGenerator : public FakePaintImageGenerator {
   public:
    explicit MockImageGenerator(const gfx::Size& size)
        : FakePaintImageGenerator(
              SkImageInfo::MakeN32Premul(size.width(), size.height())) {}

    MOCK_METHOD4(
        GetPixels,
        bool(SkPixmap, size_t, PaintImage::GeneratorClientId, uint32_t));
  };

  void TearDown() override {
    // Allow all tasks on the image worker to run now. Any scheduled decodes
    // will be aborted.
    task_runner_->set_run_tasks_synchronously(true);
  }

  LayerTreeSettings CreateSettings() override {
    auto settings = TestLayerTreeHostBase::CreateSettings();
    settings.enable_checker_imaging = true;
    settings.min_image_bytes_to_checker = 512 * 1024;
    return settings;
  }

  std::unique_ptr<FakeLayerTreeHostImpl> CreateHostImpl(
      const LayerTreeSettings& settings,
      TaskRunnerProvider* task_runner_provider,
      TaskGraphRunner* task_graph_runner) override {
    task_runner_ = base::MakeRefCounted<SynchronousSimpleTaskRunner>();
    return std::make_unique<FakeLayerTreeHostImpl>(
        settings, task_runner_provider, task_graph_runner, task_runner_);
  }

  std::unique_ptr<TaskGraphRunner> CreateTaskGraphRunner() override {
    return std::make_unique<SynchronousTaskGraphRunner>();
  }

  void FlushDecodeTasks() {
    while (task_runner_->HasPendingTask()) {
      task_runner_->RunUntilIdle();
      base::RunLoop().RunUntilIdle();
    }
  }

  void CleanUpTileManager() {
    task_runner_->set_run_tasks_synchronously(true);
    host_impl()->tile_manager()->FinishTasksAndCleanUp();
    task_runner_->set_run_tasks_synchronously(false);
  }

 private:
  scoped_refptr<SynchronousSimpleTaskRunner> task_runner_;
};

TEST_F(CheckerImagingTileManagerTest,
       NoImageDecodeDependencyForCheckeredTiles) {
  const gfx::Size layer_bounds(512, 512);

  FakeRecordingSource recording_source(layer_bounds);
  recording_source.set_fill_with_nonsolid_color(true);

  auto generator =
      sk_make_sp<testing::StrictMock<MockImageGenerator>>(gfx::Size(512, 512));
  PaintImage image = PaintImageBuilder::WithDefault()
                         .set_id(PaintImage::GetNextId())
                         .set_paint_image_generator(generator)
                         .set_decoding_mode(PaintImage::DecodingMode::kAsync)
                         .TakePaintImage();
  recording_source.add_draw_image(image, gfx::Point(0, 0));

  recording_source.Rerecord();
  scoped_refptr<RasterSource> raster_source =
      recording_source.CreateRasterSource();

  Region invalidation((gfx::Rect(layer_bounds)));
  SetupPendingTree(raster_source, layer_bounds, invalidation);

  PictureLayerTilingSet* tiling_set =
      pending_layer()->picture_layer_tiling_set();
  PictureLayerTiling* tiling = tiling_set->tiling_at(0);
  tiling->set_resolution(HIGH_RESOLUTION);
  tiling->CreateAllTilesForTesting(gfx::Rect(layer_bounds));
  tiling->set_can_require_tiles_for_activation(true);

  // PrepareTiles and synchronously run all tasks added to the TaskGraph. Since
  // we are using a strict mock for the SkImageGenerator, if the decode runs as
  // a part of raster tasks, the test should fail.
  host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());
  EXPECT_TRUE(host_impl()->tile_manager()->HasScheduledTileTasksForTesting());
  static_cast<SynchronousTaskGraphRunner*>(task_graph_runner())->RunUntilIdle();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(host_impl()->tile_manager()->HasScheduledTileTasksForTesting());
}

class EmptyCacheTileManagerTest : public TileManagerTest {
 public:
  LayerTreeSettings CreateSettings() override {
    auto settings = TileManagerTest::CreateSettings();
    settings.decoded_image_working_set_budget_bytes = 0;
    return settings;
  }
};

TEST_F(EmptyCacheTileManagerTest, AtRasterOnScreenTileRasterTasks) {
  const gfx::Size layer_bounds(500, 500);

  FakeRecordingSource recording_source(layer_bounds);
  recording_source.set_fill_with_nonsolid_color(true);

  int dimension = 500;
  PaintImage image = MakeCheckerablePaintImage(gfx::Size(dimension, dimension));
  recording_source.add_draw_image(image, gfx::Point(0, 0));

  recording_source.Rerecord();
  scoped_refptr<RasterSource> raster_source =
      recording_source.CreateRasterSource();

  gfx::Size tile_size(500, 500);
  Region invalidation((gfx::Rect(layer_bounds)));
  SetupPendingTree(raster_source, tile_size, invalidation);

  PictureLayerTilingSet* tiling_set =
      pending_layer()->picture_layer_tiling_set();
  PictureLayerTiling* pending_tiling = tiling_set->tiling_at(0);
  pending_tiling->set_resolution(HIGH_RESOLUTION);
  pending_tiling->CreateAllTilesForTesting(gfx::Rect(layer_bounds));

  host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());
  // There will be a tile raster task and an image decode task.
  EXPECT_TRUE(pending_tiling->TileAt(0, 0)->HasRasterTask());
  EXPECT_TRUE(host_impl()->tile_manager()->HasScheduledTileTasksForTesting());
}

TEST_F(EmptyCacheTileManagerTest, AtRasterPrepaintTileRasterTasksSkipped) {
  const gfx::Size layer_bounds(500, 500);

  FakeRecordingSource recording_source(layer_bounds);
  recording_source.set_fill_with_nonsolid_color(true);

  int dimension = 500;
  PaintImage image = MakeCheckerablePaintImage(gfx::Size(dimension, dimension));
  recording_source.add_draw_image(image, gfx::Point(0, 0));

  recording_source.Rerecord();
  scoped_refptr<RasterSource> raster_source =
      recording_source.CreateRasterSource();

  gfx::Size tile_size(500, 500);
  Region invalidation((gfx::Rect(layer_bounds)));
  SetupPendingTree(raster_source, tile_size, invalidation);

  PictureLayerTilingSet* tiling_set =
      pending_layer()->picture_layer_tiling_set();
  PictureLayerTiling* pending_tiling = tiling_set->tiling_at(0);
  pending_tiling->set_resolution(HIGH_RESOLUTION);
  pending_tiling->CreateAllTilesForTesting();
  pending_tiling->SetTilePriorityRectsForTesting(
      gfx::Rect(),  // An empty visual rect leads to the tile being pre-paint.
      gfx::Rect(layer_bounds),   // Skewport rect.
      gfx::Rect(layer_bounds),   // Soon rect.
      gfx::Rect(layer_bounds));  // Eventually rect.

  host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());
  // There will be no tile raster task, but there will be an image decode task.
  EXPECT_FALSE(pending_tiling->TileAt(0, 0)->HasRasterTask());
  EXPECT_TRUE(host_impl()->tile_manager()->HasScheduledTileTasksForTesting());
}

TEST_F(CheckerImagingTileManagerTest, BuildsImageDecodeQueueAsExpected) {
  const gfx::Size layer_bounds(900, 900);

  FakeRecordingSource recording_source(layer_bounds);
  recording_source.set_fill_with_nonsolid_color(true);

  int dimension = 450;
  PaintImage image1 =
      MakeCheckerablePaintImage(gfx::Size(dimension, dimension));
  PaintImage image2 =
      MakeCheckerablePaintImage(gfx::Size(dimension, dimension));
  PaintImage image3 =
      MakeCheckerablePaintImage(gfx::Size(dimension, dimension));
  recording_source.add_draw_image(image1, gfx::Point(0, 0));
  recording_source.add_draw_image(image2, gfx::Point(600, 0));
  recording_source.add_draw_image(image3, gfx::Point(0, 600));

  recording_source.Rerecord();
  scoped_refptr<RasterSource> raster_source =
      recording_source.CreateRasterSource();

  gfx::Size tile_size(500, 500);
  Region invalidation((gfx::Rect(layer_bounds)));
  SetupPendingTree(raster_source, tile_size, invalidation);

  PictureLayerTilingSet* tiling_set =
      pending_layer()->picture_layer_tiling_set();
  PictureLayerTiling* pending_tiling = tiling_set->tiling_at(0);
  pending_tiling->set_resolution(HIGH_RESOLUTION);
  pending_tiling->CreateAllTilesForTesting(gfx::Rect(layer_bounds));

  // PrepareTiles and make sure we account correctly for tiles that have been
  // scheduled with checkered images.
  host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());
  EXPECT_TRUE(host_impl()->tile_manager()->HasScheduledTileTasksForTesting());

  for (int i = 0; i < 2; i++) {
    for (int j = 0; j < 2; j++) {
      const Tile* tile = pending_tiling->TileAt(i, j);
      EXPECT_TRUE(tile->HasRasterTask());
      if (i == 1 && j == 1)
        EXPECT_FALSE(tile->raster_task_scheduled_with_checker_images());
      else
        EXPECT_TRUE(tile->raster_task_scheduled_with_checker_images());
    }
  }
  EXPECT_EQ(host_impl()->tile_manager()->num_of_tiles_with_checker_images(), 3);

  // Now raster all the tiles and make sure these tiles are still accounted for
  // with checkered images.
  static_cast<SynchronousTaskGraphRunner*>(task_graph_runner())->RunUntilIdle();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(host_impl()->tile_manager()->HasScheduledTileTasksForTesting());
  for (int i = 0; i < 2; i++) {
    for (int j = 0; j < 2; j++) {
      const Tile* tile = pending_tiling->TileAt(i, j);
      EXPECT_FALSE(tile->HasRasterTask());
      EXPECT_FALSE(tile->raster_task_scheduled_with_checker_images());
      EXPECT_TRUE(tile->draw_info().has_resource());
      if (i == 1 && j == 1)
        EXPECT_FALSE(tile->draw_info().is_checker_imaged());
      else
        EXPECT_TRUE(tile->draw_info().is_checker_imaged());
    }
  }
  EXPECT_EQ(host_impl()->tile_manager()->num_of_tiles_with_checker_images(), 3);

  // Activate the pending tree.
  ActivateTree();

  // Set empty tile priority rects so an empty image decode queue is used.
  gfx::Rect empty_rect;
  PictureLayerTiling* active_tiling =
      active_layer()->picture_layer_tiling_set()->tiling_at(0);
  active_tiling->SetTilePriorityRectsForTesting(
      gfx::Rect(empty_rect),   // Visible rect.
      gfx::Rect(empty_rect),   // Skewport rect.
      gfx::Rect(empty_rect),   // Soon rect.
      gfx::Rect(empty_rect));  // Eventually rect.
  host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());

  // Run the decode tasks. Since the first decode is always scheduled, the
  // completion for it should be triggered.
  FlushDecodeTasks();

  // Create a new pending tree to invalidate tiles for decoded images and verify
  // that only tiles for |image1| are invalidated.
  EXPECT_TRUE(host_impl()->client()->did_request_impl_side_invalidation());
  PerformImplSideInvalidation();
  for (int i = 0; i < 2; i++) {
    for (int j = 0; j < 2; j++) {
      const Tile* tile = pending_tiling->TileAt(i, j);
      if (i == 0 && j == 0)
        EXPECT_TRUE(tile);
      else
        EXPECT_FALSE(tile);
    }
  }
  host_impl()->client()->reset_did_request_impl_side_invalidation();

  // Activating the tree replaces the checker-imaged tile.
  EXPECT_EQ(host_impl()->tile_manager()->num_of_tiles_with_checker_images(), 3);
  ActivateTree();
  EXPECT_EQ(host_impl()->tile_manager()->num_of_tiles_with_checker_images(), 2);

  // Set the tile priority rects such that only the tile with the second image
  // is scheduled for decodes, since it is checker-imaged.
  gfx::Rect rect_to_raster(600, 0, 300, 900);
  active_tiling->SetTilePriorityRectsForTesting(
      gfx::Rect(rect_to_raster),   // Visible rect.
      gfx::Rect(rect_to_raster),   // Skewport rect.
      gfx::Rect(rect_to_raster),   // Soon rect.
      gfx::Rect(rect_to_raster));  // Eventually rect.
  host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());

  // Finish all raster and dispatch completion callback so that the decode work
  // for checkered images can be scheduled.
  static_cast<SynchronousTaskGraphRunner*>(task_graph_runner())->RunUntilIdle();
  base::RunLoop().RunUntilIdle();

  // Run decode tasks to trigger completion of any pending decodes.
  FlushDecodeTasks();

  // Create a new pending tree to invalidate tiles for decoded images and verify
  // that only tiles for |image2| are invalidated.
  EXPECT_TRUE(host_impl()->client()->did_request_impl_side_invalidation());
  PerformImplSideInvalidation();
  for (int i = 0; i < 2; i++) {
    for (int j = 0; j < 2; j++) {
      const Tile* tile = pending_tiling->TileAt(i, j);
      if (i == 1 && j == 0)
        EXPECT_TRUE(tile);
      else
        EXPECT_FALSE(tile);
    }
  }
  host_impl()->client()->reset_did_request_impl_side_invalidation();

  // Activating the tree replaces the checker-imaged tile.
  EXPECT_EQ(host_impl()->tile_manager()->num_of_tiles_with_checker_images(), 2);
  ActivateTree();
  EXPECT_EQ(host_impl()->tile_manager()->num_of_tiles_with_checker_images(), 1);

  // Set the tile priority rects to cover the complete tiling and change the
  // visibility. While |image3| has not yet been decoded, since we are
  // invisible no decodes should have been scheduled.
  active_tiling->SetTilePriorityRectsForTesting(
      gfx::Rect(layer_bounds),   // Visible rect.
      gfx::Rect(layer_bounds),   // Skewport rect.
      gfx::Rect(layer_bounds),   // Soon rect.
      gfx::Rect(layer_bounds));  // Eventually rect.
  host_impl()->SetVisible(false);
  host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());
  FlushDecodeTasks();
  EXPECT_FALSE(host_impl()->client()->did_request_impl_side_invalidation());
}

TEST_F(CheckerImagingTileManagerTest,
       TileManagerCleanupClearsCheckerImagedDecodes) {
  const gfx::Size layer_bounds(512, 512);

  FakeRecordingSource recording_source(layer_bounds);
  recording_source.set_fill_with_nonsolid_color(true);
  PaintImage image = MakeCheckerablePaintImage(gfx::Size(512, 512));
  recording_source.add_draw_image(image, gfx::Point(0, 0));
  recording_source.Rerecord();
  scoped_refptr<RasterSource> raster_source =
      recording_source.CreateRasterSource();

  SetupPendingTree(raster_source, gfx::Size(100, 100),
                   Region(gfx::Rect(0, 0, 500, 500)));

  host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());
  // Finish all raster and dispatch completion callback so that the decode work
  // for checkered images can be scheduled.
  static_cast<SynchronousTaskGraphRunner*>(task_graph_runner())->RunUntilIdle();
  base::RunLoop().RunUntilIdle();
  FlushDecodeTasks();

  EXPECT_TRUE(host_impl()
                  ->tile_manager()
                  ->checker_image_tracker()
                  .has_locked_decodes_for_testing());

  host_impl()->pending_tree()->ReleaseTileResources();
  CleanUpTileManager();

  EXPECT_FALSE(host_impl()
                   ->tile_manager()
                   ->checker_image_tracker()
                   .has_locked_decodes_for_testing());
  EXPECT_TRUE(
      host_impl()->tile_manager()->TakeImagesToInvalidateOnSyncTree().empty());
}

TEST_F(CheckerImagingTileManagerTest,
       TileManagerCorrectlyPrioritizesCheckerImagedDecodes) {
  gfx::Size layer_bounds(500, 500);

  FakeRecordingSource recording_source(layer_bounds);
  recording_source.set_fill_with_nonsolid_color(true);
  PaintImage image = MakeCheckerablePaintImage(gfx::Size(512, 512));
  recording_source.add_draw_image(image, gfx::Point(0, 0));
  recording_source.Rerecord();
  scoped_refptr<RasterSource> raster_source =
      recording_source.CreateRasterSource();

  // Required for activation tiles block checker-imaged decodes.
  SetupPendingTree(raster_source, gfx::Size(100, 100),
                   Region(gfx::Rect(0, 0, 500, 500)));
  host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());
  EXPECT_TRUE(host_impl()->tile_manager()->HasScheduledTileTasksForTesting());
  EXPECT_TRUE(host_impl()
                  ->tile_manager()
                  ->checker_image_tracker()
                  .no_decodes_allowed_for_testing());
  while (!host_impl()->client()->ready_to_activate()) {
    static_cast<SynchronousTaskGraphRunner*>(task_graph_runner())
        ->RunSingleTaskForTesting();
    base::RunLoop().RunUntilIdle();
  }
  EXPECT_EQ(host_impl()
                ->tile_manager()
                ->checker_image_tracker()
                .decode_priority_allowed_for_testing(),
            CheckerImageTracker::DecodeType::kRaster);

  // Finishing all tasks allows pre-decodes.
  static_cast<SynchronousTaskGraphRunner*>(task_graph_runner())->RunUntilIdle();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(host_impl()
                ->tile_manager()
                ->checker_image_tracker()
                .decode_priority_allowed_for_testing(),
            CheckerImageTracker::DecodeType::kPreDecode);

  // Required for draw tiles block checker-imaged decodes.
  // Free all tile resources and perform another PrepareTiles.
  ActivateTree();
  EXPECT_TRUE(host_impl()->tile_manager()->IsReadyToDraw());
  host_impl()->tile_manager()->PrepareTiles(
      GlobalStateThatImpactsTilePriority());
  EXPECT_FALSE(host_impl()->tile_manager()->IsReadyToDraw());

  host_impl()->client()->reset_ready_to_draw();
  host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());
  EXPECT_TRUE(host_impl()->tile_manager()->HasScheduledTileTasksForTesting());
  EXPECT_TRUE(host_impl()
                  ->tile_manager()
                  ->checker_image_tracker()
                  .no_decodes_allowed_for_testing());
  while (!host_impl()->tile_manager()->IsReadyToDraw()) {
    static_cast<SynchronousTaskGraphRunner*>(task_graph_runner())
        ->RunSingleTaskForTesting();
    base::RunLoop().RunUntilIdle();
  }
  EXPECT_EQ(host_impl()
                ->tile_manager()
                ->checker_image_tracker()
                .decode_priority_allowed_for_testing(),
            CheckerImageTracker::DecodeType::kRaster);
}

class CheckerImagingTileManagerMemoryTest
    : public CheckerImagingTileManagerTest {
 public:
  std::unique_ptr<FakeLayerTreeHostImpl> CreateHostImpl(
      const LayerTreeSettings& settings,
      TaskRunnerProvider* task_runner_provider,
      TaskGraphRunner* task_graph_runner) override {
    LayerTreeSettings new_settings = settings;
    new_settings.memory_policy.num_resources_limit = 4;
    return CheckerImagingTileManagerTest::CreateHostImpl(
        new_settings, task_runner_provider, task_graph_runner);
  }
};

TEST_F(CheckerImagingTileManagerMemoryTest, AddsAllNowTilesToImageDecodeQueue) {
  const gfx::Size layer_bounds(900, 1400);

  FakeRecordingSource recording_source(layer_bounds);
  recording_source.set_fill_with_nonsolid_color(true);

  int dimension = 450;
  PaintImage image1 =
      MakeCheckerablePaintImage(gfx::Size(dimension, dimension));
  PaintImage image2 =
      MakeCheckerablePaintImage(gfx::Size(dimension, dimension));
  recording_source.add_draw_image(image1, gfx::Point(0, 515));
  recording_source.add_draw_image(image2, gfx::Point(515, 515));

  recording_source.Rerecord();
  scoped_refptr<RasterSource> raster_source =
      recording_source.CreateRasterSource();

  gfx::Size tile_size(500, 500);
  Region invalidation((gfx::Rect(layer_bounds)));
  SetupPendingTree(raster_source, tile_size, invalidation);

  PictureLayerTilingSet* tiling_set =
      pending_layer()->picture_layer_tiling_set();
  PictureLayerTiling* pending_tiling = tiling_set->tiling_at(0);
  pending_tiling->set_resolution(HIGH_RESOLUTION);

  // Use a rect that only rasterizes the bottom 2 rows of tiles.
  gfx::Rect rect_to_raster(0, 500, 900, 900);
  pending_tiling->CreateAllTilesForTesting(rect_to_raster);

  // PrepareTiles, rasterize all scheduled tiles and activate while no images
  // have been decoded.
  host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());
  static_cast<SynchronousTaskGraphRunner*>(task_graph_runner())->RunUntilIdle();
  base::RunLoop().RunUntilIdle();
  ActivateTree();

  // Expand the visible rect to include the complete tiling. The tile iteration
  // will not go beyond the first tile since there are no resources with a lower
  // priority that can be evicted. But we should still see image decodes
  // scheduled for all visible tiles.
  gfx::Rect complete_tiling_rect(layer_bounds);
  PictureLayerTiling* active_tiling =
      active_layer()->picture_layer_tiling_set()->tiling_at(0);
  active_tiling->SetTilePriorityRectsForTesting(
      complete_tiling_rect,   // Visible rect.
      complete_tiling_rect,   // Skewport rect.
      complete_tiling_rect,   // Soon rect.
      complete_tiling_rect);  // Eventually rect.
  host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());

  // Finish all raster work so the decode work for checkered images can be
  // scheduled.
  static_cast<SynchronousTaskGraphRunner*>(task_graph_runner())->RunUntilIdle();
  base::RunLoop().RunUntilIdle();

  // Flush all decode tasks. The tiles with checkered images should be
  // invalidated.
  FlushDecodeTasks();
  EXPECT_TRUE(host_impl()->client()->did_request_impl_side_invalidation());
  PerformImplSideInvalidation();
  for (int i = 0; i < 2; i++) {
    for (int j = 0; j < 3; j++) {
      const Tile* tile = pending_tiling->TileAt(i, j);
      if (j == 1)
        EXPECT_TRUE(tile);
      else
        EXPECT_FALSE(tile);
    }
  }
  host_impl()->client()->reset_did_request_impl_side_invalidation();
}

class VerifyImageProviderRasterBuffer : public RasterBuffer {
 public:
  VerifyImageProviderRasterBuffer() = default;
  ~VerifyImageProviderRasterBuffer() override { EXPECT_TRUE(did_raster_); }

  void Playback(const RasterSource* raster_source,
                const gfx::Rect& raster_full_rect,
                const gfx::Rect& raster_dirty_rect,
                uint64_t new_content_id,
                const gfx::AxisTransform2d& transform,
                const RasterSource::PlaybackSettings& playback_settings,
                const GURL& url) override {
    did_raster_ = true;
    EXPECT_TRUE(playback_settings.image_provider);
  }

  bool SupportsBackgroundThreadPriority() const override { return true; }

 private:
  bool did_raster_ = false;
};

class VerifyImageProviderRasterBufferProvider
    : public FakeRasterBufferProviderImpl {
 public:
  VerifyImageProviderRasterBufferProvider() = default;
  ~VerifyImageProviderRasterBufferProvider() override {
    EXPECT_GT(buffer_count_, 0);
  }

  std::unique_ptr<RasterBuffer> AcquireBufferForRaster(
      const ResourcePool::InUsePoolResource& resource,
      uint64_t resource_content_id,
      uint64_t previous_content_id,
      bool depends_on_at_raster_decodes,
      bool depends_on_hardware_accelerated_jpeg_candidates,
      bool depends_on_hardware_accelerated_webp_candidates) override {
    buffer_count_++;
    return std::make_unique<VerifyImageProviderRasterBuffer>();
  }

 private:
  int buffer_count_ = 0;
};

class SynchronousRasterTileManagerTest : public TileManagerTest {
 public:
  std::unique_ptr<TaskGraphRunner> CreateTaskGraphRunner() override {
    return std::make_unique<SynchronousTaskGraphRunner>();
  }
};

TEST_F(SynchronousRasterTileManagerTest, AlwaysUseImageCache) {
  // Tests that we always use the ImageDecodeCache during raster.
  VerifyImageProviderRasterBufferProvider raster_buffer_provider;
  host_impl()->tile_manager()->SetRasterBufferProviderForTesting(
      &raster_buffer_provider);

  gfx::Size layer_bounds(500, 500);
  scoped_refptr<FakeRasterSource> raster_source =
      FakeRasterSource::CreateFilled(layer_bounds);
  Region invalidation((gfx::Rect(layer_bounds)));
  SetupPendingTree(raster_source, layer_bounds, invalidation);
  PictureLayerTilingSet* tiling_set =
      pending_layer()->picture_layer_tiling_set();
  PictureLayerTiling* pending_tiling = tiling_set->tiling_at(0);
  pending_tiling->set_resolution(HIGH_RESOLUTION);
  pending_tiling->CreateAllTilesForTesting(gfx::Rect(layer_bounds));

  host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());
  static_cast<SynchronousTaskGraphRunner*>(task_graph_runner())->RunUntilIdle();

  // Destroy the LTHI since it accesses the RasterBufferProvider during cleanup.
  ClearLayersAndHost();
}

class DecodedImageTrackerTileManagerTest : public TestLayerTreeHostBase {
 public:
  void TearDown() override {
    // Allow all tasks on the image worker to run now. Any scheduled decodes
    // will be aborted.
    task_runner_->set_run_tasks_synchronously(true);
  }

  LayerTreeSettings CreateSettings() override {
    auto settings = TestLayerTreeHostBase::CreateSettings();
    settings.max_preraster_distance_in_screen_pixels = 100;
    return settings;
  }

  std::unique_ptr<FakeLayerTreeHostImpl> CreateHostImpl(
      const LayerTreeSettings& settings,
      TaskRunnerProvider* task_runner_provider,
      TaskGraphRunner* task_graph_runner) override {
    task_runner_ = base::MakeRefCounted<SynchronousSimpleTaskRunner>();
    return std::make_unique<FakeLayerTreeHostImpl>(
        settings, task_runner_provider, task_graph_runner, task_runner_);
  }

  std::unique_ptr<TaskGraphRunner> CreateTaskGraphRunner() override {
    return std::make_unique<SynchronousTaskGraphRunner>();
  }

  void FlushDecodeTasks() {
    while (task_runner_->HasPendingTask()) {
      task_runner_->RunUntilIdle();
      base::RunLoop().RunUntilIdle();
    }
  }

 private:
  scoped_refptr<SynchronousSimpleTaskRunner> task_runner_;
};

TEST_F(DecodedImageTrackerTileManagerTest, DecodedImageTrackerDropsLocksOnUse) {
  // Pick arbitrary IDs - they don't really matter as long as they're constant.
  const int kLayerId = 7;

  host_impl()->tile_manager()->SetTileTaskManagerForTesting(
      std::make_unique<FakeTileTaskManagerImpl>());

  // Create two test images, one will be positioned to be needed NOW, the other
  // will be positioned to be prepaint.
  int dimension = 250;
  PaintImage image1 =
      CreateDiscardablePaintImage(gfx::Size(dimension, dimension));
  PaintImage image2 =
      CreateDiscardablePaintImage(gfx::Size(dimension, dimension));

  // Add the images to our decoded_image_tracker.
  host_impl()->tile_manager()->decoded_image_tracker().QueueImageDecode(
      image1, TargetColorParams(), base::DoNothing());
  host_impl()->tile_manager()->decoded_image_tracker().QueueImageDecode(
      image2, TargetColorParams(), base::DoNothing());
  EXPECT_EQ(0u, host_impl()
                    ->tile_manager()
                    ->decoded_image_tracker()
                    .NumLockedImagesForTesting());
  FlushDecodeTasks();
  EXPECT_EQ(2u, host_impl()
                    ->tile_manager()
                    ->decoded_image_tracker()
                    .NumLockedImagesForTesting());

  // Add images to a fake recording source.
  const gfx::Size layer_bounds(1000, 500);
  FakeRecordingSource recording_source(layer_bounds);
  recording_source.set_fill_with_nonsolid_color(true);
  recording_source.add_draw_image(image1, gfx::Point(0, 0));
  recording_source.add_draw_image(image2, gfx::Point(700, 0));
  recording_source.Rerecord();

  scoped_refptr<FakeRasterSource> pending_raster_source =
      FakeRasterSource::CreateFromRecordingSource(recording_source);

  host_impl()->CreatePendingTree();
  LayerTreeImpl* pending_tree = host_impl()->pending_tree();
  pending_tree->SetDeviceViewportRect(
      host_impl()->active_tree()->GetDeviceViewport());

  // Steal from the recycled tree.
  std::unique_ptr<FakePictureLayerImpl> pending_layer =
      FakePictureLayerImpl::Create(pending_tree, kLayerId,
                                   pending_raster_source);
  pending_layer->SetDrawsContent(true);

  // The bounds() are half the recording source size, allowing for prepaint
  // images.
  pending_layer->SetBounds(gfx::Size(500, 500));
  SetupRootProperties(pending_layer.get());
  pending_tree->SetRootLayerForTesting(std::move(pending_layer));

  // Add tilings/tiles for the layer.
  UpdateDrawProperties(host_impl()->pending_tree());

  // Build the raster queue and invalidate the top tile if partial raster is
  // enabled.
  std::unique_ptr<RasterTilePriorityQueue> queue(host_impl()->BuildRasterQueue(
      SAME_PRIORITY_FOR_BOTH_TREES, RasterTilePriorityQueue::Type::ALL));
  ASSERT_FALSE(queue->IsEmpty());

  // PrepareTiles to schedule tasks. This should cause the decoded image tracker
  // to release its lock.
  EXPECT_EQ(2u, host_impl()
                    ->tile_manager()
                    ->decoded_image_tracker()
                    .NumLockedImagesForTesting());
  host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());
  EXPECT_EQ(0u, host_impl()
                    ->tile_manager()
                    ->decoded_image_tracker()
                    .NumLockedImagesForTesting());
}

class HdrImageTileManagerTest : public CheckerImagingTileManagerTest {
 public:
  void DecodeHdrImage(const gfx::ColorSpace& output_cs) {
    auto color_space = gfx::ColorSpace::CreateHDR10();
    auto size = gfx::Size(250, 250);
    auto info =
        SkImageInfo::Make(size.width(), size.height(), kRGBA_F16_SkColorType,
                          kPremul_SkAlphaType, color_space.ToSkColorSpace());
    SkBitmap bitmap;
    bitmap.allocPixels(info);
    PaintImage hdr_image = PaintImageBuilder::WithDefault()
                               .set_id(PaintImage::kInvalidId)
                               .set_is_high_bit_depth(true)
                               .set_image(SkImages::RasterFromBitmap(bitmap),
                                          PaintImage::GetNextContentId())
                               .TakePaintImage();

    // Add the image to our decoded_image_tracker.
    TargetColorParams target_color_params;
    target_color_params.color_space = output_cs;
    host_impl()->tile_manager()->decoded_image_tracker().QueueImageDecode(
        hdr_image, target_color_params, base::DoNothing());
    FlushDecodeTasks();

    // Add images to a fake recording source.
    constexpr gfx::Size kLayerBounds(1000, 500);
    FakeRecordingSource recording_source(kLayerBounds);
    recording_source.set_fill_with_nonsolid_color(true);
    recording_source.add_draw_image(hdr_image, gfx::Point(0, 0));
    recording_source.Rerecord();

    auto raster_source = recording_source.CreateRasterSource();

    constexpr gfx::Size kTileSize(500, 500);
    Region invalidation((gfx::Rect(kLayerBounds)));
    SetupPendingTree(raster_source, kTileSize, invalidation);

    constexpr float kCustomWhiteLevel = 200.f;
    auto display_cs = gfx::DisplayColorSpaces(output_cs);
    if (output_cs.IsHDR())
      display_cs.SetSDRMaxLuminanceNits(kCustomWhiteLevel);

    pending_layer()->layer_tree_impl()->SetDisplayColorSpaces(display_cs);
    PictureLayerTilingSet* tiling_set =
        pending_layer()->picture_layer_tiling_set();
    PictureLayerTiling* pending_tiling = tiling_set->tiling_at(0);
    pending_tiling->set_resolution(HIGH_RESOLUTION);
    pending_tiling->CreateAllTilesForTesting(gfx::Rect(kLayerBounds));

    host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());
    ASSERT_TRUE(host_impl()->tile_manager()->HasScheduledTileTasksForTesting());

    auto pending_tiles = pending_tiling->AllTilesForTesting();
    ASSERT_FALSE(pending_tiles.empty());

    const auto raster_cs = gfx::ColorSpace::CreateExtendedSRGB();
    if (output_cs.IsHDR()) {
      // Only the last tile will have any pending tasks.
      const auto& pending_tasks =
          host_impl()->tile_manager()->decode_tasks_for_testing(
              pending_tiles.back()->id());
      EXPECT_FALSE(pending_tasks.empty());
      for (const auto& draw_info : pending_tasks) {
        EXPECT_EQ(draw_info.target_color_space(), raster_cs);
        EXPECT_FLOAT_EQ(draw_info.sdr_white_level(), kCustomWhiteLevel);
      }
    }

    // Raster all tiles.
    static_cast<SynchronousTaskGraphRunner*>(task_graph_runner())
        ->RunUntilIdle();
    base::RunLoop().RunUntilIdle();
    ASSERT_FALSE(
        host_impl()->tile_manager()->HasScheduledTileTasksForTesting());

    auto expected_format = output_cs.IsHDR()
                               ? viz::SinglePlaneFormat::kRGBA_F16
                               : viz::SinglePlaneFormat::kRGBA_8888;
    auto all_tiles = host_impl()->tile_manager()->AllTilesForTesting();
    for (const auto* tile : all_tiles)
      EXPECT_EQ(expected_format,
                tile->draw_info().resource_shared_image_format());
  }
};

TEST_F(HdrImageTileManagerTest, DecodeHdrImagesToHdrPq) {
  DecodeHdrImage(gfx::ColorSpace::CreateHDR10());
}

TEST_F(HdrImageTileManagerTest, DecodeHdrImagesToHdrHlg) {
  DecodeHdrImage(gfx::ColorSpace::CreateHLG());
}

TEST_F(HdrImageTileManagerTest, DecodeHdrImagesToSdrSrgb) {
  DecodeHdrImage(gfx::ColorSpace::CreateSRGB());
}

TEST_F(HdrImageTileManagerTest, DecodeHdrImagesToSdrP3) {
  DecodeHdrImage(gfx::ColorSpace::CreateDisplayP3D65());
}

class TileManagerCheckRasterQueriesTest : public TileManagerTest {
 public:
  TileManagerCheckRasterQueriesTest()
      : pending_raster_queries_(
            viz::TestContextProvider::CreateWorker().get()) {}

  void SetUp() override {
    TileManagerTest::SetUp();
    host_impl()->tile_manager()->SetPendingRasterQueriesForTesting(
        &pending_raster_queries_);
  }

 protected:
  class MockRasterQueryQueue : public FakeRasterQueryQueue {
   public:
    explicit MockRasterQueryQueue(
        viz::RasterContextProvider* const worker_context_provider)
        : FakeRasterQueryQueue(worker_context_provider) {}
    MOCK_METHOD0(CheckRasterFinishedQueries, bool());
  };

  MockRasterQueryQueue pending_raster_queries_;
};

TEST_F(TileManagerCheckRasterQueriesTest,
       ChecksRasterQueriesInAllTilesDoneTask) {
  base::RunLoop run_loop;
  EXPECT_FALSE(host_impl()->tile_manager()->HasScheduledTileTasksForTesting());
  EXPECT_CALL(MockHostImpl(), NotifyAllTileTasksCompleted())
      .WillOnce(testing::Invoke([&run_loop]() { run_loop.Quit(); }));
  EXPECT_CALL(pending_raster_queries_, CheckRasterFinishedQueries()).Times(1);
  host_impl()->tile_manager()->PrepareTiles(host_impl()->global_tile_state());
  EXPECT_TRUE(host_impl()->tile_manager()->HasScheduledTileTasksForTesting());
  run_loop.Run();
}

class TileManagerTileReclaimTest : public TileManagerTest {
 public:
  void SetUp() override {
    TileManagerTest::SetUp();
    task_runner_ = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
    host_impl()->tile_manager()->SetOverridesForTesting(
        task_runner_, task_runner_->GetMockTickClock());

    host_impl()->AdvanceToNextFrame(base::Milliseconds(1));
    gfx::Size layer_bounds(1000, 1000);
    SetupDefaultTrees(layer_bounds);

    // Create a pending child layer.
    scoped_refptr<FakeRasterSource> pending_raster_source =
        FakeRasterSource::CreateFilled(layer_bounds);
    auto* pending_child = AddLayer<FakePictureLayerImpl>(
        host_impl()->pending_tree(), pending_raster_source);
    pending_child->SetDrawsContent(true);
    CopyProperties(pending_layer(), pending_child);
  }

  void TearDown() override {
    ASSERT_FALSE(task_runner()->HasPendingTask());
    host_impl()->tile_manager()->SetOverridesForTesting(nullptr, nullptr);
    TileManagerTest::TearDown();
  }

  scoped_refptr<base::TestMockTimeTaskRunner> task_runner() const {
    return task_runner_;
  }

  void MakeFrame(GlobalStateThatImpactsTilePriority global_tile_state) {
    host_impl()->AdvanceToNextFrame(base::Milliseconds(1));
    UpdateDrawProperties(host_impl()->active_tree());
    UpdateDrawProperties(host_impl()->pending_tree());

    base::RunLoop run_loop;
    EXPECT_CALL(MockHostImpl(), NotifyAllTileTasksCompleted())
        .WillOnce(Invoke([&run_loop]() { run_loop.Quit(); }));
    host_impl()->tile_manager()->PrepareTiles(global_tile_state);
    run_loop.Run();
  }

 private:
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
};

TEST_F(TileManagerTileReclaimTest, ReclaimOldPrepainTilesSimple) {
  base::test::ScopedFeatureList scoped_feature_list{
      features::kReclaimOldPrepaintTiles};

  // Set a small viewport, so we have soon and eventually tiles.
  gfx::Rect viewport{100, 100, 100, 100};

  GlobalStateThatImpactsTilePriority global_tile_state =
      host_impl()->global_tile_state();
  ASSERT_EQ(global_tile_state.memory_limit_policy,
            TileMemoryLimitPolicy::ALLOW_ANYTHING);
  host_impl()->active_tree()->SetDeviceViewportRect(viewport);
  MakeFrame(global_tile_state);

  auto eviction_queue = host_impl()->BuildEvictionQueue(
      host_impl()->global_tile_state().tree_priority);
  bool has_eventually_tiles = false;
  size_t tiles_count_before = 0;
  for (; !eviction_queue->IsEmpty(); eviction_queue->Pop()) {
    const auto& tile = eviction_queue->Top();
    has_eventually_tiles |=
        tile.priority().priority_bin == TilePriority::EVENTUALLY &&
        tile.tile()->draw_info().IsReadyToDraw();
    // Tiles are initially marked as "used".
    EXPECT_TRUE(tile.tile()->used());
    tiles_count_before++;
  }
  ASSERT_TRUE(has_eventually_tiles);

  task_runner()->FastForwardBy(TileManager::GetTrimPrepaintTilesDelay());

  // Since the policy is still ALLOW_ANYTHING, no changes.
  eviction_queue = host_impl()->BuildEvictionQueue(
      host_impl()->global_tile_state().tree_priority);
  size_t tiles_count_after = 0;
  for (; !eviction_queue->IsEmpty(); eviction_queue->Pop()) {
    const auto& tile = eviction_queue->Top();
    has_eventually_tiles |=
        tile.priority().priority_bin == TilePriority::EVENTUALLY &&
        tile.tile()->draw_info().IsReadyToDraw();
    EXPECT_TRUE(tile.tile()->used());
    tiles_count_after++;
  }
  EXPECT_EQ(tiles_count_after, tiles_count_before);

  // No progress can be made, do not repost a task.
  EXPECT_FALSE(task_runner()->HasPendingTask());

  // Change to a policy where EVENTUALLY tiles can be reclaimed.
  global_tile_state.memory_limit_policy =
      TileMemoryLimitPolicy::ALLOW_PREPAINT_ONLY;
  host_impl()->active_tree()->SetDeviceViewportRect(viewport);
  MakeFrame(global_tile_state);
  EXPECT_TRUE(task_runner()->HasPendingTask());

  task_runner()->FastForwardBy(TileManager::GetTrimPrepaintTilesDelay());

  tiles_count_after = 0;
  eviction_queue = host_impl()->BuildEvictionQueue(
      host_impl()->global_tile_state().tree_priority);
  for (; !eviction_queue->IsEmpty(); eviction_queue->Pop()) {
    const auto& tile = eviction_queue->Top();
    if (tile.priority().priority_bin == TilePriority::EVENTUALLY) {
      EXPECT_FALSE(tile.tile()->used());
    } else {
      EXPECT_TRUE(tile.tile()->used());
    }
    tiles_count_after++;
  }
  EXPECT_EQ(tiles_count_after, tiles_count_before);

  // A task is re-posted, since we can make progress (some tiles became "old").
  EXPECT_TRUE(task_runner()->HasPendingTask());
  task_runner()->FastForwardBy(TileManager::GetTrimPrepaintTilesDelay());

  // All the EVENTUALLY tiles are gone.
  tiles_count_after = 0;
  eviction_queue = host_impl()->BuildEvictionQueue(
      host_impl()->global_tile_state().tree_priority);
  for (; !eviction_queue->IsEmpty(); eviction_queue->Pop()) {
    const auto& tile = eviction_queue->Top();
    EXPECT_NE(tile.priority().priority_bin, TilePriority::EVENTUALLY);
    tiles_count_after++;
  }
  EXPECT_LT(tiles_count_after, tiles_count_before);

  // And there is nothing left to do.
  EXPECT_FALSE(task_runner()->HasPendingTask());
}

TEST_F(TileManagerTileReclaimTest, ReclaimOldPrepainTilesOldYoungTiles) {
  base::test::ScopedFeatureList scoped_feature_list{
      features::kReclaimOldPrepaintTiles};

  // Set a small viewport, so we have soon and eventually tiles.
  gfx::Rect viewport{100, 100, 100, 100};

  GlobalStateThatImpactsTilePriority global_tile_state =
      host_impl()->global_tile_state();
  host_impl()->active_tree()->SetDeviceViewportRect(viewport);
  MakeFrame(global_tile_state);

  auto eviction_queue = host_impl()->BuildEvictionQueue(
      host_impl()->global_tile_state().tree_priority);
  bool has_eventually_tiles = false;
  size_t tiles_count_before = 0;
  for (; !eviction_queue->IsEmpty(); eviction_queue->Pop()) {
    const auto& tile = eviction_queue->Top();
    has_eventually_tiles |=
        tile.priority().priority_bin == TilePriority::EVENTUALLY &&
        tile.tile()->draw_info().IsReadyToDraw();
    // Tiles are initially marked as "used".
    EXPECT_TRUE(tile.tile()->used());
    tiles_count_before++;
  }
  ASSERT_TRUE(has_eventually_tiles);

  global_tile_state.memory_limit_policy =
      TileMemoryLimitPolicy::ALLOW_PREPAINT_ONLY;
  host_impl()->active_tree()->SetDeviceViewportRect(viewport);
  MakeFrame(global_tile_state);
  EXPECT_TRUE(task_runner()->HasPendingTask());

  task_runner()->FastForwardBy(TileManager::GetTrimPrepaintTilesDelay());

  size_t tiles_count_after = 0;
  Tile* first_eventually_tile = nullptr;
  eviction_queue = host_impl()->BuildEvictionQueue(
      host_impl()->global_tile_state().tree_priority);
  for (; !eviction_queue->IsEmpty(); eviction_queue->Pop()) {
    const auto& tile = eviction_queue->Top();
    if (tile.priority().priority_bin == TilePriority::EVENTUALLY) {
      EXPECT_FALSE(tile.tile()->used());
      if (!first_eventually_tile) {
        first_eventually_tile = tile.tile();
      }
    } else {
      EXPECT_TRUE(tile.tile()->used());
    }
    tiles_count_after++;
  }
  EXPECT_EQ(tiles_count_after, tiles_count_before);
  // A task is re-posted, since we can make progress (some tiles became "old").
  EXPECT_TRUE(task_runner()->HasPendingTask());

  // Pretend that the tile was used in the meantime.
  first_eventually_tile->mark_used();

  task_runner()->FastForwardBy(TileManager::GetTrimPrepaintTilesDelay());

  // The tile is there, it's the only remaining EVENTUALLY one.
  tiles_count_after = 0;
  bool has_found_tile = false;
  eviction_queue = host_impl()->BuildEvictionQueue(
      host_impl()->global_tile_state().tree_priority);
  for (; !eviction_queue->IsEmpty(); eviction_queue->Pop()) {
    const auto& tile = eviction_queue->Top();
    EXPECT_TRUE(tile.priority().priority_bin != TilePriority::EVENTUALLY ||
                tile.tile() == first_eventually_tile);
    has_found_tile |= tile.tile() == first_eventually_tile;
    tiles_count_after++;
  }
  EXPECT_LT(tiles_count_after, tiles_count_before);
  EXPECT_TRUE(has_found_tile);
  EXPECT_FALSE(first_eventually_tile->used());

  // Progress can be made
  EXPECT_TRUE(task_runner()->HasPendingTask());
  task_runner()->FastForwardBy(TileManager::GetTrimPrepaintTilesDelay());

  // The tile is not there anymore.
  eviction_queue = host_impl()->BuildEvictionQueue(
      host_impl()->global_tile_state().tree_priority);
  for (; !eviction_queue->IsEmpty(); eviction_queue->Pop()) {
    const auto& tile = eviction_queue->Top();
    EXPECT_NE(tile.tile(), first_eventually_tile);
  }
  // No progress can be made, do not repost a task.
  EXPECT_FALSE(task_runner()->HasPendingTask());
}

}  // namespace
}  // namespace cc
