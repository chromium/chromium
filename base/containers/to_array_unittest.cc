// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/to_array.h"

#include <memory>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/adapters.h"
#include "base/containers/span.h"
#include "base/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::test {

namespace {

using ::testing::ElementsAre;
using ::testing::IsNull;
using ::testing::Pointee;

TEST(ToArrayTest, IdentityFixedExtent) {
  const int kRawArray[] = {1, 2, 3};

  // From fixed-extent base::span
  base::span<const int, 3> fixed_span(kRawArray);
  auto arr1 = base::ToArray(fixed_span);
  static_assert(std::same_as<decltype(arr1), std::array<int, 3>>);
  EXPECT_THAT(arr1, ElementsAre(1, 2, 3));

  // From C-style array
  auto arr2 = base::ToArray(kRawArray);
  static_assert(std::same_as<decltype(arr2), std::array<int, 3>>);
  EXPECT_THAT(arr2, ElementsAre(1, 2, 3));

  // From std::array
  const std::array<int, 3> std_arr = {1, 2, 3};
  auto arr3 = base::ToArray(std_arr);
  static_assert(std::same_as<decltype(arr3), std::array<int, 3>>);
  EXPECT_THAT(arr3, ElementsAre(1, 2, 3));

  // From std::initializer_list
  auto arr4 = base::ToArray({1, 2, 3});
  static_assert(std::same_as<decltype(arr4), std::array<int, 3>>);
  EXPECT_THAT(arr4, ElementsAre(1, 2, 3));
}

TEST(ToArrayTest, IdentityDynamicExtent) {
  const int kRawArray[] = {1, 2, 3};

  // From dynamic-extent base::span
  base::span<const int> dynamic_span(kRawArray);
  auto arr1 = base::ToArray<3>(dynamic_span);
  static_assert(std::same_as<decltype(arr1), std::array<int, 3>>);
  EXPECT_THAT(arr1, ElementsAre(1, 2, 3));

  // From std::vector
  std::vector<int> vec = {1, 2, 3};
  auto arr2 = base::ToArray<3>(vec);
  static_assert(std::same_as<decltype(arr2), std::array<int, 3>>);
  EXPECT_THAT(arr2, ElementsAre(1, 2, 3));
}

TEST(ToArrayTest, CustomType) {
  const int kRawArray[] = {1, 2, 3};

  // Fixed-extent base::span with custom type
  base::span<const int, 3> fixed_span(kRawArray);
  auto arr1 = base::ToArray<int64_t>(fixed_span);
  static_assert(std::same_as<decltype(arr1), std::array<int64_t, 3>>);
  EXPECT_THAT(arr1, ElementsAre(1L, 2L, 3L));

  // Dynamic-extent base::span with custom type
  base::span<const int> dynamic_span(kRawArray);
  auto arr2 = base::ToArray<3, int64_t>(dynamic_span);
  static_assert(std::same_as<decltype(arr2), std::array<int64_t, 3>>);
  EXPECT_THAT(arr2, ElementsAre(1L, 2L, 3L));

  // Rvalue array with custom type
  auto arr3 = base::ToArray<std::string_view>({"foo", "bar", "baz"});
  static_assert(std::same_as<decltype(arr3), std::array<std::string_view, 3>>);
  EXPECT_THAT(arr3, ElementsAre("foo", "bar", "baz"));
}

TEST(ToArrayTest, Projection) {
  const int kRawArray[] = {1, 2, 3};

  // Fixed-extent base::span with projection
  base::span<const int, 3> fixed_span(kRawArray);
  auto arr1 = base::ToArray(fixed_span, [](int x) { return x * 2; });
  static_assert(std::same_as<decltype(arr1), std::array<int, 3>>);
  EXPECT_THAT(arr1, ElementsAre(2, 4, 6));

  // Dynamic-extent base::span with projection
  base::span<const int> dynamic_span(kRawArray);
  auto arr2 = base::ToArray<3>(dynamic_span, [](int x) { return x * 2; });
  static_assert(std::same_as<decltype(arr2), std::array<int, 3>>);
  EXPECT_THAT(arr2, ElementsAre(2, 4, 6));
}

TEST(ToArrayTest, MoveOnlyRvalueArray) {
  auto arr = base::ToArray({
      std::make_unique<int>(10),
      std::make_unique<int>(20),
      std::make_unique<int>(30),
  });
  static_assert(
      std::same_as<decltype(arr), std::array<std::unique_ptr<int>, 3>>);
  EXPECT_THAT(arr, ElementsAre(Pointee(10), Pointee(20), Pointee(30)));
}

TEST(ToArrayTest, MoveOnlyRange) {
  std::vector<std::unique_ptr<int>> vec;
  vec.push_back(std::make_unique<int>(1));
  vec.push_back(std::make_unique<int>(2));

  auto arr = base::ToArray<2>(std::views::as_rvalue(vec));
  static_assert(
      std::same_as<decltype(arr), std::array<std::unique_ptr<int>, 2>>);
  EXPECT_THAT(arr, ElementsAre(Pointee(1), Pointee(2)));
  // NOLINTNEXTLINE(bugprone-use-after-move)
  EXPECT_THAT(vec, ElementsAre(IsNull(), IsNull()));
}

TEST(ToArrayTest, ConstexprTest) {
  static constexpr int kArray[] = {1, 2, 3};
  static constexpr base::span<const int, 3> kFixedSpan(kArray);
  static constexpr base::span<const int> kDynamicSpan(kArray);

  static_assert(base::ToArray(kFixedSpan) == std::array{1, 2, 3});
  static_assert(base::ToArray<3>(kDynamicSpan) == std::array{1, 2, 3});
  static_assert(base::ToArray(kArray) == std::array{1, 2, 3});
  static_assert(base::ToArray({1, 2, 3}) == std::array{1, 2, 3});
  static_assert(base::ToArray(kFixedSpan, [](int x) { return x + 1; }) ==
                std::array{2, 3, 4});
  static_assert(base::ToArray<3>(kDynamicSpan, [](int x) { return x + 1; }) ==
                std::array{2, 3, 4});
}

TEST(ToArrayDeathTest, DynamicExtentSizeMismatch) {
  const int kArray[] = {1, 2, 3};
  base::span<const int> dynamic_span(kArray);

  EXPECT_CHECK_DEATH((base::ToArray<2>(dynamic_span)));
  EXPECT_CHECK_DEATH((base::ToArray<4>(dynamic_span)));
}

class ConstArrayMemberTestClass {
 public:
  explicit ConstArrayMemberTestClass(base::span<const int, 3> span_data)
      : data_(base::ToArray(span_data)) {}

  explicit ConstArrayMemberTestClass(base::span<const int> span_data)
      : data_(base::ToArray<3>(span_data)) {}

  const std::array<int, 3>& data() const { return data_; }

 private:
  const std::array<int, 3> data_;
};

TEST(ToArrayTest, ConstClassMemberInitialization) {
  const int kData[] = {10, 20, 30};

  ConstArrayMemberTestClass obj1{base::span<const int, 3>(kData)};
  EXPECT_THAT(obj1.data(), ElementsAre(10, 20, 30));

  ConstArrayMemberTestClass obj2{base::span<const int>(kData)};
  EXPECT_THAT(obj2.data(), ElementsAre(10, 20, 30));
}

}  // namespace

}  // namespace base::test
