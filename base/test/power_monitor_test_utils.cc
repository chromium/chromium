// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/power_monitor_test_utils.h"

namespace base::test {

TestSamplingEventSource::TestSamplingEventSource() = default;
TestSamplingEventSource::~TestSamplingEventSource() = default;

bool TestSamplingEventSource::Start(SamplingEventCallback callback) {
  sampling_event_callback_ = std::move(callback);
  return true;
}

void TestSamplingEventSource::SimulateEvent() {
  sampling_event_callback_.Run();
}

TestBatteryLevelProvider::TestBatteryLevelProvider() = default;

void TestBatteryLevelProvider::GetBatteryState(
    base::OnceCallback<
        void(const std::optional<base::BatteryLevelProvider::BatteryState>&)>
        callback) {
  std::move(callback).Run(battery_state_);
}

void TestBatteryLevelProvider::SetBatteryState(
    std::optional<base::BatteryLevelProvider::BatteryState> battery_state) {
  battery_state_ = battery_state;
}

// static
base::BatteryLevelProvider::BatteryState
TestBatteryLevelProvider::CreateBatteryState(int battery_count,
                                             bool is_external_power_connected,
                                             int charge_percent) {
  return {
      .battery_count = battery_count,
      .is_external_power_connected = is_external_power_connected,
      .current_capacity = charge_percent,
      .full_charged_capacity = 100,
      .charge_unit = base::BatteryLevelProvider::BatteryLevelUnit::kRelative,
      .capture_time = base::TimeTicks::Now(),
  };
}

}  // namespace base::test
