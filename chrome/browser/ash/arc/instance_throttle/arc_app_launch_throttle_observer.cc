// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/instance_throttle/arc_app_launch_throttle_observer.h"

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
  // when launched app is displayed - and we use that instead
  // of task creation, which comes too early.
  if (ash::ArcWindowWatcher::instance()) {
    window_display_observation_.Observe(ash::ArcWindowWatcher::instance());
  } else {
    auto* app_list_prefs = ArcAppListPrefs::Get(context);
    if (app_list_prefs) {  // for unit testing
      task_creation_observation_.Observe(app_list_prefs);
    }
  }
  AddAppLaunchObserver(context, this);
}

void ArcAppLaunchThrottleObserver::StopObserving() {
  RemoveAppLaunchObserver(context(), this);
  window_display_observation_.Reset();
  task_creation_observation_.Reset();
  ThrottleObserver::StopObserving();
}

void ArcAppLaunchThrottleObserver::OnAppLaunchRequested(
    const ArcAppListPrefs::AppInfo& app_info) {
  SetActive(true);
  current_requests_.insert(app_info.package_name);
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ArcAppLaunchThrottleObserver::OnLaunchedOrRequestExpired,
                     weak_ptr_factory_.GetWeakPtr(), app_info.package_name),
      kAppLaunchTimeout);
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

void ArcAppLaunchThrottleObserver::OnLaunchedOrRequestExpired(
    const std::string& name) {
  // This request has already expired or there are outstanding requests,
  // do not deactivate the observer.
  if (!current_requests_.erase(name) || current_requests_.size())
    return;
  SetActive(false);
}

}  // namespace arc
