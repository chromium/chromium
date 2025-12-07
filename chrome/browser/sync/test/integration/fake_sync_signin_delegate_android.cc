// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/fake_sync_signin_delegate_android.h"

#include <optional>

#include "base/notreached.h"
#include "chrome/browser/sync/test/integration/sync_test_utils_android.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"

namespace {

// A value other than nullopt means the account is managed.
std::optional<std::string> GetHostedDomain(SyncTestAccount account) {
  // Keep this in sync with `GetEmailForAccount()` below.
  switch (account) {
    case SyncTestAccount::kConsumerAccount1:
    case SyncTestAccount::kConsumerAccount2:
      return std::nullopt;
    case SyncTestAccount::kEnterpriseAccount1:
      return "managed-domain.com";
    case SyncTestAccount::kGoogleDotComAccount1:
      return "google.com";
  }
  NOTREACHED();
}

}  // namespace

bool FakeSyncSigninDelegateAndroid::SignIn(SyncTestAccount account,
                                           signin::ConsentLevel consent_level) {
  CHECK(GetEmailForAccount(account).ends_with(
      GetHostedDomain(account).value_or("@gmail.com")));

  sync_test_utils_android::SetUpFakeAccountAndSignInForTesting(
      GetEmailForAccount(account), GetHostedDomain(account), consent_level);
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

GaiaId FakeSyncSigninDelegateAndroid::GetGaiaIdForAccount(
    SyncTestAccount account) {
  return signin::GetTestGaiaIdForEmail(GetEmailForAccount(account));
}

std::string FakeSyncSigninDelegateAndroid::GetEmailForAccount(
    SyncTestAccount account) {
  switch (account) {
    case SyncTestAccount::kConsumerAccount1:
      return "user1@gmail.com";
    case SyncTestAccount::kConsumerAccount2:
      return "user2@gmail.com";
    case SyncTestAccount::kEnterpriseAccount1:
      return "user1@managed-domain.com";
    case SyncTestAccount::kGoogleDotComAccount1:
      return "user1@google.com";
  }
  NOTREACHED();
}
