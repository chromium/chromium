// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/document_transition_content_layer.h"

#include <memory>

#include "base/logging.h"
#include "cc/layers/document_transition_content_layer_impl.h"
#include "cc/trees/layer_tree_host.h"

namespace cc {

scoped_refptr<DocumentTransitionContentLayer>
DocumentTransitionContentLayer::Create(
    const viz::SharedElementResourceId& resource_id,
    bool is_live_content_layer) {
  return base::WrapRefCounted(
      new DocumentTransitionContentLayer(resource_id, is_live_content_layer));
}

DocumentTransitionContentLayer::DocumentTransitionContentLayer(
    const viz::SharedElementResourceId& resource_id,
    bool is_live_content_layer)
    : resource_id_(resource_id),
      is_live_content_layer_(is_live_content_layer) {}

DocumentTransitionContentLayer::~DocumentTransitionContentLayer() = default;

viz::SharedElementResourceId
DocumentTransitionContentLayer::DocumentTransitionResourceId() const {
  return resource_id_;
}

std::unique_ptr<LayerImpl> DocumentTransitionContentLayer::CreateLayerImpl(
    LayerTreeImpl* tree_impl) const {
  return DocumentTransitionContentLayerImpl::Create(
      tree_impl, id(), resource_id_, is_live_content_layer_);
}

}  // namespace cc
