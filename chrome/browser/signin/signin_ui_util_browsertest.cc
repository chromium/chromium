// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_ui_util.h"

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_init_params.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/account_preview_data_service_factory.h"
#include "chrome/browser/signin/dice_tab_helper.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/signin_browser_test_base.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/signin/signin_ui_delegate.h"
#include "chrome/browser/signin/signin_ui_delegate_impl_dice.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/signin/promos/signin_promo_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/account_id/account_id.h"
#include "components/google/core/common/google_util.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "google_apis/gaia/gaia_id.h"
#include "google_apis/gaia/gaia_urls.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"

#if !BUILDFLAG(ENABLE_DICE_SUPPORT)
#error This file only contains DICE browser tests for now.
#endif

namespace signin_ui_util {

namespace {
const char kMainEmail[] = "main_email@example.com";
const GaiaId::Literal kMainGaiaID("main_gaia_id");
}  // namespace

using testing::_;

namespace {

// TODO(crbug.com/40834209): move out testing of SigninUiDelegateImplDice
// in a separate file.
class MockSigninUiDelegate : public SigninUiDelegateImplDice {
 public:
  MOCK_METHOD(void,
              ShowHistorySyncOptinUI,
              (Profile * profile,
               const CoreAccountId& account_id,
               signin_metrics::AccessPoint access_point),
              (override));
  MOCK_METHOD(void,
              ShowReauthUI,
              (Profile * profile,
               const std::string& email,
               bool enable_history_sync,
               signin_metrics::AccessPoint access_point,
               signin_metrics::PromoAction promo_action),
              (override));
};

std::unique_ptr<KeyedService> CreateTestSyncService(content::BrowserContext*) {
  return std::make_unique<syncer::TestSyncService>();
}

}  // namespace

class SigninUiUtilTest : public SigninBrowserTestBase {
 public:
  SigninUiUtilTest()
      : delegate_auto_reset_(SetSigninUiDelegateForTesting(&mock_delegate_)) {
    ON_CALL(mock_delegate_, ShowReauthUI)
        .WillByDefault([this](Profile* profile, const std::string& email,
                              bool enable_history_sync,
                              signin_metrics::AccessPoint access_point,
                              signin_metrics::PromoAction promo_action) {
          mock_delegate_.SigninUiDelegateImplDice::ShowReauthUI(
              profile, email, enable_history_sync, access_point, promo_action);
        });
  }

 protected:
  // Returns the identity manager.
  signin::IdentityManager* GetIdentityManager() {
    return IdentityManagerFactory::GetForProfile(browser()->GetProfile());
  }

  void EnableSync(const CoreAccountInfo& account_info,
                  bool is_default_promo_account) {
    EnableHistorySyncFromMultiAccountPromo(browser()->GetProfile(),
                                           account_info, access_point_,
                                           is_default_promo_account);
  }

  void SignIn(const CoreAccountInfo& account_info) {
    SignInFromSingleAccountPromo(browser()->GetProfile(), account_info,
                                 access_point_);
  }

  void ExpectNoSigninStartedHistograms(
      const base::HistogramTester& histogram_tester) {
    histogram_tester.ExpectTotalCount("Signin.SignIn.Started", 0);
    histogram_tester.ExpectTotalCount("Signin.SigninStartedAccessPoint", 0);
    histogram_tester.ExpectTotalCount(
        "Signin.SigninStartedAccessPoint.WithDefault", 0);
    histogram_tester.ExpectTotalCount(
        "Signin.SigninStartedAccessPoint.NotDefault", 0);
    histogram_tester.ExpectTotalCount(
        "Signin.SigninStartedAccessPoint.NewAccountNoExistingAccount", 0);
    histogram_tester.ExpectTotalCount(
        "Signin.SigninStartedAccessPoint.NewAccountExistingAccount", 0);
  }

  void ExpectOneSigninStartedHistograms(
      const base::HistogramTester& histogram_tester,
      signin_metrics::PromoAction expected_promo_action) {
    histogram_tester.ExpectUniqueSample("Signin.SignIn.Started", access_point_,
                                        1);
    histogram_tester.ExpectUniqueSample("Signin.SigninStartedAccessPoint",
                                        access_point_, 1);
    switch (expected_promo_action) {
      case signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO:
        histogram_tester.ExpectTotalCount(
            "Signin.SigninStartedAccessPoint.NotDefault", 0);
        histogram_tester.ExpectTotalCount(
            "Signin.SigninStartedAccessPoint.WithDefault", 0);
        histogram_tester.ExpectTotalCount(
            "Signin.SigninStartedAccessPoint.NewAccountNoExistingAccount", 0);
        histogram_tester.ExpectTotalCount(
            "Signin.SigninStartedAccessPoint.NewAccountExistingAccount", 0);
        break;
      case signin_metrics::PromoAction::PROMO_ACTION_WITH_DEFAULT:
        histogram_tester.ExpectTotalCount(
            "Signin.SigninStartedAccessPoint.NotDefault", 0);
        histogram_tester.ExpectUniqueSample(
            "Signin.SigninStartedAccessPoint.WithDefault", access_point_, 1);
        histogram_tester.ExpectTotalCount(
            "Signin.SigninStartedAccessPoint.NewAccountNoExistingAccount", 0);
        histogram_tester.ExpectTotalCount(
            "Signin.SigninStartedAccessPoint.NewAccountExistingAccount", 0);
        break;
      case signin_metrics::PromoAction::PROMO_ACTION_NOT_DEFAULT:
        histogram_tester.ExpectTotalCount(
            "Signin.SigninStartedAccessPoint.WithDefault", 0);
        histogram_tester.ExpectUniqueSample(
            "Signin.SigninStartedAccessPoint.NotDefault", access_point_, 1);
        histogram_tester.ExpectTotalCount(
            "Signin.SigninStartedAccessPoint.NewAccountNoExistingAccount", 0);
        histogram_tester.ExpectTotalCount(
            "Signin.SigninStartedAccessPoint.NewAccountExistingAccount", 0);
        break;
      case signin_metrics::PromoAction::
          PROMO_ACTION_NEW_ACCOUNT_NO_EXISTING_ACCOUNT:
        histogram_tester.ExpectTotalCount(
            "Signin.SigninStartedAccessPoint.WithDefault", 0);
        histogram_tester.ExpectTotalCount(
            "Signin.SigninStartedAccessPoint.NotDefault", 0);
        histogram_tester.ExpectUniqueSample(
            "Signin.SigninStartedAccessPoint.NewAccountNoExistingAccount",
            access_point_, 1);
        histogram_tester.ExpectTotalCount(
            "Signin.SigninStartedAccessPoint.NewAccountExistingAccount", 0);
        break;
      case signin_metrics::PromoAction::
          PROMO_ACTION_NEW_ACCOUNT_EXISTING_ACCOUNT:
        histogram_tester.ExpectTotalCount(
            "Signin.SigninStartedAccessPoint.WithDefault", 0);
        histogram_tester.ExpectTotalCount(
            "Signin.SigninStartedAccessPoint.NotDefault", 0);
        histogram_tester.ExpectTotalCount(
            "Signin.SigninStartedAccessPoint.NewAccountNoExistingAccount", 0);
        histogram_tester.ExpectUniqueSample(
            "Signin.SigninStartedAccessPoint.NewAccountExistingAccount",
            access_point_, 1);
        break;
    }
  }

  signin_metrics::AccessPoint access_point_ =
      signin_metrics::AccessPoint::kBookmarkBubble;

  testing::StrictMock<MockSigninUiDelegate> mock_delegate_;
  base::AutoReset<SigninUiDelegate*> delegate_auto_reset_;
};

IN_PROC_BROWSER_TEST_F(SigninUiUtilTest, EnableSyncWithExistingAccount) {
  CoreAccountId account_id =
      GetIdentityManager()->GetAccountsMutator()->AddOrUpdateAccount(
          kMainGaiaID, kMainEmail, "refresh_token", false,
          signin_metrics::AccessPoint::kStartPage,
          signin_metrics::SourceForRefreshTokenOperation::kUnknown);
  GetIdentityManager()->GetPrimaryAccountMutator()->SetPrimaryAccount(
      account_id, signin::ConsentLevel::kSignin,
      signin_metrics::AccessPoint::kStartPage);

  for (bool is_default_promo_account : {true, false}) {
    base::HistogramTester histogram_tester;
    base::UserActionTester user_action_tester;

    EXPECT_EQ(0, user_action_tester.GetActionCount(
                     "Signin_Signin_FromBookmarkBubble"));

    EXPECT_CALL(
        mock_delegate_,
        ShowHistorySyncOptinUI(browser()->GetProfile(), account_id,
                               signin_metrics::AccessPoint::kBookmarkBubble));
    EnableSync(GetIdentityManager()
                   ->FindExtendedAccountInfoByAccountId(account_id)
                   .GetCoreAccountInfo(),
               is_default_promo_account);

    ExpectNoSigninStartedHistograms(histogram_tester);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Signin_Signin_FromBookmarkBubble"));
  }
}

IN_PROC_BROWSER_TEST_F(SigninUiUtilTest, EnableSyncWithAccountThatNeedsReauth) {
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("http://example.com"),
      WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  CoreAccountId account_id =
      GetIdentityManager()->GetAccountsMutator()->AddOrUpdateAccount(
          kMainGaiaID, kMainEmail, "refresh_token", false,
          signin_metrics::AccessPoint::kStartPage,
          signin_metrics::SourceForRefreshTokenOperation::kUnknown);

  // Add an account and then put its refresh token into an error state to
  // require a reauth before enabling sync.
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      GetIdentityManager(), account_id,
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::UNKNOWN));

  for (bool is_default_promo_account : {true, false}) {
    base::HistogramTester histogram_tester;
    base::UserActionTester user_action_tester;
    signin_metrics::PromoAction promo_action =
        is_default_promo_account
            ? signin_metrics::PromoAction::PROMO_ACTION_WITH_DEFAULT
            : signin_metrics::PromoAction::PROMO_ACTION_NOT_DEFAULT;

    ExpectNoSigninStartedHistograms(histogram_tester);
    EXPECT_EQ(0, user_action_tester.GetActionCount(
                     "Signin_Signin_FromBookmarkBubble"));
    EXPECT_CALL(mock_delegate_,
                ShowReauthUI(browser()->GetProfile(), kMainEmail,
                             /*enable_history_sync=*/true, access_point_,
                             promo_action));

    EnableSync(GetIdentityManager()
                   ->FindExtendedAccountInfoByAccountId(account_id)
                   .GetCoreAccountInfo(),
               is_default_promo_account);

    ExpectOneSigninStartedHistograms(histogram_tester, promo_action);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Signin_Signin_FromBookmarkBubble"));

    // Verify that the active tab has the correct DICE sign-in URL.
    TabStripModel* tab_strip = browser()->GetTabStripModel();
    content::WebContents* active_contents = tab_strip->GetActiveWebContents();
    ASSERT_TRUE(active_contents);
    EXPECT_EQ(signin::GetChromeSyncURLForDice(
                  {kMainEmail, GURL(google_util::kGoogleHomepageURL)}),
              active_contents->GetVisibleURL());
    tab_strip->CloseWebContents(active_contents,
                                TabCloseTypes::CLOSE_USER_GESTURE);
  }
}

IN_PROC_BROWSER_TEST_F(SigninUiUtilTest, EnableSyncForNewAccountWithNoTab) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  ExpectNoSigninStartedHistograms(histogram_tester);
  EXPECT_EQ(
      0, user_action_tester.GetActionCount("Signin_Signin_FromBookmarkBubble"));

  EnableSync(CoreAccountInfo(), false /* is_default_promo_account (not used)*/);

  ExpectOneSigninStartedHistograms(
      histogram_tester, signin_metrics::PromoAction::
                            PROMO_ACTION_NEW_ACCOUNT_NO_EXISTING_ACCOUNT);
  EXPECT_EQ(
      1, user_action_tester.GetActionCount("Signin_Signin_FromBookmarkBubble"));

  // Verify that the active tab has the correct DICE sign-in URL.
  content::WebContents* active_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  ASSERT_TRUE(active_contents);
  EXPECT_EQ(signin::GetChromeSyncURLForDice(
                {.continue_url = GURL(google_util::kGoogleHomepageURL)}),
            active_contents->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_F(SigninUiUtilTest,
                       EnableSyncForNewAccountWithNoTabWithExisting) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  GetIdentityManager()->GetAccountsMutator()->AddOrUpdateAccount(
      kMainGaiaID, kMainEmail, "refresh_token", false,
      signin_metrics::AccessPoint::kStartPage,
      signin_metrics::SourceForRefreshTokenOperation::kUnknown);

  ExpectNoSigninStartedHistograms(histogram_tester);
  EXPECT_EQ(
      0, user_action_tester.GetActionCount("Signin_Signin_FromBookmarkBubble"));

  EnableSync(CoreAccountInfo(), false /* is_default_promo_account (not used)*/);

  ExpectOneSigninStartedHistograms(
      histogram_tester,
      signin_metrics::PromoAction::PROMO_ACTION_NEW_ACCOUNT_EXISTING_ACCOUNT);
  EXPECT_EQ(
      1, user_action_tester.GetActionCount("Signin_Signin_FromBookmarkBubble"));
}

IN_PROC_BROWSER_TEST_F(SigninUiUtilTest, EnableSyncForNewAccountWithOneTab) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("http://foo/1"), WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  ExpectNoSigninStartedHistograms(histogram_tester);
  EXPECT_EQ(
      0, user_action_tester.GetActionCount("Signin_Signin_FromBookmarkBubble"));

  EnableSync(CoreAccountInfo(), false /* is_default_promo_account (not used)*/);

  ExpectOneSigninStartedHistograms(
      histogram_tester, signin_metrics::PromoAction::
                            PROMO_ACTION_NEW_ACCOUNT_NO_EXISTING_ACCOUNT);
  EXPECT_EQ(
      1, user_action_tester.GetActionCount("Signin_Signin_FromBookmarkBubble"));

  // Verify that the active tab has the correct DICE sign-in URL.
  content::WebContents* active_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  ASSERT_TRUE(active_contents);
  EXPECT_EQ(signin::GetChromeSyncURLForDice(
                {.continue_url = GURL(google_util::kGoogleHomepageURL)}),
            active_contents->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_F(SigninUiUtilTest, SignInWithAlreadySignedInAccount) {
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://example.com"),
      WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  CoreAccountId account_id =
      GetIdentityManager()->GetAccountsMutator()->AddOrUpdateAccount(
          kMainGaiaID, kMainEmail, "refresh_token", false,
          signin_metrics::AccessPoint::kStartPage,
          signin_metrics::SourceForRefreshTokenOperation::kUnknown);
  GetIdentityManager()->GetPrimaryAccountMutator()->SetPrimaryAccount(
      account_id, signin::ConsentLevel::kSignin,
      signin_metrics::AccessPoint::kStartPage);

  SignIn(GetIdentityManager()
             ->FindExtendedAccountInfoByAccountId(account_id)
             .GetCoreAccountInfo());

  // Verify that the primary account is still set.
  EXPECT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  // Verify that the active tab does not open the DICE sign-in URL.
  TabStripModel* tab_strip = browser()->GetTabStripModel();
  content::WebContents* active_contents = tab_strip->GetActiveWebContents();
  ASSERT_TRUE(active_contents);
  EXPECT_EQ(GURL("https://example.com"), active_contents->GetVisibleURL());
  tab_strip->CloseWebContents(active_contents,
                              TabCloseTypes::CLOSE_USER_GESTURE);
}

IN_PROC_BROWSER_TEST_F(SigninUiUtilTest, SignInWithAccountThatNeedsReauth) {
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("http://example.com"),
      WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  CoreAccountId account_id =
      GetIdentityManager()->GetAccountsMutator()->AddOrUpdateAccount(
          kMainGaiaID, kMainEmail, "refresh_token", false,
          signin_metrics::AccessPoint::kStartPage,
          signin_metrics::SourceForRefreshTokenOperation::kUnknown);

  // Add an account and then put its refresh token into an error state to
  // require a reauth before signing in.
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      GetIdentityManager(), account_id,
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::UNKNOWN));

  EXPECT_CALL(
      mock_delegate_,
      ShowReauthUI(browser()->GetProfile(), kMainEmail,
                   /*enable_history_sync=*/false, access_point_,
                   signin_metrics::PromoAction::PROMO_ACTION_WITH_DEFAULT));
  SignIn(GetIdentityManager()
             ->FindExtendedAccountInfoByAccountId(account_id)
             .GetCoreAccountInfo());

  // Verify that the active tab has the correct DICE sign-in URL.
  TabStripModel* tab_strip = browser()->GetTabStripModel();
  content::WebContents* active_contents = tab_strip->GetActiveWebContents();
  ASSERT_TRUE(active_contents);
  EXPECT_EQ(signin::GetAddAccountURLForDice(
                kMainEmail, GURL(google_util::kGoogleHomepageURL)),
            active_contents->GetVisibleURL());
  tab_strip->CloseWebContents(active_contents,
                              TabCloseTypes::CLOSE_USER_GESTURE);
}

IN_PROC_BROWSER_TEST_F(SigninUiUtilTest, SignInForNewAccountWithNoTab) {
  SignIn(CoreAccountInfo());

  // Verify that the active tab has the correct DICE sign-in URL.
  content::WebContents* active_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  ASSERT_TRUE(active_contents);
  EXPECT_EQ(signin::GetChromeSyncURLForDice(
                {.email = std::string(),
                 .continue_url = GURL(google_util::kGoogleHomepageURL)}),
            active_contents->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_F(SigninUiUtilTest, SignInForNewAccountWithOneTab) {
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("http://foo/1"), WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  SignIn(CoreAccountInfo());

  // Verify that the active tab has the correct DICE sign-in URL.
  content::WebContents* active_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  ASSERT_TRUE(active_contents);
  EXPECT_EQ(signin::GetChromeSyncURLForDice(
                {.email = std::string(),
                 .continue_url = GURL(google_util::kGoogleHomepageURL)}),
            active_contents->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_F(SigninUiUtilTest, GetOrderedAccountsForDisplay) {
  auto enable_disclaimer_on_primary_account_change_resetter =
      enterprise_util::DisableAutomaticManagementDisclaimerUntilReset(
          browser()->GetProfile());
  Profile* profile = browser()->GetProfile();
  auto* account_preview_data_service =
      AccountPreviewDataServiceFactory::GetForProfile(profile);
  signin::IdentityManager* identity_manager_empty =
      IdentityManagerFactory::GetForProfile(profile);
  std::vector<AccountInfo> accounts_empty = GetOrderedAccountsForDisplay(
      identity_manager_empty, account_preview_data_service,
      /*restrict_to_accounts_eligible_for_signin=*/true);
  EXPECT_TRUE(accounts_empty.empty());

  // Fill with accounts.
  const char kTestEmail1[] = "me1@gmail.com";
  const char kTestEmail2[] = "me2@gmail.com";
  const char kTestEmail3[] = "me3@gmail.com";
  const char kTestEmail4[] = "me4@gmail.com";

  signin::IdentityTestEnvironment* test_env = identity_test_env();
  signin::IdentityManager* identity_manager = test_env->identity_manager();

  // The cookies are added separately in order to show behaviour in the case
  // that refresh tokens and cookies are not added at the same time.
  test_env->MakeAccountAvailable(kTestEmail1);
  test_env->MakeAccountAvailable(kTestEmail2);
  test_env->MakeAccountAvailable(kTestEmail3);
  test_env->MakeAccountAvailable(kTestEmail4);

  test_env->SetCookieAccounts(
      {{kTestEmail4, signin::GetTestGaiaIdForEmail(kTestEmail4)},
       {kTestEmail3, signin::GetTestGaiaIdForEmail(kTestEmail3)},
       {kTestEmail2, signin::GetTestGaiaIdForEmail(kTestEmail2)},
       {kTestEmail1, signin::GetTestGaiaIdForEmail(kTestEmail1)}});

  // No primary account set.
  std::vector<AccountInfo> accounts = GetOrderedAccountsForDisplay(
      identity_manager, account_preview_data_service,
      /*restrict_to_accounts_eligible_for_signin=*/false);

  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kTestEmail4),
            accounts[0].GetGaiaId());
  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kTestEmail3),
            accounts[1].GetGaiaId());
  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kTestEmail2),
            accounts[2].GetGaiaId());
  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kTestEmail1),
            accounts[3].GetGaiaId());

  // Set a primary account.
  test_env->SetPrimaryAccount(kTestEmail3, signin::ConsentLevel::kSignin);
  accounts = GetOrderedAccountsForDisplay(
      identity_manager, account_preview_data_service,
      /*restrict_to_accounts_eligible_for_signin=*/false);

  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kTestEmail3),
            accounts[0].GetGaiaId());
  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kTestEmail4),
            accounts[1].GetGaiaId());
  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kTestEmail2),
            accounts[2].GetGaiaId());
  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kTestEmail1),
            accounts[3].GetGaiaId());

  // Set a different primary account.
  test_env->SetPrimaryAccount(kTestEmail1, signin::ConsentLevel::kSignin);
  accounts = GetOrderedAccountsForDisplay(
      identity_manager, account_preview_data_service,
      /*restrict_to_accounts_eligible_for_signin=*/false);

  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kTestEmail1),
            accounts[0].GetGaiaId());
  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kTestEmail4),
            accounts[1].GetGaiaId());
  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kTestEmail3),
            accounts[2].GetGaiaId());
  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kTestEmail2),
            accounts[3].GetGaiaId());

  // Primary account should still be included if not in cookies, other accounts
  // should not.
  test_env->SetCookieAccounts(
      {{kTestEmail4, signin::GetTestGaiaIdForEmail(kTestEmail4)},
       {kTestEmail2, signin::GetTestGaiaIdForEmail(kTestEmail2)}});
  accounts = GetOrderedAccountsForDisplay(
      identity_manager, account_preview_data_service,
      /*restrict_to_accounts_eligible_for_signin=*/false);

  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kTestEmail1),
            accounts[0].GetGaiaId());
  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kTestEmail4),
            accounts[1].GetGaiaId());
  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kTestEmail2),
            accounts[2].GetGaiaId());
}

IN_PROC_BROWSER_TEST_F(SigninUiUtilTest, MergeDiceSigninTab) {
  base::UserActionTester user_action_tester;
  EnableSync(CoreAccountInfo(), false);
  EXPECT_EQ(
      1, user_action_tester.GetActionCount("Signin_Signin_FromBookmarkBubble"));

  // Signin tab is reused.
  EnableSync(CoreAccountInfo(), false);
  EXPECT_EQ(
      1, user_action_tester.GetActionCount("Signin_Signin_FromBookmarkBubble"));

  // Give focus to a different tab.
  TabStripModel* tab_strip = browser()->GetTabStripModel();
  ASSERT_EQ(0, tab_strip->active_index());
  GURL other_url = GURL("https://example.com");

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), other_url, WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  tab_strip->ActivateTabAt(
      1, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  ASSERT_EQ(other_url, tab_strip->GetActiveWebContents()->GetVisibleURL());
  ASSERT_EQ(1, tab_strip->active_index());

  // Extensions reuse the tab but do not take focus.
  access_point_ = signin_metrics::AccessPoint::kExtensions;
  EnableSync(CoreAccountInfo(), false);
  EXPECT_EQ(
      1, user_action_tester.GetActionCount("Signin_Signin_FromBookmarkBubble"));
  EXPECT_EQ(2, tab_strip->active_index());

  // Other access points reuse the tab and take focus.
  access_point_ = signin_metrics::AccessPoint::kSettings;
  EnableSync(CoreAccountInfo(), false);
  EXPECT_EQ(
      1, user_action_tester.GetActionCount("Signin_Signin_FromBookmarkBubble"));
  EXPECT_EQ(2, tab_strip->active_index());
}

IN_PROC_BROWSER_TEST_F(SigninUiUtilTest, ShowReauthTab) {
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("http://example.com"),
      WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  AccountInfo account_info = signin::MakePrimaryAccountAvailable(
      GetIdentityManager(), "foo@example.com", signin::ConsentLevel::kSync);

  // Add an account and then put its refresh token into an error state to
  // require a reauth before enabling sync.
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      GetIdentityManager(), account_info.GetAccountId(),
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::UNKNOWN));

  EXPECT_CALL(
      mock_delegate_,
      ShowReauthUI(browser()->GetProfile(), "foo@example.com",
                   /*enable_history_sync=*/false,
                   signin_metrics::AccessPoint::kProfileMenuPrimaryButton,
                   signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO));
  signin_ui_util::ShowReauthForPrimaryAccountWithAuthError(
      browser()->GetProfile(),
      signin_metrics::AccessPoint::kProfileMenuPrimaryButton);

  // Verify that the active tab has the correct DICE sign-in URL.
  TabStripModel* tab_strip = browser()->GetTabStripModel();
  content::WebContents* active_contents = tab_strip->GetActiveWebContents();
  ASSERT_TRUE(active_contents);
  EXPECT_THAT(
      active_contents->GetVisibleURL().spec(),
      testing::StartsWith(GaiaUrls::GetInstance()->add_account_url().spec()));
}

IN_PROC_BROWSER_TEST_F(SigninUiUtilTest, GetSignInTabWithAccessPoint) {
  signin::MakePrimaryAccountAvailable(GetIdentityManager(), "foo@example.com",
                                      signin::ConsentLevel::kSignin);

  Profile* profile = browser()->GetProfile();
  TabStripModel* tab_strip = browser()->GetTabStripModel();
  EXPECT_EQ(1, tab_strip->count());

  // Add tabs.
  EXPECT_CALL(mock_delegate_, ShowReauthUI).Times(4);
  ShowReauthForAccount(profile, "test1@gmail.com",
                       signin_metrics::AccessPoint::kSettings);
  ShowReauthForAccount(
      profile, "test2@gmail.com",
      signin_metrics::AccessPoint::kChromeSigninInterceptBubble);
  ShowReauthForAccount(profile, "test3@gmail.com",
                       signin_metrics::AccessPoint::kPasswordBubble);
  EXPECT_EQ(3, tab_strip->count());

  // Look for existing tab.
  content::WebContents* sign_in_tab = GetSignInTabWithAccessPoint(
      browser(), signin_metrics::AccessPoint::kChromeSigninInterceptBubble);
  EXPECT_EQ(signin::GetAddAccountURLForDice(
                "test2@gmail.com", GURL(google_util::kGoogleHomepageURL)),
            sign_in_tab->GetVisibleURL());

  // Look for non existing tab.
  sign_in_tab = GetSignInTabWithAccessPoint(
      browser(), signin_metrics::AccessPoint::kForcedSignin);
  EXPECT_EQ(nullptr, sign_in_tab);

  // Two tabs with the same access point, will return the first tab found.
  ShowReauthForAccount(profile, "test4@gmail.com",
                       signin_metrics::AccessPoint::kSettings);
  EXPECT_EQ(4, tab_strip->count());

  sign_in_tab = GetSignInTabWithAccessPoint(
      browser(), signin_metrics::AccessPoint::kSettings);
  EXPECT_EQ(signin::GetAddAccountURLForDice(
                "test1@gmail.com", GURL(google_util::kGoogleHomepageURL)),
            sign_in_tab->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_F(SigninUiUtilTest, EnableSyncWithExistingWebOnlyAccount) {
  CoreAccountId account_id =
      GetIdentityManager()->GetAccountsMutator()->AddOrUpdateAccount(
          kMainGaiaID, kMainEmail, "refresh_token", false,
          signin_metrics::AccessPoint::kStartPage,
          signin_metrics::SourceForRefreshTokenOperation::kUnknown);
  ASSERT_FALSE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  EXPECT_EQ(
      0, user_action_tester.GetActionCount("Signin_Signin_FromBookmarkBubble"));

  EXPECT_CALL(
      mock_delegate_,
      ShowHistorySyncOptinUI(browser()->GetProfile(), account_id,
                             signin_metrics::AccessPoint::kBookmarkBubble));
  EnableSync(GetIdentityManager()
                 ->FindExtendedAccountInfoByAccountId(account_id)
                 .GetCoreAccountInfo(),
             /*is_default_promo_account=*/true);

  EXPECT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  histogram_tester.ExpectBucketCount(
      "Signin.SignIn.Started", signin_metrics::AccessPoint::kBookmarkBubble, 1);
  EXPECT_EQ(
      1, user_action_tester.GetActionCount("Signin_Signin_FromBookmarkBubble"));
}

IN_PROC_BROWSER_TEST_F(SigninUiUtilTest, SignInWithExistingWebOnlyAccount) {
  CoreAccountId account_id =
      GetIdentityManager()->GetAccountsMutator()->AddOrUpdateAccount(
          kMainGaiaID, kMainEmail, "refresh_token", false,
          signin_metrics::AccessPoint::kStartPage,
          signin_metrics::SourceForRefreshTokenOperation::kUnknown);

  // Verify that the primary account is not set before.
  EXPECT_FALSE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  SignIn(GetIdentityManager()
             ->FindExtendedAccountInfoByAccountId(account_id)
             .GetCoreAccountInfo());

  // Verify that the primary account has been set.
  EXPECT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
}

IN_PROC_BROWSER_TEST_F(
    SigninUiUtilTest,
    ShouldShowAnimatedIdentityOnOpeningWindowIfMultipleWindowsAtStartup) {
  EXPECT_TRUE(
      ShouldShowAnimatedIdentityOnOpeningWindow(*browser()->GetProfile()));
  // Record that the identity was shown.
  RecordAnimatedIdentityTriggered(browser()->GetProfile());
  // The identity can be shown again immediately (which is what happens if there
  // is multiple windows at startup).
  EXPECT_TRUE(
      ShouldShowAnimatedIdentityOnOpeningWindow(*browser()->GetProfile()));
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

class SigninUiUtilTest_HistorySyncOptinTest : public SigninUiUtilTest {
 public:
  // This setup happens before SetUpOnMainThread() as an initial startup.
  void SetUpInProcessBrowserTestFixture() override {
    SigninUiUtilTest::SetUpInProcessBrowserTestFixture();
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &SigninUiUtilTest_HistorySyncOptinTest::SetupTestFactories,
                base::Unretained(this)));
  }

  void SetupTestFactories(content::BrowserContext* context) {
    SyncServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&CreateTestSyncService));
  }

 protected:
  syncer::TestSyncService* sync_service() {
    return static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetForProfile(browser()->GetProfile()));
  }

 private:
  base::CallbackListSubscription create_services_subscription_;
};

IN_PROC_BROWSER_TEST_F(SigninUiUtilTest_HistorySyncOptinTest,
                       ShowSignInUiForHistorySyncOptin_SignedOut) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;
  sync_service()->GetUserSettings()->SetSelectedTypes(false, {});

  SignInAndEnableHistorySync(browser(), browser()->GetProfile(),
                             signin_metrics::AccessPoint::kRecentTabs);
  content::WebContents* active_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  EXPECT_TRUE(SigninPromoTabHelper::GetForWebContents(*active_contents)
                  ->IsInitializedForTesting());
  DiceTabHelper* dice_tab_helper =
      DiceTabHelper::FromWebContents(active_contents);
  ASSERT_TRUE(dice_tab_helper);
  EXPECT_TRUE(dice_tab_helper->IsChromeSigninInProgress());
  EXPECT_EQ(signin_metrics::Reason::kSigninPrimaryAccount,
            dice_tab_helper->signin_reason());
  EXPECT_EQ(signin_metrics::AccessPoint::kRecentTabs,
            dice_tab_helper->signin_access_point());
  EXPECT_EQ(
      signin_metrics::PromoAction::PROMO_ACTION_NEW_ACCOUNT_NO_EXISTING_ACCOUNT,
      dice_tab_helper->signin_promo_action());
  EXPECT_EQ(chrome::ChromeUINewTabURLAsGURL(), dice_tab_helper->redirect_url());
  histogram_tester.ExpectUniqueSample("Signin.HistorySyncOptIn.Started",
                                      signin_metrics::AccessPoint::kRecentTabs,
                                      1);
  EXPECT_EQ(user_action_tester.GetActionCount("Signin_HistorySync_Started"), 1);
  histogram_tester.ExpectTotalCount("Signin.HistorySyncOptIn.Completed", 0);
  EXPECT_EQ(user_action_tester.GetActionCount("Signin_HistorySync_Completed"),
            0);

  // Signing in should also enable history sync.
  identity_test_env()->MakeAccountAvailable(
      signin::AccountAvailabilityOptionsBuilder()
          .AsPrimary(signin::ConsentLevel::kSignin)
          .WithAccessPoint(signin_metrics::AccessPoint::kRecentTabs)
          .Build("test@email.com"));

  EXPECT_TRUE(sync_service()->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kHistory));
  EXPECT_TRUE(sync_service()->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kTabs));
  EXPECT_TRUE(sync_service()->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kSavedTabGroups));
  histogram_tester.ExpectUniqueSample("Signin.HistorySyncOptIn.Started",
                                      signin_metrics::AccessPoint::kRecentTabs,
                                      1);
  EXPECT_EQ(user_action_tester.GetActionCount("Signin_HistorySync_Started"), 1);
  histogram_tester.ExpectUniqueSample("Signin.HistorySyncOptIn.Completed",
                                      signin_metrics::AccessPoint::kRecentTabs,
                                      1);
  EXPECT_EQ(user_action_tester.GetActionCount("Signin_HistorySync_Completed"),
            1);
}

IN_PROC_BROWSER_TEST_F(SigninUiUtilTest_HistorySyncOptinTest,
                       ShowSignInUiForHistorySyncOptin_WebSignedIn) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;
  // Sign in with an account, but only on the web. The primary account is not
  // set.
  AccountInfo info = signin::MakeAccountAvailable(
      identity_manager(),
      signin::AccountAvailabilityOptionsBuilder(test_url_loader_factory())
          .WithCookie()
          .WithAccessPoint(signin_metrics::AccessPoint::kRecentTabs)
          .Build("test@email.com"));

  sync_service()->GetUserSettings()->SetSelectedTypes(false, {});

  SignInAndEnableHistorySync(browser(), browser()->GetProfile(),
                             signin_metrics::AccessPoint::kRecentTabs);

  // The sign in tab should not be shown: user is expected to be signed in
  // silently by the `SignInAndEnableHistorySync()`.
  EXPECT_FALSE(SigninPromoTabHelper::GetForWebContents(
                   *browser()->GetTabStripModel()->GetActiveWebContents())
                   ->IsInitializedForTesting());
  EXPECT_TRUE(sync_service()->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kHistory));
  EXPECT_TRUE(sync_service()->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kTabs));
  EXPECT_TRUE(sync_service()->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kSavedTabGroups));
  histogram_tester.ExpectUniqueSample("Signin.HistorySyncOptIn.Started",
                                      signin_metrics::AccessPoint::kRecentTabs,
                                      1);
  EXPECT_EQ(user_action_tester.GetActionCount("Signin_HistorySync_Started"), 1);
  histogram_tester.ExpectUniqueSample("Signin.HistorySyncOptIn.Completed",
                                      signin_metrics::AccessPoint::kRecentTabs,
                                      1);
  EXPECT_EQ(user_action_tester.GetActionCount("Signin_HistorySync_Completed"),
            1);
}

IN_PROC_BROWSER_TEST_F(SigninUiUtilTest_HistorySyncOptinTest,
                       ShowSignInUiForHistorySyncOptin_SignInPending) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;
  AccountInfo info = signin::MakePrimaryAccountAvailable(
      GetIdentityManager(), "test@email.com", signin::ConsentLevel::kSignin);
  sync_service()->GetUserSettings()->SetSelectedTypes(false, {});
  identity_test_env()->SetInvalidRefreshTokenForPrimaryAccount();

  // Pre-condition: history sync is not enabled and the account is in error
  // state.
  ASSERT_FALSE(sync_service()->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kHistory));
  ASSERT_FALSE(sync_service()->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kTabs));
  ASSERT_FALSE(sync_service()->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kSavedTabGroups));
  ASSERT_EQ(signin_util::SignedInState::kSignInPending,
            signin_util::GetSignedInState(identity_manager()));

  // A regular reauth tab is expected to be shown.
  EXPECT_CALL(
      mock_delegate_,
      ShowReauthUI(browser()->GetProfile(), "test@email.com",
                   /*enable_history_sync=*/false,
                   signin_metrics::AccessPoint::kRecentTabs,
                   signin_metrics::PromoAction::PROMO_ACTION_WITH_DEFAULT));

  SignInAndEnableHistorySync(browser(), browser()->GetProfile(),
                             signin_metrics::AccessPoint::kRecentTabs);

  // History sync should be enabled immediately, before the reauth is completed.
  EXPECT_TRUE(sync_service()->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kHistory));
  EXPECT_TRUE(sync_service()->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kTabs));
  EXPECT_TRUE(sync_service()->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kSavedTabGroups));
  histogram_tester.ExpectUniqueSample("Signin.HistorySyncOptIn.Started",
                                      signin_metrics::AccessPoint::kRecentTabs,
                                      1);
  EXPECT_EQ(user_action_tester.GetActionCount("Signin_HistorySync_Started"), 1);
  histogram_tester.ExpectUniqueSample("Signin.HistorySyncOptIn.Completed",
                                      signin_metrics::AccessPoint::kRecentTabs,
                                      1);
  EXPECT_EQ(user_action_tester.GetActionCount("Signin_HistorySync_Completed"),
            1);

  // No SigninPromoTabHelper in this case.
  EXPECT_FALSE(SigninPromoTabHelper::GetForWebContents(
                   *browser()->GetTabStripModel()->GetActiveWebContents())
                   ->IsInitializedForTesting());
}

IN_PROC_BROWSER_TEST_F(
    SigninUiUtilTest_HistorySyncOptinTest,
    ShowSignInUiForHistorySyncOptin_SignedInWithoutHistorySync) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;
  identity_test_env()->MakePrimaryAccountAvailable(
      "test@email.com", signin::ConsentLevel::kSignin);

  sync_service()->GetUserSettings()->SetSelectedTypes(false, {});

  SignInAndEnableHistorySync(
      browser(), browser()->GetProfile(),
      signin_metrics::AccessPoint::kCollaborationShareTabGroup);

  EXPECT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  EXPECT_FALSE(SigninPromoTabHelper::GetForWebContents(
                   *browser()->GetTabStripModel()->GetActiveWebContents())
                   ->IsInitializedForTesting());

  EXPECT_TRUE(sync_service()->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kHistory));
  EXPECT_TRUE(sync_service()->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kTabs));
  EXPECT_TRUE(sync_service()->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kSavedTabGroups));
  histogram_tester.ExpectUniqueSample(
      "Signin.HistorySyncOptIn.Started",
      signin_metrics::AccessPoint::kCollaborationShareTabGroup, 1);
  EXPECT_EQ(user_action_tester.GetActionCount("Signin_HistorySync_Started"), 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.HistorySyncOptIn.Completed",
      signin_metrics::AccessPoint::kCollaborationShareTabGroup, 1);
  EXPECT_EQ(user_action_tester.GetActionCount("Signin_HistorySync_Completed"),
            1);
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

}  // namespace signin_ui_util
