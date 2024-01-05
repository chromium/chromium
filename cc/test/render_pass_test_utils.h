// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_RENDER_PASS_TEST_UTILS_H_
#define CC_TEST_RENDER_PASS_TEST_UTILS_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "cc/paint/filter_operations.h"
#include "components/viz/common/quads/aggregated_render_pass.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "third_party/skia/include/core/SkColor.h"

namespace gfx {
class Rect;
class Transform;
}

namespace gpu {
struct SyncToken;
}

namespace viz {
class ClientResourceProvider;
class DisplayResourceProvider;
class CompositorRenderPass;
class RasterContextProvider;
}  // namespace viz

namespace cc {

// Adds a new render pass with the provided properties to the given
// render pass list.
viz::CompositorRenderPass* AddRenderPass(
    viz::CompositorRenderPassList* pass_list,
    viz::CompositorRenderPassId render_pass_id,
    const gfx::Rect& output_rect,
    const gfx::Transform& root_transform,
    const FilterOperations& filters);

viz::AggregatedRenderPass* AddRenderPass(
    viz::AggregatedRenderPassList* pass_list,
    viz::AggregatedRenderPassId render_pass_id,
    const gfx::Rect& output_rect,
    const gfx::Transform& root_transform,
    const FilterOperations& filters);

// Adds a new render pass with the provided properties to the given
// render pass list.
viz::CompositorRenderPass* AddRenderPassWithDamage(
    viz::CompositorRenderPassList* pass_list,
    viz::CompositorRenderPassId render_pass_id,
    const gfx::Rect& output_rect,
    const gfx::Rect& damage_rect,
    const gfx::Transform& root_transform,
    const FilterOperations& filters);
viz::AggregatedRenderPass* AddRenderPassWithDamage(
    viz::AggregatedRenderPassList* pass_list,
    viz::AggregatedRenderPassId render_pass_id,
    const gfx::Rect& output_rect,
    const gfx::Rect& damage_rect,
    const gfx::Transform& root_transform,
    const FilterOperations& filters);

// Adds a solid quad to a given render pass.
template <typename RenderPassType>
inline viz::SolidColorDrawQuad* AddTransparentQuad(RenderPassType* pass,
                                                   const gfx::Rect& rect,
                                                   SkColor4f color,
                                                   float opacity) {
  viz::SharedQuadState* shared_state = pass->CreateAndAppendSharedQuadState();
  shared_state->SetAll(gfx::Transform(), rect, rect, gfx::MaskFilterInfo(),
                       /*clip=*/std::nullopt, /*contents_opaque=*/false,
                       opacity, SkBlendMode::kSrcOver, /*sorting_context=*/0,
                       /*layer_id=*/0u, /*fast_rounded_corner=*/false);
  auto* quad =
      pass->template CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
  quad->SetNew(shared_state, rect, rect, color, false);
  return quad;
}

template <typename RenderPassType>
inline viz::SolidColorDrawQuad* AddQuad(RenderPassType* pass,
                                        const gfx::Rect& rect,
                                        SkColor4f color) {
  return AddTransparentQuad(pass, rect, color, 1.0);
}

// Adds a solid quad to a given render pass and sets is_clipped=true.
viz::SolidColorDrawQuad* AddClippedQuad(viz::AggregatedRenderPass* pass,
                                        const gfx::Rect& rect,
                                        SkColor color);

// Adds a solid quad with a transform to a given render pass.
viz::SolidColorDrawQuad* AddTransformedQuad(viz::AggregatedRenderPass* pass,
                                            const gfx::Rect& rect,
                                            SkColor color,
                                            const gfx::Transform& transform);

// Adds a render pass quad to an existing render pass.
viz::CompositorRenderPassDrawQuad* AddRenderPassQuad(
    viz::CompositorRenderPass* to_pass,
    viz::CompositorRenderPass* contributing_pass);
viz::AggregatedRenderPassDrawQuad* AddRenderPassQuad(
    viz::AggregatedRenderPass* to_pass,
    viz::AggregatedRenderPass* contributing_pass);

// Adds a render pass quad with the given mask resource, filter, and transform.
void AddRenderPassQuad(viz::AggregatedRenderPass* to_pass,
                       viz::AggregatedRenderPass* contributing_pass,
                       viz::ResourceId mask_resource_id,
                       gfx::Transform transform,
                       SkBlendMode blend_mode);

std::vector<viz::ResourceId> AddOneOfEveryQuadType(
    viz::CompositorRenderPass* to_pass,
    viz::ClientResourceProvider* resource_provider,
    viz::CompositorRenderPassId child_pass_id);

// Adds a render pass quad with the given mask resource, filter, and transform.
// The resource used in render pass is created by viz::ClientResourceProvider,
// then transferred to viz::DisplayResourceProvider.
void AddOneOfEveryQuadTypeInDisplayResourceProvider(
    viz::AggregatedRenderPass* to_pass,
    viz::DisplayResourceProvider* resource_provider,
    viz::ClientResourceProvider* child_resource_provider,
    viz::RasterContextProvider* child_context_provider,
    viz::AggregatedRenderPassId child_pass_id,
    gpu::SyncToken* sync_token_for_mailbox_texture);

std::unique_ptr<viz::AggregatedRenderPass> CopyToAggregatedRenderPass(
    viz::CompositorRenderPass* from_pass,
    viz::AggregatedRenderPassId to_id,
    gfx::ContentColorUsage content_usage);

}  // namespace cc

#endif  // CC_TEST_RENDER_PASS_TEST_UTILS_H_
