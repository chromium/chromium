// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_GLIC_SYNTHETIC_TRIAL_MANAGER_H_
#define CHROME_BROWSER_GLIC_HOST_GLIC_SYNTHETIC_TRIAL_MANAGER_H_

#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/metrics/metrics_logs_event_manager.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/variations/synthetic_trial_registry.h"

namespace glic {

class GlicSyntheticTrialManager : metrics::MetricsLogsEventManager::Observer {
 public:
  explicit GlicSyntheticTrialManager(
      metrics_services_manager::MetricsServicesManager*
          metrics_services_manager);

  GlicSyntheticTrialManager(const GlicSyntheticTrialManager&) = delete;
  GlicSyntheticTrialManager& operator=(const GlicSyntheticTrialManager&) =
      delete;

  ~GlicSyntheticTrialManager() override;

  // Used by the web client to enroll Chrome in the specified synthetic trial
  // group. If a conflicting group is already registered by another profile
  // than we instead register into a new group called `MultiProfileDetected` to
  // indicate the log file is corrupted. At this point we stage the requested
  // group for enrollment when the next log file is created.
  void SetSyntheticExperimentState(const std::string& trial_name,
                                   const std::string& group_name);

  // metrics::MetricsLogsEventManager::Observer:
  void OnLogEvent(metrics::MetricsLogsEventManager::LogEvent event,
                  std::string_view log_hash,
                  std::string_view message) override;
  void OnLogCreated(
      std::string_view log_hash,
      std::string_view log_data,
      std::string_view log_timestamp,
      metrics::MetricsLogsEventManager::CreateReason reason) override {}

 private:
  std::map<std::string, std::string> synthetic_field_trial_groups_;
  std::map<std::string, std::string> staged_synthetic_field_trial_groups_;
  raw_ptr<metrics::MetricsService> metrics_service_;
  const raw_ptr<metrics_services_manager::MetricsServicesManager>
      metrics_services_manager_;

  base::WeakPtrFactory<GlicSyntheticTrialManager> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_GLIC_SYNTHETIC_TRIAL_MANAGER_H_
