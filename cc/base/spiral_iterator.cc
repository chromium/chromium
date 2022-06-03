// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/spiral_iterator.h"

#include "base/check_op.h"

#include <algorithm>

namespace cc {

SpiralIterator::SpiralIterator()
    : around_index_rect_(-1, -1, -1, -1),
      consider_index_rect_(-1, -1, -1, -1),
      ignore_index_rect_(-1, -1, -1, -1),
      index_x_(-1),
      index_y_(-1) {}

SpiralIterator::SpiralIterator(const IndexRect& around_index_rect,
                               const IndexRect& consider_index_rect,
                               const IndexRect& ignore_index_rect)
    : around_index_rect_(around_index_rect),
      consider_index_rect_(consider_index_rect),
      ignore_index_rect_(ignore_index_rect),
      index_x_(-1),
      index_y_(-1),
      direction_(RIGHT),
      delta_x_(1),
      delta_y_(0),
      current_step_(0),
      horizontal_step_count_(0),
      vertical_step_count_(0) {
  vertical_step_count_ = around_index_rect_.num_indices_y();
  horizontal_step_count_ = around_index_rect_.num_indices_x();
  current_step_ = horizontal_step_count_ - 1;

  index_x_ = around_index_rect_.right();
  index_y_ = around_index_rect_.bottom();

  // The current index is the bottom right of the around rect, which is also
  // ignored. So we have to advance.
  ++(*this);
}

SpiralIterator::operator bool() const {
  return index_x_ != -1 && index_y_ != -1;
}

SpiralIterator& SpiralIterator::operator++() {
  int cannot_hit_consider_count = 0;
  while (cannot_hit_consider_count < 4) {
    if (needs_direction_switch())
      switch_direction();

    index_x_ += delta_x_;
    index_y_ += delta_y_;
    ++current_step_;

    if (consider_index_rect_.Contains(index_x_, index_y_)) {
      cannot_hit_consider_count = 0;

      if (!ignore_index_rect_.Contains(index_x_, index_y_))
        break;

      // Steps needed to reach the very edge of the ignore rect, while remaining
      // inside (so that the continue would take us outside).
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
      int max_steps = current_step_count() - current_step_;
      int steps_to_take = max_steps;
      bool can_hit_consider_rect = false;
      switch (direction_) {
        case UP:
          if (consider_index_rect_.valid_column(index_x_) &&
              consider_index_rect_.bottom() < index_y_)
            steps_to_take = index_y_ - consider_index_rect_.bottom() - 1;
          can_hit_consider_rect |= consider_index_rect_.right() >= index_x_;
          break;
        case LEFT:
          if (consider_index_rect_.valid_row(index_y_) &&
              consider_index_rect_.right() < index_x_)
            steps_to_take = index_x_ - consider_index_rect_.right() - 1;
          can_hit_consider_rect |= consider_index_rect_.top() <= index_y_;
          break;
        case DOWN:
          if (consider_index_rect_.valid_column(index_x_) &&
              consider_index_rect_.top() > index_y_)
            steps_to_take = consider_index_rect_.top() - index_y_ - 1;
          can_hit_consider_rect |= consider_index_rect_.left() <= index_x_;
          break;
        case RIGHT:
          if (consider_index_rect_.valid_row(index_y_) &&
              consider_index_rect_.left() > index_x_)
            steps_to_take = consider_index_rect_.left() - index_x_ - 1;
          can_hit_consider_rect |= consider_index_rect_.bottom() >= index_y_;
          break;
      }
      steps_to_take = std::min(steps_to_take, max_steps);
      DCHECK_GE(steps_to_take, 0);

      index_x_ += steps_to_take * delta_x_;
      index_y_ += steps_to_take * delta_y_;
      current_step_ += steps_to_take;

      if (can_hit_consider_rect)
        cannot_hit_consider_count = 0;
      else
        ++cannot_hit_consider_count;
    }
  }

  if (cannot_hit_consider_count >= 4) {
    index_x_ = -1;
    index_y_ = -1;
  }

  return *this;
}

bool SpiralIterator::needs_direction_switch() const {
  return current_step_ >= current_step_count();
}

void SpiralIterator::switch_direction() {
  // Note that delta_x_ and delta_y_ always remain between -1 and 1.
  int new_delta_x = delta_y_;
  delta_y_ = -delta_x_;
  delta_x_ = new_delta_x;

  current_step_ = 0;
  direction_ = static_cast<Direction>((direction_ + 1) % 4);

  if (direction_ == RIGHT || direction_ == LEFT) {
    ++vertical_step_count_;
    ++horizontal_step_count_;
  }
}

}  // namespace cc
