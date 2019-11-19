// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_PUSH_PROPERTIES_COUNTING_LAYER_H_
#define CC_TEST_PUSH_PROPERTIES_COUNTING_LAYER_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "cc/layers/layer.h"

namespace cc {

class LayerImpl;
class LayerTreeImpl;

class PushPropertiesCountingLayer : public Layer {
 public:
  static scoped_refptr<PushPropertiesCountingLayer> Create();

  PushPropertiesCountingLayer(const PushPropertiesCountingLayer&) = delete;
  PushPropertiesCountingLayer& operator=(const PushPropertiesCountingLayer&) =
      delete;

  // Layer implementation.
  void PushPropertiesTo(LayerImpl* layer) override;
  std::unique_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl) override;

  // Something to make this layer push properties, but no other layer.
  void MakePushProperties();

  size_t push_properties_count() const { return push_properties_count_; }
  void reset_push_properties_count() { push_properties_count_ = 0; }

 private:
  PushPropertiesCountingLayer();
  ~PushPropertiesCountingLayer() override;

  void AddPushPropertiesCount();

  size_t push_properties_count_ = 0;
};

}  // namespace cc

#endif  // CC_TEST_PUSH_PROPERTIES_COUNTING_LAYER_H_
