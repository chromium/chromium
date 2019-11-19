// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_SCROLLBAR_LAYER_BASE_H_
#define CC_LAYERS_SCROLLBAR_LAYER_BASE_H_

#include "cc/cc_export.h"
#include "cc/layers/layer.h"

namespace cc {

class CC_EXPORT ScrollbarLayerBase : public Layer {
 public:
  void SetScrollElementId(ElementId element_id);

  void PushPropertiesTo(LayerImpl* layer) override;

 protected:
  ScrollbarLayerBase();
  ~ScrollbarLayerBase() override;

 private:
  ElementId scroll_element_id_;
};

}  // namespace cc

#endif  // CC_LAYERS_SCROLLBAR_LAYER_BASE_H_
