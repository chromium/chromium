// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/view_transition_content_layer.h"

#include <memory>

#include "base/logging.h"
#include "cc/layers/view_transition_content_layer_impl.h"
#include "cc/trees/layer_tree_host.h"

namespace cc {

scoped_refptr<ViewTransitionContentLayer> ViewTransitionContentLayer::Create(
    const viz::ViewTransitionElementResourceId& resource_id,
    bool is_live_content_layer) {
  return base::WrapRefCounted(
      new ViewTransitionContentLayer(resource_id, is_live_content_layer));
}

ViewTransitionContentLayer::ViewTransitionContentLayer(
    const viz::ViewTransitionElementResourceId& resource_id,
    bool is_live_content_layer)
    : resource_id_(resource_id),
      is_live_content_layer_(is_live_content_layer) {}

ViewTransitionContentLayer::~ViewTransitionContentLayer() = default;

viz::ViewTransitionElementResourceId
ViewTransitionContentLayer::ViewTransitionResourceId() const {
  return resource_id_;
}

std::unique_ptr<LayerImpl> ViewTransitionContentLayer::CreateLayerImpl(
    LayerTreeImpl* tree_impl) const {
  return ViewTransitionContentLayerImpl::Create(
      tree_impl, id(), resource_id_, is_live_content_layer_, gfx::RectF());
}

void ViewTransitionContentLayer::SetMaxExtentsRectInOriginatingLayerSpace(
    const gfx::RectF& max_extents_rect) {
  if (max_extents_rect == max_extents_rect_.Read(*this)) {
    return;
  }

  max_extents_rect_.Write(*this) = max_extents_rect;
  SetNeedsCommit();
}

void ViewTransitionContentLayer::PushPropertiesTo(
    LayerImpl* layer,
    const CommitState& commit_state,
    const ThreadUnsafeCommitState& unsafe_state) {
  Layer::PushPropertiesTo(layer, commit_state, unsafe_state);
  static_cast<ViewTransitionContentLayerImpl*>(layer)->SetMaxExtentsRect(
      max_extents_rect_.Read(*this));
}

}  // namespace cc
