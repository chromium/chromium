// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/apps/app_usage_observer.h"

#include <memory>
#include <optional>

#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/app_service_proxy_ash.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/apps/app_metric_reporting_utils.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/apps/app_platform_metrics_retriever.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/metric_reporting_prefs.h"
#include "chrome/browser/chromeos/reporting/metric_default_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/protos/app_types.pb.h"
#include "content/public/browser/browser_thread.h"

namespace reporting {

// static
std::unique_ptr<AppUsageObserver> AppUsageObserver::Create(
    Profile* profile,
    const ReportingSettings* reporting_settings) {
  CHECK(profile);
  auto app_platform_metrics_retriever =
      std::make_unique<AppPlatformMetricsRetriever>(profile->GetWeakPtr());
  return base::WrapUnique(
      new AppUsageObserver(profile->GetWeakPtr(), reporting_settings,
                           std::move(app_platform_metrics_retriever)));
}

// static
std::unique_ptr<AppUsageObserver> AppUsageObserver::CreateForTest(
    Profile* profile,
    const ReportingSettings* reporting_settings,
    std::unique_ptr<AppPlatformMetricsRetriever>
        app_platform_metrics_retriever) {
  CHECK(profile);
  return base::WrapUnique(
      new AppUsageObserver(profile->GetWeakPtr(), reporting_settings,
                           std::move(app_platform_metrics_retriever)));
}

AppUsageObserver::AppUsageObserver(
    base::WeakPtr<Profile> profile,
    const ReportingSettings* reporting_settings,
    std::unique_ptr<AppPlatformMetricsRetriever> app_platform_metrics_retriever)
    : profile_(profile),
      reporting_settings_(reporting_settings),
      app_platform_metrics_retriever_(
          std::move(app_platform_metrics_retriever)) {
  CHECK(app_platform_metrics_retriever_);
  app_platform_metrics_retriever_->GetAppPlatformMetrics(base::BindOnce(
      &AppUsageObserver::InitUsageObserver, weak_ptr_factory_.GetWeakPtr()));
}

AppUsageObserver::~AppUsageObserver() = default;

void AppUsageObserver::InitUsageObserver(
    ::apps::AppPlatformMetrics* app_platform_metrics) {
  if (!app_platform_metrics) {
    // This can happen if the `AppPlatformMetrics` component initialization
    // failed (for example, component was destructed). We just abort
    // initialization of the usage observer when this happens.
    return;
  }
  observer_.Observe(app_platform_metrics);
}

void AppUsageObserver::OnAppUsage(const std::string& app_id,
                                  ::apps::AppType app_type,
                                  const base::UnguessableToken& instance_id,
                                  base::TimeDelta running_time) {
  CHECK(reporting_settings_);
  if (!profile_ ||
      !::ash::reporting::IsAppTypeAllowed(app_type, reporting_settings_.get(),
                                          ::ash::reporting::kReportAppUsage)) {
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

void AppUsageObserver::OnAppPlatformMetricsDestroyed() {
  observer_.Reset();
}

void AppUsageObserver::CreateOrUpdateAppUsageEntry(
    const std::string& app_id,
    ::apps::AppType app_type,
    const base::UnguessableToken& instance_id,
    const base::TimeDelta& running_time) {
  DCHECK_CURRENTLY_ON(::content::BrowserThread::UI);
  CHECK(profile_);
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
    MaybeSetAppPublisherId(usage_time);
    usage_dict_pref->SetByDottedPath(instance_id_string,
                                     usage_time.ConvertToDict());
    return;
  }

  // Aggregate and update just the running time otherwise.
  ::apps::AppPlatformMetrics::UsageTime usage_time(
      *usage_dict_pref->FindByDottedPath(instance_id_string));
  usage_time.reporting_usage_time += running_time;
  MaybeSetAppPublisherId(usage_time);
  usage_dict_pref->SetByDottedPath(instance_id_string,
                                   usage_time.ConvertToDict());
}

void AppUsageObserver::MaybeSetAppPublisherId(
    ::apps::AppPlatformMetrics::UsageTime& usage_time) {
  DCHECK_CURRENTLY_ON(::content::BrowserThread::UI);
  CHECK(profile_);
  if (!usage_time.app_publisher_id.empty()) {
    // We are already tracking the app publisher id.
    return;
  }
  if (const std::optional<std::string> app_publisher_id =
          GetPublisherIdForApp(usage_time.app_id, profile_.get());
      app_publisher_id.has_value()) {
    usage_time.app_publisher_id = app_publisher_id.value();
  }
}

}  // namespace reporting
