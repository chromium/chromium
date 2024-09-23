// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/to_value_list.h"

#include <set>

#include "base/containers/flat_set.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

// `Value` isn't copyable, so it's not possible to match against Value(x)
// directly in `testing::ElementsAre`. This is why Value(x) is replaced with
// IsInt(x).
auto IsInt(int value) {
  return testing::Property(&Value::GetInt, testing::Eq(value));
}

template <class C>
void IdentityTest() {
  C c = {1, 2, 3, 4, 5};
  // See comment in `IsInt()` above.
  EXPECT_THAT(ToValueList(c), testing::ElementsAre(IsInt(1), IsInt(2), IsInt(3),
                                                   IsInt(4), IsInt(5)));
}

template <class C>
void ProjectionTest() {
  C c = {1, 2, 3, 4, 5};
  // See comment in `IsInt()` above.
  EXPECT_THAT(
      ToValueList(c, [](int x) { return x + 1; }),
      testing::ElementsAre(IsInt(2), IsInt(3), IsInt(4), IsInt(5), IsInt(6)));
}

TEST(ToListTest, Identity) {
  IdentityTest<std::vector<int>>();
  IdentityTest<std::set<int>>();
  IdentityTest<int[]>();
  IdentityTest<flat_set<int>>();
}

TEST(ToListTest, Projection) {
  ProjectionTest<std::vector<int>>();
  ProjectionTest<std::set<int>>();
  ProjectionTest<int[]>();
  ProjectionTest<flat_set<int>>();
}

// Validates that consuming projections work as intended (every single `Value`
// inside `Value::List` is a move-only type).
TEST(ToListTest, MoveOnly) {
  Value::List list;
  list.resize(10);
  Value::List mapped_list = ToValueList(
      std::move(list), [](Value& value) { return std::move(value); });
  EXPECT_EQ(mapped_list.size(), 10U);
}

}  // namespace
}  // namespace base
