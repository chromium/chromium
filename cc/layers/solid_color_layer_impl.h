// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_SOLID_COLOR_LAYER_IMPL_H_
#define CC_LAYERS_SOLID_COLOR_LAYER_IMPL_H_

#include <memory>

#include "base/memory/ptr_util.h"
#include "cc/cc_export.h"
#include "cc/layers/layer_impl.h"

namespace cc {

class CC_EXPORT SolidColorLayerImpl : public LayerImpl {
 public:
  static std::unique_ptr<SolidColorLayerImpl> Create(LayerTreeImpl* tree_impl,
                                                     int id) {
    return base::WrapUnique(new SolidColorLayerImpl(tree_impl, id));
  }

  SolidColorLayerImpl(const SolidColorLayerImpl&) = delete;
  SolidColorLayerImpl& operator=(const SolidColorLayerImpl&) = delete;

  static void AppendSolidQuads(viz::CompositorRenderPass* render_pass,
                               const Occlusion& occlusion_in_layer_space,
                               viz::SharedQuadState* shared_quad_state,
                               const gfx::Rect& visible_layer_rect,
                               SkColor4f color,
                               bool force_anti_aliasing_off,
                               SkBlendMode effect_blend_mode,
                               AppendQuadsData* append_quads_data);

  ~SolidColorLayerImpl() override;

  // LayerImpl overrides.
  mojom::LayerType GetLayerType() const override;
  std::unique_ptr<LayerImpl> CreateLayerImpl(
      LayerTreeImpl* tree_impl) const override;
  void AppendQuads(viz::CompositorRenderPass* render_pass,
                   AppendQuadsData* append_quads_data) override;

 protected:
  SolidColorLayerImpl(LayerTreeImpl* tree_impl, int id);
};

}  // namespace cc

#endif  // CC_LAYERS_SOLID_COLOR_LAYER_IMPL_H_
