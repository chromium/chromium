// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_PLATFORM_METRICS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_PLATFORM_METRICS_H_

#include <map>
#include <set>

#include "base/time/time.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/instance_registry.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
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
  kStandaloneBrowserExtension = 13,

  // Add any new values above this one, and update kMaxValue to the highest
  // enumerator value.
  kMaxValue = kStandaloneBrowserExtension,
};

// This is used for logging, so do not remove or reorder existing entries.
// The diferences with AppTypeName are:
// 1. If a Chrome app opened in a tab, it is logged as kChromeBrowser in
// AppTypeName, but logged as kChromeAppTab in AppTypeNameV2.
// 2. If a web app opened in a tab, it is logged as kChromeBrowser in
// AppTypeName, but logged as kWebTab in AppTypeNameV2.
enum class AppTypeNameV2 {
  kUnknown = 0,
  kArc = 1,
  kBuiltIn = 2,
  kCrostini = 3,
  kChromeAppWindow = 4,
  kChromeAppTab = 5,
  kWebWindow = 6,
  kWebTab = 7,
  kMacOs = 8,
  kPluginVm = 9,
  kStandaloneBrowser = 10,
  kRemote = 11,
  kBorealis = 12,
  kSystemWeb = 13,
  kChromeBrowser = 14,
  kStandaloneBrowserExtension = 15,

  // Add any new values above this one, and update kMaxValue to the highest
  // enumerator value.
  kMaxValue = kStandaloneBrowserExtension,
};

// This is used for logging, so do not remove or reorder existing entries.
enum class InstallTime {
  kInit = 0,
  kRunning = 1,

  // Add any new values above this one, and update kMaxValue to the highest
  // enumerator value.
  kMaxValue = kRunning,
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

extern const char kChromeAppTabHistogramName[];
extern const char kChromeAppWindowHistogramName[];
extern const char kWebAppTabHistogramName[];
extern const char kWebAppWindowHistogramName[];

std::string GetAppTypeHistogramName(apps::AppTypeName app_type_name);
std::string GetAppTypeHistogramNameV2(apps::AppTypeNameV2 app_type_name);

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

  // UMA metrics name for apps usage time in Chrome OS for AppTypeName.
  static std::string GetAppsUsageTimeHistogramNameForTest(
      AppTypeName app_type_name);

  // UMA metrics name for apps usage time in Chrome OS for AppTypeNameV2.
  static std::string GetAppsUsageTimeHistogramNameForTest(
      AppTypeNameV2 app_type_name);

  void OnNewDay();
  void OnTenMinutes();
  void OnFiveMinutes();

  // Records UKM when launching an app.
  void RecordAppLaunchUkm(apps::mojom::AppType app_type,
                          const std::string& app_id,
                          apps::mojom::LaunchSource launch_source,
                          apps::mojom::LaunchContainer container);

  // Records UKM when uninstalling an app.
  void RecordAppUninstallUkm(apps::mojom::AppType app_type,
                             const std::string& app_id,
                             apps::mojom::UninstallSource uninstall_source);

 private:
  struct RunningStartTime {
    base::TimeTicks start_time;
    AppTypeName app_type_name;
    AppTypeNameV2 app_type_name_v2;
    std::string app_id;
  };

  struct UsageTime {
    base::TimeDelta running_time;
    ukm::SourceId source_id = ukm::kInvalidSourceId;
    AppTypeName app_type_name = AppTypeName::kUnknown;
    bool window_is_closed = false;
  };

  struct BrowserToTab {
    aura::Window* browser_window = nullptr;
    aura::Window* tab_window = nullptr;
  };

  using BrowserToTabs = std::list<BrowserToTab>;

  // AppRegistryCache::Observer:
  void OnAppTypeInitialized(apps::mojom::AppType app_type) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;
  void OnAppUpdate(const apps::AppUpdate& update) override;

  // apps::InstanceRegistry::Observer:
  void OnInstanceUpdate(const apps::InstanceUpdate& update) override;
  void OnInstanceRegistryWillBeDestroyed(
      apps::InstanceRegistry* cache) override;

  // Updates the browser window status when the web app tab of `tab_window` is
  // inactivated.
  void UpdateBrowserWindowStatus(aura::Window* tab_window);

  // Returns true if the browser with `browser_window` has activated tabs.
  // Otherwise, returns false.
  bool HasActivatedTab(aura::Window* browser_window);

  // Returns the browser window for `tab_window`.
  aura::Window* GetBrowserWindow(aura::Window* tab_window) const;

  // Adds an activated `browser_window` and `tab_window` to
  // `active_browser_to_tabs_`.
  void AddActivatedTab(aura::Window* browser_window, aura::Window* tab_window);

  // Removes `tab_window` from `active_browser_to_tabs_`.
  void RemoveActivatedTab(aura::Window* tab_window);

  void SetWindowActivated(apps::mojom::AppType app_type,
                          AppTypeName app_type_name,
                          AppTypeNameV2 app_type_name_v2,
                          const std::string& app_id,
                          aura::Window* window);
  void SetWindowInActivated(const std::string& app_id,
                            aura::Window* window,
                            apps::InstanceState state);

  void InitRunningDuration();
  void ClearRunningDuration();

  // Records the number of apps of the given `app_type` that the family user has
  // recently used.
  void RecordAppsCount(apps::mojom::AppType app_type);

  // Records the app running duration.
  void RecordAppsRunningDuration();

  // Records the app usage time metrics (both UMA and UKM) in five minutes
  // intervals.
  void RecordAppsUsageTime();

  // Records the app usage time UKM in five minutes intervals.
  void RecordAppsUsageTimeUkm();

  // Records the installed app in Chrome OS.
  void RecordAppsInstallUkm(const apps::AppUpdate& update,
                            InstallTime install_time);

  // Returns true if we are allowed to record UKM. Otherwise, returns false.
  bool ShouldRecordUkm();

  // Returns the SourceId of UKM for `app_id`.
  ukm::SourceId GetSourceId(const std::string& app_id);

  Profile* const profile_ = nullptr;

  AppRegistryCache& app_registry_cache_;

  bool should_record_metrics_on_new_day_ = false;

  bool should_refresh_duration_pref = false;
  bool should_refresh_activated_count_pref = false;

  int user_type_by_device_type_;

  // Records the map from browsers to activated web apps tabs.
  BrowserToTabs active_browsers_to_tabs_;

  // |running_start_time_| and |running_duration_| are used for accumulating app
  // running duration per each day interval.
  std::map<aura::Window*, RunningStartTime> running_start_time_;
  std::map<AppTypeName, base::TimeDelta> running_duration_;
  std::map<AppTypeName, int> activated_count_;

  // |start_time_per_five_minutes_|, |app_type_running_time_per_five_minutes_|,
  // |app_type_v2_running_time_per_five_minutes_|, and
  // |usage_time_per_five_minutes_| are used for accumulating app
  // running duration per 5 minutes interval.
  std::map<aura::Window*, RunningStartTime> start_time_per_five_minutes_;
  std::map<AppTypeName, base::TimeDelta>
      app_type_running_time_per_five_minutes_;
  std::map<AppTypeNameV2, base::TimeDelta>
      app_type_v2_running_time_per_five_minutes_;
  std::map<aura::Window*, UsageTime> usage_time_per_five_minutes_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_PLATFORM_METRICS_H_
