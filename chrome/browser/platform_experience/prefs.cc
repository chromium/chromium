// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/platform_experience/prefs.h"

#include "base/debug/stack_trace.h"
#include "base/feature_list.h"
#include "chrome/browser/platform_experience/features.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace platform_experience::prefs {

void RegisterPrefs(PrefRegistrySimple& registry) {
  registry.RegisterBooleanPref(kDisablePEHNotificationsPrefName, false);
}

// TODO(crbug.com/423037244): Add overrides for notification text index, gated
// on engagement levels.
void SetPrefOverrides(PrefService& local_state) {
  local_state.SetBoolean(
      kDisablePEHNotificationsPrefName,
      base::FeatureList::IsEnabled(features::kDisablePEHNotifications));
}

}  // namespace platform_experience::prefs
