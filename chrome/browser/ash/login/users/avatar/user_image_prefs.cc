// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/users/avatar/user_image_prefs.h"

#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash::user_image::prefs {

const char kUserAvatarCustomizationSelectorsEnabled[] =
    "user_avatar_customization_selectors_enabled";

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kUserAvatarCustomizationSelectorsEnabled, true);
}

bool IsCustomizationSelectorsPrefEnabled(const PrefService* prefs) {
  return prefs->GetBoolean(
      user_image::prefs::kUserAvatarCustomizationSelectorsEnabled);
}

}  // namespace ash::user_image::prefs
