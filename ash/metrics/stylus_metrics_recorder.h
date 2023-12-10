// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_METRICS_STYLUS_METRICS_RECORDER_H_
#define ASH_METRICS_STYLUS_METRICS_RECORDER_H_

#include <optional>

#include "ash/ash_export.h"
#include "ash/system/power/peripheral_battery_listener.h"
#include "chromeos/ash/components/feature_usage/feature_usage_metrics.h"

namespace ash {

// Combine delegate callbacks and FeatureUsageMetrics state for each metric.
class StylusSessionMetricsDelegate final
    : public feature_usage::FeatureUsageMetrics::Delegate {
 public:
  explicit StylusSessionMetricsDelegate(const std::string& feature_name);
  StylusSessionMetricsDelegate(const StylusSessionMetricsDelegate&) = delete;
  StylusSessionMetricsDelegate& operator=(const StylusSessionMetricsDelegate&) =
      delete;
  ~StylusSessionMetricsDelegate() final;

  // feature_usage::FeatureUsageMetrics::Delegate:
  bool IsEligible() const final;
  bool IsEnabled() const final;

  void SetState(bool now_capable, bool in_session);

 private:
  bool capable_ = false;
  bool active_ = false;
  feature_usage::FeatureUsageMetrics metrics_;
};

// A metrics recorder that records stylus related metrics.
class ASH_EXPORT StylusMetricsRecorder
    : public PeripheralBatteryListener::Observer {
 public:
  StylusMetricsRecorder();
  StylusMetricsRecorder(const StylusMetricsRecorder&) = delete;
  StylusMetricsRecorder& operator=(const StylusMetricsRecorder&) = delete;
  ~StylusMetricsRecorder() override;

  // ash::PeripheralBatteryListener::Observer:
  void OnAddingBattery(
      const PeripheralBatteryListener::BatteryInfo& battery) override;
  void OnRemovingBattery(
      const PeripheralBatteryListener::BatteryInfo& battery) override;
  void OnUpdatedBatteryLevel(
      const PeripheralBatteryListener::BatteryInfo& battery) override;

 private:
  void UpdateStylusState();

  std::optional<bool> stylus_on_charge_ = std::nullopt;

  // Indicate whether a stylus garage is known to be present on this device:
  // garage refers to a charger that holds the stylus within the body of the
  // device, and can detect whether the stylus is currently 'garaged'.
  bool stylus_garage_present_ = false;

  // Indicate whether a stylus dock is known to be present on this device:
  // dock refers to a charger that holds the stylus to the body of the device,
  // and can detect whether stylus is currently 'docked'.
  bool stylus_dock_present_ = false;

  StylusSessionMetricsDelegate
      stylus_detached_from_garage_session_metrics_delegate_{
          "StylusDetachedFromGarageSession"};
  StylusSessionMetricsDelegate
      stylus_detached_from_dock_session_metrics_delegate_{
          "StylusDetachedFromDockSession"};
  StylusSessionMetricsDelegate
      stylus_detached_from_garage_or_dock_session_metrics_delegate_{
          "StylusDetachedFromGarageOrDockSession"};
};

}  // namespace ash

#endif  // ASH_METRICS_STYLUS_METRICS_RECORDER_H_
