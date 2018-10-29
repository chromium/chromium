// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/account_mapper_util.h"

#include "components/account_id/account_id.h"
#include "components/signin/core/browser/account_info.h"
#include "components/signin/core/browser/account_tracker_service.h"

namespace chromeos {

AccountMapperUtil::AccountMapperUtil(
    AccountTrackerService* account_tracker_service)
    : account_tracker_service_(account_tracker_service) {}

AccountMapperUtil::~AccountMapperUtil() = default;

std::string AccountMapperUtil::AccountKeyToOAuthAccountId(
    const AccountManager::AccountKey& account_key) const {
  DCHECK(account_key.IsValid());

  if (account_key.account_type !=
      account_manager::AccountType::ACCOUNT_TYPE_GAIA) {
    return std::string();
  }
  const AccountInfo& account_info = AccountKeyToGaiaAccountInfo(account_key);
  DCHECK(!account_info.account_id.empty()) << "Can't find account id";
  return account_info.account_id;
}

AccountManager::AccountKey AccountMapperUtil::OAuthAccountIdToAccountKey(
    const std::string& account_id) const {
  DCHECK(!account_id.empty());

  const AccountInfo& account_info =
      account_tracker_service_->GetAccountInfo(account_id);

  DCHECK(!account_info.gaia.empty()) << "Can't find account info";
  return AccountManager::AccountKey{
      account_info.gaia, account_manager::AccountType::ACCOUNT_TYPE_GAIA};
}

AccountInfo AccountMapperUtil::AccountKeyToGaiaAccountInfo(
    const AccountManager::AccountKey& account_key) const {
  DCHECK(account_key.IsValid());
  if (account_key.account_type !=
      account_manager::AccountType::ACCOUNT_TYPE_GAIA) {
    return AccountInfo();
  }
  AccountInfo account_info =
      account_tracker_service_->FindAccountInfoByGaiaId(account_key.id);
  DCHECK(!account_info.IsEmpty()) << "Can't find account info";

  return account_info;
}

// static
bool AccountMapperUtil::IsEqual(const AccountManager::AccountKey& account_key,
                                const AccountId& account_id) {
  switch (account_key.account_type) {
    case chromeos::account_manager::AccountType::ACCOUNT_TYPE_GAIA:
      return (account_id.GetAccountType() == AccountType::GOOGLE) &&
             (account_id.GetGaiaId() == account_key.id);
    case chromeos::account_manager::AccountType::ACCOUNT_TYPE_ACTIVE_DIRECTORY:
      return (account_id.GetAccountType() == AccountType::ACTIVE_DIRECTORY) &&
             (account_id.GetObjGuid() == account_key.id);
    case chromeos::account_manager::AccountType::ACCOUNT_TYPE_UNSPECIFIED:
      return false;
  }
}

}  // namespace chromeos
