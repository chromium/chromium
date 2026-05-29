// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/sticky_position_constraint.h"

#include <algorithm>

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

gfx::Vector2dF StickyPositionConstraint::StickyPositionOffset(
    gfx::PointF scroll_position,
    gfx::Vector2dF constraint_box_expansion,
    gfx::Vector2dF ancestor_sticky_box_offset,
    gfx::Vector2dF ancestor_containing_block_offset) const {
  gfx::RectF clip = constraint_box_rect;
  clip.Offset(scroll_position.x(), scroll_position.y());
  clip.set_width(clip.width() + constraint_box_expansion.x());
  clip.set_height(clip.height() + constraint_box_expansion.y());

  // Compute the current position of the constraint rects based on the original
  // positions and the offsets from ancestor sticky elements.
  gfx::RectF sticky_box_rect = scroll_container_relative_sticky_box_rect +
                               ancestor_sticky_box_offset +
                               ancestor_containing_block_offset;
  gfx::RectF containing_block_rect =
      scroll_container_relative_containing_block_rect +
      ancestor_containing_block_offset;

  // In each of the following cases, we measure the limit which is the point
  // that the element should stick to, clamping on one side to 0 (because sticky
  // only pushes elements in one direction). Then we clamp to how far we can
  // push the element in that direction without being pushed outside of its
  // containing block.
  //
  // Note: The order of applying the sticky constraints is applied such that
  // left offset takes precedence over right offset, and top takes precedence
  // over bottom offset.
  gfx::Vector2dF sticky_offset;
  if (is_anchored_right) {
    float right_limit = clip.right() - right_offset;
    float right_delta =
        std::min<float>(0, right_limit - sticky_box_rect.right());
    float available_space =
        std::min<float>(0, containing_block_rect.x() - sticky_box_rect.x());
    if (right_delta < available_space) {
      right_delta = available_space;
    }
    sticky_offset.set_x(sticky_offset.x() + right_delta);
  }
  if (is_anchored_left) {
    float left_limit = clip.x() + left_offset;
    float left_delta = std::max<float>(0, left_limit - sticky_box_rect.x());
    float available_space = std::max<float>(
        0, containing_block_rect.right() - sticky_box_rect.right());
    if (left_delta > available_space) {
      left_delta = available_space;
    }
    sticky_offset.set_x(sticky_offset.x() + left_delta);
  }
  if (is_anchored_bottom) {
    float bottom_limit = clip.bottom() - bottom_offset;
    float bottom_delta =
        std::min<float>(0, bottom_limit - sticky_box_rect.bottom());
    float available_space =
        std::min<float>(0, containing_block_rect.y() - sticky_box_rect.y());
    if (bottom_delta < available_space) {
      bottom_delta = available_space;
    }
    sticky_offset.set_y(sticky_offset.y() + bottom_delta);
  }
  if (is_anchored_top) {
    float top_limit = clip.y() + top_offset;
    float top_delta = std::max<float>(0, top_limit - sticky_box_rect.y());
    float available_space = std::max<float>(
        0, containing_block_rect.bottom() - sticky_box_rect.bottom());
    if (top_delta > available_space) {
      top_delta = available_space;
    }
    sticky_offset.set_y(sticky_offset.y() + top_delta);
  }

  return sticky_offset;
}

bool StickyPositionConstraint::operator==(
    const StickyPositionConstraint& other) const = default;

}  // namespace cc
