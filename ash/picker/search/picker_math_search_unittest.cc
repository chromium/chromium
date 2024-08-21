// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/search/picker_math_search.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

using ::testing::AllOf;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Optional;
using ::testing::Property;
using ::testing::VariantWith;

TEST(PickerMathSearchTest, NoResult) {
  EXPECT_FALSE(PickerMathSearch(u"abc").has_value());
}

TEST(PickerMathSearchTest, OnePlusOneEqualsTwo) {
  EXPECT_THAT(
      PickerMathSearch(u"1 + 1"),
      Optional(AllOf(VariantWith<PickerTextResult>(
                         Field("text", &PickerTextResult::primary_text, u"2")),
                     VariantWith<PickerTextResult>(
                         Field("source", &PickerTextResult::source,
                               PickerTextResult::Source::kMath)))));
}

TEST(PickerMathSearchTest, ReturnsExamples) {
  std::vector<PickerSearchResult> results = PickerMathExamples();
  EXPECT_THAT(results, Not(IsEmpty()));
  EXPECT_THAT(
      results,
      Each(VariantWith<PickerSearchRequestResult>(AllOf(
          Field("primary_text", &PickerSearchRequestResult::primary_text,
                Not(IsEmpty())),
          Field("secondary_text", &PickerSearchRequestResult::secondary_text,
                Not(IsEmpty()))))));
}

}  // namespace
}  // namespace ash
