// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_group_sync/tab_group_trial.h"

#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "components/saved_tab_groups/public/synthetic_field_trial_helper.h"
#include "components/variations/synthetic_trials.h"

namespace tab_groups {

namespace {
const char kSyntheticTrialName[] = "SyncableTabGroups";
}  // namespace

// static
void TabGroupTrial::OnTabGroupSyncEnabled(bool enabled) {
  RegisterFieldTrial(kSyntheticTrialName, enabled ? "Enabled" : "Disabled");
}

// static
void TabGroupTrial::OnHadSyncedTabGroup(bool had_synced_group) {
  RegisterFieldTrial(kSyncedTabGroupFieldTrialName,
                     had_synced_group ? kHasOwnedTabGroupTypeName
                                      : kHasNotOwnedTabGroupTypeName);
}

// static
void TabGroupTrial::OnHadSharedTabGroup(bool had_shared_group) {
  RegisterFieldTrial(kSharedTabGroupFieldTrialName,
                     had_shared_group ? kHasOwnedTabGroupTypeName
                                      : kHasNotOwnedTabGroupTypeName);
}

// static
void TabGroupTrial::RegisterFieldTrial(std::string_view trial_name,
                                       std::string_view group_name) {
  ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
      trial_name, group_name,
      variations::SyntheticTrialAnnotationMode::kCurrentLog);
}

}  // namespace tab_groups
