// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/live_sync_signin_delegate_android.h"

#include "base/command_line.h"
#include "base/notreached.h"
#include "chrome/browser/sync/test/integration/sync_test_utils_android.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"

namespace {

// Command-line switches used for live tests.
constexpr char kSyncUserForTest[] = "sync-user-for-test";
constexpr char kSyncPasswordForTest[] = "sync-password-for-test";

}  // namespace

bool LiveSyncSigninDelegateAndroid::SignIn(SyncTestAccount account,
                                           signin::ConsentLevel consent_level) {
  if (account != SyncTestAccount::kDefaultAccount) {
    LOG(ERROR) << "Live tests only support one account";
    return false;
  }

  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  const std::string username = cl->GetSwitchValueASCII(kSyncUserForTest);
  const std::string password = cl->GetSwitchValueASCII(kSyncPasswordForTest);
  if (username.empty() || password.empty()) {
    LOG(ERROR) << "Cannot run live sync tests without GAIA credentials.";
    return false;
  }

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

GaiaId LiveSyncSigninDelegateAndroid::GetGaiaIdForAccount(
    SyncTestAccount account) {
  NOTREACHED() << "Not supported";
}

std::string LiveSyncSigninDelegateAndroid::GetEmailForAccount(
    SyncTestAccount account) {
  CHECK_EQ(account, SyncTestAccount::kDefaultAccount)
      << "Only one account is supported in live tests";

  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  return cl->GetSwitchValueASCII(kSyncUserForTest);
}
