// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/account_manager/account_manager_ash.h"

#include <utility>

#include "ash/components/account_manager/account_manager.h"
#include "ash/components/account_manager/account_manager_ui.h"
#include "base/callback.h"
#include "base/notreached.h"
#include "components/account_manager_core/account_addition_result.h"
#include "components/account_manager_core/account_manager_util.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace {

void MarshalAccounts(
    crosapi::mojom::AccountManager::GetAccountsCallback callback,
    const std::vector<account_manager::Account>& accounts_to_marshal) {
  std::vector<crosapi::mojom::AccountPtr> mojo_accounts;
  for (const account_manager::Account& account : accounts_to_marshal) {
    mojo_accounts.emplace_back(account_manager::ToMojoAccount(account));
  }
}

}  // namespace

namespace crosapi {

AccountManagerAsh::AccountManagerAsh(ash::AccountManager* account_manager)
    : account_manager_(account_manager) {
  DCHECK(account_manager_);
  account_manager_->AddObserver(this);
}

AccountManagerAsh::~AccountManagerAsh() {
  account_manager_->RemoveObserver(this);
}

void AccountManagerAsh::BindReceiver(
    mojo::PendingReceiver<mojom::AccountManager> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void AccountManagerAsh::SetAccountManagerUI(
    std::unique_ptr<ash::AccountManagerUI> account_manager_ui) {
  account_manager_ui_ = std::move(account_manager_ui);
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

void AccountManagerAsh::GetAccounts(
    mojom::AccountManager::GetAccountsCallback callback) {
  account_manager_->GetAccounts(
      base::BindOnce(&MarshalAccounts, std::move(callback)));
}

void AccountManagerAsh::ShowAddAccountDialog(
    ShowAddAccountDialogCallback callback) {
  DCHECK(account_manager_ui_);
  if (account_manager_ui_->IsDialogShown()) {
    std::move(callback).Run(
        ToMojoAccountAdditionResult(account_manager::AccountAdditionResult(
            account_manager::AccountAdditionResult::Status::
                kAlreadyInProgress)));
    return;
  }

  DCHECK(!account_addition_in_progress_);
  account_addition_in_progress_ = true;
  account_addition_callback_ = std::move(callback);
  account_manager_ui_->ShowAddAccountDialog(
      base::BindOnce(&AccountManagerAsh::OnAddAccountDialogClosed,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AccountManagerAsh::ShowReauthAccountDialog(const std::string& email,
                                                base::OnceClosure closure) {
  DCHECK(account_manager_ui_);
  if (account_manager_ui_->IsDialogShown())
    return;

  account_manager_ui_->ShowReauthAccountDialog(email, std::move(closure));
}

void AccountManagerAsh::ShowManageAccountsSettings() {
  account_manager_ui_->ShowManageAccountsSettings();
}

void AccountManagerAsh::OnTokenUpserted(
    const account_manager::Account& account) {
  for (auto& observer : observers_)
    observer->OnTokenUpserted(ToMojoAccount(account));
}

void AccountManagerAsh::OnAccountRemoved(
    const account_manager::Account& account) {
  for (auto& observer : observers_)
    observer->OnAccountRemoved(ToMojoAccount(account));
}

void AccountManagerAsh::OnAccountAdditionFinished(
    const account_manager::AccountAdditionResult& result) {
  if (!account_addition_in_progress_)
    return;

  FinishAddAccount(result);
}

void AccountManagerAsh::OnAddAccountDialogClosed() {
  if (!account_addition_in_progress_)
    return;

  // Account addition is still in progress. It means that user didn't complete
  // the account addition flow and closed the dialog.
  FinishAddAccount(account_manager::AccountAdditionResult(
      account_manager::AccountAdditionResult::Status::kCancelledByUser));
}

void AccountManagerAsh::FinishAddAccount(
    const account_manager::AccountAdditionResult& result) {
  account_addition_in_progress_ = false;

  DCHECK(!account_addition_callback_.is_null());
  std::move(account_addition_callback_)
      .Run(ToMojoAccountAdditionResult(result));
}

void AccountManagerAsh::FlushMojoForTesting() {
  observers_.FlushForTesting();
}

}  // namespace crosapi
