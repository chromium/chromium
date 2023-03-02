// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/apps/app_usage_collector.h"

#include "chrome/browser/apps/app_service/metrics/app_platform_metrics.h"
#include "chrome/browser/chromeos/reporting/metric_default_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/protos/app_types.pb.h"

namespace reporting {

AppUsageCollector::AppUsageCollector(
    Profile* profile,
    const ReportingSettings* reporting_settings,
    ::apps::AppPlatformMetrics* app_platform_metrics)
    : profile_(profile),
      reporting_settings_(reporting_settings),
      app_platform_metrics_(app_platform_metrics) {
  DCHECK(app_platform_metrics_);
  app_platform_metrics_->AddObserver(this);
}

AppUsageCollector::~AppUsageCollector() {
  if (IsInObserverList()) {
    app_platform_metrics_->RemoveObserver(this);
  }
}

void AppUsageCollector::OnAppUsage(const std::string& app_id,
                                   ::apps::AppType app_type,
                                   const base::UnguessableToken& instance_id,
                                   base::TimeDelta running_time) {
  DCHECK(profile_);
  DCHECK(reporting_settings_);
  auto is_enabled = metrics::kReportDeviceAppInfoDefaultValue;
  reporting_settings_->GetBoolean(::ash::kReportDeviceAppInfo, &is_enabled);
  if (!is_enabled) {
    return;
  }

  if (!profile_->GetPrefs()->HasPrefPath(::apps::kAppUsageTime)) {
    // No data in the pref store, so we create an empty dictionary for now.
    profile_->GetPrefs()->SetDict(::apps::kAppUsageTime, base::Value::Dict());
  }

  CreateOrUpdateAppUsageEntry(app_id, app_type, instance_id, running_time);
}

void AppUsageCollector::CreateOrUpdateAppUsageEntry(
    const std::string& app_id,
    ::apps::AppType app_type,
    const base::UnguessableToken& instance_id,
    const base::TimeDelta& running_time) {
  ScopedDictPrefUpdate usage_dict_pref(profile_->GetPrefs(),
                                       ::apps::kAppUsageTime);
  const auto& instance_id_string = instance_id.ToString();
  if (!usage_dict_pref->contains(instance_id_string)) {
    // Create a new entry in the pref store with the specified running time.
    ::apps::AppPlatformMetrics::UsageTime usage_time;
    usage_time.app_id = app_id;
    usage_time.app_type_name =
        ::apps::GetAppTypeName(profile_, app_type, app_id,
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
