// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/slim/nine_patch_layer.h"

#include <utility>

#include "cc/layers/nine_patch_generator.h"
#include "cc/layers/nine_patch_layer.h"
#include "cc/slim/features.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/texture_draw_quad.h"
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

}  // namespace cc::slim
