// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_DOCUMENT_TRANSITION_CONTENT_LAYER_H_
#define CC_LAYERS_DOCUMENT_TRANSITION_CONTENT_LAYER_H_

#include <memory>

#include "base/logging.h"
#include "cc/cc_export.h"
#include "cc/layers/layer.h"
#include "components/viz/common/shared_element_resource_id.h"

namespace cc {

// A layer that renders a texture cached in the Viz process.
class CC_EXPORT DocumentTransitionContentLayer : public Layer {
 public:
  static scoped_refptr<DocumentTransitionContentLayer> Create(
      const viz::SharedElementResourceId& resource_id,
      bool is_live_content_layer);

  DocumentTransitionContentLayer(const DocumentTransitionContentLayer&) =
      delete;
  DocumentTransitionContentLayer& operator=(
      const DocumentTransitionContentLayer&) = delete;

  viz::SharedElementResourceId DocumentTransitionResourceId() const override;

  // Layer overrides.
  std::unique_ptr<LayerImpl> CreateLayerImpl(
      LayerTreeImpl* tree_impl) const override;

 protected:
  explicit DocumentTransitionContentLayer(
      const viz::SharedElementResourceId& resource_id,
      bool is_live_content_layer);

 private:
  ~DocumentTransitionContentLayer() override;

  const viz::SharedElementResourceId resource_id_;
  const bool is_live_content_layer_;
};

}  // namespace cc

#endif  // CC_LAYERS_DOCUMENT_TRANSITION_CONTENT_LAYER_H_
