// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/scroll_node.h"

#include "cc/base/math_util.h"
#include "cc/layers/layer.h"
#include "cc/paint/element_id.h"
#include "cc/trees/property_tree.h"

#include "base/trace_event/traced_value.h"

namespace cc {

ScrollNode::ScrollNode() = default;
ScrollNode::ScrollNode(const ScrollNode& other) = default;
ScrollNode::~ScrollNode() = default;

#if DCHECK_IS_ON()
bool ScrollNode::operator==(const ScrollNode& other) const = default;
#endif

void ScrollNode::AsValueInto(base::trace_event::TracedValue* value) const {
  value->SetInteger("id", id);
  value->SetInteger("parent_id", parent_id);
  MathUtil::AddToTracedValue("container_bounds", container_bounds, value);
  MathUtil::AddToTracedValue("bounds", bounds, value);
  value->SetBoolean("user_scrollable_horizontal", user_scrollable_horizontal);
  value->SetBoolean("user_scrollable_vertical", user_scrollable_vertical);

  value->SetBoolean("scrolls_inner_viewport", scrolls_inner_viewport);
  value->SetBoolean("scrolls_outer_viewport", scrolls_outer_viewport);
  value->SetBoolean("prevent_viewport_scrolling_from_inner",
                    prevent_viewport_scrolling_from_inner);

  element_id.AddToTracedValue(value);
  value->SetInteger("transform_id", transform_id);
  value->SetInteger("overscroll_behavior_x",
                    static_cast<int>(overscroll_behavior.x));
  value->SetInteger("overscroll_behavior_y",
                    static_cast<int>(overscroll_behavior.y));

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

  value->SetBoolean("is_composited", is_composited);
}

}  // namespace cc
