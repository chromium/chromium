// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_DRAW_PROPERTIES_H_
#define CC_LAYERS_DRAW_PROPERTIES_H_

#include <stddef.h>

#include <memory>

#include "cc/trees/occlusion.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/rrect_f.h"
#include "ui/gfx/transform.h"

namespace cc {

// Container for properties that layers need to compute before they can be
// drawn.
struct CC_EXPORT DrawProperties {
  DrawProperties();
  ~DrawProperties();

  // Transforms objects from content space to target surface space, where
  // this layer would be drawn.
  gfx::Transform target_space_transform;

  // Transforms objects from content space to screen space (viewport space).
  gfx::Transform screen_space_transform;

  // Known occlusion above the layer mapped to the content space of the layer.
  Occlusion occlusion_in_content_space;

  // DrawProperties::opacity may be different than LayerImpl::opacity,
  // particularly in the case when a RenderSurface re-parents the layer's
  // opacity, or when opacity is compounded by the hierarchy.
  float opacity = 0.f;

  // Whether the layer has a potentially animating transform in its chain of
  // transforms to the screen. This is essentially a cache of the transform
  // node's potentially-animated status.
  bool screen_space_transform_is_animating = false;

  // True if the layer needs to be clipped by clip_rect.
  bool is_clipped = false;

  // If set, it makes the layer's rounded corner not trigger a render surface if
  // possible.
  bool is_fast_rounded_corner = false;

  // This rect is a bounding box around what part of the layer is visible, in
  // the layer's coordinate space.
  gfx::Rect visible_layer_rect;

  // In target surface space, the rect that encloses the clipped, drawable
  // content of the layer.
  gfx::Rect drawable_content_rect;

  // In target surface space, the original rect that clipped this layer. This
  // value is used to avoid unnecessarily changing GL scissor state.
  gfx::Rect clip_rect;

  // Contains a rounded corner rect to clip this layer when drawing. This rrect
  // is in the target space of the layer.
  gfx::RRectF rounded_corner_bounds;
};

}  // namespace cc

#endif  // CC_LAYERS_DRAW_PROPERTIES_H_
