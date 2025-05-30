// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/fake_sync_signin_delegate_android.h"

#include "base/notreached.h"
#include "chrome/browser/sync/test/integration/sync_test_utils_android.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"

bool FakeSyncSigninDelegateAndroid::SignIn(const std::string& username,
                                           const std::string& password,
                                           signin::ConsentLevel consent_level) {
  sync_test_utils_android::SetUpFakeAccountAndSignInForTesting(username,
                                                               consent_level);
  return true;
}

bool FakeSyncSigninDelegateAndroid::ConfirmSync() {
  // Nothing to do: the fake sign-in used by this delegate doesn't require any
  // further steps beyond what SyncServiceImplHarness already does.
  return true;
}

void FakeSyncSigninDelegateAndroid::SignOut() {
  sync_test_utils_android::SignOutForTesting();
}

GaiaId FakeSyncSigninDelegateAndroid::GetGaiaIdForUsername(
    const std::string& username) {
  return signin::GetTestGaiaIdForEmail(username);
}
