// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/auto_sleep/device_weekly_scheduled_suspend_controller.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/ash/app_mode/auto_sleep/repeating_time_interval_task_executor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/policy/weekly_time/weekly_time_interval.h"
#include "components/prefs/pref_service.h"

namespace ash {

using ::policy::WeeklyTimeInterval;

namespace {

// Tag prefix for instances of `RepeatingTimeIntervalTaskExecutor`.
const char kRepeatingTaskExecutorTagPrefix[] =
    "DeviceWeeklyScheduledSuspend_%zu";

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
    const base::RepeatingClosure& on_start_callback,
    const base::RepeatingClosure& on_end_callback) {
  std::vector<std::unique_ptr<WeeklyTimeInterval>> intervals =
      GetPolicyConfigAsWeeklyTimeIntervals(policy_config);

  std::vector<std::string> task_executor_tags;

  for (size_t i = 0; i < intervals.size(); ++i) {
    std::string executor_tag =
        base::StringPrintf(kRepeatingTaskExecutorTagPrefix, i);
    task_executor_tags.emplace_back(executor_tag);
  }

  std::vector<std::unique_ptr<RepeatingTimeIntervalTaskExecutor>> executors;

  std::transform(intervals.begin(), intervals.end(), task_executor_tags.begin(),
                 std::back_inserter(executors),
                 [&](const std::unique_ptr<WeeklyTimeInterval>& interval,
                     const std::string& executor_name) {
                   CHECK(interval != nullptr);
                   return task_executor_factory->Create(
                       std::move(*interval), on_start_callback, on_end_callback,
                       executor_name);
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
}

DeviceWeeklyScheduledSuspendController::
    ~DeviceWeeklyScheduledSuspendController() = default;

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
    executor->Start();
  }
}

void DeviceWeeklyScheduledSuspendController::OnTaskExecutorIntervalStart() {
  chromeos::PowerManagerClient::Get()->RequestSuspend();
}

void DeviceWeeklyScheduledSuspendController::OnTaskExecutorIntervalEnd() {
  // No device wake-up needed. The `RepeatingTimeIntervalTaskExecutor`'s
  // underlying `NativeTimer` handles device wake-up at interval end.
}

}  // namespace ash
