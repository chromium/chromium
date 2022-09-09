// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_METRIC_REPORTING_MANAGER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_METRIC_REPORTING_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/strings/string_piece_forward.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_reporting_settings.h"
#include "chrome/browser/ash/policy/status_collector/managed_session_service.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/chromeos/reporting/metric_reporting_manager_delegate_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "components/reporting/metrics/configured_sampler.h"
#include "components/reporting/metrics/event_driven_telemetry_sampler_pool.h"
#include "components/reporting/proto/synced/record_constants.pb.h"

namespace reporting {

class EventDetector;
class MetricEventObserver;
class MetricEventObserverManager;
class MetricReportQueue;
class CollectorBase;
class Sampler;

// Class to initialize and start info, event, and telemetry collection and
// reporting.
class MetricReportingManager : public policy::ManagedSessionService::Observer,
                               public ::ash::DeviceSettingsService::Observer,
                               public EventDrivenTelemetrySamplerPool {
 public:
  // Delegate class for dependencies and behaviors that need to be overridden
  // for testing purposes.
  class Delegate : public metrics::MetricReportingManagerDelegateBase {
   public:
    Delegate() = default;

    Delegate(const Delegate& other) = delete;
    Delegate& operator=(const Delegate& other) = delete;

    ~Delegate() override = default;

    bool IsAffiliated(Profile* profile) const override;

    virtual bool IsDeprovisioned() const;
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

  // EventDrivenTelemetrySamplerPool:
  std::vector<ConfiguredSampler*> GetTelemetrySamplers(
      MetricEventType event_type) override;

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
  void InitOnAffiliatedLogin();

  // Init telemetry samplers that can only be used in affiliated users sessions.
  void InitTelemetrySamplersOnAffiliatedLogin();

  // Init collectors and event observers that need to start after an affiliated
  // user login with a delay, should only be scheduled once on login.
  void DelayedInitOnAffiliatedLogin(Profile* profile);

  void InitInfoCollector(std::unique_ptr<Sampler> sampler,
                         const std::string& enable_setting_path,
                         bool setting_enabled_default_value);

  void InitOneShotTelemetryCollector(const std::string& sampler_name,
                                     MetricReportQueue* metric_report_queue);

  void InitPeriodicCollector(const std::string& sampler_name,
                             MetricReportQueue* metric_report_queue,
                             const std::string& rate_setting_path,
                             base::TimeDelta default_rate,
                             int rate_unit_to_ms = 1);

  void InitPeriodicEventCollector(const std::string& sampler_name,
                                  std::unique_ptr<EventDetector> event_detector,
                                  MetricReportQueue* metric_report_queue,
                                  const std::string& rate_setting_path,
                                  base::TimeDelta default_rate,
                                  int rate_unit_to_ms = 1);

  void InitEventObserverManager(
      std::unique_ptr<MetricEventObserver> event_observer,
      const std::string& enable_setting_path,
      bool setting_enabled_default_value);

  void UploadTelemetry();

  void CreateCrosHealthdInfoCollector(
      ::ash::cros_healthd::mojom::ProbeCategoryEnum probe_category,
      const std::string& setting_path,
      bool default_value);

  void InitTelemetryConfiguredSampler(const std::string& sampler_name,
                                      std::unique_ptr<Sampler> sampler,
                                      const std::string& enable_setting_path,
                                      bool default_value);

  void InitNetworkCollectors(Profile* profile);

  void InitNetworkPeriodicCollector(const std::string& sampler_name,
                                    MetricReportQueue* metric_report_queue);

  void InitNetworkConfiguredSampler(const std::string& sampler_name,
                                    std::unique_ptr<Sampler> sampler);

  void InitAudioCollectors();

  void InitPeripheralsCollectors();

  void InitDisplayCollectors();

  CrosReportingSettings reporting_settings_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Samplers and queues should be destructed on the same sequence where
  // collectors are destructed. Queues should also be destructed on the same
  // sequence where event observer managers are destructed, this is currently
  // enforced by destructing all of them using the `Shutdown` method if they
  // need to be deleted before the destruction of the MetricReportingManager
  // instance.
  std::vector<std::unique_ptr<Sampler>> info_samplers_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::flat_map<std::string, std::unique_ptr<ConfiguredSampler>>
      telemetry_sampler_map_ GUARDED_BY_CONTEXT(sequence_checker_);

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

  base::ScopedObservation<policy::ManagedSessionService,
                          policy::ManagedSessionService::Observer>
      managed_session_observation_{this};

  base::ScopedObservation<::ash::DeviceSettingsService,
                          ::ash::DeviceSettingsService::Observer>
      device_settings_observation_{this};

  base::OneShotTimer delayed_init_timer_;

  base::OneShotTimer delayed_init_on_login_timer_;

  base::OneShotTimer initial_upload_timer_;

  // This sampler will be removed with lacros, so we avoid adding it to
  // `telemetry_sampler_map_` to make sure it won't be used for event driven
  // telemetry.
  std::unique_ptr<Sampler> network_bandwidth_sampler_;

  std::unique_ptr<Delegate> delegate_;
};

}  // namespace reporting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_METRIC_REPORTING_MANAGER_H_
