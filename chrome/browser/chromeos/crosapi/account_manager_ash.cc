// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crosapi/account_manager_ash.h"

#include <utility>

#include "base/callback.h"
#include "base/notreached.h"
#include "components/account_manager_core/account_manager_util.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace crosapi {

AccountManagerAsh::AccountManagerAsh(chromeos::AccountManager* account_manager)
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
    const account_manager::Account& account) {
  for (auto& observer : observers_)
    observer->OnTokenUpserted(ToMojoAccount(account));
}

void AccountManagerAsh::OnAccountRemoved(
    const account_manager::Account& account) {
  for (auto& observer : observers_)
    observer->OnAccountRemoved(ToMojoAccount(account));
}

void AccountManagerAsh::FlushMojoForTesting() {
  observers_.FlushForTesting();
}

}  // namespace crosapi
