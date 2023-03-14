// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_APPS_APP_EVENTS_OBSERVER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_APPS_APP_EVENTS_OBSERVER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/apps/app_platform_metrics_retriever.h"
#include "chrome/browser/profiles/profile.h"
#include "components/reporting/metrics/metric_event_observer.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_types.h"

namespace reporting {

// Event observer that listens to relevant app related events supported by the
// `AppPlatformMetrics` component for reporting purposes.
class AppEventsObserver : public MetricEventObserver,
                          public ::apps::AppPlatformMetrics::Observer {
 public:
  // Static helper that instantiates the `AppEventsObserver` for the given
  // profile.
  static std::unique_ptr<AppEventsObserver> CreateForProfile(Profile* profile);

  // Static test helper that instantiates the `AppEventsObserver` using the
  // specified `AppPlatformMetricsRetriever`.
  static std::unique_ptr<AppEventsObserver> CreateForTest(
      std::unique_ptr<AppPlatformMetricsRetriever>
          app_platform_metrics_retriever);

  AppEventsObserver(const AppEventsObserver& other) = delete;
  AppEventsObserver& operator=(const AppEventsObserver& other) = delete;
  ~AppEventsObserver() override;

  // MetricEventObserver:
  void SetOnEventObservedCallback(MetricRepeatingCallback callback) override;

  // MetricEventObserver:
  void SetReportingEnabled(bool is_enabled) override;

 private:
  explicit AppEventsObserver(std::unique_ptr<AppPlatformMetricsRetriever>
                                 app_platform_metrics_retriever);

  // Initializes events observer and starts observing app events tracked by the
  // `AppPlatformMetrics` component (if initialized).
  void InitEventObserver(::apps::AppPlatformMetrics* app_platform_metrics);

  // ::apps::AppPlatformMetrics::Observer:
  void OnAppInstalled(const std::string& app_id,
                      ::apps::AppType app_type,
                      ::apps::InstallSource app_install_source,
                      ::apps::InstallReason app_install_reason,
                      ::apps::InstallTime app_install_time) override;

  // ::apps::AppPlatformMetrics::Observer:
  void OnAppLaunched(const std::string& app_id,
                     ::apps::AppType app_type,
                     ::apps::LaunchSource app_launch_source) override;

  // ::apps::AppPlatformMetrics::Observer:
  void OnAppUninstalled(const std::string& app_id,
                        ::apps::AppType app_type,
                        ::apps::UninstallSource app_uninstall_source) override;

  // ::apps::AppPlatformMetrics::Observer:
  void OnAppPlatformMetricsDestroyed() override;

  SEQUENCE_CHECKER(sequence_checker_);

  // Retriever that retrieves the `AppPlatformMetrics` component so the
  // `AppEventsObserver` can start observing app events.
  const std::unique_ptr<AppPlatformMetricsRetriever>
      app_platform_metrics_retriever_;

  // Observer for tracking app events. Will be reset if the `AppPlatformMetrics`
  // component gets destructed before the event observer.
  base::ScopedObservation<::apps::AppPlatformMetrics,
                          ::apps::AppPlatformMetrics::Observer>
      observer_ GUARDED_BY_CONTEXT(sequence_checker_){this};

  // Boolean that controls app metric collection and reporting.
  bool is_enabled_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  // Callback triggered when app metrics are collected and app metric
  // reporting is enabled.
  MetricRepeatingCallback on_metric_observed_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<AppEventsObserver> weak_ptr_factory_{this};
};

}  // namespace reporting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_APPS_APP_EVENTS_OBSERVER_H_
