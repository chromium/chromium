// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_VIEW_TRANSITION_CONTENT_LAYER_IMPL_H_
#define CC_LAYERS_VIEW_TRANSITION_CONTENT_LAYER_IMPL_H_

#include <memory>

#include "cc/cc_export.h"
#include "cc/layers/layer_impl.h"
#include "components/viz/common/view_transition_element_resource_id.h"

namespace cc {

class CC_EXPORT ViewTransitionContentLayerImpl : public LayerImpl {
 public:
  static std::unique_ptr<ViewTransitionContentLayerImpl> Create(
      LayerTreeImpl* tree_impl,
      int id,
      const viz::ViewTransitionElementResourceId& resource_id,
      bool is_live_content_layer,
      const gfx::RectF& max_extents_rect);

  ViewTransitionContentLayerImpl(const ViewTransitionContentLayerImpl&) =
      delete;
  ~ViewTransitionContentLayerImpl() override;

  ViewTransitionContentLayerImpl& operator=(
      const ViewTransitionContentLayerImpl&) = delete;

  // LayerImpl overrides.
  mojom::LayerType GetLayerType() const override;
  std::unique_ptr<LayerImpl> CreateLayerImpl(
      LayerTreeImpl* tree_impl) const override;
  void AppendQuads(viz::CompositorRenderPass* render_pass,
                   AppendQuadsData* append_quads_data) override;

  void NotifyKnownResourceIdsBeforeAppendQuads(
      const base::flat_set<viz::ViewTransitionElementResourceId>&
          known_resource_ids) override;

  viz::ViewTransitionElementResourceId ViewTransitionResourceId()
      const override;

  void SetMaxExtentsRect(const gfx::RectF& max_extents_rect);
  void PushPropertiesTo(LayerImpl* layer) override;
  void SetOriginatingSurfaceContentRect(
      const gfx::Rect&
          originating_surface_content_rect_in_layer_coordinate_space);

 protected:
  ViewTransitionContentLayerImpl(
      LayerTreeImpl* tree_impl,
      int id,
      const viz::ViewTransitionElementResourceId& resource_id,
      bool is_live_content_layer,
      const gfx::RectF& max_extents_rect);

 private:
  const viz::ViewTransitionElementResourceId resource_id_;
  const bool is_live_content_layer_;
  bool skip_unseen_resource_quads_ = false;

  // max_extents_rect_ is the maximum possible size of the originating layer, as
  // known to blink, in the originating layer's coordinate space.
  gfx::RectF max_extents_rect_in_originating_layer_coordinate_space_;
  // actual_extents_rect_ is the actual size of the surface, computed by CC, in
  // this layer's coordinate space.
  gfx::Rect actual_extents_rect_;
};

}  // namespace cc

#endif  // CC_LAYERS_VIEW_TRANSITION_CONTENT_LAYER_IMPL_H_
