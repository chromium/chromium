// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/ranges/functional.h"

#include <functional>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(RangesTest, Identity) {
  static constexpr identity id;

  std::vector<int> v;
  EXPECT_EQ(&v, &id(v));

  constexpr int arr = {0};
  static_assert(arr == id(arr), "");
}

TEST(RangesTest, Invoke) {
  struct S {
    int i;
    constexpr int add(int x) const { return i + x; }
  };

  constexpr S s = {1};

  // Note: The tests involving a std::reference_wrapper are not static_asserts,
  // since std::reference_wrapper is not constexpr prior to C++20.
  static_assert(invoke(&S::add, s, 2) == 3, "");
  EXPECT_EQ(invoke(&S::add, std::ref(s), 2), 3);
  static_assert(invoke(&S::add, &s, 3) == 4, "");

  static_assert(invoke(&S::i, s) == 1, "");
  EXPECT_EQ(invoke(&S::i, std::ref(s)), 1);
  static_assert(invoke(&S::i, &s) == 1, "");

  static_assert(invoke(std::plus<>(), 1, 2) == 3, "");
}

TEST(RangesTest, EqualTo) {
  ranges::equal_to eq;
  EXPECT_TRUE(eq(0, 0));
  EXPECT_FALSE(eq(0, 1));
  EXPECT_FALSE(eq(1, 0));
}

TEST(RangesTest, Less) {
  ranges::less lt;
  EXPECT_FALSE(lt(0, 0));
  EXPECT_TRUE(lt(0, 1));
  EXPECT_FALSE(lt(1, 0));
}

}  // namespace base
