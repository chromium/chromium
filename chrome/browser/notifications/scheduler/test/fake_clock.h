// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_TEST_FAKE_CLOCK_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_TEST_FAKE_CLOCK_H_

#include "base/time/clock.h"
#include "base/time/time.h"

namespace notifications {
namespace test {

// Clock to mock Clock::Now() to get a fixed time in the test.
class FakeClock : public base::Clock {
 public:
  // Helper function to convert a string to a time object.
  static base::Time GetTime(const char* time_str);

  FakeClock();
  FakeClock(const FakeClock&) = delete;
  FakeClock& operator=(const FakeClock&) = delete;
  ~FakeClock() override;

  // Helper functions to set the current timestamp.
  void SetNow(const char* time_str);
  void SetNow(const base::Time& time);

  // Resets to use base::Time::Now().
  void Reset();

  // base::Clock implementation.
  base::Time Now() const override;

 private:
  // Mocked time.
  base::Time time_;

  // Whether Now() should return mocked time.
  bool time_mocked_;
};

}  // namespace test
}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_TEST_FAKE_CLOCK_H_
