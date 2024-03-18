// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/search/picker_date_search.h"

#include "base/check.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

using ::testing::Field;
using ::testing::Optional;
using ::testing::Property;
using ::testing::VariantWith;

base::Time TimeFromString(const char* time_string) {
  base::Time date;
  bool result = base::Time::FromString(time_string, &date);
  CHECK(result);
  return date;
}

TEST(PickerDateSearchTest, NoResult) {
  EXPECT_FALSE(PickerDateSearch(TimeFromString("23 Jan 2000 10:00 GMT"), u"abc")
                   .has_value());
}

TEST(PickerDateSearchTest, ShowsTodaysDate) {
  EXPECT_THAT(
      PickerDateSearch(TimeFromString("23 Jan 2000 10:00 GMT"), u"today"),
      Optional(Property(
          "data", &PickerSearchResult::data,
          VariantWith<PickerSearchResult::TextData>(
              Field("text", &PickerSearchResult::TextData::text, u"Jan 23")))));
}

TEST(PickerDateSearchTest, ShowsYesterdaysDate) {
  EXPECT_THAT(
      PickerDateSearch(TimeFromString("23 Jan 2000 10:00 GMT"), u"yesterday"),
      Optional(Property(
          "data", &PickerSearchResult::data,
          VariantWith<PickerSearchResult::TextData>(
              Field("text", &PickerSearchResult::TextData::text, u"Jan 22")))));
}

TEST(PickerDateSearchTest, ShowsTomorrowsDate) {
  EXPECT_THAT(
      PickerDateSearch(TimeFromString("23 Jan 2000 10:00 GMT"), u"tomorrow"),
      Optional(Property(
          "data", &PickerSearchResult::data,
          VariantWith<PickerSearchResult::TextData>(
              Field("text", &PickerSearchResult::TextData::text, u"Jan 24")))));
}

}  // namespace
}  // namespace ash
