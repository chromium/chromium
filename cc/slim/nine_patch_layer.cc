// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/slim/nine_patch_layer.h"

#include <utility>

#include "cc/layers/nine_patch_generator.h"
#include "cc/slim/layer_tree_impl.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/resources/resource_id.h"

namespace cc::slim {

// static
scoped_refptr<NinePatchLayer> NinePatchLayer::Create() {
  return base::AdoptRef(new NinePatchLayer());
}

NinePatchLayer::NinePatchLayer() = default;
NinePatchLayer::~NinePatchLayer() = default;

void NinePatchLayer::SetBorder(const gfx::Rect& border) {
  if (border_ == border) {
    return;
  }
  border_ = border;
  NotifyPropertyChanged();
}

void NinePatchLayer::SetAperture(const gfx::Rect& aperture) {
  if (aperture_ == aperture) {
    return;
  }
  aperture_ = aperture;
  NotifyPropertyChanged();
}

void NinePatchLayer::SetFillCenter(bool fill_center) {
  if (fill_center_ == fill_center) {
    return;
  }
  fill_center_ = fill_center;
  NotifyPropertyChanged();
}

void NinePatchLayer::SetNearestNeighbor(bool nearest_neighbor) {
  if (nearest_neighbor_ == nearest_neighbor) {
    return;
  }
  nearest_neighbor_ = nearest_neighbor;
  NotifyPropertyChanged();
}

void NinePatchLayer::AppendQuads(viz::CompositorRenderPass& render_pass,
                                 FrameData& data,
                                 const gfx::Transform& transform_to_root,
                                 const gfx::Transform& transform_to_target,
                                 const gfx::Rect* clip_in_target,
                                 const gfx::Rect& visible_rect,
                                 float opacity) {
  LayerTreeImpl* layer_tree_impl = static_cast<LayerTreeImpl*>(layer_tree());
  viz::ResourceId viz_resource_id =
      layer_tree_impl->GetVizResourceId(resource_id());
  if (viz_resource_id == viz::kInvalidResourceId) {
    return;
  }

  viz::SharedQuadState* quad_state =
      CreateAndAppendSharedQuadState(render_pass, data, transform_to_target,
                                     clip_in_target, visible_rect, opacity);

  constexpr gfx::Rect kOcclusion;
  const gfx::Size image_bounds =
      layer_tree_impl->GetUIResourceSize(resource_id());
  quad_generator_.SetLayout(image_bounds, bounds(), aperture_, border_,
                            kOcclusion, fill_center_, nearest_neighbor_);
  const bool opaque = layer_tree_impl->IsUIResourceOpaque(resource_id());
  quad_generator_.AppendQuads(
      viz_resource_id, opaque,
      [quad_state](const gfx::Rect& rect) {
        return gfx::IntersectRects(quad_state->visible_quad_layer_rect, rect);
      },
      layer_tree_impl->GetClientResourceProvider(), &render_pass, quad_state,
      quad_generator_.GeneratePatches());
}

}  // namespace cc::slim
