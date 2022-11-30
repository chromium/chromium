// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_POWER_MONITOR_TEST_UTILS_H_
#define BASE_TEST_POWER_MONITOR_TEST_UTILS_H_

#include "base/functional/callback.h"
#include "base/power_monitor/battery_level_provider.h"
#include "base/power_monitor/sampling_event_source.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base::test {

class TestSamplingEventSource : public SamplingEventSource {
 public:
  TestSamplingEventSource();
  ~TestSamplingEventSource() override;

  bool Start(SamplingEventCallback callback) override;

  void SimulateEvent();

 private:
  SamplingEventCallback sampling_event_callback_;
};

class TestBatteryLevelProvider : public base::BatteryLevelProvider {
 public:
  TestBatteryLevelProvider();
  void GetBatteryState(
      base::OnceCallback<
          void(const absl::optional<base::BatteryLevelProvider::BatteryState>&)>
          callback) override;

  void SetBatteryState(
      absl::optional<base::BatteryLevelProvider::BatteryState> battery_state);

  static base::BatteryLevelProvider::BatteryState CreateBatteryState(
      int battery_count = 1,
      bool is_external_power_connected = false,
      int charge_percent = 100);

 private:
  absl::optional<base::BatteryLevelProvider::BatteryState> battery_state_;
};

}  // namespace base::test

#endif  // BASE_TEST_POWER_MONITOR_TEST_UTILS_H_
