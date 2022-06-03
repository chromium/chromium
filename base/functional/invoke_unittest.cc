// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/invoke.h"

#include <functional>

#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(FunctionalTest, Invoke) {
  struct S {
    int i;
    constexpr int add(int x) const { return i + x; }
  };

  constexpr S s = {1};

  // Note: The tests involving a std::reference_wrapper are not static_asserts,
  // since std::reference_wrapper is not constexpr prior to C++20.
  static_assert(base::invoke(&S::add, s, 2) == 3, "");
  EXPECT_EQ(base::invoke(&S::add, std::ref(s), 2), 3);
  static_assert(base::invoke(&S::add, &s, 3) == 4, "");

  static_assert(base::invoke(&S::i, s) == 1, "");
  EXPECT_EQ(base::invoke(&S::i, std::ref(s)), 1);
  static_assert(base::invoke(&S::i, &s) == 1, "");

  static_assert(base::invoke(std::plus<>(), 1, 2) == 3, "");
}

}  // namespace base
