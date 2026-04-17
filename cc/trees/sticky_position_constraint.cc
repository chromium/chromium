// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/sticky_position_constraint.h"

namespace cc {

StickyPositionConstraint::StickyPositionConstraint() = default;
StickyPositionConstraint::StickyPositionConstraint(
    const StickyPositionConstraint& other) = default;

StickyPositionConstraint& StickyPositionConstraint::operator=(
    const StickyPositionConstraint& other) = default;

bool StickyPositionConstraint::CanMerge(
    const StickyPositionConstraint& other) const {
  if (is_anchored_left != other.is_anchored_left ||
      is_anchored_right != other.is_anchored_right ||
      is_anchored_top != other.is_anchored_top ||
      is_anchored_bottom != other.is_anchored_bottom ||
      constraint_box_rect != other.constraint_box_rect ||
      scroll_container_relative_containing_block_rect !=
          other.scroll_container_relative_containing_block_rect ||
      x_scroll_ancestor_element_id != other.x_scroll_ancestor_element_id ||
      y_scroll_ancestor_element_id != other.y_scroll_ancestor_element_id ||
      nearest_element_shifting_sticky_box !=
          other.nearest_element_shifting_sticky_box ||
      nearest_element_shifting_containing_block !=
          other.nearest_element_shifting_containing_block) {
    return false;
  }

  // The following conditions are slightly stricter than needed to simplify
  // code. See TransformTree::StickyPositionOffset() for how these fields
  // affect sticky position offset in different anchoring modes.
  if (is_anchored_left || is_anchored_right) {
    if (left_offset != other.left_offset ||
        right_offset != other.right_offset ||
        scroll_container_relative_sticky_box_rect.x() !=
            other.scroll_container_relative_sticky_box_rect.x() ||
        scroll_container_relative_sticky_box_rect.width() !=
            other.scroll_container_relative_sticky_box_rect.width() ||
        pixel_snap_offset.x() != other.pixel_snap_offset.x()) {
      return false;
    }
  }
  if (is_anchored_top || is_anchored_bottom) {
    if (top_offset != other.top_offset ||
        bottom_offset != other.bottom_offset ||
        scroll_container_relative_sticky_box_rect.y() !=
            other.scroll_container_relative_sticky_box_rect.y() ||
        scroll_container_relative_sticky_box_rect.height() !=
            other.scroll_container_relative_sticky_box_rect.height() ||
        pixel_snap_offset.y() != other.pixel_snap_offset.y()) {
      return false;
    }
  }

  return true;
}

bool StickyPositionConstraint::operator==(
    const StickyPositionConstraint& other) const = default;

}  // namespace cc
