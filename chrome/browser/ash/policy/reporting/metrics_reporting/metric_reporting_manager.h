// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_METRIC_REPORTING_MANAGER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_METRIC_REPORTING_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/scoped_observation.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_metric_sampler.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_reporting_settings.h"
#include "chrome/browser/ash/policy/status_collector/managed_session_service.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "components/reporting/proto/synced/record_constants.pb.h"

namespace reporting {

class EventDetector;
class MetricEventObserver;
class MetricEventObserverManager;
class MetricReportQueue;
class OneShotCollector;
class PeriodicCollector;
class ReportQueue;
class Sampler;

// Class to initialize and start info, event, and telemetry collection and
// reporting.
class MetricReportingManager : public policy::ManagedSessionService::Observer,
                               public ::ash::DeviceSettingsService::Observer {
 public:
  static const base::Feature kEnableNetworkTelemetryReporting;

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
    CreateReportQueue(Destination destination);

    virtual std::unique_ptr<Sampler> CreateHttpsLatencySampler();

    virtual bool IsDeprovisioned();
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

  // Init reporting queues and collectors that need to start before login,
  // should only be called once on construction.
  void Init();
  // Init reporting queues and collectors that need to start after an
  // affiliated user login, should only be called once on login.
  void InitOnAffiliatedLogin();

  std::unique_ptr<MetricReportQueue> CreateMetricReportQueue(
      Destination destination,
      Priority priority);
  std::unique_ptr<MetricReportQueue> CreateTelemetryQueue();

  void CreateOneShotCollector(std::unique_ptr<Sampler> sampler,
                              MetricReportQueue* report_queue,
                              const std::string& enable_setting_path,
                              bool setting_enabled_default_value);
  void CreatePeriodicCollector(std::unique_ptr<Sampler> sampler,
                               const std::string& enable_setting_path,
                               bool setting_enabled_default_value,
                               const std::string& rate_setting_path,
                               base::TimeDelta default_rate,
                               int rate_unit_to_ms = 1);
  void CreatePeriodicEventCollector(
      std::unique_ptr<Sampler> sampler,
      std::unique_ptr<EventDetector> event_detector,
      std::vector<Sampler*> additional_samplers,
      const std::string& enable_setting_path,
      bool setting_enabled_default_value,
      const std::string& rate_setting_path,
      base::TimeDelta default_rate,
      int rate_unit_to_ms = 1);
  void CreateEventObserverManager(
      std::unique_ptr<MetricEventObserver> event_observer,
      const std::string& enable_setting_path,
      bool setting_enabled_default_value,
      std::vector<Sampler*> additional_samplers = {});

  void InitCrosHealthdInfoCollector(
      chromeos::cros_healthd::mojom::ProbeCategoryEnum probe_category,
      const std::string& setting_path,
      bool default_value);

  void InitNetworkCollectors();

  CrosReportingSettings reporting_settings_;

  std::vector<std::unique_ptr<Sampler>> samplers_;

  std::vector<std::unique_ptr<PeriodicCollector>> periodic_collectors_;
  std::vector<std::unique_ptr<OneShotCollector>> one_shot_collectors_;
  std::vector<std::unique_ptr<MetricEventObserverManager>>
      event_observer_managers_;

  std::unique_ptr<MetricReportQueue> info_report_queue_;
  std::unique_ptr<MetricReportQueue> telemetry_report_queue_;
  std::unique_ptr<MetricReportQueue> event_report_queue_;

  const std::unique_ptr<Delegate> delegate_;

  base::ScopedObservation<policy::ManagedSessionService,
                          policy::ManagedSessionService::Observer>
      managed_session_observation_{this};

  base::ScopedObservation<::ash::DeviceSettingsService,
                          ::ash::DeviceSettingsService::Observer>
      device_settings_observation_{this};
};
}  // namespace reporting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_METRIC_REPORTING_MANAGER_H_
