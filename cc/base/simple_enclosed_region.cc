// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/simple_enclosed_region.h"

#include <stddef.h>
#include <stdint.h>

#include "base/check_op.h"
#include "cc/base/region.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {

static bool RectIsLargerArea(const gfx::Rect& a, const gfx::Rect b) {
  int64_t a_area = static_cast<int64_t>(a.width()) * a.height();
  int64_t b_area = static_cast<int64_t>(b.width()) * b.height();
  return a_area > b_area;
}

SimpleEnclosedRegion::SimpleEnclosedRegion(const Region& region) {
  for (gfx::Rect rect : region)
    Union(rect);
}

SimpleEnclosedRegion::~SimpleEnclosedRegion() = default;

void SimpleEnclosedRegion::Subtract(const gfx::Rect& sub_rect) {
  // We want to keep as much of the current rect as we can, so find the one
  // largest rectangle inside |rect_| that does not intersect with |sub_rect|.
  if (!rect_.Intersects(sub_rect))
    return;
  if (sub_rect.Contains(rect_)) {
    rect_ = gfx::Rect();
    return;
  }

  int left = rect_.x();
  int right = rect_.right();
  int top = rect_.y();
  int bottom = rect_.bottom();

  int delta_left = sub_rect.x() - left;
  int delta_right = right - sub_rect.right();
  int delta_top = sub_rect.y() - top;
  int delta_bottom = bottom - sub_rect.bottom();

  // The horizontal rect is the larger of the two rectangles above or below
  // |sub_rect| and inside rect_.
  int horizontal_top = top;
  int horizontal_bottom = bottom;
  if (delta_top > delta_bottom)
    horizontal_bottom = sub_rect.y();
  else
    horizontal_top = sub_rect.bottom();
  // The vertical rect is the larger of the two rectangles to the left or the
  // right of |sub_rect| and inside rect_.
  int vertical_left = left;
  int vertical_right = right;
  if (delta_left > delta_right)
    vertical_right = sub_rect.x();
  else
    vertical_left = sub_rect.right();

  rect_.SetRect(
      left, horizontal_top, right - left, horizontal_bottom - horizontal_top);

  gfx::Rect vertical_rect(
      vertical_left, top, vertical_right - vertical_left, bottom - top);
  if (RectIsLargerArea(vertical_rect, rect_))
    rect_ = vertical_rect;
}

void SimpleEnclosedRegion::Union(const gfx::Rect& new_rect) {
  // We want to keep track of a region but bound its complexity at a constant
  // size. We keep track of the largest rectangle seen by area. If we can add
  // the |new_rect| to this rectangle then we do that, as that is the cheapest
  // way to increase the area returned without increasing the complexity.
  if (new_rect.IsEmpty())
    return;
  if (rect_.Contains(new_rect))
    return;
  if (new_rect.Contains(rect_)) {
    rect_ = new_rect;
    return;
  }

  int left = rect_.x();
  int top = rect_.y();
  int right = rect_.right();
  int bottom = rect_.bottom();

  int new_left = new_rect.x();
  int new_top = new_rect.y();
  int new_right = new_rect.right();
  int new_bottom = new_rect.bottom();

  // This attempts to expand each edge of |rect_| if the |new_rect| entirely
  // covers or is adjacent to an entire edge of |rect_|. If this is true for
  // an edge of |rect_| then it can be expanded out to share that edge with the
  // same edge of |new_rect|. After, the same thing is done to try expand
  // |new_rect| relative to |rect_|.
  if (new_top <= top && new_bottom >= bottom) {
    if (new_left < left && new_right >= left)
      left = new_left;
    if (new_right > right && new_left <= right)
      right = new_right;
  } else if (new_left <= left && new_right >= right) {
    if (new_top < top && new_bottom >= top)
      top = new_top;
    if (new_bottom > bottom && new_top <= bottom)
      bottom = new_bottom;
  } else if (top <= new_top && bottom >= new_bottom) {
    if (left < new_left && right >= new_left)
      new_left = left;
    if (right > new_right && left <= new_right)
      new_right = right;
  } else if (left <= new_left && right >= new_right) {
    if (top < new_top && bottom >= new_top)
      new_top = top;
    if (bottom > new_bottom && top <= new_bottom)
      new_bottom = bottom;
  }

  rect_.SetRect(left, top, right - left, bottom - top);
  int64_t rect_area = static_cast<int64_t>(rect_.width()) * rect_.height();
  gfx::Rect adjusted_new_rect(
      new_left, new_top, new_right - new_left, new_bottom - new_top);
  int64_t adjust_new_rect_area =
      static_cast<int64_t>(adjusted_new_rect.width()) *
      adjusted_new_rect.height();
  gfx::Rect overlap = gfx::IntersectRects(rect_, adjusted_new_rect);
  int64_t overlap_area =
      static_cast<int64_t>(overlap.width()) * overlap.height();

  // Based on the assumption that as we compute occlusion, each step is
  // more likely to be occluded by things added to this region more recently due
  // to the way we build scenes with overlapping elements adjacent to each other
  // in the Z order. So, the area of the new rect has a weight of 2 in the
  // weighted area calculation.
  if (adjust_new_rect_area * 2 > rect_area + overlap_area)
    rect_ = adjusted_new_rect;
}

gfx::Rect SimpleEnclosedRegion::GetRect(size_t i) const {
  DCHECK_LT(i, GetRegionComplexity());
  return rect_;
}

}  // namespace cc
