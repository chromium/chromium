// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/transform_node.h"
#include "base/trace_event/traced_value.h"
#include "cc/base/math_util.h"
#include "cc/layers/layer.h"
#include "cc/trees/mutator_host.h"
#include "cc/trees/property_tree.h"
#include "ui/gfx/geometry/point3_f.h"

namespace cc {

TransformNode::TransformNode()
    : id(TransformTree::kInvalidNodeId),
      parent_id(TransformTree::kInvalidNodeId),
      sticky_position_constraint_id(-1),
      sorting_context_id(0),
      needs_local_transform_update(true),
      node_and_ancestors_are_animated_or_invertible(true),
      is_invertible(true),
      ancestors_are_invertible(true),
      has_potential_animation(false),
      is_currently_animating(false),
      to_screen_is_potentially_animated(false),
      flattens_inherited_transform(true),
      node_and_ancestors_are_flat(true),
      node_and_ancestors_have_only_integer_translation(true),
      scrolls(false),
      should_be_snapped(false),
      moved_by_outer_viewport_bounds_delta_y(false),
      in_subtree_of_page_scale_layer(false),
      transform_changed(false),
      maximum_animation_scale(kNotScaled),
      starting_animation_scale(kNotScaled) {}

TransformNode::TransformNode(const TransformNode&) = default;

bool TransformNode::operator==(const TransformNode& other) const {
  return id == other.id && parent_id == other.parent_id &&
         element_id == other.element_id && local == other.local &&
         origin == other.origin && post_translation == other.post_translation &&
         to_parent == other.to_parent &&
         sorting_context_id == other.sorting_context_id &&
         needs_local_transform_update == other.needs_local_transform_update &&
         node_and_ancestors_are_animated_or_invertible ==
             other.node_and_ancestors_are_animated_or_invertible &&
         is_invertible == other.is_invertible &&
         ancestors_are_invertible == other.ancestors_are_invertible &&
         has_potential_animation == other.has_potential_animation &&
         is_currently_animating == other.is_currently_animating &&
         to_screen_is_potentially_animated ==
             other.to_screen_is_potentially_animated &&
         flattens_inherited_transform == other.flattens_inherited_transform &&
         node_and_ancestors_are_flat == other.node_and_ancestors_are_flat &&
         node_and_ancestors_have_only_integer_translation ==
             other.node_and_ancestors_have_only_integer_translation &&
         scrolls == other.scrolls &&
         should_be_snapped == other.should_be_snapped &&
         moved_by_outer_viewport_bounds_delta_y ==
             other.moved_by_outer_viewport_bounds_delta_y &&
         in_subtree_of_page_scale_layer ==
             other.in_subtree_of_page_scale_layer &&
         transform_changed == other.transform_changed &&
         scroll_offset == other.scroll_offset &&
         snap_amount == other.snap_amount &&
         maximum_animation_scale == other.maximum_animation_scale &&
         starting_animation_scale == other.starting_animation_scale;
}

void TransformNode::AsValueInto(base::trace_event::TracedValue* value) const {
  value->SetInteger("id", id);
  value->SetInteger("parent_id", parent_id);
  element_id.AddToTracedValue(value);
  MathUtil::AddToTracedValue("local", local, value);
  MathUtil::AddToTracedValue("origin", origin, value);
  MathUtil::AddToTracedValue("post_translation", post_translation, value);
  value->SetInteger("sorting_context_id", sorting_context_id);
  value->SetInteger("flattens_inherited_transform",
                    flattens_inherited_transform);
  MathUtil::AddToTracedValue("scroll_offset", scroll_offset, value);
  MathUtil::AddToTracedValue("snap_amount", snap_amount, value);
}

TransformCachedNodeData::TransformCachedNodeData()
    : is_showing_backface(false) {}

TransformCachedNodeData::TransformCachedNodeData(
    const TransformCachedNodeData& other) = default;

TransformCachedNodeData::~TransformCachedNodeData() = default;

bool TransformCachedNodeData::operator==(
    const TransformCachedNodeData& other) const {
  return from_screen == other.from_screen && to_screen == other.to_screen &&
         is_showing_backface == other.is_showing_backface;
}

}  // namespace cc
