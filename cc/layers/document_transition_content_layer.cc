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
    const viz::SharedElementResourceId& resource_id) {
  return base::WrapRefCounted(new DocumentTransitionContentLayer(resource_id));
}

DocumentTransitionContentLayer::DocumentTransitionContentLayer(
    const viz::SharedElementResourceId& resource_id)
    : resource_id_(resource_id) {}

DocumentTransitionContentLayer::~DocumentTransitionContentLayer() = default;

viz::SharedElementResourceId
DocumentTransitionContentLayer::DocumentTransitionResourceId() const {
  return resource_id_;
}

void DocumentTransitionContentLayer::SetSourceOpacity(float opacity) {
  source_opacity_ = opacity;
  SetNeedsPushProperties();
}

void DocumentTransitionContentLayer::PushPropertiesTo(
    LayerImpl* layer,
    const CommitState& commit_state,
    const ThreadUnsafeCommitState& unsafe_state) {
  Layer::PushPropertiesTo(layer, commit_state, unsafe_state);

  auto* content_layer_impl =
      static_cast<DocumentTransitionContentLayerImpl*>(layer);
  PushLocalPropertiesTo(content_layer_impl);
}

void DocumentTransitionContentLayer::PushLocalPropertiesTo(
    DocumentTransitionContentLayerImpl* layer_impl) const {
  layer_impl->SetSourceOpacity(source_opacity_);
}

std::unique_ptr<LayerImpl> DocumentTransitionContentLayer::CreateLayerImpl(
    LayerTreeImpl* tree_impl) const {
  auto layer =
      DocumentTransitionContentLayerImpl::Create(tree_impl, id(), resource_id_);
  PushLocalPropertiesTo(layer.get());
  return layer;
}

}  // namespace cc
