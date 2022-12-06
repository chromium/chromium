// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INSTANCE_THROTTLE_ARC_APP_LAUNCH_THROTTLE_OBSERVER_H_
#define CHROME_BROWSER_ASH_ARC_INSTANCE_THROTTLE_ARC_APP_LAUNCH_THROTTLE_OBSERVER_H_

#include <set>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/throttle_observer.h"

namespace content {
class BrowserContext;
}

namespace arc {

// This class observes ARC app launches and sets its state to active while an
// app is being launched.
class ArcAppLaunchThrottleObserver : public ash::ThrottleObserver,
                                     public ArcAppListPrefs::Observer,
                                     public AppLaunchObserver {
 public:
  ArcAppLaunchThrottleObserver();

  ArcAppLaunchThrottleObserver(const ArcAppLaunchThrottleObserver&) = delete;
  ArcAppLaunchThrottleObserver& operator=(const ArcAppLaunchThrottleObserver&) =
      delete;

  ~ArcAppLaunchThrottleObserver() override;

  // ash::ThrottleObserver:
  void StartObserving(content::BrowserContext* context,
                      const ObserverStateChangedCallback& callback) override;
  void StopObserving() override;

  // AppLaunchObserver:
  void OnAppLaunchRequested(const ArcAppListPrefs::AppInfo& app_info) override;

  // ArcAppListPrefs::Observer:
  void OnTaskCreated(int32_t task_id,
                     const std::string& package_name,
                     const std::string& activity,
                     const std::string& intent,
                     int32_t session_id) override;

 private:
  void OnLaunchedOrRequestExpired(const std::string& name);

  std::set<std::string> current_requests_;
  // Must go last.
  base::WeakPtrFactory<ArcAppLaunchThrottleObserver> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_INSTANCE_THROTTLE_ARC_APP_LAUNCH_THROTTLE_OBSERVER_H_
