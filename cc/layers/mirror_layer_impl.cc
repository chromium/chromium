// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/mirror_layer_impl.h"

#include <memory>

#include "cc/trees/effect_node.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/occlusion.h"
#include "components/viz/common/quads/compositor_render_pass_draw_quad.h"

namespace cc {

MirrorLayerImpl::MirrorLayerImpl(LayerTreeImpl* tree_impl, int id)
    : LayerImpl(tree_impl, id) {}

MirrorLayerImpl::~MirrorLayerImpl() = default;

std::unique_ptr<LayerImpl> MirrorLayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) const {
  return MirrorLayerImpl::Create(tree_impl, id());
}

void MirrorLayerImpl::AppendQuads(viz::CompositorRenderPass* render_pass,
                                  AppendQuadsData* append_quads_data) {
  // TODO(mohsen): Currently, effects on the mirrored layer (e.g mask and
  // opacity) are ignored. Consider applying them here.

  auto* mirrored_layer = layer_tree_impl()->LayerById(mirrored_layer_id_);
  auto* mirrored_render_surface =
      GetEffectTree().GetRenderSurface(mirrored_layer->effect_tree_index());
  gfx::Rect content_rect = mirrored_render_surface->content_rect();

  gfx::Rect unoccluded_content_rect =
      draw_properties().occlusion_in_content_space.GetUnoccludedContentRect(
          content_rect);
  if (unoccluded_content_rect.IsEmpty())
    return;

  const bool contents_opaque = false;
  viz::SharedQuadState* shared_quad_state =
      render_pass->CreateAndAppendSharedQuadState();
  // TODO(crbug.com/1196414): Support 2D scales in mirror layers.
  PopulateScaledSharedQuadStateWithContentRects(
      shared_quad_state, mirrored_layer->GetIdealContentsScaleKey(),
      content_rect, content_rect, contents_opaque);

  AppendDebugBorderQuad(render_pass, content_rect, shared_quad_state,
                        append_quads_data);

  viz::ResourceId mask_resource_id = viz::kInvalidResourceId;
  gfx::RectF mask_uv_rect;
  gfx::Size mask_texture_size;

  auto* mirrored_effect_node = mirrored_render_surface->OwningEffectNode();
  auto* quad =
      render_pass->CreateAndAppendDrawQuad<viz::CompositorRenderPassDrawQuad>();
  quad->SetNew(shared_quad_state, content_rect, unoccluded_content_rect,
               mirrored_layer_render_pass_id(), mask_resource_id, mask_uv_rect,
               mask_texture_size, mirrored_effect_node->surface_contents_scale,
               gfx::PointF(), gfx::RectF(gfx::Rect(content_rect.size())),
               !layer_tree_impl()->settings().enable_edge_anti_aliasing, 0.f);
}

void MirrorLayerImpl::PushPropertiesTo(LayerImpl* layer) {
  LayerImpl::PushPropertiesTo(layer);

  auto* mirror_layer = static_cast<MirrorLayerImpl*>(layer);
  mirror_layer->SetMirroredLayerId(mirrored_layer_id_);
}

gfx::Rect MirrorLayerImpl::GetDamageRect() const {
  // TOOD(mohsen): Currently, the whole layer is marked as damaged. We should
  // only consider the damage from the mirrored layer.
  return gfx::Rect(bounds());
}

gfx::Rect MirrorLayerImpl::GetEnclosingVisibleRectInTargetSpace() const {
  const LayerImpl* mirrored_layer =
      layer_tree_impl()->LayerById(mirrored_layer_id_);
  return GetScaledEnclosingVisibleRectInTargetSpace(
      mirrored_layer->GetIdealContentsScaleKey());
}

const char* MirrorLayerImpl::LayerTypeAsString() const {
  return "cc::MirrorLayerImpl";
}

}  // namespace cc
