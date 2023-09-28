// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/reporting/metric_reporting_prefs.h"
#include <algorithm>

#include "base/check.h"
#include "base/values.h"
#include "chrome/browser/chromeos/reporting/metric_default_utils.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/reporting/metrics/reporting_settings.h"
#include "url/gurl.h"

namespace reporting {

void RegisterProfilePrefs(::user_prefs::PrefRegistrySyncable* registry) {
  CHECK(registry);
  registry->RegisterListPref(kReportWebsiteActivityAllowlist);
  registry->RegisterListPref(kReportWebsiteTelemetryAllowlist);
  registry->RegisterListPref(kReportWebsiteTelemetry);
  registry->RegisterIntegerPref(
      kReportWebsiteTelemetryCollectionRateMs,
      ::reporting::metrics::kDefaultWebsiteTelemetryCollectionRate
          .InMilliseconds());
  registry->RegisterDictionaryPref(kWebsiteUsage);
}

bool IsWebsiteUrlAllowlisted(const GURL& url,
                             const ReportingSettings* reporting_settings,
                             const std::string& policy_setting) {
  CHECK(reporting_settings);
  if (!url.SchemeIsHTTPOrHTTPS()) {
    // Unsupported/invalid URL scheme.
    return false;
  }

  const base::Value::List* allowlisted_urls;
  if (!reporting_settings->GetList(policy_setting, &allowlisted_urls)) {
    // Policy likely unset. Disallow website metrics reporting regardless.
    return false;
  }

  CHECK(allowlisted_urls);
  auto it =
      std::find_if(allowlisted_urls->begin(), allowlisted_urls->end(),
                   [&url](const base::Value& value) {
                     const ContentSettingsPattern pattern =
                         ContentSettingsPattern::FromString(value.GetString());
                     return pattern.Matches(url);
                   });

  // Return result of the match
  const auto has_match = (it != allowlisted_urls->end());
  return has_match;
}

}  // namespace reporting
