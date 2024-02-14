// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/device_weekly_scheduled_suspend_controller.h"

#include <memory>

#include "base/values.h"
#include "chrome/browser/ash/app_mode/repeating_time_interval_task_executor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace ash {

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
  const base::Value::List& schedule_list =
      g_browser_process->local_state()->GetList(
          prefs::kDeviceWeeklyScheduledSuspend);

  // TODO(b/322341636): Validate that the pref contains valid schedules, and
  // that it does not contain overlapped entries.
  interval_executors_.clear();

  for (const base::Value& schedule : schedule_list) {
    std::unique_ptr<policy::WeeklyTimeInterval> interval =
        policy::WeeklyTimeInterval::ExtractFromDict(
            schedule.GetDict(),
            /*timezone_offset=*/std::nullopt);
    CHECK(interval != nullptr);

    interval_executors_.emplace_back(std::make_unique<
                                     RepeatingTimeIntervalTaskExecutor>(
        std::move(*interval),
        base::BindRepeating(&DeviceWeeklyScheduledSuspendController::
                                OnTaskExecutorIntervalStart,
                            weak_factory_.GetWeakPtr()),
        base::BindRepeating(
            &DeviceWeeklyScheduledSuspendController::OnTaskExecutorIntervalEnd,
            weak_factory_.GetWeakPtr())));
  }
}

void DeviceWeeklyScheduledSuspendController::OnTaskExecutorIntervalStart() {
  // TODO(b/319210835): Request suspend from PowerManagerClient.
}

void DeviceWeeklyScheduledSuspendController::OnTaskExecutorIntervalEnd() {
  // No device wake-up needed. The `RepeatingTimeIntervalTaskExecutor`'s
  // underlying `NativeTimer` handles device wake-up at interval end.
}

}  // namespace ash
