// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/solid_color_layer.h"

#include <memory>

#include "cc/layers/solid_color_layer_impl.h"

namespace cc {

std::unique_ptr<LayerImpl> SolidColorLayer::CreateLayerImpl(
    LayerTreeImpl* tree_impl) const {
  return SolidColorLayerImpl::Create(tree_impl, id());
}

scoped_refptr<SolidColorLayer> SolidColorLayer::Create() {
  return base::WrapRefCounted(new SolidColorLayer());
}

SolidColorLayer::SolidColorLayer() = default;

SolidColorLayer::~SolidColorLayer() = default;

void SolidColorLayer::SetBackgroundColor(SkColor4f color) {
  SetContentsOpaque(color.isOpaque());
  Layer::SetBackgroundColor(color);
}

}  // namespace cc
