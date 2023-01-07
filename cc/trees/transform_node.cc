// Copyright 2016 The Chromium Authors
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
    : id(kInvalidPropertyNodeId),
      parent_id(kInvalidPropertyNodeId),
      parent_frame_id(kInvalidPropertyNodeId),
      sticky_position_constraint_id(-1),
      anchor_scroll_containers_data_id(-1),
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
      scrolls(false),
      should_undo_overscroll(false),
      should_be_snapped(false),
      moved_by_outer_viewport_bounds_delta_y(false),
      in_subtree_of_page_scale_layer(false),
      transform_changed(false),
      delegates_to_parent_for_backface(false),
      will_change_transform(false),
      node_or_ancestors_will_change_transform(false),
      maximum_animation_scale(kInvalidScale) {}

TransformNode::TransformNode(const TransformNode&) = default;

TransformNode& TransformNode::operator=(const TransformNode&) = default;

#if DCHECK_IS_ON()
bool TransformNode::operator==(const TransformNode& other) const {
  return id == other.id && parent_id == other.parent_id &&
         parent_frame_id == other.parent_frame_id &&
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
         scrolls == other.scrolls &&
         should_undo_overscroll == other.should_undo_overscroll &&
         should_be_snapped == other.should_be_snapped &&
         moved_by_outer_viewport_bounds_delta_y ==
             other.moved_by_outer_viewport_bounds_delta_y &&
         in_subtree_of_page_scale_layer ==
             other.in_subtree_of_page_scale_layer &&
         delegates_to_parent_for_backface ==
             other.delegates_to_parent_for_backface &&
         will_change_transform == other.will_change_transform &&
         node_or_ancestors_will_change_transform ==
             other.node_or_ancestors_will_change_transform &&
         transform_changed == other.transform_changed &&
         scroll_offset == other.scroll_offset &&
         snap_amount == other.snap_amount &&
         maximum_animation_scale == other.maximum_animation_scale &&
         visible_frame_element_id == other.visible_frame_element_id;
}
#endif  // DCHECK_IS_ON()

void TransformNode::AsValueInto(base::trace_event::TracedValue* value) const {
  value->SetInteger("id", id);
  value->SetInteger("parent_id", parent_id);
  element_id.AddToTracedValue(value);
  MathUtil::AddToTracedValue("local", local, value);
  MathUtil::AddToTracedValue("origin", origin, value);
  MathUtil::AddToTracedValue("post_translation", post_translation, value);
  value->SetInteger("sorting_context_id", sorting_context_id);
  value->SetBoolean("flattens_inherited_transform",
                    flattens_inherited_transform);
  value->SetBoolean("will_change_transform", will_change_transform);
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
