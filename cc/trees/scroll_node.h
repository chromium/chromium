// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_SCROLL_NODE_H_
#define CC_TREES_SCROLL_NODE_H_

#include "cc/base/region.h"
#include "cc/cc_export.h"
#include "cc/input/overscroll_behavior.h"
#include "cc/input/scroll_snap_data.h"
#include "cc/paint/element_id.h"
#include "cc/paint/filter_operations.h"
#include "ui/gfx/geometry/size.h"

namespace base {
namespace trace_event {
class TracedValue;
}  // namespace trace_event
}  // namespace base

namespace cc {

struct CC_EXPORT ScrollNode {
  ScrollNode();
  ScrollNode(const ScrollNode& other);
  ~ScrollNode();

  // The node index of this node in the scroll tree node vector.
  int id;
  // The node index of the parent node in the scroll tree node vector.
  int parent_id;

  uint32_t main_thread_scrolling_reasons;

  // Size of the container area that the contents scrolls in, not including
  // non-overlay scrollbars. Overlay scrollbars do not affect these bounds.
  gfx::Size container_bounds;

  // Size of the content that is scrolled within the container bounds.
  gfx::Size bounds;

  // This is used for subtrees that should not be scrolled independently. For
  // example, when there is a layer that is not scrollable itself but is inside
  // a scrolling layer.
  bool scrollable : 1;
  bool max_scroll_offset_affected_by_page_scale : 1;
  bool scrolls_inner_viewport : 1;
  bool scrolls_outer_viewport : 1;
  bool prevent_viewport_scrolling_from_inner : 1;
  bool should_flatten : 1;
  bool user_scrollable_horizontal : 1;
  bool user_scrollable_vertical : 1;

  // This offset is used when |scrollable| is false and there isn't a transform
  // node already present that covers this offset. For layer tree mode only.
  gfx::Vector2dF offset_to_transform_parent;

  ElementId element_id;
  int transform_id;

  OverscrollBehavior overscroll_behavior;

  base::Optional<SnapContainerData> snap_container_data;

  bool operator==(const ScrollNode& other) const;
  void AsValueInto(base::trace_event::TracedValue* value) const;
};

}  // namespace cc

#endif  // CC_TREES_SCROLL_NODE_H_
