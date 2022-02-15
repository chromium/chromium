// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/floating_workspace/floating_workspace_util.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/check.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"

namespace ash {

namespace {
PrefService* GetPrimaryUserPrefService() {
  auto* primary_user = user_manager::UserManager::Get()->GetPrimaryUser();
  auto* user_profile =
      chromeos::ProfileHelper::Get()->GetProfileByUser(primary_user);
  return user_profile->GetPrefs();
}

}  // namespace

namespace floating_workspace_util {

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kFloatingWorkspaceEnabled, false);
}

bool IsFloatingWorkspaceEnabled() {
  PrefService* pref_service = GetPrimaryUserPrefService();
  DCHECK(pref_service);

  const PrefService::Preference* floating_workspace_pref =
      pref_service->FindPreference(ash::prefs::kFloatingWorkspaceEnabled);

  DCHECK(floating_workspace_pref);

  if (floating_workspace_pref->IsManaged()) {
    // If there is a policy managing the pref, return what is set by policy.
    return pref_service->GetBoolean(ash::prefs::kFloatingWorkspaceEnabled);
  }
  // If the policy is not set, return feature flag status.
  return features::IsFloatingWorkspaceEnabled();
}

}  // namespace floating_workspace_util
}  // namespace ash
