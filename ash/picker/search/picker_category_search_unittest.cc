// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/search/picker_category_search.h"

#include "ash/public/cpp/picker/picker_category.h"
#include "ash/public/cpp/picker/picker_search_result.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Property;
using ::testing::VariantWith;

TEST(PickerCategorySearchTest, UnavailableCategory) {
  EXPECT_THAT(PickerCategorySearch({}, u"Emoji"), IsEmpty());
}

TEST(PickerCategorySearchTest, ExactMatch) {
  EXPECT_THAT(PickerCategorySearch({{PickerCategory::kEmojis}}, u"Emojis"),
              ElementsAre(Property(
                  "data", &PickerSearchResult::data,
                  VariantWith<PickerSearchResult::CategoryData>(Field(
                      "category", &PickerSearchResult::CategoryData::category,
                      PickerCategory::kEmojis)))));
}

TEST(PickerCategorySearchTest, NoMatch) {
  EXPECT_THAT(PickerCategorySearch({{PickerCategory::kEmojis}}, u"NotEmoji"),
              IsEmpty());
}

}  // namespace
}  // namespace ash
