// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_infobar_utils.h"

#include <optional>

#include "components/password_manager/core/browser/password_sync_util.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace password_manager {

std::optional<AccountInfo> GetAccountInfoForPasswordMessages(
    syncer::SyncService* sync_service,
    signin::IdentityManager* identity_manager) {
  if (!password_manager::sync_util::HasChosenToSyncPasswords(sync_service)) {
    return std::nullopt;
  }
  CoreAccountId account_id =
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  return identity_manager->FindExtendedAccountInfoByAccountId(account_id);
}

std::string GetDisplayableAccountName(
    syncer::SyncService* sync_service,
    signin::IdentityManager* identity_manager) {
  std::optional<AccountInfo> account_info =
      password_manager::GetAccountInfoForPasswordMessages(sync_service,
                                                          identity_manager);
  if (!account_info.has_value()) {
    return "";
  }
  return account_info->CanHaveEmailAddressDisplayed()
             ? account_info.value().email
             : account_info.value().full_name;
}

}  // namespace password_manager
