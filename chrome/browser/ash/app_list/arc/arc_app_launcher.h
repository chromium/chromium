// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_APP_LAUNCHER_H_
#define CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_APP_LAUNCHER_H_

#include <stdint.h>

#include <string>

#include "ash/components/arc/metrics/arc_metrics_constants.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/intent.h"

namespace content {
class BrowserContext;
}

// Helper class for deferred ARC app launching in case app is not ready on the
// moment of request.
class ArcAppLauncher : public ArcAppListPrefs::Observer,
                       public apps::AppRegistryCache::Observer {
 public:
  ArcAppLauncher(content::BrowserContext* context,
                 const std::string& app_id,
                 apps::IntentPtr launch_intent,
                 bool deferred_launch_allowed,
                 int64_t display_id,
                 apps::LaunchSource launch_source);

  ArcAppLauncher(const ArcAppLauncher&) = delete;
  ArcAppLauncher& operator=(const ArcAppLauncher&) = delete;

  ~ArcAppLauncher() override;

  bool app_launched() const { return app_launched_; }

  // ArcAppListPrefs::Observer:
  void OnAppRegistered(const std::string& app_id,
                       const ArcAppListPrefs::AppInfo& app_info) override;
  void OnAppStatesChanged(const std::string& app_id,
                          const ArcAppListPrefs::AppInfo& app_info) override;
  void OnArcAppListPrefsDestroyed() override;

  // apps::AppRegistryCache::Observer overrides:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

 private:
  bool MaybeLaunchApp(const std::string& app_id,
                      const ArcAppListPrefs::AppInfo& app_info,
                      apps::Readiness readiness);

  // Unowned pointer.
  raw_ptr<content::BrowserContext> context_;
  // ARC app id.
  const std::string app_id_;
  // Optional intent to launch the app. If not set then app is started default
  // way.
  apps::IntentPtr launch_intent_;
  // If it is set to true that means app is allowed to launch in deferred mode
  // once it is registered, regardless it is ready or not. Otherwise app is
  // launched when it becomes ready.
  const bool deferred_launch_allowed_;
  // Display where the app should be launched.
  const int64_t display_id_;
  // Flag indicating that ARC app was launched.
  bool app_launched_ = false;
  // Enum that indicates what type of metric to record to UMA on launch.
  apps::LaunchSource launch_source_;

  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      app_registry_cache_observer_{this};

  base::ScopedObservation<ArcAppListPrefs, ArcAppListPrefs::Observer>
      arc_app_list_prefs_observer_{this};
};

#endif  // CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_APP_LAUNCHER_H_
