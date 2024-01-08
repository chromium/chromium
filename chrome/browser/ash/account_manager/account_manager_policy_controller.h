// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCOUNT_MANAGER_ACCOUNT_MANAGER_POLICY_CONTROLLER_H_
#define CHROME_BROWSER_ASH_ACCOUNT_MANAGER_ACCOUNT_MANAGER_POLICY_CONTROLLER_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
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
class AccountManager;
}

namespace ash {
class EduCoexistenceConsentInvalidationController;

class AccountManagerPolicyController : public KeyedService {
 public:
  AccountManagerPolicyController(
      Profile* profile,
      account_manager::AccountManager* account_manager,
      account_manager::AccountManagerFacade* account_manager_facade,
      const AccountId& device_account_id);

  AccountManagerPolicyController(const AccountManagerPolicyController&) =
      delete;
  AccountManagerPolicyController& operator=(
      const AccountManagerPolicyController&) = delete;

  ~AccountManagerPolicyController() override;

  // Starts applying the behaviour required by |account_manager::AccountManager|
  // specific prefs and policies.
  void Start();

 private:
  // Callback handler for |account_manager::AccountManager::GetAccounts|.
  void RemoveSecondaryAccounts(const std::vector<::account_manager::Account>&);

  // Callback for handling changes in |kSecondaryGoogleAccountSigninAllowed|
  // pref.
  void OnSecondaryAccountsSigninAllowedPrefChanged();

  // Callback for handling child account type changes. If user type was changed
  // from Regular to Child or from Child to Regular on session start,
  // |type_changed| is be set to true.
  void OnChildAccountTypeChanged(bool type_changed);

  // KeyedService implementation.
  void Shutdown() override;

  // Non-owning pointers.
  const raw_ptr<Profile> profile_;
  const raw_ptr<account_manager::AccountManager> account_manager_;
  const raw_ptr<account_manager::AccountManagerFacade> account_manager_facade_;

  const AccountId device_account_id_;

  // For listening on Pref changes.
  PrefChangeRegistrar pref_change_registrar_;

  base::CallbackListSubscription child_account_type_changed_subscription_;

  std::unique_ptr<EduCoexistenceConsentInvalidationController>
      edu_coexistence_consent_invalidation_controller_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<AccountManagerPolicyController> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCOUNT_MANAGER_ACCOUNT_MANAGER_POLICY_CONTROLLER_H_
