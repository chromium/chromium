// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/auto_screen_brightness/brightness_monitor_impl.h"

#include <cmath>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/task/post_task.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"

namespace chromeos {
namespace power {
namespace auto_screen_brightness {

constexpr base::TimeDelta BrightnessMonitorImpl::kBrightnessSampleDelay;

BrightnessMonitorImpl::BrightnessMonitorImpl(
    chromeos::PowerManagerClient* const power_manager_client)
    : BrightnessMonitorImpl(
          power_manager_client,
          base::CreateSequencedTaskRunnerWithTraits(
              {base::TaskPriority::BEST_EFFORT, base::MayBlock(),
               base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {}

BrightnessMonitorImpl::~BrightnessMonitorImpl() = default;

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

void BrightnessMonitorImpl::ScreenBrightnessChanged(
    const power_manager::BacklightBrightnessChange& change) {
  if (brightness_monitor_status_ != Status::kSuccess) {
    // Either
    // (1). we're waiting for init brightness to come in from powerd, or
    // (2). we've failed to get init brightness from powerd.
    // In any case, we ignore this brightness change.
    return;
  }

  if (change.cause() ==
      power_manager::BacklightBrightnessChange_Cause_USER_REQUEST) {
    // This is the only brightness change caused by explicit user selection.
    NotifyUserBrightnessChangeRequested();
    user_brightness_percent_ = base::Optional<double>(change.percent());
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
  stable_brightness_percent_ = base::Optional<double>(change.percent());
}

std::unique_ptr<BrightnessMonitorImpl> BrightnessMonitorImpl::CreateForTesting(
    chromeos::PowerManagerClient* const power_manager_client,
    const scoped_refptr<base::SequencedTaskRunner> task_runner) {
  return base::WrapUnique(
      new BrightnessMonitorImpl(power_manager_client, task_runner));
}

BrightnessMonitorImpl::BrightnessMonitorImpl(
    chromeos::PowerManagerClient* const power_manager_client,
    const scoped_refptr<base::SequencedTaskRunner> task_runner)
    : power_manager_client_observer_(this),
      power_manager_client_(power_manager_client),
      brightness_task_runner_(task_runner),
      weak_ptr_factory_(this) {
  DCHECK(power_manager_client);
  power_manager_client_observer_.Add(power_manager_client);
  brightness_sample_timer_.SetTaskRunner(brightness_task_runner_);

  power_manager_client_->WaitForServiceToBeAvailable(
      base::BindOnce(&BrightnessMonitorImpl::OnPowerManagerServiceAvailable,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BrightnessMonitorImpl::OnPowerManagerServiceAvailable(
    const bool service_is_ready) {
  if (!service_is_ready) {
    brightness_monitor_status_ = Status::kDisabled;
    OnInitializationComplete();
    return;
  }
  power_manager_client_->GetScreenBrightnessPercent(
      base::BindOnce(&BrightnessMonitorImpl::OnReceiveInitialBrightnessPercent,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BrightnessMonitorImpl::OnReceiveInitialBrightnessPercent(
    const base::Optional<double> brightness_percent) {
  DCHECK_EQ(brightness_monitor_status_, Status::kInitializing);

  if (brightness_percent) {
    stable_brightness_percent_ = brightness_percent;
    brightness_monitor_status_ = Status::kSuccess;
  } else {
    brightness_monitor_status_ = Status::kDisabled;
  }

  OnInitializationComplete();
}

void BrightnessMonitorImpl::OnInitializationComplete() {
  DCHECK_NE(brightness_monitor_status_, Status::kInitializing);
  const bool success = brightness_monitor_status_ == Status::kSuccess;
  for (auto& observer : observers_)
    observer.OnBrightnessMonitorInitialized(success);
}

void BrightnessMonitorImpl::StartBrightnessSampleTimer() {
  // It's ok if the timer is already running, we simply wait a bit longer.
  brightness_sample_timer_.Start(
      FROM_HERE, kBrightnessSampleDelay, this,
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
