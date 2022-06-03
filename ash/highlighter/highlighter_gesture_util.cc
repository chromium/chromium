// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/highlighter/highlighter_gesture_util.h"

#include <cmath>

#include "ash/fast_ink/fast_ink_points.h"
#include "base/numerics/math_constants.h"

namespace ash {

namespace {

constexpr float kHorizontalStrokeLengthThreshold = 20;
constexpr float kHorizontalStrokeThicknessThreshold = 2;
constexpr float kHorizontalStrokeFlatnessThreshold = 0.1;

constexpr double kClosedShapeSweepThreshold = base::kPiDouble * 2 * 0.8;
constexpr double kClosedShapeJiggleThreshold = 0.1;

bool DetectHorizontalStroke(const gfx::RectF& box,
                            const gfx::SizeF& pen_tip_size) {
  return box.width() > kHorizontalStrokeLengthThreshold &&
         box.height() <
             pen_tip_size.height() * kHorizontalStrokeThicknessThreshold &&
         box.height() < box.width() * kHorizontalStrokeFlatnessThreshold;
}

bool DetectClosedShape(const gfx::RectF& box,
                       const fast_ink::FastInkPoints& points) {
  if (points.GetNumberOfPoints() < 3)
    return false;

  const gfx::PointF center = box.CenterPoint();

  // Analyze vectors pointing from the center to each point.
  // Compute the cumulative swept angle and count positive
  // and negative angles separately.
  double swept_angle = 0.0;
  int positive = 0;
  int negative = 0;

  double prev_angle = 0.0;
  bool has_prev_angle = false;

  for (const auto& point : points.points()) {
    const double angle =
        atan2(point.location.y() - center.y(), point.location.x() - center.x());
    if (has_prev_angle) {
      double diff_angle = angle - prev_angle;
      if (diff_angle > base::kPiDouble) {
        diff_angle -= base::kPiDouble * 2;
      } else if (diff_angle < -base::kPiDouble) {
        diff_angle += base::kPiDouble * 2;
      }
      swept_angle += diff_angle;
      if (diff_angle > 0)
        positive++;
      if (diff_angle < 0)
        negative++;
    } else {
      has_prev_angle = true;
    }
    prev_angle = angle;
  }

  if (std::abs(swept_angle) < kClosedShapeSweepThreshold) {
    // Has not swept enough of the full circle.
    return false;
  }

  if (swept_angle > 0 && (static_cast<double>(negative) / positive) >
                             kClosedShapeJiggleThreshold) {
    // Main direction is positive, but went too often in the negative direction.
    return false;
  }
  if (swept_angle < 0 && (static_cast<double>(positive) / negative) >
                             kClosedShapeJiggleThreshold) {
    // Main direction is negative, but went too often in the positive direction.
    return false;
  }

  return true;
}

}  // namespace

HighlighterGestureType DetectHighlighterGesture(
    const gfx::RectF& box,
    const gfx::SizeF& pen_tip_size,
    const fast_ink::FastInkPoints& points) {
  if (DetectHorizontalStroke(box, pen_tip_size))
    return HighlighterGestureType::kHorizontalStroke;

  if (DetectClosedShape(box, points))
    return HighlighterGestureType::kClosedShape;

  return HighlighterGestureType::kNotRecognized;
}

}  // namespace ash
