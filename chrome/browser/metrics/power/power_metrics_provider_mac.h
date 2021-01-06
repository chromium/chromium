// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_POWER_POWER_METRICS_PROVIDER_MAC_H_
#define CHROME_BROWSER_METRICS_POWER_POWER_METRICS_PROVIDER_MAC_H_

#include "components/metrics/metrics_provider.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/metrics/power/battery_level_provider.h"

// Records battery power drain in a histogram. To use, repeatedly call
// RecordBatteryDischarge() at regular intervals. The implementation tries to
// correct for cases where the function was called too late or too early to
// avoid recording faulty measurements.
// Example use:
//
//   class UserClass {
//    public:
//     static constexpr base::TimeDelta kRecordingInterval =
//     base::TimeDelta::FromMinutes(1);
//
//     UserClass(){
//       power_drain_recorder_ =
//       std::make_unique<PowerDrainRecorder>(kRecordingInterval);
//     }
//
//     void StartRecording() {
//       timer_.Start(FROM_HERE, kRecordingInterval,
//                    power_drain_recorder_.get(),
//                    &PowerDrainRecorder::RecordBatteryDischarge);
//     }
//
//    private:
//     std::unique_ptr<PowerDrainRecorder> power_drain_recorder_;
//     base::RepeatingTimer timer_;
//   };
//
class PowerDrainRecorder {
 public:
  // |recording_interval| is the time that is supposed to elapse between calls
  // to RecordBatteryDischarge().
  explicit PowerDrainRecorder(base::TimeDelta recording_interval);
  ~PowerDrainRecorder();

  PowerDrainRecorder(const PowerDrainRecorder& other) = delete;
  PowerDrainRecorder& operator=(const PowerDrainRecorder& other) = delete;

  // Calling this function repeatedly will store the battery discharge that
  // happened between calls in a histogram.
  void RecordBatteryDischarge();

  // Replace the function used to get BatteryState values. Use only for testing
  // to not depend on actual system information.
  void SetBatteryLevelProviderForTesting(
      std::unique_ptr<BatteryLevelProvider> provider);

 private:
  // Used to get the current battery state.
  std::unique_ptr<BatteryLevelProvider> battery_level_provider_ =
      BatteryLevelProvider::Create();

  // Latest battery state provided by |battery_level_provider_|.
  base::Optional<BatteryLevelProvider::BatteryState> battery_state_;

  // Time that should elapse between calls to RecordBatteryDischarge.
  const base::TimeDelta recording_interval_;

  friend class PowerMetricsProviderTest;
  FRIEND_TEST_ALL_PREFIXES(PowerMetricsProviderTest, BatteryDischargeOnPower);
  FRIEND_TEST_ALL_PREFIXES(PowerMetricsProviderTest, BatteryDischargeOnBattery);
  FRIEND_TEST_ALL_PREFIXES(PowerMetricsProviderTest,
                           BatteryDischargeCapacityGrew);
  FRIEND_TEST_ALL_PREFIXES(PowerMetricsProviderTest,
                           BatteryDischargeCaptureIsTooEarly);
  FRIEND_TEST_ALL_PREFIXES(PowerMetricsProviderTest,
                           BatteryDischargeCaptureIsEarly);
  FRIEND_TEST_ALL_PREFIXES(PowerMetricsProviderTest,
                           BatteryDischargeCaptureIsTooLate);
  FRIEND_TEST_ALL_PREFIXES(PowerMetricsProviderTest,
                           BatteryDischargeCaptureIsLate);
};

class PowerMetricsProvider : public metrics::MetricsProvider {
 public:
  PowerMetricsProvider();
  ~PowerMetricsProvider() override;

  PowerMetricsProvider(const PowerMetricsProvider& other) = delete;
  PowerMetricsProvider& operator=(const PowerMetricsProvider& other) = delete;

  // metrics::MetricsProvider overrides
  void OnRecordingEnabled() override;
  void OnRecordingDisabled() override;

 private:
  class Impl;
  scoped_refptr<Impl> impl_;
};

#endif  // CHROME_BROWSER_METRICS_POWER_POWER_METRICS_PROVIDER_MAC_H_
