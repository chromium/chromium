// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/span.h"

#include <stdint.h>

#include <algorithm>
#include <concepts>
#include <iterator>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/adapters.h"
#include "base/containers/checked_iterators.h"
#include "base/debug/alias.h"
#include "base/memory/raw_span.h"
#include "base/numerics/byte_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_ostream_operators.h"
#include "base/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::Pointwise;

namespace base {

namespace {

// Tests for span(It, StrictNumeric<size_t>) deduction guide. These tests use a
// helper function to wrap the static_asserts, as most STL containers don't work
// well in a constexpr context. std::array<T, N> does, but base::span has
// specific overloads for std::array<T, n>, so that ends up being less helpful
// than it would initially appear.
//
// Another alternative would be to use std::declval, but that would be fairly
// verbose.
[[maybe_unused]] void TestDeductionGuides() {
  // Tests for span(It, EndOrSize) deduction guide.
  {
    const std::vector<int> v;
    static_assert(
        std::is_same_v<decltype(span(v.cbegin(), v.size())), span<const int>>);
    static_assert(
        std::is_same_v<decltype(span(v.begin(), v.size())), span<const int>>);
    static_assert(
        std::is_same_v<decltype(span(v.data(), v.size())), span<const int>>);
  }

  {
    std::vector<int> v;
    static_assert(
        std::is_same_v<decltype(span(v.cbegin(), v.size())), span<const int>>);
    static_assert(
        std::is_same_v<decltype(span(v.begin(), v.size())), span<int>>);
    static_assert(
        std::is_same_v<decltype(span(v.data(), v.size())), span<int>>);
  }

  {
    const std::vector<int> v;
    static_assert(
        std::is_same_v<decltype(span(v.cbegin(), v.cend())), span<const int>>);
    static_assert(
        std::is_same_v<decltype(span(v.begin(), v.end())), span<const int>>);
  }

  {
    std::vector<int> v;
    static_assert(
        std::is_same_v<decltype(span(v.cbegin(), v.cend())), span<const int>>);
    static_assert(
        std::is_same_v<decltype(span(v.begin(), v.end())), span<int>>);
  }

  // Tests for span(Range&&) deduction guide.
  {
    const int kArray[] = {1, 2, 3};
    static_assert(std::is_same_v<decltype(span(kArray)), span<const int, 3>>);
  }
  {
    int kArray[] = {1, 2, 3};
    static_assert(std::is_same_v<decltype(span(kArray)), span<int, 3>>);
  }
  // We also deduce an rvalue array to make a fixed-span over const values,
  // which matches the span<const T> constructor from an array.
  static_assert(std::is_same_v<decltype(span({1, 2, 3})), span<const int, 3>>);

  static_assert(
      std::is_same_v<decltype(span(std::declval<std::array<const bool, 3>&>())),
                     span<const bool, 3>>);
  static_assert(
      std::is_same_v<decltype(span(std::declval<std::array<bool, 3>&>())),
                     span<bool, 3>>);

  static_assert(
      std::is_same_v<decltype(span(
                         std::declval<const std::array<const bool, 3>&>())),
                     span<const bool, 3>>);
  static_assert(
      std::is_same_v<decltype(span(
                         std::declval<const std::array<const bool, 3>&&>())),
                     span<const bool, 3>>);
  static_assert(std::is_same_v<
                decltype(span(std::declval<std::array<const bool, 3>&&>())),
                span<const bool, 3>>);
  static_assert(
      std::is_same_v<decltype(span(std::declval<const std::array<bool, 3>&>())),
                     span<const bool, 3>>);
  static_assert(std::is_same_v<
                decltype(span(std::declval<const std::array<bool, 3>&&>())),
                span<const bool, 3>>);
  static_assert(
      std::is_same_v<decltype(span(std::declval<std::array<bool, 3>&&>())),
                     span<const bool, 3>>);

  static_assert(
      std::is_same_v<decltype(span(std::declval<const std::string&>())),
                     span<const char>>);
  static_assert(
      std::is_same_v<decltype(span(std::declval<const std::string&&>())),
                     span<const char>>);
  static_assert(
      std::is_same_v<decltype(span(std::declval<std::string&>())), span<char>>);
  static_assert(std::is_same_v<decltype(span(std::declval<std::string&&>())),
                               span<const char>>);
  static_assert(
      std::is_same_v<decltype(span(std::declval<const std::u16string&>())),
                     span<const char16_t>>);
  static_assert(
      std::is_same_v<decltype(span(std::declval<const std::u16string&&>())),
                     span<const char16_t>>);
  static_assert(std::is_same_v<decltype(span(std::declval<std::u16string&>())),
                               span<char16_t>>);
  static_assert(std::is_same_v<decltype(span(std::declval<std::u16string&&>())),
                               span<const char16_t>>);
  static_assert(std::is_same_v<
                decltype(span(std::declval<const std::array<float, 9>&>())),
                span<const float, 9>>);
  static_assert(std::is_same_v<
                decltype(span(std::declval<const std::array<float, 9>&&>())),
                span<const float, 9>>);
  static_assert(
      std::is_same_v<decltype(span(std::declval<std::array<float, 9>&>())),
                     span<float, 9>>);
  static_assert(
      std::is_same_v<decltype(span(std::declval<std::array<float, 9>&&>())),
                     span<const float, 9>>);
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
  // SAFETY: zero size is correct when pointer argument is NULL.
  constexpr span<int> UNSAFE_BUFFERS(empty_span(kNull, 0u));
  EXPECT_TRUE(empty_span.empty());
  EXPECT_EQ(nullptr, empty_span.data());

  std::vector<int> vector = {1, 1, 2, 3, 5, 8};

  // SAFETY: `vector.size()` describes valid portion of `vector.data()`.
  span<int> UNSAFE_BUFFERS(dynamic_span(vector.data(), vector.size()));
  EXPECT_EQ(vector.data(), dynamic_span.data());
  EXPECT_EQ(vector.size(), dynamic_span.size());

  for (size_t i = 0; i < dynamic_span.size(); ++i) {
    EXPECT_EQ(vector[i], dynamic_span[i]);
  }

  // SAFETY: `vector.size()` describes valid portion of `vector.data()`.
  span<int, 6> UNSAFE_BUFFERS(static_span(vector.data(), vector.size()));
  EXPECT_EQ(vector.data(), static_span.data());
  EXPECT_EQ(vector.size(), static_span.size());

  for (size_t i = 0; i < static_span.size(); ++i) {
    EXPECT_EQ(vector[i], static_span[i]);
  }
}

TEST(SpanTest, ConstructFromDataAndZeroSize) {
  char* nullptr_to_char = nullptr;

  auto empty_span = UNSAFE_BUFFERS(span<char>(nullptr_to_char, 0u));
  EXPECT_EQ(empty_span.size(), 0u);
  EXPECT_EQ(empty_span.data(), nullptr);
  EXPECT_TRUE(empty_span.empty());

  // We expect a `DCHECK` to catch construction of a dangling span - let's cover
  // this expectation in a test, so that future `//base` refactorings (e.g.
  // maybe switching to `std::span`) won't just silently change of this aspect
  // of span behavior.
  EXPECT_DCHECK_DEATH({ UNSAFE_BUFFERS(span<char>(nullptr_to_char, 123u)); });
}

TEST(SpanTest, ConstructFromIterAndSize) {
  constexpr int* kNull = nullptr;
  // SAFETY: zero size is correct when pointer argument is NULL.
  constexpr span<int> UNSAFE_BUFFERS(empty_span(kNull, 0u));
  EXPECT_TRUE(empty_span.empty());
  EXPECT_EQ(nullptr, empty_span.data());

  std::vector<int> vector = {1, 1, 2, 3, 5, 8};

  // SAFETY: `vector.size()` describes valid bytes following `vector.begin()`.
  span<int> UNSAFE_BUFFERS(dynamic_span(vector.begin(), vector.size()));
  EXPECT_EQ(vector.data(), dynamic_span.data());
  EXPECT_EQ(vector.size(), dynamic_span.size());

  for (size_t i = 0; i < dynamic_span.size(); ++i) {
    EXPECT_EQ(vector[i], dynamic_span[i]);
  }

  // SAFETY: `vector.size()` describes valid bytes following `vector.begin()`.
  span<int, 6> UNSAFE_BUFFERS(static_span(vector.begin(), vector.size()));
  EXPECT_EQ(vector.data(), static_span.data());
  EXPECT_EQ(vector.size(), static_span.size());

  for (size_t i = 0; i < static_span.size(); ++i) {
    EXPECT_EQ(vector[i], static_span[i]);
  }
}

TEST(SpanTest, ConstructFromIterPair) {
  constexpr int* kNull = nullptr;
  // SAFETY: required for test, NULL range valid.
  constexpr span<int> UNSAFE_BUFFERS(empty_span(kNull, kNull));
  EXPECT_TRUE(empty_span.empty());
  EXPECT_EQ(nullptr, empty_span.data());

  std::vector<int> vector = {1, 1, 2, 3, 5, 8};

  // SAFETY: `vector.size()` describes valid portion of `vector.data()`,
  // thus one-half `vector.size()` is within this range.
  span<int> UNSAFE_BUFFERS(
      dynamic_span(vector.begin(), vector.begin() + vector.size() / 2));
  EXPECT_EQ(vector.data(), dynamic_span.data());
  EXPECT_EQ(vector.size() / 2, dynamic_span.size());

  for (size_t i = 0; i < dynamic_span.size(); ++i) {
    EXPECT_EQ(vector[i], dynamic_span[i]);
  }

  // SAFETY: `vector.size()` describes valid portion of `vector.data()`,
  // thus one-half `vector.size()` is within this range.
  span<int, 3> UNSAFE_BUFFERS(
      static_span(vector.begin(), vector.begin() + vector.size() / 2));
  EXPECT_EQ(vector.data(), static_span.data());
  EXPECT_EQ(vector.size() / 2, static_span.size());

  for (size_t i = 0; i < static_span.size(); ++i) {
    EXPECT_EQ(vector[i], static_span[i]);
  }
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
      std::is_convertible_v<std::array<int, 3>&, base::span<int>>,
      "Error: l-value reference to std::array<int> should be convertible to "
      "base::span<int> with dynamic extent.");
  static_assert(
      std::is_convertible_v<std::array<int, 3>&, base::span<int, 3>>,
      "Error: l-value reference to std::array<int> should be convertible to "
      "base::span<int> with the same static extent.");
  static_assert(
      std::is_convertible_v<std::array<int, 3>&, base::span<const int>>,
      "Error: l-value reference to std::array<int> should be convertible to "
      "base::span<const int> with dynamic extent.");
  static_assert(
      std::is_convertible_v<std::array<int, 3>&, base::span<const int, 3>>,
      "Error: l-value reference to std::array<int> should be convertible to "
      "base::span<const int> with the same static extent.");
  static_assert(
      std::is_convertible_v<const std::array<int, 3>&, base::span<const int>>,
      "Error: const l-value reference to std::array<int> should be "
      "convertible to base::span<const int> with dynamic extent.");
  static_assert(
      std::is_convertible_v<const std::array<int, 3>&,
                            base::span<const int, 3>>,
      "Error: const l-value reference to std::array<int> should be convertible "
      "to base::span<const int> with the same static extent.");
  static_assert(
      std::is_convertible_v<std::array<const int, 3>&, base::span<const int>>,
      "Error: l-value reference to std::array<const int> should be "
      "convertible to base::span<const int> with dynamic extent.");
  static_assert(
      std::is_convertible_v<std::array<const int, 3>&,
                            base::span<const int, 3>>,
      "Error: l-value reference to std::array<const int> should be convertible "
      "to base::span<const int> with the same static extent.");
  static_assert(
      std::is_convertible_v<const std::array<const int, 3>&,
                            base::span<const int>>,
      "Error: const l-value reference to std::array<const int> should be "
      "convertible to base::span<const int> with dynamic extent.");
  static_assert(
      std::is_convertible_v<const std::array<const int, 3>&,
                            base::span<const int, 3>>,
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
      !std::is_constructible_v<base::span<int>, const std::array<int, 3>&>,
      "Error: base::span<int> with dynamic extent should not be constructible "
      "from const l-value reference to std::array<int>");

  static_assert(
      !std::is_constructible_v<base::span<int>, std::array<const int, 3>&>,
      "Error: base::span<int> with dynamic extent should not be constructible "
      "from l-value reference to std::array<const int>");

  static_assert(
      !std::is_constructible_v<base::span<int>,
                               const std::array<const int, 3>&>,
      "Error: base::span<int> with dynamic extent should not be constructible "
      "const from l-value reference to std::array<const int>");

  static_assert(
      !std::is_constructible_v<base::span<int, 2>, std::array<int, 3>&>,
      "Error: base::span<int> with static extent should not be constructible "
      "from l-value reference to std::array<int> with different extent");

  static_assert(
      !std::is_constructible_v<base::span<int, 4>, std::array<int, 3>&>,
      "Error: base::span<int> with dynamic extent should not be constructible "
      "from l-value reference to std::array<int> with different extent");

  static_assert(
      !std::is_constructible_v<base::span<int>, std::array<bool, 3>&>,
      "Error: base::span<int> with dynamic extent should not be constructible "
      "from l-value reference to std::array<bool>");
}

TEST(SpanTest, ConstructFromConstexprArray) {
  static constexpr int kArray[] = {5, 4, 3, 2, 1};

  constexpr span<const int> dynamic_span(kArray);
  static_assert(kArray == dynamic_span.data(), "");
  static_assert(std::size(kArray) == dynamic_span.size(), "");

  static_assert(kArray[0] == dynamic_span[0], "");
  static_assert(kArray[1] == dynamic_span[1], "");
  static_assert(kArray[2] == dynamic_span[2], "");
  static_assert(kArray[3] == dynamic_span[3], "");
  static_assert(kArray[4] == dynamic_span[4], "");

  constexpr span<const int, std::size(kArray)> static_span(kArray);
  static_assert(kArray == static_span.data(), "");
  static_assert(std::size(kArray) == static_span.size(), "");

  static_assert(kArray[0] == static_span[0], "");
  static_assert(kArray[1] == static_span[1], "");
  static_assert(kArray[2] == static_span[2], "");
  static_assert(kArray[3] == static_span[3], "");
  static_assert(kArray[4] == static_span[4], "");
}

TEST(SpanTest, ConstructFromArray) {
  int array[] = {5, 4, 3, 2, 1};

  span<const int> const_span = array;
  EXPECT_EQ(array, const_span.data());
  EXPECT_THAT(const_span, ElementsAreArray(array));

  span<int> dynamic_span = array;
  EXPECT_EQ(array, dynamic_span.data());
  EXPECT_THAT(dynamic_span, ElementsAreArray(array));

  span<int, std::size(array)> static_span = array;
  EXPECT_EQ(array, static_span.data());
  EXPECT_EQ(std::size(array), static_span.size());
  EXPECT_THAT(static_span, ElementsAreArray(array));

  [](span<const int> dynamic_span) {
    EXPECT_EQ(dynamic_span.size(), 5u);
    EXPECT_EQ(dynamic_span[0u], 5);
    EXPECT_EQ(dynamic_span[4u], 1);
  }({{5, 4, 3, 2, 1}});

  [](span<const int, 5u> static_span) {
    EXPECT_EQ(static_span.size(), 5u);
    EXPECT_EQ(static_span[0u], 5);
    EXPECT_EQ(static_span[4u], 1);
  }({{5, 4, 3, 2, 1}});
}

TEST(SpanTest, ConstructFromVolatileArray) {
  static volatile int array[] = {5, 4, 3, 2, 1};
  span<const volatile int> const_span(array);
  static_assert(std::is_same_v<decltype(&const_span[1]), const volatile int*>);
  static_assert(
      std::is_same_v<decltype(const_span.data()), const volatile int*>);
  EXPECT_EQ(array, const_span.data());
  EXPECT_EQ(std::size(array), const_span.size());
  for (size_t i = 0; i < const_span.size(); ++i) {
    // SAFETY: `const_span` is the same size as `array` per previous
    // EXPECT_EQ(), and const_span.size() describes the valid portion of
    // const span, so indexing `array` at the same place is valid.
    EXPECT_EQ(UNSAFE_BUFFERS(array[i]), const_span[i]);
  }

  span<volatile int> dynamic_span(array);
  static_assert(std::is_same_v<decltype(&dynamic_span[1]), volatile int*>);
  static_assert(std::is_same_v<decltype(dynamic_span.data()), volatile int*>);
  EXPECT_EQ(array, dynamic_span.data());
  EXPECT_EQ(std::size(array), dynamic_span.size());
  for (size_t i = 0; i < dynamic_span.size(); ++i) {
    // SAFETY: `dynamic_span` is the same size as `array` per previous
    // EXPECT_EQ(), and `dynamic_span.size()` describes the valid portion of
    // `dynamic_span`, so indexing `array` at the same place is valid.
    EXPECT_EQ(UNSAFE_BUFFERS(array[i]), dynamic_span[i]);
  }

  span<volatile int, std::size(array)> static_span(array);
  static_assert(std::is_same_v<decltype(&static_span[1]), volatile int*>);
  static_assert(std::is_same_v<decltype(static_span.data()), volatile int*>);
  EXPECT_EQ(array, static_span.data());
  EXPECT_EQ(std::size(array), static_span.size());
  for (size_t i = 0; i < static_span.size(); ++i) {
    // SAFETY: `static_span` is the same size as `array` per previous
    // EXPECT_EQ(), and `static_span.size()` describes the valid portion of
    // `static_span`, so indexing `array` at the same place is valid.
    EXPECT_EQ(UNSAFE_BUFFERS(array[i]), static_span[i]);
  }
}

TEST(SpanTest, ConstructFromStdArray) {
  // Note: Constructing a constexpr span from a constexpr std::array does not
  // work prior to C++17 due to non-constexpr std::array::data.
  std::array<int, 5> array = {{5, 4, 3, 2, 1}};

  span<const int> const_span(array);
  EXPECT_EQ(array.data(), const_span.data());
  EXPECT_EQ(array.size(), const_span.size());
  for (size_t i = 0; i < const_span.size(); ++i) {
    EXPECT_EQ(array[i], const_span[i]);
  }

  span<int> dynamic_span(array);
  EXPECT_EQ(array.data(), dynamic_span.data());
  EXPECT_EQ(array.size(), dynamic_span.size());
  for (size_t i = 0; i < dynamic_span.size(); ++i) {
    EXPECT_EQ(array[i], dynamic_span[i]);
  }

  span<int, std::size(array)> static_span(array);
  EXPECT_EQ(array.data(), static_span.data());
  EXPECT_EQ(array.size(), static_span.size());
  for (size_t i = 0; i < static_span.size(); ++i) {
    EXPECT_EQ(array[i], static_span[i]);
  }
}

TEST(SpanTest, ConstructFromInitializerList) {
  std::initializer_list<int> il = {1, 1, 2, 3, 5, 8};

  span<const int> const_span(il);
  EXPECT_EQ(il.begin(), const_span.data());
  EXPECT_EQ(il.size(), const_span.size());

  for (size_t i = 0; i < const_span.size(); ++i) {
    // SAFETY: `il.begin()` is valid to index up to `il.size()`, and
    // `il.size()` equals `const_span.size()`, so `il.begin()` is valid
    // to index up to `const_span.size()` per above loop condition.
    EXPECT_EQ(UNSAFE_BUFFERS(il.begin()[i]), const_span[i]);
  }

  // SAFETY: [il.begin()..il.end()) is a valid range over `il`.
  span<const int, 6> UNSAFE_BUFFERS(static_span(il.begin(), il.end()));
  EXPECT_EQ(il.begin(), static_span.data());
  EXPECT_EQ(il.size(), static_span.size());

  for (size_t i = 0; i < static_span.size(); ++i) {
    // SAFETY: `il.begin()` is valid to index up to `il.size()`, and
    // `il.size()` equals `static_span.size()`, so `il.begin()` is valid
    // to index up to `static_span.size()` per above loop condition.
    EXPECT_EQ(UNSAFE_BUFFERS(il.begin()[i]), static_span[i]);
  }
}

TEST(SpanTest, ConstructFromStdString) {
  std::string str = "foobar";

  span<const char> const_span(str);
  EXPECT_EQ(str.data(), const_span.data());
  EXPECT_EQ(str.size(), const_span.size());

  for (size_t i = 0; i < const_span.size(); ++i) {
    EXPECT_EQ(str[i], const_span[i]);
  }

  span<char> dynamic_span(str);
  EXPECT_EQ(str.data(), dynamic_span.data());
  EXPECT_EQ(str.size(), dynamic_span.size());

  for (size_t i = 0; i < dynamic_span.size(); ++i) {
    EXPECT_EQ(str[i], dynamic_span[i]);
  }

  // SAFETY: `str.size()` describes the valid portion of `str.data()` prior
  // to the terminating NUL.
  span<char, 6> UNSAFE_BUFFERS(static_span(str.data(), str.size()));
  EXPECT_EQ(str.data(), static_span.data());
  EXPECT_EQ(str.size(), static_span.size());

  for (size_t i = 0; i < static_span.size(); ++i) {
    EXPECT_EQ(str[i], static_span[i]);
  }
}

TEST(SpanTest, ConstructFromConstContainer) {
  const std::vector<int> vector = {1, 1, 2, 3, 5, 8};

  span<const int> const_span(vector);
  EXPECT_EQ(vector.data(), const_span.data());
  EXPECT_EQ(vector.size(), const_span.size());

  for (size_t i = 0; i < const_span.size(); ++i) {
    EXPECT_EQ(vector[i], const_span[i]);
  }

  // SAFETY: `vector.size()` describes valid portion of `vector.data()`.
  span<const int, 6> UNSAFE_BUFFERS(static_span(vector.data(), vector.size()));
  EXPECT_EQ(vector.data(), static_span.data());
  EXPECT_EQ(vector.size(), static_span.size());

  for (size_t i = 0; i < static_span.size(); ++i) {
    EXPECT_EQ(vector[i], static_span[i]);
  }
}

TEST(SpanTest, ConstructFromContainer) {
  std::vector<int> vector = {1, 1, 2, 3, 5, 8};

  span<const int> const_span(vector);
  EXPECT_EQ(vector.data(), const_span.data());
  EXPECT_EQ(vector.size(), const_span.size());

  for (size_t i = 0; i < const_span.size(); ++i) {
    EXPECT_EQ(vector[i], const_span[i]);
  }

  span<int> dynamic_span(vector);
  EXPECT_EQ(vector.data(), dynamic_span.data());
  EXPECT_EQ(vector.size(), dynamic_span.size());

  for (size_t i = 0; i < dynamic_span.size(); ++i) {
    EXPECT_EQ(vector[i], dynamic_span[i]);
  }

  // SAFETY: vector.size() describes valid portion of vector.data().
  span<int, 6> UNSAFE_BUFFERS(static_span(vector.data(), vector.size()));
  EXPECT_EQ(vector.data(), static_span.data());
  EXPECT_EQ(vector.size(), static_span.size());

  for (size_t i = 0; i < static_span.size(); ++i) {
    EXPECT_EQ(vector[i], static_span[i]);
  }
}

TEST(SpanTest, ConstructFromRange) {
  struct Range {
    using iterator = base::span<const int>::iterator;
    iterator begin() const { return base::span(arr_).begin(); }
    iterator end() const { return base::span(arr_).end(); }

    std::array<const int, 3u> arr_ = {1, 2, 3};
  };
  static_assert(std::ranges::contiguous_range<Range>);
  {
    Range r;
    auto s = base::span(r);
    static_assert(std::same_as<decltype(s), base::span<const int>>);
    EXPECT_EQ(s, base::span({1, 2, 3}));

    // Implicit from modern range with dynamic size to dynamic span.
    base::span<const int> imp = r;
    EXPECT_EQ(imp, base::span({1, 2, 3}));
  }
  {
    Range r;
    auto s = base::span<const int, 3u>(r);
    EXPECT_EQ(s, base::span({1, 2, 3}));

    // Explicit from modern range with dynamic size to fixed span.
    static_assert(!std::convertible_to<decltype(r), base::span<const int, 3u>>);
    base::span<const int, 3u> imp(r);
    EXPECT_EQ(imp, base::span({1, 2, 3}));
  }

  struct LegacyRange {
    const int* data() const { return arr_.data(); }
    size_t size() const { return arr_.size(); }

    std::array<const int, 3u> arr_ = {1, 2, 3};
  };
  static_assert(!std::ranges::contiguous_range<LegacyRange>);
  static_assert(base::internal::LegacyRange<LegacyRange>);
  {
    LegacyRange r;
    auto s = base::span(r);
    static_assert(std::same_as<decltype(s), base::span<const int>>);
    EXPECT_EQ(s, base::span({1, 2, 3}));

    // Implicit from legacy range with dynamic size to dynamic span.
    base::span<const int> imp = r;
    EXPECT_EQ(imp, base::span({1, 2, 3}));
  }
  {
    LegacyRange r;
    auto s = base::span<const int, 3u>(r);
    EXPECT_EQ(s, base::span({1, 2, 3}));

    // Explicit from legacy range with dynamic size to fixed span.
    static_assert(!std::convertible_to<decltype(r), base::span<const int, 3u>>);
    base::span<const int, 3> imp(r);
    EXPECT_EQ(imp, base::span({1, 2, 3}));
  }

  using FixedRange = const std::array<int, 3>;
  static_assert(std::ranges::contiguous_range<FixedRange>);
  static_assert(std::ranges::sized_range<FixedRange>);
  {
    FixedRange r = {1, 2, 3};
    auto s = base::span(r);
    static_assert(std::same_as<decltype(s), base::span<const int, 3>>);
    EXPECT_EQ(s, base::span({1, 2, 3}));

    // Implicit from fixed size to dynamic span.
    base::span<const int> imp = r;
    EXPECT_EQ(imp, base::span({1, 2, 3}));
  }
  {
    FixedRange r = {1, 2, 3};
    auto s = base::span<const int, 3u>(r);
    EXPECT_EQ(s, base::span({1, 2, 3}));

    // Implicit from fixed size to fixed span.
    base::span<const int, 3u> imp = r;
    EXPECT_EQ(imp, base::span({1, 2, 3}));
  }

  // Construction from std::vectors.

  {
    // Implicit.
    static_assert(std::convertible_to<const std::vector<int>, span<const int>>);
    const std::vector<int> i{1, 2, 3};
    span<const int> s = i;
    EXPECT_EQ(s, i);
  }
  {
    // Explicit.
    static_assert(
        !std::convertible_to<const std::vector<int>, span<const int, 3u>>);
    static_assert(
        std::constructible_from<span<const int, 3u>, const std::vector<int>>);
    const std::vector<int> i{1, 2, 3};
    span<const int, 3u> s(i);
    EXPECT_EQ(s, base::span(i));
  }

  // vector<bool> is special and can't be converted to a span since it does not
  // actually hold an array of `bool`.
  static_assert(
      !std::constructible_from<span<const bool>, const std::vector<bool>>);
  static_assert(
      !std::constructible_from<span<const bool, 3u>, const std::vector<bool>>);
}

TEST(SpanTest, FromRefOfMutableStackVariable) {
  int x = 123;

  auto s = span_from_ref(x);
  static_assert(std::is_same_v<decltype(s), span<int, 1u>>);
  EXPECT_EQ(&x, s.data());
  EXPECT_EQ(1u, s.size());
  EXPECT_EQ(sizeof(int), s.size_bytes());
  EXPECT_EQ(123, s[0]);

  s[0] = 456;
  EXPECT_EQ(456, x);
  EXPECT_EQ(456, s[0]);

  auto b = byte_span_from_ref(x);
  static_assert(std::is_same_v<decltype(b), span<uint8_t, sizeof(int)>>);
  EXPECT_EQ(reinterpret_cast<uint8_t*>(&x), b.data());
  EXPECT_EQ(sizeof(int), b.size());
}

TEST(SpanTest, FromRefOfConstStackVariable) {
  const int x = 123;

  auto s = span_from_ref(x);
  static_assert(std::is_same_v<decltype(s), span<const int, 1u>>);
  EXPECT_EQ(&x, s.data());
  EXPECT_EQ(1u, s.size());
  EXPECT_EQ(sizeof(int), s.size_bytes());
  EXPECT_EQ(123, s[0]);

  auto b = byte_span_from_ref(x);
  static_assert(std::is_same_v<decltype(b), span<const uint8_t, sizeof(int)>>);
  EXPECT_EQ(reinterpret_cast<const uint8_t*>(&x), b.data());
  EXPECT_EQ(sizeof(int), b.size());
}

TEST(SpanTest, FromCString) {
  // No terminating null, size known at compile time.
  {
    auto s = base::span_from_cstring("hello");
    static_assert(std::same_as<decltype(s), span<const char, 5u>>);
    EXPECT_EQ(s[0u], 'h');
    EXPECT_EQ(s[1u], 'e');
    EXPECT_EQ(s[4u], 'o');
  }
  // No terminating null, size not known at compile time. string_view loses
  // the size.
  {
    auto s = base::span(std::string_view("hello"));
    static_assert(std::same_as<decltype(s), span<const char>>);
    EXPECT_EQ(s[0u], 'h');
    EXPECT_EQ(s[1u], 'e');
    EXPECT_EQ(s[4u], 'o');
    EXPECT_EQ(s.size(), 5u);
  }
  // Includes the terminating null, size known at compile time.
  {
    auto s = base::span_with_nul_from_cstring("hello");
    static_assert(std::same_as<decltype(s), span<const char, 6u>>);
    EXPECT_EQ(s[0u], 'h');
    EXPECT_EQ(s[1u], 'e');
    EXPECT_EQ(s[4u], 'o');
    EXPECT_EQ(s[5u], '\0');
  }

  // No terminating null, size known at compile time. Converted to a span of
  // uint8_t bytes.
  {
    auto s = base::byte_span_from_cstring("hello");
    static_assert(std::same_as<decltype(s), span<const uint8_t, 5u>>);
    EXPECT_EQ(s[0u], 'h');
    EXPECT_EQ(s[1u], 'e');
    EXPECT_EQ(s[4u], 'o');
  }
  // Includes the terminating null, size known at compile time. Converted to a
  // span of uint8_t bytes.
  {
    auto s = base::byte_span_with_nul_from_cstring("hello");
    static_assert(std::same_as<decltype(s), span<const uint8_t, 6u>>);
    EXPECT_EQ(s[0u], 'h');
    EXPECT_EQ(s[1u], 'e');
    EXPECT_EQ(s[4u], 'o');
    EXPECT_EQ(s[5u], '\0');
  }
}

TEST(SpanTest, FromCStringEmpty) {
  // No terminating null, size known at compile time.
  {
    auto s = base::span_from_cstring("");
    static_assert(std::same_as<decltype(s), span<const char, 0u>>);
    EXPECT_EQ(s.size(), 0u);
  }
  // No terminating null, size not known at compile time. string_view loses
  // the size.
  {
    auto s = base::span(std::string_view(""));
    static_assert(std::same_as<decltype(s), span<const char>>);
    EXPECT_EQ(s.size(), 0u);
  }
  // Includes the terminating null, size known at compile time.
  {
    auto s = base::span_with_nul_from_cstring("");
    static_assert(std::same_as<decltype(s), span<const char, 1u>>);
    ASSERT_EQ(s.size(), 1u);
    EXPECT_EQ(s[0u], '\0');
  }
  // No terminating null, size known at compile time. Converted to a span of
  // uint8_t bytes.
  {
    auto s = base::byte_span_from_cstring("");
    static_assert(std::same_as<decltype(s), span<const uint8_t, 0u>>);
    ASSERT_EQ(s.size(), 0u);
  }
  // Includes the terminating null, size known at compile time. Converted to a
  // span of uint8_t bytes.
  {
    auto s = base::byte_span_with_nul_from_cstring("");
    static_assert(std::same_as<decltype(s), span<const uint8_t, 1u>>);
    ASSERT_EQ(s.size(), 1u);
    EXPECT_EQ(s[0u], '\0');
  }
}

TEST(SpanTest, FromCStringEmbeddedNul) {
  // No terminating null, size known at compile time.
  {
    auto s = base::span_from_cstring("h\0\0\0o");
    static_assert(std::same_as<decltype(s), span<const char, 5u>>);
    EXPECT_THAT(s, ElementsAre('h', '\0', '\0', '\0', 'o'));
  }
  // No terminating null, size not known at compile time. string_view loses
  // the size, and stops at embedded NUL. Beware.
  {
    auto s = base::span(std::string_view("h\0\0\0o"));
    static_assert(std::same_as<decltype(s), span<const char>>);
    EXPECT_THAT(s, ElementsAre('h'));
  }
  // Includes the terminating null, size known at compile time.
  {
    auto s = base::span_with_nul_from_cstring("h\0\0\0o");
    static_assert(std::same_as<decltype(s), span<const char, 6u>>);
    EXPECT_THAT(s, ElementsAre('h', '\0', '\0', '\0', 'o', '\0'));
  }

  // No terminating null, size known at compile time. Converted to a span of
  // uint8_t bytes.
  {
    auto s = base::byte_span_from_cstring("h\0\0\0o");
    static_assert(std::same_as<decltype(s), span<const uint8_t, 5u>>);
    EXPECT_THAT(s, ElementsAre('h', '\0', '\0', '\0', 'o'));
  }
  // Includes the terminating null, size known at compile time. Converted to a
  // span of uint8_t bytes.
  {
    auto s = base::byte_span_with_nul_from_cstring("h\0\0\0o");
    static_assert(std::same_as<decltype(s), span<const uint8_t, 6u>>);
    EXPECT_THAT(s, ElementsAre('h', '\0', '\0', '\0', 'o', '\0'));
  }
}

TEST(SpanTest, FromCStringOtherTypes) {
  {
    auto s = base::span_from_cstring("hello");
    static_assert(std::same_as<decltype(s), span<const char, 5u>>);
    EXPECT_EQ(s[0u], 'h');
    EXPECT_EQ(s[1u], 'e');
    EXPECT_EQ(s[4u], 'o');
  }
  {
    auto s = base::span_from_cstring(L"hello");
    static_assert(std::same_as<decltype(s), span<const wchar_t, 5u>>);
    EXPECT_EQ(s[0u], L'h');
    EXPECT_EQ(s[1u], L'e');
    EXPECT_EQ(s[4u], L'o');
  }
  {
    auto s = base::span_from_cstring(u"hello");
    static_assert(std::same_as<decltype(s), span<const char16_t, 5u>>);
    EXPECT_EQ(s[0u], u'h');
    EXPECT_EQ(s[1u], u'e');
    EXPECT_EQ(s[4u], u'o');
  }
  {
    auto s = base::span_from_cstring(U"hello");
    static_assert(std::same_as<decltype(s), span<const char32_t, 5u>>);
    EXPECT_EQ(s[0u], U'h');
    EXPECT_EQ(s[1u], U'e');
    EXPECT_EQ(s[4u], U'o');
  }
}

TEST(SpanTest, ConvertNonConstIntegralToConst) {
  std::vector<int> vector = {1, 1, 2, 3, 5, 8};

  // SAFETY: `vector.size()` describes valid portion of `vector.data()`.
  span<int> UNSAFE_BUFFERS(int_span(vector.data(), vector.size()));
  span<const int> const_span(int_span);
  EXPECT_EQ(int_span.size(), const_span.size());

  EXPECT_THAT(const_span, Pointwise(Eq(), int_span));

  // SAFETY: `vector.size()` describes valid portion of `vector.data()`.
  span<int, 6> UNSAFE_BUFFERS(static_int_span(vector.data(), vector.size()));
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

  // SAFETY: `vector.size()` describes valid portion of `vector.data()`.
  span<int*, 3> UNSAFE_BUFFERS(
      static_non_const_pointer_span(vector.data(), vector.size()));
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

  // SAFETY: `vector.size()` describes valid portion of `vector.data()`.
  span<int32_t, 5> UNSAFE_BUFFERS(
      static_int32_t_span(vector.data(), vector.size()));
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
    // SAFETY: static_assert() doesn't execute code at runtime.
    static_assert(UNSAFE_BUFFERS(span.data() + 3) == subspan.data(), "");
    static_assert(0u == subspan.size(), "");
    static_assert(0u == decltype(subspan)::extent, "");
  }

  {
    constexpr auto subspan = span.last<1>();
    // SAFETY: static_assert() doesn't execute code at runtime.
    static_assert(UNSAFE_BUFFERS(span.data() + 2) == subspan.data(), "");
    static_assert(1u == subspan.size(), "");
    static_assert(1u == decltype(subspan)::extent, "");
    static_assert(3 == subspan[0], "");
  }

  {
    constexpr auto subspan = span.last<2>();
    // SAFETY: static_assert() doesn't execute code at runtime.
    static_assert(UNSAFE_BUFFERS(span.data() + 1) == subspan.data(), "");
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
    // SAFETY: static_assert() doesn't execute code at runtime.
    static_assert(UNSAFE_BUFFERS(span.data() + 1) == subspan.data(), "");
    static_assert(2u == subspan.size(), "");
    static_assert(2u == decltype(subspan)::extent, "");
    static_assert(2 == subspan[0], "");
    static_assert(3 == subspan[1], "");
  }

  {
    constexpr auto subspan = span.subspan<2>();
    // SAFETY: static_assert() doesn't execute code at runtime.
    static_assert(UNSAFE_BUFFERS(span.data() + 2) == subspan.data(), "");
    static_assert(1u == subspan.size(), "");
    static_assert(1u == decltype(subspan)::extent, "");
    static_assert(3 == subspan[0], "");
  }

  {
    constexpr auto subspan = span.subspan<3>();
    // SAFETY: static_assert() doesn't execute code at runtime.
    static_assert(UNSAFE_BUFFERS(span.data() + 3) == subspan.data(), "");
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
    // SAFETY: static_assert() doesn't execute code at runtime.
    static_assert(UNSAFE_BUFFERS(span.data() + 1) == subspan.data(), "");
    static_assert(0u == subspan.size(), "");
    static_assert(0u == decltype(subspan)::extent, "");
  }

  {
    constexpr auto subspan = span.subspan<2, 0>();
    // SAFETY: static_assert() doesn't execute code at runtime.
    static_assert(UNSAFE_BUFFERS(span.data() + 2) == subspan.data(), "");
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
    // SAFETY: static_assert() doesn't execute code at runtime.
    static_assert(UNSAFE_BUFFERS(span.data() + 1) == subspan.data(), "");
    static_assert(1u == subspan.size(), "");
    static_assert(1u == decltype(subspan)::extent, "");
    static_assert(2 == subspan[0], "");
  }

  {
    constexpr auto subspan = span.subspan<2, 1>();
    // SAFETY: static_assert() doesn't execute code at runtime.
    static_assert(UNSAFE_BUFFERS(span.data() + 2) == subspan.data(), "");
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
    // SAFETY: static_assert() doesn't execute code at runtime.
    static_assert(UNSAFE_BUFFERS(span.data() + 1) == subspan.data(), "");
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
  std::array<int, 3> array = {1, 2, 3};
  span<const int> const_span(array);
  for (size_t i = 0; i < const_span.size(); ++i) {
    // SAFETY: The range starting at `const_span.begin()` is valid up
    // to `const_span.size()`.
    EXPECT_EQ(array[i], UNSAFE_BUFFERS(const_span.begin()[i]));
  }

  span<int> mutable_span(array);
  for (size_t i = 0; i < mutable_span.size(); ++i) {
    // SAFETY: The range starting at `mutable_span.begin()` is valid up
    // to `mutable_span.size()`.
    EXPECT_EQ(array[i], UNSAFE_BUFFERS(mutable_span.begin()[i]));
  }
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
    // `array` has three elmenents, so `span` has three elements, so
    // `span.data() + 3` points to one byte beyond the object as allowed
    // per standards.
    EXPECT_EQ(UNSAFE_BUFFERS(span.data() + 3), subspan.data());
    EXPECT_EQ(0u, subspan.size());
    static_assert(0u == decltype(subspan)::extent, "");
  }

  {
    auto subspan = span.last<1>();
    // `array` has three elmenents, so `span` has three elements, so
    // `span.data() + 2` points within it.
    EXPECT_EQ(UNSAFE_BUFFERS(span.data() + 2), subspan.data());
    EXPECT_EQ(1u, subspan.size());
    static_assert(1u == decltype(subspan)::extent, "");
    EXPECT_EQ(3, subspan[0]);
  }

  {
    auto subspan = span.last<2>();
    // `array` has three elmenents, so `span` has three elements, so
    // `span.data() + 1` points within it.
    EXPECT_EQ(UNSAFE_BUFFERS(span.data() + 1), subspan.data());
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
    // SAFETY: `array` has three elmenents, so `span` has three elements, so
    // `span.data() + 1` points within it.
    EXPECT_EQ(UNSAFE_BUFFERS(span.data() + 1), subspan.data());
    EXPECT_EQ(2u, subspan.size());
    static_assert(2u == decltype(subspan)::extent, "");
    EXPECT_EQ(2, subspan[0]);
    EXPECT_EQ(3, subspan[1]);
  }

  {
    auto subspan = span.subspan<2>();
    // SAFETY: `array` has three elmenents, so `span` has three elements, so
    // `span.data() + 2` points within it.
    EXPECT_EQ(UNSAFE_BUFFERS(span.data() + 2), subspan.data());
    EXPECT_EQ(1u, subspan.size());
    static_assert(1u == decltype(subspan)::extent, "");
    EXPECT_EQ(3, subspan[0]);
  }

  {
    auto subspan = span.subspan<3>();
    // SAFETY: `array` has three elmenents, so `span` has three elements, so
    // `span.data() + 3` points to one byte beyond the object as permitted by
    // C++ specification.
    EXPECT_EQ(UNSAFE_BUFFERS(span.data() + 3), subspan.data());
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
    // SAFETY: `array` has three elmenents, so `span` has three elements, so
    // `span.data() + 1` points within it.
    EXPECT_EQ(UNSAFE_BUFFERS(span.data() + 1), subspan.data());
    EXPECT_EQ(0u, subspan.size());
    static_assert(0u == decltype(subspan)::extent, "");
  }

  {
    auto subspan = span.subspan<2, 0>();
    // SAFETY: `array` has three elmenents, so `span` has three elements, so
    // `span.data() + 2` points within it.
    EXPECT_EQ(UNSAFE_BUFFERS(span.data() + 2), subspan.data());
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
    // SAFETY: `array` has three elmenents, so `span` has three elements, so
    // `span.data() + 1` points within it.
    EXPECT_EQ(UNSAFE_BUFFERS(span.data() + 1), subspan.data());
    EXPECT_EQ(1u, subspan.size());
    static_assert(1u == decltype(subspan)::extent, "");
    EXPECT_EQ(2, subspan[0]);
  }

  {
    auto subspan = span.subspan<2, 1>();
    // SAFETY: `array` has three elmenents, so `span` has three elements, so
    // `span.data() + 2` points within it.
    EXPECT_EQ(UNSAFE_BUFFERS(span.data() + 2), subspan.data());
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
    // SAFETY: `array` has three elmenents, so `span` has three elements, so
    // `span.data() + 1` points within it.
    EXPECT_EQ(UNSAFE_BUFFERS(span.data() + 1), subspan.data());
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
    auto subspan = span.first(0u);
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_EQ(0u, subspan.size());
  }

  {
    auto subspan = span.first(1u);
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_EQ(1u, subspan.size());
    EXPECT_EQ(1, subspan[0]);
  }

  {
    auto subspan = span.first(2u);
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_EQ(2u, subspan.size());
    EXPECT_EQ(1, subspan[0]);
    EXPECT_EQ(2, subspan[1]);
  }

  {
    auto subspan = span.first(3u);
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
    auto subspan = span.last(0u);
    // SAFETY: `array` has three elmenents, so `span` has three elements, so
    // `span.data() + 3` points to one byte beyond the object, as permitted by
    // C++ specification.
    EXPECT_EQ(UNSAFE_BUFFERS(span.data() + 3), subspan.data());
    EXPECT_EQ(0u, subspan.size());
  }

  {
    auto subspan = span.last(1u);
    // SAFETY: `array` has three elmenents, so `span` has three elements, so
    // `span.data() + 2` points within it.
    EXPECT_EQ(UNSAFE_BUFFERS(span.data() + 2), subspan.data());
    EXPECT_EQ(1u, subspan.size());
    EXPECT_EQ(3, subspan[0]);
  }

  {
    auto subspan = span.last(2u);
    // SAFETY: `array` has three elmenents, so `span` has three elements, so
    // `span.data() + 1` points within it.
    EXPECT_EQ(UNSAFE_BUFFERS(span.data() + 1), subspan.data());
    EXPECT_EQ(2u, subspan.size());
    EXPECT_EQ(2, subspan[0]);
    EXPECT_EQ(3, subspan[1]);
  }

  {
    auto subspan = span.last(3u);
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
    // SAFETY: `array` has three elmenents, so `span` has three elements, so
    // `span.data() + 1` points within it.
    EXPECT_EQ(UNSAFE_BUFFERS(span.data() + 1), subspan.data());
    EXPECT_EQ(2u, subspan.size());
    EXPECT_EQ(2, subspan[0]);
    EXPECT_EQ(3, subspan[1]);
  }

  {
    auto subspan = span.subspan(2);
    // SAFETY: `array` has three elmenents, so `span` has three elements, so
    // `span.data() + 2` points within it.
    EXPECT_EQ(UNSAFE_BUFFERS(span.data() + 2), subspan.data());
    EXPECT_EQ(1u, subspan.size());
    EXPECT_EQ(3, subspan[0]);
  }

  {
    auto subspan = span.subspan(3);
    // SAFETY: `array` has three elmenents, so `span` has three elements, so
    // `span.data() + 3` points to one byte beyond the object, as permitted by
    // C++ specification.
    EXPECT_EQ(UNSAFE_BUFFERS(span.data() + 3), subspan.data());
    EXPECT_EQ(0u, subspan.size());
  }

  {
    auto subspan = span.subspan(0, 0);
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_EQ(0u, subspan.size());
  }

  {
    auto subspan = span.subspan(1, 0);
    // SAFETY: `array` has three elmenents, so `span` has three elements, so
    // `span.data() + 1` points within it.
    EXPECT_EQ(UNSAFE_BUFFERS(span.data() + 1), subspan.data());
    EXPECT_EQ(0u, subspan.size());
  }

  {
    auto subspan = span.subspan(2, 0);
    // SAFETY: `array` has three elmenents, so `span` has three elements, so
    // `span.data() + 2` points within it.
    EXPECT_EQ(UNSAFE_BUFFERS(span.data() + 2), subspan.data());
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
    // SAFETY: `array` has three elmenents, so `span` has three elements, so
    // `span.data() + 1` points within it.
    EXPECT_EQ(UNSAFE_BUFFERS(span.data() + 1), subspan.data());
    EXPECT_EQ(1u, subspan.size());
    EXPECT_EQ(2, subspan[0]);
  }

  {
    auto subspan = span.subspan(2, 1);
    // SAFETY: `array` has three elmenents, so `span` has three elements, so
    // `span.data() + 2` points within it.
    EXPECT_EQ(UNSAFE_BUFFERS(span.data() + 2), subspan.data());
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
    // SAFETY: `array` has three elmenents, so `span` has three elements, so
    // `span.data() + 1` points within it.
    EXPECT_EQ(UNSAFE_BUFFERS(span.data() + 1), subspan.data());
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

TEST(SpanTest, ToFixedExtent) {
  {
    const int kArray[] = {1, 2, 3};
    const span<const int> s(kArray);

    auto static_span = s.to_fixed_extent<3>();
    ASSERT_TRUE(static_span.has_value());
    static_assert(std::same_as<typename decltype(static_span)::value_type,
                               span<const int, 3>>);
    EXPECT_EQ(s.data(), static_span->data());
    EXPECT_EQ(s.size(), static_span->size());

    EXPECT_EQ(std::nullopt, s.to_fixed_extent<4>());
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

  {
    std::vector<int> vector = {1, 2, 3};
    span<int> s = vector;
    // SAFETY: The empty range at end of a vector is a valid range.
    span<int> span_of_checked_iterators = UNSAFE_BUFFERS({s.end(), s.end()});
    EXPECT_TRUE(span_of_checked_iterators.empty());
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
  for (int i : span) {
    results.emplace_back(i);
  }
  EXPECT_THAT(results, ElementsAre(1, 6, 1, 8, 0));
}

TEST(SpanTest, ConstexprIterator) {
  static constexpr int kArray[] = {1, 6, 1, 8, 0};
  constexpr span<const int> span(kArray);

  static_assert(ranges::equal(kArray, span), "");
  static_assert(1 == span.begin()[0], "");
  static_assert(1 == *(span.begin() += 0), "");
  static_assert(6 == *(span.begin() += 1), "");

  static_assert(1 == *((span.begin() + 1) -= 1), "");
  static_assert(6 == *((span.begin() + 1) -= 0), "");

  static_assert(0 + span.begin() == span.begin() + 0);
  static_assert(1 + span.begin() == span.begin() + 1);
}

TEST(SpanTest, ReverseIterator) {
  static constexpr int kArray[] = {1, 6, 1, 8, 0};
  constexpr span<const int> span(kArray);

  EXPECT_TRUE(ranges::equal(Reversed(kArray), Reversed(span)));
}

TEST(SpanTest, AsBytes) {
  {
    constexpr int kArray[] = {2, 3, 5, 7, 11, 13};
    auto bytes_span = as_bytes(make_span(kArray));
    static_assert(std::is_same_v<decltype(bytes_span),
                                 base::span<const uint8_t, sizeof(kArray)>>);
    EXPECT_EQ(reinterpret_cast<const uint8_t*>(kArray), bytes_span.data());
    EXPECT_EQ(sizeof(kArray), bytes_span.size());
    EXPECT_EQ(bytes_span.size(), bytes_span.size_bytes());
  }
  {
    std::vector<int> vec = {1, 1, 2, 3, 5, 8};
    span<int> mutable_span(vec);
    auto bytes_span = as_bytes(mutable_span);
    static_assert(
        std::is_same_v<decltype(bytes_span), base::span<const uint8_t>>);
    EXPECT_EQ(reinterpret_cast<const uint8_t*>(vec.data()), bytes_span.data());
    EXPECT_EQ(sizeof(int) * vec.size(), bytes_span.size());
    EXPECT_EQ(bytes_span.size(), bytes_span.size_bytes());
  }
}

TEST(SpanTest, AsWritableBytes) {
  {
    std::vector<int> vec = {1, 1, 2, 3, 5, 8};
    span<int> mutable_span(vec);
    auto writable_bytes_span = as_writable_bytes(mutable_span);
    static_assert(
        std::is_same_v<decltype(writable_bytes_span), base::span<uint8_t>>);
    EXPECT_EQ(reinterpret_cast<uint8_t*>(vec.data()),
              writable_bytes_span.data());
    EXPECT_EQ(sizeof(int) * vec.size(), writable_bytes_span.size());
    EXPECT_EQ(writable_bytes_span.size(), writable_bytes_span.size_bytes());

    // Set the first entry of vec by writing through the span.
    std::ranges::fill(writable_bytes_span.first(sizeof(int)), 'a');
    static_assert(sizeof(int) == 4u);  // Otherwise char literal wrong below.
    EXPECT_EQ('aaaa', vec[0]);
  }
  {
    std::vector<int> vec = {1, 1, 2, 3, 5, 8};
    raw_span<int> mutable_raw_span(vec);
    auto writable_bytes_span = as_writable_bytes(mutable_raw_span);
    static_assert(
        std::is_same_v<decltype(writable_bytes_span), base::span<uint8_t>>);
    EXPECT_EQ(reinterpret_cast<uint8_t*>(vec.data()),
              writable_bytes_span.data());
    EXPECT_EQ(sizeof(int) * vec.size(), writable_bytes_span.size());
    EXPECT_EQ(writable_bytes_span.size(), writable_bytes_span.size_bytes());

    // Set the first entry of vec by writing through the span.
    std::ranges::fill(writable_bytes_span.first(sizeof(int)), 'a');
    static_assert(sizeof(int) == 4u);  // Otherwise char literal wrong below.
    EXPECT_EQ('aaaa', vec[0]);
  }
}

TEST(SpanTest, AsChars) {
  {
    constexpr int kArray[] = {2, 3, 5, 7, 11, 13};
    auto chars_span = as_chars(make_span(kArray));
    static_assert(std::is_same_v<decltype(chars_span),
                                 base::span<const char, sizeof(kArray)>>);
    EXPECT_EQ(reinterpret_cast<const char*>(kArray), chars_span.data());
    EXPECT_EQ(sizeof(kArray), chars_span.size());
    EXPECT_EQ(chars_span.size(), chars_span.size_bytes());
  }
  {
    std::vector<int> vec = {1, 1, 2, 3, 5, 8};
    span<int> mutable_span(vec);
    auto chars_span = as_chars(mutable_span);
    static_assert(std::is_same_v<decltype(chars_span), base::span<const char>>);
    EXPECT_EQ(reinterpret_cast<const char*>(vec.data()), chars_span.data());
    EXPECT_EQ(sizeof(int) * vec.size(), chars_span.size());
    EXPECT_EQ(chars_span.size(), chars_span.size_bytes());
  }
  {
    std::vector<int> vec = {1, 1, 2, 3, 5, 8};
    raw_span<int> mutable_span(vec);
    auto chars_span = as_chars(mutable_span);
    static_assert(std::is_same_v<decltype(chars_span), base::span<const char>>);
    EXPECT_EQ(reinterpret_cast<const char*>(vec.data()), chars_span.data());
    EXPECT_EQ(sizeof(int) * vec.size(), chars_span.size());
    EXPECT_EQ(chars_span.size(), chars_span.size_bytes());
  }
}

TEST(SpanTest, AsWritableChars) {
  {
    std::vector<int> vec = {1, 1, 2, 3, 5, 8};
    span<int> mutable_span(vec);
    auto writable_chars_span = as_writable_chars(mutable_span);
    static_assert(
        std::is_same_v<decltype(writable_chars_span), base::span<char>>);
    EXPECT_EQ(reinterpret_cast<char*>(vec.data()), writable_chars_span.data());
    EXPECT_EQ(sizeof(int) * vec.size(), writable_chars_span.size());
    EXPECT_EQ(writable_chars_span.size(), writable_chars_span.size_bytes());

    // Set the first entry of vec by writing through the span.
    std::ranges::fill(writable_chars_span.first(sizeof(int)), 'a');
    static_assert(sizeof(int) == 4u);  // Otherwise char literal wrong below.
    EXPECT_EQ('aaaa', vec[0]);
  }
  {
    std::vector<int> vec = {1, 1, 2, 3, 5, 8};
    raw_span<int> mutable_span(vec);
    auto writable_chars_span = as_writable_chars(mutable_span);
    static_assert(
        std::is_same_v<decltype(writable_chars_span), base::span<char>>);
    EXPECT_EQ(reinterpret_cast<char*>(vec.data()), writable_chars_span.data());
    EXPECT_EQ(sizeof(int) * vec.size(), writable_chars_span.size());
    EXPECT_EQ(writable_chars_span.size(), writable_chars_span.size_bytes());

    // Set the first entry of vec by writing through the span.
    std::ranges::fill(writable_chars_span.first(sizeof(int)), 'a');
    static_assert(sizeof(int) == 4u);  // Otherwise char literal wrong below.
    EXPECT_EQ('aaaa', vec[0]);
  }
}

TEST(SpanTest, AsByteSpan) {
  {
    constexpr int kArray[] = {2, 3, 5, 7, 11, 13};
    auto byte_span = as_byte_span(kArray);
    static_assert(std::is_same_v<decltype(byte_span),
                                 span<const uint8_t, 6u * sizeof(int)>>);
    EXPECT_EQ(byte_span.data(), reinterpret_cast<const uint8_t*>(kArray));
    EXPECT_EQ(byte_span.size(), sizeof(kArray));
  }
  {
    const std::vector<int> kVec({2, 3, 5, 7, 11, 13});
    auto byte_span = as_byte_span(kVec);
    static_assert(std::is_same_v<decltype(byte_span), span<const uint8_t>>);
    EXPECT_EQ(byte_span.data(), reinterpret_cast<const uint8_t*>(kVec.data()));
    EXPECT_EQ(byte_span.size(), kVec.size() * sizeof(int));
  }
  {
    int kMutArray[] = {2, 3, 5, 7};
    auto byte_span = as_byte_span(kMutArray);
    static_assert(std::is_same_v<decltype(byte_span),
                                 span<const uint8_t, 4u * sizeof(int)>>);
    EXPECT_EQ(byte_span.data(), reinterpret_cast<const uint8_t*>(kMutArray));
    EXPECT_EQ(byte_span.size(), sizeof(kMutArray));
  }
  {
    std::vector<int> kMutVec({2, 3, 5, 7});
    auto byte_span = as_byte_span(kMutVec);
    static_assert(std::is_same_v<decltype(byte_span), span<const uint8_t>>);
    EXPECT_EQ(byte_span.data(),
              reinterpret_cast<const uint8_t*>(kMutVec.data()));
    EXPECT_EQ(byte_span.size(), kMutVec.size() * sizeof(int));
  }
  // Rvalue input.
  {
    [](auto byte_span) {
      static_assert(std::is_same_v<decltype(byte_span),
                                   span<const uint8_t, 6u * sizeof(int)>>);
      EXPECT_EQ(byte_span.size(), 6u * sizeof(int));
      // Little endian puts the low bits in the first byte.
      EXPECT_EQ(byte_span[0u], 2);
    }(as_byte_span({2, 3, 5, 7, 11, 13}));
  }
}

TEST(SpanTest, AsWritableByteSpan) {
  {
    int kMutArray[] = {2, 3, 5, 7};
    auto byte_span = as_writable_byte_span(kMutArray);
    static_assert(
        std::is_same_v<decltype(byte_span), span<uint8_t, 4u * sizeof(int)>>);
    EXPECT_EQ(byte_span.data(), reinterpret_cast<uint8_t*>(kMutArray));
    EXPECT_EQ(byte_span.size(), sizeof(kMutArray));
  }
  {
    std::vector<int> kMutVec({2, 3, 5, 7});
    auto byte_span = as_writable_byte_span(kMutVec);
    static_assert(std::is_same_v<decltype(byte_span), span<uint8_t>>);
    EXPECT_EQ(byte_span.data(), reinterpret_cast<uint8_t*>(kMutVec.data()));
    EXPECT_EQ(byte_span.size(), kMutVec.size() * sizeof(int));
  }
  // Rvalue input.
  {
    [](auto byte_span) {
      static_assert(
          std::is_same_v<decltype(byte_span), span<uint8_t, 6u * sizeof(int)>>);
      EXPECT_EQ(byte_span.size(), 6u * sizeof(int));
      // Little endian puts the low bits in the first byte.
      EXPECT_EQ(byte_span[0u], 2);
    }(as_writable_byte_span({2, 3, 5, 7, 11, 13}));
  }
}

TEST(SpanTest, AsStringView) {
  {
    constexpr uint8_t kArray[] = {'h', 'e', 'l', 'l', 'o'};
    // Fixed size span.
    auto s = as_string_view(kArray);
    static_assert(std::is_same_v<decltype(s), std::string_view>);
    EXPECT_EQ(s.data(), reinterpret_cast<const char*>(&kArray[0u]));
    EXPECT_EQ(s.size(), std::size(kArray));

    // Dynamic size span.
    auto s2 = as_string_view(base::span<const uint8_t>(kArray));
    static_assert(std::is_same_v<decltype(s2), std::string_view>);
    EXPECT_EQ(s2.data(), reinterpret_cast<const char*>(&kArray[0u]));
    EXPECT_EQ(s2.size(), std::size(kArray));
  }
  {
    constexpr char kArray[] = {'h', 'e', 'l', 'l', 'o'};
    // Fixed size span.
    auto s = as_string_view(kArray);
    static_assert(std::is_same_v<decltype(s), std::string_view>);
    EXPECT_EQ(s.data(), &kArray[0u]);
    EXPECT_EQ(s.size(), std::size(kArray));

    // Dynamic size span.
    auto s2 = as_string_view(base::span<const char>(kArray));
    static_assert(std::is_same_v<decltype(s2), std::string_view>);
    EXPECT_EQ(s2.data(), &kArray[0u]);
    EXPECT_EQ(s2.size(), std::size(kArray));
  }
}

TEST(SpanTest, MakeSpanFromDataAndSize) {
  int* nullint = nullptr;
  // SAFETY: zero size is correct when pointer is NULL.
  auto empty_span = UNSAFE_BUFFERS(make_span(nullint, 0u));
  EXPECT_TRUE(empty_span.empty());
  EXPECT_EQ(nullptr, empty_span.data());

  std::vector<int> vector = {1, 1, 2, 3, 5, 8};
  // SAFETY: vector.size() describes valid portion of vector.data().
  span<int> UNSAFE_BUFFERS(expected_span(vector.data(), vector.size()));
  auto made_span = UNSAFE_BUFFERS(make_span(vector.data(), vector.size()));
  EXPECT_EQ(expected_span.data(), made_span.data());
  EXPECT_EQ(expected_span.size(), made_span.size());
  static_assert(decltype(made_span)::extent == dynamic_extent, "");
  static_assert(std::is_same_v<decltype(expected_span), decltype(made_span)>,
                "the type of made_span differs from expected_span!");
}

TEST(SpanTest, MakeSpanFromPointerPair) {
  int* nullint = nullptr;
  // SAFETY: The empty range between NULL and NULL is valid range.
  auto empty_span = UNSAFE_BUFFERS(make_span(nullint, nullint));
  EXPECT_TRUE(empty_span.empty());
  EXPECT_EQ(nullptr, empty_span.data());

  std::vector<int> vector = {1, 1, 2, 3, 5, 8};
  // SAFETY: `vector.size()` describes valid portion of `vector.data()`.
  span<int> UNSAFE_BUFFERS(expected_span(vector.data(), vector.size()));
  auto made_span =
      UNSAFE_BUFFERS(make_span(vector.data(), vector.data() + vector.size()));
  EXPECT_EQ(expected_span.data(), made_span.data());
  EXPECT_EQ(expected_span.size(), made_span.size());
  static_assert(decltype(made_span)::extent == dynamic_extent, "");
  static_assert(std::is_same_v<decltype(expected_span), decltype(made_span)>,
                "the type of made_span differs from expected_span!");
}

TEST(SpanTest, MakeSpanFromConstexprArray) {
  static constexpr int kArray[] = {1, 2, 3, 4, 5};
  constexpr span<const int, 5> expected_span(kArray);
  constexpr auto made_span = make_span(kArray);
  EXPECT_EQ(expected_span.data(), made_span.data());
  EXPECT_EQ(expected_span.size(), made_span.size());
  static_assert(decltype(made_span)::extent == 5, "");
  static_assert(std::is_same_v<decltype(expected_span), decltype(made_span)>,
                "the type of made_span differs from expected_span!");
}

TEST(SpanTest, MakeSpanFromStdArray) {
  const std::array<int, 5> kArray = {{1, 2, 3, 4, 5}};
  span<const int, 5> expected_span(kArray);
  auto made_span = make_span(kArray);
  EXPECT_EQ(expected_span.data(), made_span.data());
  EXPECT_EQ(expected_span.size(), made_span.size());
  static_assert(decltype(made_span)::extent == 5, "");
  static_assert(std::is_same_v<decltype(expected_span), decltype(made_span)>,
                "the type of made_span differs from expected_span!");
}

TEST(SpanTest, MakeSpanFromConstContainer) {
  const std::vector<int> vector = {-1, -2, -3, -4, -5};
  span<const int> expected_span(vector);
  auto made_span = make_span(vector);
  EXPECT_EQ(expected_span.data(), made_span.data());
  EXPECT_EQ(expected_span.size(), made_span.size());
  static_assert(decltype(made_span)::extent == dynamic_extent, "");
  static_assert(std::is_same_v<decltype(expected_span), decltype(made_span)>,
                "the type of made_span differs from expected_span!");
}

TEST(SpanTest, MakeSpanFromContainer) {
  std::vector<int> vector = {-1, -2, -3, -4, -5};
  span<int> expected_span(vector);
  auto made_span = make_span(vector);
  EXPECT_EQ(expected_span.data(), made_span.data());
  EXPECT_EQ(expected_span.size(), made_span.size());
  static_assert(decltype(made_span)::extent == dynamic_extent, "");
  static_assert(std::is_same_v<decltype(expected_span), decltype(made_span)>,
                "the type of made_span differs from expected_span!");
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
  static_assert(std::is_same_v<decltype(expected_span), decltype(made_span)>,
                "the type of made_span differs from expected_span!");
}

TEST(SpanTest, MakeSpanFromDynamicSpan) {
  static constexpr int kArray[] = {1, 2, 3, 4, 5};
  constexpr span<const int> expected_span(kArray);
  constexpr auto made_span = make_span(expected_span);
  static_assert(std::is_same_v<decltype(expected_span)::element_type,
                               decltype(made_span)::element_type>,
                "make_span(span) should have the same element_type as span");

  static_assert(expected_span.data() == made_span.data(),
                "make_span(span) should have the same data() as span");

  static_assert(expected_span.size() == made_span.size(),
                "make_span(span) should have the same size() as span");

  static_assert(decltype(made_span)::extent == decltype(expected_span)::extent,
                "make_span(span) should have the same extent as span");

  static_assert(std::is_same_v<decltype(expected_span), decltype(made_span)>,
                "the type of made_span differs from expected_span!");
}

TEST(SpanTest, MakeSpanFromStaticSpan) {
  static constexpr int kArray[] = {1, 2, 3, 4, 5};
  constexpr span<const int, 5> expected_span(kArray);
  constexpr auto made_span = make_span(expected_span);
  static_assert(std::is_same_v<decltype(expected_span)::element_type,
                               decltype(made_span)::element_type>,
                "make_span(span) should have the same element_type as span");

  static_assert(expected_span.data() == made_span.data(),
                "make_span(span) should have the same data() as span");

  static_assert(expected_span.size() == made_span.size(),
                "make_span(span) should have the same size() as span");

  static_assert(decltype(made_span)::extent == decltype(expected_span)::extent,
                "make_span(span) should have the same extent as span");

  static_assert(std::is_same_v<decltype(expected_span), decltype(made_span)>,
                "the type of made_span differs from expected_span!");
}

TEST(SpanTest, EnsureConstexprGoodness) {
  static constexpr std::array<int, 5> kArray = {5, 4, 3, 2, 1};
  constexpr span<const int> constexpr_span(kArray);
  const size_t size = 2;

  const size_t start = 1;
  constexpr span<const int> subspan =
      constexpr_span.subspan(start, start + size);
  for (size_t i = 0; i < subspan.size(); ++i) {
    EXPECT_EQ(kArray[start + i], subspan[i]);
  }

  constexpr span<const int> firsts = constexpr_span.first(size);
  for (size_t i = 0; i < firsts.size(); ++i) {
    EXPECT_EQ(kArray[i], firsts[i]);
  }

  constexpr span<const int> lasts = constexpr_span.last(size);
  for (size_t i = 0; i < lasts.size(); ++i) {
    const size_t j = (std::size(kArray) - size) + i;
    EXPECT_EQ(kArray[j], lasts[i]);
  }

  constexpr int item = constexpr_span[size];
  EXPECT_EQ(kArray[size], item);
}

TEST(SpanTest, OutOfBoundsDeath) {
  constexpr span<int, 0> kEmptySpan;
  ASSERT_DEATH_IF_SUPPORTED(kEmptySpan[0], "");
  ASSERT_DEATH_IF_SUPPORTED(kEmptySpan.first(1u), "");
  ASSERT_DEATH_IF_SUPPORTED(kEmptySpan.last(1u), "");
  ASSERT_DEATH_IF_SUPPORTED(kEmptySpan.subspan(1u), "");

  constexpr span<int> kEmptyDynamicSpan;
  ASSERT_DEATH_IF_SUPPORTED(kEmptyDynamicSpan[0], "");
  ASSERT_DEATH_IF_SUPPORTED(kEmptyDynamicSpan.front(), "");
  ASSERT_DEATH_IF_SUPPORTED(kEmptyDynamicSpan.first(1u), "");
  ASSERT_DEATH_IF_SUPPORTED(kEmptyDynamicSpan.last(1u), "");
  ASSERT_DEATH_IF_SUPPORTED(kEmptyDynamicSpan.back(), "");
  ASSERT_DEATH_IF_SUPPORTED(kEmptyDynamicSpan.subspan(1), "");

  static constexpr int kArray[] = {0, 1, 2};
  constexpr span<const int> kNonEmptyDynamicSpan(kArray);
  EXPECT_EQ(3U, kNonEmptyDynamicSpan.size());
  ASSERT_DEATH_IF_SUPPORTED(kNonEmptyDynamicSpan[4], "");
  ASSERT_DEATH_IF_SUPPORTED(kNonEmptyDynamicSpan.subspan(10), "");
  ASSERT_DEATH_IF_SUPPORTED(kNonEmptyDynamicSpan.subspan(1, 7), "");

  size_t minus_one = static_cast<size_t>(-1);
  ASSERT_DEATH_IF_SUPPORTED(kNonEmptyDynamicSpan.subspan(minus_one), "");
  ASSERT_DEATH_IF_SUPPORTED(kNonEmptyDynamicSpan.subspan(minus_one, minus_one),
                            "");
  ASSERT_DEATH_IF_SUPPORTED(kNonEmptyDynamicSpan.subspan(minus_one, 1), "");

  // Span's iterators should be checked. To confirm the crashes come from the
  // iterator checks and not stray memory accesses, we create spans that are
  // backed by larger arrays.
  int array1[] = {1, 2, 3, 4};
  int array2[] = {1, 2, 3, 4};
  span<int> span_len2 = span(array1).first(2u);
  span<int> span_len3 = span(array2).first(3u);
  ASSERT_DEATH_IF_SUPPORTED(*span_len2.end(), "");
  ASSERT_DEATH_IF_SUPPORTED(span_len2.begin()[2], "");
  ASSERT_DEATH_IF_SUPPORTED(span_len2.begin() + 3, "");
  ASSERT_DEATH_IF_SUPPORTED(span_len2.begin() - 1, "");
  ASSERT_DEATH_IF_SUPPORTED(span_len2.end() + 1, "");

  // When STL functions take explicit end iterators, bounds checking happens
  // at the caller, when end iterator is created. However, some APIs take only a
  // begin iterator and determine end implicitly. In that case, bounds checking
  // happens inside the STL. However, the STL sometimes specializes operations
  // on contiguous iterators. These death ensures this specialization does not
  // lose hardening.
  //
  // Note that these tests are necessary, but not sufficient, to demonstrate
  // that iterators are suitably checked. The output iterator is currently
  // checked too late due to https://crbug.com/1520041.

  // Copying more values than fit in the destination.
  ASSERT_DEATH_IF_SUPPORTED(
      std::copy(span_len3.begin(), span_len3.end(), span_len2.begin()), "");
  ASSERT_DEATH_IF_SUPPORTED(std::ranges::copy(span_len3, span_len2.begin()),
                            "");
  ASSERT_DEATH_IF_SUPPORTED(
      std::copy_n(span_len3.begin(), 3, span_len2.begin()), "");

  // Copying more values than exist in the source.
  ASSERT_DEATH_IF_SUPPORTED(
      std::copy_n(span_len2.begin(), 3, span_len3.begin()), "");
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
        // SAFETY: TODO(tsepez): iterator constructor safety is dubious
        // given that we are adding indices like -4 to `data()`.
        UNSAFE_BUFFERS(CheckedContiguousIterator<const int>(
            span.data() + dest_start_index,
            span.data() + dest_start_index + kNumElements))));
  }

  // Non-overlapping ranges.
  for (const int dest_start_index : kNonOverlappingStartIndexes) {
    EXPECT_TRUE(CheckedContiguousIterator<const int>::IsRangeMoveSafe(
        span.begin(), span.end(),
        // SAFETY: TODO(tsepez): iterator constructor safety is dubious
        // given that we are adding indices like -7 to `data()`.
        UNSAFE_BUFFERS(CheckedContiguousIterator<const int>(
            span.data() + dest_start_index,
            span.data() + dest_start_index + kNumElements))));
  }

  // IsRangeMoveSafe is true if the length to be moved is 0.
  EXPECT_TRUE(CheckedContiguousIterator<const int>::IsRangeMoveSafe(
      span.begin(), span.begin(),
      // SAFETY: Empty range at the start of a span is always valid.
      UNSAFE_BUFFERS(
          CheckedContiguousIterator<const int>(span.data(), span.data()))));

  // IsRangeMoveSafe is false if end < begin.
  EXPECT_FALSE(CheckedContiguousIterator<const int>::IsRangeMoveSafe(
      span.end(), span.begin(),
      // SAFETY: Empty range at the start of a span is always valid.
      UNSAFE_BUFFERS(
          CheckedContiguousIterator<const int>(span.data(), span.data()))));
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
  static_assert(std::is_constructible_v<span<int, 0>, span<int>>,
                "Error: static span should be constructible from dynamic span");

  static_assert(
      !std::is_convertible_v<span<int>, span<int, 0>>,
      "Error: static span should not be convertible from dynamic span");

  static_assert(!std::is_constructible_v<span<int, 2>, span<int, 1>>,
                "Error: static span should not be constructible from static "
                "span with different extent");

  static_assert(std::is_convertible_v<span<int, 0>, span<int>>,
                "Error: static span should be convertible to dynamic span");

  static_assert(std::is_convertible_v<span<int>, span<int>>,
                "Error: dynamic span should be convertible to dynamic span");

  static_assert(std::is_convertible_v<span<int, 2>, span<int, 2>>,
                "Error: static span should be convertible to static span");
}

TEST(SpanTest, IteratorConversions) {
  static_assert(
      std::is_convertible_v<span<int>::iterator, span<const int>::iterator>,
      "Error: iterator should be convertible to const iterator");

  static_assert(
      !std::is_convertible_v<span<const int>::iterator, span<int>::iterator>,
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

TEST(SpanTest, CopyFrom) {
  int arr[] = {1, 2, 3};
  span<int, 0> empty_static_span;
  span<int, 3> static_span = base::make_span(arr);

  std::vector<int> vec = {4, 5, 6};
  span<int> empty_dynamic_span;
  span<int> dynamic_span = base::make_span(vec);

  // Handle empty cases gracefully.
  // Dynamic size to static size requires an explicit conversion.
  empty_static_span.copy_from(*empty_dynamic_span.to_fixed_extent<0>());
  empty_dynamic_span.copy_from(empty_static_span);
  static_span.first(empty_static_span.size()).copy_from(empty_static_span);
  dynamic_span.first(empty_dynamic_span.size()).copy_from(empty_dynamic_span);
  EXPECT_THAT(arr, ElementsAre(1, 2, 3));
  EXPECT_THAT(vec, ElementsAre(4, 5, 6));

  // Test too small destinations.
  EXPECT_DEATH_IF_SUPPORTED(empty_static_span.copy_from(dynamic_span), "");
  EXPECT_DEATH_IF_SUPPORTED(empty_dynamic_span.copy_from(static_span), "");
  EXPECT_DEATH_IF_SUPPORTED(empty_dynamic_span.copy_from(dynamic_span), "");
  EXPECT_DEATH_IF_SUPPORTED(dynamic_span.last(2u).copy_from(static_span), "");

  std::vector<int> source = {7, 8, 9};

  static_span.first(2u).copy_from(span(source).last(2u));
  EXPECT_THAT(arr, ElementsAre(8, 9, 3));

  dynamic_span.first(2u).copy_from(span(source).last(2u));
  EXPECT_THAT(vec, ElementsAre(8, 9, 6));

  static_span.first(1u).copy_from(span(source).last(1u));
  EXPECT_THAT(arr, ElementsAre(9, 9, 3));

  dynamic_span.first(1u).copy_from(span(source).last(1u));
  EXPECT_THAT(vec, ElementsAre(9, 9, 6));

  struct NonTrivial {
    NonTrivial(int o) : i(o) {}
    NonTrivial(const NonTrivial& o) : i(o) {}
    NonTrivial& operator=(const NonTrivial& o) {
      i = o;
      return *this;
    }
    operator int() const { return i; }
    int i;
  };

  // Overlapping spans. Fixed size.
  {
    int long_arr_is_long[] = {1, 2, 3, 4, 5, 6, 7};
    auto left = span(long_arr_is_long).first<5>();
    auto right = span(long_arr_is_long).last<5>();
    left.copy_from(right);
    EXPECT_THAT(long_arr_is_long, ElementsAre(3, 4, 5, 6, 7, 6, 7));
  }
  {
    int long_arr_is_long[] = {1, 2, 3, 4, 5, 6, 7};
    auto left = span(long_arr_is_long).first<5>();
    auto right = span(long_arr_is_long).last<5>();
    right.copy_from(left);
    EXPECT_THAT(long_arr_is_long, ElementsAre(1, 2, 1, 2, 3, 4, 5));
  }
  {
    int long_arr_is_long[] = {1, 2, 3, 4, 5, 6, 7};
    auto left = span(long_arr_is_long).first<5>();
    left.copy_from(left);
    EXPECT_THAT(long_arr_is_long, ElementsAre(1, 2, 3, 4, 5, 6, 7));
  }
  {
    NonTrivial long_arr_is_long[] = {1, 2, 3, 4, 5, 6, 7};
    auto left = span(long_arr_is_long).first<5>();
    auto right = span(long_arr_is_long).last<5>();
    left.copy_from(right);
    EXPECT_THAT(long_arr_is_long, ElementsAre(3, 4, 5, 6, 7, 6, 7));
  }
  {
    NonTrivial long_arr_is_long[] = {1, 2, 3, 4, 5, 6, 7};
    auto left = span(long_arr_is_long).first<5>();
    auto right = span(long_arr_is_long).last<5>();
    right.copy_from(left);
    EXPECT_THAT(long_arr_is_long, ElementsAre(1, 2, 1, 2, 3, 4, 5));
  }
  {
    NonTrivial long_arr_is_long[] = {1, 2, 3, 4, 5, 6, 7};
    auto left = span(long_arr_is_long).first<5>();
    left.copy_from(left);
    EXPECT_THAT(long_arr_is_long, ElementsAre(1, 2, 3, 4, 5, 6, 7));
  }

  // Overlapping spans. Dynamic size.
  {
    int long_arr_is_long[] = {1, 2, 3, 4, 5, 6, 7};
    auto left = span<int>(long_arr_is_long).first(5u);
    auto right = span<int>(long_arr_is_long).last(5u);
    left.copy_from(right);
    EXPECT_THAT(long_arr_is_long, ElementsAre(3, 4, 5, 6, 7, 6, 7));
  }
  {
    int long_arr_is_long[] = {1, 2, 3, 4, 5, 6, 7};
    auto left = span<int>(long_arr_is_long).first(5u);
    auto right = span<int>(long_arr_is_long).last(5u);
    right.copy_from(left);
    EXPECT_THAT(long_arr_is_long, ElementsAre(1, 2, 1, 2, 3, 4, 5));
  }
  {
    int long_arr_is_long[] = {1, 2, 3, 4, 5, 6, 7};
    auto left = span<int>(long_arr_is_long).first(5u);
    left.copy_from(left);
    EXPECT_THAT(long_arr_is_long, ElementsAre(1, 2, 3, 4, 5, 6, 7));
  }
  {
    NonTrivial long_arr_is_long[] = {1, 2, 3, 4, 5, 6, 7};
    auto left = span<NonTrivial>(long_arr_is_long).first(5u);
    auto right = span<NonTrivial>(long_arr_is_long).last(5u);
    left.copy_from(right);
    EXPECT_THAT(long_arr_is_long, ElementsAre(3, 4, 5, 6, 7, 6, 7));
  }
  {
    NonTrivial long_arr_is_long[] = {1, 2, 3, 4, 5, 6, 7};
    auto left = span<NonTrivial>(long_arr_is_long).first(5u);
    auto right = span<NonTrivial>(long_arr_is_long).last(5u);
    right.copy_from(left);
    EXPECT_THAT(long_arr_is_long, ElementsAre(1, 2, 1, 2, 3, 4, 5));
  }
  {
    NonTrivial long_arr_is_long[] = {1, 2, 3, 4, 5, 6, 7};
    auto left = span<NonTrivial>(long_arr_is_long).first(5u);
    left.copy_from(left);
    EXPECT_THAT(long_arr_is_long, ElementsAre(1, 2, 3, 4, 5, 6, 7));
  }
}

TEST(SpanTest, CopyFromNonoverlapping) {
  int arr[] = {1, 2, 3};
  span<int, 0> empty_static_span;
  span<int, 3> static_span = base::make_span(arr);

  std::vector<int> vec = {4, 5, 6};
  span<int> empty_dynamic_span;
  span<int> dynamic_span = base::make_span(vec);

  // Handle empty cases gracefully.
  UNSAFE_BUFFERS({
    empty_static_span.copy_from_nonoverlapping(empty_dynamic_span);
    empty_dynamic_span.copy_from_nonoverlapping(empty_static_span);
    static_span.first(empty_static_span.size())
        .copy_from_nonoverlapping(empty_static_span);
    dynamic_span.first(empty_dynamic_span.size())
        .copy_from_nonoverlapping(empty_dynamic_span);
    EXPECT_THAT(arr, ElementsAre(1, 2, 3));
    EXPECT_THAT(vec, ElementsAre(4, 5, 6));

    // Test too small destinations.
    EXPECT_DEATH_IF_SUPPORTED(
        empty_static_span.copy_from_nonoverlapping(dynamic_span), "");
    EXPECT_DEATH_IF_SUPPORTED(
        empty_dynamic_span.copy_from_nonoverlapping(static_span), "");
    EXPECT_DEATH_IF_SUPPORTED(
        empty_dynamic_span.copy_from_nonoverlapping(dynamic_span), "");
    EXPECT_DEATH_IF_SUPPORTED(
        dynamic_span.last(2u).copy_from_nonoverlapping(static_span), "");

    std::vector<int> source = {7, 8, 9};

    static_span.first(2u).copy_from_nonoverlapping(span(source).last(2u));
    EXPECT_THAT(arr, ElementsAre(8, 9, 3));

    dynamic_span.first(2u).copy_from_nonoverlapping(span(source).last(2u));
    EXPECT_THAT(vec, ElementsAre(8, 9, 6));

    static_span.first(1u).copy_from_nonoverlapping(span(source).last(1u));
    EXPECT_THAT(arr, ElementsAre(9, 9, 3));

    dynamic_span.first(1u).copy_from_nonoverlapping(span(source).last(1u));
    EXPECT_THAT(vec, ElementsAre(9, 9, 6));
  })
}

TEST(SpanTest, CopyFromConversion) {
  int arr[] = {1, 2, 3};
  span<int, 3> static_span = base::make_span(arr);

  std::vector<int> vec = {4, 5, 6};
  span<int> dynamic_span = base::make_span(vec);

  std::vector convert_from = {7, 8, 9};
  static_span.copy_from(convert_from);
  dynamic_span.copy_from(convert_from);
  EXPECT_THAT(static_span, ElementsAre(7, 8, 9));
  EXPECT_THAT(dynamic_span, ElementsAre(7, 8, 9));

  std::array<int, 3u> convert_from_fixed = {4, 5, 6};
  static_span.copy_from(convert_from_fixed);
  dynamic_span.copy_from(convert_from_fixed);
  EXPECT_THAT(static_span, ElementsAre(4, 5, 6));
  EXPECT_THAT(dynamic_span, ElementsAre(4, 5, 6));

  int convert_from_array[] = {1, 2, 3};
  static_span.copy_from(convert_from_array);
  dynamic_span.copy_from(convert_from_array);
  EXPECT_THAT(static_span, ElementsAre(1, 2, 3));
  EXPECT_THAT(dynamic_span, ElementsAre(1, 2, 3));

  int convert_from_const_array[] = {-1, -2, -3};
  static_span.copy_from(convert_from_const_array);
  dynamic_span.copy_from(convert_from_const_array);
  EXPECT_THAT(static_span, ElementsAre(-1, -2, -3));
  EXPECT_THAT(dynamic_span, ElementsAre(-1, -2, -3));
}

TEST(SpanTest, CopyPrefixFrom) {
  const int vals[] = {1, 2, 3, 4, 5};
  int arr[] = {1, 2, 3, 4, 5};
  span<int, 2> fixed2 = span(arr).first<2>();
  span<int, 3> fixed3 = span(arr).last<3>();
  span<int> dyn2 = span(arr).first(2u);
  span<int> dyn3 = span(arr).last(3u);

  // Copy from a larger buffer.
  EXPECT_CHECK_DEATH(fixed2.copy_prefix_from(dyn3));
  EXPECT_CHECK_DEATH(dyn2.copy_prefix_from(fixed3));
  EXPECT_CHECK_DEATH(dyn2.copy_prefix_from(dyn3));

  // Copy from a smaller buffer into the prefix.
  fixed3.copy_prefix_from(fixed2);
  EXPECT_THAT(arr, ElementsAre(1, 2, 1, 2, 5));
  span(arr).copy_from(vals);

  fixed3.copy_prefix_from(dyn2);
  EXPECT_THAT(arr, ElementsAre(1, 2, 1, 2, 5));
  span(arr).copy_from(vals);

  dyn3.copy_prefix_from(fixed2);
  EXPECT_THAT(arr, ElementsAre(1, 2, 1, 2, 5));
  span(arr).copy_from(vals);

  dyn3.copy_prefix_from(dyn2);
  EXPECT_THAT(arr, ElementsAre(1, 2, 1, 2, 5));
  span(arr).copy_from(vals);

  // Copy from an empty buffer.
  fixed2.copy_prefix_from(span<int, 0>());
  EXPECT_THAT(arr, ElementsAre(1, 2, 3, 4, 5));
  fixed2.copy_prefix_from(span<int>());
  EXPECT_THAT(arr, ElementsAre(1, 2, 3, 4, 5));
  dyn2.copy_prefix_from(span<int, 0>());
  EXPECT_THAT(arr, ElementsAre(1, 2, 3, 4, 5));
  dyn2.copy_prefix_from(span<int>());
  EXPECT_THAT(arr, ElementsAre(1, 2, 3, 4, 5));

  // Copy from a same-size buffer.
  fixed3.first<2>().copy_prefix_from(fixed2);
  EXPECT_THAT(arr, ElementsAre(1, 2, 1, 2, 5));
  span(arr).copy_from(vals);

  fixed3.first<2>().copy_prefix_from(dyn2);
  EXPECT_THAT(arr, ElementsAre(1, 2, 1, 2, 5));
  span(arr).copy_from(vals);

  dyn3.first(2u).copy_prefix_from(fixed2);
  EXPECT_THAT(arr, ElementsAre(1, 2, 1, 2, 5));
  span(arr).copy_from(vals);

  dyn3.first(2u).copy_prefix_from(dyn2);
  EXPECT_THAT(arr, ElementsAre(1, 2, 1, 2, 5));
  span(arr).copy_from(vals);
}

TEST(SpanTest, SplitAt) {
  int arr[] = {1, 2, 3};
  span<int, 0> empty_static_span;
  span<int, 3> static_span = base::make_span(arr);

  std::vector<int> vec = {4, 5, 6};
  span<int> empty_dynamic_span;
  span<int> dynamic_span = base::make_span(vec);

  {
    auto [left, right] = empty_static_span.split_at(0u);
    EXPECT_EQ(left.size(), 0u);
    EXPECT_EQ(right.size(), 0u);
  }
  {
    auto [left, right] = empty_dynamic_span.split_at(0u);
    EXPECT_EQ(left.size(), 0u);
    EXPECT_EQ(right.size(), 0u);
  }

  {
    auto [left, right] = static_span.split_at(0u);
    EXPECT_EQ(left.size(), 0u);
    EXPECT_EQ(right.size(), 3u);
    EXPECT_EQ(right.front(), 1);
  }
  {
    auto [left, right] = static_span.split_at(3u);
    EXPECT_EQ(left.size(), 3u);
    EXPECT_EQ(right.size(), 0u);
    EXPECT_EQ(left.front(), 1);
  }
  {
    auto [left, right] = static_span.split_at(1u);
    EXPECT_EQ(left.size(), 1u);
    EXPECT_EQ(right.size(), 2u);
    EXPECT_EQ(left.front(), 1);
    EXPECT_EQ(right.front(), 2);
  }

  {
    auto [left, right] = dynamic_span.split_at(0u);
    EXPECT_EQ(left.size(), 0u);
    EXPECT_EQ(right.size(), 3u);
    EXPECT_EQ(right.front(), 4);
  }
  {
    auto [left, right] = dynamic_span.split_at(3u);
    EXPECT_EQ(left.size(), 3u);
    EXPECT_EQ(right.size(), 0u);
    EXPECT_EQ(left.front(), 4);
  }
  {
    auto [left, right] = dynamic_span.split_at(1u);
    EXPECT_EQ(left.size(), 1u);
    EXPECT_EQ(right.size(), 2u);
    EXPECT_EQ(left.front(), 4);
    EXPECT_EQ(right.front(), 5);
  }

  // Fixed-size splits.
  {
    auto [left, right] = static_span.split_at<0u>();
    static_assert(std::same_as<decltype(left), span<int, 0u>>);
    static_assert(std::same_as<decltype(right), span<int, 3u>>);
    EXPECT_EQ(left.data(), static_span.data());
    EXPECT_EQ(right.data(), static_span.data());
  }
  {
    auto [left, right] = static_span.split_at<1u>();
    static_assert(std::same_as<decltype(left), span<int, 1u>>);
    static_assert(std::same_as<decltype(right), span<int, 2u>>);
    EXPECT_EQ(left.data(), static_span.data());
    // SAFETY: `array` has three elmenents, so `static_span` has three
    // elements, so `static_span.data() + 1u` points within it.
    EXPECT_EQ(right.data(), UNSAFE_BUFFERS(static_span.data() + 1u));
  }
  {
    auto [left, right] = static_span.split_at<3u>();
    static_assert(std::same_as<decltype(left), span<int, 3u>>);
    static_assert(std::same_as<decltype(right), span<int, 0u>>);
    EXPECT_EQ(left.data(), static_span.data());
    // SAFETY: `array` has three elmenents, so `static_span` has three
    // elements, so `static_span.data() + 3u` points to one byte beyond
    // the end of the object as permitted by C++ standard.
    EXPECT_EQ(right.data(), UNSAFE_BUFFERS(static_span.data() + 3u));
  }
  {
    auto [left, right] = dynamic_span.split_at<0u>();
    static_assert(std::same_as<decltype(left), span<int, 0u>>);
    static_assert(std::same_as<decltype(right), span<int>>);
    EXPECT_EQ(left.data(), dynamic_span.data());
    EXPECT_EQ(right.data(), dynamic_span.data());
    EXPECT_EQ(right.size(), 3u);
  }
  {
    auto [left, right] = dynamic_span.split_at<1u>();
    static_assert(std::same_as<decltype(left), span<int, 1u>>);
    static_assert(std::same_as<decltype(right), span<int>>);
    EXPECT_EQ(left.data(), dynamic_span.data());
    // SAFETY: `array` has three elmenents, so `dynamic_span` has three
    // elements, so `dynamic_span.data() + 1u` points within it.
    EXPECT_EQ(right.data(), UNSAFE_BUFFERS(dynamic_span.data() + 1u));
    EXPECT_EQ(right.size(), 2u);
  }
  {
    auto [left, right] = dynamic_span.split_at<3u>();
    static_assert(std::same_as<decltype(left), span<int, 3u>>);
    static_assert(std::same_as<decltype(right), span<int>>);
    EXPECT_EQ(left.data(), dynamic_span.data());
    // SAFETY: `array` has three elmenents, so `dynamic_span` has three
    // elements, so `dynamic_span.data() + 3u` points to one byte beyond
    // the end of the object as permitted by C++ standard.
    EXPECT_EQ(right.data(), UNSAFE_BUFFERS(dynamic_span.data() + 3u));
    EXPECT_EQ(right.size(), 0u);
  }
  // Invalid fixed-size split from dynamic will fail at runtime.
  EXPECT_CHECK_DEATH({ dynamic_span.split_at<4u>(); });
}

TEST(SpanTest, CompareEquality) {
  static_assert(std::equality_comparable<int>);
  int32_t arr2[] = {1, 2};
  int32_t arr3[] = {1, 2, 3};
  int32_t rra3[] = {3, 2, 1};
  int32_t vec3[] = {1, 2, 3};
  constexpr const int32_t arr2_c[] = {1, 2};
  constexpr const int32_t arr3_c[] = {1, 2, 3};
  constexpr const int32_t rra3_c[] = {3, 2, 1};

  // Comparing empty spans that are fixed and dynamic size.
  EXPECT_TRUE((span<int32_t>() == span<int32_t>()));
  EXPECT_TRUE((span<int32_t, 0u>() == span<int32_t>()));
  EXPECT_TRUE((span<int32_t>() == span<int32_t, 0u>()));
  EXPECT_TRUE((span<int32_t, 0u>() == span<int32_t, 0u>()));
  // Non-null data pointer, but both are empty.
  EXPECT_TRUE(span(arr2).first(0u) == span(arr2).last(0u));
  EXPECT_TRUE(span(arr2).first<0u>() == span(arr2).last<0u>());

  // Spans of different dynamic sizes.
  EXPECT_TRUE(span(arr2).first(2u) != span(arr3).first(3u));
  // Spans of same dynamic size and same values.
  EXPECT_TRUE(span(arr2).first(2u) == span(arr3).first(2u));
  // Spans of same dynamic size but different values.
  EXPECT_TRUE(span(arr2).first(2u) != span(rra3).first(2u));

  // Spans of different sizes (one dynamic one fixed).
  EXPECT_TRUE(span(arr2).first<2u>() != span(arr3).first(3u));
  EXPECT_TRUE(span(arr2).first(2u) != span(arr3).first<3u>());
  // Spans of same size and same values.
  EXPECT_TRUE(span(arr2).first<2u>() == span(arr3).first(2u));
  EXPECT_TRUE(span(arr2).first(2u) == span(arr3).first<2u>());
  // Spans of same size but different values.
  EXPECT_TRUE(span(arr2).first<2u>() != span(rra3).first(2u));
  EXPECT_TRUE(span(arr2).first(2u) != span(rra3).first<2u>());

  // Spans of different fixed sizes do not compile (as in Rust)
  // https://godbolt.org/z/MrnbPeozr and are covered in nocompile tests.

  // Comparing const and non-const. Same tests as above otherwise.

  EXPECT_TRUE((span<const int32_t>() == span<int32_t>()));
  EXPECT_TRUE((span<const int32_t, 0u>() == span<int32_t>()));
  EXPECT_TRUE((span<const int32_t>() == span<int32_t, 0u>()));
  EXPECT_TRUE((span<const int32_t, 0u>() == span<int32_t, 0u>()));

  EXPECT_TRUE((span<int32_t>() == span<const int32_t>()));
  EXPECT_TRUE((span<int32_t, 0u>() == span<const int32_t>()));
  EXPECT_TRUE((span<int32_t>() == span<const int32_t, 0u>()));
  EXPECT_TRUE((span<int32_t, 0u>() == span<const int32_t, 0u>()));

  EXPECT_TRUE(span(arr2_c).first(0u) == span(arr2).last(0u));
  EXPECT_TRUE(span(arr2_c).first<0u>() == span(arr2).last<0u>());

  EXPECT_TRUE(span(arr2).first(0u) == span(arr2_c).last(0u));
  EXPECT_TRUE(span(arr2).first<0u>() == span(arr2_c).last<0u>());

  EXPECT_TRUE(span(arr2_c).first(2u) != span(arr3).first(3u));
  EXPECT_TRUE(span(arr2_c).first(2u) == span(arr3).first(2u));
  EXPECT_TRUE(span(arr2_c).first(2u) != span(rra3).first(2u));

  EXPECT_TRUE(span(arr2).first(2u) != span(arr3_c).first(3u));
  EXPECT_TRUE(span(arr2).first(2u) == span(arr3_c).first(2u));
  EXPECT_TRUE(span(arr2).first(2u) != span(rra3_c).first(2u));

  EXPECT_TRUE(span(arr2_c).first<2u>() != span(arr3).first(3u));
  EXPECT_TRUE(span(arr2_c).first(2u) != span(arr3).first<3u>());
  EXPECT_TRUE(span(arr2_c).first<2u>() == span(arr3).first(2u));
  EXPECT_TRUE(span(arr2_c).first(2u) == span(arr3).first<2u>());
  EXPECT_TRUE(span(arr2_c).first<2u>() != span(rra3).first(2u));
  EXPECT_TRUE(span(arr2_c).first(2u) != span(rra3).first<2u>());

  EXPECT_TRUE(span(arr2).first<2u>() != span(arr3_c).first(3u));
  EXPECT_TRUE(span(arr2).first(2u) != span(arr3_c).first<3u>());
  EXPECT_TRUE(span(arr2).first<2u>() == span(arr3_c).first(2u));
  EXPECT_TRUE(span(arr2).first(2u) == span(arr3_c).first<2u>());
  EXPECT_TRUE(span(arr2).first<2u>() != span(rra3_c).first(2u));
  EXPECT_TRUE(span(arr2).first(2u) != span(rra3_c).first<2u>());

  // Comparing different types which are comparable. Same tests as above
  // otherwise.

  static_assert(std::equality_comparable_with<int32_t, int64_t>);
  int64_t arr2_l[] = {1, 2};
  int64_t arr3_l[] = {1, 2, 3};
  int64_t rra3_l[] = {3, 2, 1};

  EXPECT_TRUE((span<int32_t>() == span<int64_t>()));
  EXPECT_TRUE((span<int32_t, 0u>() == span<int64_t>()));
  EXPECT_TRUE((span<int32_t>() == span<int64_t, 0u>()));
  EXPECT_TRUE((span<int32_t, 0u>() == span<int64_t, 0u>()));

  EXPECT_TRUE((span<int32_t>() == span<int64_t>()));
  EXPECT_TRUE((span<int32_t, 0u>() == span<int64_t>()));
  EXPECT_TRUE((span<int32_t>() == span<int64_t, 0u>()));
  EXPECT_TRUE((span<int32_t, 0u>() == span<int64_t, 0u>()));

  EXPECT_TRUE(span(arr2_l).first(0u) == span(arr2).last(0u));
  EXPECT_TRUE(span(arr2_l).first<0u>() == span(arr2).last<0u>());

  EXPECT_TRUE(span(arr2).first(0u) == span(arr2_l).last(0u));
  EXPECT_TRUE(span(arr2).first<0u>() == span(arr2_l).last<0u>());

  EXPECT_TRUE(span(arr2_l).first(2u) != span(arr3).first(3u));
  EXPECT_TRUE(span(arr2_l).first(2u) == span(arr3).first(2u));
  EXPECT_TRUE(span(arr2_l).first(2u) != span(rra3).first(2u));

  EXPECT_TRUE(span(arr2).first(2u) != span(arr3_l).first(3u));
  EXPECT_TRUE(span(arr2).first(2u) == span(arr3_l).first(2u));
  EXPECT_TRUE(span(arr2).first(2u) != span(rra3_l).first(2u));

  EXPECT_TRUE(span(arr2_l).first<2u>() != span(arr3).first(3u));
  EXPECT_TRUE(span(arr2_l).first(2u) != span(arr3).first<3u>());
  EXPECT_TRUE(span(arr2_l).first<2u>() == span(arr3).first(2u));
  EXPECT_TRUE(span(arr2_l).first(2u) == span(arr3).first<2u>());
  EXPECT_TRUE(span(arr2_l).first<2u>() != span(rra3).first(2u));
  EXPECT_TRUE(span(arr2_l).first(2u) != span(rra3).first<2u>());

  EXPECT_TRUE(span(arr2).first<2u>() != span(arr3_l).first(3u));
  EXPECT_TRUE(span(arr2).first(2u) != span(arr3_l).first<3u>());
  EXPECT_TRUE(span(arr2).first<2u>() == span(arr3_l).first(2u));
  EXPECT_TRUE(span(arr2).first(2u) == span(arr3_l).first<2u>());
  EXPECT_TRUE(span(arr2).first<2u>() != span(rra3_l).first(2u));
  EXPECT_TRUE(span(arr2).first(2u) != span(rra3_l).first<2u>());

  // Comparing different types and different const-ness at the same time.

  constexpr const int64_t arr2_lc[] = {1, 2};
  constexpr const int64_t arr3_lc[] = {1, 2, 3};
  constexpr const int64_t rra3_lc[] = {3, 2, 1};

  EXPECT_TRUE((span<const int32_t>() == span<int64_t>()));
  EXPECT_TRUE((span<const int32_t, 0u>() == span<int64_t>()));
  EXPECT_TRUE((span<const int32_t>() == span<int64_t, 0u>()));
  EXPECT_TRUE((span<const int32_t, 0u>() == span<int64_t, 0u>()));

  EXPECT_TRUE((span<int32_t>() == span<const int64_t>()));
  EXPECT_TRUE((span<int32_t, 0u>() == span<const int64_t>()));
  EXPECT_TRUE((span<int32_t>() == span<const int64_t, 0u>()));
  EXPECT_TRUE((span<int32_t, 0u>() == span<const int64_t, 0u>()));

  EXPECT_TRUE(span(arr2_lc).first(0u) == span(arr2).last(0u));
  EXPECT_TRUE(span(arr2_lc).first<0u>() == span(arr2).last<0u>());

  EXPECT_TRUE(span(arr2).first(0u) == span(arr2_lc).last(0u));
  EXPECT_TRUE(span(arr2).first<0u>() == span(arr2_lc).last<0u>());

  EXPECT_TRUE(span(arr2_lc).first(2u) != span(arr3).first(3u));
  EXPECT_TRUE(span(arr2_lc).first(2u) == span(arr3).first(2u));
  EXPECT_TRUE(span(arr2_lc).first(2u) != span(rra3).first(2u));

  EXPECT_TRUE(span(arr2).first(2u) != span(arr3_lc).first(3u));
  EXPECT_TRUE(span(arr2).first(2u) == span(arr3_lc).first(2u));
  EXPECT_TRUE(span(arr2).first(2u) != span(rra3_lc).first(2u));

  EXPECT_TRUE(span(arr2_lc).first<2u>() != span(arr3).first(3u));
  EXPECT_TRUE(span(arr2_lc).first(2u) != span(arr3).first<3u>());
  EXPECT_TRUE(span(arr2_lc).first<2u>() == span(arr3).first(2u));
  EXPECT_TRUE(span(arr2_lc).first(2u) == span(arr3).first<2u>());
  EXPECT_TRUE(span(arr2_lc).first<2u>() != span(rra3).first(2u));
  EXPECT_TRUE(span(arr2_lc).first(2u) != span(rra3).first<2u>());

  EXPECT_TRUE(span(arr2).first<2u>() != span(arr3_lc).first(3u));
  EXPECT_TRUE(span(arr2).first(2u) != span(arr3_lc).first<3u>());
  EXPECT_TRUE(span(arr2).first<2u>() == span(arr3_lc).first(2u));
  EXPECT_TRUE(span(arr2).first(2u) == span(arr3_lc).first<2u>());
  EXPECT_TRUE(span(arr2).first<2u>() != span(rra3_lc).first(2u));
  EXPECT_TRUE(span(arr2).first(2u) != span(rra3_lc).first<2u>());

  // Comparing with an implicit conversion to span. This only works if the span
  // types actually match (i.e. not for any comparable types) since otherwise
  // the type can not be deduced. Implicit conversion from mutable to const
  // can be inferred though.

  EXPECT_TRUE(arr2 != span(arr3).first(3u));
  EXPECT_TRUE(arr2 == span(arr3).first(2u));
  EXPECT_TRUE(arr2 != span(rra3).first(2u));

  EXPECT_TRUE(arr2 != span(arr3_c).first(3u));
  EXPECT_TRUE(arr2 == span(arr3_c).first(2u));
  EXPECT_TRUE(arr2 != span(rra3_c).first(2u));

  EXPECT_TRUE(arr2_c != span(arr3).first(3u));
  EXPECT_TRUE(arr2_c == span(arr3).first(2u));
  EXPECT_TRUE(arr2_c != span(rra3).first(2u));

  // Comparing mutable to mutable, there's no ambiguity about which overload to
  // call (mutable or implicit-const).
  EXPECT_FALSE(span(arr3) == rra3);            // Fixed size.
  EXPECT_FALSE(span(vec3).first(2u) == vec3);  // Dynamic size.
  EXPECT_FALSE(span(arr3).first(2u) == rra3);  // Fixed with dynamic size.

  // Constexpr comparison.
  static_assert(span<int>() == span<int, 0u>());
  static_assert(span(arr2_c) == span(arr3_c).first(2u));
  static_assert(span(arr2_c) == span(arr3_lc).first(2u));
}

TEST(SpanTest, CompareOrdered) {
  static_assert(std::three_way_comparable<int>);
  int32_t arr2[] = {1, 2};
  int32_t arr3[] = {1, 2, 3};
  int32_t rra3[] = {3, 2, 1};
  int32_t vec3[] = {1, 2, 3};
  constexpr const int32_t arr2_c[] = {1, 2};
  constexpr const int32_t arr3_c[] = {1, 2, 3};
  constexpr const int32_t rra3_c[] = {3, 2, 1};

  // Less than.
  EXPECT_TRUE(span(arr3) < span(rra3));
  EXPECT_TRUE(span(arr2).first(2u) < span(arr3));
  // Greater than.
  EXPECT_TRUE(span(rra3) > span(arr3));
  EXPECT_TRUE(span(arr3) > span(arr2).first(2u));

  // Comparing empty spans that are fixed and dynamic size.
  EXPECT_TRUE((span<int32_t>() <=> span<int32_t>()) == 0);
  EXPECT_TRUE((span<int32_t, 0u>() <=> span<int32_t>()) == 0);
  EXPECT_TRUE((span<int32_t>() <=> span<int32_t, 0u>()) == 0);
  EXPECT_TRUE((span<int32_t, 0u>() <=> span<int32_t, 0u>()) == 0);
  // Non-null data pointer, but both are empty.
  EXPECT_TRUE(span(arr2).first(0u) <=> span(arr2).last(0u) == 0);
  EXPECT_TRUE(span(arr2).first<0u>() <=> span(arr2).last<0u>() == 0);

  // Spans of different dynamic sizes.
  EXPECT_TRUE(span(arr2).first(2u) <=> span(arr3).first(3u) < 0);
  // Spans of same dynamic size and same values.
  EXPECT_TRUE(span(arr2).first(2u) <=> span(arr3).first(2u) == 0);
  // Spans of same dynamic size but different values.
  EXPECT_TRUE(span(arr2).first(2u) <=> span(rra3).first(2u) < 0);

  // Spans of different sizes (one dynamic one fixed).
  EXPECT_TRUE(span(arr2).first<2u>() <=> span(arr3).first(3u) < 0);
  EXPECT_TRUE(span(arr2).first(2u) <=> span(arr3).first<3u>() < 0);
  // Spans of same size and same values.
  EXPECT_TRUE(span(arr2).first<2u>() <=> span(arr3).first(2u) == 0);
  EXPECT_TRUE(span(arr2).first(2u) <=> span(arr3).first<2u>() == 0);
  // Spans of same size but different values.
  EXPECT_TRUE(span(arr2).first<2u>() <=> span(rra3).first(2u) < 0);
  EXPECT_TRUE(span(arr2).first(2u) <=> span(rra3).first<2u>() < 0);

  // Spans of different fixed sizes do not compile (as in Rust)
  // https://godbolt.org/z/MrnbPeozr and are covered in nocompile tests.

  // Comparing const and non-const. Same tests as above otherwise.

  EXPECT_TRUE((span<const int32_t>() <=> span<int32_t>()) == 0);
  EXPECT_TRUE((span<const int32_t, 0u>() <=> span<int32_t>()) == 0);
  EXPECT_TRUE((span<const int32_t>() <=> span<int32_t, 0u>()) == 0);
  EXPECT_TRUE((span<const int32_t, 0u>() <=> span<int32_t, 0u>()) == 0);

  EXPECT_TRUE((span<int32_t>() <=> span<const int32_t>()) == 0);
  EXPECT_TRUE((span<int32_t, 0u>() <=> span<const int32_t>()) == 0);
  EXPECT_TRUE((span<int32_t>() <=> span<const int32_t, 0u>()) == 0);
  EXPECT_TRUE((span<int32_t, 0u>() <=> span<const int32_t, 0u>()) == 0);

  EXPECT_TRUE(span(arr2_c).first(0u) <=> span(arr2).last(0u) == 0);
  EXPECT_TRUE(span(arr2_c).first<0u>() <=> span(arr2).last<0u>() == 0);

  EXPECT_TRUE(span(arr2).first(0u) <=> span(arr2_c).last(0u) == 0);
  EXPECT_TRUE(span(arr2).first<0u>() <=> span(arr2_c).last<0u>() == 0);

  EXPECT_TRUE(span(arr2_c).first(2u) <=> span(arr3).first(3u) < 0);
  EXPECT_TRUE(span(arr2_c).first(2u) <=> span(arr3).first(2u) == 0);
  EXPECT_TRUE(span(arr2_c).first(2u) <=> span(rra3).first(2u) < 0);

  EXPECT_TRUE(span(arr2).first(2u) <=> span(arr3_c).first(3u) < 0);
  EXPECT_TRUE(span(arr2).first(2u) <=> span(arr3_c).first(2u) == 0);
  EXPECT_TRUE(span(arr2).first(2u) <=> span(rra3_c).first(2u) < 0);

  EXPECT_TRUE(span(arr2_c).first<2u>() <=> span(arr3).first(3u) < 0);
  EXPECT_TRUE(span(arr2_c).first(2u) <=> span(arr3).first<3u>() < 0);
  EXPECT_TRUE(span(arr2_c).first<2u>() <=> span(arr3).first(2u) == 0);
  EXPECT_TRUE(span(arr2_c).first(2u) <=> span(arr3).first<2u>() == 0);
  EXPECT_TRUE(span(arr2_c).first<2u>() <=> span(rra3).first(2u) < 0);
  EXPECT_TRUE(span(arr2_c).first(2u) <=> span(rra3).first<2u>() < 0);

  EXPECT_TRUE(span(arr2).first<2u>() <=> span(arr3_c).first(3u) < 0);
  EXPECT_TRUE(span(arr2).first(2u) <=> span(arr3_c).first<3u>() < 0);
  EXPECT_TRUE(span(arr2).first<2u>() <=> span(arr3_c).first(2u) == 0);
  EXPECT_TRUE(span(arr2).first(2u) <=> span(arr3_c).first<2u>() == 0);
  EXPECT_TRUE(span(arr2).first<2u>() <=> span(rra3_c).first(2u) < 0);
  EXPECT_TRUE(span(arr2).first(2u) <=> span(rra3_c).first<2u>() < 0);

  // Comparing different types which are comparable. Same tests as above
  // otherwise.

  static_assert(std::three_way_comparable_with<int32_t, int64_t>);
  int64_t arr2_l[] = {1, 2};
  int64_t arr3_l[] = {1, 2, 3};
  int64_t rra3_l[] = {3, 2, 1};

  EXPECT_TRUE((span<int32_t>() <=> span<int64_t>()) == 0);
  EXPECT_TRUE((span<int32_t, 0u>() <=> span<int64_t>()) == 0);
  EXPECT_TRUE((span<int32_t>() <=> span<int64_t, 0u>()) == 0);
  EXPECT_TRUE((span<int32_t, 0u>() <=> span<int64_t, 0u>()) == 0);

  EXPECT_TRUE((span<int32_t>() <=> span<int64_t>()) == 0);
  EXPECT_TRUE((span<int32_t, 0u>() <=> span<int64_t>()) == 0);
  EXPECT_TRUE((span<int32_t>() <=> span<int64_t, 0u>()) == 0);
  EXPECT_TRUE((span<int32_t, 0u>() <=> span<int64_t, 0u>()) == 0);

  EXPECT_TRUE(span(arr2_l).first(0u) <=> span(arr2).last(0u) == 0);
  EXPECT_TRUE(span(arr2_l).first<0u>() <=> span(arr2).last<0u>() == 0);

  EXPECT_TRUE(span(arr2).first(0u) <=> span(arr2_l).last(0u) == 0);
  EXPECT_TRUE(span(arr2).first<0u>() <=> span(arr2_l).last<0u>() == 0);

  EXPECT_TRUE(span(arr2_l).first(2u) <=> span(arr3).first(3u) < 0);
  EXPECT_TRUE(span(arr2_l).first(2u) <=> span(arr3).first(2u) == 0);
  EXPECT_TRUE(span(arr2_l).first(2u) <=> span(rra3).first(2u) < 0);

  EXPECT_TRUE(span(arr2).first(2u) <=> span(arr3_l).first(3u) < 0);
  EXPECT_TRUE(span(arr2).first(2u) <=> span(arr3_l).first(2u) == 0);
  EXPECT_TRUE(span(arr2).first(2u) <=> span(rra3_l).first(2u) < 0);

  EXPECT_TRUE(span(arr2_l).first<2u>() <=> span(arr3).first(3u) < 0);
  EXPECT_TRUE(span(arr2_l).first(2u) <=> span(arr3).first<3u>() < 0);
  EXPECT_TRUE(span(arr2_l).first<2u>() <=> span(arr3).first(2u) == 0);
  EXPECT_TRUE(span(arr2_l).first(2u) <=> span(arr3).first<2u>() == 0);
  EXPECT_TRUE(span(arr2_l).first<2u>() <=> span(rra3).first(2u) < 0);
  EXPECT_TRUE(span(arr2_l).first(2u) <=> span(rra3).first<2u>() < 0);

  EXPECT_TRUE(span(arr2).first<2u>() <=> span(arr3_l).first(3u) < 0);
  EXPECT_TRUE(span(arr2).first(2u) <=> span(arr3_l).first<3u>() < 0);
  EXPECT_TRUE(span(arr2).first<2u>() <=> span(arr3_l).first(2u) == 0);
  EXPECT_TRUE(span(arr2).first(2u) <=> span(arr3_l).first<2u>() == 0);
  EXPECT_TRUE(span(arr2).first<2u>() <=> span(rra3_l).first(2u) < 0);
  EXPECT_TRUE(span(arr2).first(2u) <=> span(rra3_l).first<2u>() < 0);

  // Comparing different types and different const-ness at the same time.

  constexpr const int64_t arr2_lc[] = {1, 2};
  constexpr const int64_t arr3_lc[] = {1, 2, 3};
  constexpr const int64_t rra3_lc[] = {3, 2, 1};

  EXPECT_TRUE((span<const int32_t>() <=> span<int64_t>()) == 0);
  EXPECT_TRUE((span<const int32_t, 0u>() <=> span<int64_t>()) == 0);
  EXPECT_TRUE((span<const int32_t>() <=> span<int64_t, 0u>()) == 0);
  EXPECT_TRUE((span<const int32_t, 0u>() <=> span<int64_t, 0u>()) == 0);

  EXPECT_TRUE((span<int32_t>() <=> span<const int64_t>()) == 0);
  EXPECT_TRUE((span<int32_t, 0u>() <=> span<const int64_t>()) == 0);
  EXPECT_TRUE((span<int32_t>() <=> span<const int64_t, 0u>()) == 0);
  EXPECT_TRUE((span<int32_t, 0u>() <=> span<const int64_t, 0u>()) == 0);

  EXPECT_TRUE(span(arr2_lc).first(0u) <=> span(arr2).last(0u) == 0);
  EXPECT_TRUE(span(arr2_lc).first<0u>() <=> span(arr2).last<0u>() == 0);

  EXPECT_TRUE(span(arr2).first(0u) <=> span(arr2_lc).last(0u) == 0);
  EXPECT_TRUE(span(arr2).first<0u>() <=> span(arr2_lc).last<0u>() == 0);

  EXPECT_TRUE(span(arr2_lc).first(2u) <=> span(arr3).first(3u) < 0);
  EXPECT_TRUE(span(arr2_lc).first(2u) <=> span(arr3).first(2u) == 0);
  EXPECT_TRUE(span(arr2_lc).first(2u) <=> span(rra3).first(2u) < 0);

  EXPECT_TRUE(span(arr2).first(2u) <=> span(arr3_lc).first(3u) < 0);
  EXPECT_TRUE(span(arr2).first(2u) <=> span(arr3_lc).first(2u) == 0);
  EXPECT_TRUE(span(arr2).first(2u) <=> span(rra3_lc).first(2u) < 0);

  EXPECT_TRUE(span(arr2_lc).first<2u>() <=> span(arr3).first(3u) < 0);
  EXPECT_TRUE(span(arr2_lc).first(2u) <=> span(arr3).first<3u>() < 0);
  EXPECT_TRUE(span(arr2_lc).first<2u>() <=> span(arr3).first(2u) == 0);
  EXPECT_TRUE(span(arr2_lc).first(2u) <=> span(arr3).first<2u>() == 0);
  EXPECT_TRUE(span(arr2_lc).first<2u>() <=> span(rra3).first(2u) < 0);
  EXPECT_TRUE(span(arr2_lc).first(2u) <=> span(rra3).first<2u>() < 0);

  EXPECT_TRUE(span(arr2).first<2u>() <=> span(arr3_lc).first(3u) < 0);
  EXPECT_TRUE(span(arr2).first(2u) <=> span(arr3_lc).first<3u>() < 0);
  EXPECT_TRUE(span(arr2).first<2u>() <=> span(arr3_lc).first(2u) == 0);
  EXPECT_TRUE(span(arr2).first(2u) <=> span(arr3_lc).first<2u>() == 0);
  EXPECT_TRUE(span(arr2).first<2u>() <=> span(rra3_lc).first(2u) < 0);
  EXPECT_TRUE(span(arr2).first(2u) <=> span(rra3_lc).first<2u>() < 0);

  // Comparing with an implicit conversion to span. This only works if the span
  // types actually match (i.e. not for any comparable types) since otherwise
  // the type can not be deduced. Implicit conversion from mutable to const
  // can be inferred though.

  EXPECT_TRUE(arr2 <=> span(arr3).first(3u) < 0);
  EXPECT_TRUE(arr2 <=> span(arr3).first(2u) == 0);
  EXPECT_TRUE(arr2 <=> span(rra3).first(2u) < 0);

  EXPECT_TRUE(arr2 <=> span(arr3_c).first(3u) < 0);
  EXPECT_TRUE(arr2 <=> span(arr3_c).first(2u) == 0);
  EXPECT_TRUE(arr2 <=> span(rra3_c).first(2u) < 0);

  EXPECT_TRUE(arr2_c <=> span(arr3).first(3u) < 0);
  EXPECT_TRUE(arr2_c <=> span(arr3).first(2u) == 0);
  EXPECT_TRUE(arr2_c <=> span(rra3).first(2u) < 0);

  // Comparing mutable to mutable, there's no ambiguity about which overload to
  // call (mutable or implicit-const).
  EXPECT_FALSE(span(arr3) <=> rra3 == 0);            // Fixed size.
  EXPECT_FALSE(span(vec3).first(2u) <=> vec3 == 0);  // Dynamic size.
  EXPECT_FALSE(span(arr3).first(2u) <=> rra3 == 0);  // Fixed with dynamic size.

  // Constexpr comparison.
  static_assert(span<int>() <=> span<int, 0u>() == 0);
  static_assert(span(arr2_c) <=> span(arr3_c).first(2u) == 0);
  static_assert(span(arr2_c) <=> span(arr3_lc).first(2u) == 0);
}

TEST(SpanTest, GMockMacroCompatibility) {
  int arr1[] = {1, 3, 5};
  int arr2[] = {1, 3, 5};
  std::vector vec1(std::begin(arr1), std::end(arr1));
  std::vector vec2(std::begin(arr2), std::end(arr2));
  span<int, 3> static_span1(arr1);
  span<int, 3> static_span2(arr2);
  span<int> dynamic_span1(vec1);
  span<int> dynamic_span2(vec2);

  EXPECT_THAT(arr1, ElementsAreArray(static_span2));
  EXPECT_THAT(arr1, ElementsAreArray(dynamic_span2));

  EXPECT_THAT(vec1, ElementsAreArray(static_span2));
  EXPECT_THAT(vec1, ElementsAreArray(dynamic_span2));

  EXPECT_THAT(static_span1, ElementsAre(1, 3, 5));
  EXPECT_THAT(static_span1, ElementsAreArray(arr2));
  EXPECT_THAT(static_span1, ElementsAreArray(static_span2));
  EXPECT_THAT(static_span1, ElementsAreArray(dynamic_span2));
  EXPECT_THAT(static_span1, ElementsAreArray(vec2));

  EXPECT_THAT(dynamic_span1, ElementsAre(1, 3, 5));
  EXPECT_THAT(dynamic_span1, ElementsAreArray(arr2));
  EXPECT_THAT(dynamic_span1, ElementsAreArray(static_span2));
  EXPECT_THAT(dynamic_span1, ElementsAreArray(dynamic_span2));
  EXPECT_THAT(dynamic_span1, ElementsAreArray(vec2));
}

TEST(SpanTest, GTestMacroCompatibility) {
  int arr1[] = {1, 3, 5};
  int arr2[] = {1, 3, 5};
  int arr3[] = {2, 4, 6, 8};
  std::vector vec1(std::begin(arr1), std::end(arr1));
  std::vector vec2(std::begin(arr2), std::end(arr2));
  std::vector vec3(std::begin(arr3), std::end(arr3));
  span<int, 3> static_span1(arr1);
  span<int, 3> static_span2(arr2);
  span<int, 4> static_span3(arr3);
  span<int> dynamic_span1(vec1);
  span<int> dynamic_span2(vec2);
  span<int> dynamic_span3(vec3);

  // Alas, many desirable comparisions are still not possible. They
  // are commented out below.
  EXPECT_EQ(arr1, static_span2);
  EXPECT_EQ(arr1, dynamic_span2);

  // EXPECT_EQ(vec1, static_span2);
  EXPECT_EQ(vec1, dynamic_span2);

  EXPECT_EQ(static_span1, arr2);
  EXPECT_EQ(static_span1, static_span2);
  EXPECT_EQ(static_span1, dynamic_span2);
  // EXPECT_EQ(static_span1, vec2);

  EXPECT_EQ(dynamic_span1, arr2);
  EXPECT_EQ(dynamic_span1, static_span2);
  EXPECT_EQ(dynamic_span1, dynamic_span2);
  EXPECT_EQ(dynamic_span1, vec2);

  // EXPECT_NE(arr1, static_span3);
  EXPECT_NE(arr1, dynamic_span3);

  // EXPECT_NE(vec1, static_span3);
  EXPECT_NE(vec1, dynamic_span3);

  // EXPECT_NE(static_span1, arr3);
  // EXPECT_NE(static_span1, static_span3);
  EXPECT_NE(static_span1, dynamic_span3);
  // EXPECT_NE(static_span1, vec3);

  EXPECT_NE(dynamic_span1, arr3);
  EXPECT_NE(dynamic_span1, static_span3);
  EXPECT_NE(dynamic_span1, dynamic_span3);
  EXPECT_NE(dynamic_span1, vec3);
}

// These are all examples from //docs/unsafe_buffers.md, copied here to ensure
// they compile.
TEST(SpanTest, Example_UnsafeBuffersPatterns) {
  struct Object {
    int a;
  };
  auto func_with_const_ptr_size = [](const uint8_t*, size_t) {};
  auto func_with_mut_ptr_size = [](uint8_t*, size_t) {};
  auto func_with_const_span = [](span<const uint8_t>) {};
  auto func_with_mut_span = [](span<uint8_t>) {};
  auto two_byte_arrays = [](const uint8_t*, const uint8_t*) {};
  auto two_byte_spans = [](span<const uint8_t>, span<const uint8_t>) {};

  UNSAFE_BUFFERS({
    uint8_t array1[12];
    uint8_t array2[16];
    uint64_t array3[2];
    memcpy(array1, array2 + 8, 4);
    memcpy(array1 + 4, array3, 8);
  })

  {
    uint8_t array1[12];
    uint8_t array2[16];
    uint64_t array3[2];
    base::span(array1).first(4u).copy_from(base::span(array2).subspan(8u, 4u));
    base::span(array1).subspan(4u).copy_from(
        base::as_byte_span(array3).first(8u));

    {
      // Use `split_at()` to ensure `array1` is fully written.
      auto [from2, from3] = base::span(array1).split_at(4u);
      from2.copy_from(base::span(array2).subspan(8u, 4u));
      from3.copy_from(base::as_byte_span(array3).first(8u));
    }
    {
      // This can even be ensured at compile time (if sizes and offsets are all
      // constants).
      auto [from2, from3] = base::span(array1).split_at<4u>();
      from2.copy_from(base::span(array2).subspan<8u, 4u>());
      from3.copy_from(base::as_byte_span(array3).first<8u>());
    }
  }

  UNSAFE_BUFFERS({
    uint8_t array1[12];
    uint64_t array2[2];
    Object array3[4];
    memset(array1, 0, 12);
    memset(array2, 0, 2 * sizeof(uint64_t));
    memset(array3, 0, 4 * sizeof(Object));
  })

  {
    uint8_t array1[12];
    uint64_t array2[2];
    Object array3[4];
    std::ranges::fill(array1, 0u);
    std::ranges::fill(array2, 0u);
    std::ranges::fill(base::as_writable_byte_span(array3), 0u);
  }

  UNSAFE_BUFFERS({
    uint8_t array1[12] = {};
    uint8_t array2[12] = {};
    [[maybe_unused]] bool ne = memcmp(array1, array2, sizeof(array1)) == 0;
    [[maybe_unused]] bool less = memcmp(array1, array2, sizeof(array1)) < 0;

    // In tests.
    for (size_t i = 0; i < sizeof(array1); ++i) {
      SCOPED_TRACE(i);
      EXPECT_EQ(array1[i], array2[i]);
    }
  })

  {
    uint8_t array1[12] = {};
    uint8_t array2[12] = {};
    // If one side is a span, the other will convert to span too.
    [[maybe_unused]] bool eq = base::span(array1) == array2;
    [[maybe_unused]] bool less = base::span(array1) < array2;

    // In tests.
    EXPECT_EQ(base::span(array1), array2);
  }

  UNSAFE_BUFFERS({
    uint8_t array[44] = {};
    uint32_t v1;
    memcpy(&v1, array, sizeof(v1));  // Front.
    uint64_t v2;
    memcpy(&v2, array + 6, sizeof(v2));  // Middle.
  })

  {
    uint8_t array[44] = {};
    [[maybe_unused]] uint32_t v1 =
        base::U32FromLittleEndian(base::span(array).first<4u>());  // Front.
    [[maybe_unused]] uint64_t v2 = base::U64FromLittleEndian(
        base::span(array).subspan<6u, 8u>());  // Middle.
  }

  UNSAFE_BUFFERS({
    // `array` must be aligned for the cast to be valid. Moreover, the
    // dereference is only valid because Chromium builds with
    // -fno-strict-aliasing.
    alignas(uint64_t) uint8_t array[44] = {};
    [[maybe_unused]] uint32_t v1 =
        *reinterpret_cast<const uint32_t*>(array);  // Front.
    [[maybe_unused]] uint64_t v2 =
        *reinterpret_cast<const uint64_t*>(array + 16);  // Middle.
  })

  {
    uint8_t array[44] = {};
    [[maybe_unused]] uint32_t v1 =
        base::U32FromLittleEndian(base::span(array).first<4u>());  // Front.
    [[maybe_unused]] uint64_t v2 = base::U64FromLittleEndian(
        base::span(array).subspan<16u, 8u>());  // Middle.
  }

  UNSAFE_BUFFERS({
    std::string str = "hello world";
    func_with_const_ptr_size(reinterpret_cast<const uint8_t*>(str.data()),
                             str.size());
    func_with_mut_ptr_size(reinterpret_cast<uint8_t*>(str.data()), str.size());
  })

  {
    std::string str = "hello world";
    base::span<const uint8_t> bytes = base::as_byte_span(str);
    func_with_const_ptr_size(bytes.data(), bytes.size());
    base::span<uint8_t> mut_bytes = base::as_writable_byte_span(str);
    func_with_mut_ptr_size(mut_bytes.data(), mut_bytes.size());

    // Replace pointer and size with a span, though.
    func_with_const_span(base::as_byte_span(str));
    func_with_mut_span(base::as_writable_byte_span(str));
  }

  UNSAFE_BUFFERS({
    uint8_t array[8];
    uint64_t val;
    two_byte_arrays(array, reinterpret_cast<const uint8_t*>(&val));
  })

  {
    uint8_t array[8];
    uint64_t val;
    base::span<uint8_t> val_span = base::byte_span_from_ref(val);
    two_byte_arrays(array, val_span.data());

    // Replace an unbounded pointer a span, though.
    two_byte_spans(base::span(array), base::byte_span_from_ref(val));
  }
}

TEST(SpanTest, Printing) {
  struct S {
    std::string ToString() const { return "S()"; }
  };

  // Gtest prints values in the spans. Chars are special.
  EXPECT_EQ(testing::PrintToString(base::span({1, 2, 3})), "[1, 2, 3]");
  EXPECT_EQ(testing::PrintToString(base::span({S(), S()})), "[S(), S()]");
  EXPECT_EQ(testing::PrintToString(base::span({'a', 'b', 'c'})), "[\"abc\"]");
  EXPECT_EQ(testing::PrintToString(base::span({'a', 'b', 'c', '\0'})),
            std::string_view("[\"abc\0\"]", 8u));
  EXPECT_EQ(testing::PrintToString(base::span({'a', 'b', '\0', 'c', '\0'})),
            std::string_view("[\"ab\0c\0\"]", 9u));
  EXPECT_EQ(testing::PrintToString(base::span<int>()), "[]");
  EXPECT_EQ(testing::PrintToString(base::span<char>()), "[\"\"]");

  EXPECT_EQ(testing::PrintToString(base::span({u'a', u'b', u'c'})),
            "[u\"abc\"]");
  EXPECT_EQ(testing::PrintToString(base::span({L'a', L'b', L'c'})),
            "[L\"abc\"]");

  // Base prints values in spans. Chars are special.
  EXPECT_EQ(base::ToString(base::span({1, 2, 3})), "[1, 2, 3]");
  EXPECT_EQ(base::ToString(base::span({S(), S()})), "[S(), S()]");
  EXPECT_EQ(base::ToString(base::span({'a', 'b', 'c'})), "[\"abc\"]");
  EXPECT_EQ(base::ToString(base::span({'a', 'b', 'c', '\0'})),
            std::string_view("[\"abc\0\"]", 8u));
  EXPECT_EQ(base::ToString(base::span({'a', 'b', '\0', 'c', '\0'})),
            std::string_view("[\"ab\0c\0\"]", 9u));
  EXPECT_EQ(base::ToString(base::span<int>()), "[]");
  EXPECT_EQ(base::ToString(base::span<char>()), "[\"\"]");

  EXPECT_EQ(base::ToString(base::span({u'a', u'b', u'c'})), "[u\"abc\"]");
  EXPECT_EQ(base::ToString(base::span({L'a', L'b', L'c'})), "[L\"abc\"]");
}

}  // namespace base

// Test for compatibility with std::span<>, in case some third-party
// API decides to use it. The size() and data() convention should mean
// that everyone's spans are compatible with each other.
TEST(SpanTest, FromStdSpan) {
  int kData[] = {10, 11, 12};
  std::span<const int> std_span(kData);
  std::span<int> mut_std_span(kData);
  std::span<const int, 3u> fixed_std_span(kData);
  std::span<int, 3u> mut_fixed_std_span(kData);

  // Tests *implicit* conversions through assignment construction.
  {
    base::span<const int> base_span = std_span;
    EXPECT_EQ(base_span.size(), 3u);
    EXPECT_EQ(base_span.data(), kData);
  }
  {
    base::span<const int> base_span = mut_std_span;
    EXPECT_EQ(base_span.size(), 3u);
    EXPECT_EQ(base_span.data(), kData);
  }
  {
    base::span<const int> base_span = fixed_std_span;
    EXPECT_EQ(base_span.size(), 3u);
    EXPECT_EQ(base_span.data(), kData);
  }
  {
    base::span<const int> base_span = mut_fixed_std_span;
    EXPECT_EQ(base_span.size(), 3u);
    EXPECT_EQ(base_span.data(), kData);
  }

  {
    base::span<const int, 3u> base_span = fixed_std_span;
    EXPECT_EQ(base_span.size(), 3u);
    EXPECT_EQ(base_span.data(), kData);
  }
  {
    base::span<const int, 3u> base_span = mut_fixed_std_span;
    EXPECT_EQ(base_span.size(), 3u);
    EXPECT_EQ(base_span.data(), kData);
  }
  {
    base::span<int, 3u> base_span = mut_fixed_std_span;
    EXPECT_EQ(base_span.size(), 3u);
    EXPECT_EQ(base_span.data(), kData);
  }

  {
    auto base_made_span = base::make_span(std_span);
    EXPECT_EQ(base_made_span.size(), 3u);
    EXPECT_EQ(base_made_span.data(), kData);
  }
  {
    auto base_byte_span = base::as_byte_span(std_span);
    EXPECT_EQ(base_byte_span.size(), sizeof(int) * 3u);
    EXPECT_EQ(base_byte_span.data(), reinterpret_cast<const uint8_t*>(kData));
  }
}

TEST(SpanTest, ToStdSpan) {
  int kData[] = {10, 11, 12};
  base::span<const int> base_span(kData);
  base::span<int> mut_base_span(kData);
  base::span<const int, 3u> fixed_base_span(kData);
  base::span<int, 3u> mut_fixed_base_span(kData);

  // Tests *implicit* conversions through assignment construction.
  {
    std::span<const int> std_span = base_span;
    EXPECT_EQ(std_span.size(), 3u);
    EXPECT_EQ(std_span.data(), kData);
  }
  {
    std::span<const int> std_span = mut_base_span;
    EXPECT_EQ(std_span.size(), 3u);
    EXPECT_EQ(std_span.data(), kData);
  }
  {
    std::span<const int> std_span = fixed_base_span;
    EXPECT_EQ(std_span.size(), 3u);
    EXPECT_EQ(std_span.data(), kData);
  }
  {
    std::span<const int> std_span = mut_fixed_base_span;
    EXPECT_EQ(std_span.size(), 3u);
    EXPECT_EQ(std_span.data(), kData);
  }

  {
    std::span<const int, 3u> std_span = fixed_base_span;
    EXPECT_EQ(std_span.size(), 3u);
    EXPECT_EQ(std_span.data(), kData);
  }
  {
    std::span<const int, 3u> std_span = mut_fixed_base_span;
    EXPECT_EQ(std_span.size(), 3u);
    EXPECT_EQ(std_span.data(), kData);
  }
  {
    std::span<int, 3u> std_span = mut_fixed_base_span;
    EXPECT_EQ(std_span.size(), 3u);
    EXPECT_EQ(std_span.data(), kData);
  }

  // no make_span() or as_byte_span() in std::span.
}
