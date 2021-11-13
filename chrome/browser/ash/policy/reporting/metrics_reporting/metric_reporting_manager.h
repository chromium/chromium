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
#include "base/time/time.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_reporting_settings.h"
#include "chrome/browser/ash/policy/status_collector/managed_session_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/reporting/proto/synced/record_constants.pb.h"

namespace reporting {

class MetricReportQueue;
class PeriodicCollector;
class Sampler;

// Class to initialize and start info, event, and telemetry collection and
// reporting.
class MetricReportingManager : public policy::ManagedSessionService::Observer {
 public:
  static const base::Feature kEnableNetworkTelemetryReporting;

  // Delegate class for dependencies and behaviors that need to be overridden
  // for testing purposes.
  class Delegate {
   public:
    Delegate();

    Delegate(const Delegate& other) = delete;
    Delegate& operator=(const Delegate& other) = delete;

    virtual ~Delegate();

    virtual std::unique_ptr<MetricReportQueue> CreateInfoReportQueue();

    virtual std::unique_ptr<MetricReportQueue> CreateEventReportQueue();

    virtual std::unique_ptr<MetricReportQueue> CreateTelemetryReportQueue(
        ReportingSettings* reporting_settings,
        const std::string& rate_setting_path,
        base::TimeDelta default_rate);

    virtual Sampler* AddSampler(std::unique_ptr<Sampler> sampler);

    virtual bool IsAffiliated(Profile* profile);

   private:
    std::vector<std::unique_ptr<Sampler>> samplers_;
  };

  static std::unique_ptr<MetricReportingManager> Create(
      policy::ManagedSessionService* managed_session_service);

  static std::unique_ptr<MetricReportingManager> CreateForTesting(
      std::unique_ptr<Delegate> delegate,
      policy::ManagedSessionService* managed_session_service);

  ~MetricReportingManager() override;

  void OnLogin(Profile* profile) override;

 private:
  MetricReportingManager(
      std::unique_ptr<Delegate> delegate,
      policy::ManagedSessionService* managed_session_service);

  // Init reporting queues and collectors that need to start before login,
  // should only be called once on construction.
  void Init();
  // Init reporting queues and collectors that need to start after an
  // affiliated user login, should only be called once on login.
  void InitOnAffiliatedLogin();

  void CreatePeriodicCollector(Sampler* sampler,
                               const std::string& enable_setting_path,
                               const std::string& rate_setting_path,
                               base::TimeDelta default_rate,
                               int rate_unit_to_ms = 1);

  void InitNetworkCollectors();

  CrosReportingSettings reporting_settings_;

  std::vector<std::unique_ptr<PeriodicCollector>> periodic_collectors_;

  const std::unique_ptr<Delegate> delegate_;

  std::unique_ptr<MetricReportQueue> telemetry_report_queue_;

  base::ScopedObservation<policy::ManagedSessionService,
                          policy::ManagedSessionService::Observer>
      managed_session_observation_{this};
};
}  // namespace reporting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_METRIC_REPORTING_MANAGER_H_
