// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/pointer_crop_geometry.h"

#include <algorithm>

#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"

namespace ttc {

namespace {

// How much of the page to show on every side of the pointed-at spots, and the
// least width to show, so a single spot still has enough context around it.
// TODO(crbug.com/571268337): Make these feature params when get_pointer uses
// them.
constexpr float kMarginDip = 150;
constexpr float kMinWidthDip = 500;

}  // namespace

gfx::Rect ComputePointerCropRect(base::span<const gfx::PointF> points,
                                 const gfx::Size& viewport_dip) {
  if (points.empty() || viewport_dip.IsEmpty()) {
    return gfx::Rect();
  }

  // The bounding box of the points, each kept inside the viewport.
  const gfx::RectF viewport(viewport_dip.width(), viewport_dip.height());
  gfx::PointF top_left = viewport.bottom_right();
  gfx::PointF bottom_right = viewport.origin();
  for (const gfx::PointF& point : points) {
    const gfx::PointF clamped = viewport.ClosestPoint(point);
    top_left.SetToMin(clamped);
    bottom_right.SetToMax(clamped);
  }
  const gfx::RectF bounds = gfx::BoundingRect(top_left, bottom_right);

  // The points plus margins, at least the minimum width, at most the viewport.
  gfx::SizeF size = bounds.size();
  size.Enlarge(2 * kMarginDip, 2 * kMarginDip);
  size.SetToMax(gfx::SizeF(kMinWidthDip, 0));
  size.SetToMin(viewport.size());

  // Centered on the points, then shifted to stay inside the viewport.
  const gfx::PointF center = bounds.CenterPoint();
  const gfx::PointF origin(std::clamp(center.x() - size.width() / 2, 0.0f,
                                      viewport.width() - size.width()),
                           std::clamp(center.y() - size.height() / 2, 0.0f,
                                      viewport.height() - size.height()));

  // Enclosing may overshoot by a pixel at the far edges.
  gfx::Rect crop = gfx::ToEnclosingRect(gfx::RectF(origin, size));
  crop.Intersect(gfx::Rect(viewport_dip));
  return crop;
}

}  // namespace ttc
