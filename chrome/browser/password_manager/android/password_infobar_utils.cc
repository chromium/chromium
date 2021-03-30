// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_infobar_utils.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace password_manager {
base::Optional<AccountInfo> GetAccountInfoForPasswordInfobars(Profile* profile,
                                                              bool is_syncing) {
  DCHECK(profile);
  if (!base::FeatureList::IsEnabled(
          autofill::features::
              kAutofillEnablePasswordInfoBarAccountIndicationFooter) ||
      !is_syncing ||
      !base::FeatureList::IsEnabled(
          autofill::features::
              kAutofillEnableInfoBarAccountIndicationFooterForSyncUsers)) {
    return base::nullopt;
  }
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  CoreAccountId account_id =
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSync);
  base::Optional<AccountInfo> account_info =
      identity_manager
          ->FindExtendedAccountInfoForAccountWithRefreshTokenByAccountId(
              account_id);
  bool is_single_account_user =
      identity_manager->GetAccountsWithRefreshTokens().size() == 1;

  bool should_show_account_footer =
      (!is_single_account_user ||
       base::FeatureList::IsEnabled(
           autofill::features::
               kAutofillEnableInfoBarAccountIndicationFooterForSingleAccountUsers)) &&
      account_info.has_value();

  return should_show_account_footer ? account_info : base::nullopt;
}

base::Optional<AccountInfo> GetAccountInfoForPasswordMessages(Profile* profile,
                                                              bool is_syncing) {
  DCHECK(profile);
  if (!is_syncing)
    return base::nullopt;

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  CoreAccountId account_id =
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSync);
  return identity_manager
      ->FindExtendedAccountInfoForAccountWithRefreshTokenByAccountId(
          account_id);
}

}  // namespace password_manager
