// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_utils.h"

#include "ash/components/settings/timezone_settings.h"
#include "ash/test/ash_test_base.h"
#include "base/time/time.h"

namespace ash {

using CalendarUtilsUnittest = AshTestBase;

// Tests the time difference calculation with different timezones and
// considering daylight savings.
TEST_F(CalendarUtilsUnittest, GetTimeDifference) {
  // Create a date: Aug,1st 2021.
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("1 Aug 2021 10:00 GMT", &date));

  // Sets the timezone to "America/Los_Angeles";
  ash::system::TimezoneSettings::GetInstance()->SetTimezoneFromID(u"PST");

  // Before daylight saving the time difference is 7 hours.
  EXPECT_EQ(-420, calendar_utils::GetTimeDifferenceInMinutes(date));

  // Create a date after daylight saving: Dec,1st 2021.
  base::Time date2;
  ASSERT_TRUE(base::Time::FromString("1 Dec 2021 10:00 GMT", &date2));

  // After daylight saving the time difference is 8 hours.
  EXPECT_EQ(-480, calendar_utils::GetTimeDifferenceInMinutes(date2));

  // Set the timezone to GMT.
  ash::system::TimezoneSettings::GetInstance()->SetTimezoneFromID(u"GMT");

  EXPECT_EQ(0, calendar_utils::GetTimeDifferenceInMinutes(date));
  EXPECT_EQ(0, calendar_utils::GetTimeDifferenceInMinutes(date2));
}

}  // namespace ash
