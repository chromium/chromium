// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_BATTERY_ANDROID_BATTERY_METRICS_H_
#define CHROME_BROWSER_ANDROID_BATTERY_ANDROID_BATTERY_METRICS_H_

#include "base/android/application_status_listener.h"
#include "base/macros.h"
#include "base/power_monitor/power_observer.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"

// Records metrics around battery usage on Android.
class AndroidBatteryMetrics : public base::PowerObserver {
 public:
  AndroidBatteryMetrics();
  ~AndroidBatteryMetrics() override;

  // base::PowerObserver implementation:
  void OnPowerStateChange(bool on_battery_power) override;

 private:
  // Called by base::android::ApplicationStatusListener.
  void OnAppStateChanged(base::android::ApplicationState);

  void UpdateMetricsEnabled();
  void CaptureAndReportMetrics();
  void UpdateAndReportRadio();

  // Whether or not we've seen at least two consecutive capacity drops while
  // Chrome was the foreground app. Battery drain reported prior to this could
  // be caused by a different app.
  bool IsMeasuringDrainExclusively() const;

  // Battery drain is captured and reported periodically in this interval while
  // the device is on battery power and Chrome is the foreground activity.
  static constexpr base::TimeDelta kMetricsInterval =
      base::TimeDelta::FromSeconds(30);

  std::unique_ptr<base::android::ApplicationStatusListener> app_state_listener_;
  base::android::ApplicationState app_state_;
  bool on_battery_power_;
  int last_remaining_capacity_uah_ = 0;
  int64_t last_tx_bytes_ = -1;
  int64_t last_rx_bytes_ = -1;
  base::RepeatingTimer metrics_timer_;
  int skipped_timers_ = 0;

  // Number of consecutive charge drops seen while the app has been in the
  // foreground.
  int observed_capacity_drops_ = 0;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(AndroidBatteryMetrics);
};

#endif  // CHROME_BROWSER_ANDROID_BATTERY_ANDROID_BATTERY_METRICS_H_
