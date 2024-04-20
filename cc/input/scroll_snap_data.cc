// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/scroll_snap_data.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>

#include "base/auto_reset.h"
#include "base/check.h"
#include "base/notreached.h"
#include "cc/base/features.h"
#include "cc/input/snap_selection_strategy.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace cc {
namespace {

gfx::Vector2dF DistanceFromCorridor(double dx,
                                    double dy,
                                    const gfx::RectF& area) {
  gfx::Vector2dF distance;

  if (dx < 0)
    distance.set_x(-dx);
  else if (dx > area.width())
    distance.set_x(dx - area.width());
  else
    distance.set_x(0);

  if (dy < 0)
    distance.set_y(-dy);
  else if (dy > area.height())
    distance.set_y(dy - area.height());
  else
    distance.set_y(0);

  return distance;
}

bool IsMutualVisible(const SnapSearchResult& a, const SnapSearchResult& b) {
  return gfx::RangeF(b.snap_offset()).IsBoundedBy(a.visible_range()) &&
         gfx::RangeF(a.snap_offset()).IsBoundedBy(b.visible_range());
}

void SetOrUpdateResult(const SnapSearchResult& candidate,
                       std::optional<SnapSearchResult>* result) {
  if (result->has_value()) {
    result->value().Union(candidate);
    if (candidate.has_focus_within()) {
      result->value().set_element_id(candidate.element_id());
    }
  } else {
    *result = candidate;
  }
}

const std::optional<SnapSearchResult>& ClosestSearchResult(
    const gfx::PointF reference_point,
    SearchAxis axis,
    const std::optional<SnapSearchResult>& a,
    const std::optional<SnapSearchResult>& b) {
  if (!a.has_value())
    return b;
  if (!b.has_value())
    return a;

  float reference_position =
      axis == SearchAxis::kX ? reference_point.x() : reference_point.y();
  float position_a = a.value().snap_offset();
  float position_b = b.value().snap_offset();
  DCHECK(
      (reference_position <= position_a && reference_position <= position_b) ||
      (reference_position >= position_a && reference_position >= position_b));

  float distance_a = std::abs(position_a - reference_position);
  float distance_b = std::abs(position_b - reference_position);

  return distance_a < distance_b ? a : b;
}

std::optional<SnapSearchResult> SearchResultForDodgingRange(
    const gfx::RangeF& area_range,
    const gfx::RangeF& dodging_range,
    const SnapSearchResult& aligned_candidate,
    float preferred_offset,
    float scroll_padding,
    float snapport_size,
    SnapAlignment alignment) {
  if (dodging_range.is_empty() || dodging_range.is_reversed()) {
    return std::nullopt;
  }

  // Use aligned_candidate as a template (we will override snap_offset and
  // covered_range).
  SnapSearchResult result = aligned_candidate;

  float min_offset = dodging_range.start() - scroll_padding;
  float max_offset = dodging_range.end() - scroll_padding - snapport_size;

  if (max_offset > min_offset) {
    result.set_snap_offset(
        std::clamp(preferred_offset, min_offset, max_offset));
    result.set_covered_range(gfx::RangeF(min_offset, max_offset));
    return result;
  }

  // The scrollport does not fit in the dodging range, but we should still
  // return a snap position so that the content inside the dodging range is not
  // unreachable. Choose a position by applying the snap area's alignment.

  float offset;
  switch (alignment) {
    case SnapAlignment::kStart:
      offset = min_offset;
      break;
    case SnapAlignment::kCenter:
      offset = (min_offset + max_offset) / 2;
      break;
    case SnapAlignment::kEnd:
      offset = max_offset;
      break;
    default:
      NOTREACHED();
  }

  min_offset = area_range.start() - scroll_padding;
  max_offset = area_range.end() - scroll_padding - snapport_size;
  if (max_offset < min_offset) {
    return std::nullopt;
  }

  result.set_snap_offset(std::clamp(offset, min_offset, max_offset));
  return result;
}

bool CanCoverSnapportOnAxis(SearchAxis axis,
                            const gfx::RectF& container_rect,
                            const gfx::RectF& area_rect) {
  return (axis == SearchAxis::kY &&
          area_rect.height() >= container_rect.height()) ||
         (axis == SearchAxis::kX &&
          area_rect.width() >= container_rect.width());
}

}  // namespace

SnapSearchResult::SnapSearchResult(float offset,
                                   SearchAxis axis,
                                   gfx::RangeF snapport_visible_range,
                                   float snapport_max_visible)
    : snap_offset_(offset),
      axis_(axis),
      snapport_visible_range_(snapport_visible_range),
      snapport_max_visible_(snapport_max_visible) {}

void SnapSearchResult::Clip(float max_snap) {
  snap_offset_ = std::clamp(snap_offset_, 0.0f, max_snap);
}

void SnapSearchResult::Union(const SnapSearchResult& other) {
  DCHECK(snap_offset_ == other.snap_offset_);
  DCHECK(rect_.has_value() && other.rect().has_value());
  if (rect_ && other.rect().has_value()) {
    rect_->Union(other.rect().value());
  }
}

SnapContainerData::SnapContainerData()
    : proximity_range_(gfx::PointF(std::numeric_limits<float>::max(),
                                   std::numeric_limits<float>::max())) {}

SnapContainerData::SnapContainerData(ScrollSnapType type)
    : scroll_snap_type_(type),
      proximity_range_(gfx::PointF(std::numeric_limits<float>::max(),
                                   std::numeric_limits<float>::max())) {}

SnapContainerData::SnapContainerData(ScrollSnapType type,
                                     const gfx::RectF& rect,
                                     const gfx::PointF& max)
    : scroll_snap_type_(type),
      rect_(rect),
      max_position_(max),
      proximity_range_(gfx::PointF(std::numeric_limits<float>::max(),
                                   std::numeric_limits<float>::max())) {}

SnapContainerData::SnapContainerData(const SnapContainerData& other) = default;

SnapContainerData::SnapContainerData(SnapContainerData&& other) = default;

SnapContainerData::~SnapContainerData() = default;

SnapContainerData& SnapContainerData::operator=(
    const SnapContainerData& other) = default;

SnapContainerData& SnapContainerData::operator=(SnapContainerData&& other) =
    default;

void SnapContainerData::AddSnapAreaData(SnapAreaData snap_area_data) {
  snap_area_list_.push_back(snap_area_data);
}

SnapPositionData SnapContainerData::FindSnapPositionWithViewportAdjustment(
    const SnapSelectionStrategy& strategy,
    double snapport_height_adjustment) {
  base::AutoReset<double> resetter{&snapport_height_adjustment_,
                                   snapport_height_adjustment};
  return FindSnapPosition(strategy);
}

SnapPositionData SnapContainerData::FindSnapPosition(
    const SnapSelectionStrategy& strategy) const {
  SnapPositionData result;
  result.target_element_ids = TargetSnapAreaElementIds();
  if (scroll_snap_type_.is_none)
    return result;

  gfx::PointF base_position = strategy.base_position();
  SnapAxis axis = scroll_snap_type_.axis;
  bool should_snap_on_x = strategy.ShouldSnapOnX() &&
                          (axis == SnapAxis::kX || axis == SnapAxis::kBoth);
  bool should_snap_on_y = strategy.ShouldSnapOnY() &&
                          (axis == SnapAxis::kY || axis == SnapAxis::kBoth);
  if (!should_snap_on_x && !should_snap_on_y) {
    // We may arrive here because the strategy wants to snap in an axis in
    // which we do not snap, and doesn't want to snap in an axis in which we do
    // snap. Ensure that we retain the id of the target in any axis where we are
    // snapped.
    if (axis == SnapAxis::kY) {
      result.target_element_ids.y = target_snap_area_element_ids_.y;
    } else {
      result.target_element_ids.x = target_snap_area_element_ids_.x;
    }
    return result;
  }

  bool should_prioritize_x_target =
      strategy.ShouldPrioritizeSnapTargets() &&
      target_snap_area_element_ids_.x != ElementId();
  bool should_prioritize_y_target =
      strategy.ShouldPrioritizeSnapTargets() &&
      target_snap_area_element_ids_.y != ElementId();

  std::optional<SnapSearchResult> selected_x, selected_y;
  if (should_snap_on_x) {
    // Start from current position in the cross axis. The search algorithm
    // expects the cross axis position to be inside scroller bounds. But since
    // we cannot always assume that the incoming value fits this criteria we
    // clamp it to the bounds to ensure this variant.
    SnapSearchResult initial_snap_position_y = {
        std::clamp(base_position.y(), 0.f, max_position_.y()), SearchAxis::kY,
        gfx::RangeF(rect_.x(), rect_.right()), max_position_.x()};
    if (should_prioritize_x_target) {
      selected_x = GetTargetSnapAreaSearchResult(strategy, SearchAxis::kX,
                                                 initial_snap_position_y);
    }
    if (!selected_x) {
      selected_x = FindClosestValidArea(SearchAxis::kX, strategy,
                                        initial_snap_position_y);
    }
  }
  if (should_snap_on_y) {
    SnapSearchResult initial_snap_position_x = {
        std::clamp(base_position.x(), 0.f, max_position_.x()), SearchAxis::kX,
        gfx::RangeF(rect_.y(), rect_.bottom()), max_position_.y()};
    if (should_prioritize_y_target) {
      selected_y = GetTargetSnapAreaSearchResult(strategy, SearchAxis::kY,
                                                 initial_snap_position_x);
    }
    if (!selected_y) {
      selected_y = FindClosestValidArea(SearchAxis::kY, strategy,
                                        initial_snap_position_x);
    }
  }

  if (!selected_x.has_value() && !selected_y.has_value()) {
    // Searching along each axis separately can miss valid snap positions if
    // snapping along both axes and the snap positions are off screen.
    if (should_snap_on_x && should_snap_on_y &&
        !strategy.ShouldRespectSnapStop() &&
        FindSnapPositionForMutualSnap(strategy, &result.position)) {
      result.type = SnapPositionData::Type::kAligned;
    }

    return result;
  }

  if (selected_x.has_value() && selected_y.has_value() &&
      !IsMutualVisible(selected_x.value(), selected_y.value())) {
    SnapAxis axis_to_follow = SelectAxisToFollowForMutualVisibility(
        strategy, selected_x.value(), selected_y.value());
    if (axis_to_follow == SnapAxis::kX) {
      selected_y =
          FindClosestValidArea(SearchAxis::kY, strategy, selected_x.value());
    } else {
      selected_x =
          FindClosestValidArea(SearchAxis::kX, strategy, selected_y.value());
    }
  }

  // For each axis, the alternative makes a better selection if it is also
  // aligned in the cross axis.
  if (selected_y && selected_y->alternative()) {
    SelectAlternativeIdForSearchResult(*selected_y, selected_x,
                                       strategy.current_position().x(),
                                       max_position_.x());
  }
  if (selected_x && selected_x->alternative()) {
    SelectAlternativeIdForSearchResult(*selected_x, selected_y,
                                       strategy.current_position().y(),
                                       max_position_.y());
  }

  result.type = SnapPositionData::Type::kAligned;
  result.position = strategy.current_position();
  // Make sure that |result| retains what we are currently snapped to in each
  // axis in case this search had no result for one axis. This ensures we don't
  // incorrectly trigger a snap event. Don't retain ids of areas that may no
  // longer exist.
  for (const auto& area : snap_area_list_) {
    if (area.element_id == target_snap_area_element_ids_.x) {
      result.target_element_ids.x = target_snap_area_element_ids_.x;
    }
    if (area.element_id == target_snap_area_element_ids_.y) {
      result.target_element_ids.y = target_snap_area_element_ids_.y;
    }
  }

  if (selected_x) {
    result.position.set_x(selected_x->snap_offset());
    result.target_element_ids.x = selected_x->element_id();
    result.covered_range_x = selected_x->covered_range();
  }
  if (selected_y) {
    result.position.set_y(selected_y->snap_offset());
    result.target_element_ids.y = selected_y->element_id();
    result.covered_range_y = selected_y->covered_range();
  }
  if ((!selected_x || result.covered_range_x) &&
      (!selected_y || result.covered_range_y)) {
    result.type = SnapPositionData::Type::kCovered;
  }
  return result;
}

// This method is called only if the preferred algorithm fails to find either an
// x or a y snap position.
// The base algorithm searches on x (if appropriate) and then y (if
// appropriate). Each search is along the corridor in the search direction.
// For a search in the x-direction, areas as excluded from consideration if the
// range in the y-direction does not overlap the y base position (i.e. can
// scroll-snap in the x-direction without scrolling in the y-direction). Rules
// for scroll-snap in the y-direction are symmetric. This is the preferred
// approach, though the ordering of the searches should perhaps be determined
// based on axis locking.
// In cases where no valid snap points are found via searches along the axis
// corridors, the snap selection strategy allows for selection of areas outside
// of the corridors.
bool SnapContainerData::FindSnapPositionForMutualSnap(
    const SnapSelectionStrategy& strategy,
    gfx::PointF* snap_position) const {
  DCHECK(strategy.ShouldSnapOnX() && strategy.ShouldSnapOnY());
  bool found = false;
  gfx::Vector2dF smallest_distance(std::numeric_limits<float>::max(),
                                   std::numeric_limits<float>::max());

  // Snap to same element for x & y if possible.
  for (const SnapAreaData& area : snap_area_list_) {
    if (!strategy.IsValidSnapArea(SearchAxis::kX, area))
      continue;

    if (!strategy.IsValidSnapArea(SearchAxis::kY, area))
      continue;

    SnapSearchResult x_candidate = GetSnapSearchResult(SearchAxis::kX, area);
    float dx = x_candidate.snap_offset() - strategy.current_position().x();
    if (std::abs(dx) > proximity_range_.x())
      continue;

    SnapSearchResult y_candidate = GetSnapSearchResult(SearchAxis::kY, area);
    float dy = y_candidate.snap_offset() - strategy.current_position().y();
    if (std::abs(dy) > proximity_range_.y())
      continue;

    // Preferentially minimize block scrolling distance. Ties in block scrolling
    // distance are resolved by considering inline scrolling distance.
    gfx::Vector2dF distance = DistanceFromCorridor(dx, dy, snapport());
    if (distance.y() < smallest_distance.y() ||
        (distance.y() == smallest_distance.y() &&
         distance.x() < smallest_distance.x())) {
      smallest_distance = distance;
      snap_position->set_x(x_candidate.snap_offset());
      snap_position->set_y(y_candidate.snap_offset());
      found = true;
    }
  }

  return found;
}

std::optional<SnapSearchResult>
SnapContainerData::GetTargetSnapAreaSearchResult(
    const SnapSelectionStrategy& strategy,
    SearchAxis axis,
    SnapSearchResult cross_axis_snap_result) const {
  ElementId target_id = axis == SearchAxis::kX
                            ? target_snap_area_element_ids_.x
                            : target_snap_area_element_ids_.y;
  if (target_id == ElementId())
    return std::nullopt;
  for (const SnapAreaData& area : snap_area_list_) {
    if (area.element_id == target_id && strategy.IsValidSnapArea(axis, area)) {
      auto aligned_result = GetSnapSearchResult(axis, area);
      if (base::FeatureList::IsEnabled(
              features::kScrollSnapPreferCloserCovering) &&
          CanCoverSnapportOnAxis(axis, snapport(), area.rect)) {
        // This code path handles snapping after layout changes. If the
        // target snap area is larger than the snapport, we need to consider
        // snap areas nested within it, which may themselves be large snap areas
        // containing nested snap areas.
        gfx::RangeF area_range =
            axis == SearchAxis::kX
                ? gfx::RangeF(area.rect.x(), area.rect.right())
                : gfx::RangeF(area.rect.y(), area.rect.bottom());
        auto covering_result = FindClosestValidAreaInternal(
            axis, strategy, cross_axis_snap_result, true, area_range);
        return covering_result.has_value() ? covering_result.value()
                                           : aligned_result;
      }
      return aligned_result;
    }
  }
  return std::nullopt;
}

void SnapContainerData::UpdateSnapAreaForTesting(ElementId element_id,
                                                 SnapAreaData snap_area_data) {
  for (SnapAreaData& area : snap_area_list_) {
    if (area.element_id == element_id) {
      area = snap_area_data;
    }
  }
}

const TargetSnapAreaElementIds& SnapContainerData::GetTargetSnapAreaElementIds()
    const {
  return target_snap_area_element_ids_;
}

bool SnapContainerData::SetTargetSnapAreaElementIds(
    TargetSnapAreaElementIds ids) {
  if (target_snap_area_element_ids_ == ids)
    return false;

  target_snap_area_element_ids_ = ids;
  return true;
}

std::optional<SnapSearchResult> SnapContainerData::FindClosestValidArea(
    SearchAxis axis,
    const SnapSelectionStrategy& strategy,
    const SnapSearchResult& cross_axis_snap_result) const {
  std::optional<SnapSearchResult> result =
      FindClosestValidAreaInternal(axis, strategy, cross_axis_snap_result);

  // For EndAndDirectionStrategy, if there is a snap area with snap-stop:always,
  // and is between the starting position and the above result, we should choose
  // the first snap area with snap-stop:always.
  // This additional search is executed only if we found a result, while the
  // additional search for the relaxed_strategy is executed only if we didn't
  // find a result. So we put this search first so we can return early if we
  // could find a result.
  if (result.has_value() && strategy.ShouldRespectSnapStop()) {
    std::unique_ptr<SnapSelectionStrategy> must_only_strategy =
        SnapSelectionStrategy::CreateForDirection(
            strategy.current_position(),
            strategy.intended_position() - strategy.current_position(),
            strategy.UsingFractionalOffsets(), SnapStopAlwaysFilter::kRequire);
    std::optional<SnapSearchResult> must_only_result =
        FindClosestValidAreaInternal(axis, *must_only_strategy,
                                     cross_axis_snap_result, false);
    result = ClosestSearchResult(strategy.current_position(), axis, result,
                                 must_only_result);
  }
  // Our current direction based strategies are too strict ignoring the other
  // directions even when we have no candidate in the given direction. This is
  // particularly problematic with mandatory snap points and for fling
  // gestures. To counteract this, if the direction based strategy finds no
  // candidates, we do a second search ignoring the direction (this is
  // implemented by using an equivalent EndPosition strategy).
  if (result.has_value() ||
      scroll_snap_type_.strictness == SnapStrictness::kProximity ||
      !strategy.HasIntendedDirection())
    return result;

  std::unique_ptr<SnapSelectionStrategy> relaxed_strategy =
      SnapSelectionStrategy::CreateForEndPosition(strategy.current_position(),
                                                  strategy.ShouldSnapOnX(),
                                                  strategy.ShouldSnapOnY());
  return FindClosestValidAreaInternal(axis, *relaxed_strategy,
                                      cross_axis_snap_result);
}

std::optional<SnapSearchResult> SnapContainerData::FindClosestValidAreaInternal(
    SearchAxis axis,
    const SnapSelectionStrategy& strategy,
    const SnapSearchResult& cross_axis_snap_result,
    bool should_consider_covering,
    std::optional<gfx::RangeF> active_element_range) const {
  bool horiz = axis == SearchAxis::kX;
  // The cross axis result is expected to be within bounds otherwise no snap
  // area will meet the mutual visibility requirement.
  DCHECK(cross_axis_snap_result.snap_offset() >= 0 &&
         cross_axis_snap_result.snap_offset() <=
             (horiz ? max_position_.y() : max_position_.x()));

  // The search result from the snap area that's closest to the search origin.
  std::optional<SnapSearchResult> closest;
  // The search result with the intended position if it makes a snap area cover
  // the snapport.
  std::optional<SnapSearchResult> covering_intended;

  // The intended position of the scroll operation if there's no snap. This
  // scroll position becomes the covering candidate if there is a snap area that
  // fully covers the snapport if this position is scrolled to.
  float intended_position = horiz ? strategy.intended_position().x()
                                  : strategy.intended_position().y();
  // The position from which we search for the closest snap position.
  float base_position =
      horiz ? strategy.base_position().x() : strategy.base_position().y();

  float smallest_distance = horiz ? proximity_range_.x() : proximity_range_.y();

  auto evaluate = [&](const SnapSearchResult& candidate,
                      const SnapAreaData& area) {
    if (!IsMutualVisible(candidate, cross_axis_snap_result)) {
      return;
    }
    if (!strategy.IsValidSnapPosition(axis, candidate.snap_offset())) {
      return;
    }
    float distance = std::abs(candidate.snap_offset() - base_position);
    if (distance > smallest_distance) {
      return;
    }
    // Aligned snap areas that have focus should be given preference when
    // selecting snap targets.
    if (distance < smallest_distance || candidate.has_focus_within()) {
      smallest_distance = distance;
      closest = candidate;
    } else if (closest && !closest->has_focus_within()) {
      if (closest->element_id() == targeted_area_id_) {
        return;
      }
      if (candidate.element_id() == targeted_area_id_) {
        closest = candidate;
        return;
      }
      const auto candidate_rect = candidate.rect();
      const auto closest_rect = closest->rect();
      // Prefer snapping to innermost elements when nesting snap areas.
      // RectF::Contains allows equality but the candidate should only prevail
      // if it is smaller.
      DCHECK(closest_rect && candidate_rect);
      if (closest_rect && candidate_rect &&
          closest_rect->Contains(candidate_rect.value()) &&
          closest_rect != candidate_rect) {
        smallest_distance = distance;
        closest = candidate;
      } else if ((scroll_snap_type_.axis == SnapAxis::kBoth) &&
                 (area.scroll_snap_align.alignment_block !=
                  SnapAlignment::kNone) &&
                 (area.scroll_snap_align.alignment_inline !=
                  SnapAlignment::kNone)) {
        // This candidate is equally aligned with the current closest. Since it
        // can be snapped to in both axes, designate it a potential alternative
        // if we don't already have a potential alternative or it is a better
        // alternative than the current one.
        UpdateSearchAlternative(*closest, candidate, area, strategy);
      }
    }
  };

  for (const SnapAreaData& area : snap_area_list_) {
    if (!strategy.IsValidSnapArea(axis, area))
      continue;

    if (active_element_range) {
      gfx::RangeF area_range =
          horiz ? gfx::RangeF(area.rect.x(), area.rect.right())
                : gfx::RangeF(area.rect.y(), area.rect.bottom());
      if (!active_element_range->Intersects(area_range)) {
        continue;
      }
    }

    SnapSearchResult candidate = GetSnapSearchResult(axis, area);
    evaluate(candidate, area);
    if (should_consider_covering &&
        (base::FeatureList::IsEnabled(features::kScrollSnapPreferCloserCovering)
             ? CanCoverSnapportOnAxis(axis, snapport(), area.rect)
             : IsSnapportCoveredOnAxis(axis, intended_position, area.rect))) {
      if (std::optional<SnapSearchResult> covering =
              FindCoveringCandidate(area, axis, candidate, intended_position)) {
        covering->set_has_focus_within(area.has_focus_within);
        covering->set_rect(area.rect);
        if (covering->snap_offset() == intended_position) {
          SetOrUpdateResult(*covering, &covering_intended);
        } else {
          // A covering candidate that is displaced from the intended position
          // should behave similarly to an aligned snap position, competing on
          // distance with other aligned snap positions - unlike a covering
          // candidate at the intended position which may be given a higher
          // priority in ScrollSnapStrategy::PickBestResult.
          evaluate(*covering, area);
        }
      }
    }

    // Even if a snap area covers the snapport, we need to continue this
    // search to find previous and next snap positions and also to have
    // alternative snap candidates if this covering candidate is ultimately
    // rejected. And this covering snap area has its own alignment that may
    // generates a snap position rejecting the current inplace candidate.
  }

  const std::optional<SnapSearchResult>& picked =
      strategy.PickBestResult(closest, covering_intended);
  return picked;
}

SnapSearchResult SnapContainerData::GetSnapSearchResult(
    SearchAxis axis,
    const SnapAreaData& area) const {
  SnapSearchResult result;
  gfx::RectF rect = snapport();
  if (axis == SearchAxis::kX) {
    // https://www.w3.org/TR/css-scroll-snap-1/#scroll-snap-align
    // Snap alignment has been normalized for a horizontal left to right and top
    // to bottom writing mode.
    switch (area.scroll_snap_align.alignment_inline) {
      case SnapAlignment::kStart:
        result.set_snap_offset(area.rect.x() - rect.x());
        break;
      case SnapAlignment::kCenter:
        result.set_snap_offset(area.rect.CenterPoint().x() -
                               rect.CenterPoint().x());
        break;
      case SnapAlignment::kEnd:
        result.set_snap_offset(area.rect.right() - rect.right());
        break;
      default:
        NOTREACHED();
    }
    result.Clip(max_position_.x());
    result.set_snapport_max_visible(max_position_.y());
    result.set_snapport_visible_range(gfx::RangeF(rect.y(), rect.bottom()));
  } else {
    switch (area.scroll_snap_align.alignment_block) {
      case SnapAlignment::kStart:
        result.set_snap_offset(area.rect.y() - rect.y());
        break;
      case SnapAlignment::kCenter:
        result.set_snap_offset(area.rect.CenterPoint().y() -
                               rect.CenterPoint().y());
        break;
      case SnapAlignment::kEnd:
        result.set_snap_offset(area.rect.bottom() - rect.bottom());
        break;
      default:
        NOTREACHED();
    }
    result.Clip(max_position_.y());
    result.set_snapport_max_visible(max_position_.x());
    result.set_snapport_visible_range(gfx::RangeF(rect.x(), rect.right()));
  }
  result.set_axis(axis);
  result.set_rect(area.rect);
  result.set_has_focus_within(area.has_focus_within);
  result.set_element_id(area.element_id);
  return result;
}

std::optional<SnapSearchResult> SnapContainerData::FindCoveringCandidate(
    const SnapAreaData& area,
    SearchAxis axis,
    const SnapSearchResult& aligned_candidate,
    float intended_position) const {
  bool horiz = axis == SearchAxis::kX;
  gfx::RectF rect = snapport();
  float scroll_padding = horiz ? rect.x() : rect.y();
  float snapport_size = horiz ? rect.width() : rect.height();
  SnapAlignment alignment = horiz ? area.scroll_snap_align.alignment_inline
                                  : area.scroll_snap_align.alignment_block;
  gfx::RangeF area_range = horiz
                               ? gfx::RangeF(area.rect.x(), area.rect.right())
                               : gfx::RangeF(area.rect.y(), area.rect.bottom());
  gfx::RangeF preferred_snapport(
      intended_position + scroll_padding,
      intended_position + scroll_padding + snapport_size);

  gfx::RangeF backward_dodging_range = area_range;
  gfx::RangeF middle_dodging_range = area_range;
  gfx::RangeF forward_dodging_range = area_range;

  if (base::FeatureList::IsEnabled(
          features::kScrollSnapCoveringAvoidNestedSnapAreas)) {
    for (const SnapAreaData& intruder : snap_area_list_) {
      gfx::RangeF intruder_range =
          horiz ? gfx::RangeF(intruder.rect.x(), intruder.rect.right())
                : gfx::RangeF(intruder.rect.y(), intruder.rect.bottom());

      if (intruder_range.start() > area_range.end() ||
          intruder_range.end() < area_range.start()) {
        // Does not intrude.
        continue;
      }
      if (intruder_range.start() <= area_range.start() &&
          intruder_range.end() >= area_range.end()) {
        // Superset of `area` also not treated as an intruder.
        continue;
      }

      // Try three ways of dodging the intruders.
      // In full generality this requires an interval tree. But we can simplify
      // somewhat because we only care about a dodging range that is potentially
      // closer than an aligned snap position, which each intruder also
      // produces. For example, given:
      //      |---A---|     |---preferred snapport---|
      //             |---B---|
      // We do not care about the dodging range before the start of A.

      // backward_dodging_range finds a dodging range that is above any intruder
      // that intersects the snapport.
      if (intruder_range.end() < preferred_snapport.start()) {
        backward_dodging_range.set_start(
            std::max(backward_dodging_range.start(), intruder_range.end()));
      } else {
        backward_dodging_range.set_end(
            std::min(backward_dodging_range.end(), intruder_range.start()));
      }

      // forward_dodging_range finds a dodging range that is below any intruder
      // that intersects the snapport.
      if (intruder_range.start() > preferred_snapport.end()) {
        forward_dodging_range.set_end(
            std::min(forward_dodging_range.end(), intruder_range.start()));
      } else {
        forward_dodging_range.set_start(
            std::max(forward_dodging_range.start(), intruder_range.end()));
      }

      // middle_dodging_range finds a dodging range inside the snapport, if
      // there are intruders from above and below.
      if (intruder_range.Contains(preferred_snapport) ||
          preferred_snapport.Contains(intruder_range)) {
        middle_dodging_range = gfx::RangeF();
      } else if (intruder_range.start() <= preferred_snapport.start()) {
        middle_dodging_range.set_start(
            std::max(middle_dodging_range.start(), intruder_range.end()));
      } else {
        DCHECK(intruder_range.end() >= preferred_snapport.end());
        middle_dodging_range.set_end(
            std::min(middle_dodging_range.end(), intruder_range.start()));
      }
    }
  }

  std::optional<SnapSearchResult> middle_candidate =
      SearchResultForDodgingRange(area_range, middle_dodging_range,
                                  aligned_candidate, intended_position,
                                  scroll_padding, snapport_size, alignment);
  if (middle_candidate) {
    return middle_candidate;
  }

  std::optional<SnapSearchResult> backward_candidate =
      SearchResultForDodgingRange(area_range, backward_dodging_range,
                                  aligned_candidate, intended_position,
                                  scroll_padding, snapport_size, alignment);
  std::optional<SnapSearchResult> forward_candidate =
      SearchResultForDodgingRange(area_range, forward_dodging_range,
                                  aligned_candidate, intended_position,
                                  scroll_padding, snapport_size, alignment);

  if (!backward_candidate) {
    return forward_candidate;
  }

  if (!forward_candidate) {
    return backward_candidate;
  }

  float backward_distance =
      std::abs(backward_candidate->snap_offset() - intended_position);
  float forward_distance =
      std::abs(forward_candidate->snap_offset() - intended_position);

  return backward_distance < forward_distance ? backward_candidate
                                              : forward_candidate;
}

constexpr float kSnapportCoveredTolerance = 0.5;
bool SnapContainerData::IsSnapportCoveredOnAxis(
    SearchAxis axis,
    float current_offset,
    const gfx::RectF& area_rect) const {
  // We expand the range that SnapContainerData considers covering the snapport
  // by kSnapportCoveredTolerance to handle offsets at the boundaries of
  // the snap container. At the boundaries, |current_offset| might be a rounded
  // int coming from ScrollTree::ClampScrollOffsetToLimits which uses
  // ScrollNode::bounds which is a gfx::Size which stores ints.
  // See crbug.com/1468412.
  gfx::RectF rect = snapport();
  if (axis == SearchAxis::kX) {
    if (area_rect.width() < rect.width()) {
      return false;
    }
    float left = area_rect.x() - rect.x();
    float right = area_rect.right() - rect.right();
    return current_offset >= left - kSnapportCoveredTolerance &&
           current_offset <= right + kSnapportCoveredTolerance;
  } else {
    if (area_rect.height() < rect.height()) {
      return false;
    }
    float top = area_rect.y() - rect.y();
    float bottom = area_rect.bottom() - rect.bottom();
    return current_offset >= top - kSnapportCoveredTolerance &&
           current_offset <= bottom + kSnapportCoveredTolerance;
  }
}

// TODO(crbug.com/40941354): Use tolerance value less than 1.
// It is currently set to 1 because of differences in the way Blink and cc
// currently handle fractional offsets when snapping.
constexpr float kSnappedToTolerance = 1.0;
bool SnapContainerData::IsSnappedToArea(
    const SnapAreaData& area,
    const gfx::PointF& scroll_offset) const {
  bool covered_on_y =
      IsSnapportCoveredOnAxis(SearchAxis::kY, scroll_offset.y(), area.rect);
  bool covered_on_x =
      IsSnapportCoveredOnAxis(SearchAxis::kX, scroll_offset.x(), area.rect);
  bool snaps_on_x = scroll_snap_type_.axis == SnapAxis::kX ||
                    scroll_snap_type_.axis == SnapAxis::kBoth;
  bool snaps_on_y = scroll_snap_type_.axis == SnapAxis::kY ||
                    scroll_snap_type_.axis == SnapAxis::kBoth;
  if ((snaps_on_x && covered_on_x) && (snaps_on_y && covered_on_y)) {
    return true;
  }

  if (snaps_on_y &&
      area.scroll_snap_align.alignment_block != SnapAlignment::kNone) {
    SnapSearchResult snap_result_y = GetSnapSearchResult(SearchAxis::kY, area);
    if (((std::abs(snap_result_y.snap_offset() - scroll_offset.y()) <=
          kSnappedToTolerance) ||
         covered_on_y) &&
        gfx::RangeF(scroll_offset.x())
            .IsBoundedBy(snap_result_y.visible_range())) {
      return true;
    }
  }
  if (snaps_on_x &&
      area.scroll_snap_align.alignment_inline != SnapAlignment::kNone) {
    SnapSearchResult snap_result_x = GetSnapSearchResult(SearchAxis::kX, area);
    if (((std::abs(snap_result_x.snap_offset() - scroll_offset.x()) <=
          kSnappedToTolerance) ||
         covered_on_x) &&
        gfx::RangeF(scroll_offset.y())
            .IsBoundedBy(snap_result_x.visible_range())) {
      return true;
    }
  }

  return false;
}

gfx::RectF SnapContainerData::snapport() const {
  if (!snapport_height_adjustment_) {
    return rect_;
  }

  gfx::RectF adjusted = rect_;
  // The top visible point is not changed by showing / hiding the top controls;
  // they only expand the visible rect from that anchor point.
  adjusted.set_height(adjusted.height() + snapport_height_adjustment_);
  return adjusted;
}

void SnapContainerData::UpdateSearchAlternative(
    SnapSearchResult& current_result,
    const SnapSearchResult& candidate_result,
    const SnapAreaData& candidate_area,
    const SnapSelectionStrategy& strategy) const {
  bool horiz = current_result.axis() == SearchAxis::kX;
  const auto candidate_cross_axis_aligned_result = GetSnapSearchResult(
      horiz ? SearchAxis::kY : SearchAxis::kX, candidate_area);
  const auto candidate_rect = candidate_result.rect();
  const auto current_result_rect = current_result.rect();
  DCHECK(candidate_rect && current_result_rect);
  if (!candidate_rect || !current_result_rect ||
      candidate_rect->Contains(*current_result_rect)) {
    return;
  }
  if (auto alt = current_result.alternative()) {
    float cross_axis_base_position =
        horiz ? strategy.base_position().y() : strategy.base_position().x();
    float candidate_cross_axis_distance =
        std::abs(cross_axis_base_position -
                 candidate_cross_axis_aligned_result.snap_offset());
    float alt_cross_axis_distance =
        std::abs(cross_axis_base_position - alt->cross_axis_snap_offset);
    if (candidate_cross_axis_distance > alt_cross_axis_distance) {
      return;
    }
    const auto alt_rect = alt->area_rect;
    // This candidate beats our current alternative if it is closer to the
    // base position in the cross axis than our current alternative,
    // or if it is tied with the current alternative and is nested within
    // the current alternative (inner targets are preferred to outer targets).
    if (candidate_cross_axis_distance < alt_cross_axis_distance ||
        (alt_rect != *candidate_rect && alt_rect.Contains(*candidate_rect))) {
      current_result.set_alternative(
          candidate_area.element_id, *candidate_rect,
          candidate_cross_axis_aligned_result.snap_offset());
    }
  } else {
    // We did not have an alternative before now, make the current
    // candidate our alternative.
    current_result.set_alternative(
        candidate_area.element_id, *candidate_rect,
        candidate_cross_axis_aligned_result.snap_offset());
  }
}

void SnapContainerData::SelectAlternativeIdForSearchResult(
    SnapSearchResult& selection,
    const std::optional<SnapSearchResult>& cross_selection,
    float cross_current_position,
    float cross_max_position) const {
  const auto within_snapped_tolerance = [](float v1, float v2) {
    return std::abs(v1 - v2) <= kSnappedToTolerance;
  };
  if (cross_selection) {
    if (within_snapped_tolerance(
            cross_selection->snap_offset(),
            selection.alternative()->cross_axis_snap_offset)) {
      selection.set_element_id(selection.alternative()->element_id);
    }
  } else {
    if (within_snapped_tolerance(
            std::clamp(cross_current_position, 0.0f, cross_max_position),
            selection.alternative()->cross_axis_snap_offset)) {
      selection.set_element_id(selection.alternative()->element_id);
    }
  }
}

SnapAxis SnapContainerData::SelectAxisToFollowForMutualVisibility(
    const SnapSelectionStrategy& strategy,
    const SnapSearchResult& selected_x,
    const SnapSearchResult& selected_y) const {
  // If snapping in one axis pushes off-screen the other snap area, this snap
  // position is invalid. https://drafts.csswg.org/css-scroll-snap-1/#snap-scope
  // In this case, first check if we need to prioritize snapping to the most
  // recent snap targets in each axis and prioritize one axis over the other
  // according to the following order:
  //  1. an axis with the focused area.
  //  2. an axis with the targeted [1] area.
  //  3. the block axis.
  //  (See step 8 at
  //   https://github.com/w3c/csswg-drafts/issues/9622#issue-2006578282)
  // [1]https://drafts.csswg.org/selectors/#the-target-pseudo
  // If we don't prioritize snapping to the most recent snap targets, we choose
  // the axis whose snap area is closer. Then find a new snap area on the other
  // axis that is mutually visible with the selected axis' snap area.
  if (strategy.ShouldPrioritizeSnapTargets()) {
    // If we we're previously snapped in one axis but not the other, follow the
    // axis we we're previously snapped in.
    if (target_snap_area_element_ids_.x == ElementId()) {
      return SnapAxis::kY;
    } else if (target_snap_area_element_ids_.y == ElementId()) {
      return SnapAxis::kX;
    }

    // Focused, then targeted snap areas should be followed.
    if (selected_x.has_focus_within()) {
      return SnapAxis::kX;
    } else if (selected_y.has_focus_within()) {
      return SnapAxis::kY;
    } else if (selected_x.element_id() == targeted_area_id_) {
      return SnapAxis::kX;
    } else if (selected_y.element_id() == targeted_area_id_) {
      return SnapAxis::kY;
    }

    // Follow the block axis target.
    return has_horizontal_writing_mode_ ? SnapAxis::kY : SnapAxis::kX;
  }
  return (
      std::abs(selected_x.snap_offset() - strategy.base_position().x()) <=
              std::abs(selected_y.snap_offset() - strategy.base_position().y())
          ? SnapAxis::kX
          : SnapAxis::kY);
}

std::ostream& operator<<(std::ostream& ostream, const SnapAreaData& area_data) {
  return ostream << area_data.rect.ToString();
}

std::ostream& operator<<(std::ostream& ostream,
                         const SnapContainerData& container_data) {
  ostream << "container_rect: " << container_data.rect().ToString();
  ostream << "area_rects: ";
  for (size_t i = 0; i < container_data.size(); ++i) {
    ostream << container_data.at(i) << "\n";
  }
  return ostream;
}

}  // namespace cc
