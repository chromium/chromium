// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "chrome/browser/nearby_sharing/nearby_share_default_device_name.h"

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"

// For profile name retrieval:
#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chromeos/constants/devicetype.h"
#include "components/user_manager/user_manager.h"
#include "ui/chromeos/devicetype_utils.h"
#else
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#endif

namespace {

base::Optional<std::string> GetNameFromProfile(Profile* profile) {
  if (!profile)
    return base::nullopt;

  std::string name;
#if defined(OS_CHROMEOS)
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  if (!user)
    return base::nullopt;

  name = base::UTF16ToUTF8(user->GetDisplayName());
#elif defined(OS_WIN)
  // TODO(https://crbug.com/1127603): The non-Chrome OS strategy below caused
  // Nearby Share service unit tests to crash on Windows trybots when we tried
  // to integrate this into the Nearby Share service.
  name = "First Last";
#else  // !defined(OS_CHROMEOS) && !defined(OS_WIN)
  ProfileAttributesEntry* entry = nullptr;
  if (!g_browser_process->profile_manager()
           ->GetProfileAttributesStorage()
           .GetProfileAttributesWithPath(profile->GetPath(), &entry)) {
    return base::nullopt;
  }

  name = base::UTF16ToUTF8(entry->GetLocalProfileName());
#endif

  return name.empty() ? base::nullopt : base::make_optional(name);
}

void OnHardwareInfoFetched(
    Profile* profile,
    base::OnceCallback<void(const base::Optional<std::string>&)> callback,
    base::SysInfo::HardwareInfo hardware_info) {
  base::Optional<std::string> name_from_profile = GetNameFromProfile(profile);
  if (!name_from_profile) {
    std::move(callback).Run(base::nullopt);
    return;
  }

#if defined(OS_CHROMEOS)
  // For Chrome OS, the returned model values are product code names like Eve.
  // We want to use generic names like "Chromebook".
  std::string model_name = base::UTF16ToUTF8(ui::GetChromeOSDeviceName());
#else   // !defined(OS_CHROMEOS)
  // TODO(https://crbug.com/1127017): Localize "Device".
  std::string model_name =
      hardware_info.model.empty() ? "Device" : hardware_info.model;
#endif  // defined(OS_CHROMEOS)

  // TODO(https://crbug.com/1127017): Localize string combination.
  std::move(callback).Run(*name_from_profile + "'s " + model_name);
}

}  // namespace

void GetNearbyShareDefaultDeviceName(
    Profile* profile,
    base::OnceCallback<void(const base::Optional<std::string>&)> callback) {
  base::SysInfo::GetHardwareInfo(
      base::BindOnce(&OnHardwareInfoFetched, profile, std::move(callback)));
}
