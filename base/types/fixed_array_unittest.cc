// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/types/fixed_array.h"

#include <stddef.h>

#include <cstring>
#include <memory>
#include <type_traits>

#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

TEST(FixedArrayTest, TriviallyDefaultConstructibleInitializes) {
  using T = int;
  static_assert(std::is_trivially_default_constructible_v<T>);
  using Array = FixedArray<T, 1>;

  // First try an array on the stack.
  Array stack_array(1);
  // This read and the one below are UB if `FixedArray` does not initialize the
  // elements, but hopefully even if the compiler chooses to zero memory anyway,
  // the test will fail under the memory sanitizer.
  EXPECT_EQ(0, stack_array[0]);

  // Now try an array on the heap, where we've purposefully written a non-zero
  // bitpattern in hopes of increasing the chance of catching incorrect
  // behavior.
  constexpr size_t kSize = sizeof(Array);
  alignas(Array) char storage[kSize];
  std::memset(storage, 0xAA, kSize);
  Array* placement_new_array = new (storage) Array(1);
  EXPECT_EQ(0, (*placement_new_array)[0]);
  placement_new_array->~Array();
}

}  // namespace
}  // namespace base
