// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_ANDROID_EVALUATION_EVALUATION_TEST_SCHEDULER_H_
#define CHROME_BROWSER_OFFLINE_PAGES_ANDROID_EVALUATION_EVALUATION_TEST_SCHEDULER_H_

#include "components/offline_pages/core/background/scheduler.h"

namespace offline_pages {

class RequestCoordinator;

namespace android {

class EvaluationTestScheduler : public Scheduler {
 public:
  EvaluationTestScheduler();
  ~EvaluationTestScheduler() override;

  // Scheduler implementation.
  void Schedule(const TriggerConditions& trigger_conditions) override;
  void BackupSchedule(const TriggerConditions& trigger_conditions,
                      long delay_in_seconds) override;
  void Unschedule() override;
  DeviceConditions& GetCurrentDeviceConditions() override;

  // Callback used by user request.
  void ImmediateScheduleCallback(bool result);

 private:
  RequestCoordinator* coordinator_;
  DeviceConditions device_conditions_;
};

}  // namespace android
}  // namespace offline_pages

#endif  // CHROME_BROWSER_OFFLINE_PAGES_ANDROID_EVALUATION_EVALUATION_TEST_SCHEDULER_H_
