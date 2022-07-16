// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/shared_element_layer.h"

#include "cc/layers/shared_element_layer_impl.h"
#include "cc/trees/layer_tree_host.h"

#include "base/logging.h"

namespace cc {

scoped_refptr<SharedElementLayer> SharedElementLayer::Create(
    const viz::SharedElementResourceId& resource_id) {
  return base::WrapRefCounted(new SharedElementLayer(resource_id));
}

SharedElementLayer::SharedElementLayer(
    const viz::SharedElementResourceId& resource_id)
    : resource_id_(resource_id) {}

SharedElementLayer::~SharedElementLayer() = default;

std::unique_ptr<LayerImpl> SharedElementLayer::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return SharedElementLayerImpl::Create(tree_impl, id(), resource_id_);
}

}  // namespace cc
