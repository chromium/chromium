// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/apps/app_events_observer.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string_view>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/observer_list_types.h"
#include "base/sequence_checker.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_service_proxy_ash.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/apps/app_metric_reporting_utils.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/apps/app_platform_metrics_retriever.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/metric_reporting_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/reporting/metrics/metric_event_observer.h"
#include "components/reporting/metrics/reporting_settings.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/features.h"
#include "components/services/app_service/public/protos/app_types.pb.h"

namespace reporting {
namespace {

// Derives disk space consumption for the specified list, assuming the size of
// each individual entity in the list is the same.
size_t GetDiskConsumptionForList(const base::Value::List& list) {
  if (list.empty()) {
    return 0;
  }
  return list.size() * sizeof(list.front());
}
}  // namespace

// static
std::unique_ptr<AppEventsObserver> AppEventsObserver::CreateForProfile(
    Profile* profile,
    const ReportingSettings* reporting_settings) {
  CHECK(profile);
  const auto profile_weak_ptr = profile->GetWeakPtr();
  auto app_platform_metrics_retriever =
      std::make_unique<AppPlatformMetricsRetriever>(profile_weak_ptr);
  return base::WrapUnique(new AppEventsObserver(
      profile_weak_ptr, std::move(app_platform_metrics_retriever),
      reporting_settings));
}

// static
std::unique_ptr<AppEventsObserver> AppEventsObserver::CreateForTest(
    Profile* profile,
    std::unique_ptr<AppPlatformMetricsRetriever> app_platform_metrics_retriever,
    const ReportingSettings* reporting_settings) {
  CHECK(profile);
  return base::WrapUnique(new AppEventsObserver(
      profile->GetWeakPtr(), std::move(app_platform_metrics_retriever),
      reporting_settings));
}

AppEventsObserver::AppInstallTracker::AppInstallTracker(
    base::WeakPtr<Profile> profile)
    : profile_(profile) {}

AppEventsObserver::AppInstallTracker::~AppInstallTracker() = default;

void AppEventsObserver::AppInstallTracker::Add(std::string_view app_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!profile_) {
    // Profile destroyed. Skip.
    return;
  }
  CHECK(!Contains(app_id)) << "App already being tracked";
  ScopedListPrefUpdate apps_installed_pref(profile_->GetPrefs(),
                                           ::ash::reporting::kAppsInstalled);
  apps_installed_pref->Append(app_id);
  base::UmaHistogramCounts1M(
      kDiskConsumptionMetricsName,
      GetDiskConsumptionForList(apps_installed_pref.Get()));
}

void AppEventsObserver::AppInstallTracker::Remove(std::string_view app_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!profile_) {
    // Profile destroyed. Skip.
    return;
  }
  CHECK(Contains(app_id)) << "App not being tracked";
  ScopedListPrefUpdate apps_installed_pref(profile_->GetPrefs(),
                                           ::ash::reporting::kAppsInstalled);
  apps_installed_pref->EraseValue(base::Value(app_id));
  base::UmaHistogramCounts1M(
      kDiskConsumptionMetricsName,
      GetDiskConsumptionForList(apps_installed_pref.Get()));
}

bool AppEventsObserver::AppInstallTracker::Contains(
    std::string_view app_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(profile_);
  return base::Contains(
      profile_->GetPrefs()->GetList(::ash::reporting::kAppsInstalled), app_id);
}

AppEventsObserver::AppEventsObserver(
    base::WeakPtr<Profile> profile,
    std::unique_ptr<AppPlatformMetricsRetriever> app_platform_metrics_retriever,
    const ReportingSettings* reporting_settings)
    : profile_(profile),
      app_platform_metrics_retriever_(
          std::move(app_platform_metrics_retriever)),
      reporting_settings_(reporting_settings) {
  app_install_tracker_ = std::make_unique<AppInstallTracker>(profile);

  CHECK(app_platform_metrics_retriever_);
  app_platform_metrics_retriever_->GetAppPlatformMetrics(base::BindOnce(
      &AppEventsObserver::InitEventObserver, weak_ptr_factory_.GetWeakPtr()));
}

AppEventsObserver::~AppEventsObserver() = default;

void AppEventsObserver::SetOnEventObservedCallback(
    MetricRepeatingCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  on_metric_observed_ = std::move(callback);
}

void AppEventsObserver::SetReportingEnabled(bool is_enabled) {
  // Do nothing. We retrieve the reporting setting and validate the app type is
  // allowed before we report observed events.
}

void AppEventsObserver::InitEventObserver(
    ::apps::AppPlatformMetrics* app_platform_metrics) {
  // Runs on the same sequence as the `AppPlatformMetricsRetriever` because they
  // both use the UI thread.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!app_platform_metrics) {
    // This can happen if the `AppPlatformMetrics` component initialization
    // failed (for example, component was destructed). We just abort
    // initialization of the event observer when this happens.
    return;
  }
  observer_.Observe(app_platform_metrics);
}

void AppEventsObserver::OnAppInstalled(const std::string& app_id,
                                       ::apps::AppType app_type,
                                       ::apps::InstallSource app_install_source,
                                       ::apps::InstallReason app_install_reason,
                                       ::apps::InstallTime app_install_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(reporting_settings_);
  if (!profile_) {
    // Either the profile was destroyed. Skip.
    return;
  }

  // The app was already installed (likely in a prior session). Skip
  if (app_install_tracker_->Contains(app_id)) {
    return;
  }

  // Track app install to prevent future install event reports.
  app_install_tracker_->Add(app_id);

  if (!::ash::reporting::IsAppTypeAllowed(
          app_type, reporting_settings_.get(),
          ::ash::reporting::kReportAppInventory)) {
    return;
  }

  MetricData metric_data;
  metric_data.mutable_event_data()->set_type(MetricEventType::APP_INSTALLED);

  auto* const app_install_data = metric_data.mutable_telemetry_data()
                                     ->mutable_app_telemetry()
                                     ->mutable_app_install_data();
  auto public_app_id = app_id;
  if (const std::optional<std::string> app_publisher_id =
          GetPublisherIdForApp(app_id, profile_.get());
      app_publisher_id.has_value()) {
    public_app_id = app_publisher_id.value();
  }
  app_install_data->set_app_id(public_app_id);
  app_install_data->set_app_type(
      ::apps::ConvertAppTypeToProtoApplicationType(app_type));
  app_install_data->set_app_install_source(
      ::apps::ConvertInstallSourceToProtoApplicationInstallSource(
          app_install_source));
  app_install_data->set_app_install_reason(
      ::apps::ConvertInstallReasonToProtoApplicationInstallReason(
          app_install_reason));
  app_install_data->set_app_install_time(
      ::apps::ConvertInstallTimeToProtoApplicationInstallTime(
          app_install_time));

  on_metric_observed_.Run(std::move(metric_data));
}

void AppEventsObserver::OnAppLaunched(const std::string& app_id,
                                      ::apps::AppType app_type,
                                      ::apps::LaunchSource app_launch_source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(reporting_settings_);
  if (!profile_ || !::ash::reporting::IsAppTypeAllowed(
                       app_type, reporting_settings_.get(),
                       ::ash::reporting::kReportAppInventory)) {
    return;
  }

  MetricData metric_data;
  metric_data.mutable_event_data()->set_type(MetricEventType::APP_LAUNCHED);

  auto* const app_launch_data = metric_data.mutable_telemetry_data()
                                    ->mutable_app_telemetry()
                                    ->mutable_app_launch_data();
  auto public_app_id = app_id;
  if (const std::optional<std::string> app_publisher_id =
          GetPublisherIdForApp(app_id, profile_.get());
      app_publisher_id.has_value()) {
    public_app_id = app_publisher_id.value();
  }
  app_launch_data->set_app_id(public_app_id);
  app_launch_data->set_app_type(
      ::apps::ConvertAppTypeToProtoApplicationType(app_type));
  app_launch_data->set_app_launch_source(
      ::apps::ConvertLaunchSourceToProtoApplicationLaunchSource(
          app_launch_source));

  on_metric_observed_.Run(std::move(metric_data));
}

void AppEventsObserver::OnAppUninstalled(
    const std::string& app_id,
    ::apps::AppType app_type,
    ::apps::UninstallSource app_uninstall_source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(reporting_settings_);
  if (!profile_) {
    // Profile destroyed. Return.
    return;
  }

  if (app_install_tracker_ && app_install_tracker_->Contains(app_id)) {
    // Stop tracking app install if it is being tracked.
    app_install_tracker_->Remove(app_id);
  }
  if (!::ash::reporting::IsAppTypeAllowed(
          app_type, reporting_settings_.get(),
          ::ash::reporting::kReportAppInventory)) {
    return;
  }

  MetricData metric_data;
  metric_data.mutable_event_data()->set_type(MetricEventType::APP_UNINSTALLED);

  auto* const app_uninstall_data = metric_data.mutable_telemetry_data()
                                       ->mutable_app_telemetry()
                                       ->mutable_app_uninstall_data();
  auto public_app_id = app_id;
  if (const std::optional<std::string> app_publisher_id =
          GetPublisherIdForApp(app_id, profile_.get());
      app_publisher_id.has_value()) {
    public_app_id = app_publisher_id.value();
  }
  app_uninstall_data->set_app_id(public_app_id);
  app_uninstall_data->set_app_type(
      ::apps::ConvertAppTypeToProtoApplicationType(app_type));
  app_uninstall_data->set_app_uninstall_source(
      ::apps::ConvertUninstallSourceToProtoApplicationUninstallSource(
          app_uninstall_source));

  on_metric_observed_.Run(std::move(metric_data));
}

void AppEventsObserver::OnAppPlatformMetricsDestroyed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observer_.Reset();
}

}  // namespace reporting
