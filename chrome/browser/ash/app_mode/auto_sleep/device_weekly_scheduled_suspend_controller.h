// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_AUTO_SLEEP_DEVICE_WEEKLY_SCHEDULED_SUSPEND_CONTROLLER_H_
#define CHROME_BROWSER_ASH_APP_MODE_AUTO_SLEEP_DEVICE_WEEKLY_SCHEDULED_SUSPEND_CONTROLLER_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_mode/auto_sleep/repeating_time_interval_task_executor.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;

namespace ash {

using RepeatingTimeIntervalTaskExecutors =
    std::vector<std::unique_ptr<RepeatingTimeIntervalTaskExecutor>>;

// `DeviceWeeklyScheduledSuspendController` suspends the device during a kiosk
// session based on weekly schedules defined in the DeviceWeeklyScheduledSuspend
// policy.
class DeviceWeeklyScheduledSuspendController
    : public chromeos::PowerManagerClient::Observer {
 public:
  explicit DeviceWeeklyScheduledSuspendController(PrefService* pref_service);
  DeviceWeeklyScheduledSuspendController(
      const DeviceWeeklyScheduledSuspendController&) = delete;
  DeviceWeeklyScheduledSuspendController& operator=(
      const DeviceWeeklyScheduledSuspendController&) = delete;

  ~DeviceWeeklyScheduledSuspendController() override;

  // chromeos::PowerManagerClient::Observer:
  void PowerManagerBecameAvailable(bool available) override;
  void SuspendDone(base::TimeDelta sleep_duration) override;
  void DarkSuspendImminent() override;

  const RepeatingTimeIntervalTaskExecutors& GetIntervalExecutorsForTesting()
      const;

  void SetTaskExecutorFactoryForTesting(
      std::unique_ptr<RepeatingTimeIntervalTaskExecutor::Factory> factory);
  void SetClockForTesting(base::Clock* clock);

 private:
  // Called on `kDeviceWeeklyScheduledSuspend` preference update.
  void OnDeviceWeeklyScheduledSuspendUpdate();

  // Called when a suspend interval starts with the remaining interval duration.
  void OnTaskExecutorIntervalStart(base::TimeDelta interval_duration);

  // Called when a suspend interval ends.
  // TODO(b/330836068): Remove interval end callback.
  void OnTaskExecutorIntervalEnd();

  // Monitors `kDeviceWeeklyScheduledSuspend` preference update.
  PrefChangeRegistrar pref_change_registrar_;

  // Interval executors used to schedule device suspension and wake-up.
  RepeatingTimeIntervalTaskExecutors interval_executors_;

  // Marks the end of the current sleep interval. Null while not sleeping.
  // See `OnTaskExecutorIntervalStart` definition for more info.
  std::optional<base::Time> resume_after_;

  bool power_manager_available_ = false;

  std::unique_ptr<RepeatingTimeIntervalTaskExecutor::Factory>
      task_executor_factory_;

  raw_ptr<base::Clock> clock_ = base::DefaultClock::GetInstance();

  base::ScopedObservation<chromeos::PowerManagerClient,
                          chromeos::PowerManagerClient::Observer>
      power_manager_observer_{this};

  base::WeakPtrFactory<DeviceWeeklyScheduledSuspendController> weak_factory_{
      this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_AUTO_SLEEP_DEVICE_WEEKLY_SCHEDULED_SUSPEND_CONTROLLER_H_
