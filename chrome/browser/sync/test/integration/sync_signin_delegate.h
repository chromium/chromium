// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_SIGNIN_DELEGATE_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_SIGNIN_DELEGATE_H_

#include <memory>
#include <string>

#include "components/signin/public/base/consent_level.h"

class Profile;

// Delegate to sign-in test accounts for Sync testing across platforms.
class SyncSigninDelegate {
 public:
  virtual ~SyncSigninDelegate() = default;

  // Signs in a fake account.
  virtual void SigninFake(Profile* profile,
                          const std::string& username,
                          signin::ConsentLevel consent_level) = 0;

  // Signs in a real account via the actual UI, for use in end-to-end tests
  // using real servers.
  [[nodiscard]] virtual bool SigninUI(Profile* profile,
                                      const std::string& username,
                                      const std::string& password,
                                      signin::ConsentLevel consent_level) = 0;

  // Confirms the Sync opt-in previously triggered via SigninUI(kSync).
  [[nodiscard]] virtual bool ConfirmSyncUI(Profile* profile) = 0;

  // Signs out and clears the primary account.
  virtual void SignOutPrimaryAccount(Profile* profile) = 0;
};

// Creates the platform-specific implementation of SyncSigninDelegate.
std::unique_ptr<SyncSigninDelegate> CreateSyncSigninDelegate();

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_SIGNIN_DELEGATE_H_
