// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_STICKY_POSITION_CONSTRAINT_H_
#define CC_TREES_STICKY_POSITION_CONSTRAINT_H_

#include "cc/cc_export.h"

#include "cc/paint/element_id.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"

namespace cc {

struct CC_EXPORT StickyPositionConstraint {
  StickyPositionConstraint();
  StickyPositionConstraint(const StickyPositionConstraint& other);
  StickyPositionConstraint& operator=(const StickyPositionConstraint& other);

  bool is_anchored_left : 1 = false;
  bool is_anchored_right : 1 = false;
  bool is_anchored_top : 1 = false;
  bool is_anchored_bottom : 1 = false;

  // The offset from each edge of the ancestor scroller (or the viewport) to
  // try to maintain to the sticky box as we scroll.
  float left_offset = 0;
  float right_offset = 0;
  float top_offset = 0;
  float bottom_offset = 0;

  // The rectangle in which the sticky box is able to be positioned. This may be
  // smaller than the scroller viewport due to things like padding.
  gfx::RectF constraint_box_rect;

  // The rectangle corresponding to original layout position of the sticky box
  // relative to the scroll ancestor. The sticky box is only offset once the
  // scroll has passed its initial position (e.g. top_offset will only push
  // the element down from its original position).
  gfx::RectF scroll_container_relative_sticky_box_rect;

  // The layout rectangle of the sticky box's containing block relative to the
  // scroll ancestor. The sticky box is only moved as far as its containing
  // block boundary.
  gfx::RectF scroll_container_relative_containing_block_rect;

  // The nearest ancestor sticky element ids that affect the sticky box
  // constraint rect and the containing block constraint rect respectively.
  // They are used to generate nearest_node_shifting_sticky_box and
  // nearest_node_shifting_containing_block in StickyPositionNodeData when the
  // property trees are generated. They are useless after the property trees
  // are generated.
  ElementId nearest_element_shifting_sticky_box;
  ElementId nearest_element_shifting_containing_block;

  bool operator==(const StickyPositionConstraint&) const;
  bool operator!=(const StickyPositionConstraint&) const;
};

}  // namespace cc

#endif  // CC_TREES_STICKY_POSITION_CONSTRAINT_H_
