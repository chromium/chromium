// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_METRIC_REPORTING_PREFS_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_METRIC_REPORTING_PREFS_H_

namespace user_prefs {
class PrefRegistrySyncable;
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

void RegisterProfilePrefs(::user_prefs::PrefRegistrySyncable* registry);

}  // namespace ash::reporting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_METRIC_REPORTING_PREFS_H_
