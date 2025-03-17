// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_synthetic_trial_manager.h"

#include <algorithm>
#include <cstddef>
#include <ranges>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "components/metrics/metrics_service.h"
#include "components/variations/synthetic_trial_registry.h"

namespace glic {

GlicSyntheticTrialManager::GlicSyntheticTrialManager(
    metrics_services_manager::MetricsServicesManager* metrics_services_manager)
    : metrics_services_manager_(metrics_services_manager) {}

GlicSyntheticTrialManager::~GlicSyntheticTrialManager() {
  if (metrics_service_) {
    metrics_service_->RemoveLogsObserver(this);
  }
}

void GlicSyntheticTrialManager::SetSyntheticExperimentState(
    const std::string& trial_name,
    const std::string& group_name) {
  if (!metrics_service_) {
    metrics_service_ = metrics_services_manager_->GetMetricsService();
    DCHECK(metrics_service_);
    metrics_service_->AddLogsObserver(this);
  }
  // If already registered discard the logs if in a different group. This
  // avoids combining logs from multiple groups from different profiles.
  bool conflicting_group_registered =
      synthetic_field_trial_groups_.count(trial_name) > 0 &&
      synthetic_field_trial_groups_[trial_name] != group_name;

  if (conflicting_group_registered) {
    if (synthetic_field_trial_groups_[trial_name] != "MultiProfileDetected") {
      base::UmaHistogramBoolean(
          "Glic.ChromeClient.MultiProfileSyntheticTrialConflictDetected",
          conflicting_group_registered);
    }
    ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
        trial_name, "MultiProfileDetected",
        variations::SyntheticTrialAnnotationMode::kCurrentLog);
    synthetic_field_trial_groups_[trial_name] = "MultiProfileDetected";
    // After contaminated logfile is uploaded we store the the most recently
    // requested group to register the next created log file. If TrialA.Group1
    // is reported then TrialA.Group2, then TrialA.Group3, then the next log cut
    // will register TrialA.Group3.
    staged_synthetic_field_trial_groups_[trial_name] = group_name;
  } else {
    ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
        trial_name, group_name,
        variations::SyntheticTrialAnnotationMode::kCurrentLog);
    synthetic_field_trial_groups_[trial_name] = group_name;
  }
}

void GlicSyntheticTrialManager::OnLogEvent(
    metrics::MetricsLogsEventManager::LogEvent event,
    std::string_view log_hash,
    std::string_view message) {
  if (event == metrics::MetricsLogsEventManager::LogEvent::kLogCreated &&
      !staged_synthetic_field_trial_groups_.empty()) {
    // Remove all elements from synthetic_field_trial_groups_ that have
    // entries in staged_synthetic_field_trial_groups_ with the same
    // trial_name.
    for (const auto& [trial, group] : staged_synthetic_field_trial_groups_) {
      ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
          trial, group, variations::SyntheticTrialAnnotationMode::kCurrentLog);
      synthetic_field_trial_groups_[trial] = group;
    }
    staged_synthetic_field_trial_groups_ = {};
  }
}

}  // namespace glic
