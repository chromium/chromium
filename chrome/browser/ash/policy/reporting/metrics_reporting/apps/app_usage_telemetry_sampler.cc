// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/apps/app_usage_telemetry_sampler.h"

#include <memory>
#include <optional>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics_utils.h"
#include "chrome/browser/chromeos/reporting/metric_default_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/reporting/metrics/sampler.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace reporting {

AppUsageTelemetrySampler::AppUsageTelemetrySampler(
    base::WeakPtr<Profile> profile)
    : profile_(profile) {}

AppUsageTelemetrySampler::~AppUsageTelemetrySampler() = default;

void AppUsageTelemetrySampler::MaybeCollect(OptionalMetricCallback callback) {
  if (!::content::BrowserThread::CurrentlyOn(::content::BrowserThread::UI)) {
    ::content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&AppUsageTelemetrySampler::MaybeCollect,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }
  if (!profile_) {
    // Profile has be destructed. Return.
    std::move(callback).Run(std::nullopt);
    return;
  }

  MetricData metric_data;
  auto* const app_usage_data = metric_data.mutable_telemetry_data()
                                   ->mutable_app_telemetry()
                                   ->mutable_app_usage_data();
  const PrefService* const user_prefs = profile_->GetPrefs();
  if (!user_prefs->HasPrefPath(::apps::kAppUsageTime)) {
    // No usage data in the pref store.
    std::move(callback).Run(std::nullopt);
    return;
  }

  // Parse app instance usage from the pref store and populate `app_usage_data`.
  for (auto usage_it : user_prefs->GetDict(::apps::kAppUsageTime)) {
    ::apps::AppPlatformMetrics::UsageTime usage_time(usage_it.second);
    if (usage_time.reporting_usage_time < metrics::kMinimumAppUsageTime) {
      // No reporting usage tracked by the `AppUsageObserver` since it was last
      // enabled, so we skip. The `AppPlatformMetrics` component will
      // subsequently delete this entry once it reports its UKM snapshot.
      CHECK(usage_time.reporting_usage_time.is_zero());
      continue;
    }

    ::apps::AppType app_type =
        ::apps::GetAppType(profile_.get(), usage_time.app_id);
    std::string public_app_id = usage_time.app_id;
    if (!usage_time.app_publisher_id.empty()) {
      // Use publisher id if there is one set. Mostly needed for android apps,
      // web apps, etc. because they include public app identifiers.
      public_app_id = usage_time.app_publisher_id;
    }

    AppUsageData::AppUsage* const app_usage =
        app_usage_data->mutable_app_usage()->Add();
    app_usage->set_app_instance_id(usage_it.first);
    app_usage->set_app_id(public_app_id);
    app_usage->set_app_type(
        ::apps::ConvertAppTypeToProtoApplicationType(app_type));
    app_usage->set_running_time_ms(
        usage_time.reporting_usage_time.InMilliseconds());
  }

  if (app_usage_data->app_usage().empty()) {
    // No app instance usage to report.
    std::move(callback).Run(std::nullopt);
    return;
  }

  std::move(callback).Run(metric_data);
  ResetAppUsageDataInPrefStore(app_usage_data);
}

void AppUsageTelemetrySampler::ResetAppUsageDataInPrefStore(
    const AppUsageData* app_usage_data) {
  DCHECK_CURRENTLY_ON(::content::BrowserThread::UI);
  CHECK(profile_);
  ScopedDictPrefUpdate usage_dict_pref(profile_->GetPrefs(),
                                       ::apps::kAppUsageTime);
  for (const auto& usage_info : app_usage_data->app_usage()) {
    const std::string& instance_id = usage_info.app_instance_id();
    CHECK(usage_dict_pref->contains(instance_id))
        << "Missing app usage data for instance: " << instance_id;

    // Reduce usage time tracked in the pref store based on the data that was
    // reported.
    const auto running_time = base::Milliseconds(usage_info.running_time_ms());
    ::apps::AppPlatformMetrics::UsageTime usage_time(
        *usage_dict_pref->FindByDottedPath(instance_id));
    usage_time.reporting_usage_time -= running_time;
    if (usage_time.reporting_usage_time < metrics::kMinimumAppUsageTime) {
      // Microsecond usage surplus which could interfere with record deletion.
      // We reset this so it can be marked for deletion should there be no usage
      // following this.
      usage_time.reporting_usage_time = base::TimeDelta();
    }
    usage_dict_pref->SetByDottedPath(instance_id, usage_time.ConvertToDict());
  }
}

}  // namespace reporting
