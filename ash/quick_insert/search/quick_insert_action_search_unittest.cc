// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/search/quick_insert_action_search.h"

#include <string>
#include <vector>

#include "ash/quick_insert/quick_insert_category.h"
#include "ash/quick_insert/quick_insert_search_result.h"
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

using CaseTransformType = QuickInsertCaseTransformResult::Type;

struct TestCase {
  std::vector<QuickInsertCategory> available_categories;
  bool caps_lock_state_to_search = false;
  bool search_case_transforms = false;
  std::u16string query;
  std::vector<QuickInsertSearchResult> expected_results;
};

class QuickInsertActionSearchTest : public testing::TestWithParam<TestCase> {};

TEST_P(QuickInsertActionSearchTest, MatchesExpectedCategories) {
  const TestCase& test_case = GetParam();
  EXPECT_THAT(
      PickerActionSearch(test_case.available_categories,
                         test_case.caps_lock_state_to_search,
                         test_case.search_case_transforms, test_case.query),
      test_case.expected_results);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    QuickInsertActionSearchTest,
    testing::Values(
        // Exact match
        TestCase{
            .available_categories = {QuickInsertCategory::kLinks},
            .query = u"Browsing history",
            .expected_results = {QuickInsertCategoryResult(
                QuickInsertCategory::kLinks)},
        },
        // Case-insensitive match
        TestCase{
            .available_categories = {QuickInsertCategory::kLinks},
            .query = u"bRoWsInG hIsToRy",
            .expected_results = {QuickInsertCategoryResult(
                QuickInsertCategory::kLinks)},
        },
        // Prefix match
        TestCase{
            .available_categories = {QuickInsertCategory::kLinks},
            .query = u"b",
            .expected_results = {QuickInsertCategoryResult(
                QuickInsertCategory::kLinks)},
        },
        // Prefix match in second word
        TestCase{
            .available_categories = {QuickInsertCategory::kLinks},
            .query = u"hi",
            .expected_results = {QuickInsertCategoryResult(
                QuickInsertCategory::kLinks)},
        },
        // Substring match
        TestCase{
            .available_categories = {QuickInsertCategory::kLinks},
            .query = u"ist",
        },
        // Category unavailable
        TestCase{
            .available_categories = {QuickInsertCategory::kLocalFiles},
            .query = u"Browsing history",
        },
        // Not matched
        TestCase{
            .available_categories = {QuickInsertCategory::kLinks},
            .query = u"Browsing history1",
        },
        // Caps Lock Off
        TestCase{
            .query = u"caps",
            .expected_results = {QuickInsertCapsLockResult(
                /*enabled=*/false,
                QuickInsertCapsLockResult::Shortcut::kAltSearch)},
        },
        // Caps Lock On
        TestCase{
            .caps_lock_state_to_search = true,
            .query = u"caps",
            .expected_results = {QuickInsertCapsLockResult(
                /*enabled=*/true,
                QuickInsertCapsLockResult::Shortcut::kAltSearch)},
        },
        // Uppercase
        TestCase{
            .search_case_transforms = true,
            .query = u"upper",
            .expected_results = {QuickInsertCaseTransformResult(
                CaseTransformType::kUpperCase)},
        },
        // Lowercase
        TestCase{
            .search_case_transforms = true,
            .query = u"lower",
            .expected_results = {QuickInsertCaseTransformResult(
                CaseTransformType::kLowerCase)},
        },
        // Title case
        TestCase{
            .search_case_transforms = true,
            .query = u"title",
            .expected_results = {QuickInsertCaseTransformResult(
                CaseTransformType::kTitleCase)},
        },
        // No case
        TestCase{
            .query = u"upper",
        }));

}  // namespace
}  // namespace ash
