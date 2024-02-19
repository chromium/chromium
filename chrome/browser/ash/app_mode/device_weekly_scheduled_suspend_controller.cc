// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/device_weekly_scheduled_suspend_controller.h"

#include <memory>
#include <vector>

#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/values.h"
#include "chrome/browser/ash/app_mode/repeating_time_interval_task_executor.h"
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
  base::ranges::transform(policy_config, std::back_inserter(intervals),
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
    const base::Value::List& policy_config,
    const base::RepeatingClosure& on_start_callback,
    const base::RepeatingClosure& on_end_callback) {
  std::vector<std::unique_ptr<WeeklyTimeInterval>> intervals =
      GetPolicyConfigAsWeeklyTimeIntervals(policy_config);

  std::vector<std::unique_ptr<RepeatingTimeIntervalTaskExecutor>> executors;
  base::ranges::transform(
      intervals, std::back_inserter(executors),
      [&](const std::unique_ptr<WeeklyTimeInterval>& interval) {
        CHECK(interval != nullptr);
        return std::make_unique<RepeatingTimeIntervalTaskExecutor>(
            std::move(*interval), on_start_callback, on_end_callback);
      });

  return executors;
}

}  // namespace

DeviceWeeklyScheduledSuspendController::DeviceWeeklyScheduledSuspendController(
    PrefService* pref_service) {
  pref_change_registrar_.Init(pref_service);
  pref_change_registrar_.Add(
      prefs::kDeviceWeeklyScheduledSuspend,
      base::BindRepeating(&DeviceWeeklyScheduledSuspendController::
                              OnDeviceWeeklyScheduledSuspendUpdate,
                          weak_factory_.GetWeakPtr()));
}

DeviceWeeklyScheduledSuspendController::
    ~DeviceWeeklyScheduledSuspendController() = default;

const RepeatingTimeIntervalTaskExecutors&
DeviceWeeklyScheduledSuspendController::GetIntervalExecutorsForTesting() const {
  return interval_executors_;
}

void DeviceWeeklyScheduledSuspendController::
    OnDeviceWeeklyScheduledSuspendUpdate() {
  const base::Value::List& policy_config =
      g_browser_process->local_state()->GetList(
          prefs::kDeviceWeeklyScheduledSuspend);

  interval_executors_.clear();

  if (!AllWeeklyTimeIntervalsAreValid(policy_config)) {
    return;
  }

  interval_executors_ = BuildIntervalExecutorsFromConfig(
      policy_config,
      base::BindRepeating(
          &DeviceWeeklyScheduledSuspendController::OnTaskExecutorIntervalStart,
          weak_factory_.GetWeakPtr()),
      base::BindRepeating(
          &DeviceWeeklyScheduledSuspendController::OnTaskExecutorIntervalEnd,
          weak_factory_.GetWeakPtr()));
}

void DeviceWeeklyScheduledSuspendController::OnTaskExecutorIntervalStart() {
  // TODO(b/319210835): Request suspend from PowerManagerClient.
}

void DeviceWeeklyScheduledSuspendController::OnTaskExecutorIntervalEnd() {
  // No device wake-up needed. The `RepeatingTimeIntervalTaskExecutor`'s
  // underlying `NativeTimer` handles device wake-up at interval end.
}

}  // namespace ash
