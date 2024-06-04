// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/rounded_rect_cutout_path_builder.h"

#include <array>
#include <utility>

#include "base/check_op.h"
#include "base/containers/fixed_flat_map.h"
#include "base/logging.h"
#include "third_party/skia/include/core/SkPathBuilder.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkSize.h"

namespace ash {

namespace {

enum class Direction { kCounterClockwise, kClockwise };

class CornersSequence {
 public:
  // A never ending sequence of corners around a rectangle in a given
  // `direction` starting at `start`.
  CornersSequence(RoundedRectCutoutPathBuilder::Corner start,
                  Direction direction)
      : direction_(direction) {
    size_t i = 0;
    for (; i < kOrderedCorners.size(); i++) {
      if (kOrderedCorners[i] == start) {
        break;
      }
    }
    current_ = i;
  }

  // Returns the current corner and advances in `direction_`.
  RoundedRectCutoutPathBuilder::Corner Next() {
    RoundedRectCutoutPathBuilder::Corner corner = kOrderedCorners[current_];
    current_ = NextIndex(current_, direction_);
    return corner;
  }

  // Returns the current corner then updates the current corner in the opposite
  // of `direction_`.
  RoundedRectCutoutPathBuilder::Corner Back() {
    RoundedRectCutoutPathBuilder::Corner corner = kOrderedCorners[current_];
    Direction reversed_direction = direction_ == Direction::kCounterClockwise
                                       ? Direction::kClockwise
                                       : Direction::kCounterClockwise;
    current_ = NextIndex(current_, reversed_direction);
    return corner;
  }

  // Returns the corner that is opposite of the current corner.
  RoundedRectCutoutPathBuilder::Corner OppositeCurrent() const {
    // Since this is a rectangle, the opposite corner is always 2 steps away.
    return kOrderedCorners.at((current_ + 2) % 4);
  }

 private:
  // Corners in counterclockwise order.
  static constexpr std::array<RoundedRectCutoutPathBuilder::Corner, 4>
      kOrderedCorners = {RoundedRectCutoutPathBuilder::Corner::kUpperLeft,
                         RoundedRectCutoutPathBuilder::Corner::kLowerLeft,
                         RoundedRectCutoutPathBuilder::Corner::kLowerRight,
                         RoundedRectCutoutPathBuilder::Corner::kUpperRight};

  int NextIndex(int current, Direction direction) const {
    int index = current + (direction == Direction::kCounterClockwise
                               ? 1
                               : kOrderedCorners.size() - 1);
    return index % kOrderedCorners.size();
  }

  Direction direction_;
  // Tracks the index of the current corner.
  int current_;
};

// Returns the offset vector from the kUpperLeft to `corner` for `rect`.
//
// NOTE: Since the vector is relative to `rect`, it is independent of the origin
// of `rect`.
SkVector OffsetFromUpperLeft(RoundedRectCutoutPathBuilder::Corner corner,
                             const SkRect& rect) {
  switch (corner) {
    case RoundedRectCutoutPathBuilder::Corner::kUpperLeft:
      return {0.f, 0.f};
    case RoundedRectCutoutPathBuilder::Corner::kLowerLeft:
      return {0.f, rect.height()};
    case RoundedRectCutoutPathBuilder::Corner::kLowerRight:
      return {rect.width(), rect.height()};
    case RoundedRectCutoutPathBuilder::Corner::kUpperRight:
      return {rect.width(), 0.f};
  }
}

// Returns the `corner` point of the `rect`.
SkPoint CornerFromRect(RoundedRectCutoutPathBuilder::Corner corner,
                       const SkRect& rect) {
  SkPoint top_left(rect.left(), rect.top());
  return top_left + OffsetFromUpperLeft(corner, rect);
}

// Move `rect` so that the position of `rect_corner` in `rect` matches the
// position of `view_corner` in `view`.
void MatchCorners(RoundedRectCutoutPathBuilder::Corner view_corner,
                  const SkRect& view,
                  RoundedRectCutoutPathBuilder::Corner rect_corner,
                  SkRect& rect) {
  SkPoint new_location = CornerFromRect(view_corner, view) -
                         OffsetFromUpperLeft(rect_corner, rect);
  rect.offsetTo(new_location.x(), new_location.y());
}

// Modifies `rect` so that the `corner` of `view` and `rect` match.
// e.g. If `corner` is `kBottomRight`, `rect` is moved so that the bottom right
// corner of `view` and `rect` match.
void MoveRectToCorner(RoundedRectCutoutPathBuilder::Corner corner,
                      const SkRect& view,
                      SkRect& rect) {
  MatchCorners(corner, view, corner, rect);
}

// Generates 3 rectangles (small corner, inner corner, small corner) that
// represent the bounds of the arcs for the cutout at `corner`. The cutout is
// of `cutout_size`, inscribed into `view`. Small corners have
// `outer_corner_radius` and the inner corner has `inner_corner_radius`.
//
// e.g. Corner::kLowerRight
//
//             SS
//             SS
//         LLLLOO
//         LLLLOO
//         LLLLOO
//       RROOOOOO
//       RROOOOOO
//
//  R = 1st outer corner, L = inner corner, S = 2nd outer corner, O = Remainder
//  of cutout (not in rects).
std::array<SkRect, 3> PlaceRects(RoundedRectCutoutPathBuilder::Corner corner,
                                 const SkRect& view,
                                 const SkSize& cutout_size,
                                 float outer_corner_radius,
                                 float inner_corner_radius) {
  // rects[0] is the first outer corner, rects[1] is the inner corner, rects[2]
  // is the last outer corner.
  std::array<SkRect, 3> rects = {
      SkRect::MakeWH(outer_corner_radius, outer_corner_radius),
      SkRect::MakeWH(inner_corner_radius, inner_corner_radius),
      SkRect::MakeWH(outer_corner_radius, outer_corner_radius)};

  SkRect cutout = SkRect::MakeSize(cutout_size);
  // Place `cutout` in the appropriate corner of `view`.
  MoveRectToCorner(corner, view, cutout);

  CornersSequence sequence(corner, Direction::kClockwise);
  // For the cutout, corners are drawn in the opposite direction from how we
  // iterate around the rectangle (because the cutouts are convex).  We happen
  // to draw all the corners except the corner where the cutout is located. So
  // start there but skip it.
  sequence.Next();

  MatchCorners(sequence.Next(), cutout, corner, rects[0]);
  MoveRectToCorner(sequence.Next(), cutout, rects[1]);
  MatchCorners(sequence.Next(), cutout, corner, rects[2]);

  // This draws nonsensical curves if the corners overlap.
  CHECK(!rects[1].intersect(rects[0]));
  CHECK(!rects[1].intersect(rects[2]));
  return rects;
}

// Add a rounded path to `builder` for a `corner` of `rect`. The path is drawn
// counter clockwise from the corner before `corner` to the opposite corner.
// e.g. if `corner` is kUpperRight, the path is drawn from kLowerRight to
// kUpperLeft.
void AddRoundedCorner(RoundedRectCutoutPathBuilder::Corner corner,
                      const SkRect& rect,
                      SkPathBuilder& builder,
                      Direction direction = Direction::kCounterClockwise) {
  CornersSequence sequence(corner, direction);
  // The large rounded corner starts at the corner before the corner where
  // it is drawn. So, backup one and discard it since we'll hit it again.
  sequence.Back();

  SkPoint start = CornerFromRect(sequence.Next(), rect);
  SkPoint control_point = CornerFromRect(sequence.Next(), rect);
  SkPoint end = CornerFromRect(sequence.Next(), rect);

  builder.lineTo(start);
  builder.conicTo(control_point, end, SK_ScalarRoot2Over2);
}

// Adds the required paths to `builder` to draw a the cutout of
// `cutout_size` within `view` with `outer_corner_radius` for the first two
// corners and `inner_corner_radius` for the interior corner. A line will be
// drawn to the first conic will be drawn from the location of `builder`.
// The path will end at the end of the last conic.
SkRect AddCutoutPaths(RoundedRectCutoutPathBuilder::Corner corner,
                      SkPathBuilder& builder,
                      const SkRect& view,
                      const SkSize& cutout_size,
                      int outer_corner_radius,
                      int inner_corner_radius) {
  // Create rectangles that enclose each of the curves.
  std::array<SkRect, 3> rects = PlaceRects(
      corner, view, cutout_size, outer_corner_radius, inner_corner_radius);

  // Draw the first outer corner.
  AddRoundedCorner(corner, rects[0], builder);

  // The inner corner is drawn opposite from the current corner and in the
  // clockwise direction because it is convex.
  CornersSequence sequence(corner, Direction::kCounterClockwise);
  AddRoundedCorner(sequence.OppositeCurrent(), rects[1], builder,
                   Direction::kClockwise);

  // Draw the other outer corner.
  AddRoundedCorner(corner, rects[2], builder);

  // A rectangle enclosing all the rectangles to conservatively check for
  // overlap.
  SkRect union_rect = rects[0];
  union_rect.join(rects[1]);
  union_rect.join(rects[2]);
  return union_rect;
}

}  // namespace

RoundedRectCutoutPathBuilder::RoundedRectCutoutPathBuilder(gfx::SizeF bounds)
    : bounds_(bounds) {
  CHECK_GE(bounds.width(), corner_radius_ * 2.f)
      << "Width must be at least twice as large as corner radius";
  CHECK_GE(bounds.height(), corner_radius_ * 2.f)
      << "Height must be at least twice as large as corner radius";
}

RoundedRectCutoutPathBuilder::RoundedRectCutoutPathBuilder(
    const RoundedRectCutoutPathBuilder&) = default;
RoundedRectCutoutPathBuilder& RoundedRectCutoutPathBuilder::operator=(
    const RoundedRectCutoutPathBuilder&) = default;

RoundedRectCutoutPathBuilder::~RoundedRectCutoutPathBuilder() = default;

RoundedRectCutoutPathBuilder& RoundedRectCutoutPathBuilder::CornerRadius(
    int radius) {
  corner_radius_ = radius;
  return *this;
}

RoundedRectCutoutPathBuilder& RoundedRectCutoutPathBuilder::AddCutout(
    RoundedRectCutoutPathBuilder::Corner corner,
    gfx::SizeF size) {
  if (size.IsZero()) {
    cutouts_.erase(corner);
    return *this;
  }

  cutouts_[corner] = size;
  return *this;
}

RoundedRectCutoutPathBuilder&
RoundedRectCutoutPathBuilder::CutoutInnerCornerRadius(int radius) {
  cutout_inner_corner_radius_ = radius;
  return *this;
}

RoundedRectCutoutPathBuilder&
RoundedRectCutoutPathBuilder::CutoutOuterCornerRadius(int radius) {
  cutout_outer_corner_radius_ = radius;
  return *this;
}

SkPath RoundedRectCutoutPathBuilder::Build() {
  SkRect view = SkRect::MakeWH(bounds_.width(), bounds_.height());
  CHECK_GE(view.width(), corner_radius_ * 2.f)
      << "Width must be at least twice as large as corner radius";
  CHECK_GE(view.height(), corner_radius_ * 2.f)
      << "Height must be at least twice as large as corner radius";

  SkPathBuilder builder;
  // Start at the top center of the rectangle.
  builder.moveTo(view.width() / 2.f, view.top());

  // Save cutout bounds to check for overlap.
  std::vector<SkRect> drawn_cutouts;
  drawn_cutouts.reserve(4);

  // Build paths counter clockwise around view.
  CornersSequence around(RoundedRectCutoutPathBuilder::Corner::kUpperLeft,
                         Direction::kCounterClockwise);
  RoundedRectCutoutPathBuilder::Corner cur_corner = around.Next();
  do {
    auto iter = cutouts_.find(cur_corner);
    if (iter != cutouts_.end()) {
      // Adds the paths for the cutout.
      gfx::SizeF size = iter->second;
      CHECK_GE(bounds_.width(),
               (size.width() + cutout_outer_corner_radius_ + corner_radius_))
          << "Cutout width + outer corner radius + corner radius must be less "
             "than or equal to bounds width";
      CHECK_GE(bounds_.height(),
               (size.height() + cutout_outer_corner_radius_ + corner_radius_))
          << "Cutout height + outer corner radius + corner radius must be less "
             "than or equal to bounds height";

      SkRect rect = AddCutoutPaths(
          cur_corner, builder, view, SkSize::Make(size.width(), size.height()),
          cutout_outer_corner_radius_, cutout_inner_corner_radius_);
      drawn_cutouts.push_back(rect);
    } else {
      if (corner_radius_ == 0) {
        // If corner radius is 0, it's a point so just draw a line.
        SkPoint point = CornerFromRect(cur_corner, view);
        builder.lineTo(point);
      } else {
        // Draw the rounded corners for the larger view.
        SkRect rect = SkRect::MakeWH(corner_radius_, corner_radius_);
        MoveRectToCorner(cur_corner, view, rect);
        AddRoundedCorner(cur_corner, rect, builder);
      }
    }
    cur_corner = around.Next();
  } while (cur_corner != RoundedRectCutoutPathBuilder::Corner::kUpperLeft);

  // Verify that none of the cutouts intersect or the drawn shape will not work.
  // This is checking all pairs so it's O(n^2) but n<4.
  for (size_t i = 0; i < drawn_cutouts.size(); i++) {
    const SkRect& cutout = drawn_cutouts[i];
    for (size_t j = i + 1; j < drawn_cutouts.size(); j++) {
      CHECK(!cutout.intersects(drawn_cutouts[j]))
          << "At least two cutouts intersect and the path is invalid";
    }
  }

  // `close()` will draw a line from the last point to the start (top middle of
  // the shape).
  builder.close();
  return builder.detach();
}

}  // namespace ash
