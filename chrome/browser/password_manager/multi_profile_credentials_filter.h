// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_MULTI_PROFILE_CREDENTIALS_FILTER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_MULTI_PROFILE_CREDENTIALS_FILTER_H_

#include "base/memory/raw_ptr.h"
#include "components/password_manager/core/browser/sync_credentials_filter.h"

class DiceWebSigninInterceptor;

// SyncCredentialsFilter that also supports interaction with other profiles.
// The "Save Password" bubble is suppressed when a multi-profile promo is shown.
class MultiProfileCredentialsFilter
    : public password_manager::SyncCredentialsFilter {
 public:
  // dice_web_signin_interceptor is allowed to be null if there is no signin
  // interception.
  MultiProfileCredentialsFilter(
      password_manager::PasswordManagerClient* client,
      DiceWebSigninInterceptor* dice_web_signin_interceptor);

  MultiProfileCredentialsFilter(const MultiProfileCredentialsFilter&) = delete;
  MultiProfileCredentialsFilter& operator==(
      const MultiProfileCredentialsFilter&) = delete;

  // password_manager::SyncCredentialsFilter
  bool ShouldSave(const password_manager::PasswordForm& form) const override;

 private:
  const raw_ptr<const DiceWebSigninInterceptor> dice_web_signin_interceptor_;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_MULTI_PROFILE_CREDENTIALS_FILTER_H_
