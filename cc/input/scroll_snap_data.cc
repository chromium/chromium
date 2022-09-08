// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/scroll_snap_data.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>

#include "base/check.h"
#include "base/cxx17_backports.h"
#include "base/notreached.h"
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
                       absl::optional<SnapSearchResult>* result,
                       const ElementId& active_element_id) {
  if (result->has_value()) {
    result->value().Union(candidate);
    if (candidate.element_id() == active_element_id)
      result->value().set_element_id(active_element_id);
  } else {
    *result = candidate;
  }
}

const absl::optional<SnapSearchResult>& ClosestSearchResult(
    const gfx::PointF reference_point,
    SearchAxis axis,
    const absl::optional<SnapSearchResult>& a,
    const absl::optional<SnapSearchResult>& b) {
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

}  // namespace

SnapSearchResult::SnapSearchResult(float offset, const gfx::RangeF& range)
    : snap_offset_(offset) {
  set_visible_range(range);
}

void SnapSearchResult::set_visible_range(const gfx::RangeF& range) {
  DCHECK(range.start() <= range.end());
  visible_range_ = range;
}

void SnapSearchResult::Clip(float max_snap, float max_visible) {
  snap_offset_ = base::clamp(snap_offset_, 0.0f, max_snap);
  visible_range_ =
      gfx::RangeF(base::clamp(visible_range_.start(), 0.0f, max_visible),
                  base::clamp(visible_range_.end(), 0.0f, max_visible));
}

void SnapSearchResult::Union(const SnapSearchResult& other) {
  DCHECK(snap_offset_ == other.snap_offset_);
  visible_range_ = gfx::RangeF(
      std::min(visible_range_.start(), other.visible_range_.start()),
      std::max(visible_range_.end(), other.visible_range_.end()));
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

bool SnapContainerData::FindSnapPosition(
    const SnapSelectionStrategy& strategy,
    gfx::PointF* snap_position,
    TargetSnapAreaElementIds* target_element_ids,
    const ElementId& active_element_id) const {
  *target_element_ids = TargetSnapAreaElementIds();
  if (scroll_snap_type_.is_none)
    return false;

  gfx::PointF base_position = strategy.base_position();
  SnapAxis axis = scroll_snap_type_.axis;
  bool should_snap_on_x = strategy.ShouldSnapOnX() &&
                          (axis == SnapAxis::kX || axis == SnapAxis::kBoth);
  bool should_snap_on_y = strategy.ShouldSnapOnY() &&
                          (axis == SnapAxis::kY || axis == SnapAxis::kBoth);
  if (!should_snap_on_x && !should_snap_on_y)
    return false;

  bool should_prioritize_x_target =
      strategy.ShouldPrioritizeSnapTargets() &&
      target_snap_area_element_ids_.x != ElementId();
  bool should_prioritize_y_target =
      strategy.ShouldPrioritizeSnapTargets() &&
      target_snap_area_element_ids_.y != ElementId();

  absl::optional<SnapSearchResult> selected_x, selected_y;
  if (should_snap_on_x) {
    if (should_prioritize_x_target) {
      // TODO(http://crbug.com/866127): If the target snap area is covering the
      // snapport then we should fallback to the default "closest-area" method
      // instead.
      selected_x = GetTargetSnapAreaSearchResult(SearchAxis::kX);
      DCHECK(selected_x.has_value());
    } else {
      // Start from current position in the cross axis. The search algorithm
      // expects the cross axis position to be inside scroller bounds. But since
      // we cannot always assume that the incoming value fits this criteria we
      // clamp it to the bounds to ensure this variant.
      SnapSearchResult initial_snap_position_y = {
          base::clamp(base_position.y(), 0.f, max_position_.y()),
          gfx::RangeF(0, max_position_.x())};
      selected_x = FindClosestValidArea(
          SearchAxis::kX, strategy, initial_snap_position_y, active_element_id);
    }
  }
  if (should_snap_on_y) {
    if (should_prioritize_y_target) {
      selected_y = GetTargetSnapAreaSearchResult(SearchAxis::kY);
      DCHECK(selected_y.has_value());
    } else {
      SnapSearchResult initial_snap_position_x = {
          base::clamp(base_position.x(), 0.f, max_position_.x()),
          gfx::RangeF(0, max_position_.y())};
      selected_y = FindClosestValidArea(
          SearchAxis::kY, strategy, initial_snap_position_x, active_element_id);
    }
  }

  if (!selected_x.has_value() && !selected_y.has_value()) {
    // Searching along each axis separately can miss valid snap positions if
    // snapping along both axes and the snap positions are off screen.
    if (should_snap_on_x && should_snap_on_y &&
        !strategy.ShouldRespectSnapStop())
      return FindSnapPositionForMutualSnap(strategy, snap_position);

    return false;
  }

  // If snapping in one axis pushes off-screen the other snap area, this snap
  // position is invalid. https://drafts.csswg.org/css-scroll-snap-1/#snap-scope
  // In this case, first check if we need to prioritize the snap area from
  // one axis over the other and select that axis, or if we don't prioritize an
  // axis over the other, we choose the axis whose snap area is closer.
  // Then find a new snap area on the other axis that is mutually visible with
  // the selected axis' snap area.
  if (selected_x.has_value() && selected_y.has_value() &&
      !IsMutualVisible(selected_x.value(), selected_y.value())) {
    bool keep_candidate_on_x = should_prioritize_x_target;
    if (should_prioritize_x_target == should_prioritize_y_target) {
      keep_candidate_on_x =
          std::abs(selected_x.value().snap_offset() - base_position.x()) <=
          std::abs(selected_y.value().snap_offset() - base_position.y());
    }
    if (keep_candidate_on_x) {
      selected_y = FindClosestValidArea(SearchAxis::kY, strategy,
                                        selected_x.value(), active_element_id);
    } else {
      selected_x = FindClosestValidArea(SearchAxis::kX, strategy,
                                        selected_y.value(), active_element_id);
    }
  }

  *snap_position = strategy.current_position();
  if (selected_x.has_value()) {
    snap_position->set_x(selected_x.value().snap_offset());
    target_element_ids->x = selected_x.value().element_id();
  }

  if (selected_y.has_value()) {
    snap_position->set_y(selected_y.value().snap_offset());
    target_element_ids->y = selected_y.value().element_id();
  }

  return true;
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
    gfx::Vector2dF distance = DistanceFromCorridor(dx, dy, rect_);
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

absl::optional<SnapSearchResult>
SnapContainerData::GetTargetSnapAreaSearchResult(SearchAxis axis) const {
  ElementId target_id = axis == SearchAxis::kX
                            ? target_snap_area_element_ids_.x
                            : target_snap_area_element_ids_.y;
  if (target_id == ElementId())
    return absl::nullopt;
  for (const SnapAreaData& area : snap_area_list_) {
    if (area.element_id == target_id)
      return GetSnapSearchResult(axis, area);
  }
  return absl::nullopt;
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

absl::optional<SnapSearchResult> SnapContainerData::FindClosestValidArea(
    SearchAxis axis,
    const SnapSelectionStrategy& strategy,
    const SnapSearchResult& cross_axis_snap_result,
    const ElementId& active_element_id) const {
  absl::optional<SnapSearchResult> result = FindClosestValidAreaInternal(
      axis, strategy, cross_axis_snap_result, active_element_id);

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
    absl::optional<SnapSearchResult> must_only_result =
        FindClosestValidAreaInternal(axis, *must_only_strategy,
                                     cross_axis_snap_result, active_element_id,
                                     false);
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
  return FindClosestValidAreaInternal(
      axis, *relaxed_strategy, cross_axis_snap_result, active_element_id);
}

absl::optional<SnapSearchResult>
SnapContainerData::FindClosestValidAreaInternal(
    SearchAxis axis,
    const SnapSelectionStrategy& strategy,
    const SnapSearchResult& cross_axis_snap_result,
    const ElementId& active_element_id,
    bool should_consider_covering) const {
  // The cross axis result is expected to be within bounds otherwise no snap
  // area will meet the mutual visibility requirement.
  DCHECK(cross_axis_snap_result.snap_offset() >= 0 &&
         cross_axis_snap_result.snap_offset() <=
             (axis == SearchAxis::kX ? max_position_.y() : max_position_.x()));

  // The search result from the snap area that's closest to the search origin.
  absl::optional<SnapSearchResult> closest;
  // The search result with the intended position if it makes a snap area cover
  // the snapport.
  absl::optional<SnapSearchResult> covering;

  // The valid snap positions immediately before and after the current position.
  float prev = std::numeric_limits<float>::lowest();
  float next = std::numeric_limits<float>::max();

  // The intended position of the scroll operation if there's no snap. This
  // scroll position becomes the covering candidate if there is a snap area that
  // fully covers the snapport if this position is scrolled to.
  float intended_position = axis == SearchAxis::kX
                                ? strategy.intended_position().x()
                                : strategy.intended_position().y();
  // The position from which we search for the closest snap position.
  float base_position = axis == SearchAxis::kX ? strategy.base_position().x()
                                               : strategy.base_position().y();

  float smallest_distance =
      axis == SearchAxis::kX ? proximity_range_.x() : proximity_range_.y();
  for (const SnapAreaData& area : snap_area_list_) {
    if (!strategy.IsValidSnapArea(axis, area))
      continue;

    SnapSearchResult candidate = GetSnapSearchResult(axis, area);
    if (should_consider_covering &&
        IsSnapportCoveredOnAxis(axis, intended_position, area.rect)) {
      // Since snap area will cover the snapport, we consider the intended
      // position as a valid snap position.
      SnapSearchResult covering_candidate = candidate;
      covering_candidate.set_snap_offset(intended_position);
      if (IsMutualVisible(covering_candidate, cross_axis_snap_result))
        SetOrUpdateResult(covering_candidate, &covering, active_element_id);

      // Even if a snap area covers the snapport, we need to continue this
      // search to find previous and next snap positions and also to have
      // alternative snap candidates if this covering candidate is ultimately
      // rejected. And this covering snap area has its own alignment that may
      // generates a snap position rejecting the current inplace candidate.
    }
    if (!IsMutualVisible(candidate, cross_axis_snap_result))
      continue;

    float distance = std::abs(candidate.snap_offset() - base_position);
    if (strategy.IsValidSnapPosition(axis, candidate.snap_offset())) {
      if (distance < smallest_distance ||
          (candidate.element_id() == active_element_id &&
           distance == smallest_distance)) {
        smallest_distance = distance;
        closest = candidate;
      }
    }
    if (!should_consider_covering)
      continue;

    if (candidate.snap_offset() < intended_position &&
        candidate.snap_offset() > prev) {
      prev = candidate.snap_offset();
    }
    if (candidate.snap_offset() > intended_position &&
        candidate.snap_offset() < next) {
      next = candidate.snap_offset();
    }
  }

  // According to the spec [1], if the snap area is covering the snapport, the
  // scroll position is a valid snap position only if the distance between the
  // geometrically previous and subsequent snap positions in that axis is larger
  // than size of the snapport in that axis.
  // [1] https://drafts.csswg.org/css-scroll-snap-1/#snap-overflow
  float size = axis == SearchAxis::kX ? rect_.width() : rect_.height();
  if (prev != std::numeric_limits<float>::lowest() &&
      next != std::numeric_limits<float>::max() && next - prev <= size) {
    covering = absl::nullopt;
  }

  const absl::optional<SnapSearchResult>& picked =
      strategy.PickBestResult(closest, covering);
  return picked;
}

SnapSearchResult SnapContainerData::GetSnapSearchResult(
    SearchAxis axis,
    const SnapAreaData& area) const {
  SnapSearchResult result;
  if (axis == SearchAxis::kX) {
    result.set_visible_range(gfx::RangeF(area.rect.y() - rect_.bottom(),
                                         area.rect.bottom() - rect_.y()));
    // https://www.w3.org/TR/css-scroll-snap-1/#scroll-snap-align
    // Snap alignment has been normalized for a horizontal left to right and top
    // to bottom writing mode.
    switch (area.scroll_snap_align.alignment_inline) {
      case SnapAlignment::kStart:
        result.set_snap_offset(area.rect.x() - rect_.x());
        break;
      case SnapAlignment::kCenter:
        result.set_snap_offset(area.rect.CenterPoint().x() -
                               rect_.CenterPoint().x());
        break;
      case SnapAlignment::kEnd:
        result.set_snap_offset(area.rect.right() - rect_.right());
        break;
      default:
        NOTREACHED();
    }
    result.Clip(max_position_.x(), max_position_.y());
  } else {
    result.set_visible_range(gfx::RangeF(area.rect.x() - rect_.right(),
                                         area.rect.right() - rect_.x()));
    switch (area.scroll_snap_align.alignment_block) {
      case SnapAlignment::kStart:
        result.set_snap_offset(area.rect.y() - rect_.y());
        break;
      case SnapAlignment::kCenter:
        result.set_snap_offset(area.rect.CenterPoint().y() -
                               rect_.CenterPoint().y());
        break;
      case SnapAlignment::kEnd:
        result.set_snap_offset(area.rect.bottom() - rect_.bottom());
        break;
      default:
        NOTREACHED();
    }
    result.Clip(max_position_.y(), max_position_.x());
  }
  result.set_element_id(area.element_id);
  return result;
}

bool SnapContainerData::IsSnapportCoveredOnAxis(
    SearchAxis axis,
    float current_offset,
    const gfx::RectF& area_rect) const {
  if (axis == SearchAxis::kX) {
    if (area_rect.width() < rect_.width())
      return false;
    float left = area_rect.x() - rect_.x();
    float right = area_rect.right() - rect_.right();
    return current_offset >= left && current_offset <= right;
  } else {
    if (area_rect.height() < rect_.height())
      return false;
    float top = area_rect.y() - rect_.y();
    float bottom = area_rect.bottom() - rect_.bottom();
    return current_offset >= top && current_offset <= bottom;
  }
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
