// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/span.h"

// span.h is a widely included header and its size has significant impact on
// build time. Try not to raise this limit unless absolutely necessary. See
// https://chromium.googlesource.com/chromium/src/+/HEAD/docs/wmax_tokens.md
#ifndef NACL_TC_REV
#pragma clang max_tokens_here 270000
#endif

#include <stdint.h>

#include <iterator>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "base/containers/checked_iterators.h"
#include "base/cxx17_backports.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_piece.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Pointwise;

namespace base {

namespace {

// constexpr implementation of std::equal's 4 argument overload.
template <class InputIterator1, class InputIterator2>
constexpr bool constexpr_equal(InputIterator1 first1,
                               InputIterator1 last1,
                               InputIterator2 first2,
                               InputIterator2 last2) {
  for (; first1 != last1 && first2 != last2; ++first1, ++first2) {
    if (*first1 != *first2)
      return false;
  }

  return first1 == last1 && first2 == last2;
}

}  // namespace

TEST(SpanTest, DefaultConstructor) {
  span<int> dynamic_span;
  EXPECT_EQ(nullptr, dynamic_span.data());
  EXPECT_EQ(0u, dynamic_span.size());

  constexpr span<int, 0> static_span;
  static_assert(nullptr == static_span.data(), "");
  static_assert(0u == static_span.size(), "");
}

TEST(SpanTest, ConstructFromDataAndSize) {
  constexpr int* kNull = nullptr;
  constexpr span<int> empty_span(kNull, 0);
  EXPECT_TRUE(empty_span.empty());
  EXPECT_EQ(nullptr, empty_span.data());

  std::vector<int> vector = {1, 1, 2, 3, 5, 8};

  span<int> dynamic_span(vector.data(), vector.size());
  EXPECT_EQ(vector.data(), dynamic_span.data());
  EXPECT_EQ(vector.size(), dynamic_span.size());

  for (size_t i = 0; i < dynamic_span.size(); ++i)
    EXPECT_EQ(vector[i], dynamic_span[i]);

  span<int, 6> static_span(vector.data(), vector.size());
  EXPECT_EQ(vector.data(), static_span.data());
  EXPECT_EQ(vector.size(), static_span.size());

  for (size_t i = 0; i < static_span.size(); ++i)
    EXPECT_EQ(vector[i], static_span[i]);
}

TEST(SpanTest, ConstructFromIterAndSize) {
  constexpr int* kNull = nullptr;
  constexpr span<int> empty_span(kNull, 0);
  EXPECT_TRUE(empty_span.empty());
  EXPECT_EQ(nullptr, empty_span.data());

  std::vector<int> vector = {1, 1, 2, 3, 5, 8};

  span<int> dynamic_span(vector.begin(), vector.size());
  EXPECT_EQ(vector.data(), dynamic_span.data());
  EXPECT_EQ(vector.size(), dynamic_span.size());

  for (size_t i = 0; i < dynamic_span.size(); ++i)
    EXPECT_EQ(vector[i], dynamic_span[i]);

  span<int, 6> static_span(vector.begin(), vector.size());
  EXPECT_EQ(vector.data(), static_span.data());
  EXPECT_EQ(vector.size(), static_span.size());

  for (size_t i = 0; i < static_span.size(); ++i)
    EXPECT_EQ(vector[i], static_span[i]);
}

TEST(SpanTest, ConstructFromIterPair) {
  constexpr int* kNull = nullptr;
  constexpr span<int> empty_span(kNull, kNull);
  EXPECT_TRUE(empty_span.empty());
  EXPECT_EQ(nullptr, empty_span.data());

  std::vector<int> vector = {1, 1, 2, 3, 5, 8};

  span<int> dynamic_span(vector.begin(), vector.begin() + vector.size() / 2);
  EXPECT_EQ(vector.data(), dynamic_span.data());
  EXPECT_EQ(vector.size() / 2, dynamic_span.size());

  for (size_t i = 0; i < dynamic_span.size(); ++i)
    EXPECT_EQ(vector[i], dynamic_span[i]);

  span<int, 3> static_span(vector.begin(), vector.begin() + vector.size() / 2);
  EXPECT_EQ(vector.data(), static_span.data());
  EXPECT_EQ(vector.size() / 2, static_span.size());

  for (size_t i = 0; i < static_span.size(); ++i)
    EXPECT_EQ(vector[i], static_span[i]);
}

TEST(SpanTest, AllowedConversionsFromStdArray) {
  // In the following assertions we use std::is_convertible_v<From, To>, which
  // for non-void types is equivalent to checking whether the following
  // expression is well-formed:
  //
  // T obj = std::declval<From>();
  //
  // In particular we are checking whether From is implicitly convertible to To,
  // which also implies that To is explicitly constructible from From.
  static_assert(
      std::is_convertible<std::array<int, 3>&, base::span<int>>::value,
      "Error: l-value reference to std::array<int> should be convertible to "
      "base::span<int> with dynamic extent.");
  static_assert(
      std::is_convertible<std::array<int, 3>&, base::span<int, 3>>::value,
      "Error: l-value reference to std::array<int> should be convertible to "
      "base::span<int> with the same static extent.");
  static_assert(
      std::is_convertible<std::array<int, 3>&, base::span<const int>>::value,
      "Error: l-value reference to std::array<int> should be convertible to "
      "base::span<const int> with dynamic extent.");
  static_assert(
      std::is_convertible<std::array<int, 3>&, base::span<const int, 3>>::value,
      "Error: l-value reference to std::array<int> should be convertible to "
      "base::span<const int> with the same static extent.");
  static_assert(std::is_convertible<const std::array<int, 3>&,
                                    base::span<const int>>::value,
                "Error: const l-value reference to std::array<int> should be "
                "convertible to base::span<const int> with dynamic extent.");
  static_assert(
      std::is_convertible<const std::array<int, 3>&,
                          base::span<const int, 3>>::value,
      "Error: const l-value reference to std::array<int> should be convertible "
      "to base::span<const int> with the same static extent.");
  static_assert(std::is_convertible<std::array<const int, 3>&,
                                    base::span<const int>>::value,
                "Error: l-value reference to std::array<const int> should be "
                "convertible to base::span<const int> with dynamic extent.");
  static_assert(
      std::is_convertible<std::array<const int, 3>&,
                          base::span<const int, 3>>::value,
      "Error: l-value reference to std::array<const int> should be convertible "
      "to base::span<const int> with the same static extent.");
  static_assert(
      std::is_convertible<const std::array<const int, 3>&,
                          base::span<const int>>::value,
      "Error: const l-value reference to std::array<const int> should be "
      "convertible to base::span<const int> with dynamic extent.");
  static_assert(
      std::is_convertible<const std::array<const int, 3>&,
                          base::span<const int, 3>>::value,
      "Error: const l-value reference to std::array<const int> should be "
      "convertible to base::span<const int> with the same static extent.");
}

TEST(SpanTest, DisallowedConstructionsFromStdArray) {
  // In the following assertions we use !std::is_constructible_v<T, Args>, which
  // is equivalent to checking whether the following expression is malformed:
  //
  // T obj(std::declval<Args>()...);
  //
  // In particular we are checking that T is not explicitly constructible from
  // Args, which also implies that T is not implicitly constructible from Args
  // as well.
  static_assert(
      !std::is_constructible<base::span<int>, const std::array<int, 3>&>::value,
      "Error: base::span<int> with dynamic extent should not be constructible "
      "from const l-value reference to std::array<int>");

  static_assert(
      !std::is_constructible<base::span<int>, std::array<const int, 3>&>::value,
      "Error: base::span<int> with dynamic extent should not be constructible "
      "from l-value reference to std::array<const int>");

  static_assert(
      !std::is_constructible<base::span<int>,
                             const std::array<const int, 3>&>::value,
      "Error: base::span<int> with dynamic extent should not be constructible "
      "const from l-value reference to std::array<const int>");

  static_assert(
      !std::is_constructible<base::span<int, 2>, std::array<int, 3>&>::value,
      "Error: base::span<int> with static extent should not be constructible "
      "from l-value reference to std::array<int> with different extent");

  static_assert(
      !std::is_constructible<base::span<int, 4>, std::array<int, 3>&>::value,
      "Error: base::span<int> with dynamic extent should not be constructible "
      "from l-value reference to std::array<int> with different extent");

  static_assert(
      !std::is_constructible<base::span<int>, std::array<bool, 3>&>::value,
      "Error: base::span<int> with dynamic extent should not be constructible "
      "from l-value reference to std::array<bool>");
}

TEST(SpanTest, ConstructFromConstexprArray) {
  static constexpr int kArray[] = {5, 4, 3, 2, 1};

  constexpr span<const int> dynamic_span(kArray);
  static_assert(kArray == dynamic_span.data(), "");
  static_assert(base::size(kArray) == dynamic_span.size(), "");

  static_assert(kArray[0] == dynamic_span[0], "");
  static_assert(kArray[1] == dynamic_span[1], "");
  static_assert(kArray[2] == dynamic_span[2], "");
  static_assert(kArray[3] == dynamic_span[3], "");
  static_assert(kArray[4] == dynamic_span[4], "");

  constexpr span<const int, base::size(kArray)> static_span(kArray);
  static_assert(kArray == static_span.data(), "");
  static_assert(base::size(kArray) == static_span.size(), "");

  static_assert(kArray[0] == static_span[0], "");
  static_assert(kArray[1] == static_span[1], "");
  static_assert(kArray[2] == static_span[2], "");
  static_assert(kArray[3] == static_span[3], "");
  static_assert(kArray[4] == static_span[4], "");
}

TEST(SpanTest, ConstructFromArray) {
  int array[] = {5, 4, 3, 2, 1};

  span<const int> const_span(array);
  EXPECT_EQ(array, const_span.data());
  EXPECT_EQ(base::size(array), const_span.size());
  for (size_t i = 0; i < const_span.size(); ++i)
    EXPECT_EQ(array[i], const_span[i]);

  span<int> dynamic_span(array);
  EXPECT_EQ(array, dynamic_span.data());
  EXPECT_EQ(base::size(array), dynamic_span.size());
  for (size_t i = 0; i < dynamic_span.size(); ++i)
    EXPECT_EQ(array[i], dynamic_span[i]);

  span<int, base::size(array)> static_span(array);
  EXPECT_EQ(array, static_span.data());
  EXPECT_EQ(base::size(array), static_span.size());
  for (size_t i = 0; i < static_span.size(); ++i)
    EXPECT_EQ(array[i], static_span[i]);
}

TEST(SpanTest, ConstructFromStdArray) {
  // Note: Constructing a constexpr span from a constexpr std::array does not
  // work prior to C++17 due to non-constexpr std::array::data.
  std::array<int, 5> array = {{5, 4, 3, 2, 1}};

  span<const int> const_span(array);
  EXPECT_EQ(array.data(), const_span.data());
  EXPECT_EQ(array.size(), const_span.size());
  for (size_t i = 0; i < const_span.size(); ++i)
    EXPECT_EQ(array[i], const_span[i]);

  span<int> dynamic_span(array);
  EXPECT_EQ(array.data(), dynamic_span.data());
  EXPECT_EQ(array.size(), dynamic_span.size());
  for (size_t i = 0; i < dynamic_span.size(); ++i)
    EXPECT_EQ(array[i], dynamic_span[i]);

  span<int, base::size(array)> static_span(array);
  EXPECT_EQ(array.data(), static_span.data());
  EXPECT_EQ(array.size(), static_span.size());
  for (size_t i = 0; i < static_span.size(); ++i)
    EXPECT_EQ(array[i], static_span[i]);
}

TEST(SpanTest, ConstructFromInitializerList) {
  std::initializer_list<int> il = {1, 1, 2, 3, 5, 8};

  span<const int> const_span(il);
  EXPECT_EQ(il.begin(), const_span.data());
  EXPECT_EQ(il.size(), const_span.size());

  for (size_t i = 0; i < const_span.size(); ++i)
    EXPECT_EQ(il.begin()[i], const_span[i]);

  span<const int, 6> static_span(il.begin(), il.end());
  EXPECT_EQ(il.begin(), static_span.data());
  EXPECT_EQ(il.size(), static_span.size());

  for (size_t i = 0; i < static_span.size(); ++i)
    EXPECT_EQ(il.begin()[i], static_span[i]);
}

TEST(SpanTest, ConstructFromStdString) {
  std::string str = "foobar";

  span<const char> const_span(str);
  EXPECT_EQ(str.data(), const_span.data());
  EXPECT_EQ(str.size(), const_span.size());

  for (size_t i = 0; i < const_span.size(); ++i)
    EXPECT_EQ(str[i], const_span[i]);

  span<char> dynamic_span(str);
  EXPECT_EQ(str.data(), dynamic_span.data());
  EXPECT_EQ(str.size(), dynamic_span.size());

  for (size_t i = 0; i < dynamic_span.size(); ++i)
    EXPECT_EQ(str[i], dynamic_span[i]);

  span<char, 6> static_span(data(str), str.size());
  EXPECT_EQ(str.data(), static_span.data());
  EXPECT_EQ(str.size(), static_span.size());

  for (size_t i = 0; i < static_span.size(); ++i)
    EXPECT_EQ(str[i], static_span[i]);
}

TEST(SpanTest, ConstructFromConstContainer) {
  const std::vector<int> vector = {1, 1, 2, 3, 5, 8};

  span<const int> const_span(vector);
  EXPECT_EQ(vector.data(), const_span.data());
  EXPECT_EQ(vector.size(), const_span.size());

  for (size_t i = 0; i < const_span.size(); ++i)
    EXPECT_EQ(vector[i], const_span[i]);

  span<const int, 6> static_span(vector.data(), vector.size());
  EXPECT_EQ(vector.data(), static_span.data());
  EXPECT_EQ(vector.size(), static_span.size());

  for (size_t i = 0; i < static_span.size(); ++i)
    EXPECT_EQ(vector[i], static_span[i]);
}

TEST(SpanTest, ConstructFromContainer) {
  std::vector<int> vector = {1, 1, 2, 3, 5, 8};

  span<const int> const_span(vector);
  EXPECT_EQ(vector.data(), const_span.data());
  EXPECT_EQ(vector.size(), const_span.size());

  for (size_t i = 0; i < const_span.size(); ++i)
    EXPECT_EQ(vector[i], const_span[i]);

  span<int> dynamic_span(vector);
  EXPECT_EQ(vector.data(), dynamic_span.data());
  EXPECT_EQ(vector.size(), dynamic_span.size());

  for (size_t i = 0; i < dynamic_span.size(); ++i)
    EXPECT_EQ(vector[i], dynamic_span[i]);

  span<int, 6> static_span(vector.data(), vector.size());
  EXPECT_EQ(vector.data(), static_span.data());
  EXPECT_EQ(vector.size(), static_span.size());

  for (size_t i = 0; i < static_span.size(); ++i)
    EXPECT_EQ(vector[i], static_span[i]);
}

TEST(SpanTest, ConvertNonConstIntegralToConst) {
  std::vector<int> vector = {1, 1, 2, 3, 5, 8};

  span<int> int_span(vector.data(), vector.size());
  span<const int> const_span(int_span);
  EXPECT_EQ(int_span.size(), const_span.size());

  EXPECT_THAT(const_span, Pointwise(Eq(), int_span));

  span<int, 6> static_int_span(vector.data(), vector.size());
  span<const int, 6> static_const_span(static_int_span);
  EXPECT_THAT(static_const_span, Pointwise(Eq(), static_int_span));
}

TEST(SpanTest, ConvertNonConstPointerToConst) {
  auto a = std::make_unique<int>(11);
  auto b = std::make_unique<int>(22);
  auto c = std::make_unique<int>(33);
  std::vector<int*> vector = {a.get(), b.get(), c.get()};

  span<int*> non_const_pointer_span(vector);
  EXPECT_THAT(non_const_pointer_span, Pointwise(Eq(), vector));
  span<int* const> const_pointer_span(non_const_pointer_span);
  EXPECT_THAT(const_pointer_span, Pointwise(Eq(), non_const_pointer_span));
  // Note: no test for conversion from span<int> to span<const int*>, since that
  // would imply a conversion from int** to const int**, which is unsafe.
  //
  // Note: no test for conversion from span<int*> to span<const int* const>,
  // due to CWG Defect 330:
  // http://open-std.org/JTC1/SC22/WG21/docs/cwg_defects.html#330

  span<int*, 3> static_non_const_pointer_span(vector.data(), vector.size());
  EXPECT_THAT(static_non_const_pointer_span, Pointwise(Eq(), vector));
  span<int* const, 3> static_const_pointer_span(static_non_const_pointer_span);
  EXPECT_THAT(static_const_pointer_span,
              Pointwise(Eq(), static_non_const_pointer_span));
}

TEST(SpanTest, ConvertBetweenEquivalentTypes) {
  std::vector<int32_t> vector = {2, 4, 8, 16, 32};

  span<int32_t> int32_t_span(vector);
  span<int> converted_span(int32_t_span);
  EXPECT_EQ(int32_t_span.data(), converted_span.data());
  EXPECT_EQ(int32_t_span.size(), converted_span.size());

  span<int32_t, 5> static_int32_t_span(vector.data(), vector.size());
  span<int, 5> static_converted_span(static_int32_t_span);
  EXPECT_EQ(static_int32_t_span.data(), static_converted_span.data());
  EXPECT_EQ(static_int32_t_span.size(), static_converted_span.size());
}

TEST(SpanTest, TemplatedFirst) {
  static constexpr int array[] = {1, 2, 3};
  constexpr span<const int, 3> span(array);

  {
    constexpr auto subspan = span.first<0>();
    static_assert(span.data() == subspan.data(), "");
    static_assert(0u == subspan.size(), "");
    static_assert(0u == decltype(subspan)::extent, "");
  }

  {
    constexpr auto subspan = span.first<1>();
    static_assert(span.data() == subspan.data(), "");
    static_assert(1u == subspan.size(), "");
    static_assert(1u == decltype(subspan)::extent, "");
    static_assert(1 == subspan[0], "");
  }

  {
    constexpr auto subspan = span.first<2>();
    static_assert(span.data() == subspan.data(), "");
    static_assert(2u == subspan.size(), "");
    static_assert(2u == decltype(subspan)::extent, "");
    static_assert(1 == subspan[0], "");
    static_assert(2 == subspan[1], "");
  }

  {
    constexpr auto subspan = span.first<3>();
    static_assert(span.data() == subspan.data(), "");
    static_assert(3u == subspan.size(), "");
    static_assert(3u == decltype(subspan)::extent, "");
    static_assert(1 == subspan[0], "");
    static_assert(2 == subspan[1], "");
    static_assert(3 == subspan[2], "");
  }
}

TEST(SpanTest, TemplatedLast) {
  static constexpr int array[] = {1, 2, 3};
  constexpr span<const int, 3> span(array);

  {
    constexpr auto subspan = span.last<0>();
    static_assert(span.data() + 3 == subspan.data(), "");
    static_assert(0u == subspan.size(), "");
    static_assert(0u == decltype(subspan)::extent, "");
  }

  {
    constexpr auto subspan = span.last<1>();
    static_assert(span.data() + 2 == subspan.data(), "");
    static_assert(1u == subspan.size(), "");
    static_assert(1u == decltype(subspan)::extent, "");
    static_assert(3 == subspan[0], "");
  }

  {
    constexpr auto subspan = span.last<2>();
    static_assert(span.data() + 1 == subspan.data(), "");
    static_assert(2u == subspan.size(), "");
    static_assert(2u == decltype(subspan)::extent, "");
    static_assert(2 == subspan[0], "");
    static_assert(3 == subspan[1], "");
  }

  {
    constexpr auto subspan = span.last<3>();
    static_assert(span.data() == subspan.data(), "");
    static_assert(3u == subspan.size(), "");
    static_assert(3u == decltype(subspan)::extent, "");
    static_assert(1 == subspan[0], "");
    static_assert(2 == subspan[1], "");
    static_assert(3 == subspan[2], "");
  }
}

TEST(SpanTest, TemplatedSubspan) {
  static constexpr int array[] = {1, 2, 3};
  constexpr span<const int, 3> span(array);

  {
    constexpr auto subspan = span.subspan<0>();
    static_assert(span.data() == subspan.data(), "");
    static_assert(3u == subspan.size(), "");
    static_assert(3u == decltype(subspan)::extent, "");
    static_assert(1 == subspan[0], "");
    static_assert(2 == subspan[1], "");
    static_assert(3 == subspan[2], "");
  }

  {
    constexpr auto subspan = span.subspan<1>();
    static_assert(span.data() + 1 == subspan.data(), "");
    static_assert(2u == subspan.size(), "");
    static_assert(2u == decltype(subspan)::extent, "");
    static_assert(2 == subspan[0], "");
    static_assert(3 == subspan[1], "");
  }

  {
    constexpr auto subspan = span.subspan<2>();
    static_assert(span.data() + 2 == subspan.data(), "");
    static_assert(1u == subspan.size(), "");
    static_assert(1u == decltype(subspan)::extent, "");
    static_assert(3 == subspan[0], "");
  }

  {
    constexpr auto subspan = span.subspan<3>();
    static_assert(span.data() + 3 == subspan.data(), "");
    static_assert(0u == subspan.size(), "");
    static_assert(0u == decltype(subspan)::extent, "");
  }

  {
    constexpr auto subspan = span.subspan<0, 0>();
    static_assert(span.data() == subspan.data(), "");
    static_assert(0u == subspan.size(), "");
    static_assert(0u == decltype(subspan)::extent, "");
  }

  {
    constexpr auto subspan = span.subspan<1, 0>();
    static_assert(span.data() + 1 == subspan.data(), "");
    static_assert(0u == subspan.size(), "");
    static_assert(0u == decltype(subspan)::extent, "");
  }

  {
    constexpr auto subspan = span.subspan<2, 0>();
    static_assert(span.data() + 2 == subspan.data(), "");
    static_assert(0u == subspan.size(), "");
    static_assert(0u == decltype(subspan)::extent, "");
  }

  {
    constexpr auto subspan = span.subspan<0, 1>();
    static_assert(span.data() == subspan.data(), "");
    static_assert(1u == subspan.size(), "");
    static_assert(1u == decltype(subspan)::extent, "");
    static_assert(1 == subspan[0], "");
  }

  {
    constexpr auto subspan = span.subspan<1, 1>();
    static_assert(span.data() + 1 == subspan.data(), "");
    static_assert(1u == subspan.size(), "");
    static_assert(1u == decltype(subspan)::extent, "");
    static_assert(2 == subspan[0], "");
  }

  {
    constexpr auto subspan = span.subspan<2, 1>();
    static_assert(span.data() + 2 == subspan.data(), "");
    static_assert(1u == subspan.size(), "");
    static_assert(1u == decltype(subspan)::extent, "");
    static_assert(3 == subspan[0], "");
  }

  {
    constexpr auto subspan = span.subspan<0, 2>();
    static_assert(span.data() == subspan.data(), "");
    static_assert(2u == subspan.size(), "");
    static_assert(2u == decltype(subspan)::extent, "");
    static_assert(1 == subspan[0], "");
    static_assert(2 == subspan[1], "");
  }

  {
    constexpr auto subspan = span.subspan<1, 2>();
    static_assert(span.data() + 1 == subspan.data(), "");
    static_assert(2u == subspan.size(), "");
    static_assert(2u == decltype(subspan)::extent, "");
    static_assert(2 == subspan[0], "");
    static_assert(3 == subspan[1], "");
  }

  {
    constexpr auto subspan = span.subspan<0, 3>();
    static_assert(span.data() == subspan.data(), "");
    static_assert(3u == subspan.size(), "");
    static_assert(3u == decltype(subspan)::extent, "");
    static_assert(1 == subspan[0], "");
    static_assert(2 == subspan[1], "");
    static_assert(3 == subspan[2], "");
  }
}

TEST(SpanTest, SubscriptedBeginIterator) {
  int array[] = {1, 2, 3};
  span<const int> const_span(array);
  for (size_t i = 0; i < const_span.size(); ++i)
    EXPECT_EQ(array[i], const_span.begin()[i]);

  span<int> mutable_span(array);
  for (size_t i = 0; i < mutable_span.size(); ++i)
    EXPECT_EQ(array[i], mutable_span.begin()[i]);
}

TEST(SpanTest, TemplatedFirstOnDynamicSpan) {
  int array[] = {1, 2, 3};
  span<const int> span(array);

  {
    auto subspan = span.first<0>();
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_EQ(0u, subspan.size());
    static_assert(0u == decltype(subspan)::extent, "");
  }

  {
    auto subspan = span.first<1>();
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_EQ(1u, subspan.size());
    static_assert(1u == decltype(subspan)::extent, "");
    EXPECT_EQ(1, subspan[0]);
  }

  {
    auto subspan = span.first<2>();
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_EQ(2u, subspan.size());
    static_assert(2u == decltype(subspan)::extent, "");
    EXPECT_EQ(1, subspan[0]);
    EXPECT_EQ(2, subspan[1]);
  }

  {
    auto subspan = span.first<3>();
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_EQ(3u, subspan.size());
    static_assert(3u == decltype(subspan)::extent, "");
    EXPECT_EQ(1, subspan[0]);
    EXPECT_EQ(2, subspan[1]);
    EXPECT_EQ(3, subspan[2]);
  }
}

TEST(SpanTest, TemplatedLastOnDynamicSpan) {
  int array[] = {1, 2, 3};
  span<int> span(array);

  {
    auto subspan = span.last<0>();
    EXPECT_EQ(span.data() + 3, subspan.data());
    EXPECT_EQ(0u, subspan.size());
    static_assert(0u == decltype(subspan)::extent, "");
  }

  {
    auto subspan = span.last<1>();
    EXPECT_EQ(span.data() + 2, subspan.data());
    EXPECT_EQ(1u, subspan.size());
    static_assert(1u == decltype(subspan)::extent, "");
    EXPECT_EQ(3, subspan[0]);
  }

  {
    auto subspan = span.last<2>();
    EXPECT_EQ(span.data() + 1, subspan.data());
    EXPECT_EQ(2u, subspan.size());
    static_assert(2u == decltype(subspan)::extent, "");
    EXPECT_EQ(2, subspan[0]);
    EXPECT_EQ(3, subspan[1]);
  }

  {
    auto subspan = span.last<3>();
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_EQ(3u, subspan.size());
    static_assert(3u == decltype(subspan)::extent, "");
    EXPECT_EQ(1, subspan[0]);
    EXPECT_EQ(2, subspan[1]);
    EXPECT_EQ(3, subspan[2]);
  }
}

TEST(SpanTest, TemplatedSubspanFromDynamicSpan) {
  int array[] = {1, 2, 3};
  span<int, 3> span(array);

  {
    auto subspan = span.subspan<0>();
    EXPECT_EQ(span.data(), subspan.data());
    static_assert(3u == decltype(subspan)::extent, "");
    EXPECT_EQ(3u, subspan.size());
    EXPECT_EQ(1, subspan[0]);
    EXPECT_EQ(2, subspan[1]);
    EXPECT_EQ(3, subspan[2]);
  }

  {
    auto subspan = span.subspan<1>();
    EXPECT_EQ(span.data() + 1, subspan.data());
    EXPECT_EQ(2u, subspan.size());
    static_assert(2u == decltype(subspan)::extent, "");
    EXPECT_EQ(2, subspan[0]);
    EXPECT_EQ(3, subspan[1]);
  }

  {
    auto subspan = span.subspan<2>();
    EXPECT_EQ(span.data() + 2, subspan.data());
    EXPECT_EQ(1u, subspan.size());
    static_assert(1u == decltype(subspan)::extent, "");
    EXPECT_EQ(3, subspan[0]);
  }

  {
    auto subspan = span.subspan<3>();
    EXPECT_EQ(span.data() + 3, subspan.data());
    EXPECT_EQ(0u, subspan.size());
    static_assert(0u == decltype(subspan)::extent, "");
  }

  {
    auto subspan = span.subspan<0, 0>();
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_EQ(0u, subspan.size());
    static_assert(0u == decltype(subspan)::extent, "");
  }

  {
    auto subspan = span.subspan<1, 0>();
    EXPECT_EQ(span.data() + 1, subspan.data());
    EXPECT_EQ(0u, subspan.size());
    static_assert(0u == decltype(subspan)::extent, "");
  }

  {
    auto subspan = span.subspan<2, 0>();
    EXPECT_EQ(span.data() + 2, subspan.data());
    EXPECT_EQ(0u, subspan.size());
    static_assert(0u == decltype(subspan)::extent, "");
  }

  {
    auto subspan = span.subspan<0, 1>();
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_EQ(1u, subspan.size());
    static_assert(1u == decltype(subspan)::extent, "");
    EXPECT_EQ(1, subspan[0]);
  }

  {
    auto subspan = span.subspan<1, 1>();
    EXPECT_EQ(span.data() + 1, subspan.data());
    EXPECT_EQ(1u, subspan.size());
    static_assert(1u == decltype(subspan)::extent, "");
    EXPECT_EQ(2, subspan[0]);
  }

  {
    auto subspan = span.subspan<2, 1>();
    EXPECT_EQ(span.data() + 2, subspan.data());
    EXPECT_EQ(1u, subspan.size());
    static_assert(1u == decltype(subspan)::extent, "");
    EXPECT_EQ(3, subspan[0]);
  }

  {
    auto subspan = span.subspan<0, 2>();
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_EQ(2u, subspan.size());
    static_assert(2u == decltype(subspan)::extent, "");
    EXPECT_EQ(1, subspan[0]);
    EXPECT_EQ(2, subspan[1]);
  }

  {
    auto subspan = span.subspan<1, 2>();
    EXPECT_EQ(span.data() + 1, subspan.data());
    EXPECT_EQ(2u, subspan.size());
    static_assert(2u == decltype(subspan)::extent, "");
    EXPECT_EQ(2, subspan[0]);
    EXPECT_EQ(3, subspan[1]);
  }

  {
    auto subspan = span.subspan<0, 3>();
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_EQ(3u, subspan.size());
    static_assert(3u == decltype(subspan)::extent, "");
    EXPECT_EQ(1, subspan[0]);
    EXPECT_EQ(2, subspan[1]);
    EXPECT_EQ(3, subspan[2]);
  }
}

TEST(SpanTest, First) {
  int array[] = {1, 2, 3};
  span<int> span(array);

  {
    auto subspan = span.first(0);
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_EQ(0u, subspan.size());
  }

  {
    auto subspan = span.first(1);
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_EQ(1u, subspan.size());
    EXPECT_EQ(1, subspan[0]);
  }

  {
    auto subspan = span.first(2);
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_EQ(2u, subspan.size());
    EXPECT_EQ(1, subspan[0]);
    EXPECT_EQ(2, subspan[1]);
  }

  {
    auto subspan = span.first(3);
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_EQ(3u, subspan.size());
    EXPECT_EQ(1, subspan[0]);
    EXPECT_EQ(2, subspan[1]);
    EXPECT_EQ(3, subspan[2]);
  }
}

TEST(SpanTest, Last) {
  int array[] = {1, 2, 3};
  span<int> span(array);

  {
    auto subspan = span.last(0);
    EXPECT_EQ(span.data() + 3, subspan.data());
    EXPECT_EQ(0u, subspan.size());
  }

  {
    auto subspan = span.last(1);
    EXPECT_EQ(span.data() + 2, subspan.data());
    EXPECT_EQ(1u, subspan.size());
    EXPECT_EQ(3, subspan[0]);
  }

  {
    auto subspan = span.last(2);
    EXPECT_EQ(span.data() + 1, subspan.data());
    EXPECT_EQ(2u, subspan.size());
    EXPECT_EQ(2, subspan[0]);
    EXPECT_EQ(3, subspan[1]);
  }

  {
    auto subspan = span.last(3);
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_EQ(3u, subspan.size());
    EXPECT_EQ(1, subspan[0]);
    EXPECT_EQ(2, subspan[1]);
    EXPECT_EQ(3, subspan[2]);
  }
}

TEST(SpanTest, Subspan) {
  int array[] = {1, 2, 3};
  span<int> span(array);

  {
    auto subspan = span.subspan(0);
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_EQ(3u, subspan.size());
    EXPECT_EQ(1, subspan[0]);
    EXPECT_EQ(2, subspan[1]);
    EXPECT_EQ(3, subspan[2]);
  }

  {
    auto subspan = span.subspan(1);
    EXPECT_EQ(span.data() + 1, subspan.data());
    EXPECT_EQ(2u, subspan.size());
    EXPECT_EQ(2, subspan[0]);
    EXPECT_EQ(3, subspan[1]);
  }

  {
    auto subspan = span.subspan(2);
    EXPECT_EQ(span.data() + 2, subspan.data());
    EXPECT_EQ(1u, subspan.size());
    EXPECT_EQ(3, subspan[0]);
  }

  {
    auto subspan = span.subspan(3);
    EXPECT_EQ(span.data() + 3, subspan.data());
    EXPECT_EQ(0u, subspan.size());
  }

  {
    auto subspan = span.subspan(0, 0);
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_EQ(0u, subspan.size());
  }

  {
    auto subspan = span.subspan(1, 0);
    EXPECT_EQ(span.data() + 1, subspan.data());
    EXPECT_EQ(0u, subspan.size());
  }

  {
    auto subspan = span.subspan(2, 0);
    EXPECT_EQ(span.data() + 2, subspan.data());
    EXPECT_EQ(0u, subspan.size());
  }

  {
    auto subspan = span.subspan(0, 1);
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_EQ(1u, subspan.size());
    EXPECT_EQ(1, subspan[0]);
  }

  {
    auto subspan = span.subspan(1, 1);
    EXPECT_EQ(span.data() + 1, subspan.data());
    EXPECT_EQ(1u, subspan.size());
    EXPECT_EQ(2, subspan[0]);
  }

  {
    auto subspan = span.subspan(2, 1);
    EXPECT_EQ(span.data() + 2, subspan.data());
    EXPECT_EQ(1u, subspan.size());
    EXPECT_EQ(3, subspan[0]);
  }

  {
    auto subspan = span.subspan(0, 2);
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_EQ(2u, subspan.size());
    EXPECT_EQ(1, subspan[0]);
    EXPECT_EQ(2, subspan[1]);
  }

  {
    auto subspan = span.subspan(1, 2);
    EXPECT_EQ(span.data() + 1, subspan.data());
    EXPECT_EQ(2u, subspan.size());
    EXPECT_EQ(2, subspan[0]);
    EXPECT_EQ(3, subspan[1]);
  }

  {
    auto subspan = span.subspan(0, 3);
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_EQ(span.size(), subspan.size());
    EXPECT_EQ(1, subspan[0]);
    EXPECT_EQ(2, subspan[1]);
    EXPECT_EQ(3, subspan[2]);
  }
}

TEST(SpanTest, Size) {
  {
    span<int> span;
    EXPECT_EQ(0u, span.size());
  }

  {
    int array[] = {1, 2, 3};
    span<int> span(array);
    EXPECT_EQ(3u, span.size());
  }
}

TEST(SpanTest, SizeBytes) {
  {
    span<int> span;
    EXPECT_EQ(0u, span.size_bytes());
  }

  {
    int array[] = {1, 2, 3};
    span<int> span(array);
    EXPECT_EQ(3u * sizeof(int), span.size_bytes());
  }
}

TEST(SpanTest, Empty) {
  {
    span<int> span;
    EXPECT_TRUE(span.empty());
  }

  {
    int array[] = {1, 2, 3};
    span<int> span(array);
    EXPECT_FALSE(span.empty());
  }
}

TEST(SpanTest, OperatorAt) {
  static constexpr int kArray[] = {1, 6, 1, 8, 0};
  constexpr span<const int> span(kArray);

  static_assert(&kArray[0] == &span[0],
                "span[0] does not refer to the same element as kArray[0]");
  static_assert(&kArray[1] == &span[1],
                "span[1] does not refer to the same element as kArray[1]");
  static_assert(&kArray[2] == &span[2],
                "span[2] does not refer to the same element as kArray[2]");
  static_assert(&kArray[3] == &span[3],
                "span[3] does not refer to the same element as kArray[3]");
  static_assert(&kArray[4] == &span[4],
                "span[4] does not refer to the same element as kArray[4]");
}

TEST(SpanTest, Front) {
  static constexpr int kArray[] = {1, 6, 1, 8, 0};
  constexpr span<const int> span(kArray);
  static_assert(&kArray[0] == &span.front(),
                "span.front() does not refer to the same element as kArray[0]");
}

TEST(SpanTest, Back) {
  static constexpr int kArray[] = {1, 6, 1, 8, 0};
  constexpr span<const int> span(kArray);
  static_assert(&kArray[4] == &span.back(),
                "span.back() does not refer to the same element as kArray[4]");
}

TEST(SpanTest, Iterator) {
  static constexpr int kArray[] = {1, 6, 1, 8, 0};
  constexpr span<const int> span(kArray);

  std::vector<int> results;
  for (int i : span)
    results.emplace_back(i);
  EXPECT_THAT(results, ElementsAre(1, 6, 1, 8, 0));
}

TEST(SpanTest, ConstexprIterator) {
  static constexpr int kArray[] = {1, 6, 1, 8, 0};
  constexpr span<const int> span(kArray);

  static_assert(constexpr_equal(std::begin(kArray), std::end(kArray),
                                span.begin(), span.end()),
                "");
  static_assert(1 == span.begin()[0], "");
  static_assert(1 == *(span.begin() += 0), "");
  static_assert(6 == *(span.begin() += 1), "");

  static_assert(1 == *((span.begin() + 1) -= 1), "");
  static_assert(6 == *((span.begin() + 1) -= 0), "");
}

TEST(SpanTest, ReverseIterator) {
  static constexpr int kArray[] = {1, 6, 1, 8, 0};
  constexpr span<const int> span(kArray);

  EXPECT_TRUE(std::equal(std::rbegin(kArray), std::rend(kArray), span.rbegin(),
                         span.rend()));
  EXPECT_TRUE(std::equal(std::crbegin(kArray), std::crend(kArray),
                         std::crbegin(span), std::crend(span)));
}

TEST(SpanTest, AsBytes) {
  {
    constexpr int kArray[] = {2, 3, 5, 7, 11, 13};
    span<const uint8_t, sizeof(kArray)> bytes_span =
        as_bytes(make_span(kArray));
    EXPECT_EQ(reinterpret_cast<const uint8_t*>(kArray), bytes_span.data());
    EXPECT_EQ(sizeof(kArray), bytes_span.size());
    EXPECT_EQ(bytes_span.size(), bytes_span.size_bytes());
  }

  {
    std::vector<int> vec = {1, 1, 2, 3, 5, 8};
    span<int> mutable_span(vec);
    span<const uint8_t> bytes_span = as_bytes(mutable_span);
    EXPECT_EQ(reinterpret_cast<const uint8_t*>(vec.data()), bytes_span.data());
    EXPECT_EQ(sizeof(int) * vec.size(), bytes_span.size());
    EXPECT_EQ(bytes_span.size(), bytes_span.size_bytes());
  }
}

TEST(SpanTest, AsWritableBytes) {
  std::vector<int> vec = {1, 1, 2, 3, 5, 8};
  span<int> mutable_span(vec);
  span<uint8_t> writable_bytes_span = as_writable_bytes(mutable_span);
  EXPECT_EQ(reinterpret_cast<uint8_t*>(vec.data()), writable_bytes_span.data());
  EXPECT_EQ(sizeof(int) * vec.size(), writable_bytes_span.size());
  EXPECT_EQ(writable_bytes_span.size(), writable_bytes_span.size_bytes());

  // Set the first entry of vec to zero while writing through the span.
  std::fill(writable_bytes_span.data(),
            writable_bytes_span.data() + sizeof(int), 0);
  EXPECT_EQ(0, vec[0]);
}

TEST(SpanTest, MakeSpanFromDataAndSize) {
  int* nullint = nullptr;
  auto empty_span = make_span(nullint, 0);
  EXPECT_TRUE(empty_span.empty());
  EXPECT_EQ(nullptr, empty_span.data());

  std::vector<int> vector = {1, 1, 2, 3, 5, 8};
  span<int> expected_span(vector.data(), vector.size());
  auto made_span = make_span(vector.data(), vector.size());
  EXPECT_EQ(expected_span.data(), made_span.data());
  EXPECT_EQ(expected_span.size(), made_span.size());
  static_assert(decltype(made_span)::extent == dynamic_extent, "");
  static_assert(
      std::is_same<decltype(expected_span), decltype(made_span)>::value,
      "the type of made_span differs from expected_span!");
}

TEST(SpanTest, MakeSpanFromPointerPair) {
  int* nullint = nullptr;
  auto empty_span = make_span(nullint, nullint);
  EXPECT_TRUE(empty_span.empty());
  EXPECT_EQ(nullptr, empty_span.data());

  std::vector<int> vector = {1, 1, 2, 3, 5, 8};
  span<int> expected_span(vector.data(), vector.size());
  auto made_span = make_span(vector.data(), vector.data() + vector.size());
  EXPECT_EQ(expected_span.data(), made_span.data());
  EXPECT_EQ(expected_span.size(), made_span.size());
  static_assert(decltype(made_span)::extent == dynamic_extent, "");
  static_assert(
      std::is_same<decltype(expected_span), decltype(made_span)>::value,
      "the type of made_span differs from expected_span!");
}

TEST(SpanTest, MakeSpanFromConstexprArray) {
  static constexpr int kArray[] = {1, 2, 3, 4, 5};
  constexpr span<const int, 5> expected_span(kArray);
  constexpr auto made_span = make_span(kArray);
  EXPECT_EQ(expected_span.data(), made_span.data());
  EXPECT_EQ(expected_span.size(), made_span.size());
  static_assert(decltype(made_span)::extent == 5, "");
  static_assert(
      std::is_same<decltype(expected_span), decltype(made_span)>::value,
      "the type of made_span differs from expected_span!");
}

TEST(SpanTest, MakeSpanFromStdArray) {
  const std::array<int, 5> kArray = {{1, 2, 3, 4, 5}};
  span<const int, 5> expected_span(kArray);
  auto made_span = make_span(kArray);
  EXPECT_EQ(expected_span.data(), made_span.data());
  EXPECT_EQ(expected_span.size(), made_span.size());
  static_assert(decltype(made_span)::extent == 5, "");
  static_assert(
      std::is_same<decltype(expected_span), decltype(made_span)>::value,
      "the type of made_span differs from expected_span!");
}

TEST(SpanTest, MakeSpanFromConstContainer) {
  const std::vector<int> vector = {-1, -2, -3, -4, -5};
  span<const int> expected_span(vector);
  auto made_span = make_span(vector);
  EXPECT_EQ(expected_span.data(), made_span.data());
  EXPECT_EQ(expected_span.size(), made_span.size());
  static_assert(decltype(made_span)::extent == dynamic_extent, "");
  static_assert(
      std::is_same<decltype(expected_span), decltype(made_span)>::value,
      "the type of made_span differs from expected_span!");
}

TEST(SpanTest, MakeStaticSpanFromConstContainer) {
  const std::vector<int> vector = {-1, -2, -3, -4, -5};
  span<const int, 5> expected_span(vector.data(), vector.size());
  auto made_span = make_span<5>(vector);
  EXPECT_EQ(expected_span.data(), made_span.data());
  EXPECT_EQ(expected_span.size(), made_span.size());
  static_assert(decltype(made_span)::extent == 5, "");
  static_assert(
      std::is_same<decltype(expected_span), decltype(made_span)>::value,
      "the type of made_span differs from expected_span!");
}

TEST(SpanTest, MakeSpanFromContainer) {
  std::vector<int> vector = {-1, -2, -3, -4, -5};
  span<int> expected_span(vector);
  auto made_span = make_span(vector);
  EXPECT_EQ(expected_span.data(), made_span.data());
  EXPECT_EQ(expected_span.size(), made_span.size());
  static_assert(decltype(made_span)::extent == dynamic_extent, "");
  static_assert(
      std::is_same<decltype(expected_span), decltype(made_span)>::value,
      "the type of made_span differs from expected_span!");
}

TEST(SpanTest, MakeStaticSpanFromContainer) {
  std::vector<int> vector = {-1, -2, -3, -4, -5};
  span<int, 5> expected_span(vector.data(), vector.size());
  auto made_span = make_span<5>(vector);
  EXPECT_EQ(expected_span.data(), make_span<5>(vector).data());
  EXPECT_EQ(expected_span.size(), make_span<5>(vector).size());
  static_assert(decltype(make_span<5>(vector))::extent == 5, "");
  static_assert(
      std::is_same<decltype(expected_span), decltype(made_span)>::value,
      "the type of made_span differs from expected_span!");
}

TEST(SpanTest, MakeStaticSpanFromConstexprContainer) {
  constexpr StringPiece str = "Hello, World";
  constexpr auto made_span = make_span<12>(str);
  static_assert(str.data() == made_span.data(), "Error: data() does not match");
  static_assert(str.size() == made_span.size(), "Error: size() does not match");
  static_assert(std::is_same<decltype(str)::value_type,
                             decltype(made_span)::value_type>::value,
                "Error: value_type does not match");
  static_assert(str.size() == decltype(made_span)::extent,
                "Error: extent does not match");
}

TEST(SpanTest, MakeSpanFromRValueContainer) {
  std::vector<int> vector = {-1, -2, -3, -4, -5};
  span<const int> expected_span(vector);
  // Note: While static_cast<T&&>(foo) is effectively just a fancy spelling of
  // std::move(foo), make_span does not actually take ownership of the passed in
  // container. Writing it this way makes it more obvious that we simply care
  // about the right behavour when passing rvalues.
  auto made_span = make_span(static_cast<std::vector<int>&&>(vector));
  EXPECT_EQ(expected_span.data(), made_span.data());
  EXPECT_EQ(expected_span.size(), made_span.size());
  static_assert(decltype(made_span)::extent == dynamic_extent, "");
  static_assert(
      std::is_same<decltype(expected_span), decltype(made_span)>::value,
      "the type of made_span differs from expected_span!");
}

TEST(SpanTest, MakeStaticSpanFromRValueContainer) {
  std::vector<int> vector = {-1, -2, -3, -4, -5};
  span<const int, 5> expected_span(vector.data(), vector.size());
  // Note: While static_cast<T&&>(foo) is effectively just a fancy spelling of
  // std::move(foo), make_span does not actually take ownership of the passed in
  // container. Writing it this way makes it more obvious that we simply care
  // about the right behavour when passing rvalues.
  auto made_span = make_span<5>(static_cast<std::vector<int>&&>(vector));
  EXPECT_EQ(expected_span.data(), made_span.data());
  EXPECT_EQ(expected_span.size(), made_span.size());
  static_assert(decltype(made_span)::extent == 5, "");
  static_assert(
      std::is_same<decltype(expected_span), decltype(made_span)>::value,
      "the type of made_span differs from expected_span!");
}

TEST(SpanTest, MakeSpanFromDynamicSpan) {
  static constexpr int kArray[] = {1, 2, 3, 4, 5};
  constexpr span<const int> expected_span(kArray);
  constexpr auto made_span = make_span(expected_span);
  static_assert(std::is_same<decltype(expected_span)::element_type,
                             decltype(made_span)::element_type>::value,
                "make_span(span) should have the same element_type as span");

  static_assert(expected_span.data() == made_span.data(),
                "make_span(span) should have the same data() as span");

  static_assert(expected_span.size() == made_span.size(),
                "make_span(span) should have the same size() as span");

  static_assert(decltype(made_span)::extent == decltype(expected_span)::extent,
                "make_span(span) should have the same extent as span");

  static_assert(
      std::is_same<decltype(expected_span), decltype(made_span)>::value,
      "the type of made_span differs from expected_span!");
}

TEST(SpanTest, MakeSpanFromStaticSpan) {
  static constexpr int kArray[] = {1, 2, 3, 4, 5};
  constexpr span<const int, 5> expected_span(kArray);
  constexpr auto made_span = make_span(expected_span);
  static_assert(std::is_same<decltype(expected_span)::element_type,
                             decltype(made_span)::element_type>::value,
                "make_span(span) should have the same element_type as span");

  static_assert(expected_span.data() == made_span.data(),
                "make_span(span) should have the same data() as span");

  static_assert(expected_span.size() == made_span.size(),
                "make_span(span) should have the same size() as span");

  static_assert(decltype(made_span)::extent == decltype(expected_span)::extent,
                "make_span(span) should have the same extent as span");

  static_assert(
      std::is_same<decltype(expected_span), decltype(made_span)>::value,
      "the type of made_span differs from expected_span!");
}

TEST(SpanTest, EnsureConstexprGoodness) {
  static constexpr int kArray[] = {5, 4, 3, 2, 1};
  constexpr span<const int> constexpr_span(kArray);
  const size_t size = 2;

  const size_t start = 1;
  constexpr span<const int> subspan =
      constexpr_span.subspan(start, start + size);
  for (size_t i = 0; i < subspan.size(); ++i)
    EXPECT_EQ(kArray[start + i], subspan[i]);

  constexpr span<const int> firsts = constexpr_span.first(size);
  for (size_t i = 0; i < firsts.size(); ++i)
    EXPECT_EQ(kArray[i], firsts[i]);

  constexpr span<const int> lasts = constexpr_span.last(size);
  for (size_t i = 0; i < lasts.size(); ++i) {
    const size_t j = (base::size(kArray) - size) + i;
    EXPECT_EQ(kArray[j], lasts[i]);
  }

  constexpr int item = constexpr_span[size];
  EXPECT_EQ(kArray[size], item);
}

TEST(SpanTest, OutOfBoundsDeath) {
  constexpr span<int, 0> kEmptySpan;
  ASSERT_DEATH_IF_SUPPORTED(kEmptySpan[0], "");
  ASSERT_DEATH_IF_SUPPORTED(kEmptySpan.first(1), "");
  ASSERT_DEATH_IF_SUPPORTED(kEmptySpan.last(1), "");
  ASSERT_DEATH_IF_SUPPORTED(kEmptySpan.subspan(1), "");

  constexpr span<int> kEmptyDynamicSpan;
  ASSERT_DEATH_IF_SUPPORTED(kEmptyDynamicSpan[0], "");
  ASSERT_DEATH_IF_SUPPORTED(kEmptyDynamicSpan.front(), "");
  ASSERT_DEATH_IF_SUPPORTED(kEmptyDynamicSpan.first(1), "");
  ASSERT_DEATH_IF_SUPPORTED(kEmptyDynamicSpan.last(1), "");
  ASSERT_DEATH_IF_SUPPORTED(kEmptyDynamicSpan.back(), "");
  ASSERT_DEATH_IF_SUPPORTED(kEmptyDynamicSpan.subspan(1), "");

  static constexpr int kArray[] = {0, 1, 2};
  constexpr span<const int> kNonEmptyDynamicSpan(kArray);
  EXPECT_EQ(3U, kNonEmptyDynamicSpan.size());
  ASSERT_DEATH_IF_SUPPORTED(kNonEmptyDynamicSpan[4], "");
  ASSERT_DEATH_IF_SUPPORTED(kNonEmptyDynamicSpan.subspan(10), "");
  ASSERT_DEATH_IF_SUPPORTED(kNonEmptyDynamicSpan.subspan(1, 7), "");
}

TEST(SpanTest, IteratorIsRangeMoveSafe) {
  static constexpr int kArray[] = {1, 6, 1, 8, 0};
  const size_t kNumElements = 5;
  constexpr span<const int> span(kArray);

  static constexpr int kOverlappingStartIndexes[] = {-4, 0, 3, 4};
  static constexpr int kNonOverlappingStartIndexes[] = {-7, -5, 5, 7};

  // Overlapping ranges.
  for (const int dest_start_index : kOverlappingStartIndexes) {
    EXPECT_FALSE(CheckedContiguousIterator<const int>::IsRangeMoveSafe(
        span.begin(), span.end(),
        CheckedContiguousIterator<const int>(
            span.data() + dest_start_index,
            span.data() + dest_start_index + kNumElements)));
  }

  // Non-overlapping ranges.
  for (const int dest_start_index : kNonOverlappingStartIndexes) {
    EXPECT_TRUE(CheckedContiguousIterator<const int>::IsRangeMoveSafe(
        span.begin(), span.end(),
        CheckedContiguousIterator<const int>(
            span.data() + dest_start_index,
            span.data() + dest_start_index + kNumElements)));
  }

  // IsRangeMoveSafe is true if the length to be moved is 0.
  EXPECT_TRUE(CheckedContiguousIterator<const int>::IsRangeMoveSafe(
      span.begin(), span.begin(),
      CheckedContiguousIterator<const int>(span.data(), span.data())));

  // IsRangeMoveSafe is false if end < begin.
  EXPECT_FALSE(CheckedContiguousIterator<const int>::IsRangeMoveSafe(
      span.end(), span.begin(),
      CheckedContiguousIterator<const int>(span.data(), span.data())));
}

TEST(SpanTest, Sort) {
  int array[] = {5, 4, 3, 2, 1};

  span<int> dynamic_span = array;
  ranges::sort(dynamic_span);
  EXPECT_THAT(array, ElementsAre(1, 2, 3, 4, 5));
  std::sort(dynamic_span.rbegin(), dynamic_span.rend());
  EXPECT_THAT(array, ElementsAre(5, 4, 3, 2, 1));

  span<int, 5> static_span = array;
  std::sort(static_span.rbegin(), static_span.rend(), std::greater<>());
  EXPECT_THAT(array, ElementsAre(1, 2, 3, 4, 5));
  ranges::sort(static_span, std::greater<>());
  EXPECT_THAT(array, ElementsAre(5, 4, 3, 2, 1));
}

TEST(SpanTest, SpanExtentConversions) {
  // Statically checks that various conversions between spans of dynamic and
  // static extent are possible or not.
  static_assert(
      !std::is_constructible<span<int, 0>, span<int>>::value,
      "Error: static span should not be constructible from dynamic span");

  static_assert(!std::is_constructible<span<int, 2>, span<int, 1>>::value,
                "Error: static span should not be constructible from static "
                "span with different extent");

  static_assert(std::is_convertible<span<int, 0>, span<int>>::value,
                "Error: static span should be convertible to dynamic span");

  static_assert(std::is_convertible<span<int>, span<int>>::value,
                "Error: dynamic span should be convertible to dynamic span");

  static_assert(std::is_convertible<span<int, 2>, span<int, 2>>::value,
                "Error: static span should be convertible to static span");
}

TEST(SpanTest, IteratorConversions) {
  static_assert(std::is_convertible<span<int>::iterator,
                                    span<const int>::iterator>::value,
                "Error: iterator should be convertible to const iterator");

  static_assert(!std::is_convertible<span<const int>::iterator,
                                     span<int>::iterator>::value,
                "Error: const iterator should not be convertible to iterator");
}

TEST(SpanTest, ExtentMacro) {
  constexpr size_t kSize = 10;
  std::array<uint8_t, kSize> array;
  static_assert(EXTENT(array) == kSize, "EXTENT broken");

  const std::array<uint8_t, kSize>& reference = array;
  static_assert(EXTENT(reference) == kSize, "EXTENT broken for references");

  const std::array<uint8_t, kSize>* pointer = nullptr;
  static_assert(EXTENT(*pointer) == kSize, "EXTENT broken for pointers");

  uint8_t plain_array[kSize] = {0};
  static_assert(EXTENT(plain_array) == kSize, "EXTENT broken for plain arrays");
}

}  // namespace base
