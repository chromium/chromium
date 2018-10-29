// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/snap_selection_strategy.h"

namespace cc {

std::unique_ptr<SnapSelectionStrategy>
SnapSelectionStrategy::CreateForEndPosition(
    const gfx::ScrollOffset& current_position,
    bool scrolled_x,
    bool scrolled_y) {
  return std::make_unique<EndPositionStrategy>(current_position, scrolled_x,
                                               scrolled_y);
}

std::unique_ptr<SnapSelectionStrategy>
SnapSelectionStrategy::CreateForDirection(gfx::ScrollOffset current_position,
                                          gfx::ScrollOffset step) {
  return std::make_unique<DirectionStrategy>(current_position, step);
}

std::unique_ptr<SnapSelectionStrategy>
SnapSelectionStrategy::CreateForEndAndDirection(
    gfx::ScrollOffset current_position,
    gfx::ScrollOffset displacement) {
  return std::make_unique<EndAndDirectionStrategy>(current_position,
                                                   displacement);
}

bool EndPositionStrategy::ShouldSnapOnX() const {
  return scrolled_x_;
}

bool EndPositionStrategy::ShouldSnapOnY() const {
  return scrolled_y_;
}

gfx::ScrollOffset EndPositionStrategy::intended_position() const {
  return current_position_;
}

gfx::ScrollOffset EndPositionStrategy::base_position() const {
  return current_position_;
}

// |position| is unused in this method.
bool EndPositionStrategy::IsValidSnapPosition(SearchAxis axis,
                                              float position) const {
  return (scrolled_x_ && axis == SearchAxis::kX) ||
         (scrolled_y_ && axis == SearchAxis::kY);
}

const base::Optional<SnapSearchResult>& EndPositionStrategy::PickBestResult(
    const base::Optional<SnapSearchResult>& closest,
    const base::Optional<SnapSearchResult>& covering) const {
  return covering.has_value() ? covering : closest;
}

bool DirectionStrategy::ShouldSnapOnX() const {
  return step_.x() != 0;
}

bool DirectionStrategy::ShouldSnapOnY() const {
  return step_.y() != 0;
}

gfx::ScrollOffset DirectionStrategy::intended_position() const {
  return current_position_ + step_;
}

gfx::ScrollOffset DirectionStrategy::base_position() const {
  return current_position_;
}

bool DirectionStrategy::IsValidSnapPosition(SearchAxis axis,
                                            float position) const {
  if (axis == SearchAxis::kX) {
    return (step_.x() > 0 &&
            position > current_position_.x()) ||  // "Right" arrow
           (step_.x() < 0 && position < current_position_.x());  // "Left" arrow
  } else {
    return (step_.y() > 0 &&
            position > current_position_.y()) ||                 // "Down" arrow
           (step_.y() < 0 && position < current_position_.y());  // "Up" arrow
  }
}

const base::Optional<SnapSearchResult>& DirectionStrategy::PickBestResult(
    const base::Optional<SnapSearchResult>& closest,
    const base::Optional<SnapSearchResult>& covering) const {
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

bool EndAndDirectionStrategy::ShouldSnapOnX() const {
  return displacement_.x() != 0;
}

bool EndAndDirectionStrategy::ShouldSnapOnY() const {
  return displacement_.y() != 0;
}

gfx::ScrollOffset EndAndDirectionStrategy::intended_position() const {
  return current_position_ + displacement_;
}

gfx::ScrollOffset EndAndDirectionStrategy::base_position() const {
  return current_position_ + displacement_;
}

bool EndAndDirectionStrategy::IsValidSnapPosition(SearchAxis axis,
                                                  float position) const {
  if (axis == SearchAxis::kX) {
    return (displacement_.x() > 0 &&
            position > current_position_.x()) ||  // Right
           (displacement_.x() < 0 && position < current_position_.x());  // Left
  } else {
    return (displacement_.y() > 0 &&
            position > current_position_.y()) ||                         // Down
           (displacement_.y() < 0 && position < current_position_.y());  // Up
  }
}

const base::Optional<SnapSearchResult>& EndAndDirectionStrategy::PickBestResult(
    const base::Optional<SnapSearchResult>& closest,
    const base::Optional<SnapSearchResult>& covering) const {
  return covering.has_value() ? covering : closest;
}

}  // namespace cc
