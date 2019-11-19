// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/signin/e2e_tests/live_test.h"
#include "chrome/browser/signin/e2e_tests/test_accounts_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/webui/signin/login_ui_test_utils.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/test_identity_manager_observer.h"
#include "components/sync/driver/sync_service.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"

#if !defined(OS_CHROMEOS)
#include "chrome/browser/sync/sync_ui_util.h"
#endif  // !defined(OS_CHROMEOS)

namespace signin {
namespace test {

// IdentityManager observer allowing to wait for sign out events for several
// accounts.
// Counts both token removals and token persistent errors as sign out events.
class SignOutTestObserver : public IdentityManager::Observer {
 public:
  explicit SignOutTestObserver(IdentityManager* identity_manager)
      : identity_manager_(identity_manager) {
    identity_manager_->AddObserver(this);
  }
  ~SignOutTestObserver() override { identity_manager_->RemoveObserver(this); }

  void OnRefreshTokenRemovedForAccount(
      const CoreAccountId& account_id) override {
    ++signed_out_accounts_;
    QuitIfConditionIsSatisfied();
  }

  void OnErrorStateOfRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info,
      const GoogleServiceAuthError& error) override {
    if (!error.IsPersistentError())
      return;

    ++signed_out_accounts_;
    QuitIfConditionIsSatisfied();
  }

  void WaitForRefreshTokenRemovedForAccounts(int expected_accounts) {
    expected_accounts_ = expected_accounts;
    QuitIfConditionIsSatisfied();
    run_loop_.Run();
  }

 private:
  void QuitIfConditionIsSatisfied() {
    if (expected_accounts_ != -1 && signed_out_accounts_ >= expected_accounts_)
      run_loop_.Quit();
  }

  signin::IdentityManager* identity_manager_;
  base::RunLoop run_loop_;
  int signed_out_accounts_ = 0;
  int expected_accounts_ = -1;
};

// Live tests for SignIn.
class LiveSignInTest : public signin::test::LiveTest {
 public:
  LiveSignInTest() = default;
  ~LiveSignInTest() override = default;

  void SetUp() override {
    LiveTest::SetUp();
    // Always disable animation for stability.
    ui::ScopedAnimationDurationScaleMode disable_animation(
        ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  }

  void SignInFromWeb(const TestAccount& test_account) {
    AddTabAtIndex(0, GaiaUrls::GetInstance()->add_account_url(),
                  ui::PageTransition::PAGE_TRANSITION_TYPED);
    SignInFromCurrentPage(test_account);
  }

  void SignInFromSettings(const TestAccount& test_account) {
    GURL settings_url("chrome://settings");
    AddTabAtIndex(0, settings_url, ui::PageTransition::PAGE_TRANSITION_TYPED);
    auto* settings_tab = browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_TRUE(content::ExecuteScript(
        settings_tab,
        "settings.SyncBrowserProxyImpl.getInstance().startSignIn()"));
    SignInFromCurrentPage(test_account);
  }

  void SignInFromCurrentPage(const TestAccount& test_account) {
    TestIdentityManagerObserver observer(identity_manager());
    base::RunLoop cookie_update_loop;
    observer.SetOnAccountsInCookieUpdatedCallback(
        cookie_update_loop.QuitClosure());
    base::RunLoop refresh_token_update_loop;
    observer.SetOnRefreshTokenUpdatedCallback(
        refresh_token_update_loop.QuitClosure());
    login_ui_test_utils::ExecuteJsToSigninInSigninFrame(
        browser(), test_account.user, test_account.password);
    cookie_update_loop.Run();
    refresh_token_update_loop.Run();
  }

  void TurnOnSync(const TestAccount& test_account) {
    SignInFromSettings(test_account);

    TestIdentityManagerObserver observer(identity_manager());
    base::RunLoop primary_account_set_loop;
    observer.SetOnPrimaryAccountSetCallback(
        primary_account_set_loop.QuitClosure());
    login_ui_test_utils::DismissSyncConfirmationDialog(
        browser(), base::TimeDelta::FromSeconds(3));
    primary_account_set_loop.Run();
  }

  void SignOutFromWeb(size_t signed_in_accounts) {
    TestIdentityManagerObserver observer(identity_manager());
    base::RunLoop cookie_update_loop;
    observer.SetOnAccountsInCookieUpdatedCallback(
        cookie_update_loop.QuitClosure());
    SignOutTestObserver sign_out_observer(identity_manager());
    AddTabAtIndex(0, GaiaUrls::GetInstance()->service_logout_url(),
                  ui::PageTransition::PAGE_TRANSITION_TYPED);
    cookie_update_loop.Run();
    sign_out_observer.WaitForRefreshTokenRemovedForAccounts(signed_in_accounts);
  }

  signin::IdentityManager* identity_manager() {
    return IdentityManagerFactory::GetForProfile(browser()->profile());
  }

  syncer::SyncService* sync_service() {
    return ProfileSyncServiceFactory::GetForProfile(browser()->profile());
  }
};

// Consistently timing out on windows.  http://crbug.com/1025220
#if defined(OS_WIN)
#define MAYBE_SimpleSignInFlow DISABLED_SimpleSignInFlow
#define MAYBE_WebSignOut DISABLED_WebSignOut
#define MAYBE_TurnOffSync DISABLED_TurnOffSync
#else
#define MAYBE_SimpleSignInFlow SimpleSignInFlow
#define MAYBE_WebSignOut WebSignOut
#define MAYBE_TurnOffSync TurnOffSync
#endif

// Sings in an account through the settings page and checks that the account is
// added to Chrome. Sync should be disabled because the test doesn't pass
// through the Sync confirmation dialog.
IN_PROC_BROWSER_TEST_F(LiveSignInTest, MAYBE_SimpleSignInFlow) {
  TestAccount ta;
  CHECK(GetTestAccountsUtil()->GetAccount("TEST_ACCOUNT_1", ta));
  SignInFromSettings(ta);

  const AccountsInCookieJarInfo& accounts_in_cookie_jar =
      identity_manager()->GetAccountsInCookieJar();
  EXPECT_TRUE(accounts_in_cookie_jar.accounts_are_fresh);
  ASSERT_EQ(1u, accounts_in_cookie_jar.signed_in_accounts.size());
  EXPECT_TRUE(accounts_in_cookie_jar.signed_out_accounts.empty());
  const gaia::ListedAccount& account =
      accounts_in_cookie_jar.signed_in_accounts[0];
  EXPECT_TRUE(gaia::AreEmailsSame(ta.user, account.email));
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account.id));
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount());
}

// Signs in an account through the settings page and enables Sync. Checks that
// Sync is enabled.
// Then, signs out on the web and checks that the account is removed from
// cookies and Sync paused error is displayed.
IN_PROC_BROWSER_TEST_F(LiveSignInTest, MAYBE_WebSignOut) {
  TestAccount test_account;
  CHECK(GetTestAccountsUtil()->GetAccount("TEST_ACCOUNT_1", test_account));
  TurnOnSync(test_account);

  const CoreAccountInfo& primary_account =
      identity_manager()->GetPrimaryAccountInfo();
  EXPECT_FALSE(primary_account.IsEmpty());
  EXPECT_TRUE(gaia::AreEmailsSame(test_account.user, primary_account.email));
  EXPECT_TRUE(sync_service()->IsSyncFeatureEnabled());

  SignOutFromWeb(1);

  const AccountsInCookieJarInfo& accounts_in_cookie_jar =
      identity_manager()->GetAccountsInCookieJar();
  EXPECT_TRUE(accounts_in_cookie_jar.accounts_are_fresh);
  ASSERT_TRUE(accounts_in_cookie_jar.signed_in_accounts.empty());
  ASSERT_EQ(1u, accounts_in_cookie_jar.signed_out_accounts.size());
  EXPECT_TRUE(gaia::AreEmailsSame(
      test_account.user, accounts_in_cookie_jar.signed_out_accounts[0].email));
  EXPECT_TRUE(
      identity_manager()->HasAccountWithRefreshTokenInPersistentErrorState(
          primary_account.account_id));
#if !defined(OS_CHROMEOS)
  int unused1, unused2;
  EXPECT_EQ(sync_ui_util::GetMessagesForAvatarSyncError(browser()->profile(),
                                                        &unused1, &unused2),
            sync_ui_util::AUTH_ERROR);
#endif  // !defined(OS_CHROMEOS)
}

// Sings in two accounts on the web and checks that cookies and refresh tokens
// are added to Chrome. Sync should be disabled.
// Then, signs out on the web and checks that accounts are removed from Chrome.
IN_PROC_BROWSER_TEST_F(LiveSignInTest, WebSignInAndSignOut) {
  TestAccount test_account_1;
  CHECK(GetTestAccountsUtil()->GetAccount("TEST_ACCOUNT_1", test_account_1));
  SignInFromWeb(test_account_1);

  const AccountsInCookieJarInfo& accounts_in_cookie_jar_1 =
      identity_manager()->GetAccountsInCookieJar();
  EXPECT_TRUE(accounts_in_cookie_jar_1.accounts_are_fresh);
  ASSERT_EQ(1u, accounts_in_cookie_jar_1.signed_in_accounts.size());
  EXPECT_TRUE(accounts_in_cookie_jar_1.signed_out_accounts.empty());
  const gaia::ListedAccount& account_1 =
      accounts_in_cookie_jar_1.signed_in_accounts[0];
  EXPECT_TRUE(gaia::AreEmailsSame(test_account_1.user, account_1.email));
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account_1.id));
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount());

  TestAccount test_account_2;
  CHECK(GetTestAccountsUtil()->GetAccount("TEST_ACCOUNT_2", test_account_2));
  SignInFromWeb(test_account_2);

  const AccountsInCookieJarInfo& accounts_in_cookie_jar_2 =
      identity_manager()->GetAccountsInCookieJar();
  EXPECT_TRUE(accounts_in_cookie_jar_2.accounts_are_fresh);
  ASSERT_EQ(2u, accounts_in_cookie_jar_2.signed_in_accounts.size());
  EXPECT_TRUE(accounts_in_cookie_jar_2.signed_out_accounts.empty());
  EXPECT_EQ(accounts_in_cookie_jar_2.signed_in_accounts[0].id, account_1.id);
  const gaia::ListedAccount& account_2 =
      accounts_in_cookie_jar_2.signed_in_accounts[1];
  EXPECT_TRUE(gaia::AreEmailsSame(test_account_2.user, account_2.email));
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account_2.id));
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount());

  SignOutFromWeb(2);

  const AccountsInCookieJarInfo& accounts_in_cookie_jar_3 =
      identity_manager()->GetAccountsInCookieJar();
  EXPECT_TRUE(accounts_in_cookie_jar_3.accounts_are_fresh);
  ASSERT_TRUE(accounts_in_cookie_jar_3.signed_in_accounts.empty());
  EXPECT_EQ(2u, accounts_in_cookie_jar_3.signed_out_accounts.size());
  EXPECT_TRUE(identity_manager()->GetAccountsWithRefreshTokens().empty());
}

// Signs in an account through the settings page and enables Sync. Checks that
// Sync is enabled. Signs in a second account on the web.
// Then, turns Sync off from the settings page and checks that both accounts are
// removed from Chrome and from cookies.
IN_PROC_BROWSER_TEST_F(LiveSignInTest, MAYBE_TurnOffSync) {
  TestAccount test_account_1;
  CHECK(GetTestAccountsUtil()->GetAccount("TEST_ACCOUNT_1", test_account_1));
  TurnOnSync(test_account_1);

  TestAccount test_account_2;
  CHECK(GetTestAccountsUtil()->GetAccount("TEST_ACCOUNT_2", test_account_2));
  SignInFromWeb(test_account_2);

  const CoreAccountInfo& primary_account =
      identity_manager()->GetPrimaryAccountInfo();
  EXPECT_FALSE(primary_account.IsEmpty());
  EXPECT_TRUE(gaia::AreEmailsSame(test_account_1.user, primary_account.email));
  EXPECT_TRUE(sync_service()->IsSyncFeatureEnabled());

  GURL settings_url("chrome://settings");
  AddTabAtIndex(0, settings_url, ui::PageTransition::PAGE_TRANSITION_TYPED);
  TestIdentityManagerObserver observer(identity_manager());
  base::RunLoop cookie_update_loop;
  observer.SetOnAccountsInCookieUpdatedCallback(
      cookie_update_loop.QuitClosure());
  base::RunLoop primary_account_cleared_loop;
  observer.SetOnPrimaryAccountClearedCallback(
      primary_account_cleared_loop.QuitClosure());
  SignOutTestObserver sign_out_observer(identity_manager());
  auto* settings_tab = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::ExecuteScript(
      settings_tab,
      "settings.SyncBrowserProxyImpl.getInstance().signOut(false)"));
  primary_account_cleared_loop.Run();
  sign_out_observer.WaitForRefreshTokenRemovedForAccounts(2);
  cookie_update_loop.Run();

  const AccountsInCookieJarInfo& accounts_in_cookie_jar_2 =
      identity_manager()->GetAccountsInCookieJar();
  EXPECT_TRUE(accounts_in_cookie_jar_2.accounts_are_fresh);
  ASSERT_TRUE(accounts_in_cookie_jar_2.signed_in_accounts.empty());
  EXPECT_TRUE(identity_manager()->GetAccountsWithRefreshTokens().empty());
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount());
}

}  // namespace test
}  // namespace signin
