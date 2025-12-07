// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/platform_experience/prefs.h"

#include "base/feature_list.h"
#include "chrome/browser/platform_experience/features.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace platform_experience::prefs {

void RegisterPrefs(PrefRegistrySimple& registry) {
  registry.RegisterBooleanPref(kDisablePEHNotificationsPrefName, false);
  registry.RegisterBooleanPref(kShouldUsePEHNotificationTextIndexPrefName,
                               false);
  registry.RegisterIntegerPref(kPEHNotificationTextIndexPrefName, 0);
}

void SetPrefOverrides(PrefService& local_state) {
  local_state.SetBoolean(
      kDisablePEHNotificationsPrefName,
      base::FeatureList::IsEnabled(features::kDisablePEHNotifications));

  if (base::FeatureList::IsEnabled(
          features::kLoadLowEngagementPEHFeaturesToPrefs)) {
    if (base::FeatureList::IsEnabled(
            features::kShouldUseSpecificPEHNotificationText)) {
      local_state.SetBoolean(kShouldUsePEHNotificationTextIndexPrefName, true);
      local_state.SetInteger(kPEHNotificationTextIndexPrefName,
                             features::kUseNotificationTextIndex.Get());
    }
  }
}

}  // namespace platform_experience::prefs
