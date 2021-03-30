// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCOUNT_MANAGER_ACCOUNT_MANAGER_POLICY_CONTROLLER_H_
#define CHROME_BROWSER_ASH_ACCOUNT_MANAGER_ACCOUNT_MANAGER_POLICY_CONTROLLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/account_manager/child_account_type_changed_user_data.h"
#include "components/account_id/account_id.h"
#include "components/account_manager_core/account.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"

class Profile;

namespace account_manager {
class AccountManagerFacade;
}

namespace ash {
class AccountManager;
class EduCoexistenceConsentInvalidationController;

class AccountManagerPolicyController : public KeyedService {
 public:
  AccountManagerPolicyController(
      Profile* profile,
      AccountManager* account_manager,
      account_manager::AccountManagerFacade* account_manager_facade,
      const AccountId& device_account_id);
  ~AccountManagerPolicyController() override;

  // Starts applying the behaviour required by |AccountManager|
  // specific prefs and policies.
  void Start();

 private:
  // Callback handler for |AccountManager::GetAccounts|.
  void RemoveSecondaryAccounts(const std::vector<::account_manager::Account>&);

  // Callback for handling changes in |kSecondaryGoogleAccountSigninAllowed|
  // pref.
  void OnSecondaryAccountsSigninAllowedPrefChanged();

  // Callback for handling child account type changes. If user type was changed
  // from Regular to Child or from Child to Regular on session start,
  // |type_changed| is be set to true.
  void OnChildAccountTypeChanged(bool type_changed);

  // Checks if invalidation version for parental consent in EDU accounts
  // addition has changed. If so, calls
  // |InvalidateSecondaryAccountsOnEduConsentChange|.
  void CheckEduCoexistenceSecondaryAccountsInvalidationVersion();

  // Invalidates all secondary accounts and updates consent text version.
  void InvalidateSecondaryAccountsOnEduConsentChange(
      const std::string& new_invalidation_version,
      const std::vector<::account_manager::Account>& accounts);

  // KeyedService implementation.
  void Shutdown() override;

  // Non-owning pointers.
  Profile* const profile_;
  AccountManager* const account_manager_;
  account_manager::AccountManagerFacade* const account_manager_facade_;

  const AccountId device_account_id_;

  // For listening on Pref changes.
  PrefChangeRegistrar pref_change_registrar_;

  base::CallbackListSubscription child_account_type_changed_subscription_;

  std::unique_ptr<EduCoexistenceConsentInvalidationController>
      edu_coexistence_consent_invalidation_controller_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<AccountManagerPolicyController> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AccountManagerPolicyController);
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCOUNT_MANAGER_ACCOUNT_MANAGER_POLICY_CONTROLLER_H_
