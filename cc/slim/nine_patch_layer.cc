// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/slim/nine_patch_layer.h"

#include <utility>

#include "cc/layers/nine_patch_generator.h"
#include "cc/layers/nine_patch_layer.h"
#include "cc/slim/features.h"
#include "cc/slim/layer_tree_impl.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/resources/resource_id.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace cc::slim {

// static
scoped_refptr<NinePatchLayer> NinePatchLayer::Create() {
  scoped_refptr<cc::NinePatchLayer> cc_layer;
  if (!features::IsSlimCompositorEnabled()) {
    cc_layer = cc::NinePatchLayer::Create();
  }
  return base::AdoptRef(new NinePatchLayer(std::move(cc_layer)));
}

NinePatchLayer::NinePatchLayer(scoped_refptr<cc::NinePatchLayer> cc_layer)
    : UIResourceLayer(std::move(cc_layer)) {}

NinePatchLayer::~NinePatchLayer() = default;

cc::NinePatchLayer* NinePatchLayer::cc_layer() const {
  return static_cast<cc::NinePatchLayer*>(cc_layer_.get());
}

void NinePatchLayer::SetBorder(const gfx::Rect& border) {
  if (cc_layer()) {
    cc_layer()->SetBorder(border);
    return;
  }
  if (border_ == border) {
    return;
  }
  border_ = border;
  NotifyPropertyChanged();
}

void NinePatchLayer::SetAperture(const gfx::Rect& aperture) {
  if (cc_layer()) {
    cc_layer()->SetAperture(aperture);
    return;
  }
  if (aperture_ == aperture) {
    return;
  }
  aperture_ = aperture;
  NotifyPropertyChanged();
}

void NinePatchLayer::SetFillCenter(bool fill_center) {
  if (cc_layer()) {
    cc_layer()->SetFillCenter(fill_center);
    return;
  }
  if (fill_center_ == fill_center) {
    return;
  }
  fill_center_ = fill_center;
  NotifyPropertyChanged();
}

void NinePatchLayer::SetNearestNeighbor(bool nearest_neighbor) {
  if (cc_layer()) {
    cc_layer()->SetNearestNeighbor(nearest_neighbor);
    return;
  }
  if (nearest_neighbor_ == nearest_neighbor) {
    return;
  }
  nearest_neighbor_ = nearest_neighbor;
  NotifyPropertyChanged();
}

void NinePatchLayer::AppendQuads(viz::CompositorRenderPass& render_pass,
                                 const gfx::Transform& transform,
                                 const gfx::Rect* clip) {
  LayerTreeImpl* layer_tree_impl = static_cast<LayerTreeImpl*>(layer_tree());
  viz::ResourceId viz_resource_id =
      layer_tree_impl->GetVizResourceId(resource_id());
  if (viz_resource_id == viz::kInvalidResourceId) {
    return;
  }

  viz::SharedQuadState* quad_state =
      CreateAndAppendSharedQuadState(render_pass, transform, clip);

  constexpr gfx::Rect kOcclusion;
  const gfx::Size image_bounds =
      layer_tree_impl->GetUIResourceSize(resource_id());
  quad_generator_.SetLayout(image_bounds, bounds(), aperture_, border_,
                            kOcclusion, fill_center_, nearest_neighbor_);
  const bool opaque = layer_tree_impl->IsUIResourceOpaque(resource_id());
  // Select the int instead of float version.
  auto IntersectRects =
      static_cast<gfx::Rect (*)(const gfx::Rect&, const gfx::Rect&)>(
          gfx::IntersectRects);
  quad_generator_.AppendQuads(
      viz_resource_id, opaque,
      base::BindRepeating(IntersectRects, quad_state->visible_quad_layer_rect),
      layer_tree_impl->GetClientResourceProvider(), &render_pass, quad_state,
      quad_generator_.GeneratePatches());
}

}  // namespace cc::slim
