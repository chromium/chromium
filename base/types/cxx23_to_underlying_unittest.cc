// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/types/cxx23_to_underlying.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(Cxx23ToUnderlying, Basic) {
  enum Enum : int {
    kOne = 1,
    kTwo = 2,
  };

  enum class ScopedEnum : char {
    kOne = 1,
    kTwo = 2,
  };

  static_assert(std::is_same_v<decltype(to_underlying(kOne)), int>, "");
  static_assert(std::is_same_v<decltype(to_underlying(kTwo)), int>, "");
  static_assert(to_underlying(kOne) == 1, "");
  static_assert(to_underlying(kTwo) == 2, "");

  static_assert(std::is_same_v<decltype(to_underlying(ScopedEnum::kOne)), char>,
                "");
  static_assert(std::is_same_v<decltype(to_underlying(ScopedEnum::kTwo)), char>,
                "");
  static_assert(to_underlying(ScopedEnum::kOne) == 1, "");
  static_assert(to_underlying(ScopedEnum::kTwo) == 2, "");
}

}  // namespace base
