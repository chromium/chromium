// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/tile_based_layer_impl.h"

#include "cc/layers/solid_color_layer_impl.h"
#include "cc/trees/layer_tree_impl.h"

namespace cc {

TileBasedLayerImpl::TileBasedLayerImpl(LayerTreeImpl* tree_impl, int id)
    : LayerImpl(tree_impl, id) {}

TileBasedLayerImpl::~TileBasedLayerImpl() = default;

void TileBasedLayerImpl::AppendSolidQuad(viz::CompositorRenderPass* render_pass,
                                         AppendQuadsData* append_quads_data,
                                         SkColor4f color) {
  // TODO(crbug.com/41468388): This is still hard-coded at 1.0. This has some
  // history:
  //   - for crbug.com/769319, the contents scale was allowed to change, to
  //     avoid blurring on high-dpi screens.
  //   - for crbug.com/796558, the max device scale was hard-coded back to 1.0
  //     for single-tile masks, to avoid problems with transforms.
  // To avoid those transform/scale bugs, this is currently left at 1.0. See
  // crbug.com/979672 for more context and test links.
  float max_contents_scale = 1;

  // The downstream CA layers use shared_quad_state to generate resources of
  // the right size even if it is a solid color picture layer.
  viz::SharedQuadState* shared_quad_state =
      render_pass->CreateAndAppendSharedQuadState();
  PopulateScaledSharedQuadState(shared_quad_state, max_contents_scale,
                                contents_opaque());

  AppendDebugBorderQuad(render_pass, gfx::Rect(bounds()), shared_quad_state,
                        append_quads_data);

  gfx::Rect scaled_visible_layer_rect =
      shared_quad_state->visible_quad_layer_rect;
  Occlusion occlusion = draw_properties().occlusion_in_content_space;

  EffectNode* effect_node = GetEffectTree().Node(effect_tree_index());
  SolidColorLayerImpl::AppendSolidQuads(
      render_pass, occlusion, shared_quad_state, scaled_visible_layer_rect,
      color, !layer_tree_impl()->settings().enable_edge_anti_aliasing,
      effect_node->blend_mode, append_quads_data);
}

}  // namespace cc
