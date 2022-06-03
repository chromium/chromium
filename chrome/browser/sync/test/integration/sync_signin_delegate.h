// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_SIGNIN_DELEGATE_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_SIGNIN_DELEGATE_H_

#include <string>

class Profile;

// Delegate to sign-in test accounts for Sync testing across platforms.
class SyncSigninDelegate {
 public:
  virtual ~SyncSigninDelegate() = default;

  // Signs in a fake account.
  virtual void SigninFake(Profile* profile, const std::string& username) = 0;

  // Signs in a real account via the actual UI, for use in end-to-end tests
  // using real servers.
  virtual bool SigninUI(Profile* profile,
                        const std::string& username,
                        const std::string& password) = 0;

  // Confirms the sign-in previously triggered via SigninUI.
  virtual bool ConfirmSigninUI(Profile* profile) = 0;
};

// Creates the platform-specific implementation of SyncSigninDelegate.
std::unique_ptr<SyncSigninDelegate> CreateSyncSigninDelegate();

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_SIGNIN_DELEGATE_H_
