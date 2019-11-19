// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_SIGNIN_NOTIFIER_IMPL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_SIGNIN_NOTIFIER_IMPL_H_

#include "base/macros.h"
#include "components/password_manager/core/browser/password_store_signin_notifier.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace password_manager {

// Responsible for subscribing to Chrome sign-in events and passing them to
// PasswordStore.
class PasswordStoreSigninNotifierImpl
    : public PasswordStoreSigninNotifier,
      public signin::IdentityManager::Observer {
 public:
  explicit PasswordStoreSigninNotifierImpl(
      signin::IdentityManager* identity_manager);
  ~PasswordStoreSigninNotifierImpl() override;

  // PasswordStoreSigninNotifier implementations.
  void SubscribeToSigninEvents(PasswordStore* store) override;
  void UnsubscribeFromSigninEvents() override;

  // IdentityManager::Observer implementations.
  void OnPrimaryAccountCleared(const CoreAccountInfo& account_info) override;
  void OnExtendedAccountInfoRemoved(const AccountInfo& info) override;

 private:
  signin::IdentityManager* identity_manager_ = nullptr;
};

}  // namespace password_manager
#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_SIGNIN_NOTIFIER_IMPL_H_
