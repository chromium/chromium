// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/timezone.h"

#include <memory>

#include "base/features.h"
#include "base/i18n/language_tag.h"
#include "base/i18n/rtl.h"
#include "base/i18n/test/scoped_icu_locale.h"
#include "base/task/sequence_manager/thread_controller_power_monitor.h"
#include "base/test/icu_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "third_party/icu/source/common/unicode/strenum.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace base::i18n {

namespace {

class ScopedI18nOptimizationsFeature {
 public:
  explicit ScopedI18nOptimizationsFeature(bool enable) {
    features_.InitWithFeatureState(features::kI18nOptimizations, enable);
    features::Init();
    sequence_manager::internal::ThreadControllerPowerMonitor::ResetForTesting();
  }
  ~ScopedI18nOptimizationsFeature() {
    features_.Reset();
    features::Init();
    sequence_manager::internal::ThreadControllerPowerMonitor::ResetForTesting();
  }

 private:
  test::ScopedFeatureList features_;
};

}  // namespace

TEST(TimeZoneTest, Default) {
  ScopedDefaultIcuLocale restore_locale(GetKnownLanguageTag("en-US"));
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
  for (bool enable_feature : {false, true}) {
    ScopedI18nOptimizationsFeature scoped_feature(enable_feature);
    TimeZone tz = TimeZone::GMT();
    EXPECT_EQ(tz.GetID(), "GMT");
    EXPECT_EQ(tz, TimeZone::FromString("GMT"));
    EXPECT_EQ(tz.GetRawOffset(), base::TimeDelta());
    EXPECT_FALSE(tz.UseDaylightTime());
  }
}

TEST(TimeZoneTest, Unknown) {
  for (bool enable_feature : {false, true}) {
    ScopedI18nOptimizationsFeature scoped_feature(enable_feature);
    TimeZone tz = TimeZone::Unknown();
    EXPECT_EQ(tz.GetID(), "Etc/Unknown");
    EXPECT_EQ(tz, TimeZone::FromString("Etc/Unknown"));
  }
}

TEST(TimeZoneTest, CopyAndMove) {
  TimeZone tz1 = TimeZone::FromString("America/New_York");
  TimeZone tz2 = tz1;
  EXPECT_EQ(tz2.GetID(), "America/New_York");

  TimeZone tz3(std::move(tz1));
  EXPECT_EQ(tz3.GetID(), "America/New_York");
}

TEST(TimeZoneTest, GetDisplayName) {
  ScopedDefaultIcuLocale restore_locale(GetKnownLanguageTag("en-US"));

  TimeZone tz = TimeZone::FromString("America/Los_Angeles");
  // Standard time display name.
  EXPECT_EQ(tz.GetDisplayName({.style = TimeZone::kLong}),
            u"Pacific Standard Time");
  EXPECT_EQ(tz.GetDisplayName({.style = TimeZone::kShort}), u"PST");
  constexpr auto fr = GetKnownLanguageTag("fr");
  // Locale specific.
  EXPECT_EQ(tz.GetDisplayName(fr, {.style = TimeZone::kLong}),
            u"heure normale du Pacifique nord-am\u00e9ricain");

  base::Time winter_time;
  ASSERT_TRUE(base::Time::FromUTCString("2026-01-15 12:00:00", &winter_time));
  EXPECT_EQ(tz.GetDisplayName({.is_day_light = tz.InDaylightTime(winter_time),
                               .style = TimeZone::kLong}),
            u"Pacific Standard Time");

  base::Time summer_time;
  ASSERT_TRUE(base::Time::FromUTCString("2026-07-15 12:00:00", &summer_time));
  EXPECT_EQ(tz.GetDisplayName({.is_day_light = tz.InDaylightTime(summer_time),
                               .style = TimeZone::kLong}),
            u"Pacific Daylight Time");

  // Single-field designated initializer checks.
  EXPECT_EQ(tz.GetDisplayName({.is_day_light = false}),
            u"Pacific Standard Time");
  EXPECT_EQ(tz.GetDisplayName({.is_day_light = true}),
            u"Pacific Daylight Time");

  // Default parameters check.
  EXPECT_EQ(tz.GetDisplayName(), u"Pacific Standard Time");
  EXPECT_EQ(tz.GetDisplayName(fr),
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

TEST(TimeZoneTest, ExtremeTimes) {
  const TimeZone time_zones[] = {
      TimeZone::GMT(),
      TimeZone::Unknown(),
      TimeZone::FromString("America/Los_Angeles"),
      TimeZone::FromString("Asia/Tokyo"),
  };

  // Times whose `InMillisecondsFSinceUnixEpoch()` is non-finite (including
  // finite `Time` values in `(Time::Min(), Time::Min() + 369 years]` where
  // `time - Time::UnixEpoch()` clamps to `TimeDelta::Min()`).
  const base::Time non_finite_unix_epoch_times[] = {
      base::Time::Min(),
      base::Time::Min() + base::Microseconds(1),
      base::Time::Min() +
          base::Microseconds(base::Time::kMicrosecondsFromWindowsToUnixEpoch),
      base::Time::Max(),
  };

  // Boundary finite `Time` values whose `InMillisecondsFSinceUnixEpoch()` is
  // finite.
  const base::Time boundary_finite_unix_epoch_times[] = {
      base::Time::Min() +
          base::Microseconds(base::Time::kMicrosecondsFromWindowsToUnixEpoch +
                             1),
      base::Time::FromDeltaSinceWindowsEpoch(base::TimeDelta::FiniteMax()),
  };

  for (const TimeZone& tz : time_zones) {
    for (base::Time extreme_time : non_finite_unix_epoch_times) {
      for (bool is_local : {false, true}) {
        base::TimeDelta raw_offset = base::Hours(99);
        base::TimeDelta dst_offset = base::Hours(99);
        tz.GetOffset(extreme_time, is_local, raw_offset, dst_offset);
        // For non-finite Unix epoch timestamps, `GetOffset()` falls back to
        // `GetRawOffset()` and zero DST without querying ICU.
        EXPECT_EQ(raw_offset, tz.GetRawOffset());
        EXPECT_EQ(dst_offset, base::TimeDelta());
      }
      EXPECT_FALSE(tz.InDaylightTime(extreme_time));
    }

    for (base::Time boundary_time : boundary_finite_unix_epoch_times) {
      for (bool is_local : {false, true}) {
        base::TimeDelta raw_offset = base::Hours(99);
        base::TimeDelta dst_offset = base::Hours(99);
        tz.GetOffset(boundary_time, is_local, raw_offset, dst_offset);
        // ICU returns historical Local Mean Time or future DST at these bounds,
        // so just verify that `GetOffset()` overwrote the `Hours(99)` sentinel.
        EXPECT_NE(raw_offset, base::Hours(99));
        EXPECT_NE(dst_offset, base::Hours(99));
      }
      // Expect no crash (`DCHECK(U_SUCCESS(status))`).
      tz.InDaylightTime(boundary_time);
    }
  }
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
