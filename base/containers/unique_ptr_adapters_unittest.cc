// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/unique_ptr_adapters.h"

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
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

  raw_ptr<Foo> raw_foo1 = foo1;
  raw_ptr<Foo> raw_foo2 = foo2;
  raw_ptr<Foo> raw_foo3 = foo3;
  raw_ptr<Foo, DisableDanglingPtrDetection> dangling_foo1 = foo1;
  raw_ptr<Foo, DisableDanglingPtrDetection> dangling_foo2 = foo2;
  raw_ptr<Foo, DisableDanglingPtrDetection> dangling_foo3 = foo3;

  set.emplace(foo1);
  set.emplace(foo2);

  auto it1 = set.find(foo1);
  EXPECT_TRUE(it1 != set.end());
  EXPECT_EQ(foo1, it1->get());
  EXPECT_TRUE(set.find(raw_foo1) == it1);
  EXPECT_TRUE(set.find(dangling_foo1) == it1);

  {
    auto it2 = set.find(foo2);
    EXPECT_TRUE(it2 != set.end());
    EXPECT_EQ(foo2, it2->get());
    EXPECT_TRUE(set.find(raw_foo2) == it2);
    EXPECT_TRUE(set.find(dangling_foo2) == it2);
  }

  EXPECT_TRUE(set.find(foo3) == set.end());
  EXPECT_TRUE(set.find(raw_foo3) == set.end());
  EXPECT_TRUE(set.find(dangling_foo3) == set.end());

  raw_foo1 = nullptr;  // Avoid dangling raw_ptr.
  set.erase(it1);
  EXPECT_EQ(2, Foo::instance_count);

  EXPECT_TRUE(set.find(foo1) == set.end());
  EXPECT_TRUE(set.find(dangling_foo1) == set.end());

  {
    auto it2 = set.find(foo2);
    EXPECT_TRUE(it2 != set.end());
    EXPECT_EQ(foo2, it2->get());
    EXPECT_TRUE(set.find(raw_foo2) == it2);
    EXPECT_TRUE(set.find(dangling_foo2) == it2);
  }

  raw_foo2 = nullptr;  // Avoid dangling raw_ptr.
  set.clear();
  EXPECT_EQ(1, Foo::instance_count);

  EXPECT_TRUE(set.find(foo1) == set.end());
  EXPECT_TRUE(set.find(foo2) == set.end());
  EXPECT_TRUE(set.find(foo3) == set.end());
  EXPECT_TRUE(set.find(dangling_foo1) == set.end());
  EXPECT_TRUE(set.find(dangling_foo2) == set.end());
  EXPECT_TRUE(set.find(dangling_foo3) == set.end());

  raw_foo3 = nullptr;  // Avoid dangling raw_ptr.
  delete foo3;
  EXPECT_EQ(0, Foo::instance_count);
}

TEST(UniquePtrMatcherTest, Basic) {
  std::vector<std::unique_ptr<Foo>> v;

  auto foo_ptr1 = std::make_unique<Foo>();
  Foo* foo1 = foo_ptr1.get();
  raw_ptr<Foo> raw_foo1 = foo1;
  raw_ptr<Foo, DisableDanglingPtrDetection> dangling_foo1 = foo1;
  v.push_back(std::move(foo_ptr1));

  auto foo_ptr2 = std::make_unique<Foo>();
  Foo* foo2 = foo_ptr2.get();
  raw_ptr<Foo> raw_foo2 = foo2;
  raw_ptr<Foo, DisableDanglingPtrDetection> dangling_foo2 = foo2;
  v.push_back(std::move(foo_ptr2));

  auto foo_ptr3 = std::make_unique<Foo>();
  Foo* foo3 = foo_ptr3.get();
  raw_ptr<Foo> raw_foo3 = foo3;
  raw_ptr<Foo, DisableDanglingPtrDetection> dangling_foo3 = foo3;

  {
    auto iter = ranges::find_if(v, UniquePtrMatcher<Foo>(foo1));
    ASSERT_TRUE(iter != v.end());
    EXPECT_EQ(foo1, iter->get());
    EXPECT_TRUE(ranges::find_if(v, UniquePtrMatcher<Foo>(raw_foo1)) == iter);
    EXPECT_TRUE(ranges::find_if(v, UniquePtrMatcher<Foo>(dangling_foo1)) ==
                iter);
  }

  {
    auto iter = ranges::find_if(v, UniquePtrMatcher<Foo>(foo2));
    ASSERT_TRUE(iter != v.end());
    EXPECT_EQ(foo2, iter->get());
    EXPECT_TRUE(ranges::find_if(v, UniquePtrMatcher<Foo>(raw_foo2)) == iter);
    EXPECT_TRUE(ranges::find_if(v, UniquePtrMatcher<Foo>(dangling_foo2)) ==
                iter);
  }

  {
    auto iter = ranges::find_if(v, MatchesUniquePtr(foo2));
    ASSERT_TRUE(iter != v.end());
    EXPECT_EQ(foo2, iter->get());
    EXPECT_TRUE(ranges::find_if(v, MatchesUniquePtr(raw_foo2)) == iter);
    EXPECT_TRUE(ranges::find_if(v, MatchesUniquePtr(dangling_foo2)) == iter);
  }

  EXPECT_TRUE(ranges::find_if(v, UniquePtrMatcher<Foo>(foo3)) == v.end());
  EXPECT_TRUE(ranges::find_if(v, UniquePtrMatcher<Foo>(raw_foo3)) == v.end());
  EXPECT_TRUE(ranges::find_if(v, UniquePtrMatcher<Foo>(dangling_foo3)) ==
              v.end());

  EXPECT_TRUE(ranges::find_if(v, MatchesUniquePtr(foo3)) == v.end());
  EXPECT_TRUE(ranges::find_if(v, MatchesUniquePtr(raw_foo3)) == v.end());
  EXPECT_TRUE(ranges::find_if(v, MatchesUniquePtr(dangling_foo3)) == v.end());

  raw_foo1 = nullptr;
  raw_foo2 = nullptr;
  raw_foo3 = nullptr;
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
  raw_ptr<Foo> raw_foo1 = foo1;
  raw_ptr<Foo, DisableDanglingPtrDetection> dangling_foo1 = foo1;
  v.push_back(std::move(foo_ptr1));

  UniqueFoo foo_ptr2(new Foo);
  Foo* foo2 = foo_ptr2.get();
  raw_ptr<Foo> raw_foo2 = foo2;
  raw_ptr<Foo, DisableDanglingPtrDetection> dangling_foo2 = foo2;
  v.push_back(std::move(foo_ptr2));

  UniqueFoo foo_ptr3(new Foo);
  Foo* foo3 = foo_ptr3.get();
  raw_ptr<Foo> raw_foo3 = foo3;
  raw_ptr<Foo, DisableDanglingPtrDetection> dangling_foo3 = foo3;

  {
    auto iter = ranges::find_if(v, UniquePtrMatcher<Foo, TestDeleter>(foo1));
    ASSERT_TRUE(iter != v.end());
    EXPECT_EQ(foo1, iter->get());
    EXPECT_TRUE(ranges::find_if(
                    v, UniquePtrMatcher<Foo, TestDeleter>(raw_foo1)) == iter);
    EXPECT_TRUE(ranges::find_if(v, UniquePtrMatcher<Foo, TestDeleter>(
                                       dangling_foo1)) == iter);
  }

  {
    auto iter = ranges::find_if(v, UniquePtrMatcher<Foo, TestDeleter>(foo2));
    ASSERT_TRUE(iter != v.end());
    EXPECT_EQ(foo2, iter->get());
    EXPECT_TRUE(ranges::find_if(
                    v, UniquePtrMatcher<Foo, TestDeleter>(raw_foo2)) == iter);
    EXPECT_TRUE(ranges::find_if(v, UniquePtrMatcher<Foo, TestDeleter>(
                                       dangling_foo2)) == iter);
  }

  {
    auto iter = ranges::find_if(v, MatchesUniquePtr<Foo, TestDeleter>(foo2));
    ASSERT_TRUE(iter != v.end());
    EXPECT_EQ(foo2, iter->get());
    EXPECT_TRUE(ranges::find_if(
                    v, MatchesUniquePtr<Foo, TestDeleter>(raw_foo2)) == iter);
    EXPECT_TRUE(ranges::find_if(v, MatchesUniquePtr<Foo, TestDeleter>(
                                       dangling_foo2)) == iter);
  }

  EXPECT_TRUE(ranges::find_if(v, UniquePtrMatcher<Foo, TestDeleter>(foo3)) ==
              v.end());
  EXPECT_TRUE(ranges::find_if(
                  v, UniquePtrMatcher<Foo, TestDeleter>(raw_foo3)) == v.end());
  EXPECT_TRUE(ranges::find_if(v, UniquePtrMatcher<Foo, TestDeleter>(
                                     dangling_foo3)) == v.end());

  EXPECT_TRUE(ranges::find_if(v, MatchesUniquePtr<Foo, TestDeleter>(foo3)) ==
              v.end());
  EXPECT_TRUE(ranges::find_if(
                  v, MatchesUniquePtr<Foo, TestDeleter>(raw_foo3)) == v.end());
  EXPECT_TRUE(ranges::find_if(v, MatchesUniquePtr<Foo, TestDeleter>(
                                     dangling_foo3)) == v.end());

  raw_foo1 = nullptr;
  raw_foo2 = nullptr;
  raw_foo3 = nullptr;
}

}  // namespace
}  // namespace base
