// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/device_activity/device_activity_sampler.h"

#include "components/reporting/metrics/sampler.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "ui/base/idle/idle.h"

namespace reporting {
namespace {

// Idle state threshold used to determine if the device was idle based on last
// event detection.
constexpr int kIdleStateThresholdSeconds = 5 /** minutes */ * 60;

UserStatusTelemetry::DeviceActivityState ConvertIdleStateToDeviceActivityState(
    ::ui::IdleState idle_state) {
  switch (idle_state) {
    case ::ui::IdleState::IDLE_STATE_IDLE:
      return UserStatusTelemetry::IDLE;
    case ::ui::IdleState::IDLE_STATE_LOCKED:
      return UserStatusTelemetry::LOCKED;
    case ::ui::IdleState::IDLE_STATE_ACTIVE:
      return UserStatusTelemetry::ACTIVE;
    case ::ui::IdleState::IDLE_STATE_UNKNOWN:
    default:
      return UserStatusTelemetry::DEVICE_ACTIVITY_STATE_UNKNOWN;
  }
}
}  // namespace

void DeviceActivitySampler::MaybeCollect(OptionalMetricCallback callback) {
  ::ui::IdleState idle_state =
      ::ui::CalculateIdleState(kIdleStateThresholdSeconds);
  MetricData metric_data;
  metric_data.mutable_telemetry_data()
      ->mutable_user_status_telemetry()
      ->set_device_activity_state(
          ConvertIdleStateToDeviceActivityState(idle_state));
  std::move(callback).Run(std::move(metric_data));
}

}  // namespace reporting
