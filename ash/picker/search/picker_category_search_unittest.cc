// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/search/picker_category_search.h"

#include <string>
#include <vector>

#include "ash/public/cpp/picker/picker_category.h"
#include "ash/public/cpp/picker/picker_search_result.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

using ::testing::Eq;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Pointwise;
using ::testing::Property;
using ::testing::VariantWith;

MATCHER(ResultMatchesCategory, "") {
  const auto& [result, category] = arg;
  return ExplainMatchResult(
      Property("data", &PickerSearchResult::data,
               VariantWith<PickerSearchResult::CategoryData>(Field(
                   "category", &PickerSearchResult::CategoryData::category,
                   category))),
      result, result_listener);
}

struct TestCase {
  std::vector<PickerCategory> available_categories;
  std::u16string query;
  std::vector<PickerCategory> expected_categories;
};

class PickerCategorySearchTest : public testing::TestWithParam<TestCase> {};

TEST_P(PickerCategorySearchTest, MatchesExpectedCategories) {
  const auto& [available_categories, query, expected_categories] = GetParam();
  EXPECT_THAT(PickerCategorySearch(available_categories, query),
              Pointwise(ResultMatchesCategory(), expected_categories));
}

INSTANTIATE_TEST_SUITE_P(
    ,
    PickerCategorySearchTest,
    testing::Values(
        // Exact match
        TestCase{
            .available_categories = {PickerCategory::kLocalFiles},
            .query = u"This Chromebook",
            .expected_categories = {PickerCategory::kLocalFiles},
        },
        // Case-insensitive match
        TestCase{
            .available_categories = {PickerCategory::kLocalFiles},
            .query = u"tHiS cHrOmEbOoK",
            .expected_categories = {PickerCategory::kLocalFiles},
        },
        // Prefix match
        TestCase{
            .available_categories = {PickerCategory::kLocalFiles},
            .query = u"t",
            .expected_categories = {PickerCategory::kLocalFiles},
        },
        // Prefix match in second word
        TestCase{
            .available_categories = {PickerCategory::kLocalFiles},
            .query = u"ch",
            .expected_categories = {PickerCategory::kLocalFiles},
        },
        // Substring match
        TestCase{
            .available_categories = {PickerCategory::kLocalFiles},
            .query = u"rome",
            .expected_categories = {},
        },
        // Category unavailable
        TestCase{
            .available_categories = {PickerCategory::kLinks},
            .query = u"This Chromebook",
            .expected_categories = {},
        },
        // Not matched
        TestCase{
            .available_categories = {PickerCategory::kLocalFiles},
            .query = u"This Chromebookz",
            .expected_categories = {},
        }));

}  // namespace
}  // namespace ash
