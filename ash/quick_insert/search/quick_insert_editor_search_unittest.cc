// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/search/quick_insert_editor_search.h"

#include <string>

#include "ash/quick_insert/quick_insert_search_result.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Optional;
using ::testing::Property;
using ::testing::VariantWith;

TEST(QuickInsertEditorSearchTest, MatchesEnglishString) {
  EXPECT_THAT(
      PickerEditorSearch(QuickInsertEditorResult::Mode::kWrite, u"cat"),
      Optional(VariantWith<QuickInsertEditorResult>(AllOf(
          Field("mode", &QuickInsertEditorResult::mode,
                QuickInsertEditorResult::Mode::kWrite),
          Field("display_name", &QuickInsertEditorResult::display_name, u""),
          Field("category", &QuickInsertEditorResult::category,
                std::nullopt)))));
}

TEST(QuickInsertEditorSearchTest, DoesNotMatchShortEnglishString) {
  EXPECT_EQ(PickerEditorSearch(QuickInsertEditorResult::Mode::kWrite, u"ca"),
            std::nullopt);
}

TEST(QuickInsertEditorSearchTest, MatchesJapaneseString) {
  EXPECT_THAT(
      PickerEditorSearch(QuickInsertEditorResult::Mode::kWrite, u"キツネ"),
      Optional(VariantWith<QuickInsertEditorResult>(AllOf(
          Field("mode", &QuickInsertEditorResult::mode,
                QuickInsertEditorResult::Mode::kWrite),
          Field("display_name", &QuickInsertEditorResult::display_name, u""),
          Field("category", &QuickInsertEditorResult::category,
                std::nullopt)))));
}

TEST(QuickInsertEditorSearchTest, DoesNotMatchShortJapaneseString) {
  EXPECT_EQ(PickerEditorSearch(QuickInsertEditorResult::Mode::kWrite, u"ねこ"),
            std::nullopt);
}

}  // namespace
}  // namespace ash
