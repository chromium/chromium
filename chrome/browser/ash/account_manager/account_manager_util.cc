// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/account_manager/account_manager_util.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/ash/account_manager/account_manager_ui_impl.h"
#include "chrome/browser/ash/net/delay_network_call.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/account_manager/account_manager_factory.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#include "chromeos/components/kiosk/kiosk_utils.h"
#include "chromeos/components/mgs/managed_guest_session_utils.h"
#include "components/account_manager_core/chromeos/account_manager.h"
#include "components/account_manager_core/chromeos/account_manager_mojo_service.h"
#include "components/account_manager_core/chromeos/account_manager_ui.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash {

bool IsAccountManagerAvailable(Profile* profile) {
  // Signin Profile does not have any accounts associated with it,
  // LockScreenAppProfile and LockScreenProfile do not link to the user's
  // cryptohome.
  if (!IsUserBrowserContext(profile)) {
    return false;
  }

  // Account Manager is unavailable on Guest (Incognito) Sessions.
  if (profile->IsGuestSession() || profile->IsOffTheRecord()) {
    return false;
  }

  // In Web kiosk mode, we should not enable account manager since we use robot
  // accounts.
  if (chromeos::IsWebKioskSession()) {
    return false;
  }

  // Account Manager is unavailable on Managed Guest Sessions.
  if (chromeos::IsManagedGuestSession()) {
    return false;
  }

  // Available in all other cases.
  return true;
}

void InitializeAccountManager(
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
    const base::FilePath& cryptohome_root_dir,
    base::OnceClosure initialization_callback) {
  account_manager::AccountManager* account_manager =
      AccountManagerFactory::Get()->GetAccountManager(
          /*profile_path=*/cryptohome_root_dir.value());

  account_manager->Initialize(cryptohome_root_dir,
                              std::move(shared_url_loader_factory),
                              base::BindRepeating(&DelayNetworkCall),
                              std::move(initialization_callback));

  crosapi::AccountManagerMojoService* account_manager_mojo_service =
      AccountManagerFactory::Get()->GetAccountManagerMojoService(
          /*profile_path=*/cryptohome_root_dir.value());

  account_manager_mojo_service->SetAccountManagerUI(
      std::make_unique<AccountManagerUIImpl>());
}

}  // namespace ash
