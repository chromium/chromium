// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "cc/trees/throttle_decider.h"

#include <vector>

#include "cc/layers/surface_layer_impl.h"
#include "components/viz/common/quads/compositor_render_pass_draw_quad.h"
#include "components/viz/common/quads/surface_draw_quad.h"
#include "components/viz/common/surfaces/surface_range.h"

namespace cc {

ThrottleDecider::ThrottleDecider() = default;

ThrottleDecider::~ThrottleDecider() = default;

void ThrottleDecider::Prepare() {
  last_ids_.swap(ids_);
  id_to_pass_map_.clear();
  ids_.clear();
}

void ThrottleDecider::ProcessRenderPass(
    const viz::CompositorRenderPass& render_pass) {
  bool foreground_blurred =
      render_pass.filters.HasFilterOfType(FilterOperation::BLUR);
  std::vector<gfx::RectF> blur_backdrop_filter_bounds;
  for (viz::QuadList::ConstIterator it = render_pass.quad_list.begin();
       it != render_pass.quad_list.end(); ++it) {
    const viz::DrawQuad* quad = *it;
    if (const auto* render_pass_quad =
            quad->DynamicCast<viz::CompositorRenderPassDrawQuad>()) {
      // If the quad render pass has a blur backdrop filter without a mask, add
      // the filter bounds to the bounds list.

      auto found = id_to_pass_map_.find(render_pass_quad->render_pass_id);
      if (found == id_to_pass_map_.end()) {
        // It is possible that this function is called when the render passes in
        // a frame haven't been cleaned up yet. A RPDQ can possibly refer to an
        // invalid render pass.
        continue;
      }
      const auto& child_rp = *found->second;
      if (child_rp.backdrop_filters.HasFilterOfType(FilterOperation::BLUR) &&
          render_pass_quad->resources
                  .ids[viz::RenderPassDrawQuadInternal::kMaskResourceIdIndex] ==
              viz::kInvalidResourceId) {
        gfx::RectF blur_bounds(child_rp.output_rect);
        if (child_rp.backdrop_filter_bounds)
          blur_bounds.Intersect(child_rp.backdrop_filter_bounds->rect());
        blur_bounds = quad->shared_quad_state->quad_to_target_transform.MapRect(
            blur_bounds);
        if (quad->shared_quad_state->clip_rect) {
          blur_bounds.Intersect(
              gfx::RectF(*quad->shared_quad_state->clip_rect));
        }
        blur_backdrop_filter_bounds.push_back(blur_bounds);
      }
    } else if (const auto* surface_quad =
                   quad->DynamicCast<viz::SurfaceDrawQuad>()) {
      bool inside_backdrop_filter_bounds = false;
      if (!foreground_blurred && !blur_backdrop_filter_bounds.empty()) {
        gfx::RectF rect_in_target_space =
            quad->shared_quad_state->quad_to_target_transform.MapRect(
                gfx::RectF(quad->visible_rect));
        if (quad->shared_quad_state->clip_rect) {
          rect_in_target_space.Intersect(
              gfx::RectF(*quad->shared_quad_state->clip_rect));
        }

        for (const gfx::RectF& blur_bounds : blur_backdrop_filter_bounds) {
          if (blur_bounds.Contains(rect_in_target_space)) {
            inside_backdrop_filter_bounds = true;
            break;
          }
        }
      }

      const viz::SurfaceRange& range = surface_quad->surface_range;
      DCHECK(range.IsValid());
      if (foreground_blurred || inside_backdrop_filter_bounds)
        ids_.insert(range.end().frame_sink_id());
    }
  }
  id_to_pass_map_.emplace(render_pass.id, &render_pass);
}

void ThrottleDecider::ProcessLayerNotToDraw(const LayerImpl* layer) {
  if (layer->is_surface_layer()) {
    const auto* surface_layer = static_cast<const SurfaceLayerImpl*>(layer);
    if (surface_layer->range().IsValid())
      ids_.insert(surface_layer->range().end().frame_sink_id());
  }
}

bool ThrottleDecider::HasThrottlingChanged() const {
  return ids_ != last_ids_;
}

}  // namespace cc
