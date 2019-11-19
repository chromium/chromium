// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/mirror_layer.h"

#include "cc/layers/mirror_layer_impl.h"

namespace cc {

std::unique_ptr<LayerImpl> MirrorLayer::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return MirrorLayerImpl::Create(tree_impl, id());
}

void MirrorLayer::PushPropertiesTo(LayerImpl* layer) {
  Layer::PushPropertiesTo(layer);

  auto* mirror_layer = static_cast<MirrorLayerImpl*>(layer);
  mirror_layer->SetMirroredLayerId(mirrored_layer_->id());
}

void MirrorLayer::SetLayerTreeHost(LayerTreeHost* host) {
#if DCHECK_IS_ON()
  if (host && host != layer_tree_host()) {
    for (auto* p = parent(); p; p = p->parent())
      DCHECK_NE(p, mirrored_layer_.get());
  }
#endif

  Layer::SetLayerTreeHost(host);
}

scoped_refptr<MirrorLayer> MirrorLayer::Create(
    scoped_refptr<Layer> mirrored_layer) {
  return base::WrapRefCounted(new MirrorLayer(std::move(mirrored_layer)));
}

MirrorLayer::MirrorLayer(scoped_refptr<Layer> mirrored_layer)
    : mirrored_layer_(std::move(mirrored_layer)) {
  DCHECK(mirrored_layer_);
  mirrored_layer_->IncrementMirrorCount();
}

MirrorLayer::~MirrorLayer() {
  mirrored_layer_->DecrementMirrorCount();
}

}  // namespace cc
