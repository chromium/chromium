// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_ui_util.h"

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
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
#include "google_apis/gaia/gaia_urls.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/signin/signin_ui_delegate_impl_dice.h"
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
namespace signin_ui_util {

namespace {
const char kMainEmail[] = "main_email@example.com";
const char kMainGaiaID[] = "main_gaia_id";
const char kSecondaryEmail[] = "secondary_email@example.com";
const char kSecondaryGaiaID[] = "secondary_gaia_id";
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

#if BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)

namespace {

#if BUILDFLAG(IS_CHROMEOS_LACROS)
class MockSigninUiDelegate : public SigninUiDelegate {
 public:
  MOCK_METHOD(void,
              ShowSigninUI,
              (Profile * profile,
               bool enable_sync,
               signin_metrics::AccessPoint access_point,
               signin_metrics::PromoAction promo_action),
              ());
  MOCK_METHOD(void,
              ShowReauthUI,
              (Profile * profile,
               const std::string& email,
               bool enable_sync,
               signin_metrics::AccessPoint access_point,
               signin_metrics::PromoAction promo_action),
              ());
  MOCK_METHOD(void,
              ShowTurnSyncOnUI,
              (Profile * profile,
               signin_metrics::AccessPoint access_point,
               signin_metrics::PromoAction promo_action,
               const CoreAccountId& account_id,
               TurnSyncOnHelper::SigninAbortedMode signin_aborted_mode,
               bool is_sync_promo),
              ());
};
#elif BUILDFLAG(ENABLE_DICE_SUPPORT)
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
               bool is_sync_promo),
              ());
};
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace

// TODO(crbug.com/40834209): merge SigninUiUtilTest with
// MirrorSigninUiUtilTest.
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
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
                        bool is_sync_promo) {
    EXPECT_CALL(
        mock_delegate_,
        ShowTurnSyncOnUI(profile(), access_point, promo_action, account_id,
                         signin_aborted_mode, is_sync_promo));
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

  signin_metrics::AccessPoint access_point_ =
      signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_BUBBLE;

  testing::StrictMock<MockSigninUiDelegate> mock_delegate_;
  base::AutoReset<SigninUiDelegate*> delegate_auto_reset_;
};

TEST_F(SigninUiUtilTest, EnableSyncWithExistingAccount) {
  CoreAccountId account_id =
      GetIdentityManager()->GetAccountsMutator()->AddOrUpdateAccount(
          kMainGaiaID, kMainEmail, "refresh_token", false,
          signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN,
          signin_metrics::SourceForRefreshTokenOperation::kUnknown);
  GetIdentityManager()->GetPrimaryAccountMutator()->SetPrimaryAccount(
      account_id, signin::ConsentLevel::kSignin,
      signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);

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
    ExpectTurnSyncOn(signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_BUBBLE,
                     expected_promo_action, account_id,
                     TurnSyncOnHelper::SigninAbortedMode::KEEP_ACCOUNT,
                     /*is_sync_promo=*/false);
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
          signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN,
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
      signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN,
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
          signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN,
          signin_metrics::SourceForRefreshTokenOperation::kUnknown);
  GetIdentityManager()->GetPrimaryAccountMutator()->SetPrimaryAccount(
      account_id, signin::ConsentLevel::kSignin,
      signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);

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
          signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN,
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

  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kTestEmail4),
            accounts[0].account_id.ToString());
  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kTestEmail3),
            accounts[1].account_id.ToString());
  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kTestEmail2),
            accounts[2].account_id.ToString());
  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kTestEmail1),
            accounts[3].account_id.ToString());

  // Set a primary account.
  identity_test_env.SetPrimaryAccount(kTestEmail3,
                                      signin::ConsentLevel::kSignin);
  accounts = GetOrderedAccountsForDisplay(identity_manager, false);

  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kTestEmail3),
            accounts[0].account_id.ToString());
  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kTestEmail4),
            accounts[1].account_id.ToString());
  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kTestEmail2),
            accounts[2].account_id.ToString());
  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kTestEmail1),
            accounts[3].account_id.ToString());

  // Set a different primary account.
  identity_test_env.SetPrimaryAccount(kTestEmail1,
                                      signin::ConsentLevel::kSignin);
  accounts = GetOrderedAccountsForDisplay(identity_manager, false);

  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kTestEmail1),
            accounts[0].account_id.ToString());
  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kTestEmail4),
            accounts[1].account_id.ToString());
  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kTestEmail3),
            accounts[2].account_id.ToString());
  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kTestEmail2),
            accounts[3].account_id.ToString());

  // Primary account should still be included if not in cookies, other accounts
  // should not.
  identity_test_env.SetCookieAccounts(
      {{kTestEmail4, signin::GetTestGaiaIdForEmail(kTestEmail4)},
       {kTestEmail2, signin::GetTestGaiaIdForEmail(kTestEmail2)}});
  accounts = GetOrderedAccountsForDisplay(identity_manager, false);

  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kTestEmail1),
            accounts[0].account_id.ToString());
  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kTestEmail4),
            accounts[1].account_id.ToString());
  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kTestEmail2),
            accounts[2].account_id.ToString());
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
  access_point_ = signin_metrics::AccessPoint::ACCESS_POINT_EXTENSIONS;
  EnableSync(CoreAccountInfo(), false);
  EXPECT_EQ(
      1, user_action_tester.GetActionCount("Signin_Signin_FromBookmarkBubble"));
  EXPECT_EQ(0, tab_strip->active_index());

  // Other access points re-use the tab and take focus.
  access_point_ = signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS;
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
      profile(),
      signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN);

  // Verify that the active tab has the correct DICE sign-in URL.
  TabStripModel* tab_strip = browser()->tab_strip_model();
  content::WebContents* active_contents = tab_strip->GetActiveWebContents();
  ASSERT_TRUE(active_contents);
  EXPECT_THAT(
      active_contents->GetVisibleURL().spec(),
      testing::StartsWith(GaiaUrls::GetInstance()->add_account_url().spec()));
}

TEST_F(SigninUiUtilTest,
       ShouldShowAnimatedIdentityOnOpeningWindow_ReturnsTrueForMultiProfiles) {
  const char kSecondProfile[] = "SecondProfile";
  const char16_t kSecondProfile16[] = u"SecondProfile";
  const base::FilePath profile_path =
      profile_manager()->profiles_dir().AppendASCII(kSecondProfile);
  ProfileAttributesInitParams params;
  params.profile_path = profile_path;
  params.profile_name = kSecondProfile16;
  profile_manager()->profile_attributes_storage()->AddProfile(
      std::move(params));

  EXPECT_TRUE(ShouldShowAnimatedIdentityOnOpeningWindow(
      *profile_manager()->profile_attributes_storage(), profile()));
}

TEST_F(SigninUiUtilTest,
       ShouldShowAnimatedIdentityOnOpeningWindow_ReturnsTrueForMultiSignin) {
  GetIdentityManager()->GetAccountsMutator()->AddOrUpdateAccount(
      kMainGaiaID, kMainEmail, "refresh_token", false,
      signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN,
      signin_metrics::SourceForRefreshTokenOperation::kUnknown);
  GetIdentityManager()->GetAccountsMutator()->AddOrUpdateAccount(
      kSecondaryGaiaID, kSecondaryEmail, "refresh_token", false,
      signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN,
      signin_metrics::SourceForRefreshTokenOperation::kUnknown);

  EXPECT_TRUE(ShouldShowAnimatedIdentityOnOpeningWindow(
      *profile_manager()->profile_attributes_storage(), profile()));

  // The identity can be shown again immediately (which is what happens if there
  // is multiple windows at startup).
  RecordAnimatedIdentityTriggered(profile());
  EXPECT_TRUE(ShouldShowAnimatedIdentityOnOpeningWindow(
      *profile_manager()->profile_attributes_storage(), profile()));
}

TEST_F(
    SigninUiUtilTest,
    ShouldShowAnimatedIdentityOnOpeningWindow_ReturnsFalseForSingleProfileSingleSignin) {
  GetIdentityManager()->GetAccountsMutator()->AddOrUpdateAccount(
      kMainGaiaID, kMainEmail, "refresh_token", false,
      signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN,
      signin_metrics::SourceForRefreshTokenOperation::kUnknown);

  EXPECT_FALSE(ShouldShowAnimatedIdentityOnOpeningWindow(
      *profile_manager()->profile_attributes_storage(), profile()));
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
  EXPECT_TRUE(
      base::StartsWith(tab->GetVisibleURL().spec(),
                       switches::IsExplicitBrowserSigninUIOnDesktopEnabled()
                           ? sync_url.spec()
                           : add_account_url.spec(),
                       base::CompareCase::INSENSITIVE_ASCII));
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
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(ENABLE_DICE_SUPPORT)

TEST_F(SigninUiUtilTest, GetSignInTabWithAccessPoint) {
  signin::MakePrimaryAccountAvailable(GetIdentityManager(), "foo@example.com",
                                      signin::ConsentLevel::kSignin);

  Profile* profile = browser()->profile();
  TabStripModel* tab_strip = browser()->tab_strip_model();
  EXPECT_EQ(0, tab_strip->count());

  // Add tabs.
  ShowReauthForAccount(profile, "test1@gmail.com",
                       signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS);
  ShowReauthForAccount(
      profile, "test2@gmail.com",
      signin_metrics::AccessPoint::ACCESS_POINT_CHROME_SIGNIN_INTERCEPT_BUBBLE);
  ShowReauthForAccount(
      profile, "test3@gmail.com",
      signin_metrics::AccessPoint::ACCESS_POINT_PASSWORD_BUBBLE);
  EXPECT_EQ(3, tab_strip->count());

  // Look for existing tab.
  content::WebContents* sign_in_tab = GetSignInTabWithAccessPoint(
      browser(),
      signin_metrics::AccessPoint::ACCESS_POINT_CHROME_SIGNIN_INTERCEPT_BUBBLE);
  EXPECT_EQ(signin::GetAddAccountURLForDice(
                "test2@gmail.com", GURL(google_util::kGoogleHomepageURL)),
            sign_in_tab->GetVisibleURL());

  // Look for non existing tab.
  sign_in_tab = GetSignInTabWithAccessPoint(
      browser(), signin_metrics::AccessPoint::ACCESS_POINT_FORCED_SIGNIN);
  EXPECT_EQ(nullptr, sign_in_tab);

  // Two tabs with the same access point, will return the first tab found.
  ShowReauthForAccount(profile, "test4@gmail.com",
                       signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS);
  EXPECT_EQ(4, tab_strip->count());

  sign_in_tab = GetSignInTabWithAccessPoint(
      browser(), signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS);
  EXPECT_EQ(signin::GetAddAccountURLForDice(
                "test1@gmail.com", GURL(google_util::kGoogleHomepageURL)),
            sign_in_tab->GetVisibleURL());
}

#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
class SigninUiUtilWithUnoDesktopTest : public SigninUiUtilTest {
 private:
  base::test::ScopedFeatureList feature_list_{
      switches::kExplicitBrowserSigninUIOnDesktop};
};

TEST_F(SigninUiUtilWithUnoDesktopTest, EnableSyncWithExistingWebOnlyAccount) {
  CoreAccountId account_id =
      GetIdentityManager()->GetAccountsMutator()->AddOrUpdateAccount(
          kMainGaiaID, kMainEmail, "refresh_token", false,
          signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN,
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
        signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_BUBBLE,
        expected_promo_action, account_id,
        TurnSyncOnHelper::SigninAbortedMode::KEEP_ACCOUNT_ON_WEB_ONLY,
        /*is_sync_promo=*/false);
    EnableSync(
        GetIdentityManager()->FindExtendedAccountInfoByAccountId(account_id),
        is_default_promo_account);

    ExpectOneSigninStartedHistograms(histogram_tester, expected_promo_action);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Signin_Signin_FromBookmarkBubble"));
  }
}

TEST_F(SigninUiUtilWithUnoDesktopTest, SignInWithExistingWebOnlyAccount) {
  CoreAccountId account_id =
      GetIdentityManager()->GetAccountsMutator()->AddOrUpdateAccount(
          kMainGaiaID, kMainEmail, "refresh_token", false,
          signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN,
          signin_metrics::SourceForRefreshTokenOperation::kUnknown);

  // Verify that the primary account is not set before.
  EXPECT_FALSE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  SignIn(GetIdentityManager()->FindExtendedAccountInfoByAccountId(account_id));

  // Verify that the primary account has been set.
  EXPECT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
}

TEST_F(SigninUiUtilWithUnoDesktopTest, ShowExtensionSigninPrompt) {
  Profile* profile = browser()->profile();
  TabStripModel* tab_strip = browser()->tab_strip_model();
  ShowExtensionSigninPrompt(profile, /*enable_sync=*/false,
                            /*email_hint=*/std::string());
  EXPECT_EQ(1, tab_strip->count());
  // Calling the function again reuses the tab.
  ShowExtensionSigninPrompt(profile, /*enable_sync=*/false,
                            /*email_hint=*/std::string());
  EXPECT_EQ(1, tab_strip->count());

  content::WebContents* tab = tab_strip->GetWebContentsAt(0);
  ASSERT_TRUE(tab);
  EXPECT_TRUE(base::StartsWith(
      tab->GetVisibleURL().spec(),
      GaiaUrls::GetInstance()->signin_chrome_sync_dice().spec(),
      base::CompareCase::INSENSITIVE_ASCII));
  EXPECT_NE(tab->GetVisibleURL().query().find("flow=promo"), std::string::npos);
}

TEST_F(SigninUiUtilWithUnoDesktopTest, ShowExtensionSigninPromptReauth) {
  CoreAccountId account_id =
      GetIdentityManager()->GetAccountsMutator()->AddOrUpdateAccount(
          kMainGaiaID, kMainEmail, "refresh_token", false,
          signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN,
          signin_metrics::SourceForRefreshTokenOperation::kUnknown);
  GetIdentityManager()->GetPrimaryAccountMutator()->SetPrimaryAccount(
      account_id, signin::ConsentLevel::kSignin,
      signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
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
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)

#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
class MirrorSigninUiUtilTest : public BrowserWithTestWindowTest {
 public:
  MirrorSigninUiUtilTest()
      : delegate_auto_reset_(SetSigninUiDelegateForTesting(&mock_delegate_)) {}
  ~MirrorSigninUiUtilTest() override = default;

  // BrowserWithTestWindowTest:
  TestingProfile::TestingFactories GetTestingFactories() override {
    return IdentityTestEnvironmentProfileAdaptor::
        GetIdentityTestEnvironmentFactories();
  }

  void ExpectReauth(const std::string& email,
                    bool enable_sync,
                    signin_metrics::AccessPoint access_point,
                    signin_metrics::PromoAction promo_action) {
    EXPECT_CALL(mock_delegate_, ShowReauthUI(profile(), email, enable_sync,
                                             access_point, promo_action));
  }

  void ExpectAddAccount(bool enable_sync,
                        signin_metrics::AccessPoint access_point,
                        signin_metrics::PromoAction promo_action) {
    EXPECT_CALL(mock_delegate_, ShowSigninUI(profile(), enable_sync,
                                             access_point, promo_action));
  }

  void ExpectTurnSyncOn(
      signin_metrics::AccessPoint access_point,
      signin_metrics::PromoAction promo_action,
      const CoreAccountId& account_id,
      TurnSyncOnHelper::SigninAbortedMode signin_aborted_mode) {
    EXPECT_CALL(
        mock_delegate_,
        ShowTurnSyncOnUI(profile(), access_point, promo_action, account_id,
                         signin_aborted_mode, /*is_sync_promo=*/false));
  }

 protected:
  Profile* profile() { return browser()->profile(); }

 private:
  testing::StrictMock<MockSigninUiDelegate> mock_delegate_;
  base::AutoReset<SigninUiDelegate*> delegate_auto_reset_;
};

TEST_F(MirrorSigninUiUtilTest, EnableSyncWithExistingAccount) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile());
  AccountInfo account_info = signin::MakePrimaryAccountAvailable(
      identity_manager, kMainEmail, signin::ConsentLevel::kSignin);

  for (bool is_default_promo_account : {true, false}) {
    signin_metrics::PromoAction expected_promo_action =
        is_default_promo_account
            ? signin_metrics::PromoAction::PROMO_ACTION_WITH_DEFAULT
            : signin_metrics::PromoAction::PROMO_ACTION_NOT_DEFAULT;

    ExpectTurnSyncOn(
        signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN,
        expected_promo_action, account_info.account_id,
        TurnSyncOnHelper::SigninAbortedMode::KEEP_ACCOUNT);
    EnableSyncFromMultiAccountPromo(
        profile(), account_info,
        signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN,
        is_default_promo_account);
  }
}

TEST_F(MirrorSigninUiUtilTest, EnableSyncWithAccountThatNeedsReauth) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile());
  AccountInfo account_info = signin::MakePrimaryAccountAvailable(
      identity_manager, kMainEmail, signin::ConsentLevel::kSignin);

  // Add an account and then put its refresh token into an error state to
  // require a reauth before enabling sync.
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_manager, account_info.account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));

  ExpectReauth(kMainEmail, /*enable_sync=*/true,
               signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN,
               signin_metrics::PromoAction::PROMO_ACTION_WITH_DEFAULT);
  EnableSyncFromSingleAccountPromo(
      profile(), account_info,
      signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN);
}

TEST_F(MirrorSigninUiUtilTest, EnableSyncForNewAccount) {
  ExpectAddAccount(
      /*enable_sync=*/true,
      signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN,
      signin_metrics::PromoAction::
          PROMO_ACTION_NEW_ACCOUNT_NO_EXISTING_ACCOUNT);
  EnableSyncFromMultiAccountPromo(
      profile(), CoreAccountInfo(),
      signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN,
      /*is_default_promo_account=*/false);
}

TEST_F(MirrorSigninUiUtilTest, EnableSyncForNewAccountExisting) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile());
  AccountInfo account_info = signin::MakePrimaryAccountAvailable(
      identity_manager, kMainEmail, signin::ConsentLevel::kSignin);

  ExpectAddAccount(
      /*enable_sync=*/true,
      signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN,
      signin_metrics::PromoAction::PROMO_ACTION_NEW_ACCOUNT_EXISTING_ACCOUNT);
  EnableSyncFromMultiAccountPromo(
      profile(), CoreAccountInfo(),
      signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN,
      /*is_default_promo_account=*/false);
}

TEST_F(MirrorSigninUiUtilTest, ShowReauthDialog) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile());
  AccountInfo account_info = signin::MakePrimaryAccountAvailable(
      identity_manager, kMainEmail, signin::ConsentLevel::kSync);

  // Add an account and then put its refresh token into an error state to
  // require a reauth before enabling sync.
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_manager, account_info.account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));

  ExpectReauth(kMainEmail, /*enable_sync=*/false,
               signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN,
               signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO);
  ShowReauthForPrimaryAccountWithAuthError(
      profile(),
      signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN);
}

TEST_F(MirrorSigninUiUtilTest, ShowExtensionSigninPrompt_Signin) {
  for (bool enable_sync : {true, false}) {
    ExpectAddAccount(enable_sync,
                     signin_metrics::AccessPoint::ACCESS_POINT_EXTENSIONS,
                     signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO);
    ShowExtensionSigninPrompt(profile(), enable_sync,
                              /*email_hint=*/std::string());
  }
}

TEST_F(MirrorSigninUiUtilTest, ShowExtensionSigninPrompt_Reauth) {
  for (bool enable_sync : {true, false}) {
    ExpectReauth(kMainEmail, enable_sync,
                 signin_metrics::AccessPoint::ACCESS_POINT_EXTENSIONS,
                 signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO);
    ShowExtensionSigninPrompt(profile(), enable_sync, kMainEmail);
  }
}

TEST_F(MirrorSigninUiUtilTest,
       ShowExtensionSigninPrompt_Reauth_AsLockedProfile) {
  signin_util::ScopedForceSigninSetterForTesting force_signin_setter(true);
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile()->GetPath());
  ASSERT_NE(entry, nullptr);
  entry->LockForceSigninProfile(true);

  ShowExtensionSigninPrompt(profile(), /*enable_sync=*/true, kMainEmail);
}

TEST_F(MirrorSigninUiUtilTest, ShowSigninPromptFromPromo) {
  signin_metrics::AccessPoint kAccessPoint =
      signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN;
  ExpectAddAccount(
      /*enable_sync=*/false, kAccessPoint,
      signin_metrics::PromoAction::
          PROMO_ACTION_NEW_ACCOUNT_NO_EXISTING_ACCOUNT);
  ShowSigninPromptFromPromo(profile(), kAccessPoint);
}

TEST_F(MirrorSigninUiUtilTest, ShowSigninPromptFromPromoWithExistingAccount) {
  signin::MakePrimaryAccountAvailable(
      IdentityManagerFactory::GetForProfile(profile()), "foo@example.com",
      signin::ConsentLevel::kSignin);
  ShowSigninPromptFromPromo(
      profile(),
      signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN);
}

#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

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

  // Setup accounts.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  identity_manager->GetAccountsMutator()->AddOrUpdateAccount(
      kMainGaiaID, kMainEmail, "refresh_token", false,
      signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN,
      signin_metrics::SourceForRefreshTokenOperation::kUnknown);
  identity_manager->GetAccountsMutator()->AddOrUpdateAccount(
      kSecondaryGaiaID, kSecondaryEmail, "refresh_token", false,
      signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN,
      signin_metrics::SourceForRefreshTokenOperation::kUnknown);
  EXPECT_TRUE(ShouldShowAnimatedIdentityOnOpeningWindow(
      *profile_manager.profile_attributes_storage(), profile));

  // Animation is shown once.
  RecordAnimatedIdentityTriggered(profile);

  // Wait a few seconds.
  task_environment.FastForwardBy(base::Seconds(6));

  // Animation is not shown again in a new window.
  EXPECT_FALSE(ShouldShowAnimatedIdentityOnOpeningWindow(
      *profile_manager.profile_attributes_storage(), profile));
}

}  // namespace signin_ui_util
