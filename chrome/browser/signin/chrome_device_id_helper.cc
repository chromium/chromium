// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/chrome_device_id_helper.h"

#include <string>

#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "components/signin/public/base/device_id_helper.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/command_line.h"
#include "base/logging.h"
#include "base/uuid.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_manager.h"
#endif

std::string GetSigninScopedDeviceIdForProfile(Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableSigninScopedDeviceId)) {
    return std::string();
  }

  // UserManager may not exist in unit_tests.
  if (!user_manager::UserManager::IsInitialized())
    return std::string();

  const user_manager::User* user =
      ash::ProfileHelper::Get()->GetUserByProfile(profile);
  if (!user)
    return std::string();

  user_manager::KnownUser known_user(g_browser_process->local_state());
  const std::string signin_scoped_device_id =
      known_user.GetDeviceId(user->GetAccountId());
  LOG_IF(ERROR, signin_scoped_device_id.empty())
      << "Device ID is not set for user.";
  return signin_scoped_device_id;
#else
  return signin::GetSigninScopedDeviceId(profile->GetPrefs());
#endif
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

std::string GenerateSigninScopedDeviceId(bool for_ephemeral) {
  constexpr char kEphemeralUserDeviceIDPrefix[] = "t_";
  std::string guid = base::Uuid::GenerateRandomV4().AsLowercaseString();
  return for_ephemeral ? kEphemeralUserDeviceIDPrefix + guid : guid;
}

void MigrateSigninScopedDeviceId(Profile* profile) {
  // UserManager may not exist in unit_tests.
  if (!user_manager::UserManager::IsInitialized())
    return;

  const user_manager::User* user =
      ash::ProfileHelper::Get()->GetUserByProfile(profile);
  if (!user)
    return;
  user_manager::KnownUser known_user(g_browser_process->local_state());
  const AccountId account_id = user->GetAccountId();
  if (known_user.GetDeviceId(account_id).empty()) {
    const std::string legacy_device_id = profile->GetPrefs()->GetString(
        prefs::kGoogleServicesSigninScopedDeviceId);
    if (!legacy_device_id.empty()) {
      // Need to move device ID from the old location to the new one, if it has
      // not been done yet.
      known_user.SetDeviceId(account_id, legacy_device_id);
    } else {
      known_user.SetDeviceId(
          account_id, GenerateSigninScopedDeviceId(
                          user_manager::UserManager::Get()
                              ->IsUserNonCryptohomeDataEphemeral(account_id)));
    }
  }
  profile->GetPrefs()->SetString(prefs::kGoogleServicesSigninScopedDeviceId,
                                 std::string());
}

#endif
