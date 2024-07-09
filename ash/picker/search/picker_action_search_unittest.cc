// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/search/picker_action_search.h"

#include <string>
#include <vector>

#include "ash/public/cpp/picker/picker_category.h"
#include "ash/public/cpp/picker/picker_search_result.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Property;
using ::testing::VariantWith;

struct TestCase {
  std::vector<PickerCategory> available_categories;
  std::u16string query;
  std::vector<PickerSearchResult> expected_results;
};

class PickerActionSearchTest : public testing::TestWithParam<TestCase> {};

TEST_P(PickerActionSearchTest, MatchesExpectedCategories) {
  const auto& [available_categories, query, expected_results] = GetParam();
  EXPECT_THAT(
      PickerActionSearch({.available_categories = available_categories}, query),
      expected_results);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    PickerActionSearchTest,
    testing::Values(
        // Exact match
        TestCase{
            .available_categories = {PickerCategory::kLinks},
            .query = u"Browsing history",
            .expected_results = {PickerSearchResult::Category(
                PickerCategory::kLinks)},
        },
        // Case-insensitive match
        TestCase{
            .available_categories = {PickerCategory::kLinks},
            .query = u"bRoWsInG hIsToRy",
            .expected_results = {PickerSearchResult::Category(
                PickerCategory::kLinks)},
        },
        // Prefix match
        TestCase{
            .available_categories = {PickerCategory::kLinks},
            .query = u"b",
            .expected_results = {PickerSearchResult::Category(
                PickerCategory::kLinks)},
        },
        // Prefix match in second word
        TestCase{
            .available_categories = {PickerCategory::kLinks},
            .query = u"hi",
            .expected_results = {PickerSearchResult::Category(
                PickerCategory::kLinks)},
        },
        // Substring match
        TestCase{
            .available_categories = {PickerCategory::kLinks},
            .query = u"ist",
            .expected_results = {},
        },
        // Category unavailable
        TestCase{
            .available_categories = {PickerCategory::kLocalFiles},
            .query = u"Browsing history",
            .expected_results = {},
        },
        // Not matched
        TestCase{
            .available_categories = {PickerCategory::kLinks},
            .query = u"Browsing history1",
            .expected_results = {},
        }));

}  // namespace
}  // namespace ash
