// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/icubridge/calendar.h"

#include "base/i18n/icu_util.h"
#include "base/i18n/icubridge/icu_bridge.h"
#include "base/i18n/language_tag.h"
#include "base/i18n/rtl.h"
#include "base/i18n/test/scoped_icu_locale.h"
#include "base/test/icu_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::i18n {

class CalendarTest : public testing::Test {
 public:
  void SetUp() override { InitializeICU(); }
};

TEST_F(CalendarTest, GetWeekInformationDefaultLocale) {
  const IcuBridge::Calendar& calendar = IcuBridge::GetInstance().calendar();

  // Set default locale to US
  {
    ScopedDefaultIcuLocale scoped_locale(GetKnownLanguageTag("en-US"));
    auto info_us = calendar.GetWeekInformation();
    EXPECT_EQ(IcuBridge::Calendar::Weekday::kSunday, info_us.first_weekday);
    EXPECT_EQ((std::set<IcuBridge::Calendar::Weekday>{
                  IcuBridge::Calendar::Weekday::kSaturday,
                  IcuBridge::Calendar::Weekday::kSunday}),
              info_us.weekend);
  }

  // Set default locale to GB (UK)
  {
    ScopedDefaultIcuLocale scoped_locale(GetKnownLanguageTag("en-GB"));
    auto info_gb = calendar.GetWeekInformation();
    EXPECT_EQ(IcuBridge::Calendar::Weekday::kMonday, info_gb.first_weekday);
    EXPECT_EQ((std::set<IcuBridge::Calendar::Weekday>{
                  IcuBridge::Calendar::Weekday::kSaturday,
                  IcuBridge::Calendar::Weekday::kSunday}),
              info_gb.weekend);
  }

  // Set default locale to ar-SA (Saudi Arabia)
  {
    ScopedDefaultIcuLocale scoped_locale(GetKnownLanguageTag("ar-SA"));
    auto info_sa = calendar.GetWeekInformation();
    EXPECT_EQ(IcuBridge::Calendar::Weekday::kSunday, info_sa.first_weekday);
    EXPECT_EQ((std::set<IcuBridge::Calendar::Weekday>{
                  IcuBridge::Calendar::Weekday::kFriday,
                  IcuBridge::Calendar::Weekday::kSaturday}),
              info_sa.weekend);
  }
}

}  // namespace base::i18n
