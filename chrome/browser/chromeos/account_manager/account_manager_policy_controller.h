// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ACCOUNT_MANAGER_ACCOUNT_MANAGER_POLICY_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_ACCOUNT_MANAGER_ACCOUNT_MANAGER_POLICY_CONTROLLER_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/components/account_manager/account_manager.h"
#include "components/account_id/account_id.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"

class Profile;

namespace chromeos {

class AccountManager;

class AccountManagerPolicyController : public KeyedService {
 public:
  AccountManagerPolicyController(Profile* profile,
                                 AccountManager* account_manager,
                                 const AccountId& device_account_id);
  ~AccountManagerPolicyController() override;

  // Starts applying the behaviour required by |chromeos::AccountManager|
  // specific prefs and policies.
  void Start();

 private:
  // Callback handler for |AccountManager::GetAccounts|.
  void OnGetAccounts(const std::vector<AccountManager::Account>&);

  // Callback for handling changes in |kSecondaryGoogleAccountSigninAllowed|
  // pref.
  void OnPrefChanged();

  // Non-owning pointers.
  Profile* const profile_;
  AccountManager* const account_manager_;

  const AccountId device_account_id_;

  // For listening on Pref changes.
  PrefChangeRegistrar pref_change_registrar_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<AccountManagerPolicyController> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AccountManagerPolicyController);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ACCOUNT_MANAGER_ACCOUNT_MANAGER_POLICY_CONTROLLER_H_
