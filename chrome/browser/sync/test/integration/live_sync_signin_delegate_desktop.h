// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_LIVE_SYNC_SIGNIN_DELEGATE_DESKTOP_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_LIVE_SYNC_SIGNIN_DELEGATE_DESKTOP_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/sync/test/integration/sync_signin_delegate.h"

// Delegate for desktop sign-in using real servers.
class LiveSyncSigninDelegateDesktop : public SyncSigninDelegate {
 public:
  explicit LiveSyncSigninDelegateDesktop(Profile* profile);
  ~LiveSyncSigninDelegateDesktop() override;

  LiveSyncSigninDelegateDesktop(LiveSyncSigninDelegateDesktop&&) = delete;
  LiveSyncSigninDelegateDesktop(const LiveSyncSigninDelegateDesktop&) = delete;

  // SyncSigninDelegate:
  bool SignIn(SyncTestAccount account,
              signin::ConsentLevel consent_level) override;
  bool ConfirmSync() override;
  void SignOut() override;
  GaiaId GetGaiaIdForAccount(SyncTestAccount account) override;
  std::string GetEmailForAccount(SyncTestAccount account) override;

 private:
  // WeakPtr is used to allow flexibility in tests: this object may outlive
  // `Profile` as long as it isn't exercised.
  const base::WeakPtr<Profile> profile_;
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_LIVE_SYNC_SIGNIN_DELEGATE_DESKTOP_H_
