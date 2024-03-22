// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/auto_sleep/device_weekly_scheduled_suspend_controller.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "base/logging.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/app_mode/auto_sleep/repeating_time_interval_task_executor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/policy/weekly_time/weekly_time_interval.h"
#include "components/prefs/pref_service.h"

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

std::vector<std::unique_ptr<RepeatingTimeIntervalTaskExecutor>>
BuildIntervalExecutorsFromConfig(
    RepeatingTimeIntervalTaskExecutor::Factory* task_executor_factory,
    const base::Value::List& policy_config,
    const base::RepeatingCallback<void(base::TimeDelta)>& on_start_callback,
    const base::RepeatingClosure& on_end_callback) {
  std::vector<std::unique_ptr<WeeklyTimeInterval>> intervals =
      GetPolicyConfigAsWeeklyTimeIntervals(policy_config);

  std::vector<std::unique_ptr<RepeatingTimeIntervalTaskExecutor>> executors;
  std::ranges::transform(
      intervals, std::back_inserter(executors),
      [&](const std::unique_ptr<WeeklyTimeInterval>& interval) {
        CHECK(interval != nullptr);
        return task_executor_factory->Create(
            std::move(*interval), on_start_callback, on_end_callback);
      });
  return executors;
}

}  // namespace

DeviceWeeklyScheduledSuspendController::DeviceWeeklyScheduledSuspendController(
    PrefService* pref_service)
    : task_executor_factory_(
          std::make_unique<RepeatingTimeIntervalTaskExecutor::Factory>()) {
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

const RepeatingTimeIntervalTaskExecutors&
DeviceWeeklyScheduledSuspendController::GetIntervalExecutorsForTesting() const {
  return interval_executors_;
}

void DeviceWeeklyScheduledSuspendController::SetTaskExecutorFactoryForTesting(
    std::unique_ptr<RepeatingTimeIntervalTaskExecutor::Factory> factory) {
  task_executor_factory_ = std::move(factory);
}

void DeviceWeeklyScheduledSuspendController::
    OnDeviceWeeklyScheduledSuspendUpdate() {
  // Early return in case the policy gets set before power manager is available.
  if (!power_manager_available_) {
    return;
  }
  const base::Value::List& policy_config =
      g_browser_process->local_state()->GetList(
          prefs::kDeviceWeeklyScheduledSuspend);

  interval_executors_.clear();

  if (!AllWeeklyTimeIntervalsAreValid(policy_config)) {
    return;
  }

  interval_executors_ = BuildIntervalExecutorsFromConfig(
      task_executor_factory_.get(), policy_config,
      base::BindRepeating(
          &DeviceWeeklyScheduledSuspendController::OnTaskExecutorIntervalStart,
          weak_factory_.GetWeakPtr()),
      base::BindRepeating(
          &DeviceWeeklyScheduledSuspendController::OnTaskExecutorIntervalEnd,
          weak_factory_.GetWeakPtr()));

  for (const auto& executor : interval_executors_) {
    executor->ScheduleTimer();
  }
}

void DeviceWeeklyScheduledSuspendController::OnTaskExecutorIntervalStart(
    base::TimeDelta duration) {
  // TODO(b/330664145): Use the `duration` when calling `RequestSuspend`.
  chromeos::PowerManagerClient::Get()->RequestSuspend();
}

void DeviceWeeklyScheduledSuspendController::OnTaskExecutorIntervalEnd() {
  // No device wake-up needed. The `RepeatingTimeIntervalTaskExecutor`'s
  // underlying `NativeTimer` handles device wake-up at interval end.
}

}  // namespace ash
