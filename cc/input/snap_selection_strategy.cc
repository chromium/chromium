// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/snap_selection_strategy.h"

#include <cmath>

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
  return std::make_unique<DirectionStrategy>(current_position, step, filter,
                                             use_fractional_offsets);
}

std::unique_ptr<SnapSelectionStrategy>
SnapSelectionStrategy::CreateForEndAndDirection(gfx::PointF current_position,
                                                gfx::Vector2dF displacement,
                                                bool use_fractional_offsets) {
  return std::make_unique<EndAndDirectionStrategy>(
      current_position, displacement, use_fractional_offsets);
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
  return current_position_;
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

  // "Right" or "Down" arrow.
  if ((step_.x() > 0 || step_.y() > 0) &&
      closest.value().snap_offset() < covering.value().snap_offset()) {
    return closest;
  }
  // "Left" or "Up" arrow.
  if ((step_.x() < 0 || step_.y() < 0) &&
      closest.value().snap_offset() > covering.value().snap_offset()) {
    return closest;
  }

  return covering;
}

bool DirectionStrategy::UsingFractionalOffsets() const {
  return use_fractional_offsets_;
}

std::unique_ptr<SnapSelectionStrategy> DirectionStrategy::Clone() const {
  return std::make_unique<DirectionStrategy>(*this);
}

bool EndAndDirectionStrategy::ShouldSnapOnX() const {
  return displacement_.x() != 0;
}

bool EndAndDirectionStrategy::ShouldSnapOnY() const {
  return displacement_.y() != 0;
}

gfx::PointF EndAndDirectionStrategy::intended_position() const {
  return current_position_ + displacement_;
}

gfx::PointF EndAndDirectionStrategy::base_position() const {
  return current_position_ + displacement_;
}

bool EndAndDirectionStrategy::IsValidSnapPosition(SearchAxis axis,
                                                  float position) const {
  // If not using fractional offsets then it is possible for the currently
  // snapped area's offset, which is fractional, to not be equal to the current
  // scroll offset, which is not fractional. Therefore we round the offsets so
  // that any position within 0.5 of the current position is ignored.
  if (axis == SearchAxis::kX) {
    float delta = position - current_position_.x();
    if (!use_fractional_offsets_)
      delta = std::round(delta);
    return (displacement_.x() > 0 && delta > 0) ||  // Right
           (displacement_.x() < 0 && delta < 0);    // Left
  } else {
    float delta = position - current_position_.y();
    if (!use_fractional_offsets_)
      delta = std::round(delta);
    return (displacement_.y() > 0 && delta > 0) ||  // Down
           (displacement_.y() < 0 && delta < 0);    // Up
  }
}

bool EndAndDirectionStrategy::ShouldRespectSnapStop() const {
  return true;
}

const std::optional<SnapSearchResult>& EndAndDirectionStrategy::PickBestResult(
    const std::optional<SnapSearchResult>& closest,
    const std::optional<SnapSearchResult>& covering) const {
  return covering.has_value() ? covering : closest;
}

bool EndAndDirectionStrategy::UsingFractionalOffsets() const {
  return use_fractional_offsets_;
}

std::unique_ptr<SnapSelectionStrategy> EndAndDirectionStrategy::Clone() const {
  return std::make_unique<EndAndDirectionStrategy>(*this);
}

}  // namespace cc
