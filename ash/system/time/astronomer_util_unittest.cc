// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/astronomer_util.h"

#include "base/test/icu_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace ash {

namespace {

struct TimeZoneInfo {
  const std::string timezone_id;
  double latitude;
  double longitude;
};

class AstronomerUtilTest : public testing::Test {
 public:
  // Verifies that the difference in sunrise and sunset times calculated by
  // GetSunriseSunset and GetSunriseSunsetICU are within a 5 min for a given
  // timezone, location and leap / non leap year.
  void VerifySunriseSunsetTimes(const TimeZoneInfo& tz_info) {
    base::test::ScopedRestoreDefaultTimezone tz(tz_info.timezone_id.c_str());

    auto is_leap_year = [](int year) {
      return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    };

    for (int year : {2024, 2025}) {
      SCOPED_TRACE(testing::Message()
                   << "tz=" << tz_info.timezone_id << ", year=" << year);
      base::Time start_date;
      base::Time::Exploded start_exploded = {year, 1, 0, 1, 12, 0, 0, 0};
      ASSERT_TRUE(base::Time::FromLocalExploded(start_exploded, &start_date));

      const int days_in_year = is_leap_year(year) ? 366 : 365;
      for (int i = 0; i < days_in_year; ++i) {
        base::Time date = start_date + base::Days(i);

        const auto result1 =
            GetSunriseSunset(date, tz_info.latitude, tz_info.longitude);
        const auto result2 =
            GetSunriseSunsetICU(date, tz_info.latitude, tz_info.longitude);

        ASSERT_TRUE(result1.has_value());
        ASSERT_TRUE(result2.has_value());

        EXPECT_NEAR(std::abs((result1->sunrise - result2->sunrise).InMinutes()),
                    0, 5)
            << "Sunrise mismatch on day " << i + 1 << " for timezone "
            << tz_info.timezone_id;
        EXPECT_NEAR(std::abs((result1->sunset - result2->sunset).InMinutes()),
                    0, 5)
            << "Sunset mismatch on day " << i + 1 << " for timezone "
            << tz_info.timezone_id;
      }
    }
  }
};

}  // namespace

TEST_F(AstronomerUtilTest, CompareImplementationsWithDaylightSaving) {
  VerifySunriseSunsetTimes(
      {"America/Los_Angeles", 34.0522, -118.2437});  // Los Angeles
}

TEST_F(AstronomerUtilTest, CompareImplementationsWithoutDaylightSaving) {
  VerifySunriseSunsetTimes({"Asia/Tokyo", 35.6895, 139.6917});  // Tokyo
}

}  // namespace ash
