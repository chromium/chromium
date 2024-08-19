// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INSTANCE_THROTTLE_ARC_APP_LAUNCH_THROTTLE_OBSERVER_H_
#define CHROME_BROWSER_ASH_ARC_INSTANCE_THROTTLE_ARC_APP_LAUNCH_THROTTLE_OBSERVER_H_

#include <set>
#include <string>

#include "ash/components/arc/app/arc_app_launch_notifier.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/arc/util/arc_window_watcher.h"
#include "chromeos/ash/components/throttle/throttle_observer.h"

namespace content {
class BrowserContext;
}

namespace arc {

// This class observes ARC app launches and sets its state to active while an
// app is being launched.
class ArcAppLaunchThrottleObserver
    : public ash::ThrottleObserver,
      public ArcAppListPrefs::Observer,
      public ash::ArcWindowWatcher::ArcWindowDisplayObserver,
      public ArcAppLaunchNotifier::Observer {
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

  // ArcAppLaunchNotifier::Observer:
  void OnArcAppLaunchRequested(std::string_view identifier) override;
  void OnArcAppLaunchNotifierDestroy() override;

  // ArcAppListPrefs::Observer:
  void OnTaskCreated(int32_t task_id,
                     const std::string& package_name,
                     const std::string& activity,
                     const std::string& intent,
                     int32_t session_id) override;

  // ash::ArcWindowWatcher::ArcWindowDisplayObserver
  void OnArcWindowDisplayed(const std::string& package_name) override;
  void OnWillDestroyWatcher() override;

 private:
  void OnLaunchedOrRequestExpired(const std::string& name);

  std::set<std::string> current_requests_;

  base::ScopedObservation<ash::ArcWindowWatcher,
                          ash::ArcWindowWatcher::ArcWindowDisplayObserver>
      window_display_observation_{this};

  base::ScopedObservation<ArcAppListPrefs, ArcAppListPrefs::Observer>
      task_creation_observation_{this};

  base::ScopedObservation<ArcAppLaunchNotifier, ArcAppLaunchNotifier::Observer>
      launch_request_observation_{this};

  // Must go last.
  base::WeakPtrFactory<ArcAppLaunchThrottleObserver> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_INSTANCE_THROTTLE_ARC_APP_LAUNCH_THROTTLE_OBSERVER_H_
