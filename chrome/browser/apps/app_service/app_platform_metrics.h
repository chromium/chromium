// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_PLATFORM_METRICS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_PLATFORM_METRICS_H_

#include <map>

#include "base/time/time.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/instance_registry.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "ui/aura/window.h"

class Profile;

namespace apps {

class AppUpdate;

// This is used for logging, so do not remove or reorder existing entries.
// This should be kept in sync with GetAppTypeNameSet in
// c/b/apps/app_service/app_platform_metrics_service.cc.
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

extern const char kAppRunningDuration[];
extern const char kAppActivatedCount[];

extern const char kArcHistogramName[];
extern const char kBuiltInHistogramName[];
extern const char kCrostiniHistogramName[];
extern const char kChromeAppHistogramName[];
extern const char kWebAppHistogramName[];
extern const char kMacOsHistogramName[];
extern const char kPluginVmHistogramName[];
extern const char kStandaloneBrowserHistogramName[];
extern const char kRemoteHistogramName[];
extern const char kBorealisHistogramName[];
extern const char kSystemWebAppHistogramName[];
extern const char kChromeBrowserHistogramName[];

std::string GetAppTypeHistogramName(apps::AppTypeName app_type_name);

const std::set<apps::AppTypeName>& GetAppTypeNameSet();

// Records metrics when launching apps.
void RecordAppLaunchMetrics(Profile* profile,
                            apps::mojom::AppType app_type,
                            const std::string& app_id,
                            apps::mojom::LaunchSource launch_source,
                            apps::mojom::LaunchContainer container);

class AppPlatformMetrics : public apps::AppRegistryCache::Observer,
                           public apps::InstanceRegistry::Observer {
 public:
  explicit AppPlatformMetrics(Profile* profile,
                              apps::AppRegistryCache& app_registry_cache,
                              InstanceRegistry& instance_registry);
  AppPlatformMetrics(const AppPlatformMetrics&) = delete;
  AppPlatformMetrics& operator=(const AppPlatformMetrics&) = delete;
  ~AppPlatformMetrics() override;

  // UMA metrics name for installed apps count in Chrome OS.
  static std::string GetAppsCountHistogramNameForTest(
      AppTypeName app_type_name);

  // UMA metrics name for installed apps count per InstallSource in Chrome OS.
  static std::string GetAppsCountPerInstallSourceHistogramNameForTest(
      AppTypeName app_type_name,
      apps::mojom::InstallSource install_source);

  // UMA metrics name for apps running duration in Chrome OS.
  static std::string GetAppsRunningDurationHistogramNameForTest(
      AppTypeName app_type_name);

  // UMA metrics name for apps running percentage in Chrome OS.
  static std::string GetAppsRunningPercentageHistogramNameForTest(
      AppTypeName app_type_name);

  // UMA metrics name for app window activated count in Chrome OS.
  static std::string GetAppsActivatedCountHistogramNameForTest(
      AppTypeName app_type_name);

  // UMA metrics name for apps usage time in Chrome OS.
  static std::string GetAppsUsageTimeHistogramNameForTest(
      AppTypeName app_type_name);

  void OnNewDay();
  void OnTenMinutes();
  void OnFiveMinutes();

 private:
  struct RunningStartTime {
    base::TimeTicks start_time;
    AppTypeName app_type_name;
  };

  // AppRegistryCache::Observer:
  void OnAppTypeInitialized(apps::mojom::AppType app_type) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;
  void OnAppUpdate(const apps::AppUpdate& update) override;

  // apps::InstanceRegistry::Observer:
  void OnInstanceUpdate(const apps::InstanceUpdate& update) override;
  void OnInstanceRegistryWillBeDestroyed(
      apps::InstanceRegistry* cache) override;

  void InitRunningDuration();
  void ClearRunningDuration();

  // Records the number of apps of the given `app_type` that the family user has
  // recently used.
  void RecordAppsCount(apps::mojom::AppType app_type);

  // Records the app running duration.
  void RecordAppsRunningDuration();

  // Records the app usage time in five minutes.
  void RecordAppsUsageTime();

  Profile* const profile_ = nullptr;

  AppRegistryCache& app_registry_cache_;

  bool should_record_metrics_on_new_day_ = false;

  bool should_refresh_duration_pref = false;
  bool should_refresh_activated_count_pref = false;

  // |running_start_time_| and |running_duration_| are used for accumulating app
  // running duration per each day interval.
  std::map<aura::Window*, RunningStartTime> running_start_time_;
  std::map<AppTypeName, base::TimeDelta> running_duration_;
  std::map<AppTypeName, int> activated_count_;

  // |start_time_per_five_minutes_| and |running_time_per_five_minutes_| are
  // used for accumulating app running duration per 5 minutes interval.
  std::map<aura::Window*, RunningStartTime> start_time_per_five_minutes_;
  std::map<AppTypeName, base::TimeDelta> running_time_per_five_minutes_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_PLATFORM_METRICS_H_
