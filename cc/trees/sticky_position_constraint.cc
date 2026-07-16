// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/sticky_position_constraint.h"

#include <algorithm>
#include <limits>
#include <optional>

#include "ui/gfx/range/range_f.h"

namespace cc {
namespace {

struct StickyTransitionPoints {
  // The range of scroll position in which the box is sticky to the scroll
  // port, for the constraints in each direction, assuming the constraints
  // are applied independently in each direction.
  // If the box is never sticky to the scroll port in a direction, the range is
  // empty with the default value InvalidRange(), so that == can treat all empty
  // ranges as equal.
  gfx::RangeF left = gfx::RangeF::InvalidRange();
  gfx::RangeF top = gfx::RangeF::InvalidRange();
  gfx::RangeF right = gfx::RangeF::InvalidRange();
  gfx::RangeF bottom = gfx::RangeF::InvalidRange();

  bool operator==(const StickyTransitionPoints&) const = default;

  void ClipByScrollRange(const gfx::RectF& range) {
    gfx::RangeF range_x(range.x(), range.right());
    left = left.Intersect(range_x);
    right = right.Intersect(range_x);
    gfx::RangeF range_y(range.y(), range.bottom());
    top = top.Intersect(range_y);
    bottom = bottom.Intersect(range_y);
  }
};

StickyTransitionPoints GetStickyTransitionPoints(
    const StickyPositionConstraint& constraint) {
  // Shifting ancestors are not supported for the purpose of CanMerge().
  DCHECK(!constraint.nearest_element_shifting_sticky_box);
  DCHECK(!constraint.nearest_element_shifting_containing_block);

  const auto& sticky_box_rect =
      constraint.scroll_container_relative_sticky_box_rect;
  const auto& containing_block_rect =
      constraint.scroll_container_relative_containing_block_rect;

  StickyTransitionPoints points;
  if (constraint.is_anchored_left) {
    float available_space =
        containing_block_rect.right() - sticky_box_rect.right();
    if (available_space > 0) {
      points.left.set_start(sticky_box_rect.x() -
                            constraint.constraint_box_rect.x() -
                            constraint.left_offset);
      points.left.set_end(points.left.start() + available_space);
    }
    // Otherwise the box is never sticky to the scroll port in this direction,
    // so leave the range empty with the default value InvalidRange().
  }
  if (constraint.is_anchored_right) {
    float available_space = containing_block_rect.x() - sticky_box_rect.x();
    if (available_space < 0) {
      points.right.set_end(sticky_box_rect.right() + constraint.right_offset -
                           constraint.constraint_box_rect.right());
      points.right.set_start(points.right.end() + available_space);
    }
  }
  if (constraint.is_anchored_top) {
    float available_space =
        containing_block_rect.bottom() - sticky_box_rect.bottom();
    if (available_space > 0) {
      points.top.set_start(sticky_box_rect.y() -
                           constraint.constraint_box_rect.y() -
                           constraint.top_offset);
      points.top.set_end(points.top.start() + available_space);
    }
  }
  if (constraint.is_anchored_bottom) {
    float available_space = containing_block_rect.y() - sticky_box_rect.y();
    if (available_space < 0) {
      points.bottom.set_end(sticky_box_rect.bottom() +
                            constraint.bottom_offset -
                            constraint.constraint_box_rect.bottom());
      points.bottom.set_start(points.bottom.end() + available_space);
    }
  }

  return points;
}

}  // namespace

StickyPositionConstraint::StickyPositionConstraint() = default;
StickyPositionConstraint::StickyPositionConstraint(
    const StickyPositionConstraint& other) = default;

StickyPositionConstraint& StickyPositionConstraint::operator=(
    const StickyPositionConstraint& other) = default;

StickyPositionConstraint::CanMergeResult StickyPositionConstraint::CanMerge(
    const StickyPositionConstraint& other,
    const std::optional<gfx::RectF>& scroll_range) const {
  if (*this == other) {
    return CanMergeResult::kCanAlwaysMerge;
  }

  if (x_scroll_ancestor_element_id != other.x_scroll_ancestor_element_id ||
      y_scroll_ancestor_element_id != other.y_scroll_ancestor_element_id) {
    return CanMergeResult::kCannotMerge;
  }

  // GetTransitionPoints() doesn't support shifting ancestors.
  if (nearest_element_shifting_sticky_box ||
      other.nearest_element_shifting_sticky_box ||
      nearest_element_shifting_containing_block ||
      other.nearest_element_shifting_containing_block) {
    return CanMergeResult::kCannotMerge;
  }

  // If the transition points match exactly across the full domain, the
  // constraints always produce the same StickyPositionOffset values and
  // can always merge.
  auto points1 = GetStickyTransitionPoints(*this);
  auto points2 = GetStickyTransitionPoints(other);
  if (points1 == points2) {
    return CanMergeResult::kCanAlwaysMerge;
  }

  // If full transition points differ but a scroll range is provided, compare
  // transition points clipped by the scroll range. If those match then
  // the constraints produce StickyPositionOffset values with a constant
  // difference within the scroll range and can be merged.
  if (scroll_range) {
    points1.ClipByScrollRange(*scroll_range);
    points2.ClipByScrollRange(*scroll_range);
    if (points1 == points2) {
      return CanMergeResult::kCanMergeWithinScrollRange;
    }
  }

  return CanMergeResult::kCannotMerge;
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
  // Note on precedence and constraints:
  // While the spec states that a start offset takes precedence over an end
  // offset, this code does not actively resolve any conflicts, but calculates
  // and applies delta for each direction independently. It is possible to
  // craft synthetic inputs to make e.g. both bottom_delta and top_delta
  // non-zero and the result violate constraints in both directions, but blink
  // guarantees we never encounter such cases. It is also impossible to resolve
  // conflicts here because this code doesn't know logical directions.
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
