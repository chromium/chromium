// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BASE_INDEX_RECT_H_
#define CC_BASE_INDEX_RECT_H_

#include <string>

#include "cc/base/base_export.h"

namespace cc {

// This class encapsulates the index boundaries for region on co-ordinate system
// (used for tiling). The delimiting boundaries |left_|, |right_|, |top_| and
// |bottom_| are basically leftmost, rightmost, topmost and bottommost indices
// of the region. These delimiters can span in any quadrants.
//
// If |left_| <= |right_| and |top_| <= |bottom_|, IndexRect is considered to
// hold valid indices and this can be checked using is_valid().
//
// If IndexRect is valid, it has a coverage of all the indices from |left_| to
// |right_| both inclusive and |top_| to |bottom_| both inclusive. So for
// |left_| == |right_|, num_indices_x() is 1, meaning |left_| and |right_| point
// to the same index.
//
// The following diagram shows how indices span in different quadrants and the
// positive quadrant. In the positive quadrant all indices are >= 0. The first
// index in this quadrant is (0, 0). The indices in positive quadrant represent
// the visible region and is_in_positive_quadrant() can be used to check whether
// all indices lie within this quadrant or not.
//
//              │
//              │
//  -ve index_x │  +ve index_x
//  -ve index_y │  -ve index_y
//              │
//  ────────────┼────────────
//              │
//  -ve index_x │  +ve index_x
//  +ve index_y │  +ve index_y
//              │
//              │  (+ve Quadrant)
//
// In the following example, region has |left_| = 0, |right_| = 4, |top_| = 0
// and |bottom_| = 4. Here x indices are 0, 1, 2, 3, 4 and y indices are
// 0, 1, 2, 3, 4.
//
//    x 0   1   2   3   4
//  y ┌───┬───┬───┬───┬───┐
//  0 │   │   │   │   │   │
//    ├───┼───┼───┼───┼───┤
//  1 │   │   │   │   │   │
//    ├───┼───┼───┼───┼───┤
//  2 │   │   │   │   │   │
//    ├───┼───┼───┼───┼───┤
//  3 │   │   │   │   │   │
//    ├───┼───┼───┼───┼───┤
//  4 │   │   │   │   │   │
//    └───┴───┴───┴───┴───┘
class CC_BASE_EXPORT IndexRect {
 public:
  constexpr IndexRect(int left, int right, int top, int bottom)
      : left_(left), right_(right), top_(top), bottom_(bottom) {}

  ~IndexRect() = default;

  constexpr int left() const { return left_; }
  constexpr int right() const { return right_; }
  constexpr int top() const { return top_; }
  constexpr int bottom() const { return bottom_; }

  // Returns the number of indices from left to right, including both.
  constexpr int num_indices_x() const { return right_ - left_ + 1; }
  // Returns the number of indices from top to bottom, including both.
  constexpr int num_indices_y() const { return bottom_ - top_ + 1; }

  // Returns true if the index rect has valid indices.
  constexpr bool is_valid() const { return left_ <= right_ && top_ <= bottom_; }

  // Returns true if the index rect has valid indices in positive quadrant.
  constexpr bool is_in_positive_quadrant() const {
    return is_valid() && left_ >= 0 && top_ >= 0;
  }

  // Returns true if the index identified by index_x is valid column.
  bool valid_column(int index_x) const {
    return index_x >= left() && index_x <= right();
  }
  // Returns true if the index identified by index_y is a valid row.
  bool valid_row(int index_y) const {
    return index_y >= top() && index_y <= bottom();
  }

  // Clamp indices to the given IndexRect indices. For non-intersecting rects,
  // it makes this index rect invalid.
  void ClampTo(const IndexRect& other);

  // Returns true if the given index identified by index_x and index_y falls
  // inside this index rectangle, including edge indices.
  bool Contains(int index_x, int index_y) const;

  std::string ToString() const;

 private:
  int left_;
  int right_;
  int top_;
  int bottom_;
};

inline bool operator==(const IndexRect& lhs, const IndexRect& rhs) {
  return lhs.left() == rhs.left() && lhs.right() == rhs.right() &&
         lhs.top() == rhs.top() && lhs.bottom() == rhs.bottom();
}

inline bool operator!=(const IndexRect& lhs, const IndexRect& rhs) {
  return !(lhs == rhs);
}

}  // namespace cc

#endif  // CC_BASE_INDEX_RECT_H_
