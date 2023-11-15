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

bool StickyPositionConstraint::operator==(
    const StickyPositionConstraint& other) const {
  return is_anchored_left == other.is_anchored_left &&
         is_anchored_right == other.is_anchored_right &&
         is_anchored_top == other.is_anchored_top &&
         is_anchored_bottom == other.is_anchored_bottom &&
         left_offset == other.left_offset &&
         right_offset == other.right_offset && top_offset == other.top_offset &&
         bottom_offset == other.bottom_offset &&
         constraint_box_rect == other.constraint_box_rect &&
         scroll_container_relative_sticky_box_rect ==
             other.scroll_container_relative_sticky_box_rect &&
         scroll_container_relative_containing_block_rect ==
             other.scroll_container_relative_containing_block_rect &&
         nearest_element_shifting_sticky_box ==
             other.nearest_element_shifting_sticky_box &&
         nearest_element_shifting_containing_block ==
             other.nearest_element_shifting_containing_block;
}

bool StickyPositionConstraint::operator!=(
    const StickyPositionConstraint& other) const {
  return !(*this == other);
}

}  // namespace cc
