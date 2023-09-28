// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_REPORTING_METRIC_REPORTING_PREFS_H_
#define CHROME_BROWSER_CHROMEOS_REPORTING_METRIC_REPORTING_PREFS_H_

#include <string>

#include "components/reporting/metrics/reporting_settings.h"
#include "url/gurl.h"

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

// A dictionary pref that tracks foreground website usage for URLs with the
// current user profile.
constexpr char kWebsiteUsage[] = "reporting.website_usage";

// Website telemetry types tracked by the `ReportWebsiteTelemetry` policy.
constexpr char kWebsiteTelemetryUsageType[] = "usage";

void RegisterProfilePrefs(::user_prefs::PrefRegistrySyncable* registry);

// Retrieves the corresponding website metric reporting policy and returns true
// if the specified website URL is supported and matches any of the patterns in
// the retrieved allowlist. False otherwise.
bool IsWebsiteUrlAllowlisted(const GURL& url,
                             const ReportingSettings* reporting_settings,
                             const std::string& policy_setting);
}  // namespace reporting

#endif  // CHROME_BROWSER_CHROMEOS_REPORTING_METRIC_REPORTING_PREFS_H_
