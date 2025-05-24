// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_ui_util.h"

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/profiles/profile_attributes_init_params.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/signin/signin_ui_delegate.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/account_id/account_id.h"
#include "components/google/core/common/google_util.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "google_apis/gaia/gaia_id.h"
#include "google_apis/gaia/gaia_urls.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/signin/signin_ui_delegate_impl_dice.h"
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

namespace signin_ui_util {

namespace {
const char kMainEmail[] = "main_email@example.com";
const GaiaId::Literal kMainGaiaID("main_gaia_id");
}  // namespace

using testing::_;

TEST(GetAllowedDomainTest, WithInvalidPattern) {
  EXPECT_EQ(std::string(), GetAllowedDomain("email"));
  EXPECT_EQ(std::string(), GetAllowedDomain("email@a@b"));
  EXPECT_EQ(std::string(), GetAllowedDomain("email@a[b"));
  EXPECT_EQ(std::string(), GetAllowedDomain("@$"));
  EXPECT_EQ(std::string(), GetAllowedDomain("@\\E$"));
  EXPECT_EQ(std::string(), GetAllowedDomain("@\\E$a"));
  EXPECT_EQ(std::string(), GetAllowedDomain("email@"));
  EXPECT_EQ(std::string(), GetAllowedDomain("@"));
  EXPECT_EQ(std::string(), GetAllowedDomain("example@a.com|example@b.com"));
  EXPECT_EQ(std::string(), GetAllowedDomain(""));
}

TEST(GetAllowedDomainTest, WithValidPattern) {
  EXPECT_EQ("example.com", GetAllowedDomain("email@example.com"));
  EXPECT_EQ("example.com", GetAllowedDomain("email@example.com\\E"));
  EXPECT_EQ("example.com", GetAllowedDomain("email@example.com$"));
  EXPECT_EQ("example.com", GetAllowedDomain("email@example.com\\E$"));
  EXPECT_EQ("example.com", GetAllowedDomain("*@example.com\\E$"));
  EXPECT_EQ("example.com", GetAllowedDomain(".*@example.com\\E$"));
  EXPECT_EQ("example-1.com", GetAllowedDomain("email@example-1.com"));
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)

namespace {

// TODO(crbug.com/40834209): move out testing of SigninUiDelegateImplDice
// in a separate file.
class MockSigninUiDelegate : public SigninUiDelegateImplDice {
 public:
  MOCK_METHOD(void,
              ShowTurnSyncOnUI,
              (Profile * profile,
               signin_metrics::AccessPoint access_point,
               signin_metrics::PromoAction promo_action,
               const CoreAccountId& account_id,
               TurnSyncOnHelper::SigninAbortedMode signin_aborted_mode,
               bool is_sync_promo,
               bool turn_sync_on_signed_profile),
              ());
};

}  // namespace

class SigninUiUtilTest : public BrowserWithTestWindowTest {
 public:
  SigninUiUtilTest()
      : delegate_auto_reset_(SetSigninUiDelegateForTesting(&mock_delegate_)) {}
  ~SigninUiUtilTest() override = default;

 protected:
  // BrowserWithTestWindowTest:
  TestingProfile::TestingFactories GetTestingFactories() override {
    return IdentityTestEnvironmentProfileAdaptor::
        GetIdentityTestEnvironmentFactories();
  }

  // Returns the identity manager.
  signin::IdentityManager* GetIdentityManager() {
    return IdentityManagerFactory::GetForProfile(profile());
  }

  void EnableSync(const CoreAccountInfo& account_info,
                  bool is_default_promo_account) {
    EnableSyncFromMultiAccountPromo(profile(), account_info, access_point_,
                                    is_default_promo_account);
  }

  void SignIn(const CoreAccountInfo& account_info) {
    SignInFromSingleAccountPromo(profile(), account_info, access_point_);
  }

  void ExpectTurnSyncOn(signin_metrics::AccessPoint access_point,
                        signin_metrics::PromoAction promo_action,
                        const CoreAccountId& account_id,
                        TurnSyncOnHelper::SigninAbortedMode signin_aborted_mode,
                        bool is_sync_promo,
                        bool turn_sync_on_signed_profile) {
    EXPECT_CALL(mock_delegate_,
                ShowTurnSyncOnUI(profile(), access_point, promo_action,
                                 account_id, signin_aborted_mode, is_sync_promo,
                                 turn_sync_on_signed_profile));
  }

  void ExpectNoSigninStartedHistograms(
      const base::HistogramTester& histogram_tester) {
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

  void TestEnableSyncPromoWithExistingWebOnlyAccount() {
    CoreAccountId account_id =
        GetIdentityManager()->GetAccountsMutator()->AddOrUpdateAccount(
            GaiaId(kMainGaiaID), kMainEmail, "refresh_token", false,
            signin_metrics::AccessPoint::kUnknown,
            signin_metrics::SourceForRefreshTokenOperation::kUnknown);

    // Verify that the primary account is not set before.
    ASSERT_FALSE(
        GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

    ExpectTurnSyncOn(
        access_point_, signin_metrics::PromoAction::PROMO_ACTION_WITH_DEFAULT,
        account_id, TurnSyncOnHelper::SigninAbortedMode::KEEP_ACCOUNT,
        /*is_sync_promo=*/true, /*turn_sync_on_signed_profile=*/false);
    EnableSync(
        GetIdentityManager()->FindExtendedAccountInfoByAccountId(account_id),
        /*is_default_promo_account=*/true);

    // Verify that the primary account has been set.
    EXPECT_TRUE(
        GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  }

  signin_metrics::AccessPoint access_point_ =
      signin_metrics::AccessPoint::kBookmarkBubble;

  testing::StrictMock<MockSigninUiDelegate> mock_delegate_;
  base::AutoReset<SigninUiDelegate*> delegate_auto_reset_;
};

TEST_F(SigninUiUtilTest, EnableSyncWithExistingAccount) {
  CoreAccountId account_id =
      GetIdentityManager()->GetAccountsMutator()->AddOrUpdateAccount(
          kMainGaiaID, kMainEmail, "refresh_token", false,
          signin_metrics::AccessPoint::kUnknown,
          signin_metrics::SourceForRefreshTokenOperation::kUnknown);
  GetIdentityManager()->GetPrimaryAccountMutator()->SetPrimaryAccount(
      account_id, signin::ConsentLevel::kSignin,
      signin_metrics::AccessPoint::kUnknown);

  for (bool is_default_promo_account : {true, false}) {
    base::HistogramTester histogram_tester;
    base::UserActionTester user_action_tester;

    ExpectNoSigninStartedHistograms(histogram_tester);
    EXPECT_EQ(0, user_action_tester.GetActionCount(
                     "Signin_Signin_FromBookmarkBubble"));

    signin_metrics::PromoAction expected_promo_action =
        is_default_promo_account
            ? signin_metrics::PromoAction::PROMO_ACTION_WITH_DEFAULT
            : signin_metrics::PromoAction::PROMO_ACTION_NOT_DEFAULT;
    ExpectTurnSyncOn(
        signin_metrics::AccessPoint::kBookmarkBubble, expected_promo_action,
        account_id, TurnSyncOnHelper::SigninAbortedMode::KEEP_ACCOUNT,
        /*is_sync_promo=*/false, /*turn_sync_on_signed_profile=*/true);
    EnableSync(
        GetIdentityManager()->FindExtendedAccountInfoByAccountId(account_id),
        is_default_promo_account);

    ExpectOneSigninStartedHistograms(histogram_tester, expected_promo_action);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Signin_Signin_FromBookmarkBubble"));
  }
}

TEST_F(SigninUiUtilTest, EnableSyncWithAccountThatNeedsReauth) {
  AddTab(browser(), GURL("http://example.com"));
  CoreAccountId account_id =
      GetIdentityManager()->GetAccountsMutator()->AddOrUpdateAccount(
          kMainGaiaID, kMainEmail, "refresh_token", false,
          signin_metrics::AccessPoint::kUnknown,
          signin_metrics::SourceForRefreshTokenOperation::kUnknown);

  // Add an account and then put its refresh token into an error state to
  // require a reauth before enabling sync.
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      GetIdentityManager(), account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));

  for (bool is_default_promo_account : {true, false}) {
    base::HistogramTester histogram_tester;
    base::UserActionTester user_action_tester;

    ExpectNoSigninStartedHistograms(histogram_tester);
    EXPECT_EQ(0, user_action_tester.GetActionCount(
                     "Signin_Signin_FromBookmarkBubble"));

    EnableSync(
        GetIdentityManager()->FindExtendedAccountInfoByAccountId(account_id),
        is_default_promo_account);

    ExpectOneSigninStartedHistograms(
        histogram_tester,
        is_default_promo_account
            ? signin_metrics::PromoAction::PROMO_ACTION_WITH_DEFAULT
            : signin_metrics::PromoAction::PROMO_ACTION_NOT_DEFAULT);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Signin_Signin_FromBookmarkBubble"));

    // Verify that the active tab has the correct DICE sign-in URL.
    TabStripModel* tab_strip = browser()->tab_strip_model();
    content::WebContents* active_contents = tab_strip->GetActiveWebContents();
    ASSERT_TRUE(active_contents);
    EXPECT_EQ(signin::GetChromeSyncURLForDice(
                  {kMainEmail, GURL(google_util::kGoogleHomepageURL)}),
              active_contents->GetVisibleURL());
    tab_strip->CloseWebContentsAt(
        tab_strip->GetIndexOfWebContents(active_contents),
        TabCloseTypes::CLOSE_USER_GESTURE);
  }
}

TEST_F(SigninUiUtilTest, EnableSyncForNewAccountWithNoTab) {
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
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_contents);
  EXPECT_EQ(signin::GetChromeSyncURLForDice(
                {.continue_url = GURL(google_util::kGoogleHomepageURL)}),
            active_contents->GetVisibleURL());
}

TEST_F(SigninUiUtilTest, EnableSyncForNewAccountWithNoTabWithExisting) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  GetIdentityManager()->GetAccountsMutator()->AddOrUpdateAccount(
      kMainGaiaID, kMainEmail, "refresh_token", false,
      signin_metrics::AccessPoint::kUnknown,
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

TEST_F(SigninUiUtilTest, EnableSyncForNewAccountWithOneTab) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;
  AddTab(browser(), GURL("http://foo/1"));

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
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_contents);
  EXPECT_EQ(signin::GetChromeSyncURLForDice(
                {.continue_url = GURL(google_util::kGoogleHomepageURL)}),
            active_contents->GetVisibleURL());
}

TEST_F(SigninUiUtilTest, SignInWithAlreadySignedInAccount) {
  AddTab(browser(), GURL("http://example.com"));
  CoreAccountId account_id =
      GetIdentityManager()->GetAccountsMutator()->AddOrUpdateAccount(
          kMainGaiaID, kMainEmail, "refresh_token", false,
          signin_metrics::AccessPoint::kUnknown,
          signin_metrics::SourceForRefreshTokenOperation::kUnknown);
  GetIdentityManager()->GetPrimaryAccountMutator()->SetPrimaryAccount(
      account_id, signin::ConsentLevel::kSignin,
      signin_metrics::AccessPoint::kUnknown);

  SignIn(GetIdentityManager()->FindExtendedAccountInfoByAccountId(account_id));

  // Verify that the primary account is still set.
  EXPECT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  // Verify that the active tab does not open the DICE sign-in URL.
  TabStripModel* tab_strip = browser()->tab_strip_model();
  content::WebContents* active_contents = tab_strip->GetActiveWebContents();
  ASSERT_TRUE(active_contents);
  EXPECT_EQ(GURL("http://example.com"), active_contents->GetVisibleURL());
  tab_strip->CloseWebContentsAt(
      tab_strip->GetIndexOfWebContents(active_contents),
      TabCloseTypes::CLOSE_USER_GESTURE);
}

TEST_F(SigninUiUtilTest, SignInWithAccountThatNeedsReauth) {
  AddTab(browser(), GURL("http://example.com"));
  CoreAccountId account_id =
      GetIdentityManager()->GetAccountsMutator()->AddOrUpdateAccount(
          kMainGaiaID, kMainEmail, "refresh_token", false,
          signin_metrics::AccessPoint::kUnknown,
          signin_metrics::SourceForRefreshTokenOperation::kUnknown);

  // Add an account and then put its refresh token into an error state to
  // require a reauth before signing in.
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      GetIdentityManager(), account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));

  SignIn(GetIdentityManager()->FindExtendedAccountInfoByAccountId(account_id));

  // Verify that the active tab has the correct DICE sign-in URL.
  TabStripModel* tab_strip = browser()->tab_strip_model();
  content::WebContents* active_contents = tab_strip->GetActiveWebContents();
  ASSERT_TRUE(active_contents);
  EXPECT_EQ(signin::GetAddAccountURLForDice(
                kMainEmail, GURL(google_util::kGoogleHomepageURL)),
            active_contents->GetVisibleURL());
  tab_strip->CloseWebContentsAt(
      tab_strip->GetIndexOfWebContents(active_contents),
      TabCloseTypes::CLOSE_USER_GESTURE);
}

TEST_F(SigninUiUtilTest, SignInForNewAccountWithNoTab) {
  SignIn(CoreAccountInfo());

  // Verify that the active tab has the correct DICE sign-in URL.
  content::WebContents* active_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_contents);
  EXPECT_EQ(signin::GetAddAccountURLForDice(
                std::string(), GURL(google_util::kGoogleHomepageURL)),
            active_contents->GetVisibleURL());
}

TEST_F(SigninUiUtilTest, SignInForNewAccountWithOneTab) {
  AddTab(browser(), GURL("http://foo/1"));

  SignIn(CoreAccountInfo());

  // Verify that the active tab has the correct DICE sign-in URL.
  content::WebContents* active_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_contents);
  EXPECT_EQ(signin::GetAddAccountURLForDice(
                std::string(), GURL(google_util::kGoogleHomepageURL)),
            active_contents->GetVisibleURL());
}

TEST_F(SigninUiUtilTest, GetOrderedAccountsForDisplay) {
  signin::IdentityManager* identity_manager_empty =
      IdentityManagerFactory::GetForProfile(profile());
  std::vector<AccountInfo> accounts_empty = GetOrderedAccountsForDisplay(
      identity_manager_empty, /*restrict_to_accounts_eligible_for_sync=*/true);
  EXPECT_TRUE(accounts_empty.empty());

  // Fill with accounts.
  const char kTestEmail1[] = "me1@gmail.com";
  const char kTestEmail2[] = "me2@gmail.com";
  const char kTestEmail3[] = "me3@gmail.com";
  const char kTestEmail4[] = "me4@gmail.com";

  network::TestURLLoaderFactory url_loader_factory_ =
      network::TestURLLoaderFactory();
  signin::IdentityTestEnvironment identity_test_env(&url_loader_factory_);
  signin::IdentityManager* identity_manager =
      identity_test_env.identity_manager();

  // The cookies are added separately in order to show behaviour in the case
  // that refresh tokens and cookies are not added at the same time.
  identity_test_env.MakeAccountAvailable(kTestEmail1);
  identity_test_env.MakeAccountAvailable(kTestEmail2);
  identity_test_env.MakeAccountAvailable(kTestEmail3);
  identity_test_env.MakeAccountAvailable(kTestEmail4);

  identity_test_env.SetCookieAccounts(
      {{kTestEmail4, signin::GetTestGaiaIdForEmail(kTestEmail4)},
       {kTestEmail3, signin::GetTestGaiaIdForEmail(kTestEmail3)},
       {kTestEmail2, signin::GetTestGaiaIdForEmail(kTestEmail2)},
       {kTestEmail1, signin::GetTestGaiaIdForEmail(kTestEmail1)}});

  // No primary account set.
  std::vector<AccountInfo> accounts =
      GetOrderedAccountsForDisplay(identity_manager, false);

  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kTestEmail4), accounts[0].gaia);
  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kTestEmail3), accounts[1].gaia);
  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kTestEmail2), accounts[2].gaia);
  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kTestEmail1), accounts[3].gaia);

  // Set a primary account.
  identity_test_env.SetPrimaryAccount(kTestEmail3,
                                      signin::ConsentLevel::kSignin);
  accounts = GetOrderedAccountsForDisplay(identity_manager, false);

  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kTestEmail3), accounts[0].gaia);
  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kTestEmail4), accounts[1].gaia);
  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kTestEmail2), accounts[2].gaia);
  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kTestEmail1), accounts[3].gaia);

  // Set a different primary account.
  identity_test_env.SetPrimaryAccount(kTestEmail1,
                                      signin::ConsentLevel::kSignin);
  accounts = GetOrderedAccountsForDisplay(identity_manager, false);

  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kTestEmail1), accounts[0].gaia);
  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kTestEmail4), accounts[1].gaia);
  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kTestEmail3), accounts[2].gaia);
  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kTestEmail2), accounts[3].gaia);

  // Primary account should still be included if not in cookies, other accounts
  // should not.
  identity_test_env.SetCookieAccounts(
      {{kTestEmail4, signin::GetTestGaiaIdForEmail(kTestEmail4)},
       {kTestEmail2, signin::GetTestGaiaIdForEmail(kTestEmail2)}});
  accounts = GetOrderedAccountsForDisplay(identity_manager, false);

  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kTestEmail1), accounts[0].gaia);
  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kTestEmail4), accounts[1].gaia);
  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kTestEmail2), accounts[2].gaia);
}

TEST_F(SigninUiUtilTest, MergeDiceSigninTab) {
  base::UserActionTester user_action_tester;
  EnableSync(CoreAccountInfo(), false);
  EXPECT_EQ(
      1, user_action_tester.GetActionCount("Signin_Signin_FromBookmarkBubble"));

  // Signin tab is reused.
  EnableSync(CoreAccountInfo(), false);
  EXPECT_EQ(
      1, user_action_tester.GetActionCount("Signin_Signin_FromBookmarkBubble"));

  // Give focus to a different tab.
  TabStripModel* tab_strip = browser()->tab_strip_model();
  ASSERT_EQ(0, tab_strip->active_index());
  GURL other_url = GURL("http://example.com");
  AddTab(browser(), other_url);
  tab_strip->ActivateTabAt(
      0, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  ASSERT_EQ(other_url, tab_strip->GetActiveWebContents()->GetVisibleURL());
  ASSERT_EQ(0, tab_strip->active_index());

  // Extensions re-use the tab but do not take focus.
  access_point_ = signin_metrics::AccessPoint::kExtensions;
  EnableSync(CoreAccountInfo(), false);
  EXPECT_EQ(
      1, user_action_tester.GetActionCount("Signin_Signin_FromBookmarkBubble"));
  EXPECT_EQ(0, tab_strip->active_index());

  // Other access points re-use the tab and take focus.
  access_point_ = signin_metrics::AccessPoint::kSettings;
  EnableSync(CoreAccountInfo(), false);
  EXPECT_EQ(
      1, user_action_tester.GetActionCount("Signin_Signin_FromBookmarkBubble"));
  EXPECT_EQ(1, tab_strip->active_index());
}

TEST_F(SigninUiUtilTest, ShowReauthTab) {
  AddTab(browser(), GURL("http://example.com"));
  AccountInfo account_info = signin::MakePrimaryAccountAvailable(
      GetIdentityManager(), "foo@example.com", signin::ConsentLevel::kSync);

  // Add an account and then put its refresh token into an error state to
  // require a reauth before enabling sync.
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      GetIdentityManager(), account_info.account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));

  signin_ui_util::ShowReauthForPrimaryAccountWithAuthError(
      profile(), signin_metrics::AccessPoint::kAvatarBubbleSignIn);

  // Verify that the active tab has the correct DICE sign-in URL.
  TabStripModel* tab_strip = browser()->tab_strip_model();
  content::WebContents* active_contents = tab_strip->GetActiveWebContents();
  ASSERT_TRUE(active_contents);
  EXPECT_THAT(
      active_contents->GetVisibleURL().spec(),
      testing::StartsWith(GaiaUrls::GetInstance()->add_account_url().spec()));
}

TEST_F(SigninUiUtilTest, ShowExtensionSigninPrompt) {
  const GURL add_account_url = GaiaUrls::GetInstance()->add_account_url();
  const GURL sync_url = GaiaUrls::GetInstance()->signin_chrome_sync_dice();

  Profile* profile = browser()->profile();
  TabStripModel* tab_strip = browser()->tab_strip_model();
  ShowExtensionSigninPrompt(profile, /*enable_sync=*/true,
                            /*email_hint=*/std::string());
  EXPECT_EQ(1, tab_strip->count());
  // Calling the function again reuses the tab.
  ShowExtensionSigninPrompt(profile, /*enable_sync=*/true,
                            /*email_hint=*/std::string());
  EXPECT_EQ(1, tab_strip->count());

  content::WebContents* tab = tab_strip->GetWebContentsAt(0);
  ASSERT_TRUE(tab);
  EXPECT_TRUE(base::StartsWith(tab->GetVisibleURL().spec(), sync_url.spec(),
                               base::CompareCase::INSENSITIVE_ASCII));

  // Changing the parameter opens a new tab.
  ShowExtensionSigninPrompt(profile, /*enable_sync=*/false,
                            /*email_hint=*/std::string());
  EXPECT_EQ(2, tab_strip->count());
  // Calling the function again reuses the tab.
  ShowExtensionSigninPrompt(profile, /*enable_sync=*/false,
                            /*email_hint=*/std::string());
  EXPECT_EQ(2, tab_strip->count());
  tab = tab_strip->GetWebContentsAt(1);
  ASSERT_TRUE(tab);
  // With explicit signin, `sync_url` is used even though Sync is not going to
  // be enabled. This is because that web page displays additional text
  // explaining to the user that they are signing in to Chrome.
  EXPECT_TRUE(base::StartsWith(tab->GetVisibleURL().spec(), sync_url.spec(),
                               base::CompareCase::INSENSITIVE_ASCII));
  EXPECT_NE(tab->GetVisibleURL().query().find("flow=promo"), std::string::npos);
}

TEST_F(SigninUiUtilTest, ShowExtensionSigninPrompt_AsLockedProfile) {
  signin_util::ScopedForceSigninSetterForTesting force_signin_setter(true);
  Profile* profile = browser()->profile();
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());
  ASSERT_NE(entry, nullptr);
  entry->LockForceSigninProfile(true);
  TabStripModel* tab_strip = browser()->tab_strip_model();
  ShowExtensionSigninPrompt(profile, /*enable_sync=*/true,
                            /*email_hint=*/std::string());
  EXPECT_EQ(0, tab_strip->count());
  ShowExtensionSigninPrompt(profile, /*enable_sync=*/false,
                            /*email_hint=*/std::string());
  EXPECT_EQ(0, tab_strip->count());
}

TEST_F(SigninUiUtilTest, ShowSigninPromptFromPromo) {
  Profile* profile = browser()->profile();
  TabStripModel* tab_strip = browser()->tab_strip_model();
  ShowSigninPromptFromPromo(profile, access_point_);
  EXPECT_EQ(1, tab_strip->count());

  content::WebContents* tab = tab_strip->GetWebContentsAt(0);
  ASSERT_TRUE(tab);
  EXPECT_TRUE(
      base::StartsWith(tab->GetVisibleURL().spec(),
                       GaiaUrls::GetInstance()->add_account_url().spec(),
                       base::CompareCase::INSENSITIVE_ASCII));
}

TEST_F(SigninUiUtilTest, ShowSigninPromptFromPromoWithExistingAccount) {
  signin::MakePrimaryAccountAvailable(GetIdentityManager(), "foo@example.com",
                                      signin::ConsentLevel::kSignin);

  Profile* profile = browser()->profile();
  TabStripModel* tab_strip = browser()->tab_strip_model();
  EXPECT_EQ(0, tab_strip->count());
  ShowSigninPromptFromPromo(profile, access_point_);
  EXPECT_EQ(0, tab_strip->count());
}

TEST_F(SigninUiUtilTest, GetSignInTabWithAccessPoint) {
  signin::MakePrimaryAccountAvailable(GetIdentityManager(), "foo@example.com",
                                      signin::ConsentLevel::kSignin);

  Profile* profile = browser()->profile();
  TabStripModel* tab_strip = browser()->tab_strip_model();
  EXPECT_EQ(0, tab_strip->count());

  // Add tabs.
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

TEST_F(SigninUiUtilTest, EnableSyncWithExistingWebOnlyAccount) {
  CoreAccountId account_id =
      GetIdentityManager()->GetAccountsMutator()->AddOrUpdateAccount(
          kMainGaiaID, kMainEmail, "refresh_token", false,
          signin_metrics::AccessPoint::kUnknown,
          signin_metrics::SourceForRefreshTokenOperation::kUnknown);

  for (bool is_default_promo_account : {true, false}) {
    base::HistogramTester histogram_tester;
    base::UserActionTester user_action_tester;

    ExpectNoSigninStartedHistograms(histogram_tester);
    EXPECT_EQ(0, user_action_tester.GetActionCount(
                     "Signin_Signin_FromBookmarkBubble"));

    signin_metrics::PromoAction expected_promo_action =
        is_default_promo_account
            ? signin_metrics::PromoAction::PROMO_ACTION_WITH_DEFAULT
            : signin_metrics::PromoAction::PROMO_ACTION_NOT_DEFAULT;
    ExpectTurnSyncOn(
        signin_metrics::AccessPoint::kBookmarkBubble, expected_promo_action,
        account_id,
        TurnSyncOnHelper::SigninAbortedMode::KEEP_ACCOUNT_ON_WEB_ONLY,
        /*is_sync_promo=*/false, /*turn_sync_on_signed_profile=*/false);
    EnableSync(
        GetIdentityManager()->FindExtendedAccountInfoByAccountId(account_id),
        is_default_promo_account);

    ExpectOneSigninStartedHistograms(histogram_tester, expected_promo_action);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Signin_Signin_FromBookmarkBubble"));
  }
}

TEST_F(SigninUiUtilTest,
       EnableSyncPromoWithExistingWebOnlyAccountAvatarBubble) {
  access_point_ = signin_metrics::AccessPoint::kAvatarBubbleSignInWithSyncPromo;

  TestEnableSyncPromoWithExistingWebOnlyAccount();
}

// Checks that sync is treated as a promo for kSettings.
TEST_F(SigninUiUtilTest, EnableSyncPromoWithExistingWebOnlyAccountSettings) {
  access_point_ = signin_metrics::AccessPoint::kSettings;

  TestEnableSyncPromoWithExistingWebOnlyAccount();
}

TEST_F(SigninUiUtilTest, SignInWithExistingWebOnlyAccount) {
  CoreAccountId account_id =
      GetIdentityManager()->GetAccountsMutator()->AddOrUpdateAccount(
          kMainGaiaID, kMainEmail, "refresh_token", false,
          signin_metrics::AccessPoint::kUnknown,
          signin_metrics::SourceForRefreshTokenOperation::kUnknown);

  // Verify that the primary account is not set before.
  EXPECT_FALSE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  SignIn(GetIdentityManager()->FindExtendedAccountInfoByAccountId(account_id));

  // Verify that the primary account has been set.
  EXPECT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
}

TEST_F(SigninUiUtilTest, ShowExtensionSigninPromptReauth) {
  CoreAccountId account_id =
      GetIdentityManager()->GetAccountsMutator()->AddOrUpdateAccount(
          kMainGaiaID, kMainEmail, "refresh_token", false,
          signin_metrics::AccessPoint::kUnknown,
          signin_metrics::SourceForRefreshTokenOperation::kUnknown);
  GetIdentityManager()->GetPrimaryAccountMutator()->SetPrimaryAccount(
      account_id, signin::ConsentLevel::kSignin,
      signin_metrics::AccessPoint::kUnknown);
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      GetIdentityManager(), account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));

  Profile* profile = browser()->profile();
  TabStripModel* tab_strip = browser()->tab_strip_model();
  ShowExtensionSigninPrompt(profile, /*enable_sync=*/false, kMainEmail);
  EXPECT_EQ(1, tab_strip->count());

  content::WebContents* tab = tab_strip->GetWebContentsAt(0);
  ASSERT_TRUE(tab);
  EXPECT_TRUE(
      base::StartsWith(tab->GetVisibleURL().spec(),
                       GaiaUrls::GetInstance()->add_account_url().spec(),
                       base::CompareCase::INSENSITIVE_ASCII));
}

#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

TEST_F(SigninUiUtilTest,
       ShouldShowAnimatedIdentityOnOpeningWindowIfMultipleWindowsAtStartup) {
  EXPECT_TRUE(ShouldShowAnimatedIdentityOnOpeningWindow(*profile()));
  // Record that the identity was shown.
  RecordAnimatedIdentityTriggered(profile());
  // The identity can be shown again immediately (which is what happens if there
  // is multiple windows at startup).
  EXPECT_TRUE(ShouldShowAnimatedIdentityOnOpeningWindow(*profile()));
}

// This test does not use the SigninUiUtilTest test fixture, because it
// needs a mock time environment, and BrowserWithTestWindowTest may be flaky
// when used with mock time (see https://crbug.com/1014790).
TEST(ShouldShowAnimatedIdentityOnOpeningWindow, ReturnsFalseForNewWindow) {
  // Setup a testing profile manager with mock time.
  content::BrowserTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  ScopedTestingLocalState local_state(TestingBrowserProcess::GetGlobal());
  TestingProfileManager profile_manager(TestingBrowserProcess::GetGlobal(),
                                        &local_state);
  ASSERT_TRUE(profile_manager.SetUp());
  std::string name("testing_profile");
  TestingProfile* profile = profile_manager.CreateTestingProfile(
      name, std::unique_ptr<sync_preferences::PrefServiceSyncable>(),
      base::UTF8ToUTF16(name), 0,
      IdentityTestEnvironmentProfileAdaptor::
          GetIdentityTestEnvironmentFactories());

  EXPECT_TRUE(ShouldShowAnimatedIdentityOnOpeningWindow(*profile));

  // Animation is shown once.
  RecordAnimatedIdentityTriggered(profile);

  // Wait a few seconds.
  task_environment.FastForwardBy(base::Seconds(6));

  // Animation is not shown again in a new window.
  EXPECT_FALSE(ShouldShowAnimatedIdentityOnOpeningWindow(*profile));
}

}  // namespace signin_ui_util
