// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/layer.h"

#include "base/threading/thread_task_runner_handle.h"
#include "base/timer/lap_timer.h"
#include "cc/animation/animation_host.h"
#include "cc/test/fake_impl_task_runner_provider.h"
#include "cc/test/fake_layer_tree_host.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/stub_layer_tree_host_single_thread_client.h"
#include "cc/test/test_task_graph_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"

namespace cc {
namespace {

static const int kTimeLimitMillis = 3000;
static const int kWarmupRuns = 5;
static const int kTimeCheckInterval = 10;

class LayerPerfTest : public testing::Test {
 public:
  LayerPerfTest()
      : host_impl_(&task_runner_provider_, &task_graph_runner_),
        timer_(kWarmupRuns,
               base::TimeDelta::FromMilliseconds(kTimeLimitMillis),
               kTimeCheckInterval) {}

 protected:
  void SetUp() override {
    animation_host_ = AnimationHost::CreateForTesting(ThreadInstance::MAIN);
    layer_tree_host_ = FakeLayerTreeHost::Create(
        &fake_client_, &task_graph_runner_, animation_host_.get());
    layer_tree_host_->InitializeSingleThreaded(
        &single_thread_client_, base::ThreadTaskRunnerHandle::Get());
  }

  void TearDown() override {
    layer_tree_host_->SetRootLayer(nullptr);
    layer_tree_host_ = nullptr;
  }

  perf_test::PerfResultReporter SetUpReporter(
      const std::string& metric_basename,
      const std::string& story_name) {
    perf_test::PerfResultReporter reporter(metric_basename, story_name);
    reporter.RegisterImportantMetric("", "runs/s");
    return reporter;
  }

  FakeImplTaskRunnerProvider task_runner_provider_;
  TestTaskGraphRunner task_graph_runner_;
  FakeLayerTreeHostImpl host_impl_;

  StubLayerTreeHostSingleThreadClient single_thread_client_;
  FakeLayerTreeHostClient fake_client_;
  std::unique_ptr<AnimationHost> animation_host_;
  std::unique_ptr<FakeLayerTreeHost> layer_tree_host_;
  base::LapTimer timer_;
};

TEST_F(LayerPerfTest, PushPropertiesTo) {
  scoped_refptr<Layer> test_layer = Layer::Create();
  std::unique_ptr<LayerImpl> impl_layer =
      LayerImpl::Create(host_impl_.active_tree(), 1);

  layer_tree_host_->SetRootLayer(test_layer);

  float transform_origin_z = 0;
  bool scrollable = true;
  bool contents_opaque = true;
  bool double_sided = true;
  bool hide_layer_and_subtree = true;
  bool masks_to_bounds = true;

  // Properties changed.
  timer_.Reset();
  do {
    test_layer->SetNeedsDisplayRect(gfx::Rect(5, 5));
    test_layer->SetTransformOrigin(gfx::Point3F(0.f, 0.f, transform_origin_z));
    test_layer->SetContentsOpaque(contents_opaque);
    test_layer->SetDoubleSided(double_sided);
    test_layer->SetHideLayerAndSubtree(hide_layer_and_subtree);
    test_layer->SetMasksToBounds(masks_to_bounds);
    test_layer->PushPropertiesTo(impl_layer.get());

    transform_origin_z += 0.01f;
    scrollable = !scrollable;
    contents_opaque = !contents_opaque;
    double_sided = !double_sided;
    hide_layer_and_subtree = !hide_layer_and_subtree;
    masks_to_bounds = !masks_to_bounds;

    timer_.NextLap();
  } while (!timer_.HasTimeLimitExpired());

  perf_test::PerfResultReporter reporter =
      SetUpReporter("push_properties_to", "props_changed");
  reporter.AddResult("", timer_.LapsPerSecond());

  // Properties didn't change.
  timer_.Reset();
  do {
    test_layer->PushPropertiesTo(impl_layer.get());
    timer_.NextLap();
  } while (!timer_.HasTimeLimitExpired());

  reporter = SetUpReporter("push_properties_to", "props_didnt_change");
  reporter.AddResult("", timer_.LapsPerSecond());
}

TEST_F(LayerPerfTest, ImplPushPropertiesTo) {
  std::unique_ptr<LayerImpl> test_layer =
      LayerImpl::Create(host_impl_.active_tree(), 1);
  std::unique_ptr<LayerImpl> impl_layer =
      LayerImpl::Create(host_impl_.active_tree(), 2);

  SkColor background_color = SK_ColorRED;
  gfx::Size bounds(1000, 1000);
  bool draws_content = true;
  bool contents_opaque = true;
  bool masks_to_bounds = true;

  // Properties changed.
  timer_.Reset();
  do {
    test_layer->SetBackgroundColor(background_color);
    test_layer->SetSafeOpaqueBackgroundColor(background_color);
    test_layer->SetDrawsContent(draws_content);
    test_layer->SetContentsOpaque(contents_opaque);
    test_layer->SetMasksToBounds(masks_to_bounds);

    test_layer->PushPropertiesTo(impl_layer.get());

    background_color =
        background_color == SK_ColorRED ? SK_ColorGREEN : SK_ColorRED;
    bounds = bounds == gfx::Size(1000, 1000) ? gfx::Size(500, 500)
                                             : gfx::Size(1000, 1000);
    draws_content = !draws_content;
    contents_opaque = !contents_opaque;
    masks_to_bounds = !masks_to_bounds;

    timer_.NextLap();
  } while (!timer_.HasTimeLimitExpired());

  perf_test::PerfResultReporter reporter =
      SetUpReporter("impl_push_properties_to", "props_changed");
  reporter.AddResult("", timer_.LapsPerSecond());

  // Properties didn't change.
  timer_.Reset();
  do {
    test_layer->PushPropertiesTo(impl_layer.get());
    timer_.NextLap();
  } while (!timer_.HasTimeLimitExpired());

  reporter = SetUpReporter("impl_push_properties_to", "props_didnt_change");
  reporter.AddResult("", timer_.LapsPerSecond());
}

}  // namespace
}  // namespace cc
