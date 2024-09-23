// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_group_sync/tab_group_trial.h"

#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "components/variations/synthetic_trials.h"

namespace tab_groups {

namespace {
const char kSyntheticTrialName[] = "SyncableTabGroups";
}  // namespace

void TabGroupTrial::OnTabgroupSyncEnabled(bool enabled) {
  ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
      kSyntheticTrialName, enabled ? "Enabled" : "Disabled",
      variations::SyntheticTrialAnnotationMode::kCurrentLog);
}

}  // namespace tab_groups
