// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_SIGNIN_DELEGATE_DESKTOP_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_SIGNIN_DELEGATE_DESKTOP_H_

#include "chrome/browser/sync/test/integration/sync_signin_delegate.h"

// Delegate for desktop sign-in.
class SyncSigninDelegateDesktop : public SyncSigninDelegate {
 public:
  SyncSigninDelegateDesktop() = default;
  ~SyncSigninDelegateDesktop() override = default;

  SyncSigninDelegateDesktop(SyncSigninDelegateDesktop&&) = delete;
  SyncSigninDelegateDesktop(const SyncSigninDelegateDesktop&) = delete;

  // SyncSigninDelegate:
  void SigninFake(Profile* profile, const std::string& username) override;
  bool SigninUI(Profile* profile,
                const std::string& username,
                const std::string& password) override;
  bool ConfirmSigninUI(Profile* profile) override;
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_SIGNIN_DELEGATE_DESKTOP_H_
