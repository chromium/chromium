// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/scoped_layer_tree_synchronizer.h"

#include <cmath>
#include <cstdlib>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/notreached.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/mask_filter_info.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace ash {
namespace {

using Corner = gfx::RRectF::Corner;

constexpr float PI = 3.1415926f;

float Square(float n) {
  return std::pow(n, 2);
}

// Represents an arc of the circle, defined by its center, radius and start and
// end angles.
struct CircularArc {
  static constexpr int DEGREE_0 = 0;
  static constexpr int DEGREE_90 = 90;
  static constexpr int DEGREE_180 = 180;
  static constexpr int DEGREE_270 = 270;
  static constexpr int DEGREE_360 = 360;

  gfx::PointF center;
  float radius;

  // Angles are in degrees.
  int start_angle;
  int end_angle;

  bool Intersects(const CircularArc& other) const {
    return FindIntersection(other);
  }

  // Return true if the arc inclusively contains the `point`.
  bool InclusivelyContains(const gfx::PointF point) const {
    const float distance = std::sqrt(Square(point.x() - center.x()) +
                                     Square(point.y() - center.y()));
    if (distance > radius) {
      return false;
    }

    // atan2 range is angle between (-PI, PI]. Therefore we must transform the
    // angle in range [0, 2*PI).
    // Note: atan2 assumes that the positive y-axis runs from bottom
    // to top, whereas our positive y-axis runs from top to bottom. Therefore,
    // we need to flip the direction y vector.
    const float angle_in_radians =
        std::atan2(-(point.y() - center.y()), point.x() - center.x());

    const int normalized_angle_in_degree =
        angle_in_radians < DEGREE_0
            ? (angle_in_radians + 2 * PI) * (DEGREE_180 / PI)
            : angle_in_radians * (DEGREE_180 / PI);

    return normalized_angle_in_degree >= start_angle &&
           normalized_angle_in_degree <= end_angle;
  }

  gfx::PointF GetMidPointOnArc() const {
    const float mid_angle_in_degree =
        start_angle + (end_angle - start_angle) / 2;
    const float mid_angle_in_radian = mid_angle_in_degree * PI / DEGREE_180;

    // The parametric equations for a circle with center (cx, cy) and radius r
    // are:
    // x = cx + r * cos(theta)
    // y = cy + r * sin(theta)
    // where theta is the angle counter-clockwise from +x axis.
    // Note: For y coordinate, We have a negative sign calculation since
    // our y-axis runs positive top-down.
    const float x = center.x() + radius * std::cos(mid_angle_in_radian);
    const float y = center.y() - radius * std::sin(mid_angle_in_radian);

    return {x, y};
  }

 private:
  using Points = std::pair<std::vector<gfx::PointF>, /*infinite_points=*/bool>;

  bool FindIntersection(const CircularArc& other) const {
    const Points points = FindIntersectionBetweenTwoCircles(
        center, radius, other.center, other.radius);

    // If the intersection points between two arcs are infinite, it indicates
    // that both arcs belong to the same circle.
    if (points.second) {
      return start_angle >= other.start_angle && end_angle <= other.end_angle;
    }

    for (const auto& point : points.first) {
      if (InclusivelyContains(point) && other.InclusivelyContains(point)) {
        return true;
      }
    }

    return false;
  }

  // Returns the points intersection of two circles centered at points c1, c2
  // and have radius of r1, r2 respectively. If the two circles, are identical,
  // the output has `infinite_points` boolean set to true.
  // For mathematical explanation, see b/326468096.
  Points FindIntersectionBetweenTwoCircles(const gfx::PointF& c1,
                                           float r1,
                                           const gfx::PointF& c2,
                                           float r2) const {
    static constexpr Points kNoIntersectingPoint = {{}, false};
    static constexpr Points kInfiniteIntersectingPoints = {{}, true};

    // Same circles.
    if (c1 == c2 && r1 == r2) {
      return kInfiniteIntersectingPoints;
    }

    // Distance between the centers of the two cycles.
    float d = std::sqrt(Square(c2.x() - c1.x()) + Square(c2.y() - c1.y()));

    // No intersection.
    if (d > r1 + r2) {
      return kNoIntersectingPoint;
    }

    // One circle contains the other.
    if (d < std::abs(r1 - r2)) {
      return kNoIntersectingPoint;
    }

    // Single intersection point. Circles are tangent to each other.
    if (d == r1 + r2) {
      float x = (c1.x() + c2.x()) / 2;
      float y = (c1.y() + c2.y()) / 2;
      return {{gfx::PointF(x, y)}, /*infinite_points=*/false};
    }

    // Circles intersect at two points.
    if (d > std::abs(r1 - r2) && d < r1 + r2) {
      float a = (Square(r1) - Square(r2) + Square(d)) / (2 * d);
      float h = std::sqrt(Square(r1) - Square(a));

      float p5_x = c1.x() + (a / d) * (c2.x() - c1.x());
      float p5_y = c1.y() + (a / d) * (c2.y() - c1.y());

      float p3_x = p5_x - (h / d) * (c2.y() - c1.y());
      float p3_y = p5_y + (h / d) * (c2.x() - c1.x());

      float p4_x = p3_x + (h / d) * (c2.y() - c1.y());
      float p4_y = p3_y - (h / d) * (c2.x() - c1.x());

      return {{gfx::PointF(p3_x, p3_y), gfx::PointF(p4_x, p4_y)},
              /*infinite_points=*/false};
    }

    NOTREACHED_NORETURN();
  }
};

// Returns the arc that represents the corner of `rrectf`.
CircularArc GetArcForCorner(const gfx::RRectF& rrectf, Corner corner) {
  const gfx::RectF bounding_box = rrectf.CornerBoundingRect(corner);

  CircularArc corner_arc;
  const auto radii = rrectf.GetCornerRadii(corner);
  corner_arc.radius = radii.x();

  switch (corner) {
    case Corner::kUpperRight:
      corner_arc.center = bounding_box.bottom_left();
      corner_arc.start_angle = CircularArc::DEGREE_0;
      corner_arc.end_angle = CircularArc::DEGREE_90;
      break;
    case Corner::kUpperLeft:
      corner_arc.center = bounding_box.bottom_right();
      corner_arc.start_angle = CircularArc::DEGREE_90;
      corner_arc.end_angle = CircularArc::DEGREE_180;
      break;
    case Corner::kLowerLeft:
      corner_arc.center = bounding_box.top_right();
      corner_arc.start_angle = CircularArc::DEGREE_180;
      corner_arc.end_angle = CircularArc::DEGREE_270;
      break;
    case Corner::kLowerRight:
      corner_arc.center = bounding_box.origin();
      corner_arc.start_angle = CircularArc::DEGREE_270;
      corner_arc.end_angle = CircularArc::DEGREE_360;
      break;
  }

  return corner_arc;
}

// Returns true if the point `p` is contained by `rrectf`. It takes into account
// the curvature of the corners.
bool CheckCornerContainment(const gfx::PointF& p, const gfx::RRectF& rrectf) {
  const gfx::RectF rectf = rrectf.rect();
  if (!rectf.InclusiveContains(p)) {
    return false;
  }

  Corner containing_corner;
  gfx::PointF canonical_point;  // p translated to one of the quadrants

  const float x = p.x(), y = p.y();

  if (x < rectf.x() + rrectf.GetCornerRadii(Corner::kUpperLeft).x() &&
      y < rectf.y() + rrectf.GetCornerRadii(Corner::kUpperLeft).y()) {
    // Upper left corner.
    containing_corner = Corner::kUpperLeft;
    canonical_point.SetPoint(
        x - (rectf.x() + rrectf.GetCornerRadii(Corner::kUpperLeft).x()),
        y - (rectf.y() + rrectf.GetCornerRadii(Corner::kUpperLeft).y()));
    CHECK(canonical_point.x() < 0 && canonical_point.y() < 0);
  } else if (x < rectf.x() + rrectf.GetCornerRadii(Corner::kLowerLeft).x() &&
             y > rectf.bottom() -
                     rrectf.GetCornerRadii(Corner::kLowerLeft).y()) {
    // Lower left corner.
    containing_corner = Corner::kLowerLeft;
    canonical_point.SetPoint(
        x - (rectf.x() + rrectf.GetCornerRadii(Corner::kLowerLeft).x()),
        y - (rectf.bottom() - rrectf.GetCornerRadii(Corner::kLowerLeft).y()));
    CHECK(canonical_point.x() < 0 && canonical_point.y() > 0);
  } else if (x > rectf.right() -
                     rrectf.GetCornerRadii(Corner::kUpperRight).x() &&
             y < rectf.y() + rrectf.GetCornerRadii(Corner::kUpperRight).y()) {
    // Upper right corner.
    containing_corner = Corner::kUpperRight;
    canonical_point.SetPoint(
        x - (rectf.right() - rrectf.GetCornerRadii(Corner::kUpperRight).x()),
        y - (rectf.y() + rrectf.GetCornerRadii(Corner::kUpperRight).y()));
    CHECK(canonical_point.x() > 0 && canonical_point.y() < 0);
  } else if (x > rectf.right() -
                     rrectf.GetCornerRadii(Corner::kLowerRight).x() &&
             y > rectf.bottom() -
                     rrectf.GetCornerRadii(Corner::kLowerRight).y()) {
    // Lower right corner.
    containing_corner = Corner::kLowerRight;
    canonical_point.SetPoint(
        x - (rectf.right() - rrectf.GetCornerRadii(Corner::kLowerRight).x()),
        y - (rectf.bottom() - rrectf.GetCornerRadii(Corner::kLowerRight).y()));
    CHECK(canonical_point.x() > 0 && canonical_point.y() > 0);
  } else {
    // Not in any of the corners.
    return true;
  }

  // A point is in an ellipse (in standard position) if:
  //      x^2     y^2
  //     ----- + ----- <= 1
  //      a^2     b^2
  // or :
  //     b^2*x^2 + a^2*y^2 <= (ab)^2
  const float distance =
      Square(canonical_point.x()) *
          Square(rrectf.GetCornerRadii(containing_corner).y()) +
      Square(canonical_point.y()) *
          Square(rrectf.GetCornerRadii(containing_corner).x());

  return distance <= Square(rrectf.GetCornerRadii(containing_corner).x() *
                            rrectf.GetCornerRadii(containing_corner).y());
}

gfx::PointF GetCornerCoordinates(const gfx::RectF& rectf, Corner corner) {
  switch (corner) {
    case Corner::kUpperLeft:
      return rectf.origin();
    case Corner::kUpperRight:
      return rectf.top_right();
    case Corner::kLowerRight:
      return rectf.bottom_right();
    case Corner::kLowerLeft:
      return rectf.bottom_left();
  }
}

// Determine whether the corner radius of `rect` should be overridden to match
// the corner radius of `containing_rect`. If `consider_curvature` is true,
// the curvature of `rect` is taken into account.
bool ShouldOverrideCornerRadius(const gfx::RRectF& rect,
                                const gfx::RRectF& containing_rect,
                                Corner corner,
                                bool consider_curvature) {
  if (rect.IsEmpty() || containing_rect.IsEmpty()) {
    return false;
  }

  if (!containing_rect.rect().Contains(rect.rect())) {
    return false;
  }

  const gfx::Vector2dF rect_corner_radii = rect.GetCornerRadii(corner);
  const gfx::Vector2dF containing_rect_corner_radii =
      containing_rect.GetCornerRadii(corner);

  // If both the corners are square, it does not make sense to override the
  // radius of the corner.
  if (rect_corner_radii.IsZero() && containing_rect_corner_radii.IsZero()) {
    return false;
  }

  // If only the corner of containing_rect is square, we do not need to
  // override the radius of rect since the curvature of rect's is contained by
  // the square containing rect corner.
  if (containing_rect_corner_radii.IsZero()) {
    return false;
  }

  const gfx::PointF rect_corner_coordinates =
      GetCornerCoordinates(rect.rect(), corner);

  // If only the corner of rect is square, we must override the radius if the
  // square corner of rect is located outside the curvature of the rect's
  // corner.
  if (rect_corner_radii.IsZero()) {
    return !CheckCornerContainment(rect_corner_coordinates, containing_rect);
  }

  if (!consider_curvature) {
    const gfx::PointF containing_rect_corner_coordinates =
        GetCornerCoordinates(containing_rect.rect(), corner);
    return rect_corner_coordinates == containing_rect_corner_coordinates;
  }

  const CircularArc arc = GetArcForCorner(rect, corner);
  const CircularArc other_arc = GetArcForCorner(containing_rect, corner);

  // In the case where both corners of the containing_rect and rect are rounded,
  // we should override the corner radius in following cases:
  //  * The corners (represented as arcs) intersect with each other.
  //  * The corners do not intersect but curvature of rect's corner lies outside
  //    the curvature of the containing_rect's corner.
  return arc.Intersects(other_arc) ||
         !CheckCornerContainment(arc.GetMidPointOnArc(), containing_rect);
}

using Corners = base::flat_set<gfx::RRectF::Corner>;

// Returns the set of corners of rect that need to have their radius match the
// corner radius of containing_rect.
Corners FindCornersToOverrideRadius(const gfx::RRectF& rect,
                                    const gfx::RRectF& containing_rect,
                                    bool consider_curvature) {
  Corners corners;

  for (auto corner : {Corner::kUpperLeft, Corner::kUpperRight,
                      Corner::kLowerRight, Corner::kLowerLeft}) {
    if (ShouldOverrideCornerRadius(rect, containing_rect, corner,
                                   consider_curvature)) {
      corners.insert(corner);
    }
  }

  return corners;
}

gfx::RRectF ApplyTransform(const gfx::RRectF& bounds,
                           const gfx::Transform& transform) {
  gfx::MaskFilterInfo layer_mask_info(bounds);
  layer_mask_info.ApplyTransform(transform);

  return layer_mask_info.rounded_corner_bounds();
}

gfx::Transform AccumulateTargetTransform(const ui::Layer* layer,
                                         const gfx::Transform& transform) {
  gfx::Transform translation;
  translation.Translate(layer->bounds().x(), layer->bounds().y());

  gfx::Transform accumulated_transform(transform);
  accumulated_transform.PreConcat(translation);

  const gfx::Transform& layer_transform = layer->GetTargetTransform();
  if (!layer_transform.IsIdentity()) {
    accumulated_transform.PreConcat(layer_transform);
  }

  return accumulated_transform;
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// ScopedLayerTreeSynchronizerBase:

ScopedLayerTreeSynchronizerBase::ScopedLayerTreeSynchronizerBase(
    ui::Layer* root_layer,
    bool restore_tree)
    : root_layer_(root_layer), restore_tree_(restore_tree) {
  CHECK(root_layer);
}

ScopedLayerTreeSynchronizerBase::~ScopedLayerTreeSynchronizerBase() = default;

void ScopedLayerTreeSynchronizerBase::ResetCachedLayerInfo() {
  original_layers_info_.clear();
}

bool ScopedLayerTreeSynchronizerBase::SynchronizeLayerTreeRoundedCorners(
    ui::Layer* layer,
    bool consider_curvature,
    const gfx::RRectF& reference_bounds) {
  CHECK(root_layer_->Contains(layer));
  if (reference_bounds.IsEmpty() ||
      reference_bounds.GetType() == gfx::RRectF::Type::kRect) {
    return false;
  }

  gfx::Transform transform;
  layer->GetTargetTransformRelativeTo(root_layer_, &transform);

  return SynchronizeLayerTreeRoundedCornersImpl(layer, consider_curvature,
                                                reference_bounds, transform);
}

bool ScopedLayerTreeSynchronizerBase::SynchronizeLayerTreeRoundedCornersImpl(
    ui::Layer* layer,
    bool consider_curvature,
    const gfx::RRectF& reference_bounds,
    const gfx::Transform& transform) {
  CHECK(layer);
  CHECK(transform.IsScaleOrTranslation());

  bool layer_altered = false;
  if (!layer->rounded_corner_radii().IsEmpty()) {
    // Get the `layer` bounds in the `root_layer_` coordinate space.
    // `transform` accounts for layer offset from its parent.
    gfx::RRectF layer_rrectf(gfx::RectF(layer->bounds().size()),
                             layer->rounded_corner_radii());
    auto transformed_layer_bounds = ApplyTransform(layer_rrectf, transform);

    // Finds the corners of the `layer` that either intersect with the corners
    // of the `reference_bounds` or are drawn outside the curvature (if any) of
    // the reference_bounds rounded corners. The function considers the
    // curvature (if any) of the layer corners as well.
    const Corners corners_to_update = FindCornersToOverrideRadius(
        transformed_layer_bounds, reference_bounds, consider_curvature);

    if (!corners_to_update.empty()) {
      // The inverse transform coverts from the coordinate space of `layer` to
      // the coordinate space of 'root_layer_'.
      const gfx::Transform inverse_transform = transform.GetCheckedInverse();

      const float scale_x = inverse_transform.rc(0, 0);

      // The `reference_bounds` radii are scaled so that when the transformation
      // of the `layer` is applied, the `layer` radii match the radii of the
      // reference_bounds radii.
      gfx::RoundedCornersF radii = layer->rounded_corner_radii();

      radii.Set(corners_to_update.contains(Corner::kUpperLeft)
                    ? reference_bounds.GetCornerRadii(Corner::kUpperLeft).x() *
                          scale_x
                    : radii.upper_left(),
                corners_to_update.contains(Corner::kUpperRight)
                    ? reference_bounds.GetCornerRadii(Corner::kUpperRight).x() *
                          scale_x
                    : radii.upper_right(),
                corners_to_update.contains(Corner::kLowerRight)
                    ? reference_bounds.GetCornerRadii(Corner::kLowerRight).x() *
                          scale_x
                    : radii.lower_right(),
                corners_to_update.contains(Corner::kLowerLeft)
                    ? reference_bounds.GetCornerRadii(Corner::kLowerLeft).x() *
                          scale_x
                    : radii.lower_left());

      if (radii != layer->rounded_corner_radii()) {
        // If `original_layers_info_` has an entry, it means the layer
        // radii has been changed in a prior call to
        // `SynchronizeLayerTreeRoundedCorners()`
        if (restore_tree_ && !original_layers_info_.contains(layer)) {
          original_layers_info_.insert({layer,
                                        {layer->rounded_corner_radii(),
                                         layer->is_fast_rounded_corner()}});
        }

        layer->SetRoundedCornerRadius(radii);
        layer->SetIsFastRoundedCorner(/*enable=*/!radii.IsEmpty());
        layer_altered = true;
      }
    }
  }

  bool subtree_altered = false;
  for (ui::Layer* child : layer->children()) {
    subtree_altered |= SynchronizeLayerTreeRoundedCornersImpl(
        child, consider_curvature, reference_bounds,
        AccumulateTargetTransform(child, transform));
  }

  return subtree_altered || layer_altered;
}

void ScopedLayerTreeSynchronizerBase::RestoreLayerTree(ui::Layer* layer) {
  CHECK(root_layer_->Contains(layer));
  if (original_layers_info_.empty()) {
    return;
  }

  RestoreLayerTreeImpl(layer);
}

void ScopedLayerTreeSynchronizerBase::RestoreLayerTreeImpl(ui::Layer* layer) {
  if (original_layers_info_.contains(layer)) {
    const auto& info = original_layers_info_.at(layer);
    layer->SetRoundedCornerRadius(info.first);
    layer->SetIsFastRoundedCorner(/*enable=*/info.second);
  }

  for (ui::Layer* child : layer->children()) {
    RestoreLayerTreeImpl(child);
  }
}

///////////////////////////////////////////////////////////////////////////////
// ScopedLayerTreeSynchronizer:

ScopedLayerTreeSynchronizer::ScopedLayerTreeSynchronizer(ui::Layer* root_layer,
                                                         bool restore_tree)
    : ScopedLayerTreeSynchronizerBase(root_layer, restore_tree) {}

ScopedLayerTreeSynchronizer::~ScopedLayerTreeSynchronizer() {
  Restore();
}

void ScopedLayerTreeSynchronizer::SynchronizeRoundedCorners(
    ui::Layer* layer,
    const gfx::RRectF& reference_bounds) {
  SynchronizeLayerTreeRoundedCorners(layer, /*consider_curvature=*/true,
                                     reference_bounds);
}

void ScopedLayerTreeSynchronizer::Restore() {
  RestoreLayerTree(root_layer());
  ResetCachedLayerInfo();
}

///////////////////////////////////////////////////////////////////////////////
// ScopedWindowTreeSynchronizer:

ScopedWindowTreeSynchronizer::ScopedWindowTreeSynchronizer(
    aura::Window* root_window,
    bool restore_tree)
    : ScopedLayerTreeSynchronizerBase(root_window->layer(), restore_tree) {}

ScopedWindowTreeSynchronizer::~ScopedWindowTreeSynchronizer() {
  Restore();
}

void ScopedWindowTreeSynchronizer::SynchronizeRoundedCorners(
    aura::Window* window,
    bool consider_curvature,
    const gfx::RRectF& reference_bounds,
    TransientTreeIgnorePredicate ignore_predicate) {
  for (auto* window_iter : GetTransientTreeIterator(window, ignore_predicate)) {
    const bool altered = SynchronizeLayerTreeRoundedCorners(
        window_iter->layer(), consider_curvature, reference_bounds);
    if (altered &&
        !altered_window_observations_.IsObservingSource(window_iter)) {
      altered_window_observations_.AddObservation(window_iter);
    }
  }
}

void ScopedWindowTreeSynchronizer::Restore() {
  for (aura::Window* window : altered_window_observations_.sources()) {
    RestoreLayerTree(window->layer());
  }

  ResetCachedLayerInfo();
  altered_window_observations_.RemoveAllObservations();
}

void ScopedWindowTreeSynchronizer::OnWindowDestroying(aura::Window* window) {
  altered_window_observations_.RemoveObservation(window);
}

}  // namespace ash
