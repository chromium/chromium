// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/slim/ui_resource_layer.h"

#include <utility>

#include "cc/layers/ui_resource_layer.h"
#include "cc/slim/features.h"
#include "cc/slim/layer_tree_impl.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"

namespace cc::slim {

// static
scoped_refptr<UIResourceLayer> UIResourceLayer::Create() {
  scoped_refptr<cc::UIResourceLayer> cc_layer;
  if (!features::IsSlimCompositorEnabled()) {
    cc_layer = cc::UIResourceLayer::Create();
  }
  return base::AdoptRef(new UIResourceLayer(std::move(cc_layer)));
}

UIResourceLayer::UIResourceLayer(scoped_refptr<cc::UIResourceLayer> cc_layer)
    : Layer(std::move(cc_layer)) {}

UIResourceLayer::~UIResourceLayer() = default;

cc::UIResourceLayer* UIResourceLayer::cc_layer() const {
  return static_cast<cc::UIResourceLayer*>(cc_layer_.get());
}

void UIResourceLayer::SetUIResourceId(cc::UIResourceId id) {
  if (cc_layer()) {
    cc_layer()->SetUIResourceId(id);
    return;
  }
  bitmap_.reset();
  if (resource_id_ == id) {
    return;
  }

  SetUIResourceIdInternal(id);
}

void UIResourceLayer::SetBitmap(const SkBitmap& bitmap) {
  if (cc_layer()) {
    cc_layer()->SetBitmap(bitmap);
    return;
  }
  bitmap_ = bitmap;
  if (!layer_tree()) {
    return;
  }

  SetUIResourceIdInternal(static_cast<LayerTreeImpl*>(layer_tree())
                              ->GetUIResourceManager()
                              ->GetOrCreateUIResource(bitmap));
}

void UIResourceLayer::SetUV(const gfx::PointF& top_left,
                            const gfx::PointF& bottom_right) {
  if (cc_layer()) {
    cc_layer()->SetUV(top_left, bottom_right);
    return;
  }
  if (uv_.origin() == top_left && uv_.bottom_right() == bottom_right) {
    return;
  }

  uv_.set_origin(top_left);
  uv_.set_width(bottom_right.x() - top_left.x());
  uv_.set_height(bottom_right.y() - top_left.y());
  DCHECK_EQ(uv_.bottom_right(), bottom_right);
  NotifyPropertyChanged();
}

void UIResourceLayer::SetVertexOpacity(float bottom_left,
                                       float top_left,
                                       float top_right,
                                       float bottom_right) {
  if (cc_layer()) {
    cc_layer()->SetVertexOpacity(bottom_left, top_left, top_right,
                                 bottom_right);
    return;
  }
  // Indexing according to the quad vertex generation:
  // 1--2
  // |  |
  // 0--3
  auto& opacity = vertex_opacity_;
  if (opacity[0] == bottom_left && opacity[1] == top_left &&
      opacity[2] == top_right && opacity[3] == bottom_right) {
    return;
  }

  opacity[0] = bottom_left;
  opacity[1] = top_left;
  opacity[2] = top_right;
  opacity[3] = bottom_right;
  NotifyPropertyChanged();
}

void UIResourceLayer::SetLayerTree(LayerTree* tree) {
  if (cc_layer()) {
    Layer::SetLayerTree(tree);
    return;
  }
  if (tree == layer_tree()) {
    return;
  }

  Layer::SetLayerTree(tree);
  RefreshResource();
  SetDrawsContent(HasDrawableContent());
}

bool UIResourceLayer::HasDrawableContent() const {
  return resource_id_ && Layer::HasDrawableContent();
}

void UIResourceLayer::RefreshResource() {
  if (!bitmap_.empty()) {
    SetBitmap(bitmap_);
  }
}

void UIResourceLayer::SetUIResourceIdInternal(cc::UIResourceId resource_id) {
  if (resource_id_ == resource_id) {
    return;
  }
  resource_id_ = resource_id;
  SetDrawsContent(HasDrawableContent());
  NotifyPropertyChanged();
}

}  // namespace cc::slim
