// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/shared_element_layer_impl.h"

#include "cc/layers/append_quads_data.h"
#include "cc/layers/layer_impl.h"
#include "cc/trees/layer_tree_impl.h"
#include "components/viz/common/quads/shared_element_draw_quad.h"
#include "components/viz/common/shared_element_resource_id.h"

namespace cc {

// static
std::unique_ptr<SharedElementLayerImpl> SharedElementLayerImpl::Create(
    LayerTreeImpl* tree_impl,
    int id,
    const viz::SharedElementResourceId& resource_id) {
  return base::WrapUnique(
      new SharedElementLayerImpl(tree_impl, id, resource_id));
}

SharedElementLayerImpl::SharedElementLayerImpl(
    LayerTreeImpl* tree_impl,
    int id,
    const viz::SharedElementResourceId& resource_id)
    : LayerImpl(tree_impl, id), resource_id_(resource_id) {}

SharedElementLayerImpl::~SharedElementLayerImpl() = default;

std::unique_ptr<LayerImpl> SharedElementLayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return SharedElementLayerImpl::Create(tree_impl, id(), resource_id_);
}

void SharedElementLayerImpl::AppendQuads(viz::CompositorRenderPass* render_pass,
                                         AppendQuadsData* append_quads_data) {
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

const char* SharedElementLayerImpl::LayerTypeAsString() const {
  return "cc::SharedElementLayerImpl";
}

}  // namespace cc
