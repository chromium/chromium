// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_DOCUMENT_TRANSITION_CONTENT_LAYER_IMPL_H_
#define CC_LAYERS_DOCUMENT_TRANSITION_CONTENT_LAYER_IMPL_H_

#include <memory>

#include "cc/cc_export.h"
#include "cc/layers/layer_impl.h"
#include "components/viz/common/shared_element_resource_id.h"

namespace cc {

class CC_EXPORT DocumentTransitionContentLayerImpl : public LayerImpl {
 public:
  static std::unique_ptr<DocumentTransitionContentLayerImpl> Create(
      LayerTreeImpl* tree_impl,
      int id,
      const viz::SharedElementResourceId& resource_id,
      bool is_live_content_layer);

  DocumentTransitionContentLayerImpl(
      const DocumentTransitionContentLayerImpl&) = delete;
  ~DocumentTransitionContentLayerImpl() override;

  DocumentTransitionContentLayerImpl& operator=(
      const DocumentTransitionContentLayerImpl&) = delete;

  // LayerImpl overrides.
  std::unique_ptr<LayerImpl> CreateLayerImpl(
      LayerTreeImpl* tree_impl) const override;
  void AppendQuads(viz::CompositorRenderPass* render_pass,
                   AppendQuadsData* append_quads_data) override;

  void NotifyKnownResourceIdsBeforeAppendQuads(
      const base::flat_set<viz::SharedElementResourceId>& known_resource_ids)
      override;

 protected:
  DocumentTransitionContentLayerImpl(
      LayerTreeImpl* tree_impl,
      int id,
      const viz::SharedElementResourceId& resource_id,
      bool is_live_content_layer);

 private:
  const char* LayerTypeAsString() const override;

  const viz::SharedElementResourceId resource_id_;
  const bool is_live_content_layer_;
  bool skip_unseen_resource_quads_ = false;
};

}  // namespace cc

#endif  // CC_LAYERS_DOCUMENT_TRANSITION_CONTENT_LAYER_IMPL_H_
