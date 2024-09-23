// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SLIM_SOLID_COLOR_LAYER_H_
#define CC_SLIM_SOLID_COLOR_LAYER_H_

#include "base/component_export.h"
#include "cc/slim/layer.h"

namespace cc::slim {

// A Layer that renders a solid color. The color is specified by using
// SetBackgroundColor() on the base class.
class COMPONENT_EXPORT(CC_SLIM) SolidColorLayer : public Layer {
 public:
  static scoped_refptr<SolidColorLayer> Create();

  void SetBackgroundColor(SkColor4f color) override;

 private:
  SolidColorLayer();
  ~SolidColorLayer() override;

  void AppendQuads(viz::CompositorRenderPass& render_pass,
                   FrameData& data,
                   const gfx::Transform& transform_to_root,
                   const gfx::Transform& transform_to_target,
                   const gfx::Rect* clip_in_target,
                   const gfx::Rect& visible_rect,
                   float opacity) override;
};

}  // namespace cc::slim

#endif  // CC_SLIM_SOLID_COLOR_LAYER_H_
