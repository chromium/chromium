// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <tuple>

#include "testing/gtest/include/gtest/gtest.h"

namespace {

struct OneType {
  int some_int;
};

bool operator<(const OneType& lhs, const OneType& rhs) {
  return lhs.some_int < rhs.some_int;
}

struct AnotherType {
  int some_other_int;
};

// Verify libc++ hardening terminates instead of UB with invalid clamp args.
TEST(ClampTest, Death) {
  EXPECT_DEATH_IF_SUPPORTED(std::ignore = std::clamp(3, 10, 0), "");
  EXPECT_DEATH_IF_SUPPORTED(std::ignore = std::clamp(3.0, 10.0, 0.0), "");

  OneType one_type_0{0};
  OneType one_type_3{3};
  OneType one_type_10{10};
  AnotherType another_type_0{0};
  AnotherType another_type_3{3};
  AnotherType another_type_10{10};
  auto compare_another_type = [](const auto& lhs, const auto& rhs) {
    return lhs.some_other_int < rhs.some_other_int;
  };

  EXPECT_DEATH_IF_SUPPORTED(
      std::ignore = std::clamp(one_type_3, one_type_10, one_type_0), "");
  EXPECT_DEATH_IF_SUPPORTED(
      std::ignore = std::clamp(another_type_3, another_type_10, another_type_0,
                               compare_another_type),
      "");
}

}  // namespace
