// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_UI_RESOURCE_LAYER_H_
#define CC_LAYERS_UI_RESOURCE_LAYER_H_

#include <memory>

#include "cc/cc_export.h"
#include "cc/layers/layer.h"
#include "cc/resources/ui_resource_client.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {

class LayerTreeHost;

class CC_EXPORT UIResourceLayer : public Layer {
 public:
  static scoped_refptr<UIResourceLayer> Create();

  UIResourceLayer(const UIResourceLayer&) = delete;
  UIResourceLayer& operator=(const UIResourceLayer&) = delete;

  void PushPropertiesTo(LayerImpl* layer) override;

  void SetLayerTreeHost(LayerTreeHost* host) override;

  // Sets the resource. If they don't exist already, the shared UI resource and
  // ID are generated and cached in a map in the associated UIResourceManager.
  // Currently, this resource will never be released by the UIResourceManager.
  void SetBitmap(const SkBitmap& skbitmap);

  // An alternative way of setting the resource where an ID is used directly. If
  // you use this method, you are responsible for updating the ID if the layer
  // moves between compositors.
  void SetUIResourceId(UIResourceId resource_id);

  // Sets a UV transform to be used at draw time. Defaults to (0, 0) and (1, 1).
  void SetUV(const gfx::PointF& top_left, const gfx::PointF& bottom_right);

  // Sets an opacity value per vertex. It will be multiplied by the layer
  // opacity value.
  void SetVertexOpacity(float bottom_left,
                        float top_left,
                        float top_right,
                        float bottom_right);

 protected:
  UIResourceLayer();
  ~UIResourceLayer() override;

  bool HasDrawableContent() const override;

  UIResourceId resource_id() const { return resource_id_; }

 private:
  std::unique_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl) override;
  void RecreateUIResourceIdFromBitmap();
  void SetUIResourceIdInternal(UIResourceId resource_id);

  // The resource ID will be zero when it's unset or when there's no associated
  // LayerTreeHost.
  UIResourceId resource_id_ = 0;
  SkBitmap bitmap_;

  gfx::PointF uv_top_left_;
  gfx::PointF uv_bottom_right_;
  float vertex_opacity_[4];
};

}  // namespace cc

#endif  // CC_LAYERS_UI_RESOURCE_LAYER_H_
