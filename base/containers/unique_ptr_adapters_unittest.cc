// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/unique_ptr_adapters.h"

#include <memory>
#include <vector>

#include "base/ranges/algorithm.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

class Foo {
 public:
  Foo() { instance_count++; }
  ~Foo() { instance_count--; }
  static int instance_count;
};

int Foo::instance_count = 0;

TEST(UniquePtrComparatorTest, Basic) {
  std::set<std::unique_ptr<Foo>, UniquePtrComparator> set;
  Foo* foo1 = new Foo();
  Foo* foo2 = new Foo();
  Foo* foo3 = new Foo();
  EXPECT_EQ(3, Foo::instance_count);

  set.emplace(foo1);
  set.emplace(foo2);

  auto it1 = set.find(foo1);
  EXPECT_TRUE(it1 != set.end());
  EXPECT_EQ(foo1, it1->get());

  {
    auto it2 = set.find(foo2);
    EXPECT_TRUE(it2 != set.end());
    EXPECT_EQ(foo2, it2->get());
  }

  EXPECT_TRUE(set.find(foo3) == set.end());

  set.erase(it1);
  EXPECT_EQ(2, Foo::instance_count);

  EXPECT_TRUE(set.find(foo1) == set.end());

  {
    auto it2 = set.find(foo2);
    EXPECT_TRUE(it2 != set.end());
    EXPECT_EQ(foo2, it2->get());
  }

  set.clear();
  EXPECT_EQ(1, Foo::instance_count);

  EXPECT_TRUE(set.find(foo1) == set.end());
  EXPECT_TRUE(set.find(foo2) == set.end());
  EXPECT_TRUE(set.find(foo3) == set.end());

  delete foo3;
  EXPECT_EQ(0, Foo::instance_count);
}

TEST(UniquePtrMatcherTest, Basic) {
  std::vector<std::unique_ptr<Foo>> v;
  auto foo_ptr1 = std::make_unique<Foo>();
  Foo* foo1 = foo_ptr1.get();
  v.push_back(std::move(foo_ptr1));
  auto foo_ptr2 = std::make_unique<Foo>();
  Foo* foo2 = foo_ptr2.get();
  v.push_back(std::move(foo_ptr2));

  {
    auto iter = ranges::find_if(v, UniquePtrMatcher<Foo>(foo1));
    ASSERT_TRUE(iter != v.end());
    EXPECT_EQ(foo1, iter->get());
  }

  {
    auto iter = ranges::find_if(v, UniquePtrMatcher<Foo>(foo2));
    ASSERT_TRUE(iter != v.end());
    EXPECT_EQ(foo2, iter->get());
  }

  {
    auto iter = ranges::find_if(v, MatchesUniquePtr(foo2));
    ASSERT_TRUE(iter != v.end());
    EXPECT_EQ(foo2, iter->get());
  }
}

class TestDeleter {
 public:
  void operator()(Foo* foo) { delete foo; }
};

TEST(UniquePtrMatcherTest, Deleter) {
  using UniqueFoo = std::unique_ptr<Foo, TestDeleter>;
  std::vector<UniqueFoo> v;
  UniqueFoo foo_ptr1(new Foo);
  Foo* foo1 = foo_ptr1.get();
  v.push_back(std::move(foo_ptr1));
  UniqueFoo foo_ptr2(new Foo);
  Foo* foo2 = foo_ptr2.get();
  v.push_back(std::move(foo_ptr2));

  {
    auto iter = ranges::find_if(v, UniquePtrMatcher<Foo, TestDeleter>(foo1));
    ASSERT_TRUE(iter != v.end());
    EXPECT_EQ(foo1, iter->get());
  }

  {
    auto iter = ranges::find_if(v, UniquePtrMatcher<Foo, TestDeleter>(foo2));
    ASSERT_TRUE(iter != v.end());
    EXPECT_EQ(foo2, iter->get());
  }

  {
    auto iter = ranges::find_if(v, MatchesUniquePtr<Foo, TestDeleter>(foo2));
    ASSERT_TRUE(iter != v.end());
    EXPECT_EQ(foo2, iter->get());
  }
}

}  // namespace
}  // namespace base
