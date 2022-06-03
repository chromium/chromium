// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/push_properties_counting_layer_impl.h"
#include "base/memory/ptr_util.h"

namespace cc {

// static
std::unique_ptr<PushPropertiesCountingLayerImpl>
PushPropertiesCountingLayerImpl::Create(LayerTreeImpl* tree_impl, int id) {
  return base::WrapUnique(new PushPropertiesCountingLayerImpl(tree_impl, id));
}

PushPropertiesCountingLayerImpl::PushPropertiesCountingLayerImpl(
    LayerTreeImpl* tree_impl,
    int id)
    : LayerImpl(tree_impl, id), push_properties_count_(0) {}

PushPropertiesCountingLayerImpl::~PushPropertiesCountingLayerImpl() = default;

void PushPropertiesCountingLayerImpl::PushPropertiesTo(LayerImpl* layer) {
  LayerImpl::PushPropertiesTo(layer);
  push_properties_count_++;
  // Push state to the active tree because we can only access it from there.
  static_cast<PushPropertiesCountingLayerImpl*>(layer)->push_properties_count_ =
      push_properties_count_;
}

std::unique_ptr<LayerImpl> PushPropertiesCountingLayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return PushPropertiesCountingLayerImpl::Create(tree_impl, LayerImpl::id());
}

}  // namespace cc
