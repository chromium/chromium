// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/scroll_node.h"

#include "cc/base/math_util.h"
#include "cc/input/main_thread_scrolling_reason.h"
#include "cc/layers/layer.h"
#include "cc/paint/element_id.h"
#include "cc/trees/property_tree.h"

#include "base/trace_event/traced_value.h"

namespace cc {

ScrollNode::ScrollNode()
    : id(ScrollTree::kInvalidNodeId),
      parent_id(ScrollTree::kInvalidNodeId),
      main_thread_scrolling_reasons(
          MainThreadScrollingReason::kNotScrollingOnMain),
      scrollable(false),
      max_scroll_offset_affected_by_page_scale(false),
      scrolls_inner_viewport(false),
      scrolls_outer_viewport(false),
      prevent_viewport_scrolling_from_inner(false),
      should_flatten(false),
      user_scrollable_horizontal(false),
      user_scrollable_vertical(false),
      transform_id(0),
      overscroll_behavior(OverscrollBehavior::kOverscrollBehaviorTypeAuto) {}

ScrollNode::ScrollNode(const ScrollNode& other) = default;

ScrollNode::~ScrollNode() = default;

bool ScrollNode::operator==(const ScrollNode& other) const {
  return id == other.id && parent_id == other.parent_id &&
         scrollable == other.scrollable &&
         main_thread_scrolling_reasons == other.main_thread_scrolling_reasons &&
         container_bounds == other.container_bounds && bounds == other.bounds &&
         max_scroll_offset_affected_by_page_scale ==
             other.max_scroll_offset_affected_by_page_scale &&
         scrolls_inner_viewport == other.scrolls_inner_viewport &&
         prevent_viewport_scrolling_from_inner ==
             other.prevent_viewport_scrolling_from_inner &&
         scrolls_outer_viewport == other.scrolls_outer_viewport &&
         offset_to_transform_parent == other.offset_to_transform_parent &&
         should_flatten == other.should_flatten &&
         user_scrollable_horizontal == other.user_scrollable_horizontal &&
         user_scrollable_vertical == other.user_scrollable_vertical &&
         element_id == other.element_id && transform_id == other.transform_id &&
         overscroll_behavior == other.overscroll_behavior &&
         snap_container_data == other.snap_container_data;
}

void ScrollNode::AsValueInto(base::trace_event::TracedValue* value) const {
  value->SetInteger("id", id);
  value->SetInteger("parent_id", parent_id);
  value->SetBoolean("scrollable", scrollable);
  MathUtil::AddToTracedValue("container_bounds", container_bounds, value);
  MathUtil::AddToTracedValue("bounds", bounds, value);
  MathUtil::AddToTracedValue("offset_to_transform_parent",
                             offset_to_transform_parent, value);
  value->SetBoolean("should_flatten", should_flatten);
  value->SetBoolean("user_scrollable_horizontal", user_scrollable_horizontal);
  value->SetBoolean("user_scrollable_vertical", user_scrollable_vertical);

  value->SetBoolean("scrolls_inner_viewport", scrolls_inner_viewport);
  value->SetBoolean("scrolls_outer_viewport", scrolls_outer_viewport);
  value->SetBoolean("prevent_viewport_scrolling_from_inner",
                    prevent_viewport_scrolling_from_inner);

  element_id.AddToTracedValue(value);
  value->SetInteger("transform_id", transform_id);
  value->SetInteger("overscroll_behavior_x", overscroll_behavior.x);
  value->SetInteger("overscroll_behavior_y", overscroll_behavior.y);

  if (snap_container_data) {
    value->SetString("snap_container_rect",
                     snap_container_data->rect().ToString());
    if (snap_container_data->size()) {
      value->BeginArray("snap_area_rects");
      for (size_t i = 0; i < snap_container_data->size(); ++i) {
        value->AppendString(snap_container_data->at(i).rect.ToString());
      }
      value->EndArray();
    }
  }
}

}  // namespace cc
