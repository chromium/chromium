// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_store_signin_notifier_impl.h"

#include "components/signin/public/identity_manager/identity_manager.h"

namespace password_manager {

PasswordStoreSigninNotifierImpl::PasswordStoreSigninNotifierImpl(
    signin::IdentityManager* identity_manager)
    : identity_manager_(identity_manager) {
  DCHECK(identity_manager_);
}

PasswordStoreSigninNotifierImpl::~PasswordStoreSigninNotifierImpl() {}

void PasswordStoreSigninNotifierImpl::SubscribeToSigninEvents(
    PasswordStore* store) {
  set_store(store);
  identity_manager_->AddObserver(this);
}

void PasswordStoreSigninNotifierImpl::UnsubscribeFromSigninEvents() {
  identity_manager_->RemoveObserver(this);
}

void PasswordStoreSigninNotifierImpl::OnPrimaryAccountCleared(
    const CoreAccountInfo& account_info) {
  NotifySignedOut(account_info.email, /* primary_account= */ true);
}

// IdentityManager::Observer implementations.
void PasswordStoreSigninNotifierImpl::OnExtendedAccountInfoRemoved(
    const AccountInfo& info) {
  // Only reacts to content area (non-primary) Gaia account sign-out event.
  if (info.account_id != identity_manager_->GetPrimaryAccountId()) {
    NotifySignedOut(info.email, /* primary_account= */ false);
  }
}

}  // namespace password_manager
