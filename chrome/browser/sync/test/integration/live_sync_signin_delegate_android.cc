// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/live_sync_signin_delegate_android.h"

#include "base/notreached.h"
#include "chrome/browser/sync/test/integration/sync_test_utils_android.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"

bool LiveSyncSigninDelegateAndroid::SignIn(const std::string& username,
                                           const std::string& password,
                                           signin::ConsentLevel consent_level) {
  sync_test_utils_android::SetUpLiveAccountAndSignInForTesting(
      username, password, consent_level);
  return true;
}

bool LiveSyncSigninDelegateAndroid::ConfirmSync() {
  // Nothing to do: SetUpLiveAccountAndSignInForTesting(kSync) requires no
  // further interactions with the UI.
  return true;
}

void LiveSyncSigninDelegateAndroid::SignOut() {
  sync_test_utils_android::SignOutForTesting();
}

GaiaId LiveSyncSigninDelegateAndroid::GetGaiaIdForUsername(
    const std::string& username) {
  NOTREACHED() << "Not supported";
}
