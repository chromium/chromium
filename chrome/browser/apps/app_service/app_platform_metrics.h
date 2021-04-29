// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_PLATFORM_METRICS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_PLATFORM_METRICS_H_

#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/mojom/types.mojom.h"

class Profile;

namespace apps {

class AppUpdate;

// This is used for logging, so do not remove or reorder existing entries.
enum class AppTypeName {
  kUnknown = 0,
  kArc = 1,
  kBuiltIn = 2,
  kCrostini = 3,
  kChromeApp = 4,
  kWeb = 5,
  kMacOs = 6,
  kPluginVm = 7,
  kStandaloneBrowser = 8,
  kRemote = 9,
  kBorealis = 10,
  kSystemWeb = 11,
  kChromeBrowser = 12,

  // Add any new values above this one, and update kMaxValue to the highest
  // enumerator value.
  kMaxValue = kChromeBrowser,
};

// Records metrics when launching apps.
void RecordAppLaunchMetrics(Profile* profile,
                            const apps::AppUpdate& update,
                            apps::mojom::LaunchSource launch_source,
                            apps::mojom::LaunchContainer container);

class AppPlatformMetrics : public apps::AppRegistryCache::Observer {
 public:
  explicit AppPlatformMetrics(Profile* profile,
                              apps::AppRegistryCache& app_registry_cache);
  AppPlatformMetrics(const AppPlatformMetrics&) = delete;
  AppPlatformMetrics& operator=(const AppPlatformMetrics&) = delete;
  ~AppPlatformMetrics() override;

  // UMA metrics for a snapshot count of recently used apps for a given family
  // user.
  static const char* GetAppsCountHistogramNameForTest(
      AppTypeName app_type_name);

  void OnNewDay();

 private:
  // AppRegistryCache::Observer:
  void OnAppTypeInitialized(apps::mojom::AppType app_type) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;
  void OnAppUpdate(const apps::AppUpdate& update) override;

  // Records the number of apps of the given `app_type` that the family user has
  // recently used.
  void RecordAppsCount(apps::mojom::AppType app_type);

  Profile* const profile_ = nullptr;

  apps::AppRegistryCache& app_registry_cache_;

  bool should_record_metrics_on_new_day_ = false;
  bool first_report_on_current_device_ = false;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_PLATFORM_METRICS_H_
