// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/cxx17_backports.h"

#include <array>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

TEST(Cxx17BackportTest, Size) {
  {
    std::vector<int> vector = {1, 2, 3, 4, 5};
    static_assert(
        std::is_same<decltype(std::size(vector)),
                     decltype(vector.size())>::value,
        "std::size(vector) should have the same type as vector.size()");
    EXPECT_EQ(vector.size(), std::size(vector));
  }

  {
    std::string empty_str;
    static_assert(
        std::is_same<decltype(std::size(empty_str)),
                     decltype(empty_str.size())>::value,
        "std::size(empty_str) should have the same type as empty_str.size()");
    EXPECT_EQ(0u, std::size(empty_str));
  }

  {
    std::array<int, 4> array = {{1, 2, 3, 4}};
    static_assert(
        std::is_same<decltype(std::size(array)), decltype(array.size())>::value,
        "std::size(array) should have the same type as array.size()");
    static_assert(std::size(array) == array.size(),
                  "std::size(array) should be equal to array.size()");
  }

  {
    int array[] = {1, 2, 3};
    static_assert(std::is_same<size_t, decltype(std::size(array))>::value,
                  "std::size(array) should be of type size_t");
    static_assert(3u == std::size(array), "std::size(array) should be 3");
  }
}

TEST(Cxx17BackportTest, Empty) {
  {
    std::vector<int> vector;
    static_assert(
        std::is_same<decltype(std::empty(vector)),
                     decltype(vector.empty())>::value,
        "std::empty(vector) should have the same type as vector.empty()");
    EXPECT_EQ(vector.empty(), std::empty(vector));
  }

  {
    std::array<int, 4> array = {{1, 2, 3, 4}};
    static_assert(
        std::is_same<decltype(std::empty(array)),
                     decltype(array.empty())>::value,
        "std::empty(array) should have the same type as array.empty()");
    static_assert(std::empty(array) == array.empty(),
                  "std::empty(array) should be equal to array.empty()");
  }

  {
    int array[] = {1, 2, 3};
    static_assert(std::is_same<bool, decltype(std::empty(array))>::value,
                  "std::empty(array) should be of type bool");
    static_assert(!std::empty(array), "std::empty(array) should be false");
  }

  {
    constexpr std::initializer_list<int> il;
    static_assert(std::is_same<bool, decltype(std::empty(il))>::value,
                  "std::empty(il) should be of type bool");
    static_assert(std::empty(il), "std::empty(il) should be true");
  }
}

TEST(Cxx17BackportTest, Data) {
  {
    std::vector<int> vector = {1, 2, 3, 4, 5};
    static_assert(
        std::is_same<decltype(std::data(vector)),
                     decltype(vector.data())>::value,
        "std::data(vector) should have the same type as vector.data()");
    EXPECT_EQ(vector.data(), std::data(vector));
  }

  {
    const std::string cstr = "const string";
    static_assert(
        std::is_same<decltype(std::data(cstr)), decltype(cstr.data())>::value,
        "std::data(cstr) should have the same type as cstr.data()");

    EXPECT_EQ(cstr.data(), std::data(cstr));
  }

  {
    std::string str = "mutable string";
    static_assert(std::is_same<decltype(std::data(str)), char*>::value,
                  "std::data(str) should be of type char*");
    EXPECT_EQ(str.data(), std::data(str));
  }

  {
    std::string empty_str;
    static_assert(std::is_same<decltype(std::data(empty_str)), char*>::value,
                  "std::data(empty_str) should be of type char*");
    EXPECT_EQ(empty_str.data(), std::data(empty_str));
  }

  {
    std::array<int, 4> array = {{1, 2, 3, 4}};
    static_assert(
        std::is_same<decltype(std::data(array)), decltype(array.data())>::value,
        "std::data(array) should have the same type as array.data()");
    // std::array::data() is not constexpr prior to C++17, hence the runtime
    // check.
    EXPECT_EQ(array.data(), std::data(array));
  }

  {
    constexpr int array[] = {1, 2, 3};
    static_assert(std::is_same<const int*, decltype(std::data(array))>::value,
                  "std::data(array) should be of type const int*");
    static_assert(array == std::data(array),
                  "std::data(array) should be array");
  }

  {
    constexpr std::initializer_list<int> il;
    static_assert(
        std::is_same<decltype(il.begin()), decltype(std::data(il))>::value,
        "std::data(il) should have the same type as il.begin()");
    static_assert(il.begin() == std::data(il),
                  "std::data(il) should be equal to il.begin()");
  }
}

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

TEST(Cxx17BackportTest, Clamp) {
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

  EXPECT_CHECK_DEATH(base::clamp(3, 10, 0));
  EXPECT_CHECK_DEATH(base::clamp(3.0, 10.0, 0.0));
  EXPECT_CHECK_DEATH(base::clamp(one_type_3, one_type_10, one_type_0));
  EXPECT_CHECK_DEATH(base::clamp(another_type_3, another_type_10,
                                 another_type_0, compare_another_type));
}

constexpr int Subtract(int a, int b) {
  return a - b;
}

int SubtractUnique(std::unique_ptr<int> a, std::unique_ptr<int> b) {
  return *a - *b;
}

TEST(Cxx17BackportTest, Apply) {
  // Function
  constexpr std::tuple<int, int> tuple1(3, 2);
  static_assert(base::apply(&Subtract, tuple1) == 1,
                "base::apply() can invoke functions as constexpr");

  // Function with move-only types
  std::tuple<std::unique_ptr<int>, std::unique_ptr<int>> tuple2(
      std::make_unique<int>(3), std::make_unique<int>(2));
  EXPECT_EQ(1, base::apply(&SubtractUnique, std::move(tuple2)));

  // Lambda
  const auto subtract_lambda = [](int a, int b) { return a - b; };
  EXPECT_EQ(1, base::apply(subtract_lambda, tuple1));

  // Member function
  class Foo {
   public:
    constexpr Foo(int a) : a_(a) {}

    constexpr int Bar(int b) const { return a_ - b; }

   private:
    int a_;
  };
  static constexpr Foo f(3);
  constexpr std::tuple<const Foo*, int> tuple3(&f, 2);
  static_assert(base::apply(&Foo::Bar, tuple3) == 1,
                "base::apply() can invoke member functions as constexpr");
}

}  // namespace
}  // namespace base
