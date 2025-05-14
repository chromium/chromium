// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/sync_signin_delegate_android.h"

#include "base/notreached.h"
#include "chrome/browser/sync/test/integration/sync_test_utils_android.h"

void SyncSigninDelegateAndroid::SigninFake(Profile* profile,
                                           const std::string& username,
                                           signin::ConsentLevel consent_level) {
  sync_test_utils_android::SetUpFakeAccountAndSignInForTesting(username,
                                                               consent_level);
}

bool SyncSigninDelegateAndroid::SigninUI(Profile* profile,
                                         const std::string& username,
                                         const std::string& password,
                                         signin::ConsentLevel consent_level) {
  sync_test_utils_android::SetUpLiveAccountAndSignInForTesting(
      username, password, consent_level);
  return true;
}

bool SyncSigninDelegateAndroid::ConfirmSyncUI(Profile* profile) {
  return true;
}

void SyncSigninDelegateAndroid::SignOutPrimaryAccount(Profile* profile) {
  sync_test_utils_android::SignOutForTesting();
}
