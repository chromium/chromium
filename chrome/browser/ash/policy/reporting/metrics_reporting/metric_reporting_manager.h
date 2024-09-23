// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_METRIC_REPORTING_MANAGER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_METRIC_REPORTING_MANAGER_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/feature_list.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/apps/app_usage_observer.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_sampler_handlers/cros_healthd_sampler_handler.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_reporting_settings.h"
#include "chrome/browser/ash/policy/status_collector/managed_session_service.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/chromeos/reporting/local_state_reporting_settings.h"
#include "chrome/browser/chromeos/reporting/metric_reporting_manager_delegate_base.h"
#include "chrome/browser/chromeos/reporting/user_reporting_settings.h"
#include "chrome/browser/chromeos/reporting/websites/website_usage_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "components/reporting/metrics/event_driven_telemetry_collector_pool.h"
#include "components/reporting/metrics/periodic_event_collector.h"
#include "components/reporting/proto/synced/record_constants.pb.h"

namespace reporting {

class MetricEventObserver;
class MetricEventObserverManager;
class MetricReportQueue;
class CollectorBase;
class Sampler;
class FatalCrashEventsObserver;
class ChromeFatalCrashEventsObserver;

BASE_DECLARE_FEATURE(kEnableAppEventsObserver);
BASE_DECLARE_FEATURE(kEnableFatalCrashEventsObserver);
BASE_DECLARE_FEATURE(kEnableChromeFatalCrashEventsObserver);
BASE_DECLARE_FEATURE(kEnableRuntimeCountersTelemetry);
BASE_DECLARE_FEATURE(kEnableKioskVisionTelemetry);

// Class to initialize and start info, event, and telemetry collection and
// reporting.
class MetricReportingManager : public policy::ManagedSessionService::Observer,
                               public ::ash::DeviceSettingsService::Observer,
                               public EventDrivenTelemetryCollectorPool {
 public:
  // Delegate class for dependencies and behaviors that need to be overridden
  // for testing purposes.
  class Delegate : public metrics::MetricReportingManagerDelegateBase {
   public:
    Delegate() = default;

    Delegate(const Delegate& other) = delete;
    Delegate& operator=(const Delegate& other) = delete;

    ~Delegate() override = default;

    bool IsUserAffiliated(Profile& profile) const override;

    virtual bool IsDeprovisioned() const;

    virtual std::unique_ptr<Sampler> GetHttpsLatencySampler() const;

    virtual std::unique_ptr<Sampler> GetNetworkTelemetrySampler() const;

    // Returns app service availability for the given profile. Not all profiles
    // can run apps (for example, non-guest incognito profiles).
    virtual bool IsAppServiceAvailableForProfile(Profile* profile) const;
  };

  static std::unique_ptr<MetricReportingManager> Create(
      policy::ManagedSessionService* managed_session_service);

  static std::unique_ptr<MetricReportingManager> CreateForTesting(
      std::unique_ptr<Delegate> delegate,
      policy::ManagedSessionService* managed_session_service);

  ~MetricReportingManager() override;

  // ManagedSessionService::Observer:
  void OnLogin(Profile* profile) override;

  // DeviceSettingsService::Observer:
  void DeviceSettingsUpdated() override;

  // EventDrivenTelemetryCollectorPool:
  std::vector<raw_ptr<CollectorBase, VectorExperimental>>
  GetTelemetryCollectors(MetricEventType event_type) override;

  // Can be nullptr of the feature flag is not enabled.
  FatalCrashEventsObserver* fatal_crash_events_observer();

 private:
  MetricReportingManager(
      std::unique_ptr<Delegate> delegate,
      policy::ManagedSessionService* managed_session_service);

  void Shutdown();

  // Init telemetry samplers that it is allowed to be used even before login.
  void InitDeviceTelemetrySamplers();

  // Init collectors that need to start on startup after a delay, should
  // only be scheduled once on construction.
  void DelayedInit();

  // Init samplers, collectors and event observers that need to start after an
  // affiliated user login with no delay, should only be called once on login.
  void InitOnAffiliatedLogin(Profile* profile);

  // Init telemetry samplers that can only be used in affiliated users sessions.
  void InitTelemetrySamplersOnAffiliatedLogin();

  // Init collectors and event observers that need to start after an affiliated
  // user login with a delay, should only be scheduled once on login.
  void DelayedInitOnAffiliatedLogin(Profile* profile);

  // Initializes an info data collector. An info data collector is always
  // one-shot, i.e., only collects once. The report queue is always
  // `info_report_queue_`.
  //
  // `sampler` is the sampler that collects info.
  //
  // `enable_setting_path` is the name of the setting that enables or disables
  // the collection.
  //
  // `enable_default_value` indicates whether the setting is enabled by default.
  void InitInfoCollector(std::unique_ptr<Sampler> sampler,
                         const std::string& enable_setting_path,
                         bool setting_enabled_default_value);

  // Initializes a telemetry data collector that collects once.
  //
  // `collector_name` is the name of the collector.
  //
  // `sampler` is the sampler that collects telemetry.
  //
  // `metric_report_queue` is the report queue to use.
  //
  // `enable_setting_path` is the name of the setting that enables or disables
  // the collection.
  //
  // `enable_default_value` indicates whether the setting is enabled by default.
  //
  // `init_delay` is the initial delay before the first collection occurs.
  void InitOneShotTelemetryCollector(const std::string& collector_name,
                                     Sampler* sampler,
                                     MetricReportQueue* metric_report_queue,
                                     const std::string& enable_setting_path,
                                     bool enable_default_value,
                                     base::TimeDelta init_delay);

  // Initializes a telemetry data collector that is manually triggered. See
  // `ManualCollector`.
  //
  // `collector_name` is the name of the collector.
  //
  // `sampler` is the sampler that collects telemetry.
  //
  // `metric_report_queue` is the report queue to use.
  //
  // `enable_setting_path` is the name of the setting that enables or disables
  // the collection.
  //
  // `enable_default_value` indicates whether the setting is enabled by default.
  void InitManualTelemetryCollector(const std::string& collector_name,
                                    Sampler* sampler,
                                    MetricReportQueue* metric_report_queue,
                                    const std::string& enable_setting_path,
                                    bool enable_default_value);

  // Initializes a telemetry data collector that collects periodically.
  //
  // `collector_name is the name of the collector.
  //
  // `sampler is the sampler that collects telemetry.
  //
  // `metric_report_queue is the report queue to use.
  //
  // `enable_setting_path` is the name of the setting that enables or disables
  // the collection.
  //
  // `enable_default_value` indicates whether the setting is enabled by default.
  //
  // `rate_setting_path` is the name of the setting that controls the rate. Rate
  // refers to the time between two consecutive periodic metric collections,
  // i.e., the period of the repeated metric collections. See
  // `MetricRateController`.
  //
  // `default_rate` is the default rate.
  //
  // `rate_unit_to_ms` multiplied with the rate in the rate setting results in
  // the rate in milliseconds.
  //
  // `init_delay` is the initial delay before the first collection occurs.
  void InitPeriodicTelemetryCollector(const std::string& collector_name,
                                      Sampler* sampler,
                                      MetricReportQueue* metric_report_queue,
                                      const std::string& enable_setting_path,
                                      bool enable_default_value,
                                      const std::string& rate_setting_path,
                                      base::TimeDelta default_rate,
                                      int rate_unit_to_ms,
                                      base::TimeDelta init_delay);

  // Initializes an event data collector that collects periodically.
  //
  // `sampler` is the sampler that collects events.
  //
  // `event_detector` is the event detector.
  //
  // `metric_report_queue` is the report queue to use.
  //
  // `enable_setting_path` is the name of the setting that enables or disables
  // the collection.
  //
  // `enable_default_value` indicates whether the setting is enabled by default.
  //
  // `rate_setting_path is the name of the setting that controls the rate. Rate
  // refers to the time between two consecutive periodic metric collections,
  // i.e., the period of the repeated metric collections. See
  // `MetricRateController`.
  //
  // `default_rate` is the default rate.
  //
  // `rate_unit_to_ms` multiplied with the rate in the rate setting results in
  // the rate in milliseconds.
  //
  // `init_delay` is the initial delay before the first collection occurs.
  void InitPeriodicEventCollector(
      Sampler* sampler,
      std::unique_ptr<PeriodicEventCollector::EventDetector> event_detector,
      MetricReportQueue* metric_report_queue,
      const std::string& enable_setting_path,
      bool enable_default_value,
      const std::string& rate_setting_path,
      base::TimeDelta default_rate,
      int rate_unit_to_ms,
      base::TimeDelta init_delay);

  void InitEventObserverManager(
      std::unique_ptr<MetricEventObserver> event_observer,
      MetricReportQueue* report_queue,
      ReportingSettings* reporting_settings,
      const std::string& enable_setting_path,
      bool setting_enabled_default_value,
      base::TimeDelta init_delay);

  void UploadTelemetry();

  void CreateCrosHealthdInfoCollector(
      std::unique_ptr<CrosHealthdSamplerHandler> info_handler,
      ::ash::cros_healthd::mojom::ProbeCategoryEnum probe_category,
      const std::string& setting_path,
      bool default_value);

  void InitNetworkCollectors(Profile* profile);

  void InitNetworkPeriodicCollector(const std::string& collector_name,
                                    std::unique_ptr<Sampler> sampler);

  void InitNetworkConfiguredSampler(const std::string& sampler_name,
                                    std::unique_ptr<Sampler> sampler);

  // Initializes app telemetry samplers for the given profile.
  void InitAppCollectors(Profile* profile);

  void InitAudioCollectors();

  void InitBootPerformanceCollector();

  void InitFatalCrashCollectors();

  void InitPeripheralsCollectors();

  void InitRuntimeCountersCollectors();

  void InitWebsiteMetricCollectors(Profile* profile);

  void InitDisplayCollectors();

  // Initializes a periodic collector that collects device activity state.
  void InitDeviceActivityCollector();

  // Initializes a periodic collector that sends out heartbeat signals.
  void InitKioskHeartbeatTelemetryCollector();

  // Initializes a periodic collector that sends the audience telemetry data
  // from the Kiosk vision framework.
  void InitKioskVisionTelemetryCollector();

  base::TimeDelta GetUploadDelay() const;

  std::vector<raw_ptr<CollectorBase, VectorExperimental>>
  GetTelemetryCollectorsFromSetting(std::string_view setting_name);

  CrosReportingSettings reporting_settings_;
  LocalStateReportingSettings local_state_reporting_settings_;
  std::unique_ptr<UserReportingSettings> user_reporting_settings_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Samplers and queues should be destructed on the same sequence where
  // collectors are destructed. Queues should also be destructed on the same
  // sequence where event observer managers are destructed, this is currently
  // enforced by destructing all of them using the `Shutdown` method if they
  // need to be deleted before the destruction of the MetricReportingManager
  // instance.
  std::vector<std::unique_ptr<Sampler>> samplers_
      GUARDED_BY_CONTEXT(sequence_checker_);

  std::unique_ptr<MetricReportQueue> info_report_queue_;
  std::unique_ptr<MetricReportQueue> telemetry_report_queue_;
  std::unique_ptr<MetricReportQueue> user_telemetry_report_queue_;
  std::unique_ptr<MetricReportQueue> event_report_queue_;
  std::unique_ptr<MetricReportQueue> crash_event_report_queue_;
  std::unique_ptr<MetricReportQueue> chrome_crash_event_report_queue_;
  std::unique_ptr<MetricReportQueue> user_event_report_queue_;
  std::unique_ptr<MetricReportQueue> app_event_report_queue_;
  std::unique_ptr<MetricReportQueue> website_event_report_queue_;
  std::unique_ptr<MetricReportQueue>
      user_peripheral_events_and_telemetry_report_queue_;
  std::unique_ptr<MetricReportQueue> kiosk_heartbeat_telemetry_report_queue_;

  base::ScopedObservation<policy::ManagedSessionService,
                          policy::ManagedSessionService::Observer>
      managed_session_observation_{this};

  base::ScopedObservation<::ash::DeviceSettingsService,
                          ::ash::DeviceSettingsService::Observer>
      device_settings_observation_{this};

  base::OneShotTimer initial_upload_timer_;

  // This collector will be removed with lacros, so we avoid adding it to
  // `telemetry_collectors_` to make sure it won't be used for event driven
  // telemetry.
  std::unique_ptr<CollectorBase> network_bandwidth_collector_;

  base::flat_map<std::string, std::unique_ptr<CollectorBase>>
      telemetry_collectors_ GUARDED_BY_CONTEXT(sequence_checker_);

  std::vector<std::unique_ptr<CollectorBase>> info_collectors_
      GUARDED_BY_CONTEXT(sequence_checker_);

  std::vector<std::unique_ptr<MetricEventObserverManager>>
      event_observer_managers_ GUARDED_BY_CONTEXT(sequence_checker_);
  // Fatal crash event observer. Life time of this object is owned by
  // `event_observer_managers_`.
  raw_ptr<FatalCrashEventsObserver> fatal_crash_events_observer_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Chrome fatal crash event observer. Life time of this object is owned by
  // `event_observer_managers_`.
  raw_ptr<ChromeFatalCrashEventsObserver> chrome_fatal_crash_events_observer_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // App usage observer used to observe and collect app usage reports from the
  // `AppPlatformMetrics` component.
  std::unique_ptr<AppUsageObserver> app_usage_observer_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Website usage observer used to observe and collect website usage reports
  // from the `WebsiteMetrics` component.
  std::unique_ptr<WebsiteUsageObserver> website_usage_observer_
      GUARDED_BY_CONTEXT(sequence_checker_);

  std::unique_ptr<Delegate> delegate_;
};

}  // namespace reporting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_METRIC_REPORTING_MANAGER_H_
