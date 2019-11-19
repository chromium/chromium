// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_PUSH_PROPERTIES_COUNTING_LAYER_IMPL_H_
#define CC_TEST_PUSH_PROPERTIES_COUNTING_LAYER_IMPL_H_

#include <memory>

#include "cc/layers/layer_impl.h"

namespace cc {

class LayerTreeImpl;

class PushPropertiesCountingLayerImpl : public LayerImpl {
 public:
  static std::unique_ptr<PushPropertiesCountingLayerImpl> Create(
      LayerTreeImpl* tree_impl,
      int id);
  PushPropertiesCountingLayerImpl(const PushPropertiesCountingLayerImpl&) =
      delete;
  ~PushPropertiesCountingLayerImpl() override;

  PushPropertiesCountingLayerImpl& operator=(
      const PushPropertiesCountingLayerImpl&) = delete;

  // LayerImpl implementation.
  void PushPropertiesTo(LayerImpl* layer) override;
  std::unique_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl) override;

  size_t push_properties_count() const { return push_properties_count_; }
  void reset_push_properties_count() { push_properties_count_ = 0; }

 private:
  PushPropertiesCountingLayerImpl(LayerTreeImpl* tree_impl, int id);

  size_t push_properties_count_;
};

}  // namespace cc

#endif  // CC_TEST_PUSH_PROPERTIES_COUNTING_LAYER_IMPL_H_
