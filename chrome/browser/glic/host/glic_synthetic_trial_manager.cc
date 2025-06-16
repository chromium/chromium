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
#include "components/variations/synthetic_trial_registry.h"

namespace glic {

GlicSyntheticTrialManager::GlicSyntheticTrialManager() = default;

GlicSyntheticTrialManager::~GlicSyntheticTrialManager() = default;

void GlicSyntheticTrialManager::SetSyntheticExperimentState(
    const std::string& trial_name,
    const std::string& group_name) {
  // If browser is already registered in a conflicting group then discard the
  // logs by marking them in a special group. This avoids combining logs from
  // multiple groups from different profiles.
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
  } else {
    ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
        trial_name, group_name,
        variations::SyntheticTrialAnnotationMode::kCurrentLog);
    synthetic_field_trial_groups_[trial_name] = group_name;
  }
}

}  // namespace glic
