// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_NINE_PATCH_LAYER_IMPL_H_
#define CC_LAYERS_NINE_PATCH_LAYER_IMPL_H_

#include <memory>
#include <string>

#include "base/memory/ptr_util.h"
#include "cc/cc_export.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/nine_patch_generator.h"
#include "cc/layers/ui_resource_layer_impl.h"
#include "cc/resources/ui_resource_client.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace cc {

class CC_EXPORT NinePatchLayerImpl : public UIResourceLayerImpl {
 public:
  static std::unique_ptr<NinePatchLayerImpl> Create(LayerTreeImpl* tree_impl,
                                                    int id) {
    return base::WrapUnique(new NinePatchLayerImpl(tree_impl, id));
  }
  NinePatchLayerImpl(const NinePatchLayerImpl&) = delete;
  ~NinePatchLayerImpl() override;

  NinePatchLayerImpl& operator=(const NinePatchLayerImpl&) = delete;

  // For parameter meanings, see the declaration of NinePatchGenerator.
  void SetLayout(const gfx::Rect& image_aperture,
                 const gfx::Rect& border,
                 const gfx::Rect& layer_occlusion,
                 bool fill_center,
                 bool nearest_neighbor);

  mojom::LayerType GetLayerType() const override;
  std::unique_ptr<LayerImpl> CreateLayerImpl(
      LayerTreeImpl* tree_impl) const override;
  void PushPropertiesTo(LayerImpl* layer) override;

  void AppendQuads(viz::CompositorRenderPass* render_pass,
                   AppendQuadsData* append_quads_data) override;

  void AsValueInto(base::trace_event::TracedValue* state) const override;

 protected:
  NinePatchLayerImpl(LayerTreeImpl* tree_impl, int id);

 private:
  NinePatchGenerator quad_generator_;
};

}  // namespace cc

#endif  // CC_LAYERS_NINE_PATCH_LAYER_IMPL_H_
