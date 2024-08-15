// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_SCROLL_NODE_H_
#define CC_TREES_SCROLL_NODE_H_

#include <optional>

#include "cc/base/region.h"
#include "cc/cc_export.h"
#include "cc/input/main_thread_scrolling_reason.h"
#include "cc/input/overscroll_behavior.h"
#include "cc/input/scroll_snap_data.h"
#include "cc/paint/element_id.h"
#include "cc/paint/filter_operations.h"
#include "cc/trees/property_ids.h"
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
  int id = kInvalidPropertyNodeId;
  // The node index of the parent node in the scroll tree node vector.
  int parent_id = kInvalidPropertyNodeId;

  // This will be renamed to main_thread_repaint_reasons when we clean up
  // blink killswitch ExcludePopupMainThreadScrollingReason.
  uint32_t main_thread_scrolling_reasons =
      MainThreadScrollingReason::kNotScrollingOnMain;

  // Size of the container area that the contents scrolls in, not including
  // non-overlay scrollbars. Overlay scrollbars do not affect these bounds.
  // Note, use the ScrollTree::container_bounds function for the viewport
  // scroll nodes to include the current bounds change due to top controls
  // hiding / showing.
  gfx::Size container_bounds;

  // Size of the content that is scrolled within the container bounds.
  gfx::Size bounds;

  bool max_scroll_offset_affected_by_page_scale : 1 = false;
  bool scrolls_inner_viewport : 1 = false;
  bool scrolls_outer_viewport : 1 = false;
  bool prevent_viewport_scrolling_from_inner : 1 = false;
  bool user_scrollable_horizontal : 1 = false;
  bool user_scrollable_vertical : 1 = false;
  bool is_composited : 1 = false;

  ElementId element_id;
  int transform_id = kRootPropertyNodeId;

  // The container area origin in the parent transform space of transform_id.
  // Used for scroll debug rect visualization only.
  gfx::Point container_origin;

  OverscrollBehavior overscroll_behavior{OverscrollBehavior::Type::kAuto};

  std::optional<SnapContainerData> snap_container_data;

#if DCHECK_IS_ON()
  bool operator==(const ScrollNode& other) const;
#endif

  void AsValueInto(base::trace_event::TracedValue* value) const;
};

}  // namespace cc

#endif  // CC_TREES_SCROLL_NODE_H_
