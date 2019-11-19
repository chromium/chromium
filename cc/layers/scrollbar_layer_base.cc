// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/scrollbar_layer_base.h"

#include "cc/layers/scrollbar_layer_impl_base.h"

namespace cc {

ScrollbarLayerBase::ScrollbarLayerBase() {
  SetIsScrollbar(true);
}

ScrollbarLayerBase::~ScrollbarLayerBase() = default;

void ScrollbarLayerBase::SetScrollElementId(ElementId element_id) {
  if (element_id == scroll_element_id_)
    return;

  scroll_element_id_ = element_id;
  SetNeedsCommit();
}

void ScrollbarLayerBase::PushPropertiesTo(LayerImpl* layer) {
  Layer::PushPropertiesTo(layer);
  static_cast<ScrollbarLayerImplBase*>(layer)->SetScrollElementId(
      scroll_element_id_);
}

}  // namespace cc
