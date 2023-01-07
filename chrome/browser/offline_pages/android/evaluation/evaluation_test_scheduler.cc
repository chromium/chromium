// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/android/evaluation/evaluation_test_scheduler.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/offline_pages/request_coordinator_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/offline_pages/core/background/device_conditions.h"
#include "components/offline_pages/core/background/request_coordinator.h"
#include "components/offline_pages/core/offline_event_logger.h"
#include "net/base/network_change_notifier.h"

namespace {
const int kBatteryPercentageHigh = 75;
const bool kPowerRequired = true;
}  // namespace

namespace offline_pages {

namespace android {

namespace {

const char kLogTag[] = "EvaluationTestScheduler";

void StartProcessing();

void ProcessingDoneCallback(bool result) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&StartProcessing));
}

void GetAllRequestsDone(
    std::vector<std::unique_ptr<SavePageRequest>> requests) {
  if (requests.size() > 0) {
    Profile* profile = ProfileManager::GetLastUsedProfile();
    RequestCoordinator* coordinator =
        RequestCoordinatorFactory::GetInstance()->GetForBrowserContext(profile);
    coordinator->StartImmediateProcessing(
        base::BindRepeating(&ProcessingDoneCallback));
  }
}

void StartProcessing() {
  // If there's no network connection then try in 2 seconds.
  if (net::NetworkChangeNotifier::GetConnectionType() ==
      net::NetworkChangeNotifier::ConnectionType::CONNECTION_NONE) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, base::BindOnce(&StartProcessing), base::Seconds(2));
    return;
  }
  Profile* profile = ProfileManager::GetLastUsedProfile();
  RequestCoordinator* coordinator =
      RequestCoordinatorFactory::GetInstance()->GetForBrowserContext(profile);
  coordinator->GetAllRequests(base::BindOnce(&GetAllRequestsDone));
}

}  // namespace

EvaluationTestScheduler::EvaluationTestScheduler()
    : coordinator_(nullptr),
      device_conditions_(kPowerRequired,
                         kBatteryPercentageHigh,
                         net::NetworkChangeNotifier::CONNECTION_2G) {}

EvaluationTestScheduler::~EvaluationTestScheduler() {}

void EvaluationTestScheduler::Schedule(
    const TriggerConditions& trigger_conditions) {
  if (!coordinator_) {
    Profile* profile = ProfileManager::GetLastUsedProfile();
    coordinator_ =
        RequestCoordinatorFactory::GetInstance()->GetForBrowserContext(profile);
    // It's not expected that the coordinator would be nullptr since this bridge
    // would only be used for testing scenario.
    DCHECK(coordinator_);
  }
  coordinator_->GetLogger()->RecordActivity(std::string(kLogTag) +
                                            " Start schedule!");
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&StartProcessing));
}

void EvaluationTestScheduler::BackupSchedule(
    const TriggerConditions& trigger_conditions,
    long delay_in_seconds) {
  // This method is not expected to be called in test harness. Adding a log in
  // case we somehow get called here and need to implement the method.
  if (coordinator_)
    coordinator_->GetLogger()->RecordActivity(std::string(kLogTag) +
                                              " BackupSchedule called!");
}

void EvaluationTestScheduler::Unschedule() {
  // This method is not expected to be called in test harness. Adding a log in
  // case we somehow get called here and need to implement the method.
  if (coordinator_)
    coordinator_->GetLogger()->RecordActivity(std::string(kLogTag) +
                                              " Unschedule called!");
}

DeviceConditions& EvaluationTestScheduler::GetCurrentDeviceConditions() {
  return device_conditions_;
}

void EvaluationTestScheduler::ImmediateScheduleCallback(bool result) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&StartProcessing));
}

}  // namespace android
}  // namespace offline_pages
