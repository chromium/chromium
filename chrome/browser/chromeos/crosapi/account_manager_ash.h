// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSAPI_ACCOUNT_MANAGER_ASH_H_
#define CHROME_BROWSER_CHROMEOS_CROSAPI_ACCOUNT_MANAGER_ASH_H_

#include "chromeos/components/account_manager/account_manager.h"
#include "chromeos/crosapi/mojom/account_manager.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace crosapi {

// Implements the |crosapi::mojom::AccountManager| interface in ash-chrome.
// It enables lacros-chrome to interact with accounts stored in the Chrome OS
// Account Manager.
class AccountManagerAsh : public mojom::AccountManager,
                          public chromeos::AccountManager::Observer {
 public:
  AccountManagerAsh(chromeos::AccountManager* account_manager,
                    mojo::PendingReceiver<mojom::AccountManager> receiver);
  AccountManagerAsh(const AccountManagerAsh&) = delete;
  AccountManagerAsh& operator=(const AccountManagerAsh&) = delete;
  ~AccountManagerAsh() override;

  // crosapi::mojom::AccountManager:
  void IsInitialized(IsInitializedCallback callback) override;
  void AddObserver(AddObserverCallback callback) override;

  // chromeos::AccountManager::Observer:
  void OnTokenUpserted(
      const chromeos::AccountManager::Account& account) override;
  void OnAccountRemoved(
      const chromeos::AccountManager::Account& account) override;

 private:
  friend class AccountManagerAshTest;
  friend class TestAccountManagerObserver;

  // Following util functions are static for ease of testing.
  static chromeos::AccountManager::Account FromMojoAccount(
      const mojom::AccountPtr& mojom_account);
  static mojom::AccountPtr ToMojoAccount(
      const chromeos::AccountManager::Account& account);
  static chromeos::account_manager::AccountType FromMojoAccountType(
      const mojom::AccountType& account_type);
  static mojom::AccountType ToMojoAccountType(
      const chromeos::account_manager::AccountType& account_type);

  void FlushMojoForTesting();

  chromeos::AccountManager* const account_manager_;
  mojo::Receiver<mojom::AccountManager> receiver_;
  mojo::RemoteSet<mojom::AccountManagerObserver> observers_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_CHROMEOS_CROSAPI_ACCOUNT_MANAGER_ASH_H_
