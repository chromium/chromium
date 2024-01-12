// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCOUNT_MANAGER_ACCOUNT_MANAGER_EDU_COEXISTENCE_CONTROLLER_H_
#define CHROME_BROWSER_ASH_ACCOUNT_MANAGER_ACCOUNT_MANAGER_EDU_COEXISTENCE_CONTROLLER_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/account_id/account_id.h"
#include "components/account_manager_core/account.h"
#include "components/prefs/pref_change_registrar.h"

class PrefRegistrySimple;
class Profile;

namespace account_manager {
class AccountManagerFacade;
class AccountManager;
}

namespace ash {

// Listens to changes to prefs::kEduCoexistenceToSVersion policy
// preference and invalidates secondary edu accounts with outdated terms of
// service version.
class EduCoexistenceConsentInvalidationController {
 public:
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  EduCoexistenceConsentInvalidationController(
      Profile* profile,
      account_manager::AccountManager* account_manager,
      account_manager::AccountManagerFacade* account_manager_facade,
      const AccountId& device_account_id);
  EduCoexistenceConsentInvalidationController(
      const EduCoexistenceConsentInvalidationController&) = delete;
  EduCoexistenceConsentInvalidationController& operator=(
      const EduCoexistenceConsentInvalidationController&) = delete;
  ~EduCoexistenceConsentInvalidationController();

  // Accesses the list from AccountManager to update the list of accounts stored
  // in prefs::kEduCoexistenceToSAcceptedVersion.
  void Init();

 private:
  // Removes accounts which may have been stored in pref but which have since
  // been removed as a secondary account.
  // Secondary edu accounts may have already been added to device prior to M88.
  // Therefore, it updates the
  // prefs::kEduCoexistenceToSAcceptedVersion pref to include those
  // accounts.
  void UpdateEduAccountsInTermsOfServicePref(
      const std::vector<::account_manager::Account>& accounts);

  void TermsOfServicePrefChanged();

  void InvalidateEduAccounts(
      const std::vector<std::string>& account_emails_to_invalidate,
      const std::vector<::account_manager::Account>& accounts);

  const raw_ptr<Profile> profile_;
  const raw_ptr<account_manager::AccountManager> account_manager_;
  const raw_ptr<account_manager::AccountManagerFacade> account_manager_facade_;
  const AccountId device_account_id_;
  PrefChangeRegistrar pref_change_registrar_;
  base::WeakPtrFactory<EduCoexistenceConsentInvalidationController>
      weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCOUNT_MANAGER_ACCOUNT_MANAGER_EDU_COEXISTENCE_CONTROLLER_H_
