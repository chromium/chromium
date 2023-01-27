// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/slim/surface_layer.h"

#include <algorithm>
#include <utility>

#include "cc/layers/surface_layer.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/surface_draw_quad.h"
#include "components/viz/common/surfaces/surface_range.h"

namespace cc::slim {

// static
scoped_refptr<SurfaceLayer> SurfaceLayer::Create() {
  scoped_refptr<cc::SurfaceLayer> cc_layer;
  cc_layer = cc::SurfaceLayer::Create();
  return base::AdoptRef(new SurfaceLayer(std::move(cc_layer)));
}

SurfaceLayer::SurfaceLayer(scoped_refptr<cc::SurfaceLayer> cc_layer)
    : Layer(std::move(cc_layer)) {
  this->cc_layer()->SetSurfaceHitTestable(true);
}

SurfaceLayer::~SurfaceLayer() = default;

cc::SurfaceLayer* SurfaceLayer::cc_layer() const {
  return static_cast<cc::SurfaceLayer*>(cc_layer_.get());
}

const viz::SurfaceId& SurfaceLayer::surface_id() const {
  return cc_layer()->surface_id();
}

void SurfaceLayer::SetSurfaceId(const viz::SurfaceId& surface_id,
                                const cc::DeadlinePolicy& deadline_policy) {
  cc_layer()->SetSurfaceId(surface_id, deadline_policy);
}

void SurfaceLayer::SetStretchContentToFillBounds(
    bool stretch_content_to_fill_bounds) {
  cc_layer()->SetStretchContentToFillBounds(stretch_content_to_fill_bounds);
}

bool SurfaceLayer::stretch_content_to_fill_bounds() const {
  return cc_layer()->stretch_content_to_fill_bounds();
}

void SurfaceLayer::SetMayContainVideo(bool may_contain_video) {
  cc_layer()->SetMayContainVideo(may_contain_video);
}

void SurfaceLayer::SetOldestAcceptableFallback(
    const viz::SurfaceId& surface_id) {
  cc_layer()->SetOldestAcceptableFallback(surface_id);
}

const absl::optional<viz::SurfaceId>& SurfaceLayer::oldest_acceptable_fallback()
    const {
  return cc_layer()->oldest_acceptable_fallback();
}

}  // namespace cc::slim
