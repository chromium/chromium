// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/search/picker_editor_search.h"

#include <string>

#include "ash/picker/picker_search_result.h"
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

TEST(PickerEditorSearchTest, MatchesEnglishString) {
  EXPECT_THAT(
      PickerEditorSearch(PickerEditorResult::Mode::kWrite, u"cat"),
      Optional(VariantWith<PickerEditorResult>(AllOf(
          Field("mode", &PickerEditorResult::mode,
                PickerEditorResult::Mode::kWrite),
          Field("display_name", &PickerEditorResult::display_name, u""),
          Field("category", &PickerEditorResult::category, std::nullopt)))));
}

TEST(PickerEditorSearchTest, DoesNotMatchShortEnglishString) {
  EXPECT_EQ(PickerEditorSearch(PickerEditorResult::Mode::kWrite, u"ca"),
            std::nullopt);
}

TEST(PickerEditorSearchTest, MatchesJapaneseString) {
  EXPECT_THAT(
      PickerEditorSearch(PickerEditorResult::Mode::kWrite, u"キツネ"),
      Optional(VariantWith<PickerEditorResult>(AllOf(
          Field("mode", &PickerEditorResult::mode,
                PickerEditorResult::Mode::kWrite),
          Field("display_name", &PickerEditorResult::display_name, u""),
          Field("category", &PickerEditorResult::category, std::nullopt)))));
}

TEST(PickerEditorSearchTest, DoesNotMatchShortJapaneseString) {
  EXPECT_EQ(PickerEditorSearch(PickerEditorResult::Mode::kWrite, u"ねこ"),
            std::nullopt);
}

}  // namespace
}  // namespace ash
