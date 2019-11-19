// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/timer/lap_timer.h"
#include "cc/raster/raster_buffer.h"
#include "cc/test/fake_impl_task_runner_provider.h"
#include "cc/test/fake_layer_tree_frame_sink.h"
#include "cc/test/fake_layer_tree_frame_sink_client.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_picture_layer_impl.h"
#include "cc/test/fake_raster_source.h"
#include "cc/test/fake_tile_manager.h"
#include "cc/test/fake_tile_manager_client.h"
#include "cc/test/fake_tile_task_manager.h"
#include "cc/test/layer_test_common.h"
#include "cc/test/test_layer_tree_host_base.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/test/test_tile_priorities.h"
#include "cc/tiles/tile.h"
#include "cc/tiles/tile_priority.h"
#include "cc/trees/layer_tree_impl.h"
#include "components/viz/test/begin_frame_args_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"

namespace cc {
namespace {

static const int kTimeLimitMillis = 2000;
static const int kWarmupRuns = 5;
static const int kTimeCheckInterval = 10;

class TileManagerPerfTest : public TestLayerTreeHostBase {
 public:
  TileManagerPerfTest()
      : timer_(kWarmupRuns,
               base::TimeDelta::FromMilliseconds(kTimeLimitMillis),
               kTimeCheckInterval) {}

  void InitializeFrameSink() override {
    host_impl()->SetVisible(true);
    host_impl()->InitializeFrameSink(layer_tree_frame_sink());
    tile_manager()->SetTileTaskManagerForTesting(
        std::make_unique<FakeTileTaskManagerImpl>());
  }

  void SetupDefaultTreesWithFixedTileSize(const gfx::Size& layer_bounds,
                                          const gfx::Size& tile_size) {
    scoped_refptr<FakeRasterSource> pending_raster_source =
        FakeRasterSource::CreateFilledWithImages(layer_bounds);
    scoped_refptr<FakeRasterSource> active_raster_source =
        FakeRasterSource::CreateFilledWithImages(layer_bounds);

    SetupPendingTree(std::move(active_raster_source), tile_size, Region());
    ActivateTree();
    SetupPendingTree(std::move(pending_raster_source), tile_size, Region());
  }

  perf_test::PerfResultReporter SetUpReporter(const std::string& story_name) {
    perf_test::PerfResultReporter reporter("tile_manager", story_name);
    reporter.RegisterImportantMetric("_raster_tile_queue_construct", "runs/s");
    reporter.RegisterImportantMetric("_raster_tile_queue_construct_and_iterate",
                                     "runs/s");
    reporter.RegisterImportantMetric("_eviction_tile_queue_construct",
                                     "runs/s");
    reporter.RegisterImportantMetric(
        "_eviction_tile_queue_construct_and_iterate", "runs/s");
    return reporter;
  }

  void RunRasterQueueConstructTest(const std::string& test_name,
                                   int layer_count) {
    TreePriority priorities[] = {SAME_PRIORITY_FOR_BOTH_TREES,
                                 SMOOTHNESS_TAKES_PRIORITY,
                                 NEW_CONTENT_TAKES_PRIORITY};
    int priority_count = 0;

    std::vector<FakePictureLayerImpl*> layers = CreateLayers(layer_count, 10);
    for (auto* layer : layers)
      layer->UpdateTiles();

    timer_.Reset();
    do {
      std::unique_ptr<RasterTilePriorityQueue> queue(
          host_impl()->BuildRasterQueue(priorities[priority_count],
                                        RasterTilePriorityQueue::Type::ALL));
      priority_count = (priority_count + 1) % base::size(priorities);
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    perf_test::PerfResultReporter reporter = SetUpReporter(test_name);
    reporter.AddResult("_raster_tile_queue_construct", timer_.LapsPerSecond());
  }

  void RunRasterQueueConstructAndIterateTest(const std::string& test_name,
                                             int layer_count,
                                             int tile_count) {
    TreePriority priorities[] = {SAME_PRIORITY_FOR_BOTH_TREES,
                                 SMOOTHNESS_TAKES_PRIORITY,
                                 NEW_CONTENT_TAKES_PRIORITY};

    std::vector<FakePictureLayerImpl*> layers = CreateLayers(layer_count, 100);
    for (auto* layer : layers)
      layer->UpdateTiles();

    int priority_count = 0;
    timer_.Reset();
    do {
      int count = tile_count;
      std::unique_ptr<RasterTilePriorityQueue> queue(
          host_impl()->BuildRasterQueue(priorities[priority_count],
                                        RasterTilePriorityQueue::Type::ALL));
      while (count--) {
        ASSERT_FALSE(queue->IsEmpty());
        ASSERT_TRUE(queue->Top().tile());
        queue->Pop();
      }
      priority_count = (priority_count + 1) % base::size(priorities);
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    perf_test::PerfResultReporter reporter = SetUpReporter(test_name);
    reporter.AddResult("_raster_tile_queue_construct_and_iterate",
                       timer_.LapsPerSecond());
  }

  void RunEvictionQueueConstructTest(const std::string& test_name,
                                     int layer_count) {
    TreePriority priorities[] = {SAME_PRIORITY_FOR_BOTH_TREES,
                                 SMOOTHNESS_TAKES_PRIORITY,
                                 NEW_CONTENT_TAKES_PRIORITY};
    int priority_count = 0;

    std::vector<FakePictureLayerImpl*> layers = CreateLayers(layer_count, 10);
    for (auto* layer : layers) {
      layer->UpdateTiles();
      for (size_t i = 0; i < layer->num_tilings(); ++i) {
        tile_manager()->InitializeTilesWithResourcesForTesting(
            layer->tilings()->tiling_at(i)->AllTilesForTesting());
      }
    }

    timer_.Reset();
    do {
      std::unique_ptr<EvictionTilePriorityQueue> queue(
          host_impl()->BuildEvictionQueue(priorities[priority_count]));
      priority_count = (priority_count + 1) % base::size(priorities);
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    perf_test::PerfResultReporter reporter = SetUpReporter(test_name);
    reporter.AddResult("_eviction_tile_queue_construct",
                       timer_.LapsPerSecond());
  }

  void RunEvictionQueueConstructAndIterateTest(const std::string& test_name,
                                               int layer_count,
                                               int tile_count) {
    TreePriority priorities[] = {SAME_PRIORITY_FOR_BOTH_TREES,
                                 SMOOTHNESS_TAKES_PRIORITY,
                                 NEW_CONTENT_TAKES_PRIORITY};
    int priority_count = 0;

    std::vector<FakePictureLayerImpl*> layers =
        CreateLayers(layer_count, tile_count);
    for (auto* layer : layers) {
      layer->UpdateTiles();
      for (size_t i = 0; i < layer->num_tilings(); ++i) {
        tile_manager()->InitializeTilesWithResourcesForTesting(
            layer->tilings()->tiling_at(i)->AllTilesForTesting());
      }
    }

    timer_.Reset();
    do {
      int count = tile_count;
      std::unique_ptr<EvictionTilePriorityQueue> queue(
          host_impl()->BuildEvictionQueue(priorities[priority_count]));
      while (count--) {
        ASSERT_FALSE(queue->IsEmpty());
        ASSERT_TRUE(queue->Top().tile());
        queue->Pop();
      }
      priority_count = (priority_count + 1) % base::size(priorities);
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    perf_test::PerfResultReporter reporter = SetUpReporter(test_name);
    reporter.AddResult("_eviction_tile_queue_construct_and_iterate",
                       timer_.LapsPerSecond());
  }

  std::vector<FakePictureLayerImpl*> CreateLayers(int layer_count,
                                                  int num_tiles_in_high_res) {
    // Compute the width/height required for high res to get
    // num_tiles_in_high_res tiles.
    float width = std::sqrt(static_cast<float>(num_tiles_in_high_res));
    float height = num_tiles_in_high_res / width;

    // Adjust the width and height to account for the fact that tiles
    // are bigger than 1x1.
    LayerListSettings settings;
    width *= settings.default_tile_size.width();
    height *= settings.default_tile_size.height();

    // Ensure that we start with blank trees and no tiles.
    ResetTrees();

    gfx::Size layer_bounds(width, height);
    gfx::Rect viewport(width / 5, height / 5);
    host_impl()->active_tree()->SetDeviceViewportRect(viewport);
    SetupDefaultTreesWithFixedTileSize(layer_bounds,
                                       settings.default_tile_size);

    std::vector<FakePictureLayerImpl*> layers;

    // Pending layer counts as one layer.
    layers.push_back(pending_layer());

    // Create the rest of the layers as children of the root layer.
    scoped_refptr<FakeRasterSource> raster_source =
        FakeRasterSource::CreateFilledWithImages(layer_bounds);
    while (static_cast<int>(layers.size()) < layer_count) {
      auto* child_layer = AddLayer<FakePictureLayerImpl>(
          host_impl()->pending_tree(), raster_source);
      child_layer->SetBounds(layer_bounds);
      child_layer->SetDrawsContent(true);
      layers.push_back(child_layer);
      CopyProperties(pending_layer(), child_layer);
    }

    // Property trees need to be rebuilt because layers were added above.
    host_impl()->pending_tree()->set_needs_update_draw_properties();
    UpdateDrawProperties(host_impl()->pending_tree());
    for (FakePictureLayerImpl* layer : layers)
      layer->CreateAllTiles();

    return layers;
  }

  GlobalStateThatImpactsTilePriority GlobalStateForTest() {
    GlobalStateThatImpactsTilePriority state;
    gfx::Size tile_size = LayerTreeSettings().default_tile_size;
    state.soft_memory_limit_in_bytes =
        10000u * 4u *
        static_cast<size_t>(tile_size.width() * tile_size.height());
    state.hard_memory_limit_in_bytes = state.soft_memory_limit_in_bytes;
    state.num_resources_limit = 10000;
    state.memory_limit_policy = ALLOW_ANYTHING;
    state.tree_priority = SMOOTHNESS_TAKES_PRIORITY;
    return state;
  }

  void RunPrepareTilesTest(const std::string& test_name,
                           int layer_count,
                           int approximate_tile_count_per_layer) {
    std::vector<FakePictureLayerImpl*> layers =
        CreateLayers(layer_count, approximate_tile_count_per_layer);

    timer_.Reset();
    do {
      host_impl()->AdvanceToNextFrame(base::TimeDelta::FromMilliseconds(1));
      for (auto* layer : layers)
        layer->UpdateTiles();

      GlobalStateThatImpactsTilePriority global_state(GlobalStateForTest());
      tile_manager()->PrepareTiles(global_state);
      tile_manager()->CheckForCompletedTasks();
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    perf_test::PerfResultReporter reporter("prepare_tiles", test_name);
    reporter.RegisterImportantMetric("", "runs/s");
    reporter.AddResult("", timer_.LapsPerSecond());
  }

  TileManager* tile_manager() { return host_impl()->tile_manager(); }

 protected:
  base::LapTimer timer_;
};

// Failing.  https://crbug.com/792995
TEST_F(TileManagerPerfTest, DISABLED_PrepareTiles) {
  RunPrepareTilesTest("2_100", 2, 100);
  RunPrepareTilesTest("2_500", 2, 500);
  RunPrepareTilesTest("2_1000", 2, 1000);
  RunPrepareTilesTest("10_100", 10, 100);
  RunPrepareTilesTest("10_500", 10, 500);
  RunPrepareTilesTest("10_1000", 10, 1000);
  RunPrepareTilesTest("50_100", 100, 100);
  RunPrepareTilesTest("50_500", 100, 500);
  RunPrepareTilesTest("50_1000", 100, 1000);
}

TEST_F(TileManagerPerfTest, RasterTileQueueConstruct) {
  RunRasterQueueConstructTest("2", 2);
  RunRasterQueueConstructTest("10", 10);
  RunRasterQueueConstructTest("50", 50);
}

TEST_F(TileManagerPerfTest, RasterTileQueueConstructAndIterate) {
  RunRasterQueueConstructAndIterateTest("2_16", 2, 16);
  RunRasterQueueConstructAndIterateTest("2_32", 2, 32);
  RunRasterQueueConstructAndIterateTest("2_64", 2, 64);
  RunRasterQueueConstructAndIterateTest("2_128", 2, 128);
  RunRasterQueueConstructAndIterateTest("10_16", 10, 16);
  RunRasterQueueConstructAndIterateTest("10_32", 10, 32);
  RunRasterQueueConstructAndIterateTest("10_64", 10, 64);
  RunRasterQueueConstructAndIterateTest("10_128", 10, 128);
  RunRasterQueueConstructAndIterateTest("50_16", 50, 16);
  RunRasterQueueConstructAndIterateTest("50_32", 50, 32);
  RunRasterQueueConstructAndIterateTest("50_64", 50, 64);
  RunRasterQueueConstructAndIterateTest("50_128", 50, 128);
}

TEST_F(TileManagerPerfTest, EvictionTileQueueConstruct) {
  RunEvictionQueueConstructTest("2", 2);
  RunEvictionQueueConstructTest("10", 10);
  RunEvictionQueueConstructTest("50", 50);
}

TEST_F(TileManagerPerfTest, EvictionTileQueueConstructAndIterate) {
  RunEvictionQueueConstructAndIterateTest("2_16", 2, 16);
  RunEvictionQueueConstructAndIterateTest("2_32", 2, 32);
  RunEvictionQueueConstructAndIterateTest("2_64", 2, 64);
  RunEvictionQueueConstructAndIterateTest("2_128", 2, 128);
  RunEvictionQueueConstructAndIterateTest("10_16", 10, 16);
  RunEvictionQueueConstructAndIterateTest("10_32", 10, 32);
  RunEvictionQueueConstructAndIterateTest("10_64", 10, 64);
  RunEvictionQueueConstructAndIterateTest("10_128", 10, 128);
  RunEvictionQueueConstructAndIterateTest("50_16", 50, 16);
  RunEvictionQueueConstructAndIterateTest("50_32", 50, 32);
  RunEvictionQueueConstructAndIterateTest("50_64", 50, 64);
  RunEvictionQueueConstructAndIterateTest("50_128", 50, 128);
}

}  // namespace
}  // namespace cc
