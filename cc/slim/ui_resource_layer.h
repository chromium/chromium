// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SLIM_UI_RESOURCE_LAYER_H_
#define CC_SLIM_UI_RESOURCE_LAYER_H_

#include "base/component_export.h"
#include "cc/slim/layer.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/point_f.h"

namespace cc {
class UIResourceLayer;
}

namespace cc::slim {

// Layer which to draws the contents of a single UIResource.
class COMPONENT_EXPORT(CC_SLIM) UIResourceLayer : public Layer {
 public:
  static scoped_refptr<UIResourceLayer> Create();

  // Sets the resource. If they don't exist already, the shared UI resource and
  // ID are generated and cached in a map in the associated UIResourceManager.
  // Currently, this resource will never be released by the UIResourceManager.
  void SetUIResourceId(int id);

  // An alternative way of setting the resource where an ID is used directly. If
  // you use this method, you are responsible for updating the ID if the layer
  // moves between compositors.
  void SetBitmap(const SkBitmap& bitmap);

  // Sets a UV transform to be used at draw time. Defaults to (0, 0) and (1, 1).
  void SetUV(const gfx::PointF& top_left, const gfx::PointF& bottom_right);

  // Sets an opacity value per vertex. It will be multiplied by the layer
  // opacity value.
  void SetVertexOpacity(float bottom_left,
                        float top_left,
                        float top_right,
                        float bottom_right);

 protected:
  explicit UIResourceLayer(scoped_refptr<cc::UIResourceLayer> cc_layer);
  ~UIResourceLayer() override;

 private:
  cc::UIResourceLayer* cc_layer() const;
};

}  // namespace cc::slim

#endif  // CC_SLIM_UI_RESOURCE_LAYER_H_
