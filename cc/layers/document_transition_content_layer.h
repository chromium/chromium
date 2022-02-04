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
class DocumentTransitionContentLayerImpl;

// A layer that renders a texture cached in the Viz process.
class CC_EXPORT DocumentTransitionContentLayer : public Layer {
 public:
  static scoped_refptr<DocumentTransitionContentLayer> Create(
      const viz::SharedElementResourceId& resource_id);

  DocumentTransitionContentLayer(const DocumentTransitionContentLayer&) =
      delete;
  DocumentTransitionContentLayer& operator=(
      const DocumentTransitionContentLayer&) = delete;

  viz::SharedElementResourceId DocumentTransitionResourceId() const override;

  // Set the source opacity, which is the opacity specified on the shared
  // element that this layer draws. This is multiplied by any of this layer's
  // own opacity to give the effect that the source shared element was captured
  // with its opacity preserved.
  void SetSourceOpacity(float opacity);

  // Layer overrides.
  std::unique_ptr<LayerImpl> CreateLayerImpl(
      LayerTreeImpl* tree_impl) const override;
  void PushPropertiesTo(LayerImpl* layer,
                        const CommitState& commit_state,
                        const ThreadUnsafeCommitState& unsafe_state) override;

 protected:
  explicit DocumentTransitionContentLayer(
      const viz::SharedElementResourceId& resource_id);

 private:
  ~DocumentTransitionContentLayer() override;

  void PushLocalPropertiesTo(
      DocumentTransitionContentLayerImpl* layer_impl) const;

  const viz::SharedElementResourceId resource_id_;

  float source_opacity_ = 1.f;
};

}  // namespace cc

#endif  // CC_LAYERS_DOCUMENT_TRANSITION_CONTENT_LAYER_H_
