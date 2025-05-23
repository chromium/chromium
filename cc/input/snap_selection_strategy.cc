// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/snap_selection_strategy.h"

#include <cmath>
#include <limits>

#include "ui/gfx/geometry/vector2d_f.h"

namespace cc {

std::unique_ptr<SnapSelectionStrategy>
SnapSelectionStrategy::CreateForEndPosition(const gfx::PointF& current_position,
                                            bool scrolled_x,
                                            bool scrolled_y) {
  return std::make_unique<EndPositionStrategy>(current_position, scrolled_x,
                                               scrolled_y);
}

std::unique_ptr<SnapSelectionStrategy>
SnapSelectionStrategy::CreateForDirection(gfx::PointF current_position,
                                          gfx::Vector2dF step,
                                          bool use_fractional_offsets,
                                          SnapStopAlwaysFilter filter) {
  return std::make_unique<DirectionStrategy>(
      current_position, step, DirectionStrategy::StepPreference::kDirection,
      gfx::Vector2dF(), gfx::Vector2dF(), filter, use_fractional_offsets);
}

std::unique_ptr<SnapSelectionStrategy>
SnapSelectionStrategy::CreateForDisplacement(gfx::PointF current_position,
                                             gfx::Vector2dF displacement,
                                             bool use_fractional_offsets,
                                             SnapStopAlwaysFilter filter) {
  return std::make_unique<DirectionStrategy>(
      current_position, displacement,
      DirectionStrategy::StepPreference::kDistance, gfx::Vector2dF(),
      gfx::Vector2dF(std::numeric_limits<float>::max(),
                     std::numeric_limits<float>::max()),
      filter, use_fractional_offsets);
}

std::unique_ptr<SnapSelectionStrategy>
SnapSelectionStrategy::CreateForPreferredDisplacement(
    gfx::PointF current_position,
    gfx::Vector2dF displacement,
    gfx::Vector2dF min_displacement,
    gfx::Vector2dF max_displacement,
    bool use_fractional_offsets,
    SnapStopAlwaysFilter filter) {
  return std::make_unique<DirectionStrategy>(
      current_position, displacement,
      DirectionStrategy::StepPreference::kDistance, min_displacement,
      max_displacement, filter, use_fractional_offsets);
}

std::unique_ptr<SnapSelectionStrategy>
SnapSelectionStrategy::CreateForTargetElement(gfx::PointF current_position) {
  return std::make_unique<EndPositionStrategy>(
      current_position, true /* scrolled_x */, true /* scrolled_y */,
      SnapTargetsPrioritization::kRequire);
}

bool SnapSelectionStrategy::HasIntendedDirection() const {
  return true;
}

bool SnapSelectionStrategy::ShouldRespectSnapStop() const {
  return false;
}

bool SnapSelectionStrategy::IsValidSnapArea(SearchAxis axis,
                                            const SnapAreaData& area) const {
  return axis == SearchAxis::kX
             ? area.scroll_snap_align.alignment_inline != SnapAlignment::kNone
             : area.scroll_snap_align.alignment_block != SnapAlignment::kNone;
}

bool SnapSelectionStrategy::ShouldPrioritizeSnapTargets() const {
  return false;
}

bool SnapSelectionStrategy::UsingFractionalOffsets() const {
  return false;
}

bool EndPositionStrategy::ShouldSnapOnX() const {
  return scrolled_x_;
}

bool EndPositionStrategy::ShouldSnapOnY() const {
  return scrolled_y_;
}

gfx::PointF EndPositionStrategy::intended_position() const {
  return current_position_;
}

gfx::PointF EndPositionStrategy::base_position() const {
  return current_position_;
}

bool EndPositionStrategy::IsPreferredSnapPosition(SearchAxis axis,
                                                  float position) const {
  return true;
}

// |position| is unused in this method.
bool EndPositionStrategy::IsValidSnapPosition(SearchAxis axis,
                                              float position) const {
  return (scrolled_x_ && axis == SearchAxis::kX) ||
         (scrolled_y_ && axis == SearchAxis::kY);
}

bool EndPositionStrategy::HasIntendedDirection() const {
  return false;
}

bool EndPositionStrategy::ShouldPrioritizeSnapTargets() const {
  return snap_targets_prioritization_ == SnapTargetsPrioritization::kRequire;
}

const std::optional<SnapSearchResult>& EndPositionStrategy::PickBestResult(
    const std::optional<SnapSearchResult>& closest,
    const std::optional<SnapSearchResult>& covering) const {
  return covering.has_value() ? covering : closest;
}

std::unique_ptr<SnapSelectionStrategy> EndPositionStrategy::Clone() const {
  return std::make_unique<EndPositionStrategy>(*this);
}

bool DirectionStrategy::ShouldSnapOnX() const {
  return step_.x() != 0;
}

bool DirectionStrategy::ShouldSnapOnY() const {
  return step_.y() != 0;
}

gfx::PointF DirectionStrategy::intended_position() const {
  return current_position_ + step_;
}

gfx::PointF DirectionStrategy::base_position() const {
  return preferred_step_ == StepPreference::kDirection
             ? current_position_
             : current_position_ + step_;
}

bool DirectionStrategy::IsPreferredSnapPosition(SearchAxis axis,
                                                float position) const {
  if (axis == SearchAxis::kX) {
    float delta = position - current_position_.x();
    return std::abs(delta) >= std::abs(preferred_min_displacement_.x()) &&
           std::abs(delta) <= std::abs(preferred_max_displacement_.x());
  } else {
    float delta = position - current_position_.y();
    return std::abs(delta) >= std::abs(preferred_min_displacement_.y()) &&
           std::abs(delta) <= std::abs(preferred_max_displacement_.y());
  }
}

bool DirectionStrategy::IsValidSnapPosition(SearchAxis axis,
                                            float position) const {
  // If not using fractional offsets then it is possible for the currently
  // snapped area's offset, which is fractional, to not be equal to the current
  // scroll offset, which is not fractional. Therefore we truncate the offsets
  // so that any position within 1 of the current position is ignored.
  if (axis == SearchAxis::kX) {
    float delta = position - current_position_.x();
    if (!use_fractional_offsets_)
      delta = delta > 0 ? std::floor(delta) : std::ceil(delta);
    return (step_.x() > 0 && delta > 0) ||  // "Right" arrow
           (step_.x() < 0 && delta < 0);    // "Left" arrow
  } else {
    float delta = position - current_position_.y();
    if (!use_fractional_offsets_)
      delta = delta > 0 ? std::floor(delta) : std::ceil(delta);
    return (step_.y() > 0 && delta > 0) ||  // "Down" arrow
           (step_.y() < 0 && delta < 0);    // "Up" arrow
  }
}

bool DirectionStrategy::IsValidSnapArea(SearchAxis axis,
                                        const SnapAreaData& area) const {
  return SnapSelectionStrategy::IsValidSnapArea(axis, area) &&
         (snap_stop_always_filter_ == SnapStopAlwaysFilter::kIgnore ||
          area.must_snap);
}

bool DirectionStrategy::ShouldRespectSnapStop() const {
  return true;
}

const std::optional<SnapSearchResult>& DirectionStrategy::PickBestResult(
    const std::optional<SnapSearchResult>& closest,
    const std::optional<SnapSearchResult>& covering) const {
  // We choose the |closest| result only if the default landing position (using
  // the default step) is not a valid snap position (not making a snap area
  // covering the snapport), or the |closest| is closer than the default landing
  // position.
  if (!closest.has_value())
    return covering;
  if (!covering.has_value())
    return closest;

  // If covering and closest represent the same snap area, covering best
  // preserves the intended scroll position.
  if (covering->element_id() == closest->element_id()) {
    return covering;
  }

  // If we only intend to scroll in the given direction, prefer the closer
  // snap position.
  if (preferred_step_ == StepPreference::kDirection) {
    // Scroll right or down.
    if ((step_.x() > 0 || step_.y() > 0) &&
        closest.value().snap_offset() < covering.value().snap_offset()) {
      return closest;
    }
    // Scroll left or up.
    if ((step_.x() < 0 || step_.y() < 0) &&
        closest.value().snap_offset() > covering.value().snap_offset()) {
      return closest;
    }
  }

  return covering;
}

bool DirectionStrategy::UsingFractionalOffsets() const {
  return use_fractional_offsets_;
}

std::unique_ptr<SnapSelectionStrategy> DirectionStrategy::Clone() const {
  return std::make_unique<DirectionStrategy>(*this);
}

}  // namespace cc
