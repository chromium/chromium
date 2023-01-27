// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/slim/ui_resource_layer.h"

#include <utility>

#include "cc/layers/ui_resource_layer.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"

namespace cc::slim {

// static
scoped_refptr<UIResourceLayer> UIResourceLayer::Create() {
  scoped_refptr<cc::UIResourceLayer> cc_layer;
  cc_layer = cc::UIResourceLayer::Create();
  return base::AdoptRef(new UIResourceLayer(std::move(cc_layer)));
}

UIResourceLayer::UIResourceLayer(scoped_refptr<cc::UIResourceLayer> cc_layer)
    : Layer(std::move(cc_layer)) {}

UIResourceLayer::~UIResourceLayer() = default;

cc::UIResourceLayer* UIResourceLayer::cc_layer() const {
  return static_cast<cc::UIResourceLayer*>(cc_layer_.get());
}

void UIResourceLayer::SetUIResourceId(int id) {
  cc_layer()->SetUIResourceId(id);
}

void UIResourceLayer::SetBitmap(const SkBitmap& bitmap) {
  cc_layer()->SetBitmap(bitmap);
}

void UIResourceLayer::SetUV(const gfx::PointF& top_left,
                            const gfx::PointF& bottom_right) {
  cc_layer()->SetUV(top_left, bottom_right);
}

void UIResourceLayer::SetVertexOpacity(float bottom_left,
                                       float top_left,
                                       float top_right,
                                       float bottom_right) {
  cc_layer()->SetVertexOpacity(bottom_left, top_left, top_right, bottom_right);
}

}  // namespace cc::slim
