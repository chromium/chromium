// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/apps/app_usage_collector.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/apps/app_platform_metrics_retriever.h"
#include "chrome/browser/chromeos/reporting/metric_default_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/protos/app_types.pb.h"
#include "content/public/browser/browser_thread.h"

namespace reporting {

// static
std::unique_ptr<AppUsageCollector> AppUsageCollector::Create(
    Profile* profile,
    const ReportingSettings* reporting_settings) {
  DCHECK(profile);
  auto app_platform_metrics_retriever =
      std::make_unique<AppPlatformMetricsRetriever>(profile->GetWeakPtr());
  return base::WrapUnique(
      new AppUsageCollector(profile->GetWeakPtr(), reporting_settings,
                            std::move(app_platform_metrics_retriever)));
}

// static
std::unique_ptr<AppUsageCollector> AppUsageCollector::CreateForTest(
    Profile* profile,
    const ReportingSettings* reporting_settings,
    std::unique_ptr<AppPlatformMetricsRetriever>
        app_platform_metrics_retriever) {
  DCHECK(profile);
  return base::WrapUnique(
      new AppUsageCollector(profile->GetWeakPtr(), reporting_settings,
                            std::move(app_platform_metrics_retriever)));
}

AppUsageCollector::AppUsageCollector(
    base::WeakPtr<Profile> profile,
    const ReportingSettings* reporting_settings,
    std::unique_ptr<AppPlatformMetricsRetriever> app_platform_metrics_retriever)
    : profile_(profile),
      reporting_settings_(reporting_settings),
      app_platform_metrics_retriever_(
          std::move(app_platform_metrics_retriever)) {
  DCHECK(app_platform_metrics_retriever_);
  app_platform_metrics_retriever_->GetAppPlatformMetrics(base::BindOnce(
      &AppUsageCollector::InitUsageCollector, weak_ptr_factory_.GetWeakPtr()));
}

AppUsageCollector::~AppUsageCollector() = default;

void AppUsageCollector::InitUsageCollector(
    ::apps::AppPlatformMetrics* app_platform_metrics) {
  if (!app_platform_metrics) {
    // This can happen if the `AppPlatformMetrics` component initialization
    // failed (for example, component was destructed). We just abort
    // initialization of the usage collector when this happens.
    return;
  }
  observer_.Observe(app_platform_metrics);
}

void AppUsageCollector::OnAppUsage(const std::string& app_id,
                                   ::apps::AppType app_type,
                                   const base::UnguessableToken& instance_id,
                                   base::TimeDelta running_time) {
  DCHECK(reporting_settings_);
  auto is_enabled = metrics::kReportDeviceAppInfoDefaultValue;
  reporting_settings_->GetBoolean(::ash::kReportDeviceAppInfo, &is_enabled);
  if (!profile_ || !is_enabled) {
    return;
  }

  if (running_time < metrics::kMinimumAppUsageTime) {
    // Skip if there is no usage in millisecond granularity. Needed because we
    // track app usage in milliseconds while `base::TimeDelta` internals use
    // microsecond granularity.
    return;
  }

  if (!profile_->GetPrefs()->HasPrefPath(::apps::kAppUsageTime)) {
    // No data in the pref store, so we create an empty dictionary for now.
    profile_->GetPrefs()->SetDict(::apps::kAppUsageTime, base::Value::Dict());
  }

  CreateOrUpdateAppUsageEntry(app_id, app_type, instance_id, running_time);
}

void AppUsageCollector::OnAppPlatformMetricsDestroyed() {
  observer_.Reset();
}

void AppUsageCollector::CreateOrUpdateAppUsageEntry(
    const std::string& app_id,
    ::apps::AppType app_type,
    const base::UnguessableToken& instance_id,
    const base::TimeDelta& running_time) {
  DCHECK_CURRENTLY_ON(::content::BrowserThread::UI);
  DCHECK(profile_);
  ScopedDictPrefUpdate usage_dict_pref(profile_->GetPrefs(),
                                       ::apps::kAppUsageTime);
  const auto& instance_id_string = instance_id.ToString();
  if (!usage_dict_pref->contains(instance_id_string)) {
    // Create a new entry in the pref store with the specified running time.
    ::apps::AppPlatformMetrics::UsageTime usage_time;
    usage_time.app_id = app_id;
    usage_time.app_type_name =
        ::apps::GetAppTypeName(profile_.get(), app_type, app_id,
                               ::apps::LaunchContainer::kLaunchContainerNone);
    usage_time.reporting_usage_time = running_time;
    usage_dict_pref->SetByDottedPath(instance_id_string,
                                     usage_time.ConvertToDict());
    return;
  }

  // Aggregate and update just the running time otherwise.
  ::apps::AppPlatformMetrics::UsageTime usage_time(
      *usage_dict_pref->FindByDottedPath(instance_id_string));
  usage_time.reporting_usage_time += running_time;
  usage_dict_pref->SetByDottedPath(instance_id_string,
                                   usage_time.ConvertToDict());
}

}  // namespace reporting
