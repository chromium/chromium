// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/reverse_spiral_iterator.h"

#include <algorithm>

#include "base/check_op.h"

namespace cc {

ReverseSpiralIterator::ReverseSpiralIterator()
    : around_index_rect_(-1, -1, -1, -1),
      consider_index_rect_(-1, -1, -1, -1),
      ignore_index_rect_(-1, -1, -1, -1),
      index_x_(-1),
      index_y_(-1) {}

ReverseSpiralIterator::ReverseSpiralIterator(
    const IndexRect& around_index_rect,
    const IndexRect& consider_index_rect,
    const IndexRect& ignore_index_rect)
    : around_index_rect_(around_index_rect),
      consider_index_rect_(consider_index_rect),
      ignore_index_rect_(ignore_index_rect),
      index_x_(-1),
      index_y_(-1),
      direction_(LEFT),
      delta_x_(-1),
      delta_y_(0),
      current_step_(0),
      horizontal_step_count_(0),
      vertical_step_count_(0) {
  // Figure out the maximum distance from the around edge to consider edge.
  int max_distance = 0;
  max_distance = std::max(
      max_distance, around_index_rect_.top() - consider_index_rect_.top());
  max_distance = std::max(
      max_distance, around_index_rect_.left() - consider_index_rect_.left());
  max_distance = std::max(max_distance, consider_index_rect_.bottom() -
                                            around_index_rect_.bottom());
  max_distance = std::max(
      max_distance, consider_index_rect_.right() - around_index_rect_.right());

  // The step count is the length of the edge
  // (around_index_rect_.num_indices_x()) plus twice the max distance to pad
  // (to the right and to the left). This way the initial rect is the size
  // proportional to the center, but big enough to cover the consider rect.
  //
  // C = consider rect
  // A = around rect
  // . = area of the padded around rect
  // md = max distance (note in the picture below, there's md written vertically
  //      as well).
  // I = initial starting position
  //
  //       |md|  |md|
  //
  //     - ..........
  //     m ..........
  //     d ..........
  //     - CCCCCCC...
  //       CCCCAAC...
  //       CCCCAAC...
  //     - ..........
  //     m ..........
  //     d ..........
  //     - ..........I
  vertical_step_count_ = around_index_rect_.num_indices_y() + 2 * max_distance;
  horizontal_step_count_ =
      around_index_rect_.num_indices_x() + 2 * max_distance;

  // Start with one to the right of the padded around rect.
  index_x_ = around_index_rect_.right() + max_distance + 1;
  index_y_ = around_index_rect_.bottom() + max_distance;

  // The current index is outside a valid tile, so advance immediately.
  ++(*this);
}

ReverseSpiralIterator::operator bool() const {
  return index_x_ != -1 && index_y_ != -1;
}

ReverseSpiralIterator& ReverseSpiralIterator::operator++() {
  while (!around_index_rect_.Contains(index_x_, index_y_)) {
    if (needs_direction_switch())
      switch_direction();

    index_x_ += delta_x_;
    index_y_ += delta_y_;
    ++current_step_;

    if (around_index_rect_.Contains(index_x_, index_y_)) {
      break;
    } else if (consider_index_rect_.Contains(index_x_, index_y_)) {
      // If the tile is in the consider rect but not in ignore rect, then it's a
      // valid tile to visit.
      if (!ignore_index_rect_.Contains(index_x_, index_y_))
        break;

      // Steps needed to reach the very edge of the ignore rect, while remaining
      // inside it (so that the continue would take us outside).
      int steps_to_edge = 0;
      switch (direction_) {
        case UP:
          steps_to_edge = index_y_ - ignore_index_rect_.top();
          break;
        case LEFT:
          steps_to_edge = index_x_ - ignore_index_rect_.left();
          break;
        case DOWN:
          steps_to_edge = ignore_index_rect_.bottom() - index_y_;
          break;
        case RIGHT:
          steps_to_edge = ignore_index_rect_.right() - index_x_;
          break;
      }

      // We need to switch directions in |max_steps|.
      int max_steps = current_step_count() - current_step_;

      int steps_to_take = std::min(steps_to_edge, max_steps);
      DCHECK_GE(steps_to_take, 0);

      index_x_ += steps_to_take * delta_x_;
      index_y_ += steps_to_take * delta_y_;
      current_step_ += steps_to_take;
    } else {
      // We're not in the consider rect.

      int max_steps = current_step_count() - current_step_;
      int steps_to_take = max_steps;

      // We might hit the consider rect before needing to switch directions:
      // update steps to take.
      switch (direction_) {
        case UP:
          if (consider_index_rect_.valid_column(index_x_) &&
              consider_index_rect_.bottom() < index_y_)
            steps_to_take = index_y_ - consider_index_rect_.bottom() - 1;
          break;
        case LEFT:
          if (consider_index_rect_.valid_row(index_y_) &&
              consider_index_rect_.right() < index_x_)
            steps_to_take = index_x_ - consider_index_rect_.right() - 1;
          break;
        case DOWN:
          if (consider_index_rect_.valid_column(index_x_) &&
              consider_index_rect_.top() > index_y_)
            steps_to_take = consider_index_rect_.top() - index_y_ - 1;
          break;
        case RIGHT:
          if (consider_index_rect_.valid_row(index_y_) &&
              consider_index_rect_.left() > index_x_)
            steps_to_take = consider_index_rect_.left() - index_x_ - 1;
          break;
      }
      steps_to_take = std::min(steps_to_take, max_steps);
      DCHECK_GE(steps_to_take, 0);

      index_x_ += steps_to_take * delta_x_;
      index_y_ += steps_to_take * delta_y_;
      current_step_ += steps_to_take;
    }
  }

  // Once we enter the around rect, we're done.
  if (around_index_rect_.Contains(index_x_, index_y_)) {
    index_x_ = -1;
    index_y_ = -1;
  }

  return *this;
}

bool ReverseSpiralIterator::needs_direction_switch() const {
  return current_step_ >= current_step_count();
}

void ReverseSpiralIterator::switch_direction() {
  // Note that delta_x_ and delta_y_ always remain between -1 and 1.
  int new_delta_y = delta_x_;
  delta_x_ = -delta_y_;
  delta_y_ = new_delta_y;

  current_step_ = 0;
  direction_ = static_cast<Direction>((direction_ + 1) % 4);

  if (direction_ == UP || direction_ == DOWN) {
    --vertical_step_count_;
    --horizontal_step_count_;

    // We should always end up in an around rect at some point.
    // Since the direction is now vertical, we have to ensure that we will
    // advance.
    DCHECK_GE(horizontal_step_count_, 1);
    DCHECK_GE(vertical_step_count_, 1);
  }
}

}  // namespace cc
