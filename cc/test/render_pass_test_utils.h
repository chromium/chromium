// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_RENDER_PASS_TEST_UTILS_H_
#define CC_TEST_RENDER_PASS_TEST_UTILS_H_

#include <stdint.h>

#include "cc/paint/filter_operations.h"
#include "components/viz/common/quads/render_pass.h"
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
class ContextProvider;
class DisplayResourceProvider;
class RenderPass;
class SolidColorDrawQuad;
}  // namespace viz

namespace cc {

// Adds a new render pass with the provided properties to the given
// render pass list.
viz::RenderPass* AddRenderPass(viz::RenderPassList* pass_list,
                               int render_pass_id,
                               const gfx::Rect& output_rect,
                               const gfx::Transform& root_transform,
                               const FilterOperations& filters);

// Adds a new render pass with the provided properties to the given
// render pass list.
viz::RenderPass* AddRenderPassWithDamage(viz::RenderPassList* pass_list,
                                         int render_pass_id,
                                         const gfx::Rect& output_rect,
                                         const gfx::Rect& damage_rect,
                                         const gfx::Transform& root_transform,
                                         const FilterOperations& filters);

// Adds a solid quad to a given render pass.
viz::SolidColorDrawQuad* AddQuad(viz::RenderPass* pass,
                                 const gfx::Rect& rect,
                                 SkColor color);

// Adds a solid quad to a given render pass and sets is_clipped=true.
viz::SolidColorDrawQuad* AddClippedQuad(viz::RenderPass* pass,
                                        const gfx::Rect& rect,
                                        SkColor color);

// Adds a solid quad with a transform to a given render pass.
viz::SolidColorDrawQuad* AddTransformedQuad(viz::RenderPass* pass,
                                            const gfx::Rect& rect,
                                            SkColor color,
                                            const gfx::Transform& transform);

// Adds a render pass quad to an existing render pass.
viz::RenderPassDrawQuad* AddRenderPassQuad(viz::RenderPass* to_pass,
                                           viz::RenderPass* contributing_pass);

// Adds a render pass quad with the given mask resource, filter, and transform.
void AddRenderPassQuad(viz::RenderPass* to_pass,
                       viz::RenderPass* contributing_pass,
                       viz::ResourceId mask_resource_id,
                       gfx::Transform transform,
                       SkBlendMode blend_mode);

std::vector<viz::ResourceId> AddOneOfEveryQuadType(
    viz::RenderPass* to_pass,
    viz::ClientResourceProvider* resource_provider,
    viz::RenderPassId child_pass_id);

// Adds a render pass quad with the given mask resource, filter, and transform.
// The resource used in render pass is created by viz::ClientResourceProvider,
// then transferred to viz::DisplayResourceProvider.
void AddOneOfEveryQuadTypeInDisplayResourceProvider(
    viz::RenderPass* to_pass,
    viz::DisplayResourceProvider* resource_provider,
    viz::ClientResourceProvider* child_resource_provider,
    viz::ContextProvider* child_context_provider,
    viz::RenderPassId child_pass_id,
    gpu::SyncToken* sync_token_for_mailbox_texture);

}  // namespace cc

#endif  // CC_TEST_RENDER_PASS_TEST_UTILS_H_
