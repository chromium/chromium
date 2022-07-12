// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_METRIC_REPORTING_MANAGER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_METRIC_REPORTING_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/scoped_observation.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_metric_sampler.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_reporting_settings.h"
#include "chrome/browser/ash/policy/status_collector/managed_session_service.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/proto/synced/record_constants.pb.h"

namespace reporting {

class EventDetector;
class MetricEventObserver;
class MetricEventObserverManager;
class MetricReportQueue;
class CollectorBase;
class ReportQueue;
class Sampler;

// Class to initialize and start info, event, and telemetry collection and
// reporting.
class MetricReportingManager : public policy::ManagedSessionService::Observer,
                               public ::ash::DeviceSettingsService::Observer {
 public:
  // Delegate class for dependencies and behaviors that need to be overridden
  // for testing purposes.
  class Delegate {
   public:
    Delegate() = default;

    Delegate(const Delegate& other) = delete;
    Delegate& operator=(const Delegate& other) = delete;

    virtual ~Delegate() = default;

    virtual bool IsAffiliated(Profile* profile);

    virtual std::unique_ptr<ReportQueue, base::OnTaskRunnerDeleter>
    CreateReportQueue(EventType event_type, Destination destination);

    virtual bool IsDeprovisioned();

    virtual std::unique_ptr<MetricReportQueue> CreateMetricReportQueue(
        EventType event_type,
        Destination destination,
        Priority priority);

    virtual std::unique_ptr<MetricReportQueue> CreatePeriodicUploadReportQueue(
        EventType event_type,
        Destination destination,
        Priority priority,
        ReportingSettings* reporting_settings,
        const std::string& rate_setting_path,
        base::TimeDelta default_rate,
        int rate_unit_to_ms = 1);

    virtual std::unique_ptr<CollectorBase> CreateOneShotCollector(
        Sampler* sampler,
        MetricReportQueue* metric_report_queue,
        ReportingSettings* reporting_settings,
        const std::string& enable_setting_path,
        bool setting_enabled_default_value);

    virtual std::unique_ptr<CollectorBase> CreatePeriodicCollector(
        Sampler* sampler,
        MetricReportQueue* metric_report_queue,
        ReportingSettings* reporting_settings,
        const std::string& enable_setting_path,
        bool setting_enabled_default_value,
        const std::string& rate_setting_path,
        base::TimeDelta default_rate,
        int rate_unit_to_ms);

    virtual std::unique_ptr<CollectorBase> CreatePeriodicEventCollector(
        Sampler* sampler,
        std::unique_ptr<EventDetector> event_detector,
        std::vector<Sampler*> additional_samplers,
        MetricReportQueue* metric_report_queue,
        ReportingSettings* reporting_settings,
        const std::string& enable_setting_path,
        bool setting_enabled_default_value,
        const std::string& rate_setting_path,
        base::TimeDelta default_rate,
        int rate_unit_to_ms);

    virtual std::unique_ptr<MetricEventObserverManager>
    CreateEventObserverManager(
        std::unique_ptr<MetricEventObserver> event_observer,
        MetricReportQueue* metric_report_queue,
        ReportingSettings* reporting_settings,
        const std::string& enable_setting_path,
        bool setting_enabled_default_value,
        std::vector<Sampler*> additional_samplers);

    base::TimeDelta GetInitDelay() const;

    base::TimeDelta GetInitialUploadDelay() const;
  };

  static std::unique_ptr<MetricReportingManager> Create(
      policy::ManagedSessionService* managed_session_service);

  static std::unique_ptr<MetricReportingManager> CreateForTesting(
      std::unique_ptr<Delegate> delegate,
      policy::ManagedSessionService* managed_session_service);

  ~MetricReportingManager() override;

  // ManagedSessionService::Observer
  void OnLogin(Profile* profile) override;

  // DeviceSettingsService::Observer
  void DeviceSettingsUpdated() override;

 private:
  MetricReportingManager(
      std::unique_ptr<Delegate> delegate,
      policy::ManagedSessionService* managed_session_service);

  void Shutdown();

  // Init collectors that need to start on startup after a delay, should
  // only be scheduled once on construction.
  void DelayedInit();
  // Init collectors and event observers that need to start after an affiliated
  // user login with no delay, should only be called once on login.
  void InitOnAffiliatedLogin();
  // Init collectors and event observers that need to start after an affiliated
  // user login with a delay, should only be scheduled once on login.
  void DelayedInitOnAffiliatedLogin(Profile* profile);

  void InitOneShotCollector(std::unique_ptr<Sampler> sampler,
                            MetricReportQueue* report_queue,
                            const std::string& enable_setting_path,
                            bool setting_enabled_default_value);
  void InitPeriodicCollector(std::unique_ptr<Sampler> sampler,
                             MetricReportQueue* metric_report_queue,
                             const std::string& enable_setting_path,
                             bool setting_enabled_default_value,
                             const std::string& rate_setting_path,
                             base::TimeDelta default_rate,
                             int rate_unit_to_ms = 1);
  void InitPeriodicEventCollector(std::unique_ptr<Sampler> sampler,
                                  std::unique_ptr<EventDetector> event_detector,
                                  std::vector<Sampler*> additional_samplers,
                                  MetricReportQueue* metric_report_queue,
                                  const std::string& enable_setting_path,
                                  bool setting_enabled_default_value,
                                  const std::string& rate_setting_path,
                                  base::TimeDelta default_rate,
                                  int rate_unit_to_ms = 1);
  void InitEventObserverManager(
      std::unique_ptr<MetricEventObserver> event_observer,
      const std::string& enable_setting_path,
      bool setting_enabled_default_value,
      std::vector<Sampler*> additional_samplers = {});
  void UploadTelemetry();
  void CreateCrosHealthdOneShotCollector(
      chromeos::cros_healthd::mojom::ProbeCategoryEnum probe_category,
      CrosHealthdMetricSampler::MetricType metric_type,
      const std::string& setting_path,
      bool default_value,
      MetricReportQueue* metric_report_queue);

  void InitNetworkCollectors(Profile* profile);

  void InitAudioCollectors();

  void InitPeripheralsCollectors();

  CrosReportingSettings reporting_settings_;

  // Samplers and queues should be destructed on the same sequence where
  // collectors are destructed. Queues should also be destructed on the same
  // sequence where event observer managers are destructed, this is currently
  // enforced by destructing all of them using the `Shutdown` method if they
  // need to be deleted before the destruction of the MetricReportingManager
  // instance.
  std::vector<std::unique_ptr<Sampler>> samplers_;

  std::vector<std::unique_ptr<CollectorBase>> periodic_collectors_;
  std::vector<std::unique_ptr<CollectorBase>> one_shot_collectors_;
  std::vector<std::unique_ptr<MetricEventObserverManager>>
      event_observer_managers_;

  std::unique_ptr<MetricReportQueue> info_report_queue_;
  std::unique_ptr<MetricReportQueue> telemetry_report_queue_;
  std::unique_ptr<MetricReportQueue> user_telemetry_report_queue_;
  std::unique_ptr<MetricReportQueue> event_report_queue_;
  std::unique_ptr<MetricReportQueue>
      peripheral_events_and_telemetry_report_queue_;

  const std::unique_ptr<Delegate> delegate_;

  base::ScopedObservation<policy::ManagedSessionService,
                          policy::ManagedSessionService::Observer>
      managed_session_observation_{this};

  base::ScopedObservation<::ash::DeviceSettingsService,
                          ::ash::DeviceSettingsService::Observer>
      device_settings_observation_{this};

  base::OneShotTimer delayed_init_timer_;

  base::OneShotTimer delayed_init_on_login_timer_;

  base::OneShotTimer initial_upload_timer_;
};
}  // namespace reporting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_METRIC_REPORTING_MANAGER_H_
