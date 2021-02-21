// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ACCOUNT_MANAGER_ACCOUNT_MANAGER_ASH_H_
#define ASH_COMPONENTS_ACCOUNT_MANAGER_ACCOUNT_MANAGER_ASH_H_

#include "ash/components/account_manager/account_manager.h"
#include "ash/components/account_manager/account_manager_ui.h"
#include "base/callback_forward.h"
#include "base/optional.h"
#include "chromeos/crosapi/mojom/account_manager.mojom.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_addition_result.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace chromeos {
class SigninHelper;
}  // namespace chromeos

namespace crosapi {

// Implements the |crosapi::mojom::AccountManager| interface in ash-chrome.
// It enables lacros-chrome to interact with accounts stored in the Chrome OS
// Account Manager.
class COMPONENT_EXPORT(ASH_COMPONENTS_ACCOUNT_MANAGER) AccountManagerAsh
    : public mojom::AccountManager,
      public ash::AccountManager::Observer {
 public:
  explicit AccountManagerAsh(ash::AccountManager* account_manager);
  AccountManagerAsh(const AccountManagerAsh&) = delete;
  AccountManagerAsh& operator=(const AccountManagerAsh&) = delete;
  ~AccountManagerAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::AccountManager> receiver);

  void SetAccountManagerUI(
      std::unique_ptr<ash::AccountManagerUI> account_manager_ui);

  // crosapi::mojom::AccountManager:
  void IsInitialized(IsInitializedCallback callback) override;
  void AddObserver(AddObserverCallback callback) override;
  void GetAccounts(GetAccountsCallback callback) override;
  void ShowAddAccountDialog(ShowAddAccountDialogCallback callback) override;
  void ShowReauthAccountDialog(const std::string& email,
                               base::OnceClosure closure) override;
  void ShowManageAccountsSettings() override;

  // ash::AccountManager::Observer:
  void OnTokenUpserted(const account_manager::Account& account) override;
  void OnAccountRemoved(const account_manager::Account& account) override;

 private:
  friend class AccountManagerAshTest;
  friend class TestAccountManagerObserver;
  friend class AccountManagerFacadeAshTest;
  friend class chromeos::SigninHelper;

  // This method is called by `chromeos::SigninHelper` which passes `AccountKey`
  // of account that was added.
  void OnAccountAdditionFinished(
      const account_manager::AccountAdditionResult& result);
  // A callback for `AccountManagerUI::ShowAccountAdditionDialog`.
  void OnAddAccountDialogClosed();
  void FinishAddAccount(const account_manager::AccountAdditionResult& result);
  void FlushMojoForTesting();

  ShowAddAccountDialogCallback account_addition_callback_;
  bool account_addition_in_progress_ = false;
  ash::AccountManager* const account_manager_;
  mojo::ReceiverSet<mojom::AccountManager> receivers_;
  mojo::RemoteSet<mojom::AccountManagerObserver> observers_;
  std::unique_ptr<ash::AccountManagerUI> account_manager_ui_;

  base::WeakPtrFactory<AccountManagerAsh> weak_ptr_factory_{this};
};

}  // namespace crosapi

#endif  // ASH_COMPONENTS_ACCOUNT_MANAGER_ACCOUNT_MANAGER_ASH_H_
