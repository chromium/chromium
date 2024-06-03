// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/slim/surface_layer.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "cc/slim/layer_tree_impl.h"
#include "components/viz/common/hit_test/hit_test_region_list.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/offset_tag.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/surface_draw_quad.h"
#include "components/viz/common/surfaces/surface_range.h"

namespace cc::slim {

// static
scoped_refptr<SurfaceLayer> SurfaceLayer::Create() {
  return base::AdoptRef(new SurfaceLayer());
}

SurfaceLayer::SurfaceLayer() = default;
SurfaceLayer::~SurfaceLayer() = default;

void SurfaceLayer::SetSurfaceId(const viz::SurfaceId& surface_id,
                                const cc::DeadlinePolicy& deadline_policy) {
  if (surface_range_.end() == surface_id &&
      deadline_policy.use_existing_deadline()) {
    return;
  }

  SetSurfaceRange(viz::SurfaceRange(surface_range_.start(), surface_id));
  if (!surface_range_.IsValid()) {
    deadline_in_frames_ = 0u;
  } else if (!deadline_policy.use_existing_deadline()) {
    deadline_in_frames_ = deadline_policy.deadline_in_frames();
  }
}

void SurfaceLayer::SetStretchContentToFillBounds(
    bool stretch_content_to_fill_bounds) {
  if (stretch_content_to_fill_bounds_ == stretch_content_to_fill_bounds) {
    return;
  }
  stretch_content_to_fill_bounds_ = stretch_content_to_fill_bounds;
  NotifyPropertyChanged();
}

void SurfaceLayer::SetOldestAcceptableFallback(
    const viz::SurfaceId& surface_id) {
  // The fallback should never move backwards.
  DCHECK(!surface_range_.start() ||
         !surface_range_.start()->IsNewerThan(surface_id));
  if (surface_range_.start() == surface_id) {
    return;
  }

  SetSurfaceRange(viz::SurfaceRange(
      surface_id.is_valid() ? std::optional<viz::SurfaceId>(surface_id)
                            : std::nullopt,
      surface_range_.end()));
}

void SurfaceLayer::RegisterOffsetTag(
    const viz::OffsetTag& tag,
    const viz::OffsetTagConstraints& constraints) {
  CHECK(tag);
  CHECK(constraints.IsValid());

  bool inserted = offset_tags_.insert_or_assign(tag, constraints).second;

  if (inserted && layer_tree()) {
    static_cast<LayerTreeImpl*>(layer_tree())->RegisterOffsetTag(tag, this);
  }
}

void SurfaceLayer::UnregisterOffsetTag(const viz::OffsetTag& tag) {
  CHECK(tag);

  size_t count = offset_tags_.erase(tag);
  CHECK_EQ(count, 1u);
  if (auto* layer_tree_impl = static_cast<LayerTreeImpl*>(layer_tree())) {
    layer_tree_impl->UnregisterOffsetTag(tag, this);
  }
}

viz::OffsetTagDefinition SurfaceLayer::GetOffsetTagDefinition(
    const viz::OffsetTag& tag) {
  return viz::OffsetTagDefinition(tag, surface_range_, offset_tags_.at(tag));
}

void SurfaceLayer::SetLayerTree(LayerTree* tree) {
  if (layer_tree() == tree) {
    return;
  }

  if (auto* layer_tree_impl = static_cast<LayerTreeImpl*>(layer_tree())) {
    if (surface_range_.IsValid()) {
      layer_tree_impl->RemoveSurfaceRange(surface_range_);
    }
    for (auto& [tag, constraints] : offset_tags_) {
      layer_tree_impl->UnregisterOffsetTag(tag, this);
    }
  }
  Layer::SetLayerTree(tree);
  if (auto* layer_tree_impl = static_cast<LayerTreeImpl*>(layer_tree())) {
    if (surface_range_.IsValid()) {
      layer_tree_impl->AddSurfaceRange(surface_range_);
    }
    for (auto& [tag, constraints] : offset_tags_) {
      layer_tree_impl->RegisterOffsetTag(tag, this);
    }
  }
}

void SurfaceLayer::SetSurfaceRange(const viz::SurfaceRange& surface_range) {
  if (surface_range_ == surface_range) {
    return;
  }
  if (layer_tree() && surface_range_.IsValid()) {
    static_cast<LayerTreeImpl*>(layer_tree())
        ->RemoveSurfaceRange(surface_range_);
  }
  surface_range_ = surface_range;
  if (layer_tree() && surface_range_.IsValid()) {
    static_cast<LayerTreeImpl*>(layer_tree())->AddSurfaceRange(surface_range_);
  }
  NotifyPropertyChanged();
}

void SurfaceLayer::AppendQuads(viz::CompositorRenderPass& render_pass,
                               FrameData& data,
                               const gfx::Transform& transform_to_root,
                               const gfx::Transform& transform_to_target,
                               const gfx::Rect* clip_in_target,
                               const gfx::Rect& visible_rect,
                               float opacity) {
  viz::SharedQuadState* quad_state =
      CreateAndAppendSharedQuadState(render_pass, data, transform_to_target,
                                     clip_in_target, visible_rect, opacity);

  if (surface_range_.IsValid()) {
    auto* quad = render_pass.CreateAndAppendDrawQuad<viz::SurfaceDrawQuad>();
    quad->SetNew(quad_state, quad_state->quad_layer_rect,
                 quad_state->visible_quad_layer_rect, surface_range_,
                 background_color(), stretch_content_to_fill_bounds_);

    data.activation_dependencies.insert(surface_range_.end());

    if (deadline_in_frames_) {
      if (!data.deadline_in_frames) {
        data.deadline_in_frames = 0u;
      }
      data.deadline_in_frames =
          std::max(*data.deadline_in_frames, *deadline_in_frames_);
    } else {
      data.use_default_lower_bound_deadline = true;
    }

    auto& hit_test_region = data.hit_test_regions->emplace_back();
    hit_test_region.flags = viz::HitTestRegionFlags::kHitTestMouse |
                            viz::HitTestRegionFlags::kHitTestTouch |
                            viz::HitTestRegionFlags::kHitTestChildSurface;
    hit_test_region.frame_sink_id = surface_range_.end().frame_sink_id();
    hit_test_region.rect = quad_state->visible_quad_layer_rect;
    // False will set transform to identity.
    bool rv = transform_to_root.GetInverse(&hit_test_region.transform);
    if (!rv || !hit_test_region.transform.Preserves2dAxisAlignment()) {
      hit_test_region.flags |= viz::HitTestRegionFlags::kHitTestAsk;
      hit_test_region.async_hit_test_reasons |=
          viz::AsyncHitTestReasons::kIrregularClip;
    }
  } else {
    auto* quad = render_pass.CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
    quad->SetNew(quad_state, quad_state->quad_layer_rect,
                 quad_state->visible_quad_layer_rect, background_color(),
                 false /* force_anti_aliasing_off */);
  }

  // Unless the client explicitly calls SetSurfaceId again after this
  // commit, don't block on |surface_range_| again.
  deadline_in_frames_ = 0u;
}

}  // namespace cc::slim
