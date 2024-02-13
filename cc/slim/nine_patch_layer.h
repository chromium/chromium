// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SLIM_NINE_PATCH_LAYER_H_
#define CC_SLIM_NINE_PATCH_LAYER_H_

#include "base/component_export.h"
#include "cc/layers/nine_patch_generator.h"
#include "cc/slim/ui_resource_layer.h"
#include "ui/gfx/geometry/rect.h"

namespace cc::slim {

class COMPONENT_EXPORT(CC_SLIM) NinePatchLayer : public UIResourceLayer {
 public:
  static scoped_refptr<NinePatchLayer> Create();

  // |border| is the space around the center rectangular region in layer space
  // (known as aperture in image space).  |border.x()| and |border.y()| are the
  // size of the left and top boundary, respectively.
  // |border.width()-border.x()| and |border.height()-border.y()| are the size
  // of the right and bottom boundary, respectively.
  // TODO(boliu): Should use gfx::Inset instead of gfx::Rect once this can
  // diverge from cc.
  void SetBorder(const gfx::Rect& border);

  // aperture is in the pixel space of the bitmap resource and refers to
  // the center patch of the ninepatch.  We split off eight rects surrounding
  // it and stick them on the edges of the layer. The corners are unscaled, the
  // top and bottom rects are x-stretched to fit, and the left and right rects
  // are y-stretched to fit.
  void SetAperture(const gfx::Rect& aperture);

  // Set whether to draw the center patch or not.
  void SetFillCenter(bool fill_center);

  // Use nearest neighbor sampling instead of linear (weighted average of 4
  // closest pixels) when sampling from the bitmap.
  void SetNearestNeighbor(bool nearest_neighbor);

  void AppendQuads(viz::CompositorRenderPass& render_pass,
                   FrameData& data,
                   const gfx::Transform& transform_to_root,
                   const gfx::Transform& transform_to_target,
                   const gfx::Rect* clip_in_target,
                   const gfx::Rect& visible_rect,
                   float opacity) override;

 private:
  NinePatchLayer();
  ~NinePatchLayer() override;

  gfx::Rect border_;
  gfx::Rect aperture_;
  bool fill_center_ = false;
  bool nearest_neighbor_ = false;
  cc::NinePatchGenerator quad_generator_;
};

}  // namespace cc::slim

#endif  // CC_SLIM_NINE_PATCH_LAYER_H_
