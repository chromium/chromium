// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/account_manager/account_manager_util.h"

#include <utility>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/net/delay_network_call.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chromeos/components/account_manager/account_manager.h"
#include "chromeos/components/account_manager/account_manager_factory.h"
#include "chromeos/tpm/install_attributes.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace chromeos {

bool IsAccountManagerAvailable(const Profile* const profile) {
  // Signin Profile does not have any accounts associated with it.
  if (chromeos::ProfileHelper::IsSigninProfile(profile))
    return false;

  // LockScreenAppProfile does not link to the user's cryptohome.
  if (chromeos::ProfileHelper::IsLockScreenAppProfile(profile))
    return false;

  // Account Manager is unavailable on Guest (Incognito) Sessions.
  if (profile->IsGuestSession() || profile->IsOffTheRecord())
    return false;

  // Account Manager is unavailable on Managed Guest Sessions / Public Sessions.
  if (profiles::IsPublicSession())
    return false;

  // Temporarily disabled for Active Directory devices.
  if (InstallAttributes::Get()->IsActiveDirectoryManaged())
    return false;

  // Available in all other cases.
  return true;
}

void InitializeAccountManager(const base::FilePath& cryptohome_root_dir,
                              base::OnceClosure initialization_callback) {
  chromeos::AccountManager* account_manager =
      g_browser_process->platform_part()
          ->GetAccountManagerFactory()
          ->GetAccountManager(cryptohome_root_dir.value());

  account_manager->Initialize(
      cryptohome_root_dir,
      g_browser_process->system_network_context_manager()
          ->GetSharedURLLoaderFactory(),
      base::BindRepeating(&chromeos::DelayNetworkCall,
                          base::TimeDelta::FromMilliseconds(
                              chromeos::kDefaultNetworkRetryDelayMS)),
      std::move(initialization_callback));
}

}  // namespace chromeos
