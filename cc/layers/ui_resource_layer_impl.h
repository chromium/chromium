// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_UI_RESOURCE_LAYER_IMPL_H_
#define CC_LAYERS_UI_RESOURCE_LAYER_IMPL_H_

#include <string>

#include "base/memory/ptr_util.h"
#include "cc/cc_export.h"
#include "cc/layers/layer_impl.h"
#include "cc/resources/ui_resource_client.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class DictionaryValue;
}

namespace viz {
class ClientResourceProvider;
}

namespace cc {

class CC_EXPORT UIResourceLayerImpl : public LayerImpl {
 public:
  static std::unique_ptr<UIResourceLayerImpl> Create(LayerTreeImpl* tree_impl,
                                                     int id) {
    return base::WrapUnique(new UIResourceLayerImpl(tree_impl, id));
  }
  UIResourceLayerImpl(const UIResourceLayerImpl&) = delete;
  ~UIResourceLayerImpl() override;

  UIResourceLayerImpl& operator=(const UIResourceLayerImpl&) = delete;

  void SetUIResourceId(UIResourceId uid);

  void SetImageBounds(const gfx::Size& image_bounds);

  // Sets a UV transform to be used at draw time. Defaults to (0, 0) and (1, 1).
  void SetUV(const gfx::PointF& top_left, const gfx::PointF& bottom_right);

  // Sets an opacity value per vertex. It will be multiplied by the layer
  // opacity value.
  void SetVertexOpacity(const float vertex_opacity[4]);

  std::unique_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl) override;
  void PushPropertiesTo(LayerImpl* layer) override;

  bool WillDraw(DrawMode draw_mode,
                viz::ClientResourceProvider* resource_provider) override;
  void AppendQuads(viz::RenderPass* render_pass,
                   AppendQuadsData* append_quads_data) override;

  std::unique_ptr<base::DictionaryValue> LayerAsJson() const override;

 protected:
  UIResourceLayerImpl(LayerTreeImpl* tree_impl, int id);

  // The size of the resource bitmap in pixels.
  gfx::Size image_bounds_;

  UIResourceId ui_resource_id_;

  gfx::PointF uv_top_left_;
  gfx::PointF uv_bottom_right_;
  float vertex_opacity_[4];

 private:
  const char* LayerTypeAsString() const override;
};

}  // namespace cc

#endif  // CC_LAYERS_UI_RESOURCE_LAYER_IMPL_H_
