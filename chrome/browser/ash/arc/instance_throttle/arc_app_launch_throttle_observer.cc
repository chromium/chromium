// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/instance_throttle/arc_app_launch_throttle_observer.h"

#include "base/check_is_test.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"

namespace arc {

namespace {

constexpr base::TimeDelta kAppLaunchTimeout = base::Seconds(20);

}  // namespace

ArcAppLaunchThrottleObserver::ArcAppLaunchThrottleObserver()
    : ThrottleObserver("ArcAppLaunchRequested") {}

ArcAppLaunchThrottleObserver::~ArcAppLaunchThrottleObserver() = default;

void ArcAppLaunchThrottleObserver::StartObserving(
    content::BrowserContext* context,
    const ObserverStateChangedCallback& callback) {
  ThrottleObserver::StartObserving(context, callback);

  // if ArcWindowWatcher is available, it offers a more accurate cue of
  // when launched app window is created - and we use that instead
  // of task creation, which comes too early.
  if (ash::ArcWindowWatcher::instance()) {
    window_display_observation_.Observe(ash::ArcWindowWatcher::instance());
  } else {
    // Observe `OnTaskCreated`, which means the ARC task is created, but not
    // related window creation. For some apps running in background,
    // they don't have window.
    auto* app_list_prefs = ArcAppListPrefs::Get(context);
    if (app_list_prefs) {  // for unit testing
      task_creation_observation_.Observe(app_list_prefs);
    }
  }
  // Observe launch request.
  if (auto* notifier = ArcAppLaunchNotifier::GetForBrowserContext(context)) {
    launch_request_observation_.Observe(notifier);
  } else {
    CHECK_IS_TEST();
  }
}

void ArcAppLaunchThrottleObserver::StopObserving() {
  window_display_observation_.Reset();
  task_creation_observation_.Reset();
  launch_request_observation_.Reset();
  ThrottleObserver::StopObserving();
}

void ArcAppLaunchThrottleObserver::OnArcAppLaunchRequested(
    std::string_view identifier) {
  SetActive(true);
  current_requests_.insert(identifier.data());
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      // Create new std::string to prevent dangling pointer.
      base::BindOnce(&ArcAppLaunchThrottleObserver::OnLaunchedOrRequestExpired,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::string(identifier.data())),
      kAppLaunchTimeout);
}

void ArcAppLaunchThrottleObserver::OnArcAppLaunchNotifierDestroy() {
  launch_request_observation_.Reset();
}

void ArcAppLaunchThrottleObserver::OnTaskCreated(
    int32_t task_id,
    const std::string& package_name,
    const std::string& activity,
    const std::string& intent,
    int32_t session_id) {
  OnLaunchedOrRequestExpired(package_name);
}

void ArcAppLaunchThrottleObserver::OnArcWindowDisplayed(
    const std::string& package_name) {
  OnLaunchedOrRequestExpired(package_name);
}

void ArcAppLaunchThrottleObserver::OnWillDestroyWatcher() {
  window_display_observation_.Reset();
}

void ArcAppLaunchThrottleObserver::OnLaunchedOrRequestExpired(
    const std::string& name) {
  // This request has already expired or there are outstanding requests,
  // do not deactivate the observer.
  if (!current_requests_.erase(name) || current_requests_.size())
    return;
  SetActive(false);
}

}  // namespace arc
