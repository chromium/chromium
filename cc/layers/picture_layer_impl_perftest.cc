// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/picture_layer_impl.h"

#include "base/threading/thread_task_runner_handle.h"
#include "base/timer/lap_timer.h"
#include "cc/test/fake_picture_layer_impl.h"
#include "cc/test/fake_raster_source.h"
#include "cc/test/layer_tree_impl_test_base.h"
#include "cc/tiles/tiling_set_raster_queue_all.h"
#include "cc/trees/layer_tree_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"

namespace cc {
namespace {

static const int kTimeLimitMillis = 2000;
static const int kWarmupRuns = 5;
static const int kTimeCheckInterval = 10;

void AddTiling(float scale,
               FakePictureLayerImpl* layer,
               std::vector<Tile*>* all_tiles) {
  PictureLayerTiling* tiling =
      layer->AddTiling(gfx::AxisTransform2d(scale, gfx::Vector2dF()));

  tiling->set_resolution(HIGH_RESOLUTION);
  tiling->CreateAllTilesForTesting();
  std::vector<Tile*> tiling_tiles = tiling->AllTilesForTesting();
  std::copy(
      tiling_tiles.begin(), tiling_tiles.end(), std::back_inserter(*all_tiles));
}

class PictureLayerImplPerfTest : public LayerTreeImplTestBase,
                                 public testing::Test {
 public:
  PictureLayerImplPerfTest()
      : timer_(kWarmupRuns,
               base::TimeDelta::FromMilliseconds(kTimeLimitMillis),
               kTimeCheckInterval) {}

  PictureLayerImplPerfTest(const PictureLayerImplPerfTest&) = delete;
  PictureLayerImplPerfTest& operator=(const PictureLayerImplPerfTest&) = delete;

  void SetupPendingTree(const gfx::Size& layer_bounds) {
    scoped_refptr<FakeRasterSource> raster_source =
        FakeRasterSource::CreateFilled(layer_bounds);
    host_impl()->CreatePendingTree();
    LayerTreeImpl* pending_tree = host_impl()->pending_tree();
    pending_tree->DetachLayers();

    std::unique_ptr<FakePictureLayerImpl> pending_layer =
        FakePictureLayerImpl::Create(pending_tree, 7, raster_source);
    pending_layer->SetDrawsContent(true);
    pending_layer_ = pending_layer.get();
    pending_tree->SetElementIdsForTesting();
    SetupRootProperties(pending_layer_);
    pending_tree->SetRootLayerForTesting(std::move(pending_layer));

    PrepareForUpdateDrawProperties(pending_tree);
    // Don't update draw properties because the tilings will conflict with the
    // tilings that will be added in the tests.
  }

  void RunRasterQueueConstructAndIterateTest(const std::string& test_name,
                                             int num_tiles,
                                             const gfx::Rect& viewport_rect) {
    host_impl()->active_tree()->SetDeviceViewportRect(viewport_rect);
    host_impl()->pending_tree()->UpdateDrawProperties();

    timer_.Reset();
    do {
      int count = num_tiles;
      std::unique_ptr<TilingSetRasterQueueAll> queue(
          new TilingSetRasterQueueAll(
              pending_layer_->picture_layer_tiling_set(), false, true));
      while (count--) {
        ASSERT_TRUE(!queue->IsEmpty()) << "count: " << count;
        ASSERT_TRUE(queue->Top().tile()) << "count: " << count;
        queue->Pop();
      }
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    perf_test::PerfResultReporter reporter = SetUpReporter(test_name);
    reporter.AddResult("_raster_queue_construct_and_iterate",
                       timer_.LapsPerSecond());
  }

  void RunRasterQueueConstructTest(const std::string& test_name,
                                   const gfx::Rect& viewport) {
    host_impl()->active_tree()->SetDeviceViewportRect(viewport);
    host_impl()
        ->pending_tree()
        ->property_trees()
        ->scroll_tree.UpdateScrollOffsetBaseForTesting(
            pending_layer_->element_id(),
            gfx::ScrollOffset(viewport.x(), viewport.y()));
    host_impl()->pending_tree()->UpdateDrawProperties();

    timer_.Reset();
    do {
      std::unique_ptr<TilingSetRasterQueueAll> queue(
          new TilingSetRasterQueueAll(
              pending_layer_->picture_layer_tiling_set(), false, true));
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    perf_test::PerfResultReporter reporter = SetUpReporter(test_name);
    reporter.AddResult("_raster_queue_construct", timer_.LapsPerSecond());
  }

  void RunEvictionQueueConstructAndIterateTest(const std::string& test_name,
                                               int num_tiles,
                                               const gfx::Rect& viewport_rect) {
    host_impl()->active_tree()->SetDeviceViewportRect(viewport_rect);
    host_impl()->pending_tree()->UpdateDrawProperties();

    timer_.Reset();
    do {
      int count = num_tiles;
      std::unique_ptr<TilingSetEvictionQueue> queue(new TilingSetEvictionQueue(
          pending_layer_->picture_layer_tiling_set(),
          pending_layer_->contributes_to_drawn_render_surface()));
      while (count--) {
        ASSERT_TRUE(!queue->IsEmpty()) << "count: " << count;
        ASSERT_TRUE(queue->Top().tile()) << "count: " << count;
        queue->Pop();
      }
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    perf_test::PerfResultReporter reporter = SetUpReporter(test_name);
    reporter.AddResult("_eviction_queue_construct_and_iterate",
                       timer_.LapsPerSecond());
  }

  void RunEvictionQueueConstructTest(const std::string& test_name,
                                     const gfx::Rect& viewport) {
    host_impl()->active_tree()->SetDeviceViewportRect(viewport);
    host_impl()
        ->pending_tree()
        ->property_trees()
        ->scroll_tree.UpdateScrollOffsetBaseForTesting(
            pending_layer_->element_id(),
            gfx::ScrollOffset(viewport.x(), viewport.y()));
    host_impl()->pending_tree()->UpdateDrawProperties();

    timer_.Reset();
    do {
      std::unique_ptr<TilingSetEvictionQueue> queue(new TilingSetEvictionQueue(
          pending_layer_->picture_layer_tiling_set(),
          pending_layer_->contributes_to_drawn_render_surface()));
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    perf_test::PerfResultReporter reporter = SetUpReporter(test_name);
    reporter.AddResult("_eviction_queue_construct", timer_.LapsPerSecond());
  }

 protected:
  perf_test::PerfResultReporter SetUpReporter(const std::string& story_name) {
    perf_test::PerfResultReporter reporter("tiling_set", story_name);
    reporter.RegisterImportantMetric("_raster_queue_construct_and_iterate",
                                     "runs/s");
    reporter.RegisterImportantMetric("_raster_queue_construct", "runs/s");
    reporter.RegisterImportantMetric("_eviction_queue_construct_and_iterate",
                                     "runs/s");
    return reporter;
  }

  FakePictureLayerImpl* pending_layer_;
  base::LapTimer timer_;
};

TEST_F(PictureLayerImplPerfTest, TilingSetRasterQueueConstructAndIterate) {
  SetupPendingTree(gfx::Size(10000, 10000));

  float low_res_factor = host_impl()->settings().low_res_contents_scale_factor;

  pending_layer_->AddTiling(
      gfx::AxisTransform2d(low_res_factor, gfx::Vector2dF()));
  pending_layer_->AddTiling(gfx::AxisTransform2d(0.3f, gfx::Vector2dF()));
  pending_layer_->AddTiling(gfx::AxisTransform2d(0.7f, gfx::Vector2dF()));
  pending_layer_->AddTiling(gfx::AxisTransform2d(1.0f, gfx::Vector2dF()));
  pending_layer_->AddTiling(gfx::AxisTransform2d(2.0f, gfx::Vector2dF()));

  RunRasterQueueConstructAndIterateTest("32_100x100", 32, gfx::Rect(100, 100));
  RunRasterQueueConstructAndIterateTest("32_500x500", 32, gfx::Rect(500, 500));
  RunRasterQueueConstructAndIterateTest("64_100x100", 64, gfx::Rect(100, 100));
  RunRasterQueueConstructAndIterateTest("64_500x500", 64, gfx::Rect(500, 500));
}

TEST_F(PictureLayerImplPerfTest, TilingSetRasterQueueConstruct) {
  SetupPendingTree(gfx::Size(10000, 10000));

  float low_res_factor = host_impl()->settings().low_res_contents_scale_factor;

  pending_layer_->AddTiling(
      gfx::AxisTransform2d(low_res_factor, gfx::Vector2dF()));
  pending_layer_->AddTiling(gfx::AxisTransform2d(0.3f, gfx::Vector2dF()));
  pending_layer_->AddTiling(gfx::AxisTransform2d(0.7f, gfx::Vector2dF()));
  pending_layer_->AddTiling(gfx::AxisTransform2d(1.0f, gfx::Vector2dF()));
  pending_layer_->AddTiling(gfx::AxisTransform2d(2.0f, gfx::Vector2dF()));

  RunRasterQueueConstructTest("0_0_100x100", gfx::Rect(0, 0, 100, 100));
  RunRasterQueueConstructTest("5000_0_100x100", gfx::Rect(5000, 0, 100, 100));
  RunRasterQueueConstructTest("9999_0_100x100", gfx::Rect(9999, 0, 100, 100));
}

TEST_F(PictureLayerImplPerfTest, TilingSetEvictionQueueConstructAndIterate) {
  SetupPendingTree(gfx::Size(10000, 10000));

  float low_res_factor = host_impl()->settings().low_res_contents_scale_factor;

  std::vector<Tile*> all_tiles;
  AddTiling(low_res_factor, pending_layer_, &all_tiles);
  AddTiling(0.3f, pending_layer_, &all_tiles);
  AddTiling(0.7f, pending_layer_, &all_tiles);
  AddTiling(1.0f, pending_layer_, &all_tiles);
  AddTiling(2.0f, pending_layer_, &all_tiles);

  ASSERT_TRUE(host_impl()->tile_manager() != nullptr);
  host_impl()->tile_manager()->InitializeTilesWithResourcesForTesting(
      all_tiles);

  RunEvictionQueueConstructAndIterateTest("32_100x100", 32,
                                          gfx::Rect(100, 100));
  RunEvictionQueueConstructAndIterateTest("32_500x500", 32,
                                          gfx::Rect(500, 500));
  RunEvictionQueueConstructAndIterateTest("64_100x100", 64,
                                          gfx::Rect(100, 100));
  RunEvictionQueueConstructAndIterateTest("64_500x500", 64,
                                          gfx::Rect(500, 500));
}

TEST_F(PictureLayerImplPerfTest, TilingSetEvictionQueueConstruct) {
  SetupPendingTree(gfx::Size(10000, 10000));

  float low_res_factor = host_impl()->settings().low_res_contents_scale_factor;

  std::vector<Tile*> all_tiles;
  AddTiling(low_res_factor, pending_layer_, &all_tiles);
  AddTiling(0.3f, pending_layer_, &all_tiles);
  AddTiling(0.7f, pending_layer_, &all_tiles);
  AddTiling(1.0f, pending_layer_, &all_tiles);
  AddTiling(2.0f, pending_layer_, &all_tiles);

  ASSERT_TRUE(host_impl()->tile_manager() != nullptr);
  host_impl()->tile_manager()->InitializeTilesWithResourcesForTesting(
      all_tiles);

  RunEvictionQueueConstructTest("0_0_100x100", gfx::Rect(0, 0, 100, 100));
  RunEvictionQueueConstructTest("5000_0_100x100", gfx::Rect(5000, 0, 100, 100));
  RunEvictionQueueConstructTest("9999_0_100x100", gfx::Rect(9999, 0, 100, 100));
}

}  // namespace
}  // namespace cc
