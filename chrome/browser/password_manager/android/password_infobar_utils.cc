// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_infobar_utils.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/password_manager/core/browser/password_sync_util.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_contents.h"

namespace password_manager {

AccountInfo GetAccountInfoForPasswordMessages(Profile* profile) {
  DCHECK(profile);

  if (!sync_util::IsPasswordSyncEnabled(
          SyncServiceFactory::GetForProfile(profile))) {
    return AccountInfo();
  }
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  CoreAccountId account_id =
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSync);
  return identity_manager->FindExtendedAccountInfoByAccountId(account_id);
}

std::string GetDisplayableAccountName(content::WebContents* web_contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  absl::optional<AccountInfo> account_info =
      password_manager::GetAccountInfoForPasswordMessages(profile);
  if (!account_info.has_value()) {
    return "";
  }
  return account_info->CanHaveEmailAddressDisplayed()
             ? account_info.value().email
             : account_info.value().full_name;
}

}  // namespace password_manager
