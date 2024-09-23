// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_VIEW_TRANSITION_CONTENT_LAYER_H_
#define CC_LAYERS_VIEW_TRANSITION_CONTENT_LAYER_H_

#include <memory>

#include "base/logging.h"
#include "cc/cc_export.h"
#include "cc/layers/layer.h"
#include "components/viz/common/view_transition_element_resource_id.h"

namespace cc {

// A layer that renders a texture cached in the Viz process.
class CC_EXPORT ViewTransitionContentLayer : public Layer {
 public:
  static scoped_refptr<ViewTransitionContentLayer> Create(
      const viz::ViewTransitionElementResourceId& resource_id,
      bool is_live_content_layer);

  ViewTransitionContentLayer(const ViewTransitionContentLayer&) = delete;
  ViewTransitionContentLayer& operator=(const ViewTransitionContentLayer&) =
      delete;

  viz::ViewTransitionElementResourceId ViewTransitionResourceId()
      const override;

  // Layer overrides.
  std::unique_ptr<LayerImpl> CreateLayerImpl(
      LayerTreeImpl* tree_impl) const override;

  bool is_live_content_layer_for_testing() const {
    return is_live_content_layer_;
  }

 protected:
  explicit ViewTransitionContentLayer(
      const viz::ViewTransitionElementResourceId& resource_id,
      bool is_live_content_layer);

 private:
  ~ViewTransitionContentLayer() override;

  const viz::ViewTransitionElementResourceId resource_id_;
  const bool is_live_content_layer_;
};

}  // namespace cc

#endif  // CC_LAYERS_VIEW_TRANSITION_CONTENT_LAYER_H_
