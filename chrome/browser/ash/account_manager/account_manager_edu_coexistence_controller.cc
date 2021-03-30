// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/account_manager/account_manager_edu_coexistence_controller.h"

#include <algorithm>

#include "ash/components/account_manager/account_manager.h"
#include "ash/constants/ash_pref_names.h"
#include "base/logging.h"
#include "base/optional.h"
#include "chrome/browser/ash/account_manager/account_manager_util.h"
#include "chrome/browser/chromeos/child_accounts/edu_coexistence_tos_store_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/edu_coexistence/edu_coexistence_login_handler_chromeos.h"
#include "components/account_manager_core/account_manager_facade.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash {

namespace edu_coexistence = ::chromeos::edu_coexistence;

void EduCoexistenceConsentInvalidationController::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  // |kEduCoexistenceToSVersion| is derived from Google3 CL that introduced
  // new ToS version. We use string data here for the ToS version to be more
  // future proof. In the future we might add a prefix to indicate the flow
  // where the ToS were accepted (OOBE or Settings flow).
  registry->RegisterStringPref(chromeos::prefs::kEduCoexistenceToSVersion,
                               edu_coexistence::kMinTOSVersionNumber);

  // |kEduCoexistenceToSAcceptedVersion| is a dictionary associating the
  // edu accounts present in account manager to the accepted terms of service
  // version.
  registry->RegisterDictionaryPref(
      chromeos::prefs::kEduCoexistenceToSAcceptedVersion);
}

EduCoexistenceConsentInvalidationController::
    EduCoexistenceConsentInvalidationController(
        Profile* profile,
        AccountManager* account_manager,
        account_manager::AccountManagerFacade* account_manager_facade,
        const AccountId& device_account_id)
    : profile_(profile),
      account_manager_(account_manager),
      account_manager_facade_(account_manager_facade),
      device_account_id_(device_account_id) {
  DCHECK(profile_);
  DCHECK(profile_->IsChild());
  DCHECK(IsAccountManagerAvailable(profile_));
}

EduCoexistenceConsentInvalidationController::
    ~EduCoexistenceConsentInvalidationController() = default;

void EduCoexistenceConsentInvalidationController::Init() {
  account_manager_facade_->GetAccounts(
      base::BindOnce(&EduCoexistenceConsentInvalidationController::
                         UpdateEduAccountsInTermsOfServicePref,
                     weak_factory_.GetWeakPtr()));

  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      chromeos::prefs::kEduCoexistenceToSVersion,
      base::BindRepeating(&EduCoexistenceConsentInvalidationController::
                              TermsOfServicePrefChanged,
                          weak_factory_.GetWeakPtr()));

  // No need to call |TermsOfServicePrefChanged| here because
  // |UpdateEduAccountsInTermsOfServicePref| callback created above will call
  // it.
}

void EduCoexistenceConsentInvalidationController::
    UpdateEduAccountsInTermsOfServicePref(
        const std::vector<::account_manager::Account>& accounts) {
  std::vector<edu_coexistence::UserConsentInfo>
      current_edu_account_consent_list =
          edu_coexistence::GetUserConsentInfoListForProfile(profile_);

  std::vector<edu_coexistence::UserConsentInfo> new_edu_account_consent_list;

  //  Adds secondary accounts present in account manager to
  //  |new_edu_account_consent_list|.
  for (const auto& account : accounts) {
    // Don't add the device account id.
    if (account.key.id == device_account_id_.GetGaiaId()) {
      continue;
    }

    auto iterator =
        std::find_if(current_edu_account_consent_list.begin(),
                     current_edu_account_consent_list.end(),
                     [&account](const edu_coexistence::UserConsentInfo& info) {
                       return info.edu_account_gaia_id == account.key.id;
                     });

    // If account exists in |current_edu_account_consent_list| copy the entry
    // over.
    if (iterator != current_edu_account_consent_list.end()) {
      new_edu_account_consent_list.push_back(*iterator);
    } else {
      // Create a new entry with default terms of service version number.
      // This will be used to add secondary edu accounts added in the first
      // version of EduCoexistence.
      new_edu_account_consent_list.push_back(edu_coexistence::UserConsentInfo{
          account.key.id,
          edu_coexistence::
              kMinTOSVersionNumber /* default terms of service version */});
    }
  }

  edu_coexistence::SetUserConsentInfoListForProfile(
      profile_, new_edu_account_consent_list);

  TermsOfServicePrefChanged();
}

void EduCoexistenceConsentInvalidationController::TermsOfServicePrefChanged() {
  std::string new_version = profile_->GetPrefs()->GetString(
      chromeos::prefs::kEduCoexistenceToSVersion);

  std::vector<edu_coexistence::UserConsentInfo> infos =
      edu_coexistence::GetUserConsentInfoListForProfile(profile_);

  std::vector<std::string> to_invalidate;
  for (const auto& info : infos) {
    if (edu_coexistence::IsConsentVersionLessThan(
            info.edu_coexistence_tos_version, new_version)) {
      to_invalidate.push_back(info.edu_account_gaia_id);
    }
  }

  account_manager_facade_->GetAccounts(base::BindOnce(
      &EduCoexistenceConsentInvalidationController::InvalidateEduAccounts,
      weak_factory_.GetWeakPtr(), std::move(to_invalidate)));
}

void EduCoexistenceConsentInvalidationController::InvalidateEduAccounts(
    const std::vector<std::string>& account_gaia_ids_to_invalidate,
    const std::vector<::account_manager::Account>& accounts) {
  for (const ::account_manager::Account& account : accounts) {
    if (account.key.account_type != account_manager::AccountType::kGaia) {
      continue;
    }

    // Do not invalidate the Device Account.
    if (device_account_id_.GetAccountType() == AccountType::GOOGLE &&
        account.key.id == device_account_id_.GetGaiaId()) {
      continue;
    }

    // This account should not be invalidated.
    if (!base::Contains(account_gaia_ids_to_invalidate, account.key.id)) {
      continue;
    }

    // This account is a Secondary EDU Gaia account. Invalidate it.
    account_manager_->UpdateToken(account.key, AccountManager::kInvalidToken);
  }
}

}  // namespace ash
