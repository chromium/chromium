// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_SIGNIN_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_SIGNIN_DELEGATE_ANDROID_H_

#include "chrome/browser/sync/test/integration/sync_signin_delegate.h"

// Delegate for Android sign-in.
class SyncSigninDelegateAndroid : public SyncSigninDelegate {
 public:
  SyncSigninDelegateAndroid() = default;
  ~SyncSigninDelegateAndroid() override = default;

  SyncSigninDelegateAndroid(SyncSigninDelegateAndroid&&) = delete;
  SyncSigninDelegateAndroid(const SyncSigninDelegateAndroid&) = delete;

  // SyncSigninDelegate:
  void SigninFake(Profile* profile,
                  const std::string& username,
                  signin::ConsentLevel consent_level) override;
  bool SigninUI(Profile* profile,
                const std::string& username,
                const std::string& password,
                signin::ConsentLevel consent_level) override;
  bool ConfirmSyncUI(Profile* profile) override;
  void SignOutPrimaryAccount(Profile* profile) override;
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_SIGNIN_DELEGATE_ANDROID_H_
