// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/e2e_tests/account_capabilities_observer.h"
#include "chrome/browser/signin/e2e_tests/accounts_removed_waiter.h"
#include "chrome/browser/signin/e2e_tests/live_test.h"
#include "chrome/browser/signin/e2e_tests/sign_in_test_observer.h"
#include "chrome/browser/signin/e2e_tests/signin_util.h"
#include "chrome/browser/signin/e2e_tests/test_accounts_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/webui/signin/login_ui_test_utils.h"
#include "chrome/browser/ui/webui/signin/signin_email_confirmation_dialog.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "components/sync/driver/sync_service.h"
#include "content/public/test/browser_test.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"

#if !BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/sync/sync_ui_util.h"
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

namespace signin::test {

// Live tests for SignIn.
// These tests can be run with:
// browser_tests --gtest_filter=LiveSignInTest.* --run-live-tests --run-manual
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

  signin::IdentityManager* identity_manager() {
    return signin::test::identity_manager(browser());
  }
  syncer::SyncService* sync_service() {
    return signin::test::sync_service(browser());
  }
  AccountReconcilor* account_reconcilor() {
    return signin::test::account_reconcilor(browser());
  }

  SignInFunctions sign_in_functions = SignInFunctions(
      base::BindLambdaForTesting(
          [this]() -> Browser* { return this->browser(); }),
      base::BindLambdaForTesting([this](int index,
                                        const GURL& url,
                                        ui::PageTransition transition) -> bool {
        return this->AddTabAtIndex(index, url, transition);
      }));
};

// This test can pass. Marked as manual because it TIMED_OUT on Win7.
// See crbug.com/1025335.
// Sings in an account through the settings page and checks that the account is
// added to Chrome. Sync should be disabled because the test doesn't pass
// through the Sync confirmation dialog.
IN_PROC_BROWSER_TEST_F(LiveSignInTest, MANUAL_SimpleSignInFlow) {
  TestAccount ta;
  CHECK(GetTestAccountsUtil()->GetAccount("TEST_ACCOUNT_1", ta));
  sign_in_functions.SignInFromSettings(ta, 0);

  const AccountsInCookieJarInfo& accounts_in_cookie_jar =
      identity_manager()->GetAccountsInCookieJar();
  EXPECT_TRUE(accounts_in_cookie_jar.accounts_are_fresh);
  ASSERT_EQ(1u, accounts_in_cookie_jar.signed_in_accounts.size());
  EXPECT_TRUE(accounts_in_cookie_jar.signed_out_accounts.empty());
  const gaia::ListedAccount& account =
      accounts_in_cookie_jar.signed_in_accounts[0];
  EXPECT_TRUE(gaia::AreEmailsSame(ta.user, account.email));
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account.id));
  EXPECT_FALSE(sync_service()->IsSyncFeatureEnabled());
}

// This test can pass. Marked as manual because it TIMED_OUT on Win7.
// See crbug.com/1025335.
// Signs in an account through the settings page and enables Sync. Checks that
// Sync is enabled.
// Then, signs out on the web and checks that the account is removed from
// cookies and Sync paused error is displayed.
IN_PROC_BROWSER_TEST_F(LiveSignInTest, MANUAL_WebSignOut) {
  TestAccount test_account;
  CHECK(GetTestAccountsUtil()->GetAccount("TEST_ACCOUNT_1", test_account));
  sign_in_functions.TurnOnSync(test_account, 0);

  const CoreAccountInfo& primary_account =
      identity_manager()->GetPrimaryAccountInfo(signin::ConsentLevel::kSync);
  EXPECT_FALSE(primary_account.IsEmpty());
  EXPECT_TRUE(gaia::AreEmailsSame(test_account.user, primary_account.email));
  EXPECT_TRUE(sync_service()->IsSyncFeatureEnabled());

  sign_in_functions.SignOutFromWeb();

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
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_EQ(GetAvatarSyncErrorType(browser()->profile()),
            AvatarSyncErrorType::kSyncPaused);
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
}

// This test can pass. Marked as manual because it TIMED_OUT on Win7.
// See crbug.com/1025335.
// Sings in two accounts on the web and checks that cookies and refresh tokens
// are added to Chrome. Sync should be disabled.
// Then, signs out on the web and checks that accounts are removed from Chrome.
IN_PROC_BROWSER_TEST_F(LiveSignInTest, MANUAL_WebSignInAndSignOut) {
  TestAccount test_account_1;
  CHECK(GetTestAccountsUtil()->GetAccount("TEST_ACCOUNT_1", test_account_1));
  sign_in_functions.SignInFromWeb(test_account_1, 0);

  const AccountsInCookieJarInfo& accounts_in_cookie_jar_1 =
      identity_manager()->GetAccountsInCookieJar();
  EXPECT_TRUE(accounts_in_cookie_jar_1.accounts_are_fresh);
  ASSERT_EQ(1u, accounts_in_cookie_jar_1.signed_in_accounts.size());
  EXPECT_TRUE(accounts_in_cookie_jar_1.signed_out_accounts.empty());
  const gaia::ListedAccount& account_1 =
      accounts_in_cookie_jar_1.signed_in_accounts[0];
  EXPECT_TRUE(gaia::AreEmailsSame(test_account_1.user, account_1.email));
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account_1.id));
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));

  TestAccount test_account_2;
  CHECK(GetTestAccountsUtil()->GetAccount("TEST_ACCOUNT_2", test_account_2));
  sign_in_functions.SignInFromWeb(test_account_2, 1);

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
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));

  sign_in_functions.SignOutFromWeb();

  const AccountsInCookieJarInfo& accounts_in_cookie_jar_3 =
      identity_manager()->GetAccountsInCookieJar();
  EXPECT_TRUE(accounts_in_cookie_jar_3.accounts_are_fresh);
  ASSERT_TRUE(accounts_in_cookie_jar_3.signed_in_accounts.empty());
  EXPECT_EQ(2u, accounts_in_cookie_jar_3.signed_out_accounts.size());
  EXPECT_TRUE(identity_manager()->GetAccountsWithRefreshTokens().empty());
}

// This test can pass. Marked as manual because it TIMED_OUT on Win7.
// See crbug.com/1025335.
// Signs in an account through the settings page and enables Sync. Checks that
// Sync is enabled. Signs in a second account on the web.
// Then, turns Sync off from the settings page and checks that both accounts are
// removed from Chrome and from cookies.
IN_PROC_BROWSER_TEST_F(LiveSignInTest, MANUAL_TurnOffSync) {
  TestAccount test_account_1;
  CHECK(GetTestAccountsUtil()->GetAccount("TEST_ACCOUNT_1", test_account_1));
  sign_in_functions.TurnOnSync(test_account_1, 0);

  TestAccount test_account_2;
  CHECK(GetTestAccountsUtil()->GetAccount("TEST_ACCOUNT_2", test_account_2));
  sign_in_functions.SignInFromWeb(test_account_2, 1);

  const CoreAccountInfo& primary_account =
      identity_manager()->GetPrimaryAccountInfo(signin::ConsentLevel::kSync);
  EXPECT_FALSE(primary_account.IsEmpty());
  EXPECT_TRUE(gaia::AreEmailsSame(test_account_1.user, primary_account.email));
  EXPECT_TRUE(sync_service()->IsSyncFeatureEnabled());

  sign_in_functions.TurnOffSync();

  const AccountsInCookieJarInfo& accounts_in_cookie_jar_2 =
      identity_manager()->GetAccountsInCookieJar();
  EXPECT_TRUE(accounts_in_cookie_jar_2.accounts_are_fresh);
  ASSERT_TRUE(accounts_in_cookie_jar_2.signed_in_accounts.empty());
  EXPECT_TRUE(identity_manager()->GetAccountsWithRefreshTokens().empty());
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
}

// In "Sync paused" state, when the primary account is invalid, turns off sync
// from settings. Checks that the account is removed from Chrome.
// Regression test for https://crbug.com/1114646
IN_PROC_BROWSER_TEST_F(LiveSignInTest, MANUAL_TurnOffSyncWhenPaused) {
  TestAccount test_account_1;
  CHECK(GetTestAccountsUtil()->GetAccount("TEST_ACCOUNT_1", test_account_1));
  sign_in_functions.TurnOnSync(test_account_1, 0);

  // Get in sync paused state.
  sign_in_functions.SignOutFromWeb();

  const CoreAccountInfo& primary_account =
      identity_manager()->GetPrimaryAccountInfo(signin::ConsentLevel::kSync);
  EXPECT_FALSE(primary_account.IsEmpty());
  EXPECT_TRUE(gaia::AreEmailsSame(test_account_1.user, primary_account.email));
  EXPECT_TRUE(sync_service()->IsSyncFeatureEnabled());
  EXPECT_TRUE(
      identity_manager()->HasAccountWithRefreshTokenInPersistentErrorState(
          primary_account.account_id));

  sign_in_functions.TurnOffSync();
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));

  // Wait until the signin manager clears the invalid token.
  AccountsRemovedWaiter accounts_removed_waiter(identity_manager());
  accounts_removed_waiter.Wait();
  EXPECT_TRUE(identity_manager()->GetAccountsWithRefreshTokens().empty());
}

// This test can pass. Marked as manual because it TIMED_OUT on Win7.
// See crbug.com/1025335.
// Signs in an account on the web. Goes to the Chrome settings to enable Sync
// but cancels the sync confirmation dialog. Checks that the account is still
// signed in on the web but Sync is disabled.
IN_PROC_BROWSER_TEST_F(LiveSignInTest, MANUAL_CancelSyncWithWebAccount) {
  TestAccount test_account;
  CHECK(GetTestAccountsUtil()->GetAccount("TEST_ACCOUNT_1", test_account));
  sign_in_functions.SignInFromWeb(test_account, 0);

  SignInTestObserver observer(identity_manager(), account_reconcilor());
  GURL settings_url("chrome://settings");
  ASSERT_TRUE(AddTabAtIndex(0, settings_url,
                            ui::PageTransition::PAGE_TRANSITION_TYPED));
  auto* settings_tab = browser()->tab_strip_model()->GetActiveWebContents();
  std::string start_syncing_script = base::StringPrintf(
      "settings.SyncBrowserProxyImpl.getInstance()."
      "startSyncingWithEmail(\"%s\", true);",
      test_account.user.c_str());
  EXPECT_TRUE(content::ExecuteScript(
      settings_tab, base::StringPrintf(kSettingsScriptWrapperFormat,
                                       start_syncing_script.c_str())));
  EXPECT_TRUE(login_ui_test_utils::CancelSyncConfirmationDialog(
      browser(), kDialogTimeout));
  observer.WaitForAccountChanges(1, PrimarySyncAccountWait::kWaitForCleared);

  const AccountsInCookieJarInfo& accounts_in_cookie_jar =
      identity_manager()->GetAccountsInCookieJar();
  EXPECT_TRUE(accounts_in_cookie_jar.accounts_are_fresh);
  ASSERT_EQ(1u, accounts_in_cookie_jar.signed_in_accounts.size());
  const gaia::ListedAccount& account =
      accounts_in_cookie_jar.signed_in_accounts[0];
  EXPECT_TRUE(gaia::AreEmailsSame(test_account.user, account.email));
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account.id));
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
}

// This test can pass. Marked as manual because it TIMED_OUT on Win7.
// See crbug.com/1025335.
// Starts the sign in flow from the settings page, enters credentials on the
// login page but cancels the Sync confirmation dialog. Checks that Sync is
// disabled and no account was added to Chrome.
IN_PROC_BROWSER_TEST_F(LiveSignInTest, MANUAL_CancelSync) {
  TestAccount test_account;
  CHECK(GetTestAccountsUtil()->GetAccount("TEST_ACCOUNT_1", test_account));
  sign_in_functions.SignInFromSettings(test_account, 0);

  SignInTestObserver observer(identity_manager(), account_reconcilor());
  EXPECT_TRUE(login_ui_test_utils::CancelSyncConfirmationDialog(
      browser(), kDialogTimeout));
  observer.WaitForAccountChanges(0, PrimarySyncAccountWait::kWaitForCleared);

  const AccountsInCookieJarInfo& accounts_in_cookie_jar =
      identity_manager()->GetAccountsInCookieJar();
  EXPECT_TRUE(accounts_in_cookie_jar.accounts_are_fresh);
  EXPECT_TRUE(accounts_in_cookie_jar.signed_in_accounts.empty());
  EXPECT_TRUE(identity_manager()->GetAccountsWithRefreshTokens().empty());
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
}

// This test can pass. Marked as manual because it TIMED_OUT on Win7.
// See crbug.com/1025335.
// Enables and disables sync to account 1. Enables sync to account 2 and clicks
// on "This wasn't me" in the email confirmation dialog. Checks that the new
// profile is created. Checks that Sync to account 2 is enabled in the new
// profile. Checks that account 2 was removed from the original profile.
IN_PROC_BROWSER_TEST_F(LiveSignInTest,
                       MANUAL_SyncSecondAccount_CreateNewProfile) {
  // Enable and disable sync for the first account.
  TestAccount test_account_1;
  CHECK(GetTestAccountsUtil()->GetAccount("TEST_ACCOUNT_1", test_account_1));
  sign_in_functions.TurnOnSync(test_account_1, 0);
  sign_in_functions.TurnOffSync();

  // Start enable sync for the second account.
  TestAccount test_account_2;
  CHECK(GetTestAccountsUtil()->GetAccount("TEST_ACCOUNT_2", test_account_2));
  sign_in_functions.SignInFromSettings(test_account_2, 0);

  // Set up an observer for removing the second account from the original
  // profile.
  SignInTestObserver original_browser_observer(identity_manager(),
                                               account_reconcilor());

  // Check there is only one profile.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  EXPECT_EQ(profile_manager->GetNumberOfProfiles(), 1U);
  EXPECT_EQ(chrome::GetTotalBrowserCount(), 1U);

  // Click "This wasn't me" on the email confirmation dialog and wait for a new
  // browser and profile created.
  EXPECT_TRUE(login_ui_test_utils::CompleteSigninEmailConfirmationDialog(
      browser(), kDialogTimeout,
      SigninEmailConfirmationDialog::CREATE_NEW_USER));
  Browser* new_browser = ui_test_utils::WaitForBrowserToOpen();
  EXPECT_EQ(profile_manager->GetNumberOfProfiles(), 2U);
  EXPECT_EQ(chrome::GetTotalBrowserCount(), 2U);
  EXPECT_NE(browser()->profile(), new_browser->profile());

  // Confirm sync in the new browser window.
  SignInTestObserver new_browser_observer(
      signin::test::identity_manager(new_browser),
      signin::test::account_reconcilor(new_browser));
  EXPECT_TRUE(login_ui_test_utils::ConfirmSyncConfirmationDialog(
      new_browser, kDialogTimeout));
  new_browser_observer.WaitForAccountChanges(
      1, PrimarySyncAccountWait::kWaitForAdded);

  // Check accounts in cookies in the new profile.
  const AccountsInCookieJarInfo& accounts_in_cookie_jar =
      signin::test::identity_manager(new_browser)->GetAccountsInCookieJar();
  EXPECT_TRUE(accounts_in_cookie_jar.accounts_are_fresh);
  ASSERT_EQ(1u, accounts_in_cookie_jar.signed_in_accounts.size());
  const gaia::ListedAccount& account =
      accounts_in_cookie_jar.signed_in_accounts[0];
  EXPECT_TRUE(gaia::AreEmailsSame(test_account_2.user, account.email));

  // Check the primary account in the new profile is set and syncing.
  const CoreAccountInfo& primary_account =
      signin::test::identity_manager(new_browser)
          ->GetPrimaryAccountInfo(signin::ConsentLevel::kSync);
  EXPECT_FALSE(primary_account.IsEmpty());
  EXPECT_TRUE(gaia::AreEmailsSame(test_account_2.user, primary_account.email));
  EXPECT_TRUE(signin::test::identity_manager(new_browser)
                  ->HasAccountWithRefreshToken(primary_account.account_id));
  EXPECT_TRUE(signin::test::sync_service(new_browser)->IsSyncFeatureEnabled());

  // Check that the second account was removed from the original profile.
  original_browser_observer.WaitForAccountChanges(
      0, PrimarySyncAccountWait::kWaitForCleared);
  const AccountsInCookieJarInfo& accounts_in_cookie_jar_2 =
      identity_manager()->GetAccountsInCookieJar();
  EXPECT_TRUE(accounts_in_cookie_jar_2.accounts_are_fresh);
  ASSERT_TRUE(accounts_in_cookie_jar_2.signed_in_accounts.empty());
  EXPECT_TRUE(identity_manager()->GetAccountsWithRefreshTokens().empty());
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
}

// This test can pass. Marked as manual because it TIMED_OUT on Win7.
// See crbug.com/1025335.
// Enables and disables sync to account 1. Enables sync to account 2 and clicks
// on "This was me" in the email confirmation dialog. Checks that Sync to
// account 2 is enabled in the current profile.
IN_PROC_BROWSER_TEST_F(LiveSignInTest,
                       MANUAL_SyncSecondAccount_InExistingProfile) {
  // Enable and disable sync for the first account.
  TestAccount test_account_1;
  CHECK(GetTestAccountsUtil()->GetAccount("TEST_ACCOUNT_1", test_account_1));
  sign_in_functions.TurnOnSync(test_account_1, 0);
  sign_in_functions.TurnOffSync();

  // Start enable sync for the second account.
  TestAccount test_account_2;
  CHECK(GetTestAccountsUtil()->GetAccount("TEST_ACCOUNT_2", test_account_2));
  sign_in_functions.SignInFromSettings(test_account_2, 0);

  // Check there is only one profile.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  EXPECT_EQ(profile_manager->GetNumberOfProfiles(), 1U);
  EXPECT_EQ(chrome::GetTotalBrowserCount(), 1U);

  // Click "This was me" on the email confirmation dialog, confirm sync and wait
  // for a primary account to be set.
  SignInTestObserver observer(identity_manager(), account_reconcilor());
  EXPECT_TRUE(login_ui_test_utils::CompleteSigninEmailConfirmationDialog(
      browser(), kDialogTimeout, SigninEmailConfirmationDialog::START_SYNC));
  EXPECT_TRUE(login_ui_test_utils::ConfirmSyncConfirmationDialog(
      browser(), kDialogTimeout));
  observer.WaitForAccountChanges(1, PrimarySyncAccountWait::kWaitForAdded);

  // Check no profile was created.
  EXPECT_EQ(profile_manager->GetNumberOfProfiles(), 1U);
  EXPECT_EQ(chrome::GetTotalBrowserCount(), 1U);

  // Check accounts in cookies.
  const AccountsInCookieJarInfo& accounts_in_cookie_jar =
      identity_manager()->GetAccountsInCookieJar();
  EXPECT_TRUE(accounts_in_cookie_jar.accounts_are_fresh);
  ASSERT_EQ(1u, accounts_in_cookie_jar.signed_in_accounts.size());
  const gaia::ListedAccount& account =
      accounts_in_cookie_jar.signed_in_accounts[0];
  EXPECT_TRUE(gaia::AreEmailsSame(test_account_2.user, account.email));

  // Check the primary account is set and syncing.
  const CoreAccountInfo& primary_account =
      identity_manager()->GetPrimaryAccountInfo(signin::ConsentLevel::kSync);
  EXPECT_FALSE(primary_account.IsEmpty());
  EXPECT_TRUE(gaia::AreEmailsSame(test_account_2.user, primary_account.email));
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(
      primary_account.account_id));
  EXPECT_TRUE(sync_service()->IsSyncFeatureEnabled());
}

// This test can pass. Marked as manual because it TIMED_OUT on Win7.
// See crbug.com/1025335.
// Enables and disables sync to account 1. Enables sync to account 2 and clicks
// on "Cancel" in the email confirmation dialog. Checks that the signin flow is
// canceled and no accounts are added to Chrome.
IN_PROC_BROWSER_TEST_F(LiveSignInTest,
                       MANUAL_SyncSecondAccount_CancelOnEmailConfirmation) {
  // Enable and disable sync for the first account.
  TestAccount test_account_1;
  CHECK(GetTestAccountsUtil()->GetAccount("TEST_ACCOUNT_1", test_account_1));
  sign_in_functions.TurnOnSync(test_account_1, 0);
  sign_in_functions.TurnOffSync();

  // Start enable sync for the second account.
  TestAccount test_account_2;
  CHECK(GetTestAccountsUtil()->GetAccount("TEST_ACCOUNT_2", test_account_2));
  sign_in_functions.SignInFromSettings(test_account_2, 0);

  // Check there is only one profile.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  EXPECT_EQ(profile_manager->GetNumberOfProfiles(), 1U);
  EXPECT_EQ(chrome::GetTotalBrowserCount(), 1U);

  // Click "Cancel" on the email confirmation dialog and wait for an account to
  // removed from Chrome.
  SignInTestObserver observer(identity_manager(), account_reconcilor());
  EXPECT_TRUE(login_ui_test_utils::CompleteSigninEmailConfirmationDialog(
      browser(), kDialogTimeout, SigninEmailConfirmationDialog::CLOSE));
  observer.WaitForAccountChanges(0, PrimarySyncAccountWait::kWaitForCleared);

  // Check no profile was created.
  EXPECT_EQ(profile_manager->GetNumberOfProfiles(), 1U);
  EXPECT_EQ(chrome::GetTotalBrowserCount(), 1U);

  // Check Chrome has no accounts.
  const AccountsInCookieJarInfo& accounts_in_cookie_jar =
      identity_manager()->GetAccountsInCookieJar();
  EXPECT_TRUE(accounts_in_cookie_jar.accounts_are_fresh);
  EXPECT_TRUE(accounts_in_cookie_jar.signed_in_accounts.empty());
  EXPECT_TRUE(identity_manager()->GetAccountsWithRefreshTokens().empty());
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
}

IN_PROC_BROWSER_TEST_F(LiveSignInTest,
                       MANUAL_AccountCapabilities_FetchedOnSignIn) {
  EnableAccountCapabilitiesFetches(identity_manager());

  // Test primary adult account.
  {
    AccountCapabilitiesObserver capabilities_observer(identity_manager());

    TestAccount ta;
    ASSERT_TRUE(GetTestAccountsUtil()->GetAccount("TEST_ACCOUNT_1", ta));
    sign_in_functions.SignInFromSettings(ta, 0);

    CoreAccountInfo core_account_info =
        identity_manager()->GetPrimaryAccountInfo(ConsentLevel::kSignin);
    ASSERT_TRUE(gaia::AreEmailsSame(core_account_info.email, ta.user));

    capabilities_observer.WaitForAllCapabilitiesToBeKnown(
        core_account_info.account_id);
    AccountInfo account_info =
        identity_manager()->FindExtendedAccountInfoByAccountId(
            core_account_info.account_id);
    EXPECT_EQ(account_info.capabilities.can_offer_extended_chrome_sync_promos(),
              Tribool::kTrue);
  }

  // Test secondary minor account.
  {
    AccountCapabilitiesObserver capabilities_observer(identity_manager());

    TestAccount ta;
    ASSERT_TRUE(GetTestAccountsUtil()->GetAccount("TEST_ACCOUNT_MINOR", ta));
    sign_in_functions.SignInFromWeb(ta, /*previously_signed_in_accounts=*/1);

    CoreAccountInfo core_account_info =
        identity_manager()->FindExtendedAccountInfoByEmailAddress(ta.user);
    ASSERT_FALSE(core_account_info.IsEmpty());

    capabilities_observer.WaitForAllCapabilitiesToBeKnown(
        core_account_info.account_id);
    AccountInfo account_info =
        identity_manager()->FindExtendedAccountInfoByAccountId(
            core_account_info.account_id);
    EXPECT_EQ(account_info.capabilities.can_offer_extended_chrome_sync_promos(),
              Tribool::kFalse);
  }
}

}  // namespace signin::test
