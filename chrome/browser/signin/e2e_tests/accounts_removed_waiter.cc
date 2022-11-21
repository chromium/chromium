// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "accounts_removed_waiter.h"

namespace signin::test {

AccountsRemovedWaiter::AccountsRemovedWaiter(
    signin::IdentityManager* identity_manager)
    : identity_manager_(identity_manager) {
  DCHECK(identity_manager_);
}

AccountsRemovedWaiter::~AccountsRemovedWaiter() = default;

void AccountsRemovedWaiter::Wait() {
  if (identity_manager_->GetAccountsWithRefreshTokens().empty())
    return;
  observation_.Observe(identity_manager_.get());
  run_loop_.Run();
}

void AccountsRemovedWaiter::OnRefreshTokenRemovedForAccount(
    const CoreAccountId& account_id) {
  if (!identity_manager_->GetAccountsWithRefreshTokens().empty())
    return;
  observation_.Reset();
  run_loop_.Quit();
}

}  // namespace signin::test
