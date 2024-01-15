// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/reporting/websites/website_usage_observer.h"

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/json/values_util.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/metrics/website_metrics.h"
#include "chrome/browser/chromeos/reporting/metric_default_utils.h"
#include "chrome/browser/chromeos/reporting/metric_reporting_prefs.h"
#include "chrome/browser/chromeos/reporting/websites/website_metrics_retriever_interface.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/reporting/metrics/reporting_settings.h"
#include "url/gurl.h"

namespace reporting {

WebsiteUsageObserver::WebsiteUsageObserver(
    base::WeakPtr<Profile> profile,
    const ReportingSettings* reporting_settings,
    std::unique_ptr<WebsiteMetricsRetrieverInterface> website_metrics_retriever)
    : profile_(profile),
      reporting_settings_(reporting_settings),
      website_metrics_retriever_(std::move(website_metrics_retriever)) {
  CHECK(website_metrics_retriever_);
  website_metrics_retriever_->GetWebsiteMetrics(
      base::BindOnce(&WebsiteUsageObserver::InitUsageObserver,
                     weak_ptr_factory_.GetWeakPtr()));
}

WebsiteUsageObserver::~WebsiteUsageObserver() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void WebsiteUsageObserver::OnUrlUsage(const GURL& url,
                                      base::TimeDelta running_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(reporting_settings_);
  if (!profile_ || !IsWebsiteUsageTelemetryEnabled() ||
      !IsWebsiteUrlAllowlisted(url, reporting_settings_,
                               kReportWebsiteTelemetryAllowlist)) {
    return;
  }
  if (running_time < metrics::kMinimumWebsiteUsageTime) {
    // Skip if there is no usage in millisecond granularity. Needed because we
    // track website usage in milliseconds while `base::TimeDelta` internals use
    // microsecond granularity.
    return;
  }
  if (!profile_->GetPrefs()->HasPrefPath(kWebsiteUsage)) {
    // No data in the pref store, so we create an empty dictionary for now.
    profile_->GetPrefs()->SetDict(kWebsiteUsage, base::Value::Dict());
  }

  CreateOrUpdateWebsiteUsageEntry(url, running_time);
}

void WebsiteUsageObserver::InitUsageObserver(
    ::apps::WebsiteMetrics* website_metrics) {
  if (!website_metrics) {
    // This can happen if the `WebsiteMetrics` component initialization failed
    // (for example, component was destructed). We just abort initialization of
    // the usage observer when this happens.
    return;
  }
  observer_.Observe(website_metrics);
}

void WebsiteUsageObserver::OnWebsiteMetricsDestroyed() {
  observer_.Reset();
}

void WebsiteUsageObserver::CreateOrUpdateWebsiteUsageEntry(
    const GURL& url,
    const base::TimeDelta& running_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(profile_);
  ScopedDictPrefUpdate usage_dict_pref(profile_->GetPrefs(), kWebsiteUsage);
  const std::string& url_string = url.spec();
  if (!usage_dict_pref->contains(url_string)) {
    // Create a new entry in the pref store for given URL if one does not exist
    // already.
    usage_dict_pref->Set(url_string, base::TimeDeltaToValue(running_time));
    return;
  }

  // Aggregate and update the running time otherwise.
  const std::optional<const base::TimeDelta> saved_running_time_value =
      base::ValueToTimeDelta(usage_dict_pref->Find(url_string));
  if (saved_running_time_value.has_value()) {
    usage_dict_pref->Set(
        url_string, base::TimeDeltaToValue(saved_running_time_value.value() +
                                           running_time));
  }
}

bool WebsiteUsageObserver::IsWebsiteUsageTelemetryEnabled() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(reporting_settings_);
  const base::Value::List* allowed_telemetry_types;
  if (!reporting_settings_->GetList(kReportWebsiteTelemetry,
                                    &allowed_telemetry_types)) {
    // Policy likely unset. Disallow website usage telemetry tracking in any
    // case.
    return false;
  }

  // `allowed_telemetry_types` is not expected to change as we iterate and check
  // for `usage` telemetry type below since this is triggered on the main
  // (owning) sequence.
  CHECK(allowed_telemetry_types);
  auto it = std::find_if(
      allowed_telemetry_types->begin(), allowed_telemetry_types->end(),
      [](const base::Value& telemetry_type_value) {
        return telemetry_type_value.GetString() == kWebsiteTelemetryUsageType;
      });
  const auto is_usage_telemetry_reporting_enabled =
      (it != allowed_telemetry_types->end());
  return is_usage_telemetry_reporting_enabled;
}

}  // namespace reporting
