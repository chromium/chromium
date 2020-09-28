// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crosapi/account_manager_ash.h"

#include <utility>

#include "chromeos/components/account_manager/account_manager.h"

namespace crosapi {

AccountManagerAsh::AccountManagerAsh(
    chromeos::AccountManager* account_manager,
    mojo::PendingReceiver<mojom::AccountManager> receiver)
    : account_manager_(account_manager), receiver_(this, std::move(receiver)) {
  DCHECK(account_manager_);
}

AccountManagerAsh::~AccountManagerAsh() = default;

void AccountManagerAsh::IsInitialized(IsInitializedCallback callback) {
  std::move(callback).Run(account_manager_->IsInitialized());
}

}  // namespace crosapi
