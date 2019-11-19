// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/instance_throttle/arc_app_launch_throttle_observer.h"

#include "base/location.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/time/time.h"

namespace arc {

namespace {

constexpr base::TimeDelta kAppLaunchTimeout = base::TimeDelta::FromSeconds(20);

}  // namespace

ArcAppLaunchThrottleObserver::ArcAppLaunchThrottleObserver()
    : ThrottleObserver(ThrottleObserver::PriorityLevel::CRITICAL,
                       "ArcAppLaunchRequested") {}

ArcAppLaunchThrottleObserver::~ArcAppLaunchThrottleObserver() = default;

void ArcAppLaunchThrottleObserver::StartObserving(
    content::BrowserContext* context,
    const ObserverStateChangedCallback& callback) {
  ThrottleObserver::StartObserving(context, callback);
  auto* app_list_prefs = ArcAppListPrefs::Get(context);
  if (app_list_prefs)  // for unit testing
    app_list_prefs->AddObserver(this);
  AddAppLaunchObserver(context, this);
}

void ArcAppLaunchThrottleObserver::StopObserving() {
  RemoveAppLaunchObserver(context(), this);
  auto* app_list_prefs = ArcAppListPrefs::Get(context());
  if (app_list_prefs)  // for unit testing
    app_list_prefs->RemoveObserver(this);
  ThrottleObserver::StopObserving();
}

void ArcAppLaunchThrottleObserver::OnAppLaunchRequested(
    const ArcAppListPrefs::AppInfo& app_info) {
  SetActive(true);
  current_requests_.insert(app_info.package_name);
  base::PostDelayedTask(
      FROM_HERE, {base::CurrentThread()},
      base::BindOnce(&ArcAppLaunchThrottleObserver::OnLaunchedOrRequestExpired,
                     weak_ptr_factory_.GetWeakPtr(), app_info.package_name),
      kAppLaunchTimeout);
}

void ArcAppLaunchThrottleObserver::OnTaskCreated(
    int32_t task_id,
    const std::string& package_name,
    const std::string& activity,
    const std::string& intent) {
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
