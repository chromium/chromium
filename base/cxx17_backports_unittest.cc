// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/cxx17_backports.h"

#include <tuple>

#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

struct OneType {
  int some_int;
};

bool operator<(const OneType& lhs, const OneType& rhs) {
  return lhs.some_int < rhs.some_int;
}

bool operator==(const OneType& lhs, const OneType& rhs) {
  return lhs.some_int == rhs.some_int;
}

struct AnotherType {
  int some_other_int;
};

bool operator==(const AnotherType& lhs, const AnotherType& rhs) {
  return lhs.some_other_int == rhs.some_other_int;
}

TEST(ClampTest, Behavior) {
  EXPECT_EQ(0, base::clamp(-5, 0, 10));
  EXPECT_EQ(0, base::clamp(0, 0, 10));
  EXPECT_EQ(3, base::clamp(3, 0, 10));
  EXPECT_EQ(10, base::clamp(10, 0, 10));
  EXPECT_EQ(10, base::clamp(15, 0, 10));

  EXPECT_EQ(0.0, base::clamp(-5.0, 0.0, 10.0));
  EXPECT_EQ(0.0, base::clamp(0.0, 0.0, 10.0));
  EXPECT_EQ(3.0, base::clamp(3.0, 0.0, 10.0));
  EXPECT_EQ(10.0, base::clamp(10.0, 0.0, 10.0));
  EXPECT_EQ(10.0, base::clamp(15.0, 0.0, 10.0));

  EXPECT_EQ(0, base::clamp(-5, 0, 0));
  EXPECT_EQ(0, base::clamp(0, 0, 0));
  EXPECT_EQ(0, base::clamp(3, 0, 0));

  OneType one_type_neg5{-5};
  OneType one_type_0{0};
  OneType one_type_3{3};
  OneType one_type_10{10};
  OneType one_type_15{15};

  EXPECT_EQ(one_type_0, base::clamp(one_type_neg5, one_type_0, one_type_10));
  EXPECT_EQ(one_type_0, base::clamp(one_type_0, one_type_0, one_type_10));
  EXPECT_EQ(one_type_3, base::clamp(one_type_3, one_type_0, one_type_10));
  EXPECT_EQ(one_type_10, base::clamp(one_type_10, one_type_0, one_type_10));
  EXPECT_EQ(one_type_10, base::clamp(one_type_15, one_type_0, one_type_10));

  AnotherType another_type_neg5{-5};
  AnotherType another_type_0{0};
  AnotherType another_type_3{3};
  AnotherType another_type_10{10};
  AnotherType another_type_15{15};

  auto compare_another_type = [](const auto& lhs, const auto& rhs) {
    return lhs.some_other_int < rhs.some_other_int;
  };

  EXPECT_EQ(another_type_0, base::clamp(another_type_neg5, another_type_0,
                                        another_type_10, compare_another_type));
  EXPECT_EQ(another_type_0, base::clamp(another_type_0, another_type_0,
                                        another_type_10, compare_another_type));
  EXPECT_EQ(another_type_3, base::clamp(another_type_3, another_type_0,
                                        another_type_10, compare_another_type));
  EXPECT_EQ(another_type_10,
            base::clamp(another_type_10, another_type_0, another_type_10,
                        compare_another_type));
  EXPECT_EQ(another_type_10,
            base::clamp(another_type_15, another_type_0, another_type_10,
                        compare_another_type));
}

TEST(ClampTest, Death) {
  EXPECT_DEATH_IF_SUPPORTED(std::ignore = base::clamp(3, 10, 0), "");
  EXPECT_DEATH_IF_SUPPORTED(std::ignore = base::clamp(3.0, 10.0, 0.0), "");

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
      std::ignore = base::clamp(one_type_3, one_type_10, one_type_0), "");
  EXPECT_DEATH_IF_SUPPORTED(
      std::ignore = base::clamp(another_type_3, another_type_10, another_type_0,
                                compare_another_type),
      "");
}

}  // namespace
}  // namespace base
