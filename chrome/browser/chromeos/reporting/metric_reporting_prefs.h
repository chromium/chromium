// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_REPORTING_METRIC_REPORTING_PREFS_H_
#define CHROME_BROWSER_CHROMEOS_REPORTING_METRIC_REPORTING_PREFS_H_

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace reporting {

// A list pref that specifies allowlisted URLs for website activity event
// reporting.
constexpr char kReportWebsiteActivityAllowlist[] =
    "reporting.report_website_activity_allowlist";

// A list pref that specifies allowlisted URLs for website telemetry reporting.
constexpr char kReportWebsiteTelemetryAllowlist[] =
    "reporting.report_website_telemetry_allowlist";

// A list pref that controls website telemetry data types being reported.
constexpr char kReportWebsiteTelemetry[] = "reporting.report_website_telemetry";

// An integer pref that controls the collection frequency of website telemetry
// data.
constexpr char kReportWebsiteTelemetryCollectionRateMs[] =
    "reporting.report_website_telemetry_collection_rate_ms";

void RegisterProfilePrefs(::user_prefs::PrefRegistrySyncable* registry);

}  // namespace reporting

#endif  // CHROME_BROWSER_CHROMEOS_REPORTING_METRIC_REPORTING_PREFS_H_
