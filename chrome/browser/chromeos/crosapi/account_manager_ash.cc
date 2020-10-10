// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crosapi/account_manager_ash.h"

#include <utility>

#include "base/callback.h"
#include "base/notreached.h"
#include "chromeos/components/account_manager/account_manager.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace crosapi {

AccountManagerAsh::AccountManagerAsh(
    chromeos::AccountManager* account_manager,
    mojo::PendingReceiver<mojom::AccountManager> receiver)
    : account_manager_(account_manager), receiver_(this, std::move(receiver)) {
  DCHECK(account_manager_);
  account_manager_->AddObserver(this);
}

AccountManagerAsh::~AccountManagerAsh() {
  account_manager_->RemoveObserver(this);
}

void AccountManagerAsh::IsInitialized(IsInitializedCallback callback) {
  std::move(callback).Run(account_manager_->IsInitialized());
}

void AccountManagerAsh::AddObserver(AddObserverCallback callback) {
  mojo::Remote<mojom::AccountManagerObserver> remote;
  auto receiver = remote.BindNewPipeAndPassReceiver();
  observers_.Add(std::move(remote));
  std::move(callback).Run(std::move(receiver));
}

void AccountManagerAsh::OnTokenUpserted(
    const chromeos::AccountManager::Account& account) {
  for (auto& observer : observers_)
    observer->OnTokenUpserted(ToMojoAccount(account));
}

void AccountManagerAsh::OnAccountRemoved(
    const chromeos::AccountManager::Account& account) {
  for (auto& observer : observers_)
    observer->OnAccountRemoved(ToMojoAccount(account));
}

// static
chromeos::AccountManager::Account AccountManagerAsh::FromMojoAccount(
    const mojom::AccountPtr& mojom_account) {
  chromeos::AccountManager::Account account;

  account.key.id = mojom_account->key->id;
  account.key.account_type =
      FromMojoAccountType(mojom_account->key->account_type);
  account.raw_email = mojom_account->raw_email;

  return account;
}

// static
mojom::AccountPtr AccountManagerAsh::ToMojoAccount(
    const chromeos::AccountManager::Account& account) {
  mojom::AccountPtr mojom_account = mojom::Account::New();

  mojom_account->key = mojom::AccountKey::New();
  mojom_account->key->id = account.key.id;
  mojom_account->key->account_type =
      ToMojoAccountType(account.key.account_type);
  mojom_account->raw_email = account.raw_email;

  return mojom_account;
}

// static
chromeos::account_manager::AccountType AccountManagerAsh::FromMojoAccountType(
    const mojom::AccountType& account_type) {
  switch (account_type) {
    case mojom::AccountType::kUnspecified:
      return chromeos::account_manager::AccountType::ACCOUNT_TYPE_UNSPECIFIED;
    case mojom::AccountType::kGaia:
      return chromeos::account_manager::AccountType::ACCOUNT_TYPE_GAIA;
    case mojom::AccountType::kActiveDirectory:
      return chromeos::account_manager::AccountType::
          ACCOUNT_TYPE_ACTIVE_DIRECTORY;
    default:
      // Ash does not know about this new account type. Don't consider this as
      // as error to preserve forwards compatibility with lacros.
      LOG(WARNING) << "Unknown account type: " << account_type;
      return chromeos::account_manager::AccountType::ACCOUNT_TYPE_UNSPECIFIED;
  }
}

// static
mojom::AccountType AccountManagerAsh::ToMojoAccountType(
    const chromeos::account_manager::AccountType& account_type) {
  switch (account_type) {
    case chromeos::account_manager::AccountType::ACCOUNT_TYPE_UNSPECIFIED:
      return mojom::AccountType::kUnspecified;
    case chromeos::account_manager::AccountType::ACCOUNT_TYPE_GAIA:
      return mojom::AccountType::kGaia;
    case chromeos::account_manager::AccountType::ACCOUNT_TYPE_ACTIVE_DIRECTORY:
      return mojom::AccountType::kActiveDirectory;
  }
}

void AccountManagerAsh::FlushMojoForTesting() {
  observers_.FlushForTesting();
}

}  // namespace crosapi
