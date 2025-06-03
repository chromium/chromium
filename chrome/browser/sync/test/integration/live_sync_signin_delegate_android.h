// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_LIVE_SYNC_SIGNIN_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_LIVE_SYNC_SIGNIN_DELEGATE_ANDROID_H_

#include "chrome/browser/sync/test/integration/sync_signin_delegate.h"

// Delegate for Android sign-in using real servers.
class LiveSyncSigninDelegateAndroid : public SyncSigninDelegate {
 public:
  LiveSyncSigninDelegateAndroid() = default;
  ~LiveSyncSigninDelegateAndroid() override = default;

  LiveSyncSigninDelegateAndroid(LiveSyncSigninDelegateAndroid&&) = delete;
  LiveSyncSigninDelegateAndroid(const LiveSyncSigninDelegateAndroid&) = delete;

  // SyncSigninDelegate:
  bool SignIn(SyncTestAccount account,
              signin::ConsentLevel consent_level) override;
  bool ConfirmSync() override;
  void SignOut() override;
  GaiaId GetGaiaIdForAccount(SyncTestAccount account) override;
  std::string GetEmailForAccount(SyncTestAccount account) override;
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_LIVE_SYNC_SIGNIN_DELEGATE_ANDROID_H_
