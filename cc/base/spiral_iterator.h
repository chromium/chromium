// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BASE_SPIRAL_ITERATOR_H_
#define CC_BASE_SPIRAL_ITERATOR_H_

#include "cc/base/base_export.h"
#include "cc/base/index_rect.h"

namespace cc {

// The spiral iterator which iterates based on directions around the center
// rect in the given region. If the center rect is at index (2, 2), spiral
// iterator gives following sequence on iterating.
//
//    x 0   1   2   3   4
//  y ┌───┬───┬───┬───┬───┐
//  0 │ 16│ 15│ 14│ 13│ 12│
//    ├───┼───┼───┼───┼───┤
//  1 │ 17│  4│  3│  2│ 11│
//    ├───┼───┼───┼───┼───┤
//  2 │ 18│  5│  *│  1│ 10│
//    ├───┼───┼───┼───┼───┤
//  3 │ 19│  6│  7│  8│  9│
//    ├───┼───┼───┼───┼───┤
//  4 │ 20│ 21│ 22│ 23│ 24│
//    └───┴───┴───┴───┴───┘
class CC_BASE_EXPORT SpiralIterator {
 public:
  SpiralIterator();
  SpiralIterator(const IndexRect& around_index_rect,
                 const IndexRect& consider_index_rect,
                 const IndexRect& ignore_index_rect);

  ~SpiralIterator() = default;

  operator bool() const;
  SpiralIterator& operator++();
  int index_x() const { return index_x_; }
  int index_y() const { return index_y_; }

 private:
  int current_step_count() const {
    return (direction_ == Direction::kUp || direction_ == Direction::kDown)
               ? vertical_step_count_
               : horizontal_step_count_;
  }

  bool needs_direction_switch() const;
  void switch_direction();

  enum class Direction { kUp, kLeft, kDown, kRight };

  IndexRect around_index_rect_;
  IndexRect consider_index_rect_;
  IndexRect ignore_index_rect_;
  int index_x_;
  int index_y_;

  Direction direction_;
  int delta_x_;
  int delta_y_;
  int current_step_;
  int horizontal_step_count_;
  int vertical_step_count_;
};

}  // namespace cc

#endif  // CC_BASE_SPIRAL_ITERATOR_H_
