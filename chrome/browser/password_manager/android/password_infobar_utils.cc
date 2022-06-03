// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_infobar_utils.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace password_manager {
absl::optional<AccountInfo> GetAccountInfoForPasswordInfobars(Profile* profile,
                                                              bool is_syncing) {
  DCHECK(profile);
  if (!base::FeatureList::IsEnabled(
          autofill::features::
              kAutofillEnablePasswordInfoBarAccountIndicationFooter) ||
      !is_syncing ||
      !base::FeatureList::IsEnabled(
          autofill::features::
              kAutofillEnableInfoBarAccountIndicationFooterForSyncUsers)) {
    return absl::nullopt;
  }
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  CoreAccountId account_id =
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSync);
  AccountInfo account_info =
      identity_manager->FindExtendedAccountInfoByAccountId(account_id);
  bool is_single_account_user =
      identity_manager->GetAccountsWithRefreshTokens().size() == 1;

  bool should_show_account_footer =
      (!is_single_account_user ||
       base::FeatureList::IsEnabled(
           autofill::features::
               kAutofillEnableInfoBarAccountIndicationFooterForSingleAccountUsers)) &&
      !account_info.IsEmpty();

  return should_show_account_footer
             ? absl::make_optional<AccountInfo>(account_info)
             : absl::nullopt;
}

absl::optional<AccountInfo> GetAccountInfoForPasswordMessages(
    Profile* profile) {
  DCHECK(profile);

  if (!password_bubble_experiment::IsSmartLockUser(
          SyncServiceFactory::GetForProfile(profile))) {
    return absl::nullopt;
  }
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  CoreAccountId account_id =
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSync);
  AccountInfo account_info =
      identity_manager->FindExtendedAccountInfoByAccountId(account_id);
  return account_info.IsEmpty()
             ? absl::nullopt
             : absl::make_optional<AccountInfo>(account_info);
}

}  // namespace password_manager
