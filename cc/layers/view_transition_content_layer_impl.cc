// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/view_transition_content_layer_impl.h"

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "cc/layers/append_quads_data.h"
#include "cc/layers/layer_impl.h"
#include "cc/trees/layer_tree_impl.h"
#include "components/viz/common/quads/shared_element_draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/view_transition_element_resource_id.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"

namespace cc {

// static
std::unique_ptr<ViewTransitionContentLayerImpl>
ViewTransitionContentLayerImpl::Create(
    LayerTreeImpl* tree_impl,
    int id,
    const viz::ViewTransitionElementResourceId& resource_id,
    bool is_live_content_layer,
    const gfx::RectF& max_extents_rect) {
  return base::WrapUnique(new ViewTransitionContentLayerImpl(
      tree_impl, id, resource_id, is_live_content_layer, max_extents_rect));
}

ViewTransitionContentLayerImpl::ViewTransitionContentLayerImpl(
    LayerTreeImpl* tree_impl,
    int id,
    const viz::ViewTransitionElementResourceId& resource_id,
    bool is_live_content_layer,
    const gfx::RectF& max_extents_rect)
    : LayerImpl(tree_impl, id),
      resource_id_(resource_id),
      is_live_content_layer_(is_live_content_layer),
      max_extents_rect_in_originating_layer_coordinate_space_(
          max_extents_rect) {}

ViewTransitionContentLayerImpl::~ViewTransitionContentLayerImpl() = default;

mojom::LayerType ViewTransitionContentLayerImpl::GetLayerType() const {
  return mojom::LayerType::kViewTransitionContent;
}

std::unique_ptr<LayerImpl> ViewTransitionContentLayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) const {
  return ViewTransitionContentLayerImpl::Create(
      tree_impl, id(), resource_id_, is_live_content_layer_,
      max_extents_rect_in_originating_layer_coordinate_space_);
}

void ViewTransitionContentLayerImpl::NotifyKnownResourceIdsBeforeAppendQuads(
    const base::flat_set<viz::ViewTransitionElementResourceId>&
        known_resource_ids) {
  skip_unseen_resource_quads_ = known_resource_ids.count(resource_id_) == 0;
}

void ViewTransitionContentLayerImpl::PushPropertiesTo(LayerImpl* layer) {
  LayerImpl::PushPropertiesTo(layer);
  static_cast<ViewTransitionContentLayerImpl*>(layer)->SetMaxExtentsRect(
      max_extents_rect_in_originating_layer_coordinate_space_);
}

// Compute the bounds that are actually going to be drawn, given that the
// bounds set for the layer are based on the max extents, and the actual drawn
// surface could be smaller.
void ViewTransitionContentLayerImpl::SetOriginatingSurfaceContentRect(
    const gfx::Rect&
        originating_surface_content_rect_in_layer_coordinate_space) {
  // All these scenarios mean that no correction needs to be done, and we can
  // paint the whole bounds.
  if (originating_surface_content_rect_in_layer_coordinate_space.IsEmpty() ||
      max_extents_rect_in_originating_layer_coordinate_space_.IsEmpty() ||
      gfx::RectF(originating_surface_content_rect_in_layer_coordinate_space) ==
          max_extents_rect_in_originating_layer_coordinate_space_) {
    actual_extents_rect_ = gfx::Rect();
    return;
  }

  // TODO(crbug.com/40840594): Add a CHECK that the surface rect is a subset of
  // the max extents. ATM this fails in one edge case (negative clip-path), the
  // CHECK should be added once that's fixed.

  // The actual extents rect is a subset of the max extents rect. This
  // projection maps this subset to the coordinate space of this layer (0, 0,
  // bounds()).
  actual_extents_rect_ = gfx::ToRoundedRect(gfx::MapRect(
      gfx::RectF(originating_surface_content_rect_in_layer_coordinate_space),
      max_extents_rect_in_originating_layer_coordinate_space_,
      gfx::RectF(bounds())));

  // Note that this is called late, when we compute the original layer's surface
  // content rect. So the surface content rect chain that relies on this should
  // also read this value later.
  draw_properties().visible_drawable_content_rect.Intersect(
      MathUtil::MapEnclosingClippedRect(
          draw_properties().target_space_transform, actual_extents_rect_));
}

void ViewTransitionContentLayerImpl::AppendQuads(
    viz::CompositorRenderPass* render_pass,
    AppendQuadsData* append_quads_data) {
  // Skip live content elements that don't have a corresponding resource render
  // passes.
  if (is_live_content_layer_ && skip_unseen_resource_quads_)
    return;

  auto bounds_rect = actual_extents_rect_.IsEmpty() ? gfx::Rect(bounds())
                                                    : actual_extents_rect_;

  float device_scale_factor = layer_tree_impl()->device_scale_factor();

  gfx::Rect quad_rect(
      gfx::ScaleToEnclosingRect(bounds_rect, device_scale_factor));

  gfx::Rect visible_quad_rect =
      draw_properties().occlusion_in_content_space.GetUnoccludedContentRect(
          bounds_rect);

  visible_quad_rect =
      gfx::ScaleToEnclosingRect(visible_quad_rect, device_scale_factor);
  visible_quad_rect = gfx::IntersectRects(quad_rect, visible_quad_rect);

  if (visible_quad_rect.IsEmpty()) {
    return;
  }

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

void ViewTransitionContentLayerImpl::SetMaxExtentsRect(
    const gfx::RectF& max_extents_rect) {
  if (max_extents_rect_in_originating_layer_coordinate_space_ ==
      max_extents_rect) {
    return;
  }
  max_extents_rect_in_originating_layer_coordinate_space_ = max_extents_rect;
  actual_extents_rect_ = gfx::Rect();
  NoteLayerPropertyChanged();
}

}  // namespace cc
