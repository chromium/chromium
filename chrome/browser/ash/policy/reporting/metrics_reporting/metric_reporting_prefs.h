// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_METRIC_REPORTING_PREFS_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_METRIC_REPORTING_PREFS_H_

#include <optional>
#include <string>

class PrefRegistrySimple;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace apps {
enum class AppType;
}

namespace reporting {
class ReportingSettings;
}

namespace ash::reporting {

// A list pref that controls app inventory event reporting for the specified app
// types.
constexpr char kReportAppInventory[] = "reporting.report_app_inventory";

// A list pref that controls app usage telemetry reporting for the specified app
// types.
constexpr char kReportAppUsage[] = "reporting.report_app_usage";

// An integer pref that controls the collection frequency of app usage
// telemetry.
constexpr char kReportAppUsageCollectionRateMs[] =
    "reporting.report_app_usage_collection_rate_ms";

// A list pref used to track installed apps for a particular user.
constexpr char kAppsInstalled[] = "reporting.apps_installed";

// Application category types tracked by the app metric reporting user policies.
constexpr char kAppCategoryAndroidApps[] = "android_apps";
constexpr char kAppCategoryBrowser[] = "browser";
constexpr char kAppCategoryChromeAppsExtensions[] =
    "chrome_apps_and_extensions";
constexpr char kAppCategoryGames[] = "games";
constexpr char kAppCategoryLinuxApps[] = "linux_apps";
constexpr char kAppCategoryPWA[] = "progressive_web_apps";
constexpr char kAppCategorySystemApps[] = "system_apps";

void RegisterProfilePrefs(::user_prefs::PrefRegistrySyncable* registry);
void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

// Gets the corresponding app metric reporting category for the specified app
// type.
std::optional<std::string> GetAppReportingCategoryForType(
    ::apps::AppType app_type);

// Retrieves the corresponding app reporting policy and returns true if the app
// type is in the retrieved allowlist. False otherwise.
bool IsAppTypeAllowed(::apps::AppType app_type,
                      const ::reporting::ReportingSettings* reporting_settings,
                      const std::string& policy_setting);

}  // namespace ash::reporting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_METRIC_REPORTING_PREFS_H_
