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
      Optional(AllOf(
          Property(
              "data", &PickerSearchResult::data,
              VariantWith<PickerSearchResult::TextData>(Field(
                  "text", &PickerSearchResult::TextData::primary_text, u"2"))),
          Property("data", &PickerSearchResult::data,
                   VariantWith<PickerSearchResult::TextData>(
                       Field("source", &PickerSearchResult::TextData::source,
                             PickerSearchResult::TextData::Source::kMath))))));
}

TEST(PickerMathSearchTest, ReturnsExamples) {
  std::vector<PickerSearchResult> results = PickerMathExamples();
  EXPECT_THAT(results, Not(IsEmpty()));
  EXPECT_THAT(
      results,
      Each(Property(
          "data", &PickerSearchResult::data,
          VariantWith<PickerSearchResult::SearchRequestData>(AllOf(
              Field("primary_text",
                    &PickerSearchResult::SearchRequestData::primary_text,
                    Not(IsEmpty())),
              Field("secondary_text",
                    &PickerSearchResult::SearchRequestData::secondary_text,
                    Not(IsEmpty())))))));
}

}  // namespace
}  // namespace ash
