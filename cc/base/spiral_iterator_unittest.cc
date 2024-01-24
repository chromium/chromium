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

  EXPECT_EQ(expected, actual_forward)
      << "Error from line " << source_line_number;

  std::vector<std::pair<int, int>> actual_reverse;
  for (TilingData::ReverseSpiralDifferenceIterator it(&tiling_data, consider,
                                                      ignore, center);
       it; ++it) {
    actual_reverse.push_back(it.index());
  }

  std::vector<std::pair<int, int>> reversed_expected = expected;
  std::reverse(reversed_expected.begin(), reversed_expected.end());
  EXPECT_EQ(reversed_expected, actual_reverse)
      << "Error from line " << source_line_number;
}

class SpiralIteratorTest : public ::testing::TestWithParam<gfx::Vector2d> {
 public:
  static gfx::Vector2d offset() { return GetParam(); }
  static int OffsetX(int x) { return x + offset().x(); }
  static int OffsetY(int y) { return y + offset().y(); }
  static gfx::Rect OffsetRect(int w, int h) { return OffsetRect(0, 0, w, h); }
  static gfx::Rect OffsetRect(int x, int y, int w, int h) {
    return gfx::Rect(OffsetX(x), OffsetY(y), w, h);
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         SpiralIteratorTest,
                         ::testing::Values(gfx::Vector2d(0, 0),
                                           gfx::Vector2d(50, 100),
                                           gfx::Vector2d(17, 31),
                                           gfx::Vector2d(10000, 15000)));

TEST_P(SpiralIteratorTest, NoIgnoreFullConsider) {
  TilingData tiling_data(gfx::Size(10, 10), OffsetRect(30, 30), 0);
  gfx::Rect consider = OffsetRect(30, 30);
  gfx::Rect ignore;
  std::vector<std::pair<int, int>> expected;

  // Center is in the center of the tiling.
  gfx::Rect center = OffsetRect(15, 15, 1, 1);
  /*
    // Layout of the tiling data, and expected return order:
    //    x 0   1   2
    //  y ┌───┬───┬───┐
    //  0 │  4│  3│  2│
    //    ├───┼───┼───┤
    //  1 │  5│  *│  1│
    //    ├───┼───┼───┤
    //  2 │  6│  7│  8│
    //    └───┴───┴───┘
    expected = {{2, 1}, {2, 0}, {1, 0}, {0, 0}, {0, 1}, {0, 2}, {1, 2}, {2, 2}};
    TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center,
    expected);

    // Center is off to the right side of the tiling (and far away).
    center = OffsetRect(100, 15, 1, 1);

    // Layout of the tiling data, and expected return order:
    //    x 0   1   2
    //  y ┌───┬───┬───┐
    //  0 │  7│  4│  1│
    //    ├───┼───┼───┤
    //  1 │  8│  5│  2│    *
    //    ├───┼───┼───┤
    //  2 │  9│  6│  3│
    //    └───┴───┴───┘
    expected = {{2, 0}, {2, 1}, {2, 2}, {1, 0}, {1, 1},
                {1, 2}, {0, 0}, {0, 1}, {0, 2}};
    TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center,
    expected);

    // Center is the bottom right corner of the tiling.
    center = OffsetRect(25, 25, 1, 1);

    // Layout of the tiling data, and expected return order:
    //    x 0   1   2
    //  y ┌───┬───┬───┐
    //  0 │  6│  5│  4│
    //    ├───┼───┼───┤
    //  1 │  7│  2│  1│
    //    ├───┼───┼───┤
    //  2 │  8│  3│  *│
    //    └───┴───┴───┘
    expected = {{2, 1}, {1, 1}, {1, 2}, {2, 0}, {1, 0}, {0, 0}, {0, 1}, {0, 2}};
    TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center,
    expected);
  */
  // Center is off the top left side of the tiling.
  center = OffsetRect(-60, -50, 1, 1);

  // Layout of the tiling data, and expected return order:
  //  * x 0   1   2
  //  y ┌───┬───┬───┐
  //  0 │  1│  2│  6│
  //    ├───┼───┼───┤
  //  1 │  3│  4│  5│
  //    ├───┼───┼───┤
  //  2 │  7│  8│  9│
  //    └───┴───┴───┘
  expected = {{0, 0}, {1, 0}, {0, 1}, {1, 1}, {2, 1},
              {2, 0}, {0, 2}, {1, 2}, {2, 2}};
  TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center, expected);
  /*
    // Two tile center.
    center = OffsetRect(15, 15, 1, 10);

    // Layout of the tiling data, and expected return order:
    //    x 0   1   2
    //  y ┌───┬───┬───┐
    //  0 │  5│  4│  3│
    //    ├───┼───┼───┤
    //  1 │  6│  *│  2│
    //    ├───┼───┼───┤
    //  2 │  7│  *│  1│
    //    └───┴───┴───┘
    expected = {{2, 2}, {2, 1}, {2, 0}, {1, 0}, {0, 0}, {0, 1}, {0, 2}};
    TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center,
    expected);
  */
}

TEST_P(SpiralIteratorTest, SmallConsider) {
  TilingData tiling_data(gfx::Size(10, 10), OffsetRect(50, 50), 0);
  gfx::Rect ignore;
  std::vector<std::pair<int, int>> expected;
  gfx::Rect center = OffsetRect(15, 15, 1, 1);

  // Consider is one cell.
  gfx::Rect consider = OffsetRect(1, 1);

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
  expected = {{0, 0}};
  TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center, expected);

  // Consider is bottom right corner.
  consider = OffsetRect(25, 25, 10, 10);

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
  expected = {{2, 2}, {3, 2}, {2, 3}, {3, 3}};
  TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center, expected);

  // Consider is one column.
  consider = OffsetRect(11, 0, 1, 100);

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
  expected = {{1, 0}, {1, 2}, {1, 3}, {1, 4}};
  TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center, expected);
}

TEST_P(SpiralIteratorTest, HasIgnore) {
  TilingData tiling_data(gfx::Size(10, 10), OffsetRect(50, 50), 0);
  gfx::Rect consider = OffsetRect(50, 50);
  std::vector<std::pair<int, int>> expected;
  gfx::Rect center = OffsetRect(15, 15, 1, 1);

  // Full ignore.
  gfx::Rect ignore = OffsetRect(50, 50);

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
  ignore = OffsetRect(15, 0, 20, 100);

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
  expected = {{0, 0}, {0, 1}, {0, 2}, {0, 3}, {4, 3},
              {4, 2}, {4, 1}, {4, 0}, {0, 4}, {4, 4}};
  TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center, expected);

  // Ignore covers the top half.
  ignore = OffsetRect(50, 25);

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
  expected = {{0, 3}, {1, 3}, {2, 3}, {3, 3}, {4, 3},
              {0, 4}, {1, 4}, {2, 4}, {3, 4}, {4, 4}};
  TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center, expected);
}

TEST_P(SpiralIteratorTest, RectangleCenter) {
  TilingData tiling_data(gfx::Size(10, 10), OffsetRect(50, 50), 0);
  gfx::Rect consider = OffsetRect(50, 50);
  std::vector<std::pair<int, int>> expected;
  gfx::Rect ignore;

  // Two cell center
  gfx::Rect center = OffsetRect(25, 25, 1, 10);

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
  expected = {{3, 3}, {3, 2}, {3, 1}, {2, 1}, {1, 1}, {1, 2}, {1, 3}, {1, 4},
              {2, 4}, {3, 4}, {4, 4}, {4, 3}, {4, 2}, {4, 1}, {4, 0}, {3, 0},
              {2, 0}, {1, 0}, {0, 0}, {0, 1}, {0, 2}, {0, 3}, {0, 4}};
  TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center, expected);

  // Three by two center.
  center = OffsetRect(15, 25, 20, 10);

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
  expected = {{4, 3}, {4, 2}, {4, 1}, {3, 1}, {2, 1}, {1, 1}, {0, 1},
              {0, 2}, {0, 3}, {0, 4}, {1, 4}, {2, 4}, {3, 4}, {4, 4},
              {4, 0}, {3, 0}, {2, 0}, {1, 0}, {0, 0}};
  TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center, expected);

  // Column center off the left side.
  center = OffsetRect(-50, 0, 30, 50);

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
  expected = {{0, 4}, {0, 3}, {0, 2}, {0, 1}, {0, 0}, {1, 4}, {1, 3},
              {1, 2}, {1, 1}, {1, 0}, {2, 4}, {2, 3}, {2, 2}, {2, 1},
              {2, 0}, {3, 4}, {3, 3}, {3, 2}, {3, 1}, {3, 0}, {4, 4},
              {4, 3}, {4, 2}, {4, 1}, {4, 0}};
  TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center, expected);
}

TEST_P(SpiralIteratorTest, EdgeCases) {
  TilingData tiling_data(gfx::Size(10, 10), OffsetRect(30, 30), 0);
  std::vector<std::pair<int, int>> expected;
  gfx::Rect center;
  gfx::Rect consider;
  gfx::Rect ignore;

  // Ignore contains, but is not equal to, consider and center.
  ignore = OffsetRect(15, 0, 20, 30);
  consider = OffsetRect(20, 10, 10, 20);
  center = OffsetRect(25, 0, 5, 5);

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
  ignore = OffsetRect(0, 0);
  center = OffsetRect(0, 15, 30, 15);
  consider = OffsetRect(15, 30);

  // Layout of the tiling data, and expected return order:
  //    x 0   1   2
  //  y ┌───┬───┬───┐
  //  0 │  2│  1│   │
  //    ├───┼───┼───┤
  //  1 │  *│  *│  *│
  //    ├───┼───┼───┤
  //  2 │  *│  *│  *│
  //    └───┴───┴───┘
  expected = {{1, 0}, {0, 0}};
  TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center, expected);

  // Consider and ignore are non-intersecting.
  ignore = OffsetRect(5, 30);
  consider = OffsetRect(25, 0, 5, 30);
  center = OffsetRect(15, 0, 1, 1);

  // Layout of the tiling data, and expected return order:
  //    x 0   1   2
  //  y ┌───┬───┬───┐
  //  0 │  I│  *│  1│
  //    ├───┼───┼───┤
  //  1 │  I│   │  2│
  //    ├───┼───┼───┤
  //  2 │  I│   │  3│
  //    └───┴───┴───┘
  expected = {{2, 0}, {2, 1}, {2, 2}};
  TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center, expected);

  // Center intersects with ignore.
  consider = OffsetRect(30, 30);
  center = OffsetRect(15, 0, 1, 30);
  ignore = OffsetRect(0, 15, 30, 1);

  // Layout of the tiling data, and expected return order:
  //    x 0   1   2
  //  y ┌───┬───┬───┐
  //  0 │  3│  *│  2│
  //    ├───┼───┼───┤
  //  1 │  I│  *│  I│
  //    ├───┼───┼───┤
  //  2 │  4│  *│  1│
  //    └───┴───┴───┘
  expected = {{2, 2}, {2, 0}, {0, 0}, {0, 2}};
  TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center, expected);

  // Center and ignore are the same.
  consider = OffsetRect(30, 30);
  center = OffsetRect(15, 0, 1, 30);
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
  expected = {{2, 2}, {2, 1}, {2, 0}, {0, 0}, {0, 1}, {0, 2}};

  TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center, expected);

  // Empty tiling data.
  TilingData empty_data(gfx::Size(0, 0), OffsetRect(0, 0), 0);

  expected.clear();
  TestSpiralIterate(__LINE__, empty_data, consider, ignore, center, expected);

  // Empty consider.
  ignore = OffsetRect(0, 0);
  center = OffsetRect(1, 1, 1, 1);
  consider = OffsetRect(0, 0);

  expected.clear();
  TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center, expected);

  // Empty center. Note: This arbitrarily puts the center to be off the top-left
  // corner.
  consider = OffsetRect(30, 30);
  ignore = OffsetRect(0, 0);
  center = OffsetRect(0, 0);

  // Layout of the tiling data, and expected return order:
  //    x 0   1   2
  //  y ┌───┬───┬───┐
  //  0 │  1│  2│  6│
  //    ├───┼───┼───┤
  //  1 │  3│  4│  5│
  //    ├───┼───┼───┤
  //  2 │  7│  8│  9│
  //    └───┴───┴───┘
  expected = {{0, 0}, {1, 0}, {0, 1}, {1, 1}, {2, 1},
              {2, 0}, {0, 2}, {1, 2}, {2, 2}};
  TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center, expected);

  // Every rect is empty.
  ignore = OffsetRect(0, 0);
  center = OffsetRect(0, 0);
  consider = OffsetRect(0, 0);

  expected.clear();
  TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center, expected);

  // Center is just to the left of cover, and off of the tiling's left side.
  consider = OffsetRect(30, 30);
  ignore = OffsetRect(0, 0);
  center = OffsetRect(-20, 0, 19, 30);

  // Layout of the tiling data, and expected return order:
  //     x 0   1   2
  //   y ┌───┬───┬───┐
  // * 0 │  3│  6│  9│
  //     ├───┼───┼───┤
  // * 1 │  2│  5│  8│
  //     ├───┼───┼───┤
  // * 2 │  1│  4│  7│
  //     └───┴───┴───┘
  expected = {{0, 2}, {0, 1}, {0, 0}, {1, 2}, {1, 1},
              {1, 0}, {2, 2}, {2, 1}, {2, 0}};
  TestSpiralIterate(__LINE__, tiling_data, consider, ignore, center, expected);

  // Tiling is smaller than tile size and center rect is not intersecting to
  // tiling rect.
  TilingData smaller_tiling(gfx::Size(10, 10), OffsetRect(1, 1), 0);
  consider = OffsetRect(10, 10);
  ignore = OffsetRect(0, 0);
  center = OffsetRect(2, 2, 10, 10);

  // Layout of the tiling data, and expected return order:
  //    x   0
  //  y ┌───────┐
  //    │  1    │
  //  0 │       │
  //    │     * │
  //    └───────┘
  expected = {{0, 0}};
  TestSpiralIterate(__LINE__, smaller_tiling, consider, ignore, center,
                    expected);
}

}  // namespace

}  // namespace cc
