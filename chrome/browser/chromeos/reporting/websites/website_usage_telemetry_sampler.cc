// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/reporting/websites/website_usage_telemetry_sampler.h"

#include <optional>

#include "base/json/values_util.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/chromeos/reporting/metric_reporting_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/reporting/metrics/sampler.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

namespace reporting {

WebsiteUsageTelemetrySampler::WebsiteUsageTelemetrySampler(
    base::WeakPtr<Profile> profile)
    : profile_(profile) {}

WebsiteUsageTelemetrySampler::~WebsiteUsageTelemetrySampler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void WebsiteUsageTelemetrySampler::MaybeCollect(
    OptionalMetricCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::optional<MetricData> metric_data;
  absl::Cleanup run_callback_on_return = [this, &callback, &metric_data] {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!metric_data.has_value()) {
      std::move(callback).Run(std::move(metric_data));
      return;
    }

    // Report metric data and delete tracked website usage data from the
    // pref store.
    const auto& website_usage_data =
        metric_data->telemetry_data().website_telemetry().website_usage_data();
    CHECK(!website_usage_data.website_usage().empty());
    std::move(callback).Run(std::move(metric_data));
    DeleteWebsiteUsageDataFromPrefStore(&website_usage_data);
  };
  if (!profile_) {
    // Profile has been destructed. Return.
    return;
  }
  const PrefService* const user_prefs = profile_->GetPrefs();
  if (!user_prefs->HasPrefPath(kWebsiteUsage)) {
    // No usage data being tracked in the pref store. Return.
    return;
  }
  const base::Value::Dict& usage_dict = user_prefs->GetDict(kWebsiteUsage);
  if (usage_dict.empty()) {
    // No website usage data to report. Return.
    return;
  }

  // Parse website usage across URLs from the pref store and populate
  // website usage data.
  metric_data = std::make_optional<MetricData>();
  auto* const website_usage_data = metric_data->mutable_telemetry_data()
                                       ->mutable_website_telemetry()
                                       ->mutable_website_usage_data();
  for (auto usage_it : usage_dict) {
    const std::optional<const base::TimeDelta> saved_usage_time =
        base::ValueToTimeDelta(usage_it.second);
    CHECK(saved_usage_time.has_value());
    WebsiteUsageData::WebsiteUsage* const website_usage =
        website_usage_data->mutable_website_usage()->Add();
    website_usage->set_url(usage_it.first);
    website_usage->set_running_time_ms(
        saved_usage_time.value().InMilliseconds());
  }
}

void WebsiteUsageTelemetrySampler::DeleteWebsiteUsageDataFromPrefStore(
    const WebsiteUsageData* website_usage_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(profile_);
  ScopedDictPrefUpdate usage_dict_pref(profile_->GetPrefs(), kWebsiteUsage);
  for (const auto& website_usage : website_usage_data->website_usage()) {
    const std::string& url = website_usage.url();
    CHECK(usage_dict_pref->contains(url))
        << "Missing website usage data for URL: " << url;
    usage_dict_pref->Remove(url);
  }
}

}  // namespace reporting
