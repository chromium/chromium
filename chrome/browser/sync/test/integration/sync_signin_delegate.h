// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_SIGNIN_DELEGATE_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_SIGNIN_DELEGATE_H_

#include <memory>
#include <string>

#include "chrome/browser/sync/test/integration/sync_test_account.h"
#include "components/signin/public/base/consent_level.h"
#include "google_apis/gaia/gaia_id.h"

class Profile;

// Delegate to sign-in test accounts for Sync testing across platforms.
class SyncSigninDelegate {
 public:
  virtual ~SyncSigninDelegate() = default;

  // Signs in a primary account.
  [[nodiscard]] virtual bool SignIn(SyncTestAccount account,
                                    signin::ConsentLevel consent_level) = 0;

  // Confirms the Sync opt-in previously triggered via SignIn(kSync).
  [[nodiscard]] virtual bool ConfirmSync() = 0;

  // Signs out and clears the primary account.
  virtual void SignOut() = 0;

  // Returns the gaia ID corresponding to `account`, regardless of the current
  // sign-in state. Note that not all delegates support this.
  virtual GaiaId GetGaiaIdForAccount(SyncTestAccount account) = 0;

  // Returns the email corresponding to `account`, regardless of the current
  // sign-in state. Note that not all delegates support this.
  virtual std::string GetEmailForAccount(SyncTestAccount account) = 0;
};

// Creates the platform-specific implementation of SyncSigninDelegate.
// `profile` must not be null and must outlive the returned object.
// The function is offered in two variants: one that uses a fake sign-in
// and one that actually signs in to a real server.
std::unique_ptr<SyncSigninDelegate> CreateSyncSigninDelegateWithFakeSignin(
    Profile* profile);
std::unique_ptr<SyncSigninDelegate> CreateSyncSigninDelegateWithLiveSignin(
    Profile* profile);

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_SIGNIN_DELEGATE_H_
