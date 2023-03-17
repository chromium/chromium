// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/users/avatar/user_image_prefs.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash::user_image::prefs {

const char kUserAvatarCustomizationSelectorsEnabled[] =
    "user_avatar_customization_selectors_enabled";

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kUserAvatarCustomizationSelectorsEnabled, true);
}

bool IsCustomizationSelectorsPrefEnabled() {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  PrefService* pref_service = profile->GetPrefs();
  return pref_service->GetBoolean(
      user_image::prefs::kUserAvatarCustomizationSelectorsEnabled);
}

}  // namespace ash::user_image::prefs
