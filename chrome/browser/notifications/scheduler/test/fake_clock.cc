// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/test/fake_clock.h"

namespace notifications {
namespace test {

// static
base::Time FakeClock::GetTime(const char* time_str) {
  base::Time time;
  bool success = base::Time::FromString(time_str, &time);
  DCHECK(success);
  return time;
}

FakeClock::FakeClock() : time_mocked_(false) {}

FakeClock::~FakeClock() = default;

void FakeClock::SetNow(const char* time_str) {
  base::Time now = GetTime(time_str);
  SetNow(now);
}

void FakeClock::SetNow(const base::Time& time) {
  time_ = time;
  time_mocked_ = true;
}

void FakeClock::Reset() {
  time_mocked_ = false;
}

base::Time FakeClock::Now() const {
  return time_mocked_ ? time_ : base::Time::Now();
}

}  // namespace test
}  // namespace notifications
