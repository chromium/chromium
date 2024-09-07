// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/utility/arc_curve_path_util.h"

#include <optional>

#include "base/check_op.h"
#include "base/logging.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPathBuilder.h"
#include "third_party/skia/include/core/SkPoint.h"
#include "ui/gfx/geometry/size.h"

namespace ash::util {

namespace {

// Aliases ---------------------------------------------------------------------

using CornerLocation = ArcCurveCorner::CornerLocation;

// Helpers ---------------------------------------------------------------------

// Returns the corner point specified by `location` on the bounding rectangle
// of `size`.
SkPoint GetCornerPoint(const gfx::Size& size, CornerLocation location) {
  bool at_left = false;
  switch (location) {
    case CornerLocation::kBottomLeft:
    case CornerLocation::kTopLeft:
      at_left = true;
      break;
    case CornerLocation::kBottomRight:
    case CornerLocation::kTopRight:
      at_left = false;
      break;
  }

  bool at_bottom = false;
  switch (location) {
    case CornerLocation::kBottomLeft:
    case CornerLocation::kBottomRight:
      at_bottom = true;
      break;
    case CornerLocation::kTopLeft:
    case CornerLocation::kTopRight:
      at_bottom = false;
      break;
  }

  return SkPoint::Make(at_left ? 0 : size.width(),
                       at_bottom ? size.height() : 0);
}

// Returns the next corner counterclockwise from `corner`.
CornerLocation GetNextCorner(CornerLocation corner) {
  return corner == CornerLocation::kMax
             ? CornerLocation::kMin
             : static_cast<CornerLocation>(static_cast<size_t>(corner) + 1);
}

// Returns the previous corner counterclockwise from `corner`.
CornerLocation GetPrevCorner(CornerLocation corner) {
  return corner == CornerLocation::kMin
             ? CornerLocation::kMax
             : static_cast<CornerLocation>(static_cast<size_t>(corner) - 1);
}

bool IsSizeAndCornerRadiusValid(const gfx::Size& size,
                                const std::optional<size_t>& corner_radius) {
  if (size.IsEmpty()) {
    LOG(ERROR) << "GetArcCurveRectPath() is called with an empty size: "
               << size.ToString();
    return false;
  }

  if (corner_radius &&
      (2 * corner_radius.value() >
       static_cast<size_t>(std::min(size.width(), size.height())))) {
    LOG(ERROR) << "GetArcCurveRectPath() is called with a size that is too "
                  "small for rounded corners; the size: "
               << size.ToString() << ", corner radius: " << *corner_radius;

    return false;
  }

  return true;
}

}  // namespace

// ArcCurveCorner --------------------------------------------------------------

ArcCurveCorner::ArcCurveCorner(CornerLocation location,
                               const gfx::Size& size,
                               float concave_radius,
                               float convex_radius)
    : location(location),
      size(size),
      concave_radius(concave_radius),
      convex_radius(convex_radius) {
  CHECK_LE(convex_radius * 2 + concave_radius, size.width());
  CHECK_LE(convex_radius * 2 + concave_radius, size.height());
}

// Utils -----------------------------------------------------------------------

SkPath GetArcCurveRectPath(const gfx::Size& size, const size_t corner_radius) {
  if (!IsSizeAndCornerRadiusValid(size, corner_radius)) {
    return SkPath();
  }

  const auto width = size.width();
  const auto height = size.height();

  const auto bottom_left = SkPoint::Make(0.f, height);
  const auto bottom_right = SkPoint::Make(width, height);
  const auto top_right = SkPoint::Make(width, 0.f);
  const auto top_left = SkPoint::Make(0.f, 0.f);

  // One-radius offsets that can be added to or subtracted from coordinates to
  // indicate a unidirectional move, e.g., when calculating the endpoint of an
  // arc.
  const auto horizontal_offset = SkPoint::Make(corner_radius, 0.f);
  const auto vertical_offset = SkPoint::Make(0.f, corner_radius);

  return SkPathBuilder()
      // Start just after the curve of the top-left rounded corner.
      .moveTo(0.f, corner_radius)
      .arcTo(bottom_left, bottom_left + horizontal_offset, corner_radius)
      .arcTo(bottom_right, bottom_right - vertical_offset, corner_radius)
      .arcTo(top_right, top_right - horizontal_offset, corner_radius)
      .arcTo(top_left, top_left + vertical_offset, corner_radius)
      .close()
      .detach();
}

SkPath GetArcCurveRectPath(const gfx::Size& size,
                           const ArcCurveCorner& arc_curve_corner,
                           const std::optional<size_t>& corner_radius) {
  if (!IsSizeAndCornerRadiusValid(size, corner_radius)) {
    return SkPath();
  }

  if (const gfx::Size& arc_corner_size = arc_curve_corner.size;
      size.height() < arc_corner_size.height() ||
      size.width() < arc_corner_size.width()) {
    LOG(ERROR) << "GetArcCurveRectPath() is called with a size that is too "
                  "small for the arc curve corner; the size: "
               << size.ToString() << ", arc_curve_corner size: "
               << arc_curve_corner.size.ToString();

    return SkPath();
  }

  // Iterate all corners counterclockwise, starting from the top left corner.
  // Therefore, the total iteration count should be 4.
  SkPathBuilder builder;
  CornerLocation current_corner = CornerLocation::kMin;
  for (size_t iteration_index = 0; iteration_index < 4; ++iteration_index) {
    const SkPoint current_corner_point = GetCornerPoint(size, current_corner);

    // Calculate the normalized vector from the previous corner counterclockwise
    // to `current_corner`. For example, if `current_corner` is the top left
    // one, this vector should be (-1, 0).
    SkVector prev_to_current_normalized_offset =
        current_corner_point -
        GetCornerPoint(size, GetPrevCorner(current_corner));
    SkVector::Normalize(&prev_to_current_normalized_offset);

    const bool is_arc_curve = current_corner == arc_curve_corner.location;

    // Calculate the starting point of the path for `current_corner`.
    const SkVector start_offset =
        SkVector::Make(prev_to_current_normalized_offset.x() *
                           (is_arc_curve ? arc_curve_corner.size.width()
                                         : corner_radius.value_or(0)),
                       prev_to_current_normalized_offset.y() *
                           (is_arc_curve ? arc_curve_corner.size.height()
                                         : corner_radius.value_or(0)));
    const SkPoint corner_path_start = current_corner_point - start_offset;

    if (builder.snapshot().isEmpty()) {
      builder.moveTo(corner_path_start);
    } else {
      builder.lineTo(corner_path_start);
    }

    // Calculate the normalized vector from `current_corner` to the next corner
    // counterclockwise. For example, if `current_corner` is the top left one,
    // this vector should be (0, 1).
    SkVector current_to_next_normalized_offset =
        GetCornerPoint(size, GetNextCorner(current_corner)) -
        current_corner_point;
    SkVector::Normalize(&current_to_next_normalized_offset);

    const SkVector offset_sum =
        prev_to_current_normalized_offset + current_to_next_normalized_offset;

    // Calculate the remaining offset after excluding the spacing required by
    // the convex radius and the concave radius.
    const float radius_sum_spacing =
        2 * arc_curve_corner.convex_radius + arc_curve_corner.concave_radius;
    const auto extra_spacing_offset =
        SkVector::Make(arc_curve_corner.size.width(),
                       arc_curve_corner.size.height()) -
        SkVector::Make(radius_sum_spacing, radius_sum_spacing);

    if (is_arc_curve) {
      // Draw the first convex curve.
      SkPoint arc1 = corner_path_start + prev_to_current_normalized_offset *
                                             arc_curve_corner.convex_radius;
      SkPoint arc2 =
          corner_path_start + offset_sum * arc_curve_corner.convex_radius;
      builder.arcTo(arc1, arc2, arc_curve_corner.convex_radius);

      // Draw the extra spacing.
      arc2 = arc2 + SkVector::Make(extra_spacing_offset.x() *
                                       current_to_next_normalized_offset.x(),
                                   extra_spacing_offset.y() *
                                       current_to_next_normalized_offset.y());
      builder.lineTo(arc2);

      // Draw the concave curve.
      SkPoint last_point = arc2;
      arc1 = last_point + current_to_next_normalized_offset *
                              arc_curve_corner.concave_radius;
      arc2 = last_point + offset_sum * arc_curve_corner.concave_radius;
      builder.arcTo(arc1, arc2, arc_curve_corner.concave_radius);

      // Draw the extra spacing.
      last_point = arc2;
      arc2 =
          last_point +
          SkVector::Make(
              extra_spacing_offset.x() * prev_to_current_normalized_offset.x(),
              extra_spacing_offset.y() * prev_to_current_normalized_offset.y());
      builder.lineTo(arc2);

      // Draw the second convex curve.
      last_point = arc2;
      arc1 = last_point +
             prev_to_current_normalized_offset * arc_curve_corner.convex_radius;
      arc2 = last_point + offset_sum * arc_curve_corner.convex_radius;
      builder.arcTo(arc1, arc2, arc_curve_corner.convex_radius);
    } else if (corner_radius) {
      const SkPoint arc1 =
          corner_path_start +
          prev_to_current_normalized_offset * corner_radius.value();
      const SkPoint arc2 =
          corner_path_start + offset_sum * corner_radius.value();
      builder.arcTo(arc1, arc2, corner_radius.value());
    }

    if (current_corner == CornerLocation::kMax) {
      builder.close();
    } else {
      current_corner = GetNextCorner(current_corner);
    }
  }

  return builder.detach();
}

}  // namespace ash::util
