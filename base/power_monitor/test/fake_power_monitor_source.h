// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_POWER_MONITOR_TEST_FAKE_POWER_MONITOR_SOURCE_H_
#define BASE_POWER_MONITOR_TEST_FAKE_POWER_MONITOR_SOURCE_H_

namespace base {
namespace test {

// Use FakePowerMonitorSource via ScopedFakePowerMonitorSource wrapper when you
// need to simulate power events (suspend and resume).
class FakePowerMonitorSource;

class ScopedFakePowerMonitorSource {
 public:
  ScopedFakePowerMonitorSource();
  ~ScopedFakePowerMonitorSource();

  ScopedFakePowerMonitorSource(const ScopedFakePowerMonitorSource&) = delete;
  ScopedFakePowerMonitorSource& operator=(const ScopedFakePowerMonitorSource&) =
      delete;

  // Use this method to send a power resume event.
  void Resume();

  // Use this method to send a power suspend event.
  void Suspend();

 private:
  // Owned by PowerMonitor.
  FakePowerMonitorSource* fake_power_monitor_source_ = nullptr;
};

}  // namespace test
}  // namespace base

#endif  // BASE_POWER_MONITOR_TEST_FAKE_POWER_MONITOR_SOURCE_H_
