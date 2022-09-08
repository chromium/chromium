// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <algorithm>
#include <vector>

#include "cc/base/tiling_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

void TestSpiralIterate(int source_line_number,
                       const TilingData& tiling_data,
                       const gfx::Rect& consider,
                       const gfx::Rect& ignore,
                       const gfx::Rect& center,
                       const std::vector<std::pair<int, int>>& expected) {
  std::vector<std::pair<int, int>> actual_forward;
  for (TilingData::SpiralDifferenceIterator it(&tiling_data, consider, ignore,
                                               center);
       it; ++it) {
    actual_forward.push_back(it.index());
  }

  EXPECT_EQ(expected.size(), actual_forward.size()) << "error from line "
                                                    << source_line_number;
  for (size_t i = 0; i < std::min(expected.size(), actual_forward.size());
       ++i) {
    EXPECT_EQ(expected[i].first, actual_forward[i].first)
        << "i: " << i << " error from line: " << source_line_number;
    EXPECT_EQ(expected[i].second, actual_forward[i].second)
        << "i: " << i << " error from line: " << source_line_number;
  }

  std::vector<std::pair<int, int>> actual_reverse;
  for (TilingData::ReverseSpiralDifferenceIterator it(&tiling_data, consider,
                                                      ignore, center);
       it; ++it) {
    actual_reverse.push_back(it.index());
  }

  std::vector<std::pair<int, int>> reversed_expected = expected;
  std::reverse(reversed_expected.begin(), reversed_expected.end());
  EXPECT_EQ(reversed_expected.size(), actual_reverse.size())
      << "error from line " << source_line_number;
  for (size_t i = 0;
       i < std::min(reversed_expected.size(), actual_reverse.size()); ++i) {
    EXPECT_EQ(reversed_expected[i].first, actual_reverse[i].first)
        << "i: " << i << " error from line: " << source_line_number;
    EXPECT_EQ(reversed_expected[i].second, actual_reverse[i].second)
        << "i: " << i << " error from line: " << source_line_number;
  }
}

TEST(SpiralIteratorTest, NoIgnoreFullConsider) {
  TilingData tiling_data(gfx::Size(10, 10), gfx::Size(30, 30), false);
  gfx::Rect consider(30, 30);
  gfx::Rect ignore;
  std::vector<std::pair<int, int>> expected;

  // Center is in the center of the tiling.
  gfx::Rect center(15, 15, 1, 1);

  // Layout of the tiling data, and expected return order:
  //    x 0   1   2
  //  y ┌───┬───┬───┐
  //  0 │  4│  3│  2│
  //    ├───┼───┼───┤
  //  1 │  5│  *│  1│
  //    ├───┼───┼───┤
  //  2 │  6│  7│  8│
  //    └───┴───┴───┘
  expected.push_back(std::make_pair(2, 1));
  expected.push_back(std::make_pair(2, 0));
  expected.push_back(std::make_pair(1, 0));
  expected.push_back(std::make_pair(0, 0));
  expected.push_back(std::make_pair(0, 1));
  expected.push_back(std::make_pair(0, 2));
  expected.push_back(std::make_pair(1, 2));
  expected.push_back(std::make_pair(2, 2));

  TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center, expected);

  // Center is off to the right side of the tiling (and far away).
  center = gfx::Rect(100, 15, 1, 1);

  // Layout of the tiling data, and expected return order:
  //    x 0   1   2
  //  y ┌───┬───┬───┐
  //  0 │  7│  4│  1│
  //    ├───┼───┼───┤
  //  1 │  8│  5│  2│    *
  //    ├───┼───┼───┤
  //  2 │  9│  6│  3│
  //    └───┴───┴───┘
  expected.clear();
  expected.push_back(std::make_pair(2, 0));
  expected.push_back(std::make_pair(2, 1));
  expected.push_back(std::make_pair(2, 2));
  expected.push_back(std::make_pair(1, 0));
  expected.push_back(std::make_pair(1, 1));
  expected.push_back(std::make_pair(1, 2));
  expected.push_back(std::make_pair(0, 0));
  expected.push_back(std::make_pair(0, 1));
  expected.push_back(std::make_pair(0, 2));

  TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center, expected);

  // Center is the bottom right corner of the tiling.
  center = gfx::Rect(25, 25, 1, 1);

  // Layout of the tiling data, and expected return order:
  //    x 0   1   2
  //  y ┌───┬───┬───┐
  //  0 │  6│  5│  4│
  //    ├───┼───┼───┤
  //  1 │  7│  2│  1│
  //    ├───┼───┼───┤
  //  2 │  8│  3│  *│
  //    └───┴───┴───┘
  expected.clear();
  expected.push_back(std::make_pair(2, 1));
  expected.push_back(std::make_pair(1, 1));
  expected.push_back(std::make_pair(1, 2));
  expected.push_back(std::make_pair(2, 0));
  expected.push_back(std::make_pair(1, 0));
  expected.push_back(std::make_pair(0, 0));
  expected.push_back(std::make_pair(0, 1));
  expected.push_back(std::make_pair(0, 2));

  TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center, expected);

  // Center is off the top left side of the tiling.
  center = gfx::Rect(-60, -50, 1, 1);

  // Layout of the tiling data, and expected return order:
  //  * x 0   1   2
  //  y ┌───┬───┬───┐
  //  0 │  1│  2│  6│
  //    ├───┼───┼───┤
  //  1 │  3│  4│  5│
  //    ├───┼───┼───┤
  //  2 │  7│  8│  9│
  //    └───┴───┴───┘
  expected.clear();
  expected.push_back(std::make_pair(0, 0));
  expected.push_back(std::make_pair(1, 0));
  expected.push_back(std::make_pair(0, 1));
  expected.push_back(std::make_pair(1, 1));
  expected.push_back(std::make_pair(2, 1));
  expected.push_back(std::make_pair(2, 0));
  expected.push_back(std::make_pair(0, 2));
  expected.push_back(std::make_pair(1, 2));
  expected.push_back(std::make_pair(2, 2));

  TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center, expected);

  // Two tile center.
  center = gfx::Rect(15, 15, 1, 10);

  // Layout of the tiling data, and expected return order:
  //    x 0   1   2
  //  y ┌───┬───┬───┐
  //  0 │  5│  4│  3│
  //    ├───┼───┼───┤
  //  1 │  6│  *│  2│
  //    ├───┼───┼───┤
  //  2 │  7│  *│  1│
  //    └───┴───┴───┘
  expected.clear();
  expected.push_back(std::make_pair(2, 2));
  expected.push_back(std::make_pair(2, 1));
  expected.push_back(std::make_pair(2, 0));
  expected.push_back(std::make_pair(1, 0));
  expected.push_back(std::make_pair(0, 0));
  expected.push_back(std::make_pair(0, 1));
  expected.push_back(std::make_pair(0, 2));

  TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center, expected);
}

TEST(SpiralIteratorTest, SmallConsider) {
  TilingData tiling_data(gfx::Size(10, 10), gfx::Size(50, 50), false);
  gfx::Rect ignore;
  std::vector<std::pair<int, int>> expected;
  gfx::Rect center(15, 15, 1, 1);

  // Consider is one cell.
  gfx::Rect consider(1, 1);

  // Layout of the tiling data, and expected return order:
  //    x 0   1   2   3   4
  //  y ┌───┬───┬───┬───┬───┐
  //  0 │  1│   │   │   │   │
  //    ├───┼───┼───┼───┼───┤
  //  1 │   │  *│   │   │   │
  //    ├───┼───┼───┼───┼───┤
  //  2 │   │   │   │   │   │
  //    ├───┼───┼───┼───┼───┤
  //  3 │   │   │   │   │   │
  //    ├───┼───┼───┼───┼───┤
  //  4 │   │   │   │   │   │
  //    └───┴───┴───┴───┴───┘
  expected.push_back(std::make_pair(0, 0));

  TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center, expected);

  // Consider is bottom right corner.
  consider = gfx::Rect(25, 25, 10, 10);

  // Layout of the tiling data, and expected return order:
  //    x 0   1   2   3   4
  //  y ┌───┬───┬───┬───┬───┐
  //  0 │   │   │   │   │   │
  //    ├───┼───┼───┼───┼───┤
  //  1 │   │  *│   │   │   │
  //    ├───┼───┼───┼───┼───┤
  //  2 │   │   │  1│  2│   │
  //    ├───┼───┼───┼───┼───┤
  //  3 │   │   │  3│  4│   │
  //    ├───┼───┼───┼───┼───┤
  //  4 │   │   │   │   │   │
  //    └───┴───┴───┴───┴───┘
  expected.clear();
  expected.push_back(std::make_pair(2, 2));
  expected.push_back(std::make_pair(3, 2));
  expected.push_back(std::make_pair(2, 3));
  expected.push_back(std::make_pair(3, 3));

  TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center, expected);

  // Consider is one column.
  consider = gfx::Rect(11, 0, 1, 100);

  // Layout of the tiling data, and expected return order:
  //    x 0   1   2   3   4
  //  y ┌───┬───┬───┬───┬───┐
  //  0 │   │  2│   │   │   │
  //    ├───┼───┼───┼───┼───┤
  //  1 │   │  *│   │   │   │
  //    ├───┼───┼───┼───┼───┤
  //  2 │   │  3│   │   │   │
  //    ├───┼───┼───┼───┼───┤
  //  3 │   │  4│   │   │   │
  //    ├───┼───┼───┼───┼───┤
  //  4 │   │  5│   │   │   │
  //    └───┴───┴───┴───┴───┘
  expected.clear();
  expected.push_back(std::make_pair(1, 0));
  expected.push_back(std::make_pair(1, 2));
  expected.push_back(std::make_pair(1, 3));
  expected.push_back(std::make_pair(1, 4));

  TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center, expected);
}

TEST(SpiralIteratorTest, HasIgnore) {
  TilingData tiling_data(gfx::Size(10, 10), gfx::Size(50, 50), false);
  gfx::Rect consider(50, 50);
  std::vector<std::pair<int, int>> expected;
  gfx::Rect center(15, 15, 1, 1);

  // Full ignore.
  gfx::Rect ignore(50, 50);

  // Layout of the tiling data, and expected return order:
  //    x 0   1   2   3   4
  //  y ┌───┬───┬───┬───┬───┐
  //  0 │  I│  I│  I│  I│  I│
  //    ├───┼───┼───┼───┼───┤
  //  1 │  I│  *│  I│  I│  I│
  //    ├───┼───┼───┼───┼───┤
  //  2 │  I│  I│  I│  I│  I│
  //    ├───┼───┼───┼───┼───┤
  //  3 │  I│  I│  I│  I│  I│
  //    ├───┼───┼───┼───┼───┤
  //  4 │  I│  I│  I│  I│  I│
  //    └───┴───┴───┴───┴───┘
  expected.clear();

  TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center, expected);

  // 3 column ignore.
  ignore = gfx::Rect(15, 0, 20, 100);

  // Layout of the tiling data, and expected return order:
  //    x 0   1   2   3   4
  //  y ┌───┬───┬───┬───┬───┐
  //  0 │  1│  I│  I│  I│  8│
  //    ├───┼───┼───┼───┼───┤
  //  1 │  2│  *│  I│  I│  7│
  //    ├───┼───┼───┼───┼───┤
  //  2 │  3│  I│  I│  I│  6│
  //    ├───┼───┼───┼───┼───┤
  //  3 │  4│  I│  I│  I│  5│
  //    ├───┼───┼───┼───┼───┤
  //  4 │  9│  I│  I│  I│ 10│
  //    └───┴───┴───┴───┴───┘
  expected.clear();
  expected.push_back(std::make_pair(0, 0));
  expected.push_back(std::make_pair(0, 1));
  expected.push_back(std::make_pair(0, 2));
  expected.push_back(std::make_pair(0, 3));
  expected.push_back(std::make_pair(4, 3));
  expected.push_back(std::make_pair(4, 2));
  expected.push_back(std::make_pair(4, 1));
  expected.push_back(std::make_pair(4, 0));
  expected.push_back(std::make_pair(0, 4));
  expected.push_back(std::make_pair(4, 4));

  TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center, expected);

  // Ignore covers the top half.
  ignore = gfx::Rect(50, 25);

  // Layout of the tiling data, and expected return order:
  //    x 0   1   2   3   4
  //  y ┌───┬───┬───┬───┬───┐
  //  0 │  I│  I│  I│  I│  I│
  //    ├───┼───┼───┼───┼───┤
  //  1 │  I│  *│  I│  I│  I│
  //    ├───┼───┼───┼───┼───┤
  //  2 │  I│  I│  I│  I│  I│
  //    ├───┼───┼───┼───┼───┤
  //  3 │  1│  2│  3│  4│  5│
  //    ├───┼───┼───┼───┼───┤
  //  4 │  6│  7│  8│  9│ 10│
  //    └───┴───┴───┴───┴───┘
  expected.clear();
  expected.push_back(std::make_pair(0, 3));
  expected.push_back(std::make_pair(1, 3));
  expected.push_back(std::make_pair(2, 3));
  expected.push_back(std::make_pair(3, 3));
  expected.push_back(std::make_pair(4, 3));
  expected.push_back(std::make_pair(0, 4));
  expected.push_back(std::make_pair(1, 4));
  expected.push_back(std::make_pair(2, 4));
  expected.push_back(std::make_pair(3, 4));
  expected.push_back(std::make_pair(4, 4));

  TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center, expected);
}

TEST(SpiralIteratorTest, RectangleCenter) {
  TilingData tiling_data(gfx::Size(10, 10), gfx::Size(50, 50), false);
  gfx::Rect consider(50, 50);
  std::vector<std::pair<int, int>> expected;
  gfx::Rect ignore;

  // Two cell center
  gfx::Rect center(25, 25, 1, 10);

  // Layout of the tiling data, and expected return order:
  //    x 0   1   2   3   4
  //  y ┌───┬───┬───┬───┬───┐
  //  0 │ 19│ 18│ 17│ 16│ 15│
  //    ├───┼───┼───┼───┼───┤
  //  1 │ 20│  5│  4│  3│ 14│
  //    ├───┼───┼───┼───┼───┤
  //  2 │ 21│  6│  *│  2│ 13│
  //    ├───┼───┼───┼───┼───┤
  //  3 │ 22│  7│  *│  1│ 12│
  //    ├───┼───┼───┼───┼───┤
  //  4 │ 23│  8│  9│ 10│ 11│
  //    └───┴───┴───┴───┴───┘
  expected.clear();
  expected.push_back(std::make_pair(3, 3));
  expected.push_back(std::make_pair(3, 2));
  expected.push_back(std::make_pair(3, 1));
  expected.push_back(std::make_pair(2, 1));
  expected.push_back(std::make_pair(1, 1));
  expected.push_back(std::make_pair(1, 2));
  expected.push_back(std::make_pair(1, 3));
  expected.push_back(std::make_pair(1, 4));
  expected.push_back(std::make_pair(2, 4));
  expected.push_back(std::make_pair(3, 4));
  expected.push_back(std::make_pair(4, 4));
  expected.push_back(std::make_pair(4, 3));
  expected.push_back(std::make_pair(4, 2));
  expected.push_back(std::make_pair(4, 1));
  expected.push_back(std::make_pair(4, 0));
  expected.push_back(std::make_pair(3, 0));
  expected.push_back(std::make_pair(2, 0));
  expected.push_back(std::make_pair(1, 0));
  expected.push_back(std::make_pair(0, 0));
  expected.push_back(std::make_pair(0, 1));
  expected.push_back(std::make_pair(0, 2));
  expected.push_back(std::make_pair(0, 3));
  expected.push_back(std::make_pair(0, 4));

  TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center, expected);

  // Three by two center.
  center = gfx::Rect(15, 25, 20, 10);

  // Layout of the tiling data, and expected return order:
  //    x 0   1   2   3   4
  //  y ┌───┬───┬───┬───┬───┐
  //  0 │ 19│ 18│ 17│ 16│ 15│
  //    ├───┼───┼───┼───┼───┤
  //  1 │  7│  6│  5│  4│  3│
  //    ├───┼───┼───┼───┼───┤
  //  2 │  8│  *│  *│  *│  2│
  //    ├───┼───┼───┼───┼───┤
  //  3 │  9│  *│  *│  *│  1│
  //    ├───┼───┼───┼───┼───┤
  //  4 │ 10│ 11│ 12│ 13│ 14│
  //    └───┴───┴───┴───┴───┘
  expected.clear();
  expected.push_back(std::make_pair(4, 3));
  expected.push_back(std::make_pair(4, 2));
  expected.push_back(std::make_pair(4, 1));
  expected.push_back(std::make_pair(3, 1));
  expected.push_back(std::make_pair(2, 1));
  expected.push_back(std::make_pair(1, 1));
  expected.push_back(std::make_pair(0, 1));
  expected.push_back(std::make_pair(0, 2));
  expected.push_back(std::make_pair(0, 3));
  expected.push_back(std::make_pair(0, 4));
  expected.push_back(std::make_pair(1, 4));
  expected.push_back(std::make_pair(2, 4));
  expected.push_back(std::make_pair(3, 4));
  expected.push_back(std::make_pair(4, 4));
  expected.push_back(std::make_pair(4, 0));
  expected.push_back(std::make_pair(3, 0));
  expected.push_back(std::make_pair(2, 0));
  expected.push_back(std::make_pair(1, 0));
  expected.push_back(std::make_pair(0, 0));

  TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center, expected);

  // Column center off the left side.
  center = gfx::Rect(-50, 0, 30, 50);

  // Layout of the tiling data, and expected return order:
  //     x 0   1   2   3   4
  //   y ┌───┬───┬───┬───┬───┐
  // * 0 │  5│ 10│ 15│ 20│ 25│
  //     ├───┼───┼───┼───┼───┤
  // * 1 │  4│  9│ 14│ 19│ 24│
  //     ├───┼───┼───┼───┼───┤
  // * 2 │  3│  8│ 13│ 18│ 23│
  //     ├───┼───┼───┼───┼───┤
  // * 3 │  2│  7│ 12│ 17│ 22│
  //     ├───┼───┼───┼───┼───┤
  // * 4 │  1│  6│ 11│ 16│ 21│
  //     └───┴───┴───┴───┴───┘
  expected.clear();
  expected.push_back(std::make_pair(0, 4));
  expected.push_back(std::make_pair(0, 3));
  expected.push_back(std::make_pair(0, 2));
  expected.push_back(std::make_pair(0, 1));
  expected.push_back(std::make_pair(0, 0));
  expected.push_back(std::make_pair(1, 4));
  expected.push_back(std::make_pair(1, 3));
  expected.push_back(std::make_pair(1, 2));
  expected.push_back(std::make_pair(1, 1));
  expected.push_back(std::make_pair(1, 0));
  expected.push_back(std::make_pair(2, 4));
  expected.push_back(std::make_pair(2, 3));
  expected.push_back(std::make_pair(2, 2));
  expected.push_back(std::make_pair(2, 1));
  expected.push_back(std::make_pair(2, 0));
  expected.push_back(std::make_pair(3, 4));
  expected.push_back(std::make_pair(3, 3));
  expected.push_back(std::make_pair(3, 2));
  expected.push_back(std::make_pair(3, 1));
  expected.push_back(std::make_pair(3, 0));
  expected.push_back(std::make_pair(4, 4));
  expected.push_back(std::make_pair(4, 3));
  expected.push_back(std::make_pair(4, 2));
  expected.push_back(std::make_pair(4, 1));
  expected.push_back(std::make_pair(4, 0));

  TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center, expected);
}

TEST(SpiralIteratorTest, EdgeCases) {
  TilingData tiling_data(gfx::Size(10, 10), gfx::Size(30, 30), false);
  std::vector<std::pair<int, int>> expected;
  gfx::Rect center;
  gfx::Rect consider;
  gfx::Rect ignore;

  // Ignore contains, but is not equal to, consider and center.
  ignore = gfx::Rect(15, 0, 20, 30);
  consider = gfx::Rect(20, 10, 10, 20);
  center = gfx::Rect(25, 0, 5, 5);

  // Layout of the tiling data, and expected return order:
  //    x 0   1   2
  //  y ┌───┬───┬───┐
  //  0 │   │  I│  *│
  //    ├───┼───┼───┤
  //  1 │   │  I│  I│
  //    ├───┼───┼───┤
  //  2 │   │  I│  I│
  //    └───┴───┴───┘
  expected.clear();

  TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center, expected);

  // Center intersects with consider.
  ignore = gfx::Rect();
  center = gfx::Rect(0, 15, 30, 15);
  consider = gfx::Rect(15, 30);

  // Layout of the tiling data, and expected return order:
  //    x 0   1   2
  //  y ┌───┬───┬───┐
  //  0 │  2│  1│   │
  //    ├───┼───┼───┤
  //  1 │  *│  *│  *│
  //    ├───┼───┼───┤
  //  2 │  *│  *│  *│
  //    └───┴───┴───┘
  expected.clear();
  expected.push_back(std::make_pair(1, 0));
  expected.push_back(std::make_pair(0, 0));

  TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center, expected);

  // Consider and ignore are non-intersecting.
  ignore = gfx::Rect(5, 30);
  consider = gfx::Rect(25, 0, 5, 30);
  center = gfx::Rect(15, 0, 1, 1);

  // Layout of the tiling data, and expected return order:
  //    x 0   1   2
  //  y ┌───┬───┬───┐
  //  0 │  I│  *│  1│
  //    ├───┼───┼───┤
  //  1 │  I│   │  2│
  //    ├───┼───┼───┤
  //  2 │  I│   │  3│
  //    └───┴───┴───┘
  expected.clear();
  expected.push_back(std::make_pair(2, 0));
  expected.push_back(std::make_pair(2, 1));
  expected.push_back(std::make_pair(2, 2));

  TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center, expected);

  // Center intersects with ignore.
  consider = gfx::Rect(30, 30);
  center = gfx::Rect(15, 0, 1, 30);
  ignore = gfx::Rect(0, 15, 30, 1);

  // Layout of the tiling data, and expected return order:
  //    x 0   1   2
  //  y ┌───┬───┬───┐
  //  0 │  3│  *│  2│
  //    ├───┼───┼───┤
  //  1 │  I│  *│  I│
  //    ├───┼───┼───┤
  //  2 │  4│  *│  1│
  //    └───┴───┴───┘
  expected.clear();
  expected.push_back(std::make_pair(2, 2));
  expected.push_back(std::make_pair(2, 0));
  expected.push_back(std::make_pair(0, 0));
  expected.push_back(std::make_pair(0, 2));

  TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center, expected);

  // Center and ignore are the same.
  consider = gfx::Rect(30, 30);
  center = gfx::Rect(15, 0, 1, 30);
  ignore = center;

  // Layout of the tiling data, and expected return order:
  //    x 0   1   2
  //  y ┌───┬───┬───┐
  //  0 │  4│  *│  3│
  //    ├───┼───┼───┤
  //  1 │  5│  *│  2│
  //    ├───┼───┼───┤
  //  2 │  6│  *│  1│
  //    └───┴───┴───┘
  expected.clear();
  expected.push_back(std::make_pair(2, 2));
  expected.push_back(std::make_pair(2, 1));
  expected.push_back(std::make_pair(2, 0));
  expected.push_back(std::make_pair(0, 0));
  expected.push_back(std::make_pair(0, 1));
  expected.push_back(std::make_pair(0, 2));

  TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center, expected);

  // Empty tiling data.
  TilingData empty_data(gfx::Size(0, 0), gfx::Size(0, 0), false);

  expected.clear();

  TestSpiralIterate(__LINE__, empty_data, consider, ignore, center, expected);

  // Empty consider.
  ignore = gfx::Rect();
  center = gfx::Rect(1, 1, 1, 1);
  consider = gfx::Rect();

  expected.clear();

  TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center, expected);

  // Empty center. Note: This arbitrarily puts the center to be off the top-left
  // corner.
  consider = gfx::Rect(30, 30);
  ignore = gfx::Rect();
  center = gfx::Rect();

  // Layout of the tiling data, and expected return order:
  //    x 0   1   2
  //  y ┌───┬───┬───┐
  //  0 │  1│  2│  6│
  //    ├───┼───┼───┤
  //  1 │  3│  4│  5│
  //    ├───┼───┼───┤
  //  2 │  7│  8│  9│
  //    └───┴───┴───┘
  expected.clear();
  expected.push_back(std::make_pair(0, 0));
  expected.push_back(std::make_pair(1, 0));
  expected.push_back(std::make_pair(0, 1));
  expected.push_back(std::make_pair(1, 1));
  expected.push_back(std::make_pair(2, 1));
  expected.push_back(std::make_pair(2, 0));
  expected.push_back(std::make_pair(0, 2));
  expected.push_back(std::make_pair(1, 2));
  expected.push_back(std::make_pair(2, 2));

  TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center, expected);

  // Every rect is empty.
  ignore = gfx::Rect();
  center = gfx::Rect();
  consider = gfx::Rect();

  expected.clear();

  TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center, expected);

  // Center is just to the left of cover, and off of the tiling's left side.
  consider = gfx::Rect(30, 30);
  ignore = gfx::Rect();
  center = gfx::Rect(-20, 0, 19, 30);

  // Layout of the tiling data, and expected return order:
  //     x 0   1   2
  //   y ┌───┬───┬───┐
  // * 0 │  3│  6│  9│
  //     ├───┼───┼───┤
  // * 1 │  2│  5│  8│
  //     ├───┼───┼───┤
  // * 2 │  1│  4│  7│
  //     └───┴───┴───┘
  expected.clear();
  expected.push_back(std::make_pair(0, 2));
  expected.push_back(std::make_pair(0, 1));
  expected.push_back(std::make_pair(0, 0));
  expected.push_back(std::make_pair(1, 2));
  expected.push_back(std::make_pair(1, 1));
  expected.push_back(std::make_pair(1, 0));
  expected.push_back(std::make_pair(2, 2));
  expected.push_back(std::make_pair(2, 1));
  expected.push_back(std::make_pair(2, 0));

  TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center, expected);

  // Tiling is smaller than tile size and center rect is not intersecting to
  // tiling rect.
  TilingData smaller_tiling(gfx::Size(10, 10), gfx::Size(1, 1), false);
  consider = gfx::Rect(10, 10);
  ignore = gfx::Rect();
  center = gfx::Rect(2, 2, 10, 10);

  // Layout of the tiling data, and expected return order:
  //    x   0
  //  y ┌───────┐
  //    │  1    │
  //  0 │       │
  //    │     * │
  //    └───────┘
  expected.clear();
  expected.push_back(std::make_pair(0, 0));

  TestSpiralIterate(__LINE__, smaller_tiling, consider, ignore, center,
                    expected);
}

}  // namespace

}  // namespace cc
