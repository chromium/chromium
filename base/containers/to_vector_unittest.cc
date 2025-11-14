// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/to_vector.h"

#include <ranges>
#include <set>

#include "base/containers/adapters.h"
#include "base/containers/flat_set.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::test {

namespace {

using ::testing::ElementsAre;
using ::testing::IsNull;
using ::testing::Pointee;

template <class C>
void IdentityTest() {
  C c = {1, 2, 3, 4, 5};
  auto vec = ToVector(c);
  EXPECT_THAT(vec, ElementsAre(1, 2, 3, 4, 5));
}

template <class C>
void ProjectionTest() {
  C c = {1, 2, 3, 4, 5};
  auto vec = ToVector(c, [](int x) { return x + 1; });
  EXPECT_THAT(vec, ElementsAre(2, 3, 4, 5, 6));
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
  std::vector<std::unique_ptr<int>> v;
  v.push_back(std::make_unique<int>(1));
  v.push_back(std::make_unique<int>(2));
  v.push_back(std::make_unique<int>(3));

  auto v2 = base::ToVector(base::RangeAsRvalues(std::move(v)));
  EXPECT_THAT(v2, ElementsAre(Pointee(1), Pointee(2), Pointee(3)));

  // The old vector should be consumed. The standard guarantees that a
  // moved-from std::unique_ptr will be null.
  // NOLINT(bugprone-use-after-move)
  EXPECT_THAT(v, ElementsAre(IsNull(), IsNull(), IsNull()));

  // Another method which is more verbose so not preferable.
  auto v3 = base::ToVector(
      std::move(v2), [](std::unique_ptr<int>& p) { return std::move(p); });
  EXPECT_THAT(v3, ElementsAre(Pointee(1), Pointee(2), Pointee(3)));
  // NOLINT(bugprone-use-after-move)
  EXPECT_THAT(v2, ElementsAre(IsNull(), IsNull(), IsNull()));
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

TEST(ToVectorTest, MoveConstructionFromArray) {
  auto vec = base::ToVector({
      std::make_unique<int>(1),
      std::make_unique<int>(2),
      std::make_unique<int>(3),
  });
  EXPECT_THAT(vec, ElementsAre(Pointee(1), Pointee(2), Pointee(3)));
}

}  // namespace

}  // namespace base::test
