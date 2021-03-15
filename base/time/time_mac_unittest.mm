// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time/time.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

base::Time NoonOnDate(int year, int month, int day) {
  base::Time::Exploded exploded;
  exploded.year = year;
  exploded.month = month;
  exploded.day_of_week = 0;  // Not correct, but FromExploded permits it
  exploded.day_of_month = day;
  exploded.hour = 12;
  exploded.minute = 0;
  exploded.second = 0;
  exploded.millisecond = 0;
  base::Time imploded;
  CHECK(base::Time::FromUTCExploded(exploded, &imploded));
  return imploded;
}

void CheckRoundTrip(int y, int m, int d) {
  base::Time original = NoonOnDate(y, m, d);
  base::Time roundtrip = Time::FromNSDate(original.ToNSDate());
  EXPECT_EQ(original, roundtrip);
}

TEST(TimeMacTest, RoundTripNSDate) {
  CheckRoundTrip(1911, 12, 14);
  CheckRoundTrip(1924, 9, 28);
  CheckRoundTrip(1926, 5, 12);
  CheckRoundTrip(1969, 7, 24);
}

}  // namespace
}  // namespace base
