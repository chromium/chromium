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

using CaseTransformType = PickerSearchResult::CaseTransformData::Type;

struct TestCase {
  PickerActionSearchOptions options;
  std::u16string query;
  std::vector<PickerSearchResult> expected_results;
};

class PickerActionSearchTest : public testing::TestWithParam<TestCase> {};

TEST_P(PickerActionSearchTest, MatchesExpectedCategories) {
  const auto& [options, query, expected_results] = GetParam();
  EXPECT_THAT(PickerActionSearch(options, query), expected_results);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    PickerActionSearchTest,
    testing::Values(
        // Exact match
        TestCase{
            .options =
                {
                    .available_categories = {{PickerCategory::kLinks}},
                },
            .query = u"Browsing history",
            .expected_results = {PickerSearchResult::Category(
                PickerCategory::kLinks)},
        },
        // Case-insensitive match
        TestCase{
            .options =
                {
                    .available_categories = {{PickerCategory::kLinks}},
                },
            .query = u"bRoWsInG hIsToRy",
            .expected_results = {PickerSearchResult::Category(
                PickerCategory::kLinks)},
        },
        // Prefix match
        TestCase{
            .options =
                {
                    .available_categories = {{PickerCategory::kLinks}},
                },
            .query = u"b",
            .expected_results = {PickerSearchResult::Category(
                PickerCategory::kLinks)},
        },
        // Prefix match in second word
        TestCase{
            .options =
                {
                    .available_categories = {{PickerCategory::kLinks}},
                },
            .query = u"hi",
            .expected_results = {PickerSearchResult::Category(
                PickerCategory::kLinks)},
        },
        // Substring match
        TestCase{
            .options =
                {
                    .available_categories = {{PickerCategory::kLinks}},
                },
            .query = u"ist",
            .expected_results = {},
        },
        // Category unavailable
        TestCase{
            .options =
                {
                    .available_categories = {{PickerCategory::kLocalFiles}},
                },
            .query = u"Browsing history",
            .expected_results = {},
        },
        // Not matched
        TestCase{
            .options =
                {
                    .available_categories = {{PickerCategory::kLinks}},
                },
            .query = u"Browsing history1",
            .expected_results = {},
        },
        // Caps Lock Off
        TestCase{
            .options =
                {
                    .caps_lock_state_to_search = false,
                },
            .query = u"caps",
            .expected_results = {PickerSearchResult::CapsLock(
                /*enabled=*/false,
                PickerSearchResult::CapsLockData::Shortcut::kAltSearch)},
        },
        // Caps Lock On
        TestCase{
            .options =
                {
                    .caps_lock_state_to_search = true,
                },
            .query = u"caps",
            .expected_results = {PickerSearchResult::CapsLock(
                /*enabled=*/true,
                PickerSearchResult::CapsLockData::Shortcut::kAltSearch)},
        },
        // Uppercase
        TestCase{
            .options =
                {
                    .search_case_transforms = true,
                },
            .query = u"upper",
            .expected_results = {PickerSearchResult::CaseTransform(
                CaseTransformType::kUpperCase)},
        },
        // Lowercase
        TestCase{
            .options =
                {
                    .search_case_transforms = true,
                },
            .query = u"lower",
            .expected_results = {PickerSearchResult::CaseTransform(
                CaseTransformType::kLowerCase)},
        },
        // Title case
        TestCase{
            .options =
                {
                    .search_case_transforms = true,
                },
            .query = u"title",
            .expected_results = {PickerSearchResult::CaseTransform(
                CaseTransformType::kTitleCase)},
        },
        // No case
        TestCase{
            .options =
                {
                    .search_case_transforms = false,
                },
            .query = u"upper",
            .expected_results = {},
        }));

}  // namespace
}  // namespace ash
