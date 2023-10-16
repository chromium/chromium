// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BASE_REVERSE_SPIRAL_ITERATOR_H_
#define CC_BASE_REVERSE_SPIRAL_ITERATOR_H_

#include "cc/base/base_export.h"
#include "cc/base/index_rect.h"

namespace cc {

// The spiral iterator which iterates in reverse direction based on directions
// around the center rect in the given region. If the center rect is at index
// (2, 2), reverse spiral iterator gives following sequence on iterating.
//
//    x 0   1   2   3   4
//  y ┌───┬───┬───┬───┬───┐
//  0 │  9│ 10│ 11│ 12│ 13│
//    ├───┼───┼───┼───┼───┤
//  1 │  8│ 21│ 22│ 23│ 14│
//    ├───┼───┼───┼───┼───┤
//  2 │  7│ 20│  *│ 24│ 15│
//    ├───┼───┼───┼───┼───┤
//  3 │  6│ 19│ 18│ 17│ 16│
//    ├───┼───┼───┼───┼───┤
//  4 │  5│  4│  3│  2│  1│
//    └───┴───┴───┴───┴───┘
class CC_BASE_EXPORT ReverseSpiralIterator {
 public:
  ReverseSpiralIterator();
  ReverseSpiralIterator(const IndexRect& around_index_rect,
                        const IndexRect& consider_index_rect,
                        const IndexRect& ignore_index_rect);

  ~ReverseSpiralIterator() = default;

  ReverseSpiralIterator& operator=(ReverseSpiralIterator&& other) = default;

  operator bool() const;
  ReverseSpiralIterator& operator++();
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

  enum class Direction { kLeft, kUp, kRight, kDown };

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

#endif  // CC_BASE_REVERSE_SPIRAL_ITERATOR_H_
