// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_METRICS_APP_PLATFORM_METRICS_UTILS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_METRICS_APP_PLATFORM_METRICS_UTILS_H_

#include "base/time/time.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"

class Profile;

namespace aura {
class Window;
}

namespace apps {

// This is used for logging, so do not remove or reorder existing entries.
// This should be kept in sync with:
// * c/b/apps/app_service/metrics/app_platform_metrics_utils.cc:kAppTypeNameMap
// * tools/metrics/histograms/metadata/apps/histograms.xml:AppType
enum class AppTypeName {
  kUnknown = 0,
  kArc = 1,
  // kBuiltIn = 2,  removed
  kCrostini = 3,
  kChromeApp = 4,
  kWeb = 5,
  // kMacOs = 6,                        // Removed.
  kPluginVm = 7,
  // kStandaloneBrowser = 8,            // Removed.
  kRemote = 9,
  kBorealis = 10,
  kSystemWeb = 11,
  kChromeBrowser = 12,
  // kStandaloneBrowserChromeApp = 13,  // Removed.
  kExtension = 14,
  // kStandaloneBrowserExtension = 15,  // Removed.
  // kStandaloneBrowserWebApp = 16,     // Removed.
  kBruschetta = 17,

  // Add any new values above this one, and update kMaxValue to the highest
  // enumerator value.
  kMaxValue = kBruschetta,
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
  // kBuiltIn = 2,  removed.
  kCrostini = 3,
  kChromeAppWindow = 4,
  kChromeAppTab = 5,
  kWebWindow = 6,
  kWebTab = 7,
  // kMacOs = 8,                              // Removed.
  kPluginVm = 9,
  // kStandaloneBrowser = 10,                 // Removed.
  kRemote = 11,
  kBorealis = 12,
  kSystemWeb = 13,
  kChromeBrowser = 14,
  // kStandaloneBrowserChromeApp = 15,        // Removed.
  kExtension = 16,
  // kStandaloneBrowserExtension = 17,        // Removed.
  // kStandaloneBrowserChromeAppWindow = 18,  // Removed.
  // kStandaloneBrowserChromeAppTab = 19,     // Removed.
  // kStandaloneBrowserWebAppWindow = 20,     // Removed.
  // kStandaloneBrowserWebAppTab = 21,        // Removed.
  kBruschetta = 22,

  // Add any new values above this one, and update kMaxValue to the highest
  // enumerator value.
  kMaxValue = kBruschetta,
};

extern const base::TimeDelta kMinDuration;
extern const base::TimeDelta kMaxUsageDuration;
extern const int kDurationBuckets;
extern const int kUsageTimeBuckets;

inline constexpr char kArcHistogramName[] = "Arc";
inline constexpr char kCrostiniHistogramName[] = "Crostini";
inline constexpr char kChromeAppHistogramName[] = "ChromeApp";
inline constexpr char kWebAppHistogramName[] = "WebApp";
inline constexpr char kPluginVmHistogramName[] = "PluginVm";
inline constexpr char kRemoteHistogramName[] = "RemoteApp";
inline constexpr char kBorealisHistogramName[] = "Borealis";
inline constexpr char kSystemWebAppHistogramName[] = "SystemWebApp";
inline constexpr char kChromeBrowserHistogramName[] = "ChromeBrowser";
inline constexpr char kExtensionHistogramName[] = "Extension";
inline constexpr char kBruschettaHistogramName[] = "Bruschetta";

// Determines what app type a web app should be logged as based on its launch
// container and app id. In particular, web apps in tabs are logged as part of
// Chrome browser.
AppTypeName GetAppTypeNameForWebApp(Profile* profile,
                                    const std::string& app_id,
                                    apps::LaunchContainer container);

// Returns false if |window| is a Chrome app window or a standalone web app
// window. Otherwise, return true.
bool IsAshBrowserWindow(aura::Window* window);

// Returns true if the app with |app_id| is opened as a tab in a browser window.
// Otherwise, return false.
bool IsAppOpenedInTab(AppTypeName app_type_name, const std::string& app_id);

// Returns true if the app with |app_type| is opened with a browser window.
// Otherwise, return false.
bool IsAppOpenedWithBrowserWindow(Profile* profile,
                                  AppType app_type,
                                  const std::string& app_id);

// Determines what app type a web app should be logged as based on |window|. In
// particular, web apps in tabs are logged as part of Chrome browser.
AppTypeName GetAppTypeNameForWebAppWindow(Profile* profile,
                                          const std::string& app_id,
                                          aura::Window* window);

// Returns AppTypeName used for app running metrics.
AppTypeName GetAppTypeNameForWindow(Profile* profile,
                                    AppType app_type,
                                    const std::string& app_id,
                                    aura::Window* window);

// Returns the string for `app_type_name` to present the histogram name and save
// the app type in the user pref for the usage time and input event metrics.
std::string GetAppTypeHistogramName(apps::AppTypeName app_type_name);

// Returns AppTypeName for the given `app_type_name` string.
AppTypeName GetAppTypeNameFromString(const std::string& app_type_name);

// Returns InstallReason string to use in UMA names.
std::string GetInstallReason(InstallReason install_reason);

// Returns true if it's permitted to record App keyed metrics (AppKM) for
// `app_id` in `profile`.
bool ShouldRecordAppKMForAppId(Profile* profile,
                               const AppRegistryCache& cache,
                               const std::string& app_id);

// Returns true if we are allowed to record AppKM for `profile`. When recording
// AppKM for a particular app, prefer `ShouldRecordAppKMForAppId`, which also
// checks this function. This function can be used to disable functionality
// entirely when AppKM is not allowed.
bool ShouldRecordAppKM(Profile* profile);

// Due to the privacy limitation, only ARC apps, Chrome apps and web apps(PWA),
// system web apps, builtin apps, Borealis apps, and Crostini apps are recorded
// because they are synced to server/cloud, or part of OS. Other app types,
// e.g. remote apps, etc, are not recorded. So returns true if the
// app_type_name is allowed to record AppKM. Otherwise, returns false.
//
// See DD: go/app-platform-metrics-using-ukm for details.
bool ShouldRecordAppKMForAppTypeName(AppType app_type_name);

int GetUserTypeByDeviceTypeMetrics();

// Returns AppTypeName used for app launch metrics.
AppTypeName GetAppTypeName(Profile* profile,
                           AppType app_type,
                           const std::string& app_id,
                           apps::LaunchContainer container);

// Gets the app type of a given app_id. Checks multiple sources, not just the
// app registry cache, so can identify apps which aren't registered with app
// service.
AppType GetAppType(Profile* profile, const std::string& app_id);

// Returns true if |app_id| is a system web app for a given |profile|.
bool IsSystemWebApp(Profile* profile, const std::string& app_id);

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_METRICS_APP_PLATFORM_METRICS_UTILS_H_
