// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <iostream>
#include <set>
#include <vector>

#include "base/location.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "cc/animation/animation_host.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/layers/surface_layer.h"
#include "cc/layers/surface_layer_impl.h"
#include "cc/test/fake_impl_task_runner_provider.h"
#include "cc/test/fake_layer_tree_host.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/layer_tree_test.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/trees/layer_tree_host.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

using testing::_;
using testing::Eq;
using testing::ElementsAre;
using testing::SizeIs;

constexpr viz::FrameSinkId kArbitraryFrameSinkId(1, 1);

class SurfaceLayerTest : public testing::Test {
 public:
  SurfaceLayerTest()
      : host_impl_(&task_runner_provider_, &task_graph_runner_) {}

  // Synchronizes |layer_tree_host_| and |host_impl_| and pushes surface ids.
  void SynchronizeTrees() {
    TreeSynchronizer::PushLayerProperties(layer_tree_host_.get(),
                                          host_impl_.pending_tree());
    layer_tree_host_->PushSurfaceRangesTo(host_impl_.pending_tree());
  }

 protected:
  void SetUp() override {
    animation_host_ = AnimationHost::CreateForTesting(ThreadInstance::MAIN);
    layer_tree_host_ = FakeLayerTreeHost::Create(
        &fake_client_, &task_graph_runner_, animation_host_.get());
    layer_tree_host_->SetViewportRectAndScale(gfx::Rect(10, 10), 1.f,
                                              viz::LocalSurfaceIdAllocation());
    host_impl_.CreatePendingTree();
  }

  void TearDown() override {
    if (layer_tree_host_) {
      layer_tree_host_->SetRootLayer(nullptr);
      layer_tree_host_ = nullptr;
    }
  }

  FakeLayerTreeHostClient fake_client_;
  FakeImplTaskRunnerProvider task_runner_provider_;
  TestTaskGraphRunner task_graph_runner_;
  std::unique_ptr<AnimationHost> animation_host_;
  std::unique_ptr<FakeLayerTreeHost> layer_tree_host_;
  FakeLayerTreeHostImpl host_impl_;
};

// This test verifies that if UseExistingDeadline() is used on a new
// SurfaceLayer then the deadline will be 0 frames.
TEST_F(SurfaceLayerTest, UseExistingDeadlineForNewSurfaceLayer) {
  scoped_refptr<SurfaceLayer> layer = SurfaceLayer::Create();
  layer_tree_host_->SetRootLayer(layer);
  viz::SurfaceId primary_id(
      kArbitraryFrameSinkId,
      viz::LocalSurfaceId(1, base::UnguessableToken::Create()));
  layer->SetSurfaceId(primary_id, DeadlinePolicy::UseExistingDeadline());
  EXPECT_EQ(0u, layer->deadline_in_frames());
}

// This test verifies that if UseInfiniteDeadline() is used on a new
// SurfaceLayer then the deadline will be max number of frames.
TEST_F(SurfaceLayerTest, UseInfiniteDeadlineForNewSurfaceLayer) {
  scoped_refptr<SurfaceLayer> layer = SurfaceLayer::Create();
  layer_tree_host_->SetRootLayer(layer);
  viz::SurfaceId primary_id(
      kArbitraryFrameSinkId,
      viz::LocalSurfaceId(1, base::UnguessableToken::Create()));
  layer->SetSurfaceId(primary_id, DeadlinePolicy::UseInfiniteDeadline());
  EXPECT_EQ(std::numeric_limits<uint32_t>::max(), layer->deadline_in_frames());
}

// This test verifies that if an invalid primary surface ID is set then the
// deadline will be reset to 0 frames.
TEST_F(SurfaceLayerTest, ResetDeadlineOnInvalidSurfaceId) {
  scoped_refptr<SurfaceLayer> layer = SurfaceLayer::Create();
  layer_tree_host_->SetRootLayer(layer);
  viz::SurfaceId primary_id(
      kArbitraryFrameSinkId,
      viz::LocalSurfaceId(1, base::UnguessableToken::Create()));
  layer->SetSurfaceId(primary_id, DeadlinePolicy::UseSpecifiedDeadline(3u));
  EXPECT_EQ(3u, layer->deadline_in_frames());

  // Reset the surface layer to an invalid SurfaceId. Verify that the deadline
  // is reset.
  layer->SetSurfaceId(viz::SurfaceId(),
                      DeadlinePolicy::UseSpecifiedDeadline(3u));
  EXPECT_EQ(0u, layer->deadline_in_frames());
}

// This test verifies that SurfaceLayer properties are pushed across to
// SurfaceLayerImpl.
TEST_F(SurfaceLayerTest, PushProperties) {
  scoped_refptr<SurfaceLayer> layer = SurfaceLayer::Create();
  layer_tree_host_->SetRootLayer(layer);
  viz::SurfaceId primary_id(
      kArbitraryFrameSinkId,
      viz::LocalSurfaceId(1, base::UnguessableToken::Create()));
  layer->SetSurfaceId(primary_id, DeadlinePolicy::UseSpecifiedDeadline(1u));
  layer->SetSurfaceId(primary_id, DeadlinePolicy::UseSpecifiedDeadline(2u));
  layer->SetSurfaceId(primary_id, DeadlinePolicy::UseExistingDeadline());
  layer->SetOldestAcceptableFallback(primary_id);
  layer->SetBackgroundColor(SK_ColorBLUE);
  layer->SetStretchContentToFillBounds(true);

  EXPECT_TRUE(layer_tree_host_->needs_surface_ranges_sync());
  EXPECT_EQ(layer_tree_host_->SurfaceRanges().size(), 1u);

  // Verify that pending tree has no surface ids already.
  EXPECT_FALSE(host_impl_.pending_tree()->needs_surface_ranges_sync());
  EXPECT_EQ(host_impl_.pending_tree()->SurfaceRanges().size(), 0u);

  std::unique_ptr<SurfaceLayerImpl> layer_impl =
      SurfaceLayerImpl::Create(host_impl_.pending_tree(), layer->id());
  SynchronizeTrees();

  // Verify that pending tree received the surface id and also has
  // needs_surface_ranges_sync set to true as it needs to sync with active tree.
  EXPECT_TRUE(host_impl_.pending_tree()->needs_surface_ranges_sync());
  EXPECT_EQ(host_impl_.pending_tree()->SurfaceRanges().size(), 1u);

  // Verify we have reset the state on layer tree host.
  EXPECT_FALSE(layer_tree_host_->needs_surface_ranges_sync());

  // Verify that the primary and fallback SurfaceIds are pushed through.
  EXPECT_EQ(primary_id, layer_impl->range().end());
  EXPECT_EQ(primary_id, layer_impl->range().start());
  EXPECT_EQ(SK_ColorBLUE, layer_impl->background_color());
  EXPECT_TRUE(layer_impl->stretch_content_to_fill_bounds());
  EXPECT_EQ(2u, layer_impl->deadline_in_frames());

  viz::SurfaceId fallback_id(
      kArbitraryFrameSinkId,
      viz::LocalSurfaceId(2, base::UnguessableToken::Create()));
  layer->SetOldestAcceptableFallback(fallback_id);
  layer->SetSurfaceId(fallback_id, DeadlinePolicy::UseExistingDeadline());
  layer->SetBackgroundColor(SK_ColorGREEN);
  layer->SetStretchContentToFillBounds(false);

  // Verify that fallback surface id is not recorded on the layer tree host as
  // surface synchronization is not enabled.
  EXPECT_TRUE(layer_tree_host_->needs_surface_ranges_sync());
  EXPECT_EQ(layer_tree_host_->SurfaceRanges().size(), 1u);

  SynchronizeTrees();

  EXPECT_EQ(host_impl_.pending_tree()->SurfaceRanges().size(), 1u);

  // Verify that the primary viz::SurfaceId stays the same and the new
  // fallback viz::SurfaceId is pushed through.
  EXPECT_EQ(fallback_id, layer_impl->range().end());
  EXPECT_EQ(fallback_id, layer_impl->range().start());
  EXPECT_EQ(SK_ColorGREEN, layer_impl->background_color());
  // The deadline resets back to 0 (no deadline) after the first commit.
  EXPECT_EQ(0u, layer_impl->deadline_in_frames());
  EXPECT_FALSE(layer_impl->stretch_content_to_fill_bounds());
}

// This test verifies the list of surface ids is correct when there are cloned
// surface layers. This emulates the flow of maximize and minimize animations on
// Chrome OS.
TEST_F(SurfaceLayerTest, CheckSurfaceReferencesForClonedLayer) {
  const viz::SurfaceId old_surface_id(
      kArbitraryFrameSinkId,
      viz::LocalSurfaceId(1, base::UnguessableToken::Create()));

  // This layer will always contain the old surface id and will be deleted when
  // animation is done.
  scoped_refptr<SurfaceLayer> layer1 = SurfaceLayer::Create();
  layer1->SetLayerTreeHost(layer_tree_host_.get());
  layer1->SetSurfaceId(old_surface_id, DeadlinePolicy::UseDefaultDeadline());
  layer1->SetOldestAcceptableFallback(old_surface_id);

  // This layer will eventually be switched be switched to show the new surface
  // id and will be retained when animation is done.
  scoped_refptr<SurfaceLayer> layer2 = SurfaceLayer::Create();
  layer2->SetLayerTreeHost(layer_tree_host_.get());
  layer2->SetSurfaceId(old_surface_id, DeadlinePolicy::UseDefaultDeadline());
  layer2->SetOldestAcceptableFallback(old_surface_id);

  std::unique_ptr<SurfaceLayerImpl> layer_impl1 =
      SurfaceLayerImpl::Create(host_impl_.pending_tree(), layer1->id());
  std::unique_ptr<SurfaceLayerImpl> layer_impl2 =
      SurfaceLayerImpl::Create(host_impl_.pending_tree(), layer2->id());

  SynchronizeTrees();

  // Verify that only |old_surface_id| is going to be referenced.
  EXPECT_THAT(layer_tree_host_->SurfaceRanges(),
              ElementsAre(viz::SurfaceRange(old_surface_id)));
  EXPECT_THAT(host_impl_.pending_tree()->SurfaceRanges(),
              ElementsAre(viz::SurfaceRange(old_surface_id)));

  const viz::SurfaceId new_surface_id(
      kArbitraryFrameSinkId,
      viz::LocalSurfaceId(2, base::UnguessableToken::Create()));

  // Switch the new layer to use |new_surface_id|.
  layer2->SetSurfaceId(new_surface_id, DeadlinePolicy::UseDefaultDeadline());
  layer2->SetOldestAcceptableFallback(new_surface_id);

  SynchronizeTrees();

  // Verify that both surface ids are going to be referenced.
  EXPECT_THAT(layer_tree_host_->SurfaceRanges(),
              ElementsAre(viz::SurfaceRange(old_surface_id),
                          viz::SurfaceRange(new_surface_id)));
  EXPECT_THAT(host_impl_.pending_tree()->SurfaceRanges(),
              ElementsAre(viz::SurfaceRange(old_surface_id),
                          viz::SurfaceRange(new_surface_id)));

  // Unparent the old layer like it's being destroyed at the end of animation.
  layer1->SetLayerTreeHost(nullptr);

  SynchronizeTrees();

  // Verify that only |new_surface_id| is going to be referenced.
  EXPECT_THAT(layer_tree_host_->SurfaceRanges(),
              ElementsAre(viz::SurfaceRange(new_surface_id)));
  EXPECT_THAT(host_impl_.pending_tree()->SurfaceRanges(),
              ElementsAre(viz::SurfaceRange(new_surface_id)));

  // Cleanup for destruction.
  layer2->SetLayerTreeHost(nullptr);
}

// This test verifies LayerTreeHost::needs_surface_ranges_sync() is correct when
// there are cloned surface layers.
TEST_F(SurfaceLayerTest, CheckNeedsSurfaceIdsSyncForClonedLayers) {
  const viz::SurfaceId surface_id(
      kArbitraryFrameSinkId,
      viz::LocalSurfaceId(1, base::UnguessableToken::Create()));

  scoped_refptr<SurfaceLayer> layer1 = SurfaceLayer::Create();
  layer1->SetLayerTreeHost(layer_tree_host_.get());
  layer1->SetSurfaceId(surface_id, DeadlinePolicy::UseDefaultDeadline());
  layer1->SetOldestAcceptableFallback(surface_id);

  // Verify the surface id is in SurfaceLayerIds() and
  // needs_surface_ranges_sync() is true.
  EXPECT_TRUE(layer_tree_host_->needs_surface_ranges_sync());
  EXPECT_THAT(layer_tree_host_->SurfaceRanges(), SizeIs(1));

  std::unique_ptr<SurfaceLayerImpl> layer_impl1 =
      SurfaceLayerImpl::Create(host_impl_.pending_tree(), layer1->id());
  SynchronizeTrees();

  // After syncchronizing trees verify needs_surface_ranges_sync() is false.
  EXPECT_FALSE(layer_tree_host_->needs_surface_ranges_sync());

  // Create the second layer that is a clone of the first.
  scoped_refptr<SurfaceLayer> layer2 = SurfaceLayer::Create();
  layer2->SetLayerTreeHost(layer_tree_host_.get());
  layer2->SetSurfaceId(surface_id, DeadlinePolicy::UseDefaultDeadline());
  layer2->SetOldestAcceptableFallback(surface_id);

  // Verify that after creating the second layer with the same surface id that
  // needs_surface_ranges_sync() is still false.
  EXPECT_TRUE(layer_tree_host_->needs_surface_ranges_sync());
  EXPECT_THAT(layer_tree_host_->SurfaceRanges(), SizeIs(1));

  std::unique_ptr<SurfaceLayerImpl> layer_impl2 =
      SurfaceLayerImpl::Create(host_impl_.pending_tree(), layer2->id());
  SynchronizeTrees();

  // Verify needs_surface_ranges_sync() is still false after synchronizing
  // trees.
  EXPECT_FALSE(layer_tree_host_->needs_surface_ranges_sync());

  // Destroy one of the layers, leaving one layer with the surface id.
  layer1->SetLayerTreeHost(nullptr);

  // Verify needs_surface_ranges_sync() is still false.
  EXPECT_FALSE(layer_tree_host_->needs_surface_ranges_sync());

  // Destroy the last layer, this should change the set of layer surface ids.
  layer2->SetLayerTreeHost(nullptr);

  // Verify SurfaceLayerIds() is empty and needs_surface_ranges_sync() is true.
  EXPECT_TRUE(layer_tree_host_->needs_surface_ranges_sync());
  EXPECT_THAT(layer_tree_host_->SurfaceRanges(), SizeIs(0));
}

}  // namespace
}  // namespace cc
