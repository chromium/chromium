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
      PickerEditorSearch(PickerSearchResult::EditorData::Mode::kWrite,
                         u"the quick brown fox"),
      Optional(Property(
          "data", &PickerSearchResult::data,
          VariantWith<PickerSearchResult::EditorData>(
              AllOf(Field("mode", &PickerSearchResult::EditorData::mode,
                          PickerSearchResult::EditorData::Mode::kWrite),
                    Field("display_name",
                          &PickerSearchResult::EditorData::display_name, u""),
                    Field("category", &PickerSearchResult::EditorData::category,
                          std::nullopt))))));
}

TEST(PickerEditorSearchTest, DoesNotMatchShortSentence) {
  EXPECT_EQ(PickerEditorSearch(PickerSearchResult::EditorData::Mode::kWrite,
                               u"the quick brown"),
            std::nullopt);
}

}  // namespace
}  // namespace ash
