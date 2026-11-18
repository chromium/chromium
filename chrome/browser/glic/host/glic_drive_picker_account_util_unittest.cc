// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_drive_picker_account_util.h"

#include <optional>
#include <string>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/list_accounts_test_utils.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_cookie_mutator.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/primary_account_change_event.h"
#include "components/signin/public/identity_manager/test_identity_manager_observer.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_id.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {
namespace {

using Error = DrivePickerAccountError;

class GlicDrivePickerAccountUtilTest : public testing::Test {
 protected:
  signin::IdentityManager* identity_manager() {
    return identity_env_.identity_manager();
  }

  AccountInfo SignInWithCookie(const std::string& email) {
    AccountInfo info = identity_env_.MakePrimaryAccountAvailable(
        email, signin::ConsentLevel::kSignin);
    identity_env_.SetCookieAccounts(
        {{std::string(info.GetEmail()), info.GetGaiaId()}});
    return info;
  }

  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  signin::IdentityTestEnvironment identity_env_{&test_url_loader_factory_};
};

TEST_F(GlicDrivePickerAccountUtilTest, NullIdentityManager) {
  EXPECT_EQ(CheckDrivePickerAccount(nullptr, GaiaId("some-gaia")),
            Error::kChromeSignInRequired);
}

TEST_F(GlicDrivePickerAccountUtilTest, NoPrimaryAccount) {
  EXPECT_EQ(CheckDrivePickerAccount(identity_manager(), GaiaId()),
            Error::kChromeSignInRequired);
  EXPECT_EQ(CheckDrivePickerAccount(identity_manager(), GaiaId("some-gaia")),
            Error::kChromeSignInRequired);
}

TEST_F(GlicDrivePickerAccountUtilTest, HealthyPrimaryAccount) {
  AccountInfo info = SignInWithCookie("user@gmail.com");
  EXPECT_EQ(CheckDrivePickerAccount(identity_manager(), info.GetGaiaId()),
            std::nullopt);
}

TEST_F(GlicDrivePickerAccountUtilTest, AccountMismatch) {
  SignInWithCookie("user@gmail.com");
  EXPECT_EQ(CheckDrivePickerAccount(identity_manager(), GaiaId("other-gaia")),
            Error::kAccountMismatch);
  EXPECT_EQ(CheckDrivePickerAccount(identity_manager(), GaiaId()),
            Error::kAccountMismatch);
}

TEST_F(GlicDrivePickerAccountUtilTest, MissingRefreshToken) {
  AccountInfo info = SignInWithCookie("user@gmail.com");
  identity_env_.RemoveRefreshTokenForPrimaryAccount();
  EXPECT_EQ(CheckDrivePickerAccount(identity_manager(), info.GetGaiaId()),
            Error::kChromeSignInRequired);
}

TEST_F(GlicDrivePickerAccountUtilTest, RefreshTokenInPersistentError) {
  AccountInfo info = SignInWithCookie("user@gmail.com");
  identity_env_.UpdatePersistentErrorOfRefreshTokenForAccount(
      info.GetAccountId(),
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER));
  EXPECT_EQ(CheckDrivePickerAccount(identity_manager(), info.GetGaiaId()),
            Error::kChromeSignInRequired);
}

TEST_F(GlicDrivePickerAccountUtilTest, TransientRefreshTokenErrorIsIgnored) {
  AccountInfo info = SignInWithCookie("user@gmail.com");
  const GoogleServiceAuthError transient_error =
      GoogleServiceAuthError::FromServiceUnavailable("unavailable");
  ASSERT_TRUE(transient_error.IsTransientError());
  identity_env_.UpdatePersistentErrorOfRefreshTokenForAccount(
      info.GetAccountId(), transient_error);
  EXPECT_EQ(CheckDrivePickerAccount(identity_manager(), info.GetGaiaId()),
            std::nullopt);
}

TEST_F(GlicDrivePickerAccountUtilTest, PrimaryMissingFromFreshCookies) {
  AccountInfo info = identity_env_.MakePrimaryAccountAvailable(
      "user@gmail.com", signin::ConsentLevel::kSignin);
  identity_env_.SetCookieAccounts({});
  EXPECT_EQ(CheckDrivePickerAccount(identity_manager(), info.GetGaiaId()),
            Error::kWebSessionSignedOut);
}

TEST_F(GlicDrivePickerAccountUtilTest, PrimarySignedOutInCookies) {
  AccountInfo info = identity_env_.MakePrimaryAccountAvailable(
      "user@gmail.com", signin::ConsentLevel::kSignin);
  identity_env_.SetCookieAccounts(
      {{std::string(info.GetEmail()), info.GetGaiaId(), /*signed_out=*/true}});
  EXPECT_EQ(CheckDrivePickerAccount(identity_manager(), info.GetGaiaId()),
            Error::kWebSessionSignedOut);
}

TEST_F(GlicDrivePickerAccountUtilTest, PrimaryInvalidInFreshCookies) {
  AccountInfo info = identity_env_.MakePrimaryAccountAvailable(
      "user@gmail.com", signin::ConsentLevel::kSignin);
  identity_env_.SetFreshnessOfAccountsInGaiaCookie(false);
  signin::SetListAccountsResponseOneAccountWithParams(
      {.email = std::string(info.GetEmail()),
       .gaia_id = info.GetGaiaId(),
       .valid = false,
       .signed_out = false,
       .verified = true},
      &test_url_loader_factory_);

  base::RunLoop run_loop;
  signin::TestIdentityManagerObserver cookie_observer(identity_manager());
  cookie_observer.SetOnAccountsInCookieUpdatedCallback(run_loop.QuitClosure());
  identity_manager()->GetAccountsCookieMutator()->TriggerCookieJarUpdate();
  run_loop.Run();

  const signin::AccountsInCookieJarInfo cookies =
      identity_manager()->GetCachedAccountsInCookieJar();
  ASSERT_TRUE(cookies.AreAccountsFresh());
  ASSERT_EQ(cookies.GetPotentiallyInvalidSignedInAccounts().size(), 1u);
  ASSERT_TRUE(cookies.GetValidSignedInAccounts().empty());

  EXPECT_EQ(CheckDrivePickerAccount(identity_manager(), info.GetGaiaId()),
            Error::kWebSessionSignedOut);
}

TEST_F(GlicDrivePickerAccountUtilTest, StaleCookiesAreIgnored) {
  AccountInfo info = identity_env_.MakePrimaryAccountAvailable(
      "user@gmail.com", signin::ConsentLevel::kSignin);
  identity_env_.SetCookieAccounts({});
  identity_env_.SetFreshnessOfAccountsInGaiaCookie(false);
  EXPECT_EQ(CheckDrivePickerAccount(identity_manager(), info.GetGaiaId()),
            std::nullopt);
}

TEST_F(GlicDrivePickerAccountUtilTest, WatcherReportsWebSignOut) {
  AccountInfo info = SignInWithCookie("user@gmail.com");
  base::test::TestFuture<Error> invalidated;
  DrivePickerAccountWatcher watcher(identity_manager(), info.GetGaiaId(),
                                    invalidated.GetCallback());
  // Signing out of the web (e.g. from accounts.google.com in a tab) leaves
  // the account in the cookie jar as signed out.
  identity_env_.SetCookieAccounts(
      {{std::string(info.GetEmail()), info.GetGaiaId(), /*signed_out=*/true}});
  EXPECT_EQ(invalidated.Get(), Error::kWebSessionSignedOut);
}

TEST_F(GlicDrivePickerAccountUtilTest, WatcherReportsTokenError) {
  AccountInfo info = SignInWithCookie("user@gmail.com");
  base::test::TestFuture<Error> invalidated;
  DrivePickerAccountWatcher watcher(identity_manager(), info.GetGaiaId(),
                                    invalidated.GetCallback());
  identity_env_.UpdatePersistentErrorOfRefreshTokenForAccount(
      info.GetAccountId(),
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER));
  EXPECT_EQ(invalidated.Get(), Error::kChromeSignInRequired);
}

TEST_F(GlicDrivePickerAccountUtilTest, WatcherReportsRefreshTokenRemoved) {
  AccountInfo info = SignInWithCookie("user@gmail.com");
  base::test::TestFuture<Error> invalidated;
  DrivePickerAccountWatcher watcher(identity_manager(), info.GetGaiaId(),
                                    invalidated.GetCallback());
  identity_env_.RemoveRefreshTokenForPrimaryAccount();
  EXPECT_EQ(invalidated.Get(), Error::kChromeSignInRequired);
}

TEST_F(GlicDrivePickerAccountUtilTest, WatcherReportsCookieDeletedByUser) {
  AccountInfo info = SignInWithCookie("user@gmail.com");
  base::test::TestFuture<Error> invalidated;
  DrivePickerAccountWatcher watcher(identity_manager(), info.GetGaiaId(),
                                    invalidated.GetCallback());
  watcher.OnAccountsCookieDeletedByUserAction();
  EXPECT_EQ(invalidated.Get(), Error::kWebSessionSignedOut);
}

TEST_F(GlicDrivePickerAccountUtilTest, WatcherIgnoresFailedCookieFetch) {
  AccountInfo info = SignInWithCookie("user@gmail.com");
  bool fired = false;
  DrivePickerAccountWatcher watcher(
      identity_manager(), info.GetGaiaId(),
      base::BindOnce([](bool* fired, Error) { *fired = true; }, &fired));
  watcher.OnAccountsInCookieUpdated(
      signin::AccountsInCookieJarInfo(/*accounts_are_fresh=*/true, {}),
      GoogleServiceAuthError::FromServiceUnavailable("unavailable"));
  EXPECT_FALSE(fired);
}

TEST_F(GlicDrivePickerAccountUtilTest, WatcherReportsPrimaryAccountSwitched) {
  SignInWithCookie("other@gmail.com");
  base::test::TestFuture<Error> invalidated;
  DrivePickerAccountWatcher watcher(identity_manager(), GaiaId("initial-gaia"),
                                    invalidated.GetCallback());
  watcher.OnPrimaryAccountChanged(signin::PrimaryAccountChangeEvent());
  EXPECT_EQ(invalidated.Get(), Error::kAccountMismatch);
}

TEST_F(GlicDrivePickerAccountUtilTest, WatcherReportsOnlyOnce) {
  AccountInfo info = SignInWithCookie("user@gmail.com");
  int call_count = 0;
  std::optional<Error> reported_error;
  DrivePickerAccountWatcher watcher(
      identity_manager(), info.GetGaiaId(),
      base::BindOnce(
          [](int* call_count, std::optional<Error>* reported_error,
             Error error) {
            ++(*call_count);
            *reported_error = error;
          },
          &call_count, &reported_error));
  watcher.OnAccountsCookieDeletedByUserAction();
  watcher.OnAccountsCookieDeletedByUserAction();
  EXPECT_EQ(call_count, 1);
  EXPECT_EQ(reported_error, Error::kWebSessionSignedOut);
}

TEST_F(GlicDrivePickerAccountUtilTest, WatcherHandlesIdentityManagerShutdown) {
  AccountInfo info = SignInWithCookie("user@gmail.com");
  bool fired = false;
  DrivePickerAccountWatcher watcher(
      identity_manager(), info.GetGaiaId(),
      base::BindOnce([](bool* fired, Error) { *fired = true; }, &fired));
  watcher.OnIdentityManagerShutdown(identity_manager());
  identity_env_.SetCookieAccounts({});
  EXPECT_FALSE(fired);
}

#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(GlicDrivePickerAccountUtilTest, WatcherReportsPrimaryAccountCleared) {
  AccountInfo info = SignInWithCookie("user@gmail.com");
  base::test::TestFuture<Error> invalidated;
  DrivePickerAccountWatcher watcher(identity_manager(), info.GetGaiaId(),
                                    invalidated.GetCallback());
  identity_env_.ClearPrimaryAccount();
  EXPECT_EQ(invalidated.Get(), Error::kChromeSignInRequired);
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

TEST_F(GlicDrivePickerAccountUtilTest, WatcherIgnoresUnrelatedCookieChanges) {
  AccountInfo info = SignInWithCookie("user@gmail.com");
  bool fired = false;
  DrivePickerAccountWatcher watcher(
      identity_manager(), info.GetGaiaId(),
      base::BindOnce([](bool* fired, Error) { *fired = true; }, &fired));
  // Adding a secondary web account keeps the primary signed in.
  identity_env_.SetCookieAccounts(
      {{std::string(info.GetEmail()), info.GetGaiaId()},
       {"other@gmail.com", GaiaId("other-gaia")}});
  EXPECT_FALSE(fired);
}

TEST_F(GlicDrivePickerAccountUtilTest,
       WatcherIgnoresTransientPrimaryTokenError) {
  AccountInfo info = SignInWithCookie("user@gmail.com");
  bool fired = false;
  DrivePickerAccountWatcher watcher(
      identity_manager(), info.GetGaiaId(),
      base::BindOnce([](bool* fired, Error) { *fired = true; }, &fired));
  const GoogleServiceAuthError transient_error =
      GoogleServiceAuthError::FromServiceUnavailable("unavailable");
  ASSERT_TRUE(transient_error.IsTransientError());
  identity_env_.UpdatePersistentErrorOfRefreshTokenForAccount(
      info.GetAccountId(), transient_error);
  watcher.OnErrorStateOfRefreshTokenUpdatedForAccount(
      info.GetCoreAccountInfo(), transient_error,
      signin_metrics::SourceForRefreshTokenOperation::kUnknown);
  EXPECT_FALSE(fired);
}

TEST_F(GlicDrivePickerAccountUtilTest,
       WatcherIgnoresSecondaryAccountRefreshTokenError) {
  AccountInfo primary = SignInWithCookie("user@gmail.com");
  AccountInfo secondary = identity_env_.MakeAccountAvailable("other@gmail.com");
  bool fired = false;
  DrivePickerAccountWatcher watcher(
      identity_manager(), primary.GetGaiaId(),
      base::BindOnce([](bool* fired, Error) { *fired = true; }, &fired));
  identity_env_.UpdatePersistentErrorOfRefreshTokenForAccount(
      secondary.GetAccountId(),
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER));
  EXPECT_FALSE(fired);
}

TEST_F(GlicDrivePickerAccountUtilTest,
       WatcherIgnoresSecondaryAccountRefreshTokenRemoved) {
  AccountInfo primary = SignInWithCookie("user@gmail.com");
  AccountInfo secondary = identity_env_.MakeAccountAvailable("other@gmail.com");
  bool fired = false;
  DrivePickerAccountWatcher watcher(
      identity_manager(), primary.GetGaiaId(),
      base::BindOnce([](bool* fired, Error) { *fired = true; }, &fired));
  identity_env_.RemoveRefreshTokenForAccount(secondary.GetAccountId());
  EXPECT_FALSE(fired);
}

TEST_F(GlicDrivePickerAccountUtilTest,
       WatcherPrimaryAndTokenCallbacksDoNotFetchCookiesWhenStale) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      switches::kAvoidAutoTriggerListAccountsOnStale);

  AccountInfo primary = SignInWithCookie("user@gmail.com");
  AccountInfo secondary = identity_env_.MakeAccountAvailable("other@gmail.com");
  identity_env_.SetFreshnessOfAccountsInGaiaCookie(false);
  test_url_loader_factory_.ClearResponses();
  const std::string list_accounts_url =
      GaiaUrls::GetInstance()
          ->ListAccountsURLWithSource(GaiaConstants::kChromeSource)
          .spec();
  ASSERT_FALSE(test_url_loader_factory_.IsPending(list_accounts_url));

  bool fired = false;
  DrivePickerAccountWatcher watcher(
      identity_manager(), primary.GetGaiaId(),
      base::BindOnce([](bool* fired, Error) { *fired = true; }, &fired));

  watcher.OnPrimaryAccountChanged(signin::PrimaryAccountChangeEvent());
  identity_env_.UpdatePersistentErrorOfRefreshTokenForAccount(
      secondary.GetAccountId(),
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER));
  // On ChromeOS, removing a refresh token triggers an unrelated server-side
  // OAuth2 token revocation request via AccountManager on the shared
  // TestURLLoaderFactory, so assert specifically that no /ListAccounts fetch is
  // pending.
  identity_env_.RemoveRefreshTokenForAccount(secondary.GetAccountId());

  EXPECT_FALSE(fired);
  EXPECT_FALSE(test_url_loader_factory_.IsPending(list_accounts_url));

  // Sanity check: querying cookies directly while stale triggers /ListAccounts
  // at `list_accounts_url`.
  identity_manager()->GetAccountsInCookieJar();
  EXPECT_TRUE(test_url_loader_factory_.IsPending(list_accounts_url));
}

}  // namespace
}  // namespace glic
