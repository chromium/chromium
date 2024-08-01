// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "cc/base/index_rect.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

TEST(IndexRectTest, NumIndices) {
  struct NumIndicesCase {
    int left;
    int right;
    int top;
    int bottom;
    int num_indices_x;
    int num_indices_y;
  } num_indices_cases[] = {{-10, 10, -10, 10, 21, 21},
                           {0, 5, 0, 10, 6, 11},
                           {1, 2, 3, 4, 2, 2},
                           {0, 0, 0, 0, 1, 1},
                           {10, 10, 10, 10, 1, 1}};

  for (size_t i = 0; i < std::size(num_indices_cases); ++i) {
    const NumIndicesCase& value = num_indices_cases[i];
    IndexRect rect(value.left, value.right, value.top, value.bottom);
    EXPECT_EQ(value.num_indices_x, rect.num_indices_x());
    EXPECT_EQ(value.num_indices_y, rect.num_indices_y());
  }
}

TEST(IndexRectTest, ClampTo) {
  struct Indices {
    int left;
    int right;
    int top;
    int bottom;
  };

  struct ClampToCase {
    Indices first;
    Indices second;
    Indices expected;
    bool valid;
  } clamp_to_cases[] = {{{0, 5, 0, 5}, {0, 5, 0, 5}, {0, 5, 0, 5}, true},
                        {{0, 10, 0, 10}, {0, 5, 0, 5}, {0, 5, 0, 5}, true},
                        {{0, 5, 0, 5}, {0, 10, 0, 10}, {0, 5, 0, 5}, true},
                        {{-10, 5, -10, 5}, {0, 10, 0, 10}, {0, 5, 0, 5}, true},
                        {{0, 5, 0, 5}, {10, 20, 10, 20}, {0, 0, 0, 0}, false}};

  for (size_t i = 0; i < std::size(clamp_to_cases); ++i) {
    const ClampToCase& value = clamp_to_cases[i];
    IndexRect first(value.first.left, value.first.right, value.first.top,
                    value.first.bottom);
    IndexRect second(value.second.left, value.second.right, value.second.top,
                     value.second.bottom);
    IndexRect expected(value.expected.left, value.expected.right,
                       value.expected.top, value.expected.bottom);

    first.ClampTo(second);
    EXPECT_EQ(value.valid, first.is_valid());

    if (value.valid)
      EXPECT_EQ(expected, first);
  }
}

TEST(IndexRectTest, Contains) {
  struct ContainsCase {
    int left;
    int right;
    int top;
    int bottom;
    int index_x;
    int index_y;
    bool contained;
  } contains_cases[] = {
      {-10, 10, -10, 10, -10, -10, true}, {-10, 10, -10, 10, 0, 0, true},
      {-10, 10, -10, 10, 10, 10, true},   {-10, 10, -10, 10, 5, 5, true},
      {-10, 10, -10, 10, -5, -5, true},   {-10, 10, -10, 10, -20, -20, false},
      {-10, 10, -10, 10, 20, 20, false},  {-10, 10, -10, 10, 20, 5, false},
      {-10, 10, -10, 10, 5, 20, false}};

  for (size_t i = 0; i < std::size(contains_cases); ++i) {
    const ContainsCase& value = contains_cases[i];
    IndexRect rect(value.left, value.right, value.top, value.bottom);
    EXPECT_EQ(value.contained, rect.Contains(value.index_x, value.index_y));
  }
}

TEST(IndexRectTest, Equals) {
  EXPECT_TRUE(IndexRect(0, 0, 0, 0) == IndexRect(0, 0, 0, 0));
  EXPECT_FALSE(IndexRect(0, 0, 0, 0) == IndexRect(0, 0, 0, 1));
  EXPECT_TRUE(IndexRect(0, 0, 0, 0) != IndexRect(0, 0, 0, 1));
  EXPECT_FALSE(IndexRect(0, 0, 0, 0) != IndexRect(0, 0, 0, 0));
}

}  // namespace cc
