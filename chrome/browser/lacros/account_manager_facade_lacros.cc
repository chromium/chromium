// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/account_manager_facade_lacros.h"

#include <memory>
#include <utility>

#include "base/bind.h"

namespace {

// Interface versions in //chromeos/crosapi/mojom/account_manager.mojom:
// MinVersion of crosapi::mojom::AccountManager::AddObserver
constexpr uint32_t kMinVersionWithObserver = 1;

}  // namespace

AccountManagerFacadeLacros::AccountManagerFacadeLacros(
    mojo::Remote<crosapi::mojom::AccountManager> account_manager_remote)
    : account_manager_remote_(std::move(account_manager_remote)) {
  if (!account_manager_remote_)
    return;

  account_manager_remote_.QueryVersion(base::BindOnce(
      &AccountManagerFacadeLacros::OnVersionCheck, weak_factory_.GetWeakPtr()));
}

AccountManagerFacadeLacros::~AccountManagerFacadeLacros() = default;

bool AccountManagerFacadeLacros::IsInitialized() {
  return is_initialized_;
}

void AccountManagerFacadeLacros::OnVersionCheck(uint32_t version) {
  if (version < kMinVersionWithObserver)
    return;

  account_manager_remote_->AddObserver(
      base::BindOnce(&AccountManagerFacadeLacros::OnReceiverReceived,
                     weak_factory_.GetWeakPtr()));
}

void AccountManagerFacadeLacros::OnReceiverReceived(
    mojo::PendingReceiver<AccountManagerObserver> receiver) {
  receiver_ =
      std::make_unique<mojo::Receiver<crosapi::mojom::AccountManagerObserver>>(
          this, std::move(receiver));
  // At this point (|receiver_| exists), we are subscribed to Account Manager.

  account_manager_remote_->IsInitialized(base::BindOnce(
      &AccountManagerFacadeLacros::OnInitialized, weak_factory_.GetWeakPtr()));
}
void AccountManagerFacadeLacros::OnInitialized(bool is_initialized) {
  if (is_initialized)
    is_initialized_ = true;
  // else: We will receive a notification in |OnTokenUpserted|.
}

void AccountManagerFacadeLacros::OnTokenUpserted(
    crosapi::mojom::AccountPtr account) {
  is_initialized_ = true;
}

void AccountManagerFacadeLacros::OnAccountRemoved(
    crosapi::mojom::AccountPtr account) {}
