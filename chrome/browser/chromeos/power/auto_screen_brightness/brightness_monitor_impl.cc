// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/auto_screen_brightness/brightness_monitor_impl.h"

#include <cmath>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/ranges.h"
#include "base/task/post_task.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/utils.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"

namespace chromeos {
namespace power {
namespace auto_screen_brightness {

constexpr base::TimeDelta BrightnessMonitorImpl::kBrightnessSampleDelay;

BrightnessMonitorImpl::BrightnessMonitorImpl() = default;
BrightnessMonitorImpl::~BrightnessMonitorImpl() = default;

void BrightnessMonitorImpl::Init() {
  const int brightness_sample_delay_seconds = GetFieldTrialParamByFeatureAsInt(
      features::kAutoScreenBrightness, "brightness_sample_delay_seconds",
      kBrightnessSampleDelay.InSeconds());

  brightness_sample_delay_ =
      brightness_sample_delay_seconds < 0
          ? kBrightnessSampleDelay
          : base::TimeDelta::FromSeconds(brightness_sample_delay_seconds);

  power_manager_client_observer_.Add(PowerManagerClient::Get());
}

void BrightnessMonitorImpl::AddObserver(
    BrightnessMonitor::Observer* const observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
  if (brightness_monitor_status_ != Status::kInitializing) {
    observer->OnBrightnessMonitorInitialized(brightness_monitor_status_ ==
                                             Status::kSuccess);
  }
}

void BrightnessMonitorImpl::RemoveObserver(
    BrightnessMonitor::Observer* const observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

void BrightnessMonitorImpl::PowerManagerBecameAvailable(
    const bool service_is_ready) {
  if (!service_is_ready) {
    brightness_monitor_status_ = Status::kDisabled;
    OnInitializationComplete();
    return;
  }
  PowerManagerClient::Get()->GetScreenBrightnessPercent(
      base::BindOnce(&BrightnessMonitorImpl::OnReceiveInitialBrightnessPercent,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BrightnessMonitorImpl::ScreenBrightnessChanged(
    const power_manager::BacklightBrightnessChange& change) {
  if (brightness_monitor_status_ != Status::kSuccess) {
    // Either
    // (1). we're waiting for init brightness to come in from powerd, or
    // (2). we've failed to get init brightness from powerd.
    // In any case, we ignore this brightness change.
    return;
  }

  double brightness_percent_received = change.percent();
  if (brightness_percent_received < 0.0 ||
      brightness_percent_received > 100.0) {
    // Brightness should not be outside the range of [0,100]. If it's outside
    // this range after initialization completes successfully, we clip the value
    // instead of throwing it away.
    LogDataError(DataError::kBrightnessPercent);
    brightness_percent_received =
        base::ClampToRange(brightness_percent_received, 0.0, 100.0);
  }

  if (change.cause() ==
      power_manager::BacklightBrightnessChange_Cause_USER_REQUEST) {
    // This is the only brightness change caused by explicit user selection.
    NotifyUserBrightnessChangeRequested();
    user_brightness_percent_ = brightness_percent_received;
    StartBrightnessSampleTimer();
    return;
  }

  // We treat all the other causes as non-user-initiated.
  if (user_brightness_percent_) {
    // If we've received a user-selected brightness change, stop waiting and
    // report the latest |user_brightness_percent_|.
    brightness_sample_timer_.Stop();
    NotifyUserBrightnessChanged();
  }
  stable_brightness_percent_ = brightness_percent_received;
}

base::TimeDelta BrightnessMonitorImpl::GetBrightnessSampleDelayForTesting()
    const {
  return brightness_sample_delay_;
}

void BrightnessMonitorImpl::OnReceiveInitialBrightnessPercent(
    const base::Optional<double> brightness_percent) {
  DCHECK_EQ(brightness_monitor_status_, Status::kInitializing);

  if (brightness_percent && *brightness_percent >= 0.0 &&
      *brightness_percent <= 100.0) {
    // Brightness should not be outside the range of [0,100]. If it's outside
    // this range on initialization, then we disable the monitor.
    stable_brightness_percent_ = brightness_percent;
    brightness_monitor_status_ = Status::kSuccess;
  } else {
    brightness_monitor_status_ = Status::kDisabled;
  }

  OnInitializationComplete();
}

void BrightnessMonitorImpl::OnInitializationComplete() {
  DCHECK_NE(brightness_monitor_status_, Status::kInitializing);

  UMA_HISTOGRAM_ENUMERATION("AutoScreenBrightness.BrightnessMonitorStatus",
                            brightness_monitor_status_);

  const bool success = brightness_monitor_status_ == Status::kSuccess;
  for (auto& observer : observers_)
    observer.OnBrightnessMonitorInitialized(success);
}

void BrightnessMonitorImpl::StartBrightnessSampleTimer() {
  // It's ok if the timer is already running, we simply wait a bit longer.
  brightness_sample_timer_.Start(
      FROM_HERE, brightness_sample_delay_, this,
      &BrightnessMonitorImpl::NotifyUserBrightnessChanged);
}

void BrightnessMonitorImpl::NotifyUserBrightnessChanged() {
  if (!user_brightness_percent_) {
    NOTREACHED() << "User brightness adjustment missing on sample timeout";
    return;
  }

  for (auto& observer : observers_) {
    observer.OnUserBrightnessChanged(stable_brightness_percent_.value(),
                                     user_brightness_percent_.value());
  }

  stable_brightness_percent_ = user_brightness_percent_;
  user_brightness_percent_ = base::nullopt;
}

void BrightnessMonitorImpl::NotifyUserBrightnessChangeRequested() {
  for (auto& observer : observers_)
    observer.OnUserBrightnessChangeRequested();
}

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace chromeos
