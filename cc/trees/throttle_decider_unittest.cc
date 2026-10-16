// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "cc/trees/throttle_decider.h"

#include <vector>

#include "base/memory/raw_ref.h"
#include "components/viz/common/quads/compositor_render_pass_draw_quad.h"
#include "components/viz/common/quads/surface_draw_quad.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace cc {

class ThrottleDeciderTest : public ::testing::Test {
 protected:
  struct RenderPassInfo {
    RenderPassInfo(const viz::CompositorRenderPass& render_pass,
                   bool has_foreground_blur)
        : render_pass(render_pass), has_foreground_blur(has_foreground_blur) {}

    const raw_ref<const viz::CompositorRenderPass> render_pass;
    bool has_foreground_blur;
  };

  void RunThrottleDecider(const std::vector<RenderPassInfo>& render_passes) {
    throttle_decider_.Prepare();
    for (const auto& render_pass : render_passes) {
      throttle_decider_.ProcessRenderPass(*render_pass.render_pass,
                                          render_pass.has_foreground_blur);
    }
  }
  const base::flat_set<viz::FrameSinkId>& GetFrameSinksToThrottle() const {
    return throttle_decider_.ids();
  }
  ThrottleDecider throttle_decider_;
};

TEST_F(ThrottleDeciderTest, BackdropFilter) {
  // Create two render passes. The first render pass has a blur backdrop filter.
  // The second render pass has two quads: a RPDQ referencing the first render
  // pass and a surface quad.
  viz::CompositorRenderPassList render_passes;
  render_passes.push_back(viz::CompositorRenderPass::Create());
  render_passes.push_back(viz::CompositorRenderPass::Create());
  render_passes.push_back(viz::CompositorRenderPass::Create());

  gfx::Rect render_pass_rect(0, 0, 100, 100);
  gfx::Rect quad_rect(0, 0, 100, 100);
  viz::CompositorRenderPassId id1{1u};
  viz::CompositorRenderPassId id2{2u};
  viz::CompositorRenderPassId id3{3u};

  render_passes[0]->SetNew(id1, render_pass_rect, gfx::Rect(),
                           gfx::Transform());
  render_passes[1]->SetNew(id2, render_pass_rect, gfx::Rect(),
                           gfx::Transform());

  auto* rpdq =
      render_passes[1]
          ->CreateAndAppendDrawQuad<viz::CompositorRenderPassDrawQuad>();
  rpdq->material = viz::DrawQuad::Material::kCompositorRenderPass;
  rpdq->render_pass_id = id1;
  viz::SharedQuadState* sqs1 =
      render_passes[1]->CreateAndAppendSharedQuadState();
  rpdq->shared_quad_state = sqs1;
  rpdq->rect = quad_rect;
  rpdq->backdrop_filters.Append(FilterOperation::CreateBlurFilter(5.0));

  viz::FrameSinkId frame_sink_id{10, 10};
  auto* surface_quad =
      render_passes[1]->CreateAndAppendDrawQuad<viz::SurfaceDrawQuad>();
  viz::SharedQuadState* sqs3 =
      render_passes[1]->CreateAndAppendSharedQuadState();
  surface_quad->shared_quad_state = sqs3;
  surface_quad->material = viz::DrawQuad::Material::kSurfaceContent;
  surface_quad->surface_range = viz::SurfaceRange(
      std::nullopt,
      viz::SurfaceId(frame_sink_id, viz::LocalSurfaceId(
                                        1u, base::UnguessableToken::Create())));
  surface_quad->rect = quad_rect;
  surface_quad->visible_rect = quad_rect;

  // This will embed the render pass with id2
  render_passes[2]->SetNew(id3, render_pass_rect, gfx::Rect(),
                           gfx::Transform());
  auto* rpdq2 =
      render_passes[2]
          ->CreateAndAppendDrawQuad<viz::CompositorRenderPassDrawQuad>();
  rpdq2->material = viz::DrawQuad::Material::kCompositorRenderPass;
  rpdq2->render_pass_id = id2;
  viz::SharedQuadState* sqs2 =
      render_passes[2]->CreateAndAppendSharedQuadState();
  rpdq2->shared_quad_state = sqs2;
  rpdq2->rect = quad_rect;

  std::vector<RenderPassInfo> render_pass_infos = {
      {*render_passes[0], true},
      {*render_passes[1], false},
      {*render_passes[2], false},
  };

  base::flat_set<viz::FrameSinkId> expected_frame_sinks{frame_sink_id};
  // The surface quad (0,0 100x100) is entirely behind the backdrop filter on
  // the rpdq (0,0 100x100) so it can be throttled.
  RunThrottleDecider(render_pass_infos);
  EXPECT_EQ(GetFrameSinksToThrottle(), expected_frame_sinks);

  // Put the backdrop filter within bounds (0,10 50x50).
  rpdq->backdrop_filter_bounds = SkPath::RRect(
      SkRRect::MakeRectXY(SkRect::MakeXYWH(0.0f, 10.0f, 50.0f, 50.0f), 1, 1));
  // The surface quad (0,0 100x100) is partially behind the backdrop filter on
  // the rpdq (0,10 50x50) so it should not be throttled.
  RunThrottleDecider(render_pass_infos);
  EXPECT_TRUE(GetFrameSinksToThrottle().empty());

  // Transform the surface quad to (0,10 50x50).
  gfx::Transform transform;
  transform.Translate(0, 10);
  transform.Scale(0.5f, 0.5f);
  sqs3->quad_to_target_transform = transform;
  // The surface quad (0,10 50x50) is entirely behind the backdrop filter on the
  // rpdq (0,10 50x50) so it can be throttled.
  RunThrottleDecider(render_pass_infos);
  EXPECT_EQ(GetFrameSinksToThrottle(), expected_frame_sinks);

  // Add a mask to the backdrop filter.
  rpdq->resource_id = viz::ResourceId::FromUnsafeValue(1u);

  // As the mask would make the backdrop filter to be ignored, the surface
  // should not be throttled.
  RunThrottleDecider(render_pass_infos);
  EXPECT_TRUE(GetFrameSinksToThrottle().empty());

  // Add a foreground filter to the second render pass.
  rpdq2->filters.Append(FilterOperation::CreateBlurFilter(5.0f));
  render_pass_infos[1].has_foreground_blur = true;
  // The surface quad is being blurred by the foreground filter so it can be
  // throttled.
  RunThrottleDecider(render_pass_infos);
  EXPECT_EQ(GetFrameSinksToThrottle(), expected_frame_sinks);
}

}  // namespace cc
