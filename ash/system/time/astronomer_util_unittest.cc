// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/astronomer_util.h"

#include <stdlib.h>
#include <time.h>

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
    // TODO(crbug.com/446129868): We're not using
    // base::test::ScopedRestoreDefaultTimezone because of the bug in TODO.
    // Switch to base::test::ScopedRestoreDefaultTimezone once the issue is
    // resolved.
    setenv("TZ", tz_info.timezone_id.c_str(), 1);
    tzset();

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

        const auto result =
            GetSunriseSunset(date, tz_info.latitude, tz_info.longitude);
        const auto result_icu =
            GetSunriseSunsetICU(date, tz_info.latitude, tz_info.longitude);

        ASSERT_TRUE(result.has_value());
        ASSERT_TRUE(result_icu.has_value());

        EXPECT_NEAR(
            std::abs((result->sunrise - result_icu->sunrise).InMinutes()), 0, 7)
            << "Sunrise mismatch on day " << i + 1 << " for timezone "
            << tz_info.timezone_id;
        EXPECT_NEAR(std::abs((result->sunset - result_icu->sunset).InMinutes()),
                    0, 7)
            << "Sunset mismatch on day " << i + 1 << " for timezone "
            << tz_info.timezone_id;
      }
    }
    unsetenv("TZ");
    tzset();
  }
};

}  // namespace

TEST_F(AstronomerUtilTest, CompareImplementationsNYC) {
  VerifySunriseSunsetTimes(
      {"America/New_York", 40.7128, -74.0060});  // New  York
}

TEST_F(AstronomerUtilTest, CompareImplementationsLA) {
  VerifySunriseSunsetTimes(
      {"America/Los_Angeles", 34.0522, -118.2437});  // Los Angeles
}

TEST_F(AstronomerUtilTest, CompareImplementationsRioGallegos) {
  VerifySunriseSunsetTimes(
      {"America/Argentina/Rio_Gallegos", 51.6230, -69.216});  // South America
}

TEST_F(AstronomerUtilTest, CompareImplementationsNewZealandAuckland) {
  VerifySunriseSunsetTimes(
      {"Pacific/Auckland", -33.8727, 151.2057});  // New Zealand
}

TEST_F(AstronomerUtilTest, CompareImplementationsTokyoJapan) {
  VerifySunriseSunsetTimes({"Asia/Tokyo", 35.6895, 139.6917});  // Tokyo
}

TEST_F(AstronomerUtilTest, CompareImplementationsIndiaKolkata) {
  VerifySunriseSunsetTimes({"Asia/Kolkata", 22.5744, 88.3629});  // India
}

TEST_F(AstronomerUtilTest, CompareImplementationsEgypt) {
  VerifySunriseSunsetTimes({"Egypt", 23.5, 35.88});  // Egypt
}

TEST_F(AstronomerUtilTest, CompareImplementationsIcelandGMT) {
  VerifySunriseSunsetTimes({"GMT", 64.1470, -21.9408});  // Iceland/GMT
}

TEST_F(AstronomerUtilTest, CompareImplementationsSouthAfricaJohannesburg) {
  // South Africa/Johannesburg.
  VerifySunriseSunsetTimes({"Africa/Johannesburg", -26.2056, 28.0337});
}

}  // namespace ash
