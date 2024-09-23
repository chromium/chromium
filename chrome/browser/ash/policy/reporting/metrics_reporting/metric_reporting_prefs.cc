// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/metric_reporting_prefs.h"

#include <optional>

#include "base/containers/contains.h"
#include "base/values.h"
#include "chrome/browser/chromeos/reporting/metric_default_utils.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/reporting/metrics/reporting_settings.h"
#include "components/services/app_service/public/cpp/app_types.h"

namespace ash::reporting {

void RegisterProfilePrefs(::user_prefs::PrefRegistrySyncable* registry) {
  CHECK(registry);
  registry->RegisterListPref(kReportAppInventory);
  registry->RegisterListPref(kReportAppUsage);
  registry->RegisterIntegerPref(
      kReportAppUsageCollectionRateMs,
      ::reporting::metrics::kDefaultAppUsageTelemetryCollectionRate
          .InMilliseconds());
  registry->RegisterListPref(kAppsInstalled);
}

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  CHECK(registry);
  registry->RegisterBooleanPref(
      ::ash::kHeartbeatEnabled,
      ::reporting::metrics::kHeartbeatTelemetryDefaultValue);
}

std::optional<std::string> GetAppReportingCategoryForType(
    ::apps::AppType app_type) {
  switch (app_type) {
    case ::apps::AppType::kArc:
      return kAppCategoryAndroidApps;
    case ::apps::AppType::kBuiltIn:
    case ::apps::AppType::kSystemWeb:
      return kAppCategorySystemApps;
    case ::apps::AppType::kCrostini:
    case ::apps::AppType::kBruschetta:
      return kAppCategoryLinuxApps;
    case ::apps::AppType::kChromeApp:
    case ::apps::AppType::kRemote:
    case ::apps::AppType::kStandaloneBrowserChromeApp:
    case ::apps::AppType::kExtension:
    case ::apps::AppType::kStandaloneBrowserExtension:
      return kAppCategoryChromeAppsExtensions;
    case ::apps::AppType::kWeb:
      return kAppCategoryPWA;
    case ::apps::AppType::kStandaloneBrowser:
      return kAppCategoryBrowser;
    case ::apps::AppType::kBorealis:
      return kAppCategoryGames;
    case ::apps::AppType::kPluginVm:  // Only applies to MGS, so we skip.
    case ::apps::AppType::kUnknown:   // Invalid app type.
      return std::nullopt;
  }
}

bool IsAppTypeAllowed(::apps::AppType app_type,
                      const ::reporting::ReportingSettings* reporting_settings,
                      const std::string& policy_setting) {
  CHECK(reporting_settings);
  const base::Value::List* allowed_app_types;
  if (!reporting_settings->GetList(policy_setting, &allowed_app_types)) {
    // Policy likely unset. Disallow app usage reporting regardless of app type.
    return false;
  }
  const std::optional<std::string> app_category =
      GetAppReportingCategoryForType(app_type);
  return app_category.has_value() &&
         base::Contains(*allowed_app_types, app_category.value());
}

}  // namespace ash::reporting
