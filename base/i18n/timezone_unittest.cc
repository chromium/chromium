// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/timezone.h"

#include <memory>

#include "base/i18n/rtl.h"
#include "base/i18n/tags.h"
#include "base/test/icu_test_util.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "third_party/icu/source/common/unicode/strenum.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace base::i18n {

TEST(TimeZoneTest, Default) {
  test::ScopedRestoreICUDefaultLocale restore_locale;
  SetICUDefaultLocale("en_US");
  test::ScopedRestoreDefaultTimezone la_time("America/Los_Angeles");

  TimeZone tz = TimeZone::Default();
  EXPECT_EQ(tz.GetID(), "America/Los_Angeles");
}

TEST(TimeZoneTest, FromID) {
  TimeZone tz = TimeZone::FromString("Europe/London");
  EXPECT_EQ(tz.GetID(), "Europe/London");

  TimeZone invalid = TimeZone::FromString("Invalid/Zone");
  EXPECT_EQ(invalid.GetID(), "Etc/Unknown");
}

TEST(TimeZoneTest, GMT) {
  TimeZone tz = TimeZone::GMT();
  EXPECT_EQ(tz.GetID(), "GMT");
}

TEST(TimeZoneTest, Unknown) {
  TimeZone tz = TimeZone::Unknown();
  EXPECT_EQ(tz.GetID(), "Etc/Unknown");
}

TEST(TimeZoneTest, CopyAndMove) {
  TimeZone tz1 = TimeZone::FromString("America/New_York");
  TimeZone tz2 = tz1;
  EXPECT_EQ(tz2.GetID(), "America/New_York");

  TimeZone tz3(std::move(tz1));
  EXPECT_EQ(tz3.GetID(), "America/New_York");
}

TEST(TimeZoneTest, GetDisplayName) {
  test::ScopedRestoreICUDefaultLocale restore_locale;
  SetICUDefaultLocale("en_US");

  TimeZone tz = TimeZone::FromString("America/Los_Angeles");
  // Standard time display name.
  EXPECT_EQ(tz.GetDisplayName(TimeZone::kLong), u"Pacific Standard Time");
  EXPECT_EQ(tz.GetDisplayName(TimeZone::kShort), u"PST");

  // Locale specific.
  EXPECT_EQ(tz.GetDisplayName(language_tags::FRENCH(), TimeZone::kLong),
            u"heure normale du Pacifique nord-am\u00e9ricain");
}

TEST(TimeZoneTest, Offsets) {
  TimeZone tz = TimeZone::FromString("America/Los_Angeles");
  // Los Angeles is GMT-8.
  EXPECT_EQ(tz.GetRawOffset(), base::Hours(-8));

  base::Time winter_time;
  ASSERT_TRUE(base::Time::FromString("2023-01-01 12:00:00 UTC", &winter_time));
  base::Time summer_time;
  ASSERT_TRUE(base::Time::FromString("2023-07-01 12:00:00 UTC", &summer_time));

  base::TimeDelta raw_offset;
  base::TimeDelta dst_offset;

  tz.GetOffset(winter_time, false, raw_offset, dst_offset);
  EXPECT_EQ(raw_offset, base::Hours(-8));
  EXPECT_EQ(dst_offset, base::Hours(0));

  tz.GetOffset(summer_time, false, raw_offset, dst_offset);
  EXPECT_EQ(raw_offset, base::Hours(-8));
  EXPECT_EQ(dst_offset, base::Hours(1));
}

TEST(TimeZoneTest, DaylightSavingTime) {
  TimeZone la = TimeZone::FromString("America/Los_Angeles");
  TimeZone phoenix =
      TimeZone::FromString("America/Phoenix");  // Arizona doesn't use DST.

  EXPECT_TRUE(la.UseDaylightTime());
  EXPECT_FALSE(phoenix.UseDaylightTime());

  base::Time winter_time;
  ASSERT_TRUE(base::Time::FromString("2023-01-01 12:00:00 UTC", &winter_time));
  base::Time summer_time;
  ASSERT_TRUE(base::Time::FromString("2023-07-01 12:00:00 UTC", &summer_time));

  EXPECT_FALSE(la.InDaylightTime(winter_time));
  EXPECT_TRUE(la.InDaylightTime(summer_time));

  EXPECT_FALSE(phoenix.InDaylightTime(winter_time));
  EXPECT_FALSE(phoenix.InDaylightTime(summer_time));
}

TEST(TimeZoneTest, Equality) {
  TimeZone tz1 = TimeZone::FromString("America/Los_Angeles");
  TimeZone tz2 = TimeZone::FromString("America/Los_Angeles");
  TimeZone tz3 = TimeZone::FromString("Europe/Berlin");

  EXPECT_EQ(tz1, tz2);
  EXPECT_NE(tz1, tz3);
}

TEST(TimeZoneTest, GetRegion) {
  EXPECT_EQ(TimeZone::FromString("America/Los_Angeles").GetRegion(), "US");
  EXPECT_EQ(TimeZone::FromString("Europe/Berlin").GetRegion(), "DE");
  EXPECT_EQ(TimeZone::FromString("GMT").GetRegion(), "");
  EXPECT_EQ(TimeZone::FromString("UTC").GetRegion(), "");
}

TEST(TimezoneTest, CountryCodeForTimezones) {
  std::unique_ptr<icu::StringEnumeration> timezones(
      icu::TimeZone::createEnumeration());

  UErrorCode status = U_ZERO_ERROR;
  while (const icu::UnicodeString* timezone = timezones->snext(status)) {
    icu::TimeZone::adoptDefault(icu::TimeZone::createTimeZone(*timezone));

    std::string country_code = CountryCodeForCurrentTimezone();
    if (!country_code.empty()) {
      EXPECT_EQ(2U, country_code.size()) << "country_code = " << country_code;
    }
  }

  icu::TimeZone::adoptDefault(nullptr);
}

}  // namespace base::i18n
