// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/slim/surface_layer.h"

#include <algorithm>
#include <utility>

#include "cc/layers/surface_layer.h"
#include "cc/slim/features.h"
#include "cc/slim/layer_tree_impl.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/surface_draw_quad.h"
#include "components/viz/common/surfaces/surface_range.h"

namespace cc::slim {

// static
scoped_refptr<SurfaceLayer> SurfaceLayer::Create() {
  scoped_refptr<cc::SurfaceLayer> cc_layer;
  if (!features::IsSlimCompositorEnabled()) {
    cc_layer = cc::SurfaceLayer::Create();
  }
  return base::AdoptRef(new SurfaceLayer(std::move(cc_layer)));
}

SurfaceLayer::SurfaceLayer(scoped_refptr<cc::SurfaceLayer> cc_layer)
    : Layer(std::move(cc_layer)) {
  if (this->cc_layer()) {
    this->cc_layer()->SetSurfaceHitTestable(true);
  }
}

SurfaceLayer::~SurfaceLayer() = default;

cc::SurfaceLayer* SurfaceLayer::cc_layer() const {
  return static_cast<cc::SurfaceLayer*>(cc_layer_.get());
}

const viz::SurfaceId& SurfaceLayer::surface_id() const {
  return cc_layer() ? cc_layer()->surface_id() : surface_range_.end();
}

void SurfaceLayer::SetSurfaceId(const viz::SurfaceId& surface_id,
                                const cc::DeadlinePolicy& deadline_policy) {
  if (cc_layer()) {
    cc_layer()->SetSurfaceId(surface_id, deadline_policy);
    return;
  }
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
  SetDrawsContent(HasDrawableContent());
}

void SurfaceLayer::SetStretchContentToFillBounds(
    bool stretch_content_to_fill_bounds) {
  if (cc_layer()) {
    cc_layer()->SetStretchContentToFillBounds(stretch_content_to_fill_bounds);
    return;
  }
  if (stretch_content_to_fill_bounds_ == stretch_content_to_fill_bounds) {
    return;
  }
  stretch_content_to_fill_bounds_ = stretch_content_to_fill_bounds;
  NotifyPropertyChanged();
}

bool SurfaceLayer::stretch_content_to_fill_bounds() const {
  return cc_layer() ? cc_layer()->stretch_content_to_fill_bounds()
                    : stretch_content_to_fill_bounds_;
}

void SurfaceLayer::SetMayContainVideo(bool may_contain_video) {
  if (cc_layer()) {
    cc_layer()->SetMayContainVideo(may_contain_video);
    return;
  }
  // No slim implementation. This method probably should not exist.
  // crbug.com/1410291
}

void SurfaceLayer::SetOldestAcceptableFallback(
    const viz::SurfaceId& surface_id) {
  if (cc_layer()) {
    cc_layer()->SetOldestAcceptableFallback(surface_id);
    return;
  }
  // The fallback should never move backwards.
  DCHECK(!surface_range_.start() ||
         !surface_range_.start()->IsNewerThan(surface_id));
  if (surface_range_.start() == surface_id) {
    return;
  }

  SetSurfaceRange(viz::SurfaceRange(
      surface_id.is_valid() ? absl::optional<viz::SurfaceId>(surface_id)
                            : absl::nullopt,
      surface_range_.end()));
}

const absl::optional<viz::SurfaceId>& SurfaceLayer::oldest_acceptable_fallback()
    const {
  return cc_layer() ? cc_layer()->oldest_acceptable_fallback()
                    : surface_range_.start();
}

void SurfaceLayer::SetLayerTree(LayerTree* tree) {
  if (cc_layer()) {
    Layer::SetLayerTree(tree);
    return;
  }

  if (layer_tree() && surface_range_.IsValid()) {
    static_cast<LayerTreeImpl*>(layer_tree())
        ->RemoveSurfaceRange(surface_range_);
  }
  Layer::SetLayerTree(tree);
  if (layer_tree() && surface_range_.IsValid()) {
    static_cast<LayerTreeImpl*>(layer_tree())->AddSurfaceRange(surface_range_);
  }
}

void SurfaceLayer::SetSurfaceRange(const viz::SurfaceRange& surface_range) {
  DCHECK(!cc_layer());
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

bool SurfaceLayer::HasDrawableContent() const {
  return surface_range_.IsValid() && Layer::HasDrawableContent();
}

}  // namespace cc::slim
