// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/stl_util.h"

#include <array>
#include <deque>
#include <forward_list>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <list>
#include <queue>
#include <set>
#include <stack>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <vector>

#include "base/containers/cxx20_erase_vector.h"
#include "base/containers/queue.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

using ::testing::IsNull;
using ::testing::Pair;

template <typename Container>
void RunConstCastIteratorTest() {
  using std::begin;
  using std::cbegin;

  Container c = {1, 2, 3, 4, 5};
  auto c_it = std::next(cbegin(c), 3);
  auto it = base::ConstCastIterator(c, c_it);
  static_assert(std::is_same<decltype(cbegin(std::declval<Container&>())),
                             decltype(c_it)>::value,
                "c_it is not a constant iterator.");
  static_assert(std::is_same<decltype(begin(std::declval<Container&>())),
                             decltype(it)>::value,
                "it is not a iterator.");
  EXPECT_EQ(c_it, it);
  // Const casting the iterator should not modify the underlying container.
  Container other = {1, 2, 3, 4, 5};
  EXPECT_THAT(c, testing::ContainerEq(other));
}

}  // namespace

namespace base {
namespace {

TEST(STLUtilTest, GetUnderlyingContainer) {
  {
    std::queue<int> queue({1, 2, 3, 4, 5});
    static_assert(std::is_same<decltype(GetUnderlyingContainer(queue)),
                               const std::deque<int>&>::value,
                  "GetUnderlyingContainer(queue) should be of type deque");
    EXPECT_THAT(GetUnderlyingContainer(queue),
                testing::ElementsAre(1, 2, 3, 4, 5));
  }

  {
    std::queue<int> queue;
    EXPECT_THAT(GetUnderlyingContainer(queue), testing::ElementsAre());
  }

  {
    base::queue<int> queue({1, 2, 3, 4, 5});
    static_assert(
        std::is_same<decltype(GetUnderlyingContainer(queue)),
                     const base::circular_deque<int>&>::value,
        "GetUnderlyingContainer(queue) should be of type circular_deque");
    EXPECT_THAT(GetUnderlyingContainer(queue),
                testing::ElementsAre(1, 2, 3, 4, 5));
  }

  {
    std::vector<int> values = {1, 2, 3, 4, 5};
    std::priority_queue<int> queue(values.begin(), values.end());
    static_assert(std::is_same<decltype(GetUnderlyingContainer(queue)),
                               const std::vector<int>&>::value,
                  "GetUnderlyingContainer(queue) should be of type vector");
    EXPECT_THAT(GetUnderlyingContainer(queue),
                testing::UnorderedElementsAre(1, 2, 3, 4, 5));
  }

  {
    std::stack<int> stack({1, 2, 3, 4, 5});
    static_assert(std::is_same<decltype(GetUnderlyingContainer(stack)),
                               const std::deque<int>&>::value,
                  "GetUnderlyingContainer(stack) should be of type deque");
    EXPECT_THAT(GetUnderlyingContainer(stack),
                testing::ElementsAre(1, 2, 3, 4, 5));
  }
}

TEST(STLUtilTest, ConstCastIterator) {
  // Sequence Containers
  RunConstCastIteratorTest<std::forward_list<int>>();
  RunConstCastIteratorTest<std::list<int>>();
  RunConstCastIteratorTest<std::deque<int>>();
  RunConstCastIteratorTest<std::vector<int>>();
  RunConstCastIteratorTest<std::array<int, 5>>();
  RunConstCastIteratorTest<int[5]>();

  // Associative Containers
  RunConstCastIteratorTest<std::set<int>>();
  RunConstCastIteratorTest<std::multiset<int>>();

  // Unordered Associative Containers
  RunConstCastIteratorTest<std::unordered_set<int>>();
  RunConstCastIteratorTest<std::unordered_multiset<int>>();
}

TEST(STLUtilTest, STLSetDifference) {
  std::set<int> a1;
  a1.insert(1);
  a1.insert(2);
  a1.insert(3);
  a1.insert(4);

  std::set<int> a2;
  a2.insert(3);
  a2.insert(4);
  a2.insert(5);
  a2.insert(6);
  a2.insert(7);

  {
    std::set<int> difference;
    difference.insert(1);
    difference.insert(2);
    EXPECT_EQ(difference, STLSetDifference<std::set<int> >(a1, a2));
  }

  {
    std::set<int> difference;
    difference.insert(5);
    difference.insert(6);
    difference.insert(7);
    EXPECT_EQ(difference, STLSetDifference<std::set<int> >(a2, a1));
  }

  {
    std::vector<int> difference;
    difference.push_back(1);
    difference.push_back(2);
    EXPECT_EQ(difference, STLSetDifference<std::vector<int> >(a1, a2));
  }

  {
    std::vector<int> difference;
    difference.push_back(5);
    difference.push_back(6);
    difference.push_back(7);
    EXPECT_EQ(difference, STLSetDifference<std::vector<int> >(a2, a1));
  }
}

TEST(STLUtilTest, STLSetUnion) {
  std::set<int> a1;
  a1.insert(1);
  a1.insert(2);
  a1.insert(3);
  a1.insert(4);

  std::set<int> a2;
  a2.insert(3);
  a2.insert(4);
  a2.insert(5);
  a2.insert(6);
  a2.insert(7);

  {
    std::set<int> result;
    result.insert(1);
    result.insert(2);
    result.insert(3);
    result.insert(4);
    result.insert(5);
    result.insert(6);
    result.insert(7);
    EXPECT_EQ(result, STLSetUnion<std::set<int> >(a1, a2));
  }

  {
    std::set<int> result;
    result.insert(1);
    result.insert(2);
    result.insert(3);
    result.insert(4);
    result.insert(5);
    result.insert(6);
    result.insert(7);
    EXPECT_EQ(result, STLSetUnion<std::set<int> >(a2, a1));
  }

  {
    std::vector<int> result;
    result.push_back(1);
    result.push_back(2);
    result.push_back(3);
    result.push_back(4);
    result.push_back(5);
    result.push_back(6);
    result.push_back(7);
    EXPECT_EQ(result, STLSetUnion<std::vector<int> >(a1, a2));
  }

  {
    std::vector<int> result;
    result.push_back(1);
    result.push_back(2);
    result.push_back(3);
    result.push_back(4);
    result.push_back(5);
    result.push_back(6);
    result.push_back(7);
    EXPECT_EQ(result, STLSetUnion<std::vector<int> >(a2, a1));
  }
}

TEST(STLUtilTest, STLSetIntersection) {
  std::set<int> a1;
  a1.insert(1);
  a1.insert(2);
  a1.insert(3);
  a1.insert(4);

  std::set<int> a2;
  a2.insert(3);
  a2.insert(4);
  a2.insert(5);
  a2.insert(6);
  a2.insert(7);

  {
    std::set<int> result;
    result.insert(3);
    result.insert(4);
    EXPECT_EQ(result, STLSetIntersection<std::set<int> >(a1, a2));
  }

  {
    std::set<int> result;
    result.insert(3);
    result.insert(4);
    EXPECT_EQ(result, STLSetIntersection<std::set<int> >(a2, a1));
  }

  {
    std::vector<int> result;
    result.push_back(3);
    result.push_back(4);
    EXPECT_EQ(result, STLSetIntersection<std::vector<int> >(a1, a2));
  }

  {
    std::vector<int> result;
    result.push_back(3);
    result.push_back(4);
    EXPECT_EQ(result, STLSetIntersection<std::vector<int> >(a2, a1));
  }
}

TEST(Erase, IsNotIn) {
  // Should keep both '2' but only one '4', like std::set_intersection.
  std::vector<int> lhs = {0, 2, 2, 4, 4, 4, 6, 8, 10};
  std::vector<int> rhs = {1, 2, 2, 4, 5, 6, 7};
  std::vector<int> expected = {2, 2, 4, 6};
  EXPECT_EQ(5u, EraseIf(lhs, IsNotIn<std::vector<int>>(rhs)));
  EXPECT_EQ(expected, lhs);
}

}  // namespace
}  // namespace base
