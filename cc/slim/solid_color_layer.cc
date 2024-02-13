// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/slim/solid_color_layer.h"

#include <utility>

#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"

namespace cc::slim {

// static
scoped_refptr<SolidColorLayer> SolidColorLayer::Create() {
  return base::AdoptRef(new SolidColorLayer());
}

SolidColorLayer::SolidColorLayer() = default;
SolidColorLayer::~SolidColorLayer() = default;

void SolidColorLayer::SetBackgroundColor(SkColor4f color) {
  SetContentsOpaque(color.isOpaque());
  Layer::SetBackgroundColor(color);
}

void SolidColorLayer::AppendQuads(viz::CompositorRenderPass& render_pass,
                                  FrameData& data,
                                  const gfx::Transform& transform_to_root,
                                  const gfx::Transform& transform_to_target,
                                  const gfx::Rect* clip_in_target,
                                  const gfx::Rect& visible_rect,
                                  float opacity) {
  viz::SharedQuadState* quad_state =
      CreateAndAppendSharedQuadState(render_pass, data, transform_to_target,
                                     clip_in_target, visible_rect, opacity);
  viz::SolidColorDrawQuad* quad =
      render_pass.CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
  quad->SetNew(quad_state, quad_state->quad_layer_rect,
               quad_state->visible_quad_layer_rect, background_color(),
               /*anti_aliasing_off=*/false);
}

}  // namespace cc::slim
