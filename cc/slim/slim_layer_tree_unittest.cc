// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "cc/slim/layer.h"
#include "cc/slim/solid_color_layer.h"
#include "cc/slim/surface_layer.h"
#include "cc/slim/test_frame_sink_impl.h"
#include "cc/slim/test_layer_tree_client.h"
#include "cc/slim/test_layer_tree_impl.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/common/surfaces/surface_range.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/overlay_transform.h"

namespace cc::slim {

namespace {

class SlimLayerTreeTest : public testing::Test {
 public:
  void SetUp() override {
    layer_tree_ = std::make_unique<TestLayerTreeImpl>(&client_);
  }

  void ExpectNeedsBeginFrameThenReset(
      const base::WeakPtr<TestFrameSinkImpl>& sink) {
    EXPECT_TRUE(layer_tree_->NeedsBeginFrames());
    EXPECT_TRUE(sink->needs_begin_frames());
    layer_tree_->ResetNeedsBeginFrame();
    EXPECT_FALSE(layer_tree_->NeedsBeginFrames());
    EXPECT_FALSE(sink->needs_begin_frames());
  }

  void BeginFrame(const base::WeakPtr<TestFrameSinkImpl>& sink) {
    base::TimeTicks frame_time = base::TimeTicks::Now();
    base::TimeDelta interval = viz::BeginFrameArgs::DefaultInterval();
    viz::BeginFrameArgs begin_frame_args = viz::BeginFrameArgs::Create(
        BEGINFRAME_FROM_HERE,
        /*source_id=*/1, ++sequence_id_, frame_time, frame_time + interval,
        interval, viz::BeginFrameArgs::NORMAL);
    sink->OnBeginFrame(begin_frame_args, /*timing_details=*/{},
                       /*frame_ack=*/false, {});
  }

  base::WeakPtr<TestFrameSinkImpl> SetupLayerTreeForDraw() {
    layer_tree_->SetVisible(true);
    auto frame_sink = TestFrameSinkImpl::Create();
    auto weak_frame_sink = frame_sink->GetWeakPtr();
    layer_tree_->SetFrameSink(std::move(frame_sink));
    EXPECT_TRUE(weak_frame_sink);

    gfx::Rect viewport(0, 0, 100, 100);
    float scale_factor = 2.0f;
    base::UnguessableToken token = base::UnguessableToken::Create();
    viz::LocalSurfaceId local_surface_id(1u, 2u, token);
    layer_tree_->SetViewportRectAndScale(viewport, scale_factor,
                                         local_surface_id);
    {
      auto solid_color_layer = SolidColorLayer::Create();
      solid_color_layer->SetBounds(viewport.size());
      solid_color_layer->SetBackgroundColor(SkColors::kRed);
      solid_color_layer->SetIsDrawable(true);
      layer_tree_->SetRoot(std::move(solid_color_layer));
    }

    return weak_frame_sink;
  }

 protected:
  TestLayerTreeClient client_;
  std::unique_ptr<TestLayerTreeImpl> layer_tree_;
  uint64_t sequence_id_ = 0;
};

TEST_F(SlimLayerTreeTest, SmokeTest) {
  EXPECT_TRUE(layer_tree_->GetUIResourceManager());

  gfx::Rect viewport(0, 0, 100, 100);
  float scale_factor = 2.0f;
  base::UnguessableToken token = base::UnguessableToken::Create();
  viz::LocalSurfaceId local_surface_id(1u, 2u, token);
  layer_tree_->SetViewportRectAndScale(viewport, scale_factor,
                                       local_surface_id);

  EXPECT_FALSE(layer_tree_->IsVisible());
  layer_tree_->SetVisible(true);
  EXPECT_TRUE(layer_tree_->IsVisible());

  layer_tree_->set_display_transform_hint(
      gfx::OVERLAY_TRANSFORM_FLIP_HORIZONTAL);
}

TEST_F(SlimLayerTreeTest, InitAndReleaseFrameSink) {
  EXPECT_EQ(client_.request_new_frame_sink_count(), 0u);
  layer_tree_->SetVisible(true);
  EXPECT_EQ(client_.request_new_frame_sink_count(), 1u);

  auto frame_sink = TestFrameSinkImpl::Create();
  auto weak_frame_sink = frame_sink->GetWeakPtr();
  frame_sink->SetBindToClientResult(true);
  layer_tree_->SetFrameSink(std::move(frame_sink));
  ASSERT_TRUE(weak_frame_sink);
  EXPECT_TRUE(weak_frame_sink->bind_to_client_called());
  EXPECT_EQ(client_.request_new_frame_sink_count(), 1u);
  EXPECT_EQ(client_.did_initialize_layer_tree_frame_sink_count(), 1u);
  EXPECT_EQ(client_.did_fail_to_initialize_layer_tree_frame_sink_count(), 0u);
  EXPECT_EQ(client_.did_lose_layer_tree_frame_sink_count(), 0u);

  layer_tree_->SetVisible(false);
  ASSERT_TRUE(weak_frame_sink);

  layer_tree_->ReleaseLayerTreeFrameSink();
  ASSERT_FALSE(weak_frame_sink);
  EXPECT_EQ(client_.request_new_frame_sink_count(), 1u);
  EXPECT_EQ(client_.did_initialize_layer_tree_frame_sink_count(), 1u);
  EXPECT_EQ(client_.did_fail_to_initialize_layer_tree_frame_sink_count(), 0u);
  EXPECT_EQ(client_.did_lose_layer_tree_frame_sink_count(), 0u);

  layer_tree_->SetVisible(true);
  EXPECT_EQ(client_.request_new_frame_sink_count(), 2u);
  EXPECT_EQ(client_.did_initialize_layer_tree_frame_sink_count(), 1u);
  EXPECT_EQ(client_.did_fail_to_initialize_layer_tree_frame_sink_count(), 0u);
  EXPECT_EQ(client_.did_lose_layer_tree_frame_sink_count(), 0u);
}

TEST_F(SlimLayerTreeTest, FrameSinkInitFailure) {
  EXPECT_EQ(client_.request_new_frame_sink_count(), 0u);
  layer_tree_->SetVisible(true);
  EXPECT_EQ(client_.request_new_frame_sink_count(), 1u);

  auto frame_sink = TestFrameSinkImpl::Create();
  auto weak_frame_sink = frame_sink->GetWeakPtr();
  frame_sink->SetBindToClientResult(false);
  layer_tree_->SetFrameSink(std::move(frame_sink));
  EXPECT_EQ(client_.request_new_frame_sink_count(), 1u);
  EXPECT_EQ(client_.did_initialize_layer_tree_frame_sink_count(), 0u);
  EXPECT_EQ(client_.did_fail_to_initialize_layer_tree_frame_sink_count(), 1u);
  EXPECT_EQ(client_.did_lose_layer_tree_frame_sink_count(), 0u);

  frame_sink = TestFrameSinkImpl::Create();
  weak_frame_sink = frame_sink->GetWeakPtr();
  frame_sink->SetBindToClientResult(true);
  layer_tree_->SetFrameSink(std::move(frame_sink));
  EXPECT_EQ(client_.request_new_frame_sink_count(), 1u);
  EXPECT_EQ(client_.did_initialize_layer_tree_frame_sink_count(), 1u);
  EXPECT_EQ(client_.did_fail_to_initialize_layer_tree_frame_sink_count(), 1u);
  EXPECT_EQ(client_.did_lose_layer_tree_frame_sink_count(), 0u);
}

TEST_F(SlimLayerTreeTest, LoseFrameSink) {
  EXPECT_EQ(client_.request_new_frame_sink_count(), 0u);
  layer_tree_->SetVisible(true);
  EXPECT_EQ(client_.request_new_frame_sink_count(), 1u);

  auto frame_sink = TestFrameSinkImpl::Create();
  auto weak_frame_sink = frame_sink->GetWeakPtr();
  layer_tree_->SetFrameSink(std::move(frame_sink));
  ASSERT_TRUE(weak_frame_sink);
  EXPECT_TRUE(weak_frame_sink->bind_to_client_called());

  weak_frame_sink->OnContextLost();
  ASSERT_FALSE(weak_frame_sink);
  EXPECT_EQ(client_.request_new_frame_sink_count(), 2u);

  frame_sink = TestFrameSinkImpl::Create();
  weak_frame_sink = frame_sink->GetWeakPtr();
  layer_tree_->SetFrameSink(std::move(frame_sink));
  ASSERT_TRUE(weak_frame_sink);
  EXPECT_TRUE(weak_frame_sink->bind_to_client_called());
}

TEST_F(SlimLayerTreeTest, NeedsBeginFrame) {
  layer_tree_->SetVisible(true);
  auto frame_sink = TestFrameSinkImpl::Create();
  auto weak_frame_sink = frame_sink->GetWeakPtr();
  layer_tree_->SetFrameSink(std::move(frame_sink));
  ASSERT_TRUE(weak_frame_sink);

  // Needs begin frame from SetVisible.
  ExpectNeedsBeginFrameThenReset(weak_frame_sink);

  gfx::Rect viewport(0, 0, 100, 100);
  float scale_factor = 2.0f;
  base::UnguessableToken token = base::UnguessableToken::Create();
  viz::LocalSurfaceId local_surface_id(1u, 2u, token);
  layer_tree_->SetViewportRectAndScale(viewport, scale_factor,
                                       local_surface_id);
  ExpectNeedsBeginFrameThenReset(weak_frame_sink);

  layer_tree_->SetNeedsAnimate();
  ExpectNeedsBeginFrameThenReset(weak_frame_sink);

  layer_tree_->SetNeedsAnimate();
  ExpectNeedsBeginFrameThenReset(weak_frame_sink);

  layer_tree_->set_background_color(SkColors::kGreen);
  ExpectNeedsBeginFrameThenReset(weak_frame_sink);

  auto root_layer = Layer::Create();
  layer_tree_->SetRoot(root_layer);
  ExpectNeedsBeginFrameThenReset(weak_frame_sink);

  auto child = Layer::Create();
  root_layer->AddChild(child);
  ExpectNeedsBeginFrameThenReset(weak_frame_sink);

  root_layer->SetBounds(viewport.size());
  ExpectNeedsBeginFrameThenReset(weak_frame_sink);

  child->RemoveFromParent();
  ExpectNeedsBeginFrameThenReset(weak_frame_sink);
}

TEST_F(SlimLayerTreeTest, NumUnneddedBeginFrameBeforeStop) {
  uint32_t n = 10;
  layer_tree_ = std::make_unique<TestLayerTreeImpl>(&client_, n);

  auto weak_frame_sink = SetupLayerTreeForDraw();
  ASSERT_TRUE(weak_frame_sink);

  // First begin frame should submit.
  BeginFrame(weak_frame_sink);
  EXPECT_TRUE(weak_frame_sink->GetDidSubmitAndReset());
  EXPECT_FALSE(weak_frame_sink->GetDidNotProduceFrameAndReset());
  weak_frame_sink->DidReceiveCompositorFrameAck({});
  EXPECT_TRUE(weak_frame_sink->needs_begin_frames());

  // All begin frame up to one less should not submit but keep requesting
  // begin frames.
  for (uint32_t i = 0; i < n - 1; ++i) {
    BeginFrame(weak_frame_sink);
    EXPECT_FALSE(weak_frame_sink->GetDidSubmitAndReset());
    EXPECT_TRUE(weak_frame_sink->GetDidNotProduceFrameAndReset());
    EXPECT_TRUE(weak_frame_sink->needs_begin_frames());
  }

  // Should stop requesting begin frame after last one.
  BeginFrame(weak_frame_sink);
  EXPECT_FALSE(weak_frame_sink->GetDidSubmitAndReset());
  EXPECT_TRUE(weak_frame_sink->GetDidNotProduceFrameAndReset());
  EXPECT_FALSE(weak_frame_sink->needs_begin_frames());
}

TEST_F(SlimLayerTreeTest, DeferBeginFrame) {
  auto defer_runnable = layer_tree_->DeferBeginFrame();

  layer_tree_->SetVisible(true);
  auto frame_sink = TestFrameSinkImpl::Create();
  auto weak_frame_sink = frame_sink->GetWeakPtr();
  layer_tree_->SetFrameSink(std::move(frame_sink));
  ASSERT_TRUE(weak_frame_sink);
  EXPECT_FALSE(layer_tree_->NeedsBeginFrames());
  EXPECT_FALSE(weak_frame_sink->needs_begin_frames());

  layer_tree_->set_background_color(SkColors::kGreen);
  EXPECT_FALSE(layer_tree_->NeedsBeginFrames());
  EXPECT_FALSE(weak_frame_sink->needs_begin_frames());

  std::move(defer_runnable).Run();
  EXPECT_TRUE(layer_tree_->NeedsBeginFrames());
  EXPECT_TRUE(weak_frame_sink->needs_begin_frames());
}

TEST_F(SlimLayerTreeTest, MaxPendingFrame) {
  auto weak_frame_sink = SetupLayerTreeForDraw();
  ASSERT_TRUE(weak_frame_sink);

  BeginFrame(weak_frame_sink);
  EXPECT_TRUE(weak_frame_sink->GetDidSubmitAndReset());
  EXPECT_FALSE(weak_frame_sink->GetDidNotProduceFrameAndReset());

  layer_tree_->SetNeedsAnimate();
  BeginFrame(weak_frame_sink);
  EXPECT_FALSE(weak_frame_sink->GetDidSubmitAndReset());
  EXPECT_TRUE(weak_frame_sink->GetDidNotProduceFrameAndReset());

  weak_frame_sink->DidReceiveCompositorFrameAck({});
  BeginFrame(weak_frame_sink);
  EXPECT_TRUE(weak_frame_sink->GetDidSubmitAndReset());
  EXPECT_FALSE(weak_frame_sink->GetDidNotProduceFrameAndReset());
}

TEST_F(SlimLayerTreeTest, MaxPendingResetWithFrameSink) {
  auto weak_frame_sink = SetupLayerTreeForDraw();
  ASSERT_TRUE(weak_frame_sink);

  BeginFrame(weak_frame_sink);
  EXPECT_TRUE(weak_frame_sink->GetDidSubmitAndReset());
  EXPECT_FALSE(weak_frame_sink->GetDidNotProduceFrameAndReset());

  layer_tree_->SetNeedsAnimate();
  BeginFrame(weak_frame_sink);
  EXPECT_FALSE(weak_frame_sink->GetDidSubmitAndReset());
  EXPECT_TRUE(weak_frame_sink->GetDidNotProduceFrameAndReset());

  // Release sink without ack-ing the previous frame.
  layer_tree_->SetVisible(false);
  layer_tree_->ReleaseLayerTreeFrameSink();
  EXPECT_FALSE(weak_frame_sink);

  layer_tree_->SetVisible(true);
  auto frame_sink = TestFrameSinkImpl::Create();
  weak_frame_sink = frame_sink->GetWeakPtr();
  layer_tree_->SetFrameSink(std::move(frame_sink));

  // Should allow producing frames with new sink.
  BeginFrame(weak_frame_sink);
  EXPECT_TRUE(weak_frame_sink->GetDidSubmitAndReset());
  EXPECT_FALSE(weak_frame_sink->GetDidNotProduceFrameAndReset());
}

TEST_F(SlimLayerTreeTest, ReferencedSurfaceRange) {
  scoped_refptr<SurfaceLayer> layer = SurfaceLayer::Create();
  base::UnguessableToken token = base::UnguessableToken::Create();
  viz::SurfaceId start(viz::FrameSinkId(1u, 2u),
                       viz::LocalSurfaceId(3u, 4u, token));
  viz::SurfaceId end(viz::FrameSinkId(1u, 2u),
                     viz::LocalSurfaceId(5u, 6u, token));
  layer->SetOldestAcceptableFallback(start);
  layer->SetSurfaceId(end, cc::DeadlinePolicy::UseDefaultDeadline());

  layer_tree_->SetRoot(layer);
  EXPECT_THAT(layer_tree_->referenced_surfaces(),
              testing::UnorderedElementsAre(
                  std::make_pair(viz::SurfaceRange(start, end), 1)));

  viz::SurfaceId new_end(viz::FrameSinkId(1u, 2u),
                         viz::LocalSurfaceId(7u, 8u, token));
  layer->SetSurfaceId(new_end, cc::DeadlinePolicy::UseDefaultDeadline());
  EXPECT_THAT(layer_tree_->referenced_surfaces(),
              testing::UnorderedElementsAre(
                  std::make_pair(viz::SurfaceRange(start, new_end), 1)));

  layer_tree_->SetRoot(nullptr);
  EXPECT_TRUE(layer_tree_->referenced_surfaces().empty());
}

TEST_F(SlimLayerTreeTest, DestroyTreeBeforeLayer) {
  // Regression test for use after free.
  auto root_layer = Layer::Create();

  // Use SurfaceLayer here because it accesses LayerTreeImpl pointer in
  // SetLayerTree.
  auto surface_layer = SurfaceLayer::Create();
  root_layer->AddChild(surface_layer);
  base::UnguessableToken token = base::UnguessableToken::Create();
  viz::SurfaceId end(viz::FrameSinkId(1u, 2u),
                     viz::LocalSurfaceId(5u, 6u, token));
  surface_layer->SetSurfaceId(end, cc::DeadlinePolicy::UseDefaultDeadline());

  layer_tree_->SetRoot(root_layer);

  layer_tree_.reset();

  EXPECT_EQ(root_layer->layer_tree(), nullptr);
  EXPECT_EQ(surface_layer->layer_tree(), nullptr);
}

TEST_F(SlimLayerTreeTest, NewLocalSurfaceIdForNewSink) {
  auto weak_frame_sink = SetupLayerTreeForDraw();
  viz::LocalSurfaceId old_id = weak_frame_sink->GetCurrentLocalSurfaceId();

  auto frame_sink = TestFrameSinkImpl::Create();
  weak_frame_sink = frame_sink->GetWeakPtr();
  layer_tree_->SetFrameSink(std::move(frame_sink));
  EXPECT_TRUE(weak_frame_sink);

  viz::LocalSurfaceId new_id = weak_frame_sink->GetCurrentLocalSurfaceId();
  EXPECT_NE(old_id, new_id);
  EXPECT_TRUE(new_id.IsNewerThan(old_id));
}

}  // namespace

}  // namespace cc::slim
