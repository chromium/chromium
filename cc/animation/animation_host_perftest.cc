// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "cc/animation/animation_host.h"

#include "base/task/single_thread_task_runner.h"
#include "base/timer/lap_timer.h"
#include "cc/animation/animation.h"
#include "cc/animation/animation_id_provider.h"
#include "cc/animation/animation_timeline.h"
#include "cc/test/fake_impl_task_runner_provider.h"
#include "cc/test/fake_layer_tree_host.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/stub_layer_tree_host_single_thread_client.h"
#include "cc/test/test_task_graph_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"

namespace cc {

class AnimationHostPerfTest : public testing::Test {
 protected:
  AnimationHostPerfTest() = default;

  void SetUp() override {
    LayerTreeSettings settings;
    animation_host_ = AnimationHost::CreateForTesting(ThreadInstance::kMain);
    layer_tree_host_ = FakeLayerTreeHost::Create(
        &fake_client_, &task_graph_runner_, animation_host_.get(), settings);
    layer_tree_host_->InitializeSingleThreaded(
        &single_thread_client_,
        base::SingleThreadTaskRunner::GetCurrentDefault());

    root_layer_ = Layer::Create();
    layer_tree_host_->SetRootLayer(root_layer_);

    root_layer_impl_ = layer_tree_host_->CommitToActiveTree();
  }

  void TearDown() override {
    root_layer_ = nullptr;
    root_layer_impl_ = nullptr;

    layer_tree_host_->SetRootLayer(nullptr);
    layer_tree_host_ = nullptr;
  }

  AnimationHost* host() const { return animation_host_.get(); }
  AnimationHost* host_impl() const {
    return layer_tree_host_->host_impl()->animation_host();
  }

  void CreateAnimations(int num_animations) {
    all_animations_timeline_ =
        AnimationTimeline::Create(AnimationIdProvider::NextTimelineId());
    host()->AddAnimationTimeline(all_animations_timeline_);

    first_animation_id_ = AnimationIdProvider::NextAnimationId();
    last_animation_id_ = first_animation_id_;

    for (int i = 0; i < num_animations; ++i) {
      scoped_refptr<Layer> layer = Layer::Create();
      root_layer_->AddChild(layer);
      layer->SetElementId(LayerIdToElementIdForTesting(layer->id()));

      scoped_refptr<Animation> animation =
          Animation::Create(last_animation_id_);
      last_animation_id_ = AnimationIdProvider::NextAnimationId();

      all_animations_timeline_->AttachAnimation(animation);
      animation->AttachElement(layer->element_id());
      EXPECT_TRUE(animation->element_animations());
    }

    // Create impl animations.
    layer_tree_host_->CommitToActiveTree();

    // Check impl instances created.
    scoped_refptr<AnimationTimeline> timeline_impl =
        host_impl()->GetTimelineById(all_animations_timeline_->id());
    EXPECT_TRUE(timeline_impl);
    for (int i = first_animation_id_; i < last_animation_id_; ++i)
      EXPECT_TRUE(timeline_impl->GetAnimationById(i));
  }

  void CreateTimelines(int num_timelines) {
    first_timeline_id_ = AnimationIdProvider::NextTimelineId();
    last_timeline_id_ = first_timeline_id_;

    for (int i = 0; i < num_timelines; ++i) {
      scoped_refptr<AnimationTimeline> timeline =
          AnimationTimeline::Create(last_timeline_id_);
      last_timeline_id_ = AnimationIdProvider::NextTimelineId();
      host()->AddAnimationTimeline(timeline);
    }

    // Create impl timelines.
    layer_tree_host_->CommitToActiveTree();

    // Check impl instances created.
    for (int i = first_timeline_id_; i < last_timeline_id_; ++i)
      EXPECT_TRUE(host_impl()->GetTimelineById(i));
  }

  void SetAllTimelinesNeedPushProperties() const {
    for (int i = first_timeline_id_; i < last_timeline_id_; ++i)
      host_impl()->GetTimelineById(i)->SetNeedsPushProperties();
  }

  void SetAllAnimationsNeedPushProperties() const {
    for (int i = first_animation_id_; i < last_animation_id_; ++i)
      all_animations_timeline_->GetAnimationById(i)->SetNeedsPushProperties();
  }

  void DoTest(const std::string& test_name) {
    PropertyTrees property_trees(*host());
    timer_.Reset();
    do {
      // Invalidate dirty flags.
      SetAllTimelinesNeedPushProperties();
      SetAllAnimationsNeedPushProperties();
      host()->PushPropertiesTo(host_impl(), property_trees);
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    perf_test::PerfResultReporter reporter("push_properties_to", test_name);
    reporter.RegisterImportantMetric("", "runs/s");
    reporter.AddResult("", timer_.LapsPerSecond());
  }

 private:
  StubLayerTreeHostSingleThreadClient single_thread_client_;
  FakeLayerTreeHostClient fake_client_;
  std::unique_ptr<AnimationHost> animation_host_;
  std::unique_ptr<FakeLayerTreeHost> layer_tree_host_;
  scoped_refptr<Layer> root_layer_;
  raw_ptr<LayerImpl> root_layer_impl_ = nullptr;
  scoped_refptr<AnimationTimeline> all_animations_timeline_;

  int first_timeline_id_ = 0;
  int last_timeline_id_ = 0;

  int first_animation_id_ = 0;
  int last_animation_id_ = 0;

  base::LapTimer timer_;
  TestTaskGraphRunner task_graph_runner_;
};

TEST_F(AnimationHostPerfTest, Push1000AnimationsPropertiesTo) {
  CreateAnimations(1000);
  DoTest("Push1000AnimationsPropertiesTo");
}

TEST_F(AnimationHostPerfTest, Push10TimelinesPropertiesTo) {
  CreateTimelines(10);
  DoTest("Push10TimelinesPropertiesTo");
}

TEST_F(AnimationHostPerfTest, Push1000TimelinesPropertiesTo) {
  CreateTimelines(1000);
  DoTest("Push1000TimelinesPropertiesTo");
}

}  // namespace cc
