// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_SHARED_ELEMENT_LAYER_H_
#define CC_LAYERS_SHARED_ELEMENT_LAYER_H_

#include "base/logging.h"

#include "cc/cc_export.h"
#include "cc/layers/layer.h"
#include "components/viz/common/shared_element_resource_id.h"

namespace cc {

// A layer that renders a texture cached in the Viz process.
class CC_EXPORT SharedElementLayer : public Layer {
 public:
  static scoped_refptr<SharedElementLayer> Create(
      const viz::SharedElementResourceId& resource_id);

  SharedElementLayer(const SharedElementLayer&) = delete;
  SharedElementLayer& operator=(const SharedElementLayer&) = delete;

  const viz::SharedElementResourceId& resource_id() const {
    return resource_id_;
  }

  // Layer overrides.
  std::unique_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl) override;

 protected:
  explicit SharedElementLayer(const viz::SharedElementResourceId& resource_id);

 private:
  ~SharedElementLayer() override;

  const viz::SharedElementResourceId resource_id_;
};

}  // namespace cc

#endif  // CC_LAYERS_SHARED_ELEMENT_LAYER_H_
