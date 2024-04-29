// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_group_sync/prefs.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/saved_tab_groups/features.h"

namespace tab_groups {

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  if (base::FeatureList::IsEnabled(tab_groups::kTabGroupSyncAndroid)) {
    registry->RegisterBooleanPref(prefs::kAutoOpenSyncedTabGroups,
                                  base::GetFieldTrialParamByFeatureAsBool(
                                      tab_groups::kTabGroupSyncAndroid,
                                      "auto_open_synced_tab_groups", true));
  }
}

}  // namespace tab_groups
