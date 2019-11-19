// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/draw_property_utils.h"

#include <stddef.h>

#include <memory>
#include <sstream>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_piece.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "base/timer/lap_timer.h"
#include "cc/layers/layer.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/layer_tree_json_parser.h"
#include "cc/test/layer_tree_test.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/transform_node.h"
#include "components/viz/test/paths.h"
#include "testing/perf/perf_result_reporter.h"

namespace cc {
namespace {

static const int kTimeLimitMillis = 2000;
static const int kWarmupRuns = 5;
static const int kTimeCheckInterval = 10;

class DrawPropertyUtilsPerfTest : public LayerTreeTest {
 public:
  DrawPropertyUtilsPerfTest()
      : timer_(kWarmupRuns,
               base::TimeDelta::FromMilliseconds(kTimeLimitMillis),
               kTimeCheckInterval) {}

  void ReadTestFile(const std::string& name) {
    base::FilePath test_data_dir;
    ASSERT_TRUE(
        base::PathService::Get(viz::Paths::DIR_TEST_DATA, &test_data_dir));
    base::FilePath json_file = test_data_dir.AppendASCII(name + ".json");
    ASSERT_TRUE(base::ReadFileToString(json_file, &json_));
  }

  void SetupTree() override {
    gfx::Size viewport = gfx::Size(720, 1038);
    layer_tree_host()->SetViewportRectAndScale(gfx::Rect(viewport), 1.f,
                                               viz::LocalSurfaceIdAllocation());
    scoped_refptr<Layer> root =
        ParseTreeFromJson(json_, &content_layer_client_);
    ASSERT_TRUE(root.get());
    layer_tree_host()->SetRootLayer(root);
    content_layer_client_.set_bounds(viewport);
  }

  void SetUpReporter(const std::string& story_name) {
    reporter_ = std::make_unique<perf_test::PerfResultReporter>(
        "calc_draw_props_time", story_name);
    reporter_->RegisterImportantMetric("", "us");
  }

  void AfterTest() override {
    CHECK(reporter_) << "Must SetUpReporter() before TearDown().";
    reporter_->AddResult("", timer_.TimePerLap().InMicrosecondsF());
  }

 protected:
  FakeContentLayerClient content_layer_client_;
  base::LapTimer timer_;
  std::string json_;
  std::unique_ptr<perf_test::PerfResultReporter> reporter_;
};

class CalcDrawPropsTest : public DrawPropertyUtilsPerfTest {
 public:
  void RunCalcDrawProps() { RunTest(CompositorMode::SINGLE_THREADED); }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void DrawLayersOnThread(LayerTreeHostImpl* host_impl) override {
    timer_.Reset();

    do {
      RenderSurfaceList render_surface_list;
      draw_property_utils::CalculateDrawProperties(host_impl->active_tree(),
                                                   &render_surface_list);
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    EndTest();
  }
};

TEST_F(CalcDrawPropsTest, TenTen) {
  SetUpReporter("10_10");
  ReadTestFile("10_10_layer_tree");
  RunCalcDrawProps();
}

TEST_F(CalcDrawPropsTest, HeavyPage) {
  SetUpReporter("heavy_page");
  ReadTestFile("heavy_layer_tree");
  RunCalcDrawProps();
}

TEST_F(CalcDrawPropsTest, TouchRegionLight) {
  SetUpReporter("touch_region_light");
  ReadTestFile("touch_region_light");
  RunCalcDrawProps();
}

TEST_F(CalcDrawPropsTest, TouchRegionHeavy) {
  SetUpReporter("touch_region_heavy");
  ReadTestFile("touch_region_heavy");
  RunCalcDrawProps();
}

}  // namespace
}  // namespace cc
