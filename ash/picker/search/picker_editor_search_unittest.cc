// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/search/picker_editor_search.h"

#include <string>

#include "ash/public/cpp/picker/picker_search_result.h"
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

TEST(PickerEditorSearchTest, MatchesSentence) {
  EXPECT_THAT(
      PickerEditorSearch(PickerEditorResult::Mode::kWrite,
                         u"the quick brown fox"),
      Optional(VariantWith<PickerEditorResult>(AllOf(
          Field("mode", &PickerEditorResult::mode,
                PickerEditorResult::Mode::kWrite),
          Field("display_name", &PickerEditorResult::display_name, u""),
          Field("category", &PickerEditorResult::category, std::nullopt)))));
}

TEST(PickerEditorSearchTest, DoesNotMatchShortSentence) {
  EXPECT_EQ(
      PickerEditorSearch(PickerEditorResult::Mode::kWrite, u"the quick brown"),
      std::nullopt);
}

TEST(PickerEditorSearchTest, MatchesJapaneseSentence) {
  EXPECT_THAT(
      PickerEditorSearch(PickerEditorResult::Mode::kWrite,
                         u"素早い茶色のキツネ"),
      Optional(VariantWith<PickerEditorResult>(AllOf(
          Field("mode", &PickerEditorResult::mode,
                PickerEditorResult::Mode::kWrite),
          Field("display_name", &PickerEditorResult::display_name, u""),
          Field("category", &PickerEditorResult::category, std::nullopt)))));
}

TEST(PickerEditorSearchTest, DoesNotMatchShortJapaneseSentence) {
  EXPECT_EQ(PickerEditorSearch(PickerEditorResult::Mode::kWrite, u"素早い茶色"),
            std::nullopt);
}

}  // namespace
}  // namespace ash
