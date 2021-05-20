// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/cxx17_backports.h"

#include <array>
#include <type_traits>
#include <vector>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

TEST(Cxx17BackportTest, Size) {
  {
    std::vector<int> vector = {1, 2, 3, 4, 5};
    static_assert(
        std::is_same<decltype(base::size(vector)),
                     decltype(vector.size())>::value,
        "base::size(vector) should have the same type as vector.size()");
    EXPECT_EQ(vector.size(), base::size(vector));
  }

  {
    std::string empty_str;
    static_assert(
        std::is_same<decltype(base::size(empty_str)),
                     decltype(empty_str.size())>::value,
        "base::size(empty_str) should have the same type as empty_str.size()");
    EXPECT_EQ(0u, base::size(empty_str));
  }

  {
    std::array<int, 4> array = {{1, 2, 3, 4}};
    static_assert(
        std::is_same<decltype(base::size(array)),
                     decltype(array.size())>::value,
        "base::size(array) should have the same type as array.size()");
    static_assert(base::size(array) == array.size(),
                  "base::size(array) should be equal to array.size()");
  }

  {
    int array[] = {1, 2, 3};
    static_assert(std::is_same<size_t, decltype(base::size(array))>::value,
                  "base::size(array) should be of type size_t");
    static_assert(3u == base::size(array), "base::size(array) should be 3");
  }
}

TEST(Cxx17BackportTest, Empty) {
  {
    std::vector<int> vector;
    static_assert(
        std::is_same<decltype(base::empty(vector)),
                     decltype(vector.empty())>::value,
        "base::empty(vector) should have the same type as vector.empty()");
    EXPECT_EQ(vector.empty(), base::empty(vector));
  }

  {
    std::array<int, 4> array = {{1, 2, 3, 4}};
    static_assert(
        std::is_same<decltype(base::empty(array)),
                     decltype(array.empty())>::value,
        "base::empty(array) should have the same type as array.empty()");
    static_assert(base::empty(array) == array.empty(),
                  "base::empty(array) should be equal to array.empty()");
  }

  {
    int array[] = {1, 2, 3};
    static_assert(std::is_same<bool, decltype(base::empty(array))>::value,
                  "base::empty(array) should be of type bool");
    static_assert(!base::empty(array), "base::empty(array) should be false");
  }

  {
    constexpr std::initializer_list<int> il;
    static_assert(std::is_same<bool, decltype(base::empty(il))>::value,
                  "base::empty(il) should be of type bool");
    static_assert(base::empty(il), "base::empty(il) should be true");
  }
}

TEST(Cxx17BackportTest, Data) {
  {
    std::vector<int> vector = {1, 2, 3, 4, 5};
    static_assert(
        std::is_same<decltype(base::data(vector)),
                     decltype(vector.data())>::value,
        "base::data(vector) should have the same type as vector.data()");
    EXPECT_EQ(vector.data(), base::data(vector));
  }

  {
    const std::string cstr = "const string";
    static_assert(
        std::is_same<decltype(base::data(cstr)), decltype(cstr.data())>::value,
        "base::data(cstr) should have the same type as cstr.data()");

    EXPECT_EQ(cstr.data(), base::data(cstr));
  }

  {
    std::string str = "mutable string";
    static_assert(std::is_same<decltype(base::data(str)), char*>::value,
                  "base::data(str) should be of type char*");
    EXPECT_EQ(str.data(), base::data(str));
  }

  {
    std::string empty_str;
    static_assert(std::is_same<decltype(base::data(empty_str)), char*>::value,
                  "base::data(empty_str) should be of type char*");
    EXPECT_EQ(empty_str.data(), base::data(empty_str));
  }

  {
    std::array<int, 4> array = {{1, 2, 3, 4}};
    static_assert(
        std::is_same<decltype(base::data(array)),
                     decltype(array.data())>::value,
        "base::data(array) should have the same type as array.data()");
    // std::array::data() is not constexpr prior to C++17, hence the runtime
    // check.
    EXPECT_EQ(array.data(), base::data(array));
  }

  {
    constexpr int array[] = {1, 2, 3};
    static_assert(std::is_same<const int*, decltype(base::data(array))>::value,
                  "base::data(array) should be of type const int*");
    static_assert(array == base::data(array),
                  "base::data(array) should be array");
  }

  {
    constexpr std::initializer_list<int> il;
    static_assert(
        std::is_same<decltype(il.begin()), decltype(base::data(il))>::value,
        "base::data(il) should have the same type as il.begin()");
    static_assert(il.begin() == base::data(il),
                  "base::data(il) should be equal to il.begin()");
  }
}

}  // namespace
}  // namespace base
