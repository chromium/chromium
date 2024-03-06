// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/search/picker_math_search.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

using ::testing::Field;
using ::testing::Optional;
using ::testing::Property;
using ::testing::VariantWith;

TEST(PickerMathSearchTest, NoResult) {
  EXPECT_FALSE(PickerMathSearch(u"abc").has_value());
}

TEST(PickerMathSearchTest, OnePlusOneEqualsTwo) {
  EXPECT_THAT(PickerMathSearch(u"1 + 1"),
              Optional(Property(
                  "data", &PickerSearchResult::data,
                  VariantWith<PickerSearchResult::TextData>(Field(
                      "text", &PickerSearchResult::TextData::text, u"2")))));
}

}  // namespace
}  // namespace ash
