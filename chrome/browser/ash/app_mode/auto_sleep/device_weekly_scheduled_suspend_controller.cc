// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/auto_sleep/device_weekly_scheduled_suspend_controller.h"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/app_mode/auto_sleep/weekly_interval_timer.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/policy/weekly_time/weekly_time_interval.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/prefs/pref_service.h"
#include "third_party/cros_system_api/dbus/power_manager/dbus-constants.h"

namespace ash {

using ::policy::WeeklyTimeInterval;

namespace {

// Extracts a vector of WeeklyTimeInterval objects from the policy config.
// Returns a vector containing nullptr for invalid dictionary entries.
std::vector<std::unique_ptr<WeeklyTimeInterval>>
GetPolicyConfigAsWeeklyTimeIntervals(const base::Value::List& policy_config) {
  std::vector<std::unique_ptr<WeeklyTimeInterval>> intervals;
  std::ranges::transform(policy_config, std::back_inserter(intervals),
                         [](const base::Value& value) {
                           return WeeklyTimeInterval::ExtractFromDict(
                               value.GetDict(),
                               /*timezone_offset=*/std::nullopt);
                         });
  return intervals;
}

bool IntervalsDoNotOverlap(
    const std::vector<std::unique_ptr<WeeklyTimeInterval>>& intervals) {
  for (size_t i = 0; i < intervals.size(); ++i) {
    CHECK(intervals[i]);
    for (size_t j = i + 1; j < intervals.size(); ++j) {
      CHECK(intervals[j]);
      if (WeeklyTimeInterval::IntervalsOverlap(*intervals[i], *intervals[j])) {
        LOG(ERROR) << "List entry " << i << " overlaps with list entry " << j;
        return false;
      }
    }
  }
  return true;
}

bool AllWeeklyTimeIntervalsAreValid(const base::Value::List& policy_config) {
  std::vector<std::unique_ptr<WeeklyTimeInterval>> intervals =
      GetPolicyConfigAsWeeklyTimeIntervals(policy_config);
  bool all_intervals_valid = true;

  for (size_t i = 0; i < intervals.size(); ++i) {
    if (!intervals[i]) {
      LOG(ERROR) << "Entry " << i << " in policy config is not valid";
      all_intervals_valid = false;
    }
  }

  return all_intervals_valid && IntervalsDoNotOverlap(intervals);
}

std::vector<std::unique_ptr<WeeklyIntervalTimer>> BuildIntervalTimersFromConfig(
    WeeklyIntervalTimer::Factory* interval_timer_factory,
    const base::Value::List& policy_config,
    const base::RepeatingCallback<void(base::TimeDelta)>& on_start_callback) {
  std::vector<std::unique_ptr<WeeklyTimeInterval>> intervals =
      GetPolicyConfigAsWeeklyTimeIntervals(policy_config);

  std::vector<std::unique_ptr<WeeklyIntervalTimer>> timers;
  std::ranges::transform(
      intervals, std::back_inserter(timers),
      [&](const std::unique_ptr<WeeklyTimeInterval>& interval) {
        CHECK_NE(interval, nullptr);
        return interval_timer_factory->Create(std::move(*interval),
                                              on_start_callback);
      });
  return timers;
}

}  // namespace

DeviceWeeklyScheduledSuspendController::DeviceWeeklyScheduledSuspendController(
    PrefService* pref_service)
    : weekly_interval_timer_factory_(
          std::make_unique<WeeklyIntervalTimer::Factory>()) {
  pref_change_registrar_.Init(pref_service);
  pref_change_registrar_.Add(
      prefs::kDeviceWeeklyScheduledSuspend,
      base::BindRepeating(&DeviceWeeklyScheduledSuspendController::
                              OnDeviceWeeklyScheduledSuspendUpdate,
                          weak_factory_.GetWeakPtr()));

  if (chromeos::PowerManagerClient::Get()) {
    // If the power manager service is already available then as soon as an
    // observer to the power manager is added, the `PowerManagerBecameAvailable`
    // observer method is called immediately.
    power_manager_observer_.Observe(chromeos::PowerManagerClient::Get());
  }
}

DeviceWeeklyScheduledSuspendController::
    ~DeviceWeeklyScheduledSuspendController() = default;

void DeviceWeeklyScheduledSuspendController::PowerManagerBecameAvailable(
    bool available) {
  if (!available) {
    LOG(ERROR) << "Power manager is not available, unable to perform scheduled "
                  "suspend";
    return;
  }
  power_manager_available_ = true;
  // Call the method to process the policy in case it was set already.
  OnDeviceWeeklyScheduledSuspendUpdate();
}

void DeviceWeeklyScheduledSuspendController::SuspendDone(
    base::TimeDelta sleep_duration) {
  // Full resumes are signaled by a SuspendDone call, and any full resume
  // triggered by third-parties will also cancel our full resume.
  resume_after_ = std::nullopt;
}

void DeviceWeeklyScheduledSuspendController::DarkSuspendImminent() {
  // Dark resumes are signaled by a DarkSuspendImminent call. The wake alarm
  // at the end of a sleep interval also triggers a dark resume, and when it
  // happens we want to transition to a full resume. Check `resume_after_`
  // to know when a full resume is due: the first dark resume after this
  // time will trigger a full resume; earlier events are ignored.
  if (!resume_after_ || resume_after_.value() > clock_->Now()) {
    return;
  }

  // Trigger a full resume.
  resume_after_ = std::nullopt;
  chromeos::PowerManagerClient::Get()->NotifyUserActivity(
      power_manager::USER_ACTIVITY_OTHER);
}

const WeeklyIntervalTimers&
DeviceWeeklyScheduledSuspendController::GetWeeklyIntervalTimersForTesting()
    const {
  return device_suspension_timers_;
}

void DeviceWeeklyScheduledSuspendController::
    SetWeeklyIntervalTimerFactoryForTesting(
        std::unique_ptr<WeeklyIntervalTimer::Factory> factory) {
  weekly_interval_timer_factory_ = std::move(factory);
}

void DeviceWeeklyScheduledSuspendController::SetClockForTesting(
    base::Clock* clock) {
  clock_ = clock;
}

void DeviceWeeklyScheduledSuspendController::
    OnDeviceWeeklyScheduledSuspendUpdate() {
  // Early return in case the policy is set before power manager is available.
  if (!power_manager_available_) {
    return;
  }
  const base::Value::List& policy_config =
      pref_change_registrar_.prefs()->GetList(
          prefs::kDeviceWeeklyScheduledSuspend);

  device_suspension_timers_.clear();

  if (!AllWeeklyTimeIntervalsAreValid(policy_config)) {
    return;
  }

  device_suspension_timers_ = BuildIntervalTimersFromConfig(
      weekly_interval_timer_factory_.get(), policy_config,
      base::BindRepeating(
          &DeviceWeeklyScheduledSuspendController::OnWeeklyIntervalStart,
          weak_factory_.GetWeakPtr()));
}

void DeviceWeeklyScheduledSuspendController::OnWeeklyIntervalStart(
    base::TimeDelta duration) {
  // Suspend the device for the specified duration. The device does NOT fully
  // resume automatically; rather, the powerd wake alarm triggers a dark resume
  // (signaled by a `DarkSuspendImminent` call) and we then need to trigger the
  // full resume ourselves. Note that we suspend to RAM, to ensure consistent
  // behavior across models. For more info about dark/full resumes, see:
  // https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/power_manager/docs/dark_resume.md
  chromeos::PowerManagerClient::Get()->RequestSuspend(
      /*wakeup_count=*/std::nullopt, duration.InSeconds(),
      power_manager::REQUEST_SUSPEND_TO_RAM);

  // We want any dark resume that happens within `tolerance` of the wake time
  // to trigger a full resume. Tolerance is an arbitrary duration that reduces
  // the chance of missing the end of a sleep interval due to random timing
  // issues, and is otherwise imperceptible if it causes an early full resume.
  constexpr auto tolerance = base::Seconds(2);
  resume_after_ = clock_->Now() + duration - tolerance;
}

}  // namespace ash
