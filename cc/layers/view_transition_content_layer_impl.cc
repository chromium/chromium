// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/view_transition_content_layer_impl.h"

#include "base/memory/ptr_util.h"
#include "cc/layers/append_quads_data.h"
#include "cc/layers/layer_impl.h"
#include "cc/trees/layer_tree_impl.h"
#include "components/viz/common/quads/shared_element_draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/view_transition_element_resource_id.h"

namespace cc {

// static
std::unique_ptr<ViewTransitionContentLayerImpl>
ViewTransitionContentLayerImpl::Create(
    LayerTreeImpl* tree_impl,
    int id,
    const viz::ViewTransitionElementResourceId& resource_id,
    bool is_live_content_layer) {
  return base::WrapUnique(new ViewTransitionContentLayerImpl(
      tree_impl, id, resource_id, is_live_content_layer));
}

ViewTransitionContentLayerImpl::ViewTransitionContentLayerImpl(
    LayerTreeImpl* tree_impl,
    int id,
    const viz::ViewTransitionElementResourceId& resource_id,
    bool is_live_content_layer)
    : LayerImpl(tree_impl, id),
      resource_id_(resource_id),
      is_live_content_layer_(is_live_content_layer) {}

ViewTransitionContentLayerImpl::~ViewTransitionContentLayerImpl() = default;

mojom::LayerType ViewTransitionContentLayerImpl::GetLayerType() const {
  return mojom::LayerType::kViewTransitionContent;
}

std::unique_ptr<LayerImpl> ViewTransitionContentLayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) const {
  return ViewTransitionContentLayerImpl::Create(tree_impl, id(), resource_id_,
                                                is_live_content_layer_);
}

void ViewTransitionContentLayerImpl::NotifyKnownResourceIdsBeforeAppendQuads(
    const base::flat_set<viz::ViewTransitionElementResourceId>&
        known_resource_ids) {
  skip_unseen_resource_quads_ = known_resource_ids.count(resource_id_) == 0;
}

void ViewTransitionContentLayerImpl::AppendQuads(
    viz::CompositorRenderPass* render_pass,
    AppendQuadsData* append_quads_data) {
  // Skip live content elements that don't have a corresponding resource render
  // passes.
  if (is_live_content_layer_ && skip_unseen_resource_quads_)
    return;

  float device_scale_factor = layer_tree_impl()->device_scale_factor();

  gfx::Rect quad_rect(
      gfx::ScaleToEnclosingRect(gfx::Rect(bounds()), device_scale_factor));
  gfx::Rect visible_quad_rect =
      draw_properties().occlusion_in_content_space.GetUnoccludedContentRect(
          gfx::Rect(bounds()));

  visible_quad_rect =
      gfx::ScaleToEnclosingRect(visible_quad_rect, device_scale_factor);
  visible_quad_rect = gfx::IntersectRects(quad_rect, visible_quad_rect);

  if (visible_quad_rect.IsEmpty())
    return;

  viz::SharedQuadState* shared_quad_state =
      render_pass->CreateAndAppendSharedQuadState();
  PopulateScaledSharedQuadState(shared_quad_state, device_scale_factor,
                                contents_opaque());

  auto* quad =
      render_pass->CreateAndAppendDrawQuad<viz::SharedElementDrawQuad>();
  quad->SetNew(shared_quad_state, quad_rect, visible_quad_rect, resource_id_);
  append_quads_data->has_shared_element_resources = true;
}

viz::ViewTransitionElementResourceId
ViewTransitionContentLayerImpl::ViewTransitionResourceId() const {
  return resource_id_;
}

}  // namespace cc
