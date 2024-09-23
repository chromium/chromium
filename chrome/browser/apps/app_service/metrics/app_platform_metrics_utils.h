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
  kBuiltIn = 2,
  kCrostini = 3,
  kChromeApp = 4,
  kWeb = 5,
  // kMacOs = 6,  // Removed.
  kPluginVm = 7,
  kStandaloneBrowser = 8,
  kRemote = 9,
  kBorealis = 10,
  kSystemWeb = 11,
  kChromeBrowser = 12,
  kStandaloneBrowserChromeApp = 13,
  kExtension = 14,
  kStandaloneBrowserExtension = 15,
  kStandaloneBrowserWebApp = 16,
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
  kBuiltIn = 2,
  kCrostini = 3,
  kChromeAppWindow = 4,
  kChromeAppTab = 5,
  kWebWindow = 6,
  kWebTab = 7,
  // kMacOs = 8, // Removed.
  kPluginVm = 9,
  kStandaloneBrowser = 10,
  kRemote = 11,
  kBorealis = 12,
  kSystemWeb = 13,
  kChromeBrowser = 14,
  // Deprecated. Replaced by kStandaloneBrowserChromeAppWindow and
  // kStandaloneBrowserChromeAppTab.
  kStandaloneBrowserChromeApp = 15,
  kExtension = 16,
  kStandaloneBrowserExtension = 17,
  kStandaloneBrowserChromeAppWindow = 18,
  kStandaloneBrowserChromeAppTab = 19,
  kStandaloneBrowserWebAppWindow = 20,
  kStandaloneBrowserWebAppTab = 21,
  kBruschetta = 22,

  // Add any new values above this one, and update kMaxValue to the highest
  // enumerator value.
  kMaxValue = kBruschetta,
};

extern const base::TimeDelta kMinDuration;
extern const base::TimeDelta kMaxUsageDuration;
extern const int kDurationBuckets;
extern const int kUsageTimeBuckets;

constexpr char kArcHistogramName[] = "Arc";
constexpr char kBuiltInHistogramName[] = "BuiltIn";
constexpr char kCrostiniHistogramName[] = "Crostini";
constexpr char kChromeAppHistogramName[] = "ChromeApp";
constexpr char kWebAppHistogramName[] = "WebApp";
constexpr char kPluginVmHistogramName[] = "PluginVm";
constexpr char kStandaloneBrowserHistogramName[] = "StandaloneBrowser";
constexpr char kRemoteHistogramName[] = "RemoteApp";
constexpr char kBorealisHistogramName[] = "Borealis";
constexpr char kSystemWebAppHistogramName[] = "SystemWebApp";
constexpr char kChromeBrowserHistogramName[] = "ChromeBrowser";
constexpr char kStandaloneBrowserChromeAppHistogramName[] =
    "StandaloneBrowserChromeApp";
constexpr char kExtensionHistogramName[] = "Extension";
constexpr char kStandaloneBrowserExtensionHistogramName[] =
    "StandaloneBrowserExtension";
constexpr char kStandaloneBrowserChromeAppWindowHistogramName[] =
    "StandaloneBrowserChromeAppWindow";
constexpr char kStandaloneBrowserChromeAppTabHistogramName[] =
    "StandaloneBrowserChromeAppTab";
constexpr char kStandaloneBrowserWebAppHistogramName[] =
    "StandaloneBrowserWebApp";
constexpr char kStandaloneBrowserWebAppWindowHistogramName[] =
    "StandaloneBrowserWebAppWindow";
constexpr char kStandaloneBrowserWebAppTabHistogramName[] =
    "StandaloneBrowserWebAppTab";
constexpr char kBruschettaHistogramName[] = "Bruschetta";

// Determines what app type a web app should be logged as based on its launch
// container and app id. In particular, web apps in tabs are logged as part of
// Chrome browser.
AppTypeName GetAppTypeNameForWebApp(Profile* profile,
                                    const std::string& app_id,
                                    apps::LaunchContainer container);

// Determines what app type a chrome app in Lacros should be logged as based on
// its launch container and app id. In particular, chrome apps in Lacros tabs
// are logged as part of Lacros browser.
AppTypeName GetAppTypeNameForStandaloneBrowserChromeApp(
    Profile* profile,
    const std::string& app_id,
    apps::LaunchContainer container);

// Returns false if |window| is a Chrome app window or a standalone web app
// window. Otherwise, return true.
bool IsAshBrowserWindow(aura::Window* window);

// Returns true if `window` is a lacros browser window. Otherwise, returns false
// for non Lacros windows, or Lacros standalone app windows.
bool IsLacrosBrowserWindow(Profile* profile, aura::Window* window);

// Returns true if `window` is a lacros window. Otherwise, returns false.
bool IsLacrosWindow(aura::Window* window);

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
