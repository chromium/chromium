// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_ui_util.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "build/buildflag.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/fake_gaia_cookie_manager_service_builder.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service_builder.h"
#include "chrome/browser/signin/fake_signin_manager_builder.h"
#include "chrome/browser/signin/gaia_cookie_manager_service_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/scoped_account_consistency.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/signin/core/browser/profile_management_switches.h"
#include "components/signin/core/browser/signin_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace signin_ui_util {

class GetAllowedDomainTest : public ::testing::Test {};

TEST_F(GetAllowedDomainTest, WithInvalidPattern) {
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

TEST_F(GetAllowedDomainTest, WithValidPattern) {
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
const char kMainEmail[] = "main_email@example.com";
const char kMainGaiaID[] = "main_gaia_id";
class SigninUiUtilTestBrowserWindow : public TestBrowserWindow {
 public:
  SigninUiUtilTestBrowserWindow() = default;
  ~SigninUiUtilTestBrowserWindow() override = default;
  void set_browser(Browser* browser) { browser_ = browser; }

  void ShowAvatarBubbleFromAvatarButton(
      AvatarBubbleMode mode,
      const signin::ManageAccountsParams& manage_accounts_params,
      signin_metrics::AccessPoint access_point,
      bool is_source_keyboard) override {
    ASSERT_TRUE(browser_);
    // Simulate what |BrowserView| does for a regular Chrome sign-in flow.
    browser_->signin_view_controller()->ShowSignin(
        profiles::BubbleViewMode::BUBBLE_VIEW_MODE_GAIA_SIGNIN, browser_,
        access_point);
  }

 private:
  Browser* browser_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(SigninUiUtilTestBrowserWindow);
};
}  // namespace

class DiceSigninUiUtilTest : public BrowserWithTestWindowTest {
 public:
  DiceSigninUiUtilTest()
      : BrowserWithTestWindowTest(
            content::TestBrowserThreadBundle::IO_MAINLOOP) {}
  ~DiceSigninUiUtilTest() override = default;

  struct CreateDiceTurnSyncOnHelperParams {
   public:
    Profile* profile = nullptr;
    Browser* browser = nullptr;
    signin_metrics::AccessPoint signin_access_point =
        signin_metrics::AccessPoint::ACCESS_POINT_MAX;
    signin_metrics::PromoAction signin_promo_action =
        signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO;
    signin_metrics::Reason signin_reason = signin_metrics::Reason::REASON_MAX;
    std::string account_id;
    DiceTurnSyncOnHelper::SigninAbortedMode signin_aborted_mode =
        DiceTurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT;
  };

  void CreateDiceTurnSyncOnHelper(
      Profile* profile,
      Browser* browser,
      signin_metrics::AccessPoint signin_access_point,
      signin_metrics::PromoAction signin_promo_action,
      signin_metrics::Reason signin_reason,
      const std::string& account_id,
      DiceTurnSyncOnHelper::SigninAbortedMode signin_aborted_mode) {
    create_dice_turn_sync_on_helper_called_ = true;
    create_dice_turn_sync_on_helper_params_.profile = profile;
    create_dice_turn_sync_on_helper_params_.browser = browser;
    create_dice_turn_sync_on_helper_params_.signin_access_point =
        signin_access_point;
    create_dice_turn_sync_on_helper_params_.signin_promo_action =
        signin_promo_action;
    create_dice_turn_sync_on_helper_params_.signin_reason = signin_reason;
    create_dice_turn_sync_on_helper_params_.account_id = account_id;
    create_dice_turn_sync_on_helper_params_.signin_aborted_mode =
        signin_aborted_mode;
  }

 protected:
  // BrowserWithTestWindowTest:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    static_cast<SigninUiUtilTestBrowserWindow*>(browser()->window())
        ->set_browser(browser());
  }

  // BrowserWithTestWindowTest:
  TestingProfile::TestingFactories GetTestingFactories() override {
    return {{SigninManagerFactory::GetInstance(),
             base::BindRepeating(&BuildFakeSigninManagerForTesting)},
            {ProfileOAuth2TokenServiceFactory::GetInstance(),
             base::BindRepeating(&BuildFakeProfileOAuth2TokenService)},
            {GaiaCookieManagerServiceFactory::GetInstance(),
             base::BindRepeating(
                 &BuildFakeGaiaCookieManagerServiceNoFakeUrlFetcher)}};
  }

  // BrowserWithTestWindowTest:
  BrowserWindow* CreateBrowserWindow() override {
    return new SigninUiUtilTestBrowserWindow();
  }

  // Returns the token service.
  ProfileOAuth2TokenService* GetTokenService() {
    return ProfileOAuth2TokenServiceFactory::GetForProfile(profile());
  }

  // Returns the account tracker service.
  AccountTrackerService* GetAccountTrackerService() {
    return AccountTrackerServiceFactory::GetForProfile(profile());
  }

  void EnableSync(const AccountInfo& account_info,
                  bool is_default_promo_account) {
    signin_ui_util::internal::EnableSyncFromPromo(
        browser(), account_info, access_point_, is_default_promo_account,
        base::BindOnce(&DiceSigninUiUtilTest::CreateDiceTurnSyncOnHelper,
                       base::Unretained(this)));
  }

  void ExpectNoSigninStartedHistograms(
      const base::HistogramTester& histogram_tester) {
    histogram_tester.ExpectTotalCount("Signin.SigninStartedAccessPoint", 0);
    histogram_tester.ExpectTotalCount(
        "Signin.SigninStartedAccessPoint.WithDefault", 0);
    histogram_tester.ExpectTotalCount(
        "Signin.SigninStartedAccessPoint.NotDefault", 0);
    histogram_tester.ExpectTotalCount(
        "Signin.SigninStartedAccessPoint.NewAccountPreDice", 0);
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
            "Signin.SigninStartedAccessPoint.NewAccountPreDice", 0);
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
            "Signin.SigninStartedAccessPoint.NewAccountPreDice", 0);
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
            "Signin.SigninStartedAccessPoint.NewAccountPreDice", 0);
        histogram_tester.ExpectTotalCount(
            "Signin.SigninStartedAccessPoint.NewAccountNoExistingAccount", 0);
        histogram_tester.ExpectTotalCount(
            "Signin.SigninStartedAccessPoint.NewAccountExistingAccount", 0);
        break;
      case signin_metrics::PromoAction::PROMO_ACTION_NEW_ACCOUNT_PRE_DICE:
        histogram_tester.ExpectTotalCount(
            "Signin.SigninStartedAccessPoint.WithDefault", 0);
        histogram_tester.ExpectTotalCount(
            "Signin.SigninStartedAccessPoint.NotDefault", 0);
        histogram_tester.ExpectUniqueSample(
            "Signin.SigninStartedAccessPoint.NewAccountPreDice", access_point_,
            1);
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
        histogram_tester.ExpectTotalCount(
            "Signin.SigninStartedAccessPoint.NewAccountPreDice", 0);
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
            "Signin.SigninStartedAccessPoint.NewAccountPreDice", 0);
        histogram_tester.ExpectTotalCount(
            "Signin.SigninStartedAccessPoint.NewAccountNoExistingAccount", 0);
        histogram_tester.ExpectUniqueSample(
            "Signin.SigninStartedAccessPoint.NewAccountExistingAccount",
            access_point_, 1);
        break;
    }
  }

  const ScopedAccountConsistencyDice scoped_account_consistency_;
  signin_metrics::AccessPoint access_point_ =
      signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_BUBBLE;

  bool create_dice_turn_sync_on_helper_called_ = false;
  CreateDiceTurnSyncOnHelperParams create_dice_turn_sync_on_helper_params_;
};

TEST_F(DiceSigninUiUtilTest, EnableSyncWithExistingAccount) {
  // Add an account.
  std::string account_id =
      GetAccountTrackerService()->SeedAccountInfo(kMainEmail, kMainGaiaID);
  GetTokenService()->UpdateCredentials(account_id, "token");

  for (bool is_default_promo_account : {true, false}) {
    base::HistogramTester histogram_tester;
    base::UserActionTester user_action_tester;

    ExpectNoSigninStartedHistograms(histogram_tester);
    EXPECT_EQ(0, user_action_tester.GetActionCount(
                     "Signin_Signin_FromBookmarkBubble"));

    EnableSync(GetAccountTrackerService()->GetAccountInfo(account_id),
               is_default_promo_account);
    signin_metrics::PromoAction expected_promo_action =
        is_default_promo_account
            ? signin_metrics::PromoAction::PROMO_ACTION_WITH_DEFAULT
            : signin_metrics::PromoAction::PROMO_ACTION_NOT_DEFAULT;
    ASSERT_TRUE(create_dice_turn_sync_on_helper_called_);
    ExpectOneSigninStartedHistograms(histogram_tester, expected_promo_action);

    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Signin_Signin_FromBookmarkBubble"));
    if (is_default_promo_account) {
      EXPECT_EQ(1, user_action_tester.GetActionCount(
                       "Signin_SigninWithDefault_FromBookmarkBubble"));
    } else {
      EXPECT_EQ(1, user_action_tester.GetActionCount(
                       "Signin_SigninNotDefault_FromBookmarkBubble"));
    }

    // Verify that the helper to enable sync is created with the expected
    // params.
    EXPECT_EQ(profile(), create_dice_turn_sync_on_helper_params_.profile);
    EXPECT_EQ(browser(), create_dice_turn_sync_on_helper_params_.browser);
    EXPECT_EQ(account_id, create_dice_turn_sync_on_helper_params_.account_id);
    EXPECT_EQ(signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_BUBBLE,
              create_dice_turn_sync_on_helper_params_.signin_access_point);
    EXPECT_EQ(expected_promo_action,
              create_dice_turn_sync_on_helper_params_.signin_promo_action);
    EXPECT_EQ(signin_metrics::Reason::REASON_SIGNIN_PRIMARY_ACCOUNT,
              create_dice_turn_sync_on_helper_params_.signin_reason);
    EXPECT_EQ(DiceTurnSyncOnHelper::SigninAbortedMode::KEEP_ACCOUNT,
              create_dice_turn_sync_on_helper_params_.signin_aborted_mode);
  }
}

TEST_F(DiceSigninUiUtilTest, EnableSyncWithAccountThatNeedsReauth) {
  AddTab(browser(), GURL("http://example.com"));
  // Add an account to the account tracker, but do not add it to the token
  // service in order for it to require a reauth before enabling sync.
  std::string account_id =
      GetAccountTrackerService()->SeedAccountInfo(kMainGaiaID, kMainEmail);

  for (bool is_default_promo_account : {true, false}) {
    base::HistogramTester histogram_tester;
    base::UserActionTester user_action_tester;

    ExpectNoSigninStartedHistograms(histogram_tester);
    EXPECT_EQ(0, user_action_tester.GetActionCount(
                     "Signin_Signin_FromBookmarkBubble"));

    EnableSync(GetAccountTrackerService()->GetAccountInfo(account_id),
               is_default_promo_account);
    ASSERT_FALSE(create_dice_turn_sync_on_helper_called_);

    ExpectOneSigninStartedHistograms(
        histogram_tester,
        is_default_promo_account
            ? signin_metrics::PromoAction::PROMO_ACTION_WITH_DEFAULT
            : signin_metrics::PromoAction::PROMO_ACTION_NOT_DEFAULT);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Signin_Signin_FromBookmarkBubble"));

    if (is_default_promo_account) {
      EXPECT_EQ(1, user_action_tester.GetActionCount(
                       "Signin_SigninWithDefault_FromBookmarkBubble"));
    } else {
      EXPECT_EQ(1, user_action_tester.GetActionCount(
                       "Signin_SigninNotDefault_FromBookmarkBubble"));
    }

    // Verify that the active tab has the correct DICE sign-in URL.
    TabStripModel* tab_strip = browser()->tab_strip_model();
    content::WebContents* active_contents = tab_strip->GetActiveWebContents();
    ASSERT_TRUE(active_contents);
    EXPECT_EQ(signin::GetSigninURLForDice(profile(), kMainEmail),
              active_contents->GetVisibleURL());
    tab_strip->CloseWebContentsAt(
        tab_strip->GetIndexOfWebContents(active_contents),
        TabStripModel::CLOSE_USER_GESTURE);
  }
}

TEST_F(DiceSigninUiUtilTest, EnableSyncForNewAccountWithNoTab) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  ExpectNoSigninStartedHistograms(histogram_tester);
  EXPECT_EQ(
      0, user_action_tester.GetActionCount("Signin_Signin_FromBookmarkBubble"));

  EnableSync(AccountInfo(), false /* is_default_promo_account (not used)*/);
  ASSERT_FALSE(create_dice_turn_sync_on_helper_called_);

  ExpectOneSigninStartedHistograms(
      histogram_tester, signin_metrics::PromoAction::
                            PROMO_ACTION_NEW_ACCOUNT_NO_EXISTING_ACCOUNT);
  EXPECT_EQ(
      1, user_action_tester.GetActionCount("Signin_Signin_FromBookmarkBubble"));
  EXPECT_EQ(1,
            user_action_tester.GetActionCount(
                "Signin_SigninNewAccountNoExistingAccount_FromBookmarkBubble"));

  // Verify that the active tab has the correct DICE sign-in URL.
  content::WebContents* active_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_contents);
  EXPECT_EQ(signin::GetSigninURLForDice(profile(), ""),
            active_contents->GetVisibleURL());
}

TEST_F(DiceSigninUiUtilTest, EnableSyncForNewAccountWithNoTabWithExisting) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  // Add an account.
  std::string account_id =
      GetAccountTrackerService()->SeedAccountInfo(kMainEmail, kMainGaiaID);
  GetTokenService()->UpdateCredentials(account_id, "token");

  ExpectNoSigninStartedHistograms(histogram_tester);
  EXPECT_EQ(
      0, user_action_tester.GetActionCount("Signin_Signin_FromBookmarkBubble"));

  EnableSync(AccountInfo(), false /* is_default_promo_account (not used)*/);
  ASSERT_FALSE(create_dice_turn_sync_on_helper_called_);

  ExpectOneSigninStartedHistograms(
      histogram_tester,
      signin_metrics::PromoAction::PROMO_ACTION_NEW_ACCOUNT_EXISTING_ACCOUNT);
  EXPECT_EQ(
      1, user_action_tester.GetActionCount("Signin_Signin_FromBookmarkBubble"));
  EXPECT_EQ(1,
            user_action_tester.GetActionCount(
                "Signin_SigninNewAccountExistingAccount_FromBookmarkBubble"));
}

TEST_F(DiceSigninUiUtilTest, EnableSyncForNewAccountWithOneTab) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;
  AddTab(browser(), GURL("http://foo/1"));

  ExpectNoSigninStartedHistograms(histogram_tester);
  EXPECT_EQ(
      0, user_action_tester.GetActionCount("Signin_Signin_FromBookmarkBubble"));

  EnableSync(AccountInfo(), false /* is_default_promo_account (not used)*/);
  ASSERT_FALSE(create_dice_turn_sync_on_helper_called_);

  ExpectOneSigninStartedHistograms(
      histogram_tester, signin_metrics::PromoAction::
                            PROMO_ACTION_NEW_ACCOUNT_NO_EXISTING_ACCOUNT);
  EXPECT_EQ(
      1, user_action_tester.GetActionCount("Signin_Signin_FromBookmarkBubble"));
  EXPECT_EQ(1,
            user_action_tester.GetActionCount(
                "Signin_SigninNewAccountNoExistingAccount_FromBookmarkBubble"));

  // Verify that the active tab has the correct DICE sign-in URL.
  content::WebContents* active_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_contents);
  EXPECT_EQ(signin::GetSigninURLForDice(profile(), ""),
            active_contents->GetVisibleURL());
}

TEST_F(DiceSigninUiUtilTest, GetAccountsForDicePromos) {
  // Should start off with no accounts.
  std::vector<AccountInfo> accounts = GetAccountsForDicePromos(profile());
  EXPECT_TRUE(accounts.empty());

  // TODO(tangltom): Flesh out this test.
}

TEST_F(DiceSigninUiUtilTest, MergeDiceSigninTab) {
  base::UserActionTester user_action_tester;
  EnableSync(AccountInfo(), false);
  EXPECT_EQ(
      1, user_action_tester.GetActionCount("Signin_Signin_FromBookmarkBubble"));

  // Signin tab is reused.
  EnableSync(AccountInfo(), false);
  EXPECT_EQ(
      1, user_action_tester.GetActionCount("Signin_Signin_FromBookmarkBubble"));

  // Give focus to a different tab.
  TabStripModel* tab_strip = browser()->tab_strip_model();
  ASSERT_EQ(0, tab_strip->active_index());
  GURL other_url = GURL("http://example.com");
  AddTab(browser(), other_url);
  tab_strip->ActivateTabAt(0, true);
  ASSERT_EQ(other_url, tab_strip->GetActiveWebContents()->GetVisibleURL());
  ASSERT_EQ(0, tab_strip->active_index());

  // Extensions re-use the tab but do not take focus.
  access_point_ = signin_metrics::AccessPoint::ACCESS_POINT_EXTENSIONS;
  EnableSync(AccountInfo(), false);
  EXPECT_EQ(
      1, user_action_tester.GetActionCount("Signin_Signin_FromBookmarkBubble"));
  EXPECT_EQ(0, tab_strip->active_index());

  // Other access points re-use the tab and take focus.
  access_point_ = signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS;
  EnableSync(AccountInfo(), false);
  EXPECT_EQ(
      1, user_action_tester.GetActionCount("Signin_Signin_FromBookmarkBubble"));
  EXPECT_EQ(1, tab_strip->active_index());
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

}  // namespace signin_ui_util
