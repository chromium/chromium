// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/sync_signin_delegate_android.h"

#include "base/notreached.h"
#include "chrome/browser/sync/test/integration/sync_test_utils_android.h"

void SyncSigninDelegateAndroid::SigninFake(Profile* profile,
                                           const std::string& username,
                                           signin::ConsentLevel consent_level) {
  switch (consent_level) {
    case signin::ConsentLevel::kSignin:
      sync_test_utils_android::SetUpFakeAccountAndSignInForTesting(username);
      return;
    case signin::ConsentLevel::kSync:
      // TODO(crbug.com/1117345,crbug.com/40165479): Ideally (for consistency
      // with desktop), this should sign in an account with ConsentLevel::kSync,
      // but *not* actually enable Sync-the-feature.
      sync_test_utils_android::SetUpFakeAccountAndSignInAndEnableSyncForTesting(
          username);
      return;
  }
  NOTREACHED();
}

bool SyncSigninDelegateAndroid::SigninUI(Profile* profile,
                                         const std::string& username,
                                         const std::string& password,
                                         signin::ConsentLevel consent_level) {
  switch (consent_level) {
    case signin::ConsentLevel::kSignin:
      sync_test_utils_android::SetUpLiveAccountAndSignInForTesting(username,
                                                                   password);
      return true;
    case signin::ConsentLevel::kSync:
      sync_test_utils_android::SetUpLiveAccountAndSignInAndEnableSyncForTesting(
          username, password);
      return true;
  }
  NOTREACHED();
}

bool SyncSigninDelegateAndroid::ConfirmSyncUI(Profile* profile) {
  return true;
}

void SyncSigninDelegateAndroid::SignOutPrimaryAccount(Profile* profile) {
  sync_test_utils_android::SignOutForTesting();
}
