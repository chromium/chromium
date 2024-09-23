// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/to_vector.h"

#include <set>

#include "base/containers/flat_set.h"
#include "base/ranges/ranges.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::test {

template <class C>
void IdentityTest() {
  C c = {1, 2, 3, 4, 5};
  auto vec = ToVector(c);
  EXPECT_THAT(vec, testing::ElementsAre(1, 2, 3, 4, 5));
}

template <class C>
void ProjectionTest() {
  C c = {1, 2, 3, 4, 5};
  auto vec = ToVector(c, [](int x) { return x + 1; });
  EXPECT_THAT(vec, testing::ElementsAre(2, 3, 4, 5, 6));
}

TEST(ToVectorTest, Identity) {
  IdentityTest<std::vector<int>>();
  IdentityTest<std::set<int>>();
  IdentityTest<int[]>();
  IdentityTest<base::flat_set<int>>();
}

TEST(ToVectorTest, Projection) {
  ProjectionTest<std::vector<int>>();
  ProjectionTest<std::set<int>>();
  ProjectionTest<int[]>();
  ProjectionTest<base::flat_set<int>>();
}

TEST(ToVectorTest, MoveOnly) {
  struct MoveOnly {
    MoveOnly() = default;

    MoveOnly(const MoveOnly&) = delete;
    MoveOnly& operator=(const MoveOnly&) = delete;

    MoveOnly(MoveOnly&&) = default;
    MoveOnly& operator=(MoveOnly&&) = default;
  };

  std::vector<MoveOnly> vec(/*size=*/10);
  auto mapped_vec = ToVector(std::move(vec),
                             [](MoveOnly& value) { return std::move(value); });
  EXPECT_EQ(mapped_vec.size(), 10U);
}

template <typename C, typename Proj, typename T>
constexpr bool CorrectlyProjected =
    std::is_same_v<T,
                   typename decltype(ToVector(
                       std::declval<C>(),
                       std::declval<Proj>()))::value_type>;

TEST(ToVectorTest, CorrectlyProjected) {
  // Tests that projected types are deduced correctly.
  constexpr auto proj = [](const auto& value) -> const auto& { return value; };
  static_assert(CorrectlyProjected<std::vector<std::string>, decltype(proj),
                                   std::string>);
  static_assert(
      CorrectlyProjected<std::set<std::string>, decltype(&std::string::length),
                         std::size_t>);
}

}  // namespace base::test
