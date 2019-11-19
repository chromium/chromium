// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/solid_color_layer_impl.h"

#include <algorithm>

#include "cc/layers/append_quads_data.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/occlusion.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"

namespace cc {

SolidColorLayerImpl::SolidColorLayerImpl(LayerTreeImpl* tree_impl, int id)
    : LayerImpl(tree_impl, id) {
}

SolidColorLayerImpl::~SolidColorLayerImpl() = default;

std::unique_ptr<LayerImpl> SolidColorLayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return SolidColorLayerImpl::Create(tree_impl, id());
}

void SolidColorLayerImpl::AppendSolidQuads(
    viz::RenderPass* render_pass,
    const Occlusion& occlusion_in_layer_space,
    viz::SharedQuadState* shared_quad_state,
    const gfx::Rect& visible_layer_rect,
    SkColor color,
    bool force_anti_aliasing_off,
    SkBlendMode effect_blend_mode,
    AppendQuadsData* append_quads_data) {
  // Transparent, solid quads can be omitted if the effect blend mode is
  // kSrcOver. Note that |effect_blend_mode| may be different than
  // |shared_quad_state->blend_mode|, if the blend is applied by a render
  // surface. This is because a layer that induces an effect node emits
  // two quads, one for the layer, and one for the render surface, and in
  // this situation the blend mode is lifted up to the render surface.
  // This will work for situations where there is only one layer under the
  // mask, but will not work in complex blend mode situations. This bug is
  // tracked in crbug.com/939168.
  if (effect_blend_mode == SkBlendMode::kSrcOver) {
    float alpha =
        (SkColorGetA(color) * (1.0f / 255.0f)) * shared_quad_state->opacity;

    if (alpha < std::numeric_limits<float>::epsilon())
      return;
  }

  gfx::Rect visible_quad_rect =
      occlusion_in_layer_space.GetUnoccludedContentRect(visible_layer_rect);
  auto* quad = render_pass->CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
  quad->SetNew(shared_quad_state, visible_layer_rect, visible_quad_rect, color,
               force_anti_aliasing_off);
}

void SolidColorLayerImpl::AppendQuads(viz::RenderPass* render_pass,
                                      AppendQuadsData* append_quads_data) {
  viz::SharedQuadState* shared_quad_state =
      render_pass->CreateAndAppendSharedQuadState();
  PopulateSharedQuadState(shared_quad_state, contents_opaque());

  AppendDebugBorderQuad(render_pass, gfx::Rect(bounds()), shared_quad_state,
                        append_quads_data);

  // TODO(hendrikw): We need to pass the visible content rect rather than
  // |bounds()| here.
  EffectNode* effect_node = GetEffectTree().Node(effect_tree_index());
  AppendSolidQuads(render_pass, draw_properties().occlusion_in_content_space,
                   shared_quad_state, gfx::Rect(bounds()), background_color(),
                   !layer_tree_impl()->settings().enable_edge_anti_aliasing,
                   effect_node->blend_mode, append_quads_data);
}

const char* SolidColorLayerImpl::LayerTypeAsString() const {
  return "cc::SolidColorLayerImpl";
}

}  // namespace cc
