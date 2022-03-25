// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/clip_expander.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/property_tree.h"
#include "ui/gfx/geometry/transform.h"

namespace cc {

ClipExpander::ClipExpander(int filter_effect_id)
    : target_effect_id_(filter_effect_id) {}

ClipExpander::ClipExpander(const ClipExpander& other) = default;

ClipExpander& ClipExpander::operator=(const ClipExpander& other) = default;

bool ClipExpander::operator==(const ClipExpander& other) const {
  return target_effect_id_ == other.target_effect_id_;
}

gfx::Rect ClipExpander::MapRect(const gfx::Rect& rect,
                                const PropertyTrees* property_trees) const {
  const EffectNode* effect_node =
      property_trees->effect_tree().Node(target_effect_id_);
  gfx::Transform filter_draw_transform;
  filter_draw_transform.Scale(effect_node->surface_contents_scale.x(),
                              effect_node->surface_contents_scale.y());
  return effect_node->filters.MapRect(rect,
                                      filter_draw_transform.matrix().asM33());
}

gfx::Rect ClipExpander::MapRectReverse(
    const gfx::Rect& rect,
    const PropertyTrees* property_trees) const {
  const EffectNode* effect_node =
      property_trees->effect_tree().Node(target_effect_id_);
  gfx::Transform filter_draw_transform;
  filter_draw_transform.Scale(effect_node->surface_contents_scale.x(),
                              effect_node->surface_contents_scale.y());
  return effect_node->filters.MapRectReverse(
      rect, filter_draw_transform.matrix().asM33());
}

}  // namespace cc
