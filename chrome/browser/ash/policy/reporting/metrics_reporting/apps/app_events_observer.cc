// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/apps/app_events_observer.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/observer_list_types.h"
#include "base/sequence_checker.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/apps/app_platform_metrics_retriever.h"
#include "chrome/browser/profiles/profile.h"
#include "components/reporting/metrics/metric_event_observer.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/protos/app_types.pb.h"

namespace reporting {

// static
std::unique_ptr<AppEventsObserver> AppEventsObserver::CreateForProfile(
    Profile* profile) {
  DCHECK(profile);
  auto app_platform_metrics_retriever =
      std::make_unique<AppPlatformMetricsRetriever>(profile->GetWeakPtr());
  return base::WrapUnique(
      new AppEventsObserver(std::move(app_platform_metrics_retriever)));
}

// static
std::unique_ptr<AppEventsObserver> AppEventsObserver::CreateForTest(
    std::unique_ptr<AppPlatformMetricsRetriever>
        app_platform_metrics_retriever) {
  return base::WrapUnique(
      new AppEventsObserver(std::move(app_platform_metrics_retriever)));
}

AppEventsObserver::AppEventsObserver(
    std::unique_ptr<AppPlatformMetricsRetriever> app_platform_metrics_retriever)
    : app_platform_metrics_retriever_(
          std::move(app_platform_metrics_retriever)) {
  DCHECK(app_platform_metrics_retriever_);
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_enabled_ = is_enabled;
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
  if (!is_enabled_) {
    return;
  }

  MetricData metric_data;
  metric_data.mutable_event_data()->set_type(MetricEventType::APP_INSTALLED);

  auto* const app_install_data = metric_data.mutable_telemetry_data()
                                     ->mutable_app_telemetry()
                                     ->mutable_app_install_data();
  app_install_data->set_app_id(app_id);
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
  if (!is_enabled_) {
    return;
  }

  MetricData metric_data;
  metric_data.mutable_event_data()->set_type(MetricEventType::APP_LAUNCHED);

  auto* const app_launch_data = metric_data.mutable_telemetry_data()
                                    ->mutable_app_telemetry()
                                    ->mutable_app_launch_data();
  app_launch_data->set_app_id(app_id);
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
  if (!is_enabled_) {
    return;
  }

  MetricData metric_data;
  metric_data.mutable_event_data()->set_type(MetricEventType::APP_UNINSTALLED);

  auto* const app_uninstall_data = metric_data.mutable_telemetry_data()
                                       ->mutable_app_telemetry()
                                       ->mutable_app_uninstall_data();
  app_uninstall_data->set_app_id(app_id);
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
