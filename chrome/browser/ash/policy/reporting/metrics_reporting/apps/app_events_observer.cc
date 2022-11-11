// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/apps/app_events_observer.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/observer_list_types.h"
#include "base/sequence_checker.h"
#include "chrome/browser/apps/app_service/app_service_proxy_ash.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics.h"
#include "components/reporting/metrics/metric_event_observer.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/protos/app_types.pb.h"

namespace reporting {

bool AppEventsObserver::Delegate::IsAppServiceAvailableForProfile(
    Profile* profile) {
  return ::apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(
      profile);
}

::apps::AppPlatformMetrics*
AppEventsObserver::Delegate::GetAppPlatformMetricsForProfile(Profile* profile) {
  return ::apps::AppServiceProxyFactory::GetForProfile(profile)
      ->AppPlatformMetrics();
}

// static
std::unique_ptr<AppEventsObserver> AppEventsObserver::CreateForProfile(
    Profile* profile) {
  auto delegate = std::make_unique<AppEventsObserver::Delegate>();
  return base::WrapUnique(new AppEventsObserver(profile, std::move(delegate)));
}

// static
std::unique_ptr<AppEventsObserver> AppEventsObserver::CreateForTest(
    Profile* profile,
    std::unique_ptr<AppEventsObserver::Delegate> delegate) {
  return base::WrapUnique(new AppEventsObserver(profile, std::move(delegate)));
}

AppEventsObserver::AppEventsObserver(
    Profile* profile,
    std::unique_ptr<AppEventsObserver::Delegate> delegate)
    : profile_(profile), delegate_(std::move(delegate)) {
  DCHECK(profile_);
  if (!delegate_->IsAppServiceAvailableForProfile(profile)) {
    // Profile cannot run apps, so we just return.
    return;
  }

  // Register instance so we can start observing app events.
  auto* const app_platform_metrics =
      delegate_->GetAppPlatformMetricsForProfile(profile);
  app_platform_metrics->AddObserver(this);
}

AppEventsObserver::~AppEventsObserver() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Unregister instance as an observer from `AppPlatformMetrics` if still
  // registered.
  if (IsInObserverList()) {
    delegate_->GetAppPlatformMetricsForProfile(profile_)->RemoveObserver(this);
  }
}

void AppEventsObserver::SetOnEventObservedCallback(
    MetricRepeatingCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  on_metric_observed_ = std::move(callback);
}

void AppEventsObserver::SetReportingEnabled(bool is_enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_enabled_ = is_enabled;
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

  AppTelemetry* const app_telemetry_data =
      metric_data.mutable_telemetry_data()->mutable_app_telemetry();
  app_telemetry_data->set_app_id(app_id);
  app_telemetry_data->set_app_type(
      ::apps::ConvertAppTypeToProtoApplicationType(app_type));

  AppInstallData* const app_install_data =
      app_telemetry_data->mutable_app_install_data();
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

  AppTelemetry* const app_telemetry_data =
      metric_data.mutable_telemetry_data()->mutable_app_telemetry();
  app_telemetry_data->set_app_id(app_id);
  app_telemetry_data->set_app_type(
      ::apps::ConvertAppTypeToProtoApplicationType(app_type));

  AppLaunchData* const app_launch_data =
      app_telemetry_data->mutable_app_launch_data();
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

  AppTelemetry* const app_telemetry_data =
      metric_data.mutable_telemetry_data()->mutable_app_telemetry();
  app_telemetry_data->set_app_id(app_id);
  app_telemetry_data->set_app_type(
      ::apps::ConvertAppTypeToProtoApplicationType(app_type));

  AppUninstallData* const app_uninstall_data =
      app_telemetry_data->mutable_app_uninstall_data();
  app_uninstall_data->set_app_uninstall_source(
      ::apps::ConvertUninstallSourceToProtoApplicationUninstallSource(
          app_uninstall_source));

  on_metric_observed_.Run(std::move(metric_data));
}

}  // namespace reporting
