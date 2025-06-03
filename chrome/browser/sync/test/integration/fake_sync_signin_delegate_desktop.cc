// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/fake_sync_signin_delegate_desktop.h"

#include <optional>

#include "base/check_deref.h"
#include "base/notreached.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/signin_constants.h"

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

FakeSyncSigninDelegateDesktop::FakeSyncSigninDelegateDesktop(Profile* profile)
    : profile_(CHECK_DEREF(profile).GetWeakPtr()) {}

FakeSyncSigninDelegateDesktop::~FakeSyncSigninDelegateDesktop() = default;

bool FakeSyncSigninDelegateDesktop::SignIn(SyncTestAccount account,
                                           signin::ConsentLevel consent_level) {
  CHECK(GetEmailForAccount(account).ends_with(
      GetHostedDomain(account).value_or("@gmail.com")));

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_.get());

  const std::string username = GetEmailForAccount(account);

  // Verify HasPrimaryAccount() separately because MakePrimaryAccountAvailable()
  // below DCHECK fails if there is already an authenticated account.
  if (identity_manager->HasPrimaryAccount(consent_level)) {
    CHECK_EQ(identity_manager->GetPrimaryAccountInfo(consent_level).email,
             username);
    // Don't update the refresh token if we already have one. The reason is
    // that doing so causes Sync (ServerConnectionManager in particular) to
    // mark the current access token as invalid. Since tests typically
    // always hand out the same access token string, any new access token
    // acquired later would also be considered invalid.
    if (!identity_manager->HasPrimaryAccountWithRefreshToken(consent_level)) {
      signin::SetRefreshTokenForPrimaryAccount(identity_manager);
    }
  } else if (identity_manager->HasPrimaryAccount(
                 signin::ConsentLevel::kSignin)) {
    // Handle kSignin->kSync transitions.
    CHECK_EQ(consent_level, signin::ConsentLevel::kSync);
    CHECK_EQ(
        identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
            .email,
        username);
    signin::AccountAvailabilityOptionsBuilder options =
        signin::AccountAvailabilityOptionsBuilder().AsPrimary(consent_level);

    // Similarly to the above: if there is a primary account already with a
    // refresh token, and this is about upgrading to ConsentLevel::kSync, avoid
    // setting a new refresh token. Otherwise access token requests may fail.
    if (identity_manager->HasPrimaryAccountWithRefreshToken(
            signin::ConsentLevel::kSignin)) {
      options.WithoutRefreshToken();
    }

    signin::MakeAccountAvailable(identity_manager, options.Build(username));
  } else {
    // There is no primary account previously, so mimic a new sign-in.
    const CoreAccountInfo account_info = signin::MakePrimaryAccountAvailable(
        identity_manager, username, consent_level);

    signin::SimulateSuccessfulFetchOfAccountInfo(
        identity_manager, account_info.account_id, account_info.email,
        account_info.gaia,
        GetHostedDomain(account).value_or(
            signin::constants::kNoHostedDomainFound),
        "Full Name", "Given Name", "en-US", "");
  }

  CHECK(identity_manager->HasPrimaryAccount(consent_level));
  CHECK(identity_manager->HasPrimaryAccountWithRefreshToken(consent_level));
  return true;
}

bool FakeSyncSigninDelegateDesktop::ConfirmSync() {
  // Nothing to do: the fake sign-in used by this delegate doesn't require any
  // further steps beyond what SyncServiceImplHarness already does.
  return true;
}

void FakeSyncSigninDelegateDesktop::SignOut() {
  signin::ClearPrimaryAccount(
      IdentityManagerFactory::GetForProfile(profile_.get()));
}

GaiaId FakeSyncSigninDelegateDesktop::GetGaiaIdForAccount(
    SyncTestAccount account) {
  return signin::GetTestGaiaIdForEmail(GetEmailForAccount(account));
}

std::string FakeSyncSigninDelegateDesktop::GetEmailForAccount(
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
