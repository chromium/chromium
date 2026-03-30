// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a simplified version of Chromium's
// //base/containers/span_unittest.cc. To minimize dependencies, several tests
// and APIs were removed:
//
// 1. std::span Interoperability (FromStdSpan, ToStdSpan)
//    Rationale: Avoids triggering Chromium PRESUBMIT.py checks for `std::span`.
//    We don't currently need this interoperability. We can revisit this if
//    needed in the future.
//
// 2. Type Punning (reinterpret_span)
//    Rationale: This has been ported from //base to support safe type-punning
//    in PartitionAlloc.
//
// 3. Abseil Dependencies (AbslHash)
//    Rationale: PartitionAlloc doesn't depend on Abseil.
//
// 4. Volatile Memory Support (ConstructFromVolatileArray, CopyFromVolatile)
//    Rationale: PartitionAlloc manages standard heap memory where volatile
//    semantics (typically for hardware/MMIO) are not required. Removing this
//    minimizes the template surface and dependency on volatile traits.
//
// 5. String/C-String Utilities (FromCString, FromCStringEmpty,
//    FromCStringEmbeddedNul, FromCStringOtherTypes)
//    Rationale: PA strictly handles byte-level memory, so text-focused
//    utilities and their tests are unnecessary. This avoids importing
//    base::cstring_view and its dependencies, which are likely not needed.
//
// 6. Exhaustive API & Integration Testing
//    Rationale: Several validation tests in //base depend on complex GMock
//    matchers or other //base utilities not available in the PA standalone
//    build. While core logic tests (e.g., comparison operators) are retained
//    for parity, these "extra" integration tests were omitted to minimize
//    the test-only dependency footprint.
//    TODO(sergiosolano): Port more tests if needed.
// -----------------------------------------------------------------------------

#include "partition_alloc/partition_alloc_base/containers/span.h"

#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <concepts>
#include <iterator>
#include <memory>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "partition_alloc/partition_alloc_base/bit_cast.h"
#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "partition_alloc/partition_alloc_base/containers/checked_iterators.h"
#include "partition_alloc/partition_alloc_base/debug/alias.h"
#include "partition_alloc/partition_alloc_base/test/gtest_util.h"
#include "partition_alloc/use_death_tests.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Pointwise;

namespace partition_alloc::internal::base {

TEST(PASpanTest, DeductionGuides) {
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

  // C-style arrays.
  static_assert(std::is_same_v<decltype(span(std::declval<const int (&)[3]>())),
                               span<const int, 3>>);
  static_assert(
      std::is_same_v<decltype(span(std::declval<const int (&&)[3]>())),
                     span<const int, 3>>);
  static_assert(
      std::is_same_v<decltype(span(std::declval<int (&)[3]>())), span<int, 3>>);
  static_assert(std::is_same_v<decltype(span(std::declval<int (&&)[3]>())),
                               span<const int, 3>>);

  // std::array<const T, N>.
  static_assert(
      std::is_same_v<decltype(span(
                         std::declval<const std::array<const bool, 3>&>())),
                     span<const bool, 3>>);
  static_assert(
      std::is_same_v<decltype(span(
                         std::declval<const std::array<const bool, 3>&&>())),
                     span<const bool, 3>>);
  static_assert(
      std::is_same_v<decltype(span(std::declval<std::array<const bool, 3>&>())),
                     span<const bool, 3>>);
  static_assert(std::is_same_v<
                decltype(span(std::declval<std::array<const bool, 3>&&>())),
                span<const bool, 3>>);

  // std::array<T, N>.
  static_assert(
      std::is_same_v<decltype(span(std::declval<const std::array<bool, 3>&>())),
                     span<const bool, 3>>);
  static_assert(std::is_same_v<
                decltype(span(std::declval<const std::array<bool, 3>&&>())),
                span<const bool, 3>>);
  static_assert(
      std::is_same_v<decltype(span(std::declval<std::array<bool, 3>&>())),
                     span<bool, 3>>);
  static_assert(
      std::is_same_v<decltype(span(std::declval<std::array<bool, 3>&&>())),
                     span<const bool, 3>>);

  // std::string.
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

  // std::u16string.
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

  // std::ranges::subrange<const T*>.
  static_assert(std::is_same_v<
                decltype(span(
                    std::declval<const std::ranges::subrange<const int*>&>())),
                span<const int>>);
  static_assert(std::is_same_v<
                decltype(span(
                    std::declval<const std::ranges::subrange<const int*>&&>())),
                span<const int>>);
  static_assert(
      std::is_same_v<decltype(span(
                         std::declval<std::ranges::subrange<const int*>&>())),
                     span<const int>>);
  static_assert(
      std::is_same_v<decltype(span(
                         std::declval<std::ranges::subrange<const int*>&&>())),
                     span<const int>>);

  // std::ranges::subrange<T*>.
  static_assert(
      std::is_same_v<decltype(span(
                         std::declval<const std::ranges::subrange<int*>&>())),
                     span<int>>);
  static_assert(
      std::is_same_v<decltype(span(
                         std::declval<const std::ranges::subrange<int*>&&>())),
                     span<int>>);
  static_assert(std::is_same_v<
                decltype(span(std::declval<std::ranges::subrange<int*>&>())),
                span<int>>);
  static_assert(std::is_same_v<
                decltype(span(std::declval<std::ranges::subrange<int*>&&>())),
                span<int>>);
}

TEST(PASpanTest, DefaultConstructor) {
  span<int> dynamic_span;
  EXPECT_EQ(nullptr, dynamic_span.data());
  EXPECT_EQ(0u, dynamic_span.size());

  constexpr span<int, 0> static_span;
  static_assert(nullptr == static_span.data());
  static_assert(0 == static_span.size());
}

TEST(PASpanTest, ConstructFromDataAndSize) {
  constexpr int* kNull = nullptr;
  // SAFETY: zero size is correct when pointer argument is NULL.
  constexpr span<int> PA_UNSAFE_BUFFERS(empty_span(kNull, 0u));
  EXPECT_TRUE(empty_span.empty());
  EXPECT_EQ(nullptr, empty_span.data());

  std::vector<int> vector = {1, 1, 2, 3, 5, 8};

  // SAFETY: `vector.size()` describes valid portion of `vector.data()`.
  span<int> PA_UNSAFE_BUFFERS(dynamic_span(vector.data(), vector.size()));
  EXPECT_EQ(vector.data(), dynamic_span.data());
  EXPECT_EQ(vector.size(), dynamic_span.size());

  for (size_t i = 0; i < dynamic_span.size(); ++i) {
    EXPECT_EQ(vector[i], dynamic_span[i]);
  }

  // SAFETY: `vector.size()` describes valid portion of `vector.data()`.
  span<int, 6> PA_UNSAFE_BUFFERS(static_span(vector.data(), vector.size()));
  EXPECT_EQ(vector.data(), static_span.data());
  EXPECT_EQ(vector.size(), static_span.size());

  for (size_t i = 0; i < static_span.size(); ++i) {
    EXPECT_EQ(vector[i], static_span[i]);
  }
}

TEST(PASpanTest, ConstructFromDataAndZeroSize) {
  char* nullptr_to_char = nullptr;

  auto empty_span = PA_UNSAFE_BUFFERS(span<char>(nullptr_to_char, 0u));
  EXPECT_EQ(empty_span.size(), 0u);
  EXPECT_EQ(empty_span.data(), nullptr);
  EXPECT_TRUE(empty_span.empty());

  // We expect a `PA_DCHECK` to catch construction of a dangling span - let's
  // cover this expectation in a test, so that future PartitionAlloc
  // refactorings (e.g. maybe changing the internal pointer type) won't just
  // silently change this aspect of span behavior.
  PA_EXPECT_DCHECK_DEATH(
      { PA_UNSAFE_BUFFERS(span<char>(nullptr_to_char, 123u)); });
}

TEST(PASpanTest, ConstructFromIterAndSize) {
  constexpr int* kNull = nullptr;
  // SAFETY: zero size is correct when pointer argument is NULL.
  constexpr span<int> PA_UNSAFE_BUFFERS(empty_span(kNull, 0u));
  EXPECT_TRUE(empty_span.empty());
  EXPECT_EQ(nullptr, empty_span.data());

  std::vector<int> vector = {1, 1, 2, 3, 5, 8};

  // SAFETY: `vector.size()` describes valid bytes following `vector.begin()`.
  span<int> PA_UNSAFE_BUFFERS(dynamic_span(vector.begin(), vector.size()));
  EXPECT_EQ(vector.data(), dynamic_span.data());
  EXPECT_EQ(vector.size(), dynamic_span.size());

  for (size_t i = 0; i < dynamic_span.size(); ++i) {
    EXPECT_EQ(vector[i], dynamic_span[i]);
  }

  // SAFETY: `vector.size()` describes valid bytes following `vector.begin()`.
  span<int, 6> PA_UNSAFE_BUFFERS(static_span(vector.begin(), vector.size()));
  EXPECT_EQ(vector.data(), static_span.data());
  EXPECT_EQ(vector.size(), static_span.size());

  for (size_t i = 0; i < static_span.size(); ++i) {
    EXPECT_EQ(vector[i], static_span[i]);
  }
}

TEST(PASpanTest, ConstructFromIterPair) {
  constexpr int* kNull = nullptr;
  // SAFETY: required for test, NULL range valid.
  constexpr span<int> PA_UNSAFE_BUFFERS(empty_span(kNull, kNull));
  EXPECT_TRUE(empty_span.empty());
  EXPECT_EQ(nullptr, empty_span.data());

  std::vector<int> vector = {1, 1, 2, 3, 5, 8};

  // SAFETY: `vector.size()` describes valid portion of `vector.data()`,
  // thus one-half `vector.size()` is within this range.
  span<int> PA_UNSAFE_BUFFERS(
      dynamic_span(vector.begin(), vector.begin() + vector.size() / 2));
  EXPECT_EQ(vector.data(), dynamic_span.data());
  EXPECT_EQ(vector.size() / 2, dynamic_span.size());

  for (size_t i = 0; i < dynamic_span.size(); ++i) {
    EXPECT_EQ(vector[i], dynamic_span[i]);
  }

  // SAFETY: `vector.size()` describes valid portion of `vector.data()`,
  // thus one-half `vector.size()` is within this range.
  span<int, 3> PA_UNSAFE_BUFFERS(
      static_span(vector.begin(), vector.begin() + vector.size() / 2));
  EXPECT_EQ(vector.data(), static_span.data());
  EXPECT_EQ(vector.size() / 2, static_span.size());

  for (size_t i = 0; i < static_span.size(); ++i) {
    EXPECT_EQ(vector[i], static_span[i]);
  }
}

TEST(PASpanTest, AllowedConversionsFromStdArray) {
  // In the following assertions we use std::is_convertible_v<From, To>, which
  // for non-void types is equivalent to checking whether the following
  // expression is well-formed:
  //
  // T obj = std::declval<From>();
  //
  // In particular we are checking whether From is implicitly convertible to To,
  // which also implies that To is explicitly constructible from From.
  static_assert(
      std::is_convertible_v<std::array<int, 3>&, span<int>>,
      "Error: l-value reference to std::array<int> should be convertible to "
      "span<int> with dynamic extent.");
  static_assert(
      std::is_convertible_v<std::array<int, 3>&, span<int, 3>>,
      "Error: l-value reference to std::array<int> should be convertible to "
      "span<int> with the same static extent.");
  static_assert(
      std::is_convertible_v<std::array<int, 3>&, span<const int>>,
      "Error: l-value reference to std::array<int> should be convertible to "
      "span<const int> with dynamic extent.");
  static_assert(
      std::is_convertible_v<std::array<int, 3>&, span<const int, 3>>,
      "Error: l-value reference to std::array<int> should be convertible to "
      "span<const int> with the same static extent.");
  static_assert(
      std::is_convertible_v<const std::array<int, 3>&, span<const int>>,
      "Error: const l-value reference to std::array<int> should be convertible "
      "to span<const int> with dynamic extent.");
  static_assert(
      std::is_convertible_v<const std::array<int, 3>&, span<const int, 3>>,
      "Error: const l-value reference to std::array<int> should be convertible "
      "to span<const int> with the same static extent.");
  static_assert(
      std::is_convertible_v<std::array<const int, 3>&, span<const int>>,
      "Error: l-value reference to std::array<const int> should be convertible "
      "to span<const int> with dynamic extent.");
  static_assert(
      std::is_convertible_v<std::array<const int, 3>&, span<const int, 3>>,
      "Error: l-value reference to std::array<const int> should be convertible "
      "to span<const int> with the same static extent.");
  static_assert(
      std::is_convertible_v<const std::array<const int, 3>&, span<const int>>,
      "Error: const l-value reference to std::array<const int> should be "
      "convertible to span<const int> with dynamic extent.");
  static_assert(
      std::is_convertible_v<const std::array<const int, 3>&,
                            span<const int, 3>>,
      "Error: const l-value reference to std::array<const int> should be "
      "convertible to span<const int> with the same static extent.");
}

TEST(PASpanTest, DisallowedConstructionsFromStdArray) {
  // In the following assertions we use !std::is_constructible_v<T, Args>, which
  // is equivalent to checking whether the following expression is malformed:
  //
  // T obj(std::declval<Args>()...);
  //
  // In particular we are checking that T is not explicitly constructible from
  // Args, which also implies that T is not implicitly constructible from Args
  // as well.
  static_assert(
      !std::is_constructible_v<span<int>, const std::array<int, 3>&>,
      "Error: span<int> with dynamic extent should not be constructible from "
      "const l-value reference to std::array<int>");

  static_assert(
      !std::is_constructible_v<span<int>, std::array<const int, 3>&>,
      "Error: span<int> with dynamic extent should not be constructible from "
      "l-value reference to std::array<const int>");

  static_assert(
      !std::is_constructible_v<span<int>, const std::array<const int, 3>&>,
      "Error: span<int> with dynamic extent should not be constructible from "
      "const l-value reference to std::array<const int>");

  static_assert(
      !std::is_constructible_v<span<int, 2>, std::array<int, 3>&>,
      "Error: span<int> with static extent should not be constructible from "
      "l-value reference to std::array<int> with different extent");

  static_assert(
      !std::is_constructible_v<span<int, 4>, std::array<int, 3>&>,
      "Error: span<int> with dynamic extent should not be constructible from "
      "l-value reference to std::array<int> with different extent");

  static_assert(!std::is_constructible_v<span<int>, std::array<bool, 3>&>,
                "Error: span<int> with dynamic extent should not be "
                "constructible from l-value reference to std::array<bool>");
}

TEST(PASpanTest, ConstructFromConstexprArray) {
  static constexpr int kArray[] = {5, 4, 3, 2, 1};

  constexpr span<const int> dynamic_span(kArray);
  static_assert(kArray == dynamic_span.data());
  static_assert(std::size(kArray) == dynamic_span.size());

  static_assert(kArray[0] == dynamic_span[0]);
  static_assert(kArray[1] == dynamic_span[1]);
  static_assert(kArray[2] == dynamic_span[2]);
  static_assert(kArray[3] == dynamic_span[3]);
  static_assert(kArray[4] == dynamic_span[4]);

  constexpr span<const int, std::size(kArray)> static_span(kArray);
  static_assert(kArray == static_span.data());
  static_assert(std::size(kArray) == static_span.size());

  static_assert(kArray[0] == static_span[0]);
  static_assert(kArray[1] == static_span[1]);
  static_assert(kArray[2] == static_span[2]);
  static_assert(kArray[3] == static_span[3]);
  static_assert(kArray[4] == static_span[4]);
}

TEST(PASpanTest, ConstructFromArray) {
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
}

TEST(PASpanTest, ConstructFromStdArray) {
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

TEST(PASpanTest, ConstructFromInitializerList) {
  std::initializer_list<int> il = {1, 1, 2, 3, 5, 8};

  span<const int> const_span(il);
  EXPECT_EQ(il.begin(), const_span.data());
  EXPECT_EQ(il.size(), const_span.size());

  for (size_t i = 0; i < const_span.size(); ++i) {
    // SAFETY: `il.begin()` is valid to index up to `il.size()`, and
    // `il.size()` equals `const_span.size()`, so `il.begin()` is valid
    // to index up to `const_span.size()` per above loop condition.
    EXPECT_EQ(PA_UNSAFE_BUFFERS(il.begin()[i]), const_span[i]);
  }

  // SAFETY: [il.begin()..il.end()) is a valid range over `il`.
  span<const int, 6> PA_UNSAFE_BUFFERS(static_span(il.begin(), il.end()));
  EXPECT_EQ(il.begin(), static_span.data());
  EXPECT_EQ(il.size(), static_span.size());

  for (size_t i = 0; i < static_span.size(); ++i) {
    // SAFETY: `il.begin()` is valid to index up to `il.size()`, and
    // `il.size()` equals `static_span.size()`, so `il.begin()` is valid
    // to index up to `static_span.size()` per above loop condition.
    EXPECT_EQ(PA_UNSAFE_BUFFERS(il.begin()[i]), static_span[i]);
  }
}

TEST(PASpanTest, ConstructFromStdString) {
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
  span<char, 6> PA_UNSAFE_BUFFERS(static_span(str.data(), str.size()));
  EXPECT_EQ(str.data(), static_span.data());
  EXPECT_EQ(str.size(), static_span.size());

  for (size_t i = 0; i < static_span.size(); ++i) {
    EXPECT_EQ(str[i], static_span[i]);
  }
}

TEST(PASpanTest, ConstructFromConstContainer) {
  const std::vector<int> vector = {1, 1, 2, 3, 5, 8};

  span<const int> const_span(vector);
  EXPECT_EQ(vector.data(), const_span.data());
  EXPECT_EQ(vector.size(), const_span.size());

  for (size_t i = 0; i < const_span.size(); ++i) {
    EXPECT_EQ(vector[i], const_span[i]);
  }

  // SAFETY: vector.size() describes valid portion of vector.data().
  span<const int, 6> PA_UNSAFE_BUFFERS(
      static_span(vector.data(), vector.size()));
  EXPECT_EQ(vector.data(), static_span.data());
  EXPECT_EQ(vector.size(), static_span.size());

  for (size_t i = 0; i < static_span.size(); ++i) {
    EXPECT_EQ(vector[i], static_span[i]);
  }
}

TEST(PASpanTest, ConstructFromContainer) {
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
  span<int, 6> PA_UNSAFE_BUFFERS(static_span(vector.data(), vector.size()));
  EXPECT_EQ(vector.data(), static_span.data());
  EXPECT_EQ(vector.size(), static_span.size());

  for (size_t i = 0; i < static_span.size(); ++i) {
    EXPECT_EQ(vector[i], static_span[i]);
  }
}

TEST(PASpanTest, ConstructFromRange) {
  struct Range {
    using iterator = span<const int>::iterator;
    iterator begin() const { return span(arr_).begin(); }
    iterator end() const { return span(arr_).end(); }

    std::array<const int, 3u> arr_ = {1, 2, 3};
  };
  static_assert(std::ranges::contiguous_range<Range>);
  {
    Range r;
    auto s = span(r);
    static_assert(std::same_as<decltype(s), span<const int>>);
    EXPECT_EQ(s, span<const int>({1, 2, 3}));

    // Implicit from modern range with dynamic size to dynamic span.
    span<const int> imp = r;
    EXPECT_EQ(imp, span<const int>({1, 2, 3}));
  }
  {
    Range r;
    auto s = span<const int, 3u>(r);
    EXPECT_EQ(s, span<const int>({1, 2, 3}));

    // Explicit from modern range with dynamic size to fixed span.
    static_assert(!std::convertible_to<decltype(r), span<const int, 3u>>);
    span<const int, 3u> imp(r);
    EXPECT_EQ(imp, span<const int>({1, 2, 3}));
  }

  using FixedRange = const std::array<int, 3>;
  static_assert(std::ranges::contiguous_range<FixedRange>);
  static_assert(std::ranges::sized_range<FixedRange>);
  {
    FixedRange r = {1, 2, 3};
    auto s = span(r);
    static_assert(std::same_as<decltype(s), span<const int, 3>>);
    EXPECT_EQ(s, span<const int>({1, 2, 3}));

    // Implicit from fixed size to dynamic span.
    span<const int> imp = r;
    EXPECT_EQ(imp, span<const int>({1, 2, 3}));
  }
  {
    FixedRange r = {1, 2, 3};
    auto s = span<const int, 3u>(r);
    EXPECT_EQ(s, span<const int>({1, 2, 3}));

    // Implicit from fixed size to fixed span.
    span<const int, 3u> imp = r;
    EXPECT_EQ(imp, span<const int>({1, 2, 3}));
  }

  // Construction from std::vectors.

  {
    // Implicit.
    static_assert(std::convertible_to<const std::vector<int>, span<const int>>);
    const std::vector<int> i{1, 2, 3};
    span<const int> s = i;
    EXPECT_EQ(s, span(i));
  }
  {
    // Explicit.
    static_assert(
        !std::convertible_to<const std::vector<int>, span<const int, 3u>>);
    static_assert(
        std::constructible_from<span<const int, 3u>, const std::vector<int>>);
    const std::vector<int> i{1, 2, 3};
    span<const int, 3u> s(i);
    EXPECT_EQ(s, span(i));
  }

  // vector<bool> is special and can't be converted to a span since it does not
  // actually hold an array of `bool`.
  static_assert(
      !std::constructible_from<span<const bool>, const std::vector<bool>>);
  static_assert(
      !std::constructible_from<span<const bool, 3u>, const std::vector<bool>>);
}

TEST(PASpanTest, ConstructFromSubrange) {
  std::vector<int> v = {1, 2, 3, 4, 5};
  EXPECT_THAT(span(std::ranges::subrange(v)), ElementsAre(1, 2, 3, 4, 5));
}

TEST(PASpanTest, FromRefOfMutableStackVariable) {
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

TEST(PASpanTest, FromRefOfConstStackVariable) {
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

// The sorts of constructions-from-short-lifetime-objects that trigger lifetime
// warnings with dangling refs should not warn when there is no dangling.
TEST(PASpanTest, NoLifetimeWarnings) {
  // Test each of dynamic- and fixed-extent spans.
  static constexpr auto l1 = [](span<const int> s) { return s[0] == 1; };
  static constexpr auto l2 = [](span<const int, 3> s) { return s[0] == 1; };

  // C-style array, `std::array`, and `std::initializer_list` usage is safe when
  // the produced span is consumed before the full expression ends.
  [] {
    int arr[3] = {1, 2, 3};
    return l1(arr);
  }();
  [] {
    int arr[3] = {1, 2, 3};
    return l2(arr);
  }();
  [[maybe_unused]] auto a = l1(std::to_array({1, 2, 3}));
  [[maybe_unused]] auto b = l2(std::to_array({1, 2, 3}));
  [[maybe_unused]] auto c = l1({1, 2, 3});
  [[maybe_unused]] auto d =
      l2(span<const int, 3>({1, 2, 3}));  // Constructor is explicit.

  // `std::string_view` is safe with a compile-time string constant, because it
  // refers directly to the character array in the binary.
  [[maybe_unused]] auto e = span<const char>(std::string_view("123"));
  [[maybe_unused]] auto f = span<const char, 3>(std::string_view("123"));

  // It's also safe with an lvalue `std::string`.
  std::string s = "123";
  [[maybe_unused]] auto g = span<const char>(std::string_view(s));
  [[maybe_unused]] auto h = span<const char>(std::string_view(s));

  // Non-std:: helpers should also allow safe usage.
  [[maybe_unused]] auto i = as_byte_span(std::string_view(s));
}

TEST(PASpanTest, FromRefOfRValue) {
  int x = 123;
  static_assert(std::is_same_v<decltype(span_from_ref(std::move(x))),
                               span<const int, 1u>>);
  EXPECT_EQ(&x, span_from_ref(std::move(x)).data());
}

TEST(PASpanTest, ConvertNonConstIntegralToConst) {
  std::vector<int> vector = {1, 1, 2, 3, 5, 8};

  // SAFETY: `vector.size()` describes valid portion of `vector.data()`.
  span<int> PA_UNSAFE_BUFFERS(int_span(vector.data(), vector.size()));
  span<const int> const_span(int_span);
  EXPECT_EQ(int_span.size(), const_span.size());

  EXPECT_THAT(const_span, Pointwise(Eq(), int_span));

  // SAFETY: `vector.size()` describes valid portion of `vector.data()`.
  span<int, 6> PA_UNSAFE_BUFFERS(static_int_span(vector.data(), vector.size()));
  span<const int, 6> static_const_span(static_int_span);
  EXPECT_THAT(static_const_span, Pointwise(Eq(), static_int_span));
}

TEST(PASpanTest, ConvertNonConstPointerToConst) {
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
  span<int*, 3> PA_UNSAFE_BUFFERS(
      static_non_const_pointer_span(vector.data(), vector.size()));
  EXPECT_THAT(static_non_const_pointer_span, Pointwise(Eq(), vector));
  span<int* const, 3> static_const_pointer_span(static_non_const_pointer_span);
  EXPECT_THAT(static_const_pointer_span,
              Pointwise(Eq(), static_non_const_pointer_span));
}

TEST(PASpanTest, ConvertBetweenEquivalentTypes) {
  std::vector<int32_t> vector = {2, 4, 8, 16, 32};

  span<int32_t> int32_t_span(vector);
  span<int> converted_span(int32_t_span);
  EXPECT_EQ(int32_t_span.data(), converted_span.data());
  EXPECT_EQ(int32_t_span.size(), converted_span.size());

  // SAFETY: `vector.size()` describes valid portion of `vector.data()`.
  span<int32_t, 5> PA_UNSAFE_BUFFERS(
      static_int32_t_span(vector.data(), vector.size()));
  span<int, 5> static_converted_span(static_int32_t_span);
  EXPECT_EQ(static_int32_t_span.data(), static_converted_span.data());
  EXPECT_EQ(static_int32_t_span.size(), static_converted_span.size());
}

TEST(PASpanTest, TemplatedFirst) {
  static constexpr int array[] = {1, 2, 3};
  constexpr span<const int, 3> span(array);

  {
    constexpr auto subspan = span.first<0>();
    static_assert(span.data() == subspan.data());
    static_assert(0 == subspan.size());
    static_assert(0 == decltype(subspan)::extent);
  }

  {
    constexpr auto subspan = span.first<1>();
    static_assert(span.data() == subspan.data());
    static_assert(1 == subspan.size());
    static_assert(1 == decltype(subspan)::extent);
    static_assert(1 == subspan[0]);
  }

  {
    constexpr auto subspan = span.first<2>();
    static_assert(span.data() == subspan.data());
    static_assert(2 == subspan.size());
    static_assert(2 == decltype(subspan)::extent);
    static_assert(1 == subspan[0]);
    static_assert(2 == subspan[1]);
  }

  {
    constexpr auto subspan = span.first<3>();
    static_assert(span.data() == subspan.data());
    static_assert(3 == subspan.size());
    static_assert(3 == decltype(subspan)::extent);
    static_assert(1 == subspan[0]);
    static_assert(2 == subspan[1]);
    static_assert(3 == subspan[2]);
  }
}

TEST(PASpanTest, TemplatedLast) {
  static constexpr int array[] = {1, 2, 3};
  constexpr span<const int, 3> span(array);

  {
    constexpr auto subspan = span.last<0>();
    // SAFETY: static_assert() doesn't execute code at runtime.
    static_assert(PA_UNSAFE_BUFFERS(span.data() + 3) == subspan.data());
    static_assert(0 == subspan.size());
    static_assert(0 == decltype(subspan)::extent);
  }

  {
    constexpr auto subspan = span.last<1>();
    // SAFETY: static_assert() doesn't execute code at runtime.
    static_assert(PA_UNSAFE_BUFFERS(span.data() + 2) == subspan.data());
    static_assert(1 == subspan.size());
    static_assert(1 == decltype(subspan)::extent);
    static_assert(3 == subspan[0]);
  }

  {
    constexpr auto subspan = span.last<2>();
    // SAFETY: static_assert() doesn't execute code at runtime.
    static_assert(PA_UNSAFE_BUFFERS(span.data() + 1) == subspan.data());
    static_assert(2 == subspan.size());
    static_assert(2 == decltype(subspan)::extent);
    static_assert(2 == subspan[0]);
    static_assert(3 == subspan[1]);
  }

  {
    constexpr auto subspan = span.last<3>();
    static_assert(span.data() == subspan.data());
    static_assert(3 == subspan.size());
    static_assert(3 == decltype(subspan)::extent);
    static_assert(1 == subspan[0]);
    static_assert(2 == subspan[1]);
    static_assert(3 == subspan[2]);
  }
}

TEST(PASpanTest, TemplatedSubspan) {
  static constexpr int array[] = {1, 2, 3};
  constexpr span<const int, 3> span(array);

  {
    constexpr auto subspan = span.subspan<0>();
    static_assert(span.data() == subspan.data());
    static_assert(3 == subspan.size());
    static_assert(3 == decltype(subspan)::extent);
    static_assert(1 == subspan[0]);
    static_assert(2 == subspan[1]);
    static_assert(3 == subspan[2]);
  }

  {
    constexpr auto subspan = span.subspan<1>();
    // SAFETY: static_assert() doesn't execute code at runtime.
    static_assert(PA_UNSAFE_BUFFERS(span.data() + 1) == subspan.data());
    static_assert(2 == subspan.size());
    static_assert(2 == decltype(subspan)::extent);
    static_assert(2 == subspan[0]);
    static_assert(3 == subspan[1]);
  }

  {
    constexpr auto subspan = span.subspan<2>();
    // SAFETY: static_assert() doesn't execute code at runtime.
    static_assert(PA_UNSAFE_BUFFERS(span.data() + 2) == subspan.data());
    static_assert(1 == subspan.size());
    static_assert(1 == decltype(subspan)::extent);
    static_assert(3 == subspan[0]);
  }

  {
    constexpr auto subspan = span.subspan<3>();
    // SAFETY: static_assert() doesn't execute code at runtime.
    static_assert(PA_UNSAFE_BUFFERS(span.data() + 3) == subspan.data());
    static_assert(0 == subspan.size());
    static_assert(0 == decltype(subspan)::extent);
  }

  {
    constexpr auto subspan = span.subspan<0, 0>();
    static_assert(span.data() == subspan.data());
    static_assert(0 == subspan.size());
    static_assert(0 == decltype(subspan)::extent);
  }

  {
    constexpr auto subspan = span.subspan<1, 0>();
    // SAFETY: static_assert() doesn't execute code at runtime.
    static_assert(PA_UNSAFE_BUFFERS(span.data() + 1) == subspan.data());
    static_assert(0 == subspan.size());
    static_assert(0 == decltype(subspan)::extent);
  }

  {
    constexpr auto subspan = span.subspan<2, 0>();
    // SAFETY: static_assert() doesn't execute code at runtime.
    static_assert(PA_UNSAFE_BUFFERS(span.data() + 2) == subspan.data());
    static_assert(0 == subspan.size());
    static_assert(0 == decltype(subspan)::extent);
  }

  {
    constexpr auto subspan = span.subspan<0, 1>();
    static_assert(span.data() == subspan.data());
    static_assert(1 == subspan.size());
    static_assert(1 == decltype(subspan)::extent);
    static_assert(1 == subspan[0]);
  }

  {
    constexpr auto subspan = span.subspan<1, 1>();
    // SAFETY: static_assert() doesn't execute code at runtime.
    static_assert(PA_UNSAFE_BUFFERS(span.data() + 1) == subspan.data());
    static_assert(1 == subspan.size());
    static_assert(1 == decltype(subspan)::extent);
    static_assert(2 == subspan[0]);
  }

  {
    constexpr auto subspan = span.subspan<2, 1>();
    // SAFETY: static_assert() doesn't execute code at runtime.
    static_assert(PA_UNSAFE_BUFFERS(span.data() + 2) == subspan.data());
    static_assert(1 == subspan.size());
    static_assert(1 == decltype(subspan)::extent);
    static_assert(3 == subspan[0]);
  }

  {
    constexpr auto subspan = span.subspan<0, 2>();
    static_assert(span.data() == subspan.data());
    static_assert(2 == subspan.size());
    static_assert(2 == decltype(subspan)::extent);
    static_assert(1 == subspan[0]);
    static_assert(2 == subspan[1]);
  }

  {
    constexpr auto subspan = span.subspan<1, 2>();
    // SAFETY: static_assert() doesn't execute code at runtime.
    static_assert(PA_UNSAFE_BUFFERS(span.data() + 1) == subspan.data());
    static_assert(2 == subspan.size());
    static_assert(2 == decltype(subspan)::extent);
    static_assert(2 == subspan[0]);
    static_assert(3 == subspan[1]);
  }

  {
    constexpr auto subspan = span.subspan<0, 3>();
    static_assert(span.data() == subspan.data());
    static_assert(3 == subspan.size());
    static_assert(3 == decltype(subspan)::extent);
    static_assert(1 == subspan[0]);
    static_assert(2 == subspan[1]);
    static_assert(3 == subspan[2]);
  }
}

TEST(PASpanTest, SubscriptedBeginIterator) {
  std::array<int, 3> array = {1, 2, 3};
  span<const int> const_span(array);
  for (size_t i = 0; i < const_span.size(); ++i) {
    // SAFETY: The range starting at `const_span.begin()` is valid up
    // to `const_span.size()`.
    EXPECT_EQ(array[i], PA_UNSAFE_BUFFERS(const_span.begin()[i]));
  }

  span<int> mutable_span(array);
  for (size_t i = 0; i < mutable_span.size(); ++i) {
    // SAFETY: The range starting at `mutable_span.begin()` is valid up
    // to `mutable_span.size()`.
    EXPECT_EQ(array[i], PA_UNSAFE_BUFFERS(mutable_span.begin()[i]));
  }
}

TEST(PASpanTest, TemplatedFirstOnDynamicSpan) {
  int array[] = {1, 2, 3};
  span<const int> span(array);

  {
    auto subspan = span.first<0>();
    EXPECT_EQ(span.data(), subspan.data());
    static_assert(0 == decltype(subspan)::extent);
    EXPECT_THAT(subspan, IsEmpty());
  }

  {
    auto subspan = span.first<1>();
    EXPECT_EQ(span.data(), subspan.data());
    static_assert(1 == decltype(subspan)::extent);
    EXPECT_THAT(subspan, ElementsAre(1));
  }

  {
    auto subspan = span.first<2>();
    EXPECT_EQ(span.data(), subspan.data());
    static_assert(2 == decltype(subspan)::extent);
    EXPECT_THAT(subspan, ElementsAre(1, 2));
  }

  {
    auto subspan = span.first<3>();
    EXPECT_EQ(span.data(), subspan.data());
    static_assert(3 == decltype(subspan)::extent);
    EXPECT_THAT(subspan, ElementsAre(1, 2, 3));
  }
}

TEST(PASpanTest, TemplatedLastOnDynamicSpan) {
  int array[] = {1, 2, 3};
  span<int> span(array);

  {
    auto subspan = span.last<0>();
    // `array` has three elements, so `span` has three elements, so
    // `span.data() + 3` points to one byte beyond the object as allowed
    // per standards.
    EXPECT_EQ(PA_UNSAFE_BUFFERS(span.data() + 3), subspan.data());
    static_assert(0 == decltype(subspan)::extent);
    EXPECT_THAT(subspan, IsEmpty());
  }

  {
    auto subspan = span.last<1>();
    // `array` has three elements, so `span` has three elements, so
    // `span.data() + 2` points within it.
    EXPECT_EQ(PA_UNSAFE_BUFFERS(span.data() + 2), subspan.data());
    static_assert(1 == decltype(subspan)::extent);
    EXPECT_THAT(subspan, ElementsAre(3));
  }

  {
    auto subspan = span.last<2>();
    // `array` has three elements, so `span` has three elements, so
    // `span.data() + 1` points within it.
    EXPECT_EQ(PA_UNSAFE_BUFFERS(span.data() + 1), subspan.data());
    static_assert(2 == decltype(subspan)::extent);
    EXPECT_THAT(subspan, ElementsAre(2, 3));
  }

  {
    auto subspan = span.last<3>();
    EXPECT_EQ(span.data(), subspan.data());
    static_assert(3 == decltype(subspan)::extent);
    EXPECT_THAT(subspan, ElementsAre(1, 2, 3));
  }
}

TEST(PASpanTest, TemplatedSubspanOnDynamicSpan) {
  int array[] = {1, 2, 3};
  span<int> span(array);

  {
    auto subspan = span.subspan<0>();
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_THAT(subspan, ElementsAre(1, 2, 3));
  }

  {
    auto subspan = span.subspan<1>();
    // SAFETY: `array` has three elements, so `span` has three elements, so
    // `span.data() + 1` points within it.
    EXPECT_EQ(PA_UNSAFE_BUFFERS(span.data() + 1), subspan.data());
    EXPECT_THAT(subspan, ElementsAre(2, 3));
  }

  {
    auto subspan = span.subspan<2>();
    // SAFETY: `array` has three elements, so `span` has three elements, so
    // `span.data() + 2` points within it.
    EXPECT_EQ(PA_UNSAFE_BUFFERS(span.data() + 2), subspan.data());
    EXPECT_THAT(subspan, ElementsAre(3));
  }

  {
    auto subspan = span.subspan<3>();
    // SAFETY: `array` has three elements, so `span` has three elements, so
    // `span.data() + 3` points to one byte beyond the object as permitted by
    // C++ specification.
    EXPECT_EQ(PA_UNSAFE_BUFFERS(span.data() + 3), subspan.data());
    EXPECT_THAT(subspan, IsEmpty());
  }

  {
    auto subspan = span.subspan<0, 0>();
    EXPECT_EQ(span.data(), subspan.data());
    static_assert(0 == decltype(subspan)::extent);
    EXPECT_THAT(subspan, IsEmpty());
  }

  {
    auto subspan = span.subspan<1, 0>();
    // SAFETY: `array` has three elements, so `span` has three elements, so
    // `span.data() + 1` points within it.
    EXPECT_EQ(PA_UNSAFE_BUFFERS(span.data() + 1), subspan.data());
    static_assert(0 == decltype(subspan)::extent);
    EXPECT_THAT(subspan, IsEmpty());
  }

  {
    auto subspan = span.subspan<2, 0>();
    // SAFETY: `array` has three elements, so `span` has three elements, so
    // `span.data() + 2` points within it.
    EXPECT_EQ(PA_UNSAFE_BUFFERS(span.data() + 2), subspan.data());
    static_assert(0 == decltype(subspan)::extent);
    EXPECT_THAT(subspan, IsEmpty());
  }

  {
    auto subspan = span.subspan<0, 1>();
    EXPECT_EQ(span.data(), subspan.data());
    static_assert(1 == decltype(subspan)::extent);
    EXPECT_THAT(subspan, ElementsAre(1));
  }

  {
    auto subspan = span.subspan<1, 1>();
    // SAFETY: `array` has three elements, so `span` has three elements, so
    // `span.data() + 1` points within it.
    EXPECT_EQ(PA_UNSAFE_BUFFERS(span.data() + 1), subspan.data());
    static_assert(1 == decltype(subspan)::extent);
    EXPECT_THAT(subspan, ElementsAre(2));
  }

  {
    auto subspan = span.subspan<2, 1>();
    // SAFETY: `array` has three elements, so `span` has three elements, so
    // `span.data() + 2` points within it.
    EXPECT_EQ(PA_UNSAFE_BUFFERS(span.data() + 2), subspan.data());
    static_assert(1 == decltype(subspan)::extent);
    EXPECT_THAT(subspan, ElementsAre(3));
  }

  {
    auto subspan = span.subspan<0, 2>();
    EXPECT_EQ(span.data(), subspan.data());
    static_assert(2 == decltype(subspan)::extent);
    EXPECT_THAT(subspan, ElementsAre(1, 2));
  }

  {
    auto subspan = span.subspan<1, 2>();
    // SAFETY: `array` has three elements, so `span` has three elements, so
    // `span.data() + 1` points within it.
    EXPECT_EQ(PA_UNSAFE_BUFFERS(span.data() + 1), subspan.data());
    static_assert(2 == decltype(subspan)::extent);
    EXPECT_THAT(subspan, ElementsAre(2, 3));
  }

  {
    auto subspan = span.subspan<0, 3>();
    EXPECT_EQ(span.data(), subspan.data());
    static_assert(3 == decltype(subspan)::extent);
    EXPECT_THAT(subspan, ElementsAre(1, 2, 3));
  }
}

TEST(PASpanTest, First) {
  int array[] = {1, 2, 3};
  span<int> span(array);

  {
    auto subspan = span.first(0u);
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_THAT(subspan, IsEmpty());
  }

  {
    auto subspan = span.first(1u);
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_THAT(subspan, ElementsAre(1));
  }

  {
    auto subspan = span.first(2u);
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_THAT(subspan, ElementsAre(1, 2));
  }

  {
    auto subspan = span.first(3u);
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_THAT(subspan, ElementsAre(1, 2, 3));
  }
}

TEST(PASpanTest, Last) {
  int array[] = {1, 2, 3};
  span<int> span(array);

  {
    auto subspan = span.last(0u);
    // SAFETY: `array` has three elements, so `span` has three elements, so
    // `span.data() + 3` points to one byte beyond the object, as permitted by
    // C++ specification.
    EXPECT_EQ(PA_UNSAFE_BUFFERS(span.data() + 3), subspan.data());
    EXPECT_THAT(subspan, IsEmpty());
  }

  {
    auto subspan = span.last(1u);
    // SAFETY: `array` has three elements, so `span` has three elements, so
    // `span.data() + 2` points within it.
    EXPECT_EQ(PA_UNSAFE_BUFFERS(span.data() + 2), subspan.data());
    EXPECT_THAT(subspan, ElementsAre(3));
  }

  {
    auto subspan = span.last(2u);
    // SAFETY: `array` has three elements, so `span` has three elements, so
    // `span.data() + 1` points within it.
    EXPECT_EQ(PA_UNSAFE_BUFFERS(span.data() + 1), subspan.data());
    EXPECT_THAT(subspan, ElementsAre(2, 3));
  }

  {
    auto subspan = span.last(3u);
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_THAT(subspan, ElementsAre(1, 2, 3));
  }
}

TEST(PASpanTest, Subspan) {
  int array[] = {1, 2, 3};
  span<int> span(array);

  {
    auto subspan = span.subspan(0u);
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_THAT(subspan, ElementsAre(1, 2, 3));
  }

  {
    auto subspan = span.subspan(1u);
    // SAFETY: `array` has three elements, so `span` has three elements, so
    // `span.data() + 1` points within it.
    EXPECT_EQ(PA_UNSAFE_BUFFERS(span.data() + 1), subspan.data());
    EXPECT_THAT(subspan, ElementsAre(2, 3));
  }

  {
    auto subspan = span.subspan(2u);
    // SAFETY: `array` has three elements, so `span` has three elements, so
    // `span.data() + 2` points within it.
    EXPECT_EQ(PA_UNSAFE_BUFFERS(span.data() + 2), subspan.data());
    EXPECT_THAT(subspan, ElementsAre(3));
  }

  {
    auto subspan = span.subspan(3u);
    // SAFETY: `array` has three elements, so `span` has three elements, so
    // `span.data() + 3` points to one byte beyond the object, as permitted by
    // C++ specification.
    EXPECT_EQ(PA_UNSAFE_BUFFERS(span.data() + 3), subspan.data());
    EXPECT_THAT(subspan, IsEmpty());
  }

  {
    auto subspan = span.subspan(0u, 0u);
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_THAT(subspan, IsEmpty());
  }

  {
    auto subspan = span.subspan(1u, 0u);
    // SAFETY: `array` has three elements, so `span` has three elements, so
    // `span.data() + 1` points within it.
    EXPECT_EQ(PA_UNSAFE_BUFFERS(span.data() + 1), subspan.data());
    EXPECT_THAT(subspan, IsEmpty());
  }

  {
    auto subspan = span.subspan(2u, 0u);
    // SAFETY: `array` has three elements, so `span` has three elements, so
    // `span.data() + 2` points within it.
    EXPECT_EQ(PA_UNSAFE_BUFFERS(span.data() + 2), subspan.data());
    EXPECT_THAT(subspan, IsEmpty());
  }

  {
    auto subspan = span.subspan(0u, 1u);
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_THAT(subspan, ElementsAre(1));
  }

  {
    auto subspan = span.subspan(1u, 1u);
    // SAFETY: `array` has three elements, so `span` has three elements, so
    // `span.data() + 1` points within it.
    EXPECT_EQ(PA_UNSAFE_BUFFERS(span.data() + 1), subspan.data());
    EXPECT_THAT(subspan, ElementsAre(2));
  }

  {
    auto subspan = span.subspan(2u, 1u);
    // SAFETY: `array` has three elements, so `span` has three elements, so
    // `span.data() + 2` points within it.
    EXPECT_EQ(PA_UNSAFE_BUFFERS(span.data() + 2), subspan.data());
    EXPECT_THAT(subspan, ElementsAre(3));
  }

  {
    auto subspan = span.subspan(0u, 2u);
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_THAT(subspan, ElementsAre(1, 2));
  }

  {
    auto subspan = span.subspan(1u, 2u);
    // SAFETY: `array` has three elements, so `span` has three elements, so
    // `span.data() + 1` points within it.
    EXPECT_EQ(PA_UNSAFE_BUFFERS(span.data() + 1), subspan.data());
    EXPECT_THAT(subspan, ElementsAre(2, 3));
  }

  {
    auto subspan = span.subspan(0u, 3u);
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_THAT(subspan, ElementsAre(1, 2, 3));
  }
}

TEST(PASpanTest, ToFixedExtent) {
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

TEST(PASpanTest, Size) {
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

TEST(PASpanTest, SizeBytes) {
  {
    span<int> s;
    EXPECT_EQ(0u, s.size_bytes());
  }

  {
    int arr[] = {1, 2, 3};
    span<int> s(arr);
    EXPECT_EQ(sizeof(int) * 3u, s.size_bytes());
  }

  {
    int arr[] = {1, 2, 3};
    span<int, 3> s(arr);
    EXPECT_EQ(sizeof(int) * 3u, s.size_bytes());
  }
}

TEST(PASpanTest, Empty) {
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
    span<int> span_of_checked_iterators = PA_UNSAFE_BUFFERS({s.end(), s.end()});
    EXPECT_TRUE(span_of_checked_iterators.empty());
  }
}

TEST(PASpanTest, OperatorAt) {
  static constexpr int kArray[] = {1, 6, 1, 8, 0};
  constexpr span<const int> s(kArray);

  static_assert(&kArray[0] == &s[0],
                "span[0] does not refer to the same element as kArray[0]");
  static_assert(&kArray[1] == &s[1],
                "span[1] does not refer to the same element as kArray[1]");
  static_assert(&kArray[2] == &s[2],
                "span[2] does not refer to the same element as kArray[2]");
  static_assert(&kArray[3] == &s[3],
                "span[3] does not refer to the same element as kArray[3]");
  static_assert(&kArray[4] == &s[4],
                "span[4] does not refer to the same element as kArray[4]");
}

TEST(PASpanTest, Front) {
  static constexpr int kArray[] = {1, 6, 1, 8, 0};
  constexpr span<const int> s(kArray);
  static_assert(&kArray[0] == &s.front(),
                "span.front() does not refer to the same element as kArray[0]");
}

TEST(PASpanTest, Back) {
  static constexpr int kArray[] = {1, 6, 1, 8, 0};
  constexpr span<const int> s(kArray);
  static_assert(&kArray[4] == &s.back(),
                "span.back() does not refer to the same element as kArray[4]");
}

TEST(PASpanTest, Iterator) {
  static constexpr int kArray[] = {1, 6, 1, 8, 0};
  constexpr span<const int> span(kArray);

  std::vector<int> results;
  for (int i : span) {
    results.emplace_back(i);
  }
  EXPECT_THAT(results, ElementsAre(1, 6, 1, 8, 0));
}

TEST(PASpanTest, ConstexprIterator) {
  static constexpr int kArray[] = {1, 6, 1, 8, 0};
  constexpr span<const int> span(kArray);

  static_assert(std::ranges::equal(kArray, span));
  static_assert(1 == span.begin()[0]);
  static_assert(1 == *(span.begin() += 0));
  static_assert(6 == *(span.begin() += 1));

  static_assert(1 == *((span.begin() + 1) -= 1));
  static_assert(6 == *((span.begin() + 1) -= 0));

  static_assert(0 + span.begin() == span.begin() + 0);
  static_assert(1 + span.begin() == span.begin() + 1);
}

TEST(PASpanTest, ReverseIterator) {
  static constexpr int kArray[] = {1, 6, 1, 8, 0};
  constexpr span<const int> span(kArray);

  EXPECT_TRUE(std::ranges::equal(span.rbegin(), span.rend(),
                                 std::rbegin(kArray), std::rend(kArray)));
}

TEST(PASpanTest, AsBytes) {
  {
    constexpr int kArray[] = {2, 3, 5, 7, 11, 13};
    auto bytes_span = as_bytes(span(kArray));
    static_assert(std::is_same_v<decltype(bytes_span),
                                 span<const uint8_t, sizeof(kArray)>>);
    EXPECT_EQ(reinterpret_cast<const uint8_t*>(kArray), bytes_span.data());
    EXPECT_EQ(sizeof(kArray), bytes_span.size());
    EXPECT_EQ(bytes_span.size(), bytes_span.size_bytes());
  }
  {
    std::vector<int> vec = {1, 1, 2, 3, 5, 8};
    span<int> mutable_span(vec);
    auto bytes_span = as_bytes(mutable_span);
    static_assert(std::is_same_v<decltype(bytes_span), span<const uint8_t>>);
    EXPECT_EQ(reinterpret_cast<const uint8_t*>(vec.data()), bytes_span.data());
    EXPECT_EQ(sizeof(int) * vec.size(), bytes_span.size());
    EXPECT_EQ(bytes_span.size(), bytes_span.size_bytes());
  }
}

TEST(PASpanTest, AsWritableBytes) {
  std::vector<int> vec = {1, 1, 2, 3, 5, 8};
  span<int> mutable_span(vec);
  auto writable_bytes_span = as_writable_bytes(mutable_span);
  static_assert(std::is_same_v<decltype(writable_bytes_span), span<uint8_t>>);
  EXPECT_EQ(reinterpret_cast<uint8_t*>(vec.data()), writable_bytes_span.data());
  EXPECT_EQ(sizeof(int) * vec.size(), writable_bytes_span.size());
  EXPECT_EQ(writable_bytes_span.size(), writable_bytes_span.size_bytes());

  // Set the first entry of vec by writing through the span.
  std::ranges::fill(writable_bytes_span.first(sizeof(int)), 'a');
  static_assert(sizeof(int) == 4u);  // Otherwise char literal wrong below.
  EXPECT_EQ('aaaa', vec[0]);
}

TEST(PASpanTest, AsChars) {
  {
    constexpr int kArray[] = {2, 3, 5, 7, 11, 13};
    auto chars_span = as_chars(span(kArray));
    static_assert(
        std::is_same_v<decltype(chars_span), span<const char, sizeof(kArray)>>);
    EXPECT_EQ(reinterpret_cast<const char*>(kArray), chars_span.data());
    EXPECT_EQ(sizeof(kArray), chars_span.size());
    EXPECT_EQ(chars_span.size(), chars_span.size_bytes());
  }
  {
    std::vector<int> vec = {1, 1, 2, 3, 5, 8};
    span<int> mutable_span(vec);
    auto chars_span = as_chars(mutable_span);
    static_assert(std::is_same_v<decltype(chars_span), span<const char>>);
    EXPECT_EQ(reinterpret_cast<const char*>(vec.data()), chars_span.data());
    EXPECT_EQ(sizeof(int) * vec.size(), chars_span.size());
    EXPECT_EQ(chars_span.size(), chars_span.size_bytes());
  }
}

TEST(PASpanTest, AsWritableChars) {
  std::vector<int> vec = {1, 1, 2, 3, 5, 8};
  span<int> mutable_span(vec);
  auto writable_chars_span = as_writable_chars(mutable_span);
  static_assert(std::is_same_v<decltype(writable_chars_span), span<char>>);
  EXPECT_EQ(reinterpret_cast<char*>(vec.data()), writable_chars_span.data());
  EXPECT_EQ(sizeof(int) * vec.size(), writable_chars_span.size());
  EXPECT_EQ(writable_chars_span.size(), writable_chars_span.size_bytes());

  // Set the first entry of vec by writing through the span.
  std::ranges::fill(writable_chars_span.first(sizeof(int)), 'a');
  static_assert(sizeof(int) == 4u);  // Otherwise char literal wrong below.
  EXPECT_EQ('aaaa', vec[0]);
}

TEST(PASpanTest, AsByteSpan) {
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
      EXPECT_EQ(PA_UNSAFE_BUFFERS(byte_span[0u]), 2);
    }(as_byte_span({2, 3, 5, 7, 11, 13}));
  }
}

TEST(PASpanTest, AsWritableByteSpan) {
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
  // Result can be passed as rvalue.
  {
    int kMutArray[] = {2, 3, 5, 7, 11, 13};
    [](auto byte_span) {
      static_assert(
          std::is_same_v<decltype(byte_span), span<uint8_t, 6u * sizeof(int)>>);
      EXPECT_EQ(byte_span.size(), 6u * sizeof(int));
      // Little endian puts the low bits in the first byte.
      EXPECT_EQ(byte_span[0u], 2);
    }(as_writable_byte_span(kMutArray));
  }
}

// Create some structs to test byte span conversion from non-unique-rep objects.
struct NonUnique {
  float f = 0;
};
static_assert(!std::has_unique_object_representations_v<NonUnique>);

struct Allowlisted : NonUnique {};
static_assert(!std::has_unique_object_representations_v<Allowlisted>);

// Verify we can compile byte span conversions for the above with appropriate
// carve-outs.
template <>
inline constexpr bool kCanSafelyConvertToByteSpan<Allowlisted> = true;

TEST(PASpanTest, ByteSpansFromNonUnique) {
  // Note: This test is just a compile test, and assumes the functionality tests
  // above are sufficient to verify that aspect.

  {
    static_assert(!kCanSafelyConvertToByteSpan<NonUnique>);

    // `as_[writable_](bytes,chars)()`
    NonUnique arr[] = {{1}, {2}, {3}};
    span sp(arr);
    as_bytes(allow_nonunique_obj, sp);
    as_writable_bytes(allow_nonunique_obj, sp);
    as_chars(allow_nonunique_obj, sp);
    as_writable_chars(allow_nonunique_obj, sp);

    // `byte_span_from_ref()`
    const NonUnique const_obj;
    NonUnique obj;
    // Read-only
    byte_span_from_ref(allow_nonunique_obj, const_obj);
    // Writable
    byte_span_from_ref(allow_nonunique_obj, obj);

    // `as_[writable_]byte_span()`
    std::vector<NonUnique> vec;
    // Non-borrowed range
    as_byte_span(allow_nonunique_obj, std::vector<NonUnique>());
    // Borrowed range
    as_byte_span(allow_nonunique_obj, vec);
    as_writable_byte_span(allow_nonunique_obj, vec);
    // Array
    as_byte_span(allow_nonunique_obj, arr);
    as_writable_byte_span(allow_nonunique_obj, arr);
  }

  {
    static_assert(kCanSafelyConvertToByteSpan<Allowlisted>);

    // `as_[writable_](bytes,chars)()`
    Allowlisted arr[] = {{1}, {2}, {3}};
    span sp(arr);
    as_bytes(sp);
    as_writable_bytes(sp);
    as_chars(sp);
    as_writable_chars(sp);

    // `byte_span_from_ref()`
    const Allowlisted const_obj;
    Allowlisted obj;
    // Read-only
    byte_span_from_ref(const_obj);
    // Writable
    byte_span_from_ref(obj);

    // `as_[writable_]byte_span()`
    std::vector<Allowlisted> vec;
    // Non-borrowed range
    as_byte_span(std::vector<Allowlisted>());
    // Borrowed range
    as_byte_span(vec);
    as_writable_byte_span(vec);
    // Array
    as_byte_span(arr);
    as_writable_byte_span(arr);
  }
}

TEST(PASpanTest, ReinterpretSpan) {
  // Basic usage:
  {
    alignas(uint32_t) uint8_t kAlignedArray[] = {0, 1, 2, 3, 4, 5, 6, 7};
    auto s = span(kAlignedArray);
    auto rs = subtle::reinterpret_span<uint32_t>(s);
    static_assert(
        std::is_same_v<typename decltype(rs)::element_type, uint32_t>);
    EXPECT_EQ(rs.size(), 2u);
  }

  // Fixed extent:
  {
    alignas(uint32_t) uint8_t kAlignedArray[] = {0, 1, 2, 3, 4, 5, 6, 7};
    auto s = span<uint8_t, 8u>(kAlignedArray);
    auto rs = subtle::reinterpret_span<uint32_t>(s);
    static_assert(
        std::is_same_v<typename decltype(rs)::element_type, uint32_t>);
    static_assert(decltype(rs)::extent == 2u);
    EXPECT_EQ(rs.size(), 2u);
  }

  // Const => Const.
  {
    alignas(uint32_t) uint8_t kAlignedArray[] = {0, 1, 2, 3, 4, 5, 6, 7};
    auto s = span<const uint8_t>(kAlignedArray);
    auto rs = subtle::reinterpret_span<const uint32_t>(s);
    static_assert(
        std::is_same_v<typename decltype(rs)::element_type, const uint32_t>);
    EXPECT_EQ(rs.size(), 2u);
  }

  // Mutable => Const
  {
    alignas(uint32_t) uint8_t kAlignedArray[] = {0, 1, 2, 3, 4, 5, 6, 7};
    auto s = span<uint8_t>(kAlignedArray);
    auto rs = subtle::reinterpret_span<const uint32_t>(s);
    static_assert(
        std::is_same_v<typename decltype(rs)::element_type, const uint32_t>);
    EXPECT_EQ(rs.size(), 2u);
  }

  // Empty span.
  {
    auto s = span<uint8_t>();
    auto rs = subtle::reinterpret_span<uint32_t>(s);
    EXPECT_EQ(s.size(), 0u);
    EXPECT_EQ(s.data(), nullptr);
    static_assert(
        std::is_same_v<typename decltype(rs)::element_type, uint32_t>);
    EXPECT_EQ(rs.size(), 0u);
  }

  // Check unaligned empty span.
  // Empty slice (e.g. data() != nullptr, but size() == 0).
  {
    alignas(uint32_t) uint8_t kAlignedArray[] = {0, 1, 2, 3};
    auto s = span(kAlignedArray).subspan(1u, 0u);
    EXPECT_EQ(s.size(), 0u);
    EXPECT_NE(s.data(), nullptr);  // Unaligned, but valid pointer.
    PA_EXPECT_CHECK_DEATH(
        { [[maybe_unused]] auto rs = subtle::reinterpret_span<uint32_t>(s); });
  }

  // Alignment check fails:
  {
    alignas(uint32_t) uint8_t kAlignedArray[] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
    [[maybe_unused]] auto s = span(kAlignedArray).subspan(1u, 4u);
    if (alignof(uint32_t) > 1) {
      PA_EXPECT_CHECK_DEATH(
          { [[maybe_unused]] auto r = subtle::reinterpret_span<uint32_t>(s); });
    }
  }

  // Size check fails:
  {
    alignas(uint32_t) uint8_t kAlignedArray[] = {0, 0, 0, 0, 0, 0, 0, 0};
    [[maybe_unused]] auto s = span(kAlignedArray).first(7u);
    PA_EXPECT_CHECK_DEATH(
        { [[maybe_unused]] auto r = subtle::reinterpret_span<uint32_t>(s); });
  }

  // Data integrity after round-trip.
  {
    uint32_t data[] = {0x12345678, 0x9ABCDEF0};
    auto bytes = as_byte_span(data);
    auto reconstructed = subtle::reinterpret_span<const uint32_t>(bytes);
    EXPECT_EQ(reconstructed[0], data[0]);
    EXPECT_EQ(reconstructed[1], data[1]);
  }
}

TEST(PASpanTest, EnsureConstexprGoodness) {
  static constexpr std::array<int, 5> kArray = {5, 4, 3, 2, 1};
  constexpr span<const int> constexpr_span(kArray);
  const size_t size = 2;

  const size_t start = 1;
  constexpr span<const int> subspan = constexpr_span.subspan(start, size);
  for (size_t i = 0; i < subspan.size(); ++i) {
    EXPECT_EQ(PA_UNSAFE_BUFFERS(kArray[start + i]), subspan[i]);
  }

  constexpr span<const int> firsts = constexpr_span.first(size);
  for (size_t i = 0; i < firsts.size(); ++i) {
    EXPECT_EQ(PA_UNSAFE_BUFFERS(kArray[i]), firsts[i]);
  }

  constexpr span<const int> lasts = constexpr_span.last(size);
  for (size_t i = 0; i < lasts.size(); ++i) {
    const size_t j = (std::size(kArray) - size) + i;
    EXPECT_EQ(PA_UNSAFE_BUFFERS(kArray[j]), lasts[i]);
  }

  constexpr int item = constexpr_span[size];
  EXPECT_EQ(PA_UNSAFE_BUFFERS(kArray[size]), item);
}

TEST(PASpanTest, OutOfBoundsDeath) {
  constexpr span<int, 0> kEmptySpan;
  PA_EXPECT_CHECK_DEATH({ kEmptySpan.first(1u); });
  PA_EXPECT_CHECK_DEATH({ kEmptySpan.last(1u); });
  PA_EXPECT_CHECK_DEATH({ kEmptySpan.subspan(1u); });

  constexpr span<int> kEmptyDynamicSpan;
  PA_EXPECT_CHECK_DEATH({ kEmptyDynamicSpan[0]; });
  PA_EXPECT_CHECK_DEATH({ kEmptyDynamicSpan.front(); });
  PA_EXPECT_CHECK_DEATH({ kEmptyDynamicSpan.first(1u); });
  PA_EXPECT_CHECK_DEATH({ kEmptyDynamicSpan.last(1u); });
  PA_EXPECT_CHECK_DEATH({ kEmptyDynamicSpan.back(); });
  PA_EXPECT_CHECK_DEATH({ kEmptyDynamicSpan.subspan(1u); });

  static constexpr int kArray[] = {0, 1, 2};
  constexpr span<const int> kNonEmptyDynamicSpan(kArray);
  EXPECT_EQ(3U, kNonEmptyDynamicSpan.size());
  PA_EXPECT_CHECK_DEATH({ kNonEmptyDynamicSpan[4]; });
  PA_EXPECT_CHECK_DEATH({ kNonEmptyDynamicSpan.subspan(10u); });
  PA_EXPECT_CHECK_DEATH({ kNonEmptyDynamicSpan.subspan(1u, 7u); });

  size_t minus_one = static_cast<size_t>(-1);
  PA_EXPECT_CHECK_DEATH({ kNonEmptyDynamicSpan.subspan(minus_one); });
  PA_EXPECT_CHECK_DEATH(
      { kNonEmptyDynamicSpan.subspan(minus_one, minus_one); });
  PA_EXPECT_CHECK_DEATH({ kNonEmptyDynamicSpan.subspan(minus_one, 1u); });

  // Span's iterators should be checked. To confirm the crashes come from the
  // iterator checks and not stray memory accesses, we create spans that are
  // backed by larger arrays.
  int array1[] = {1, 2, 3, 4};
  int array2[] = {1, 2, 3, 4};
  span<int> span_len2 = span(array1).first(2u);
  span<int> span_len3 = span(array2).first(3u);
  PA_EXPECT_CHECK_DEATH({ *span_len2.end(); });
  PA_EXPECT_CHECK_DEATH({ span_len2.begin()[2]; });
  PA_EXPECT_CHECK_DEATH({ (span_len2.begin() + 3); });
  PA_EXPECT_CHECK_DEATH({ (span_len2.begin() - 1); });
  PA_EXPECT_CHECK_DEATH({ (span_len2.end() + 1); });

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
  PA_EXPECT_CHECK_DEATH(
      { std::copy(span_len3.begin(), span_len3.end(), span_len2.begin()); });
  PA_EXPECT_CHECK_DEATH({ std::ranges::copy(span_len3, span_len2.begin()); });
  PA_EXPECT_CHECK_DEATH(
      { std::copy_n(span_len3.begin(), 3, span_len2.begin()); });

  // Copying more values than exist in the source.
  PA_EXPECT_CHECK_DEATH(
      { std::copy_n(span_len2.begin(), 3, span_len3.begin()); });
}

TEST(PASpanTest, Sort) {
  int array[] = {5, 4, 3, 2, 1};

  span<int> dynamic_span = array;
  std::ranges::sort(dynamic_span);
  EXPECT_THAT(array, ElementsAre(1, 2, 3, 4, 5));
  std::sort(dynamic_span.rbegin(), dynamic_span.rend());
  EXPECT_THAT(array, ElementsAre(5, 4, 3, 2, 1));

  span<int, 5> static_span = array;
  std::sort(static_span.rbegin(), static_span.rend(), std::greater<>());
  EXPECT_THAT(array, ElementsAre(1, 2, 3, 4, 5));
  std::ranges::sort(static_span, std::greater<>());
  EXPECT_THAT(array, ElementsAre(5, 4, 3, 2, 1));
}

TEST(PASpanTest, SpanExtentConversions) {
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

TEST(PASpanTest, IteratorConversions) {
  static_assert(
      std::is_convertible_v<span<int>::iterator, span<const int>::iterator>,
      "Error: iterator should be convertible to const iterator");

  static_assert(
      !std::is_convertible_v<span<const int>::iterator, span<int>::iterator>,
      "Error: const iterator should not be convertible to iterator");
}

TEST(PASpanTest, Indexing) {
  int arr[] = {1, 2, 3};
  auto fixed_span = span<int, 3u>(arr);
  auto dyn_span = span<int>(arr);

  EXPECT_EQ(&fixed_span[0u], &arr[0u]);
  EXPECT_EQ(&fixed_span[2u], &arr[2u]);
  PA_EXPECT_CHECK_DEATH(debug::Alias(&fixed_span[3u]));

  EXPECT_EQ(&dyn_span[0u], &arr[0u]);
  EXPECT_EQ(&dyn_span[2u], &arr[2u]);
  PA_EXPECT_CHECK_DEATH(debug::Alias(&dyn_span[3u]));

  EXPECT_EQ(fixed_span.get_at(0u), &arr[0u]);
  EXPECT_EQ(fixed_span.get_at(2u), &arr[2u]);
  PA_EXPECT_CHECK_DEATH(debug::Alias(fixed_span.get_at(3u)));

  EXPECT_EQ(dyn_span.get_at(0u), &arr[0u]);
  EXPECT_EQ(dyn_span.get_at(2u), &arr[2u]);
  PA_EXPECT_CHECK_DEATH(debug::Alias(dyn_span.get_at(3u)));
}

TEST(PASpanTest, CopyFrom) {
  int arr[] = {1, 2, 3};
  span<int, 0> empty_static_span;
  span<int, 3> static_span = span(arr);

  std::vector<int> vec = {4, 5, 6};
  span<int> empty_dynamic_span;
  span<int> dynamic_span = span(vec);

  // Handle empty cases gracefully.
  // Dynamic size to static size requires an explicit conversion.
  empty_static_span.copy_from(*empty_dynamic_span.to_fixed_extent<0>());
  empty_dynamic_span.copy_from(empty_static_span);
  static_span.first(empty_static_span.size()).copy_from(empty_static_span);
  dynamic_span.first(empty_dynamic_span.size()).copy_from(empty_dynamic_span);
  EXPECT_THAT(arr, ElementsAre(1, 2, 3));
  EXPECT_THAT(vec, ElementsAre(4, 5, 6));

  // Test too small destinations.
  PA_EXPECT_CHECK_DEATH(empty_static_span.copy_from(dynamic_span));
  PA_EXPECT_CHECK_DEATH(empty_dynamic_span.copy_from(static_span));
  PA_EXPECT_CHECK_DEATH(empty_dynamic_span.copy_from(dynamic_span));
  PA_EXPECT_CHECK_DEATH(dynamic_span.last(2u).copy_from(static_span));

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
    explicit NonTrivial(int o) : i(o) {}
    NonTrivial(const NonTrivial& o) : i(o) {}
    NonTrivial& operator=(const NonTrivial& o) {
      i = int{o};
      return *this;
    }
    explicit operator int() const { return i; }
    bool operator==(int j) const { return i == j; }
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
    NonTrivial long_arr_is_long[] = {
        NonTrivial(1), NonTrivial(2), NonTrivial(3), NonTrivial(4),
        NonTrivial(5), NonTrivial(6), NonTrivial(7)};
    auto left = span(long_arr_is_long).first<5>();
    auto right = span(long_arr_is_long).last<5>();
    left.copy_from(right);
    EXPECT_THAT(long_arr_is_long, ElementsAre(3, 4, 5, 6, 7, 6, 7));
  }
  {
    NonTrivial long_arr_is_long[] = {
        NonTrivial(1), NonTrivial(2), NonTrivial(3), NonTrivial(4),
        NonTrivial(5), NonTrivial(6), NonTrivial(7)};
    auto left = span(long_arr_is_long).first<5>();
    auto right = span(long_arr_is_long).last<5>();
    right.copy_from(left);
    EXPECT_THAT(long_arr_is_long, ElementsAre(1, 2, 1, 2, 3, 4, 5));
  }
  {
    NonTrivial long_arr_is_long[] = {
        NonTrivial(1), NonTrivial(2), NonTrivial(3), NonTrivial(4),
        NonTrivial(5), NonTrivial(6), NonTrivial(7)};
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
    NonTrivial long_arr_is_long[] = {
        NonTrivial(1), NonTrivial(2), NonTrivial(3), NonTrivial(4),
        NonTrivial(5), NonTrivial(6), NonTrivial(7)};
    auto left = span<NonTrivial>(long_arr_is_long).first(5u);
    auto right = span<NonTrivial>(long_arr_is_long).last(5u);
    left.copy_from(right);
    EXPECT_THAT(long_arr_is_long, ElementsAre(3, 4, 5, 6, 7, 6, 7));
  }
  {
    NonTrivial long_arr_is_long[] = {
        NonTrivial(1), NonTrivial(2), NonTrivial(3), NonTrivial(4),
        NonTrivial(5), NonTrivial(6), NonTrivial(7)};
    auto left = span<NonTrivial>(long_arr_is_long).first(5u);
    auto right = span<NonTrivial>(long_arr_is_long).last(5u);
    right.copy_from(left);
    EXPECT_THAT(long_arr_is_long, ElementsAre(1, 2, 1, 2, 3, 4, 5));
  }
  {
    NonTrivial long_arr_is_long[] = {
        NonTrivial(1), NonTrivial(2), NonTrivial(3), NonTrivial(4),
        NonTrivial(5), NonTrivial(6), NonTrivial(7)};
    auto left = span<NonTrivial>(long_arr_is_long).first(5u);
    left.copy_from(left);
    EXPECT_THAT(long_arr_is_long, ElementsAre(1, 2, 3, 4, 5, 6, 7));
  }

  // Verify that `copy_from()` works in a constexpr context.
  static constexpr auto s = span_from_cstring("abc");
  static constexpr auto fixed_c = [] {
    char arr_constexpr[3];
    span<char, 3> arr_s(arr_constexpr);
    arr_s.copy_from(s);
    return arr_s[1];
  }();
  static_assert(fixed_c == 'b');
  static constexpr auto dynamic_c = [] {
    char arr_constexpr[3];
    span<char, dynamic_extent> arr_s(arr_constexpr);
    arr_s.copy_from(s);
    return arr_s[2];
  }();
  static_assert(dynamic_c == 'c');
}

TEST(PASpanTest, CopyFromNonoverlapping) {
  int arr[] = {1, 2, 3};
  span<int, 0> empty_static_span;
  span<int, 3> static_span = span(arr);

  std::vector<int> vec = {4, 5, 6};
  span<int> empty_dynamic_span;
  span<int> dynamic_span = span(vec);

  // Handle empty cases gracefully.
  PA_UNSAFE_BUFFERS({
    empty_static_span.copy_from_nonoverlapping(empty_dynamic_span);
    empty_dynamic_span.copy_from_nonoverlapping(empty_static_span);
    static_span.first(empty_static_span.size())
        .copy_from_nonoverlapping(empty_static_span);
    dynamic_span.first(empty_dynamic_span.size())
        .copy_from_nonoverlapping(empty_dynamic_span);
    EXPECT_THAT(arr, ElementsAre(1, 2, 3));
    EXPECT_THAT(vec, ElementsAre(4, 5, 6));

    // Test too small destinations.
    PA_EXPECT_CHECK_DEATH(
        empty_static_span.copy_from_nonoverlapping(dynamic_span));
    PA_EXPECT_CHECK_DEATH(
        empty_dynamic_span.copy_from_nonoverlapping(static_span));
    PA_EXPECT_CHECK_DEATH(
        empty_dynamic_span.copy_from_nonoverlapping(dynamic_span));
    PA_EXPECT_CHECK_DEATH(
        dynamic_span.last(2u).copy_from_nonoverlapping(static_span));

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

  // Verify that `copy_from_nonoverlapping()` works in a constexpr context.
  static constexpr auto s = span_from_cstring("abc");
  static constexpr auto fixed_c = [] {
    char arr[3];
    span<char, 3> arr_s(arr);
    arr_s.copy_from_nonoverlapping(s);
    return arr_s[1];
  }();
  static_assert(fixed_c == 'b');
  static constexpr auto dynamic_c = [] {
    char arr[3];
    span<char, dynamic_extent> arr_s(arr);
    arr_s.copy_from_nonoverlapping(s);
    return arr_s[2];
  }();
  static_assert(dynamic_c == 'c');
}

TEST(PASpanTest, CopyFromConversion) {
  int arr[] = {1, 2, 3};
  span<int, 3> static_span = span(arr);

  std::vector<int> vec = {4, 5, 6};
  span<int> dynamic_span = span(vec);

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

TEST(PASpanTest, CopyPrefixFrom) {
  const int vals[] = {1, 2, 3, 4, 5};
  int arr[] = {1, 2, 3, 4, 5};
  span<int, 2> fixed2 = span(arr).first<2>();
  span<int, 3> fixed3 = span(arr).last<3>();
  span<int> dyn2 = span(arr).first(2u);
  span<int> dyn3 = span(arr).last(3u);

  // Copy from a larger buffer.
  PA_EXPECT_CHECK_DEATH(fixed2.copy_prefix_from(dyn3));
  PA_EXPECT_CHECK_DEATH(dyn2.copy_prefix_from(fixed3));
  PA_EXPECT_CHECK_DEATH(dyn2.copy_prefix_from(dyn3));

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

TEST(PASpanTest, SplitAt) {
  int arr[] = {1, 2, 3};
  span<int, 0> empty_static_span;
  span<int, 3> static_span = span(arr);

  std::vector<int> vec = {4, 5, 6};
  span<int> empty_dynamic_span;
  span<int> dynamic_span = span(vec);

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
    // SAFETY: `array` has three elements, so `static_span` has three
    // elements, so `static_span.data() + 1u` points within it.
    EXPECT_EQ(right.data(), PA_UNSAFE_BUFFERS(static_span.data() + 1u));
  }
  {
    auto [left, right] = static_span.split_at<3u>();
    static_assert(std::same_as<decltype(left), span<int, 3u>>);
    static_assert(std::same_as<decltype(right), span<int, 0u>>);
    EXPECT_EQ(left.data(), static_span.data());
    // SAFETY: `array` has three elements, so `static_span` has three
    // elements, so `static_span.data() + 3u` points to one byte beyond
    // the end of the object as permitted by C++ standard.
    EXPECT_EQ(right.data(), PA_UNSAFE_BUFFERS(static_span.data() + 3u));
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
    // SAFETY: `array` has three elements, so `dynamic_span` has three
    // elements, so `dynamic_span.data() + 1u` points within it.
    EXPECT_EQ(right.data(), PA_UNSAFE_BUFFERS(dynamic_span.data() + 1u));
    EXPECT_EQ(right.size(), 2u);
  }
  {
    auto [left, right] = dynamic_span.split_at<3u>();
    static_assert(std::same_as<decltype(left), span<int, 3u>>);
    static_assert(std::same_as<decltype(right), span<int>>);
    EXPECT_EQ(left.data(), dynamic_span.data());
    // SAFETY: `array` has three elements, so `dynamic_span` has three
    // elements, so `dynamic_span.data() + 3u` points to one byte beyond
    // the end of the object as permitted by C++ standard.
    EXPECT_EQ(right.data(), PA_UNSAFE_BUFFERS(dynamic_span.data() + 3u));
    EXPECT_EQ(right.size(), 0u);
  }
  // Invalid fixed-size split from dynamic will fail at runtime.
  PA_EXPECT_CHECK_DEATH({ dynamic_span.split_at<4u>(); });
}

TEST(PASpanTest, TakeFirst) {
  {
    span<int> empty;
    auto first = empty.take_first(0u);
    EXPECT_TRUE(first.empty());
    EXPECT_TRUE(empty.empty());
  }

  {
    std::vector<int> vec = {4, 5, 6};
    span<int> dynamic_span = span(vec);
    auto first = dynamic_span.take_first(0u);
    EXPECT_TRUE(first.empty());
    EXPECT_THAT(dynamic_span, ElementsAre(4, 5, 6));
  }
  {
    std::vector<int> vec = {4, 5, 6};
    span<int> dynamic_span = span(vec);
    auto first = dynamic_span.take_first(1u);
    EXPECT_THAT(first, ElementsAre(4));
    EXPECT_THAT(dynamic_span, ElementsAre(5, 6));
  }
  {
    std::vector<int> vec = {4, 5, 6};
    span<int> dynamic_span = span(vec);
    auto first = dynamic_span.take_first(3u);
    EXPECT_THAT(first, ElementsAre(4, 5, 6));
    EXPECT_TRUE(dynamic_span.empty());
  }
  {
    std::vector<int> vec = {4, 5, 6};
    [[maybe_unused]] span<int> dynamic_span = span(vec);
    // Invalid take will fail at runtime.
    PA_EXPECT_CHECK_DEATH({ dynamic_span.take_first(4u); });
  }

  // Fixed-size takes.
  {
    std::vector<int> vec = {4, 5, 6};
    span<int> dynamic_span = span(vec);
    auto first = dynamic_span.take_first<0>();
    static_assert(std::same_as<decltype(first), span<int, 0>>);
    EXPECT_THAT(dynamic_span, ElementsAre(4, 5, 6));
  }
  {
    std::vector<int> vec = {4, 5, 6};
    span<int> dynamic_span = span(vec);
    auto first = dynamic_span.take_first<1>();
    static_assert(std::same_as<decltype(first), span<int, 1>>);
    EXPECT_THAT(first, ElementsAre(4));
    EXPECT_THAT(dynamic_span, ElementsAre(5, 6));
  }
  {
    std::vector<int> vec = {4, 5, 6};
    span<int> dynamic_span = span(vec);
    auto first = dynamic_span.take_first<3>();
    static_assert(std::same_as<decltype(first), span<int, 3>>);
    EXPECT_THAT(first, ElementsAre(4, 5, 6));
    EXPECT_TRUE(dynamic_span.empty());
  }
  {
    std::vector<int> vec = {4, 5, 6};
    [[maybe_unused]] span<int> dynamic_span = span(vec);
    // Invalid fixed-size take will fail at runtime.
    PA_EXPECT_CHECK_DEATH({ dynamic_span.take_first<4>(); });
  }
}

TEST(PASpanTest, TakeFirstElem) {
  {
    [[maybe_unused]] span<int> empty;
    // Invalid take will fail at runtime.
    PA_EXPECT_CHECK_DEATH({ empty.take_first_elem(); });
  }

  {
    std::vector<int> vec = {4, 5, 6};
    span<int> dynamic_span = span(vec);
    auto first = dynamic_span.take_first_elem();
    static_assert(std::same_as<decltype(first), int>);
    EXPECT_EQ(first, 4);
    EXPECT_EQ(dynamic_span.size(), 2u);
    EXPECT_EQ(dynamic_span.front(), 5);
  }
}

TEST(PASpanTest, CompareEquality) {
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

TEST(PASpanTest, CompareOrdered) {
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

TEST(PASpanTest, GMockMacroCompatibility) {
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

TEST(PASpanTest, GTestMacroCompatibility) {
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

  // Alas, many desirable comparisons are still not possible. They
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
TEST(PASpanTest, Example_UnsafeBuffersPatterns) {
  struct Object {
    int a;
  };
  auto func_with_const_ptr_size = [](const uint8_t*, size_t) {};
  auto func_with_mut_ptr_size = [](uint8_t*, size_t) {};
  auto func_with_const_span = [](span<const uint8_t>) {};
  auto func_with_mut_span = [](span<uint8_t>) {};
  auto two_byte_arrays = [](const uint8_t*, const uint8_t*) {};
  auto two_byte_spans = [](span<const uint8_t>, span<const uint8_t>) {};

  PA_UNSAFE_BUFFERS({
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
    span<uint8_t> span1(array1);
    span1.take_first<4>().copy_from(span(array2).subspan<8, 4>());
    span1.copy_from(as_byte_span(array3).first<8>());

    {
      // Use `split_at()` to ensure `array1` is fully written.
      auto [from2, from3] = span(array1).split_at<4>();
      from2.copy_from(span(array2).subspan<8, 4>());
      from3.copy_from(as_byte_span(array3).first<8>());
    }
  }

  PA_UNSAFE_BUFFERS({
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
    [[maybe_unused]] Object array3[4];
    std::ranges::fill(array1, 0);
    std::ranges::fill(array2, 0);
    std::ranges::fill(as_writable_byte_span(array3), 0);
  }

  PA_UNSAFE_BUFFERS({
    uint8_t array1[12] = {};
    uint8_t array2[12] = {};
    [[maybe_unused]] bool eq = memcmp(array1, array2, sizeof(array1)) == 0;
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
    [[maybe_unused]] bool eq = span(array1) == array2;
    [[maybe_unused]] bool less = span(array1) < array2;

    // In tests.
    EXPECT_EQ(span(array1), array2);
  }

  PA_UNSAFE_BUFFERS({
    uint8_t array[44] = {};
    uint32_t v1;
    memcpy(&v1, array, sizeof(v1));  // Front.
    uint64_t v2;
    memcpy(&v2, array + 6, sizeof(v2));  // Middle.
  })

  {
    uint8_t array[44] = {};
    [[maybe_unused]] uint32_t v1;
    byte_span_from_ref(v1).copy_from(span(array).first<4u>());  // Front.
    [[maybe_unused]] uint64_t v2;
    byte_span_from_ref(v2).copy_from(span(array).subspan<6u, 8u>());  // Middle.
  }

  PA_UNSAFE_BUFFERS({
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
    [[maybe_unused]] uint32_t v1;
    byte_span_from_ref(v1).copy_from(span(array).first<4u>());  // Front.
    [[maybe_unused]] uint64_t v2;
    byte_span_from_ref(v2).copy_from(
        span(array).subspan<16u, 8u>());  // Middle.
  }

  PA_UNSAFE_BUFFERS({
    std::string str = "hello world";
    func_with_const_ptr_size(reinterpret_cast<const uint8_t*>(str.data()),
                             str.size());
    func_with_mut_ptr_size(reinterpret_cast<uint8_t*>(str.data()), str.size());
  })

  {
    std::string str = "hello world";
    span<const uint8_t> bytes = as_byte_span(str);
    func_with_const_ptr_size(bytes.data(), bytes.size());
    span<uint8_t> mut_bytes = as_writable_byte_span(str);
    func_with_mut_ptr_size(mut_bytes.data(), mut_bytes.size());

    // Replace pointer and size with a span, though.
    func_with_const_span(as_byte_span(str));
    func_with_mut_span(as_writable_byte_span(str));
  }

  PA_UNSAFE_BUFFERS({
    uint8_t array[8];
    uint64_t val;
    two_byte_arrays(array, reinterpret_cast<const uint8_t*>(&val));
  })

  {
    uint8_t array[8];
    uint64_t val;
    span<uint8_t> val_span = byte_span_from_ref(val);
    two_byte_arrays(array, val_span.data());

    // Replace an unbounded pointer a span, though.
    two_byte_spans(span(array), byte_span_from_ref(val));
  }
}

}  // namespace partition_alloc::internal::base
