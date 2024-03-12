// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SLIM_UI_RESOURCE_LAYER_H_
#define CC_SLIM_UI_RESOURCE_LAYER_H_

#include "base/component_export.h"
#include "base/gtest_prod_util.h"
#include "cc/resources/ui_resource_client.h"
#include "cc/slim/layer.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/point_f.h"

namespace cc::slim {

// Layer which to draws the contents of a single UIResource.
class COMPONENT_EXPORT(CC_SLIM) UIResourceLayer : public Layer {
 public:
  static scoped_refptr<UIResourceLayer> Create();

  // Sets the resource. If they don't exist already, the shared UI resource and
  // ID are generated and cached in a map in the associated UIResourceManager.
  // Currently, this resource will never be released by the UIResourceManager.
  void SetUIResourceId(cc::UIResourceId id);

  // An alternative way of setting the resource where an ID is used directly. If
  // you use this method, you are responsible for updating the ID if the layer
  // moves between compositors.
  void SetBitmap(const SkBitmap& bitmap);

  // Sets a UV transform to be used at draw time. Defaults to (0, 0) and (1, 1).
  void SetUV(const gfx::PointF& top_left, const gfx::PointF& bottom_right);


  // Layer implementation.
  void SetLayerTree(LayerTree* tree) override;

 protected:
  FRIEND_TEST_ALL_PREFIXES(SlimLayerTest, UIResourceLayerProperties);

  UIResourceLayer();
  ~UIResourceLayer() override;

  cc::UIResourceId resource_id() const { return resource_id_; }
  auto uv_top_left() const { return uv_top_left_; }
  auto uv_bottom_right() const { return uv_bottom_right_; }

  bool HasDrawableContent() const override;
  void AppendQuads(viz::CompositorRenderPass& render_pass,
                   FrameData& data,
                   const gfx::Transform& transform_to_root,
                   const gfx::Transform& transform_to_target,
                   const gfx::Rect* clip_in_target,
                   const gfx::Rect& visible_rect,
                   float opacity) override;

 private:
  void RefreshResource();
  void SetUIResourceIdInternal(cc::UIResourceId resource_id);

  cc::UIResourceId resource_id_ = 0;
  SkBitmap bitmap_;
  gfx::PointF uv_top_left_;
  gfx::PointF uv_bottom_right_{1.0f, 1.0f};
};

}  // namespace cc::slim

#endif  // CC_SLIM_UI_RESOURCE_LAYER_H_
