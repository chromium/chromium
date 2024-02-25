// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_APPS_APP_EVENTS_OBSERVER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_APPS_APP_EVENTS_OBSERVER_H_

#include <memory>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/apps/app_platform_metrics_retriever.h"
#include "chrome/browser/profiles/profile.h"
#include "components/reporting/metrics/metric_event_observer.h"
#include "components/reporting/metrics/reporting_settings.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_types.h"

namespace reporting {

// Event observer that listens to relevant app related events supported by the
// `AppPlatformMetrics` component for reporting purposes.
class AppEventsObserver : public MetricEventObserver,
                          public ::apps::AppPlatformMetrics::Observer {
 public:
  // Static helper that instantiates the `AppEventsObserver` for the given
  // profile using the specified `ReportingSettings`.
  static std::unique_ptr<AppEventsObserver> CreateForProfile(
      Profile* profile,
      const ReportingSettings* reporting_settings);

  // Static test helper that instantiates the `AppEventsObserver` for the given
  // profile using the specified `AppPlatformMetricsRetriever` and
  // `ReportingSettings`.
  static std::unique_ptr<AppEventsObserver> CreateForTest(
      Profile* profile,
      std::unique_ptr<AppPlatformMetricsRetriever>
          app_platform_metrics_retriever,
      const ReportingSettings* reporting_settings);

  AppEventsObserver(const AppEventsObserver& other) = delete;
  AppEventsObserver& operator=(const AppEventsObserver& other) = delete;
  ~AppEventsObserver() override;

  // MetricEventObserver:
  void SetOnEventObservedCallback(MetricRepeatingCallback callback) override;

  // MetricEventObserver:
  void SetReportingEnabled(bool is_enabled) override;

 private:
  // Tracker that tracks app installs in the user pref store and helps
  // determine if the app was already installed. The `AppRegistryCache` attempts
  // to notify observers of app updates that facilitate tracking new installs,
  // but only if the component is initialized before app is actually installed.
  // It normally reports a list of apps registered on the device on init once
  // app publishers report them, and the tracker helps identify apps that were
  // newly installed so we can ignore the ones that were previously installed.
  // TODO (go/add-app-storage-in-app-service): This will be deprecated in favor
  // of the unified app storage setup in the app service once it is implemented.
  class AppInstallTracker {
   public:
    // Disk consumption metrics name.
    static constexpr char kDiskConsumptionMetricsName[] =
        "Browser.ERP.AppInstallTrackerDiskConsumption";

    explicit AppInstallTracker(base::WeakPtr<Profile> profile);
    AppInstallTracker(const AppInstallTracker& other) = delete;
    AppInstallTracker& operator=(const AppInstallTracker& other) = delete;
    ~AppInstallTracker();

    // Adds the specified app id for tracking purposes.
    void Add(std::string_view app_id);

    // Removes the specified app id.
    void Remove(std::string_view app_id);

    // Returns true if the specified app is being tracked in the user pref
    // store. False otherwise.
    bool Contains(std::string_view app_id) const;

   private:
    SEQUENCE_CHECKER(sequence_checker_);

    // Weak pointer to the user profile. Needed to access the user pref store.
    const base::WeakPtr<Profile> profile_;
  };

  AppEventsObserver(base::WeakPtr<Profile> profile,
                    std::unique_ptr<AppPlatformMetricsRetriever>
                        app_platform_metrics_retriever,
                    const ReportingSettings* reporting_settings);

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

  // Weak pointer to the user profile. Needed to retrieve the app publisher id
  // for reporting purposes.
  base::WeakPtr<Profile> profile_;

  // App install tracker used by the event observer to filter out install event
  // notifications that include pre-installed apps.
  std::unique_ptr<AppInstallTracker> app_install_tracker_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Retriever that retrieves the `AppPlatformMetrics` component so the
  // `AppEventsObserver` can start observing app events.
  const std::unique_ptr<AppPlatformMetricsRetriever>
      app_platform_metrics_retriever_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Pointer to the reporting settings that controls app inventory event
  // reporting. Guaranteed to outlive the observer because it is managed by the
  // `MetricReportingManager`.
  const raw_ptr<const ReportingSettings> reporting_settings_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Observer for tracking app events. Will be reset if the `AppPlatformMetrics`
  // component gets destructed before the event observer.
  base::ScopedObservation<::apps::AppPlatformMetrics,
                          ::apps::AppPlatformMetrics::Observer>
      observer_ GUARDED_BY_CONTEXT(sequence_checker_){this};

  // Callback triggered when app metrics are collected and app metric
  // reporting is enabled.
  MetricRepeatingCallback on_metric_observed_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<AppEventsObserver> weak_ptr_factory_{this};
};

}  // namespace reporting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_APPS_APP_EVENTS_OBSERVER_H_
