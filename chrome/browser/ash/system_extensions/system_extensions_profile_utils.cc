// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_extensions/system_extensions_profile_utils.h"

#include "ash/constants/ash_features.h"
#include "base/files/file_path.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/user_manager/user_manager.h"

namespace ash {

constexpr char kSystemExtensionsProfileDirectory[] = "SystemExtensions";

bool IsSystemExtensionsEnabled(Profile* profile) {
  return !!GetProfileForSystemExtensions(profile);
}

Profile* GetProfileForSystemExtensions(Profile* profile) {
  if (!base::FeatureList::IsEnabled(ash::features::kSystemExtensions))
    return nullptr;

  // Enable System Extensions on the primary profile only for now. As we
  // implement new System Extension types we will enable the provider on other
  // profiles.
  if (profile->IsOffTheRecord())
    return nullptr;

  if (profile->IsSystemProfile())
    return nullptr;

  if (!ash::ProfileHelper::IsUserProfile(profile))
    return nullptr;

  if (!ash::ProfileHelper::IsPrimaryProfile(profile))
    return nullptr;

  auto* user_manager = user_manager::UserManager::Get();
  if (user_manager && user_manager->IsLoggedInAsAnyKioskApp())
    return nullptr;

  if (profile->IsGuestSession())
    return nullptr;

  return profile;
}

base::FilePath GetDirectoryForSystemExtension(Profile& profile,
                                              const SystemExtensionId& id) {
  return GetSystemExtensionsProfileDir(profile).Append(
      SystemExtension::IdToString(id));
}

base::FilePath GetSystemExtensionsProfileDir(Profile& profile) {
  return profile.GetPath().Append(kSystemExtensionsProfileDirectory);
}

}  // namespace ash
