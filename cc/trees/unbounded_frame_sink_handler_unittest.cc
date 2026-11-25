// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/unbounded_frame_sink_handler.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/unguessable_token.h"
#include "cc/test/fake_impl_task_runner_provider.h"
#include "cc/test/fake_layer_tree_frame_sink.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/layer_test_common.h"
#include "cc/test/test_task_graph_runner.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/test/compositor_frame_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {
namespace {

viz::LocalSurfaceId MakeId(uint32_t parent_sequence_number) {
  return viz::LocalSurfaceId(parent_sequence_number, 1,
                             base::UnguessableToken::CreateForTesting(1u, 2u));
}

LayerTreeSettings UnboundedSettings() {
  LayerTreeSettings settings = CommitToPendingTreeLayerTreeSettings();
  settings.enable_unbounded_element = true;
  return settings;
}

class UnboundedFrameSinkHandlerTest : public testing::Test {
 protected:
  void SetFrameSink(const viz::LocalSurfaceId& id) {
    auto sink = FakeLayerTreeFrameSink::CreateSoftware();
    sink_ = sink.get();
    handler_.SetFrameSink(std::move(sink), id);
  }

  // Submits a frame whose root pass has `output_rect` and `damage_rect`, and
  // returns the root pass as the frame sink received it.
  const viz::CompositorRenderPass& Submit(const gfx::Rect& output_rect,
                                          const gfx::Rect& damage_rect) {
    handler_.SubmitFrame(viz::CompositorFrameBuilder()
                             .AddRenderPass(output_rect, damage_rect)
                             .Build());
    return *sink_->last_sent_frame()->render_pass_list.back();
  }

  TestTaskGraphRunner task_graph_runner_;
  FakeImplTaskRunnerProvider task_runner_provider_;
  FakeLayerTreeHostImpl host_impl_{UnboundedSettings(), &task_runner_provider_,
                                   &task_graph_runner_};
  UnboundedFrameSinkHandler handler_{&host_impl_};
  raw_ptr<FakeLayerTreeFrameSink> sink_ = nullptr;
};

TEST_F(UnboundedFrameSinkHandlerTest, NewLocalSurfaceIdForcesFullDamage) {
  SetFrameSink(MakeId(1));
  const viz::CompositorRenderPass& first =
      Submit(gfx::Rect(50, 50), gfx::Rect(10, 10, 5, 5));
  EXPECT_EQ(gfx::Rect(50, 50), first.damage_rect);
  EXPECT_TRUE(first.has_damage_from_contributing_content);

  // Viz keeps the previous surface's pixels and only redraws this frame's
  // damage, so the first frame under a new id must not trust cc's damage.
  handler_.SetLocalSurfaceId(MakeId(2));
  const viz::CompositorRenderPass& second =
      Submit(gfx::Rect(50, 50), gfx::Rect());
  EXPECT_EQ(gfx::Rect(50, 50), second.damage_rect);
  EXPECT_TRUE(second.has_damage_from_contributing_content);
}

TEST_F(UnboundedFrameSinkHandlerTest, SameIdAndSizeKeepsPartialDamage) {
  SetFrameSink(MakeId(1));
  Submit(gfx::Rect(50, 50), gfx::Rect(50, 50));

  const viz::CompositorRenderPass& pass =
      Submit(gfx::Rect(50, 50), gfx::Rect(10, 10, 20, 20));
  EXPECT_EQ(gfx::Rect(50, 50), pass.output_rect);
  EXPECT_EQ(gfx::Rect(10, 10, 20, 20), pass.damage_rect);
}

TEST_F(UnboundedFrameSinkHandlerTest, GrowUnderSameIdIsClampedAndFullyDamaged) {
  SetFrameSink(MakeId(1));
  Submit(gfx::Rect(50, 50), gfx::Rect(50, 50));

  // Viz rejects a size change under the same id, so the frame keeps the
  // previous size until a new id arrives.
  const viz::CompositorRenderPass& clamped =
      Submit(gfx::Rect(50, 120), gfx::Rect(0, 50, 50, 70));
  EXPECT_EQ(gfx::Rect(50, 50), clamped.output_rect);
  EXPECT_EQ(gfx::Rect(50, 50), clamped.damage_rect);
  EXPECT_TRUE(clamped.has_damage_from_contributing_content);

  handler_.SetLocalSurfaceId(MakeId(2));
  const viz::CompositorRenderPass& resized =
      Submit(gfx::Rect(50, 120), gfx::Rect());
  EXPECT_EQ(gfx::Rect(50, 120), resized.output_rect);
  EXPECT_EQ(gfx::Rect(50, 120), resized.damage_rect);
}

TEST_F(UnboundedFrameSinkHandlerTest, ShrinkUnderSameIdDamagesClampedRect) {
  SetFrameSink(MakeId(1));
  Submit(gfx::Rect(50, 120), gfx::Rect(50, 120));

  // cc computed the damage for the 50x50 surface, but the frame is clamped
  // back to 50x120: the strip below 50 must be damaged too.
  const viz::CompositorRenderPass& pass =
      Submit(gfx::Rect(50, 50), gfx::Rect(50, 50));
  EXPECT_EQ(gfx::Rect(50, 120), pass.output_rect);
  EXPECT_EQ(gfx::Rect(50, 120), pass.damage_rect);
  EXPECT_TRUE(pass.has_damage_from_contributing_content);
}

TEST_F(UnboundedFrameSinkHandlerTest, ClampedGrowKeepsDamageUntilSizeChanges) {
  SetFrameSink(MakeId(1));
  Submit(gfx::Rect(50, 50), gfx::Rect(50, 50));
  Submit(gfx::Rect(50, 120), gfx::Rect(50, 120));

  // Later frames at the same size under the same id stay clamped and keep
  // cc's damage.
  const viz::CompositorRenderPass& same_size =
      Submit(gfx::Rect(50, 120), gfx::Rect(10, 10, 20, 20));
  EXPECT_EQ(gfx::Rect(50, 50), same_size.output_rect);
  EXPECT_EQ(gfx::Rect(10, 10, 20, 20), same_size.damage_rect);

  // Another size change under the same id is fully damaged again.
  const viz::CompositorRenderPass& resized =
      Submit(gfx::Rect(50, 150), gfx::Rect(10, 10, 20, 20));
  EXPECT_EQ(gfx::Rect(50, 50), resized.output_rect);
  EXPECT_EQ(gfx::Rect(50, 50), resized.damage_rect);
  EXPECT_TRUE(resized.has_damage_from_contributing_content);
}

TEST_F(UnboundedFrameSinkHandlerTest,
       ClampedShrinkKeepsDamageUntilSizeChanges) {
  SetFrameSink(MakeId(1));
  Submit(gfx::Rect(50, 120), gfx::Rect(50, 120));
  Submit(gfx::Rect(50, 50), gfx::Rect(50, 50));

  // The previous frame damaged the strip below 50, so later frames at the
  // same size under the same id keep cc's damage.
  const viz::CompositorRenderPass& same_size =
      Submit(gfx::Rect(50, 50), gfx::Rect(10, 10, 20, 20));
  EXPECT_EQ(gfx::Rect(50, 120), same_size.output_rect);
  EXPECT_EQ(gfx::Rect(10, 10, 20, 20), same_size.damage_rect);

  // Another size change under the same id is fully damaged again.
  const viz::CompositorRenderPass& resized =
      Submit(gfx::Rect(50, 80), gfx::Rect(10, 10, 20, 20));
  EXPECT_EQ(gfx::Rect(50, 120), resized.output_rect);
  EXPECT_EQ(gfx::Rect(50, 120), resized.damage_rect);
  EXPECT_TRUE(resized.has_damage_from_contributing_content);
}

TEST_F(UnboundedFrameSinkHandlerTest, NewFrameSinkStartsFresh) {
  SetFrameSink(MakeId(1));
  Submit(gfx::Rect(50, 50), gfx::Rect(50, 50));

  // A new frame sink with the same id must neither clamp to the old sink's
  // size nor keep partial damage on its first frame.
  SetFrameSink(MakeId(1));
  const viz::CompositorRenderPass& pass =
      Submit(gfx::Rect(50, 80), gfx::Rect(10, 10, 5, 5));
  EXPECT_EQ(gfx::Rect(50, 80), pass.output_rect);
  EXPECT_EQ(gfx::Rect(50, 80), pass.damage_rect);
}

}  // namespace
}  // namespace cc
