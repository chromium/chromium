// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_ui_util.h"

#include "base/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile_attributes_init_params.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/account_id/account_id.h"
#include "components/google/core/common/google_util.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "google_apis/gaia/gaia_urls.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "base/test/gmock_move_support.h"
#include "base/test/mock_callback.h"
#include "components/account_manager_core/mock_account_manager_facade.h"
#endif

namespace signin_ui_util {

namespace {
const char kMainEmail[] = "main_email@example.com";
const char kMainGaiaID[] = "main_gaia_id";
const char kSecondaryEmail[] = "secondary_email@example.com";
const char kSecondaryGaiaID[] = "secondary_gaia_id";
}  // namespace

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

#if BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)

namespace {

class SigninUiUtilTestBrowserWindow : public TestBrowserWindow {
 public:
  SigninUiUtilTestBrowserWindow() = default;

  SigninUiUtilTestBrowserWindow(const SigninUiUtilTestBrowserWindow&) = delete;
  SigninUiUtilTestBrowserWindow& operator=(
      const SigninUiUtilTestBrowserWindow&) = delete;

  ~SigninUiUtilTestBrowserWindow() override = default;
  void set_browser(Browser* browser) { browser_ = browser; }

  void ShowAvatarBubbleFromAvatarButton(
      AvatarBubbleMode mode,
      signin_metrics::AccessPoint access_point,
      bool is_source_keyboard) override {
    ASSERT_TRUE(browser_);
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    // TODO(https://crbug.com/1260291): add support for signed out profiles.
    NOTREACHED();
#else
    // Simulate what |BrowserView| does for a regular Chrome sign-in flow.
    browser_->signin_view_controller()->ShowSignin(
        profiles::BubbleViewMode::BUBBLE_VIEW_MODE_GAIA_SIGNIN, access_point);
#endif
  }

 private:
  raw_ptr<Browser> browser_ = nullptr;
};

class MockCreateTurnSyncOnHelper {
 public:
  struct CreateTurnSyncOnHelperParams {
   public:
    raw_ptr<Profile> profile = nullptr;
    raw_ptr<Browser> browser = nullptr;
    signin_metrics::AccessPoint signin_access_point =
        signin_metrics::AccessPoint::ACCESS_POINT_MAX;
    signin_metrics::PromoAction signin_promo_action =
        signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO;
    signin_metrics::Reason signin_reason =
        signin_metrics::Reason::kUnknownReason;
    CoreAccountId account_id;
    TurnSyncOnHelper::SigninAbortedMode signin_aborted_mode =
        TurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT;
  };

  MockCreateTurnSyncOnHelper() = default;
  ~MockCreateTurnSyncOnHelper() = default;

  internal::CreateTurnSyncOnHelperCallback GetCallback() {
    return base::BindOnce(&MockCreateTurnSyncOnHelper::CreateTurnSyncOnHelper,
                          base::Unretained(this));
  }

  bool WasCalled() { return create_turn_sync_on_helper_called_; }

  const CreateTurnSyncOnHelperParams& Params() {
    return create_turn_sync_on_helper_params_;
  }

 private:
  void CreateTurnSyncOnHelper(
      Profile* profile,
      Browser* browser,
      signin_metrics::AccessPoint signin_access_point,
      signin_metrics::PromoAction signin_promo_action,
      signin_metrics::Reason signin_reason,
      const CoreAccountId& account_id,
      TurnSyncOnHelper::SigninAbortedMode signin_aborted_mode) {
    create_turn_sync_on_helper_called_ = true;
    create_turn_sync_on_helper_params_.profile = profile;
    create_turn_sync_on_helper_params_.browser = browser;
    create_turn_sync_on_helper_params_.signin_access_point =
        signin_access_point;
    create_turn_sync_on_helper_params_.signin_promo_action =
        signin_promo_action;
    create_turn_sync_on_helper_params_.signin_reason = signin_reason;
    create_turn_sync_on_helper_params_.account_id = account_id;
    create_turn_sync_on_helper_params_.signin_aborted_mode =
        signin_aborted_mode;
  }

  bool create_turn_sync_on_helper_called_ = false;
  CreateTurnSyncOnHelperParams create_turn_sync_on_helper_params_;
};

}  // namespace

class SigninUiUtilTest : public BrowserWithTestWindowTest {
 public:
  SigninUiUtilTest() = default;
  ~SigninUiUtilTest() override = default;

 protected:
  // BrowserWithTestWindowTest:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    static_cast<SigninUiUtilTestBrowserWindow*>(browser()->window())
        ->set_browser(browser());
  }

  // BrowserWithTestWindowTest:
  TestingProfile::TestingFactories GetTestingFactories() override {
    return IdentityTestEnvironmentProfileAdaptor::
        GetIdentityTestEnvironmentFactories();
  }

  // BrowserWithTestWindowTest:
  std::unique_ptr<BrowserWindow> CreateBrowserWindow() override {
    return std::make_unique<SigninUiUtilTestBrowserWindow>();
  }

  // Returns the identity manager.
  signin::IdentityManager* GetIdentityManager() {
    return IdentityManagerFactory::GetForProfile(profile());
  }

  void EnableSync(const AccountInfo& account_info,
                  bool is_default_promo_account) {
    signin_ui_util::internal::EnableSyncFromPromo(
        browser(), account_info, access_point_, is_default_promo_account,
        mock_create_turn_sync_on_helper_.GetCallback());
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
  MockCreateTurnSyncOnHelper mock_create_turn_sync_on_helper_;
};

TEST_F(SigninUiUtilTest, EnableSyncWithExistingAccount) {
  CoreAccountId account_id =
      GetIdentityManager()->GetAccountsMutator()->AddOrUpdateAccount(
          kMainGaiaID, kMainEmail, "refresh_token", false,
          signin_metrics::SourceForRefreshTokenOperation::kUnknown);

  for (bool is_default_promo_account : {true, false}) {
    base::HistogramTester histogram_tester;
    base::UserActionTester user_action_tester;

    ExpectNoSigninStartedHistograms(histogram_tester);
    EXPECT_EQ(0, user_action_tester.GetActionCount(
                     "Signin_Signin_FromBookmarkBubble"));

    EnableSync(
        GetIdentityManager()->FindExtendedAccountInfoByAccountId(account_id),
        is_default_promo_account);
    signin_metrics::PromoAction expected_promo_action =
        is_default_promo_account
            ? signin_metrics::PromoAction::PROMO_ACTION_WITH_DEFAULT
            : signin_metrics::PromoAction::PROMO_ACTION_NOT_DEFAULT;
    ASSERT_TRUE(mock_create_turn_sync_on_helper_.WasCalled());
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
    EXPECT_EQ(profile(), mock_create_turn_sync_on_helper_.Params().profile);
    EXPECT_EQ(browser(), mock_create_turn_sync_on_helper_.Params().browser);
    EXPECT_EQ(account_id, mock_create_turn_sync_on_helper_.Params().account_id);
    EXPECT_EQ(signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_BUBBLE,
              mock_create_turn_sync_on_helper_.Params().signin_access_point);
    EXPECT_EQ(expected_promo_action,
              mock_create_turn_sync_on_helper_.Params().signin_promo_action);
    EXPECT_EQ(signin_metrics::Reason::kSigninPrimaryAccount,
              mock_create_turn_sync_on_helper_.Params().signin_reason);
    EXPECT_EQ(TurnSyncOnHelper::SigninAbortedMode::KEEP_ACCOUNT,
              mock_create_turn_sync_on_helper_.Params().signin_aborted_mode);
  }
}

// TODO(https://crbug.com/1260291): add support for signed out profiles.
#if !BUILDFLAG(IS_CHROMEOS_LACROS)

TEST_F(SigninUiUtilTest, EnableSyncWithAccountThatNeedsReauth) {
  AddTab(browser(), GURL("http://example.com"));
  CoreAccountId account_id =
      GetIdentityManager()->GetAccountsMutator()->AddOrUpdateAccount(
          kMainGaiaID, kMainEmail, "refresh_token", false,
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
    ASSERT_FALSE(mock_create_turn_sync_on_helper_.WasCalled());

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
    EXPECT_EQ(signin::GetChromeSyncURLForDice(kMainEmail,
                                              google_util::kGoogleHomepageURL),
              active_contents->GetVisibleURL());
    tab_strip->CloseWebContentsAt(
        tab_strip->GetIndexOfWebContents(active_contents),
        TabStripModel::CLOSE_USER_GESTURE);
  }
}

TEST_F(SigninUiUtilTest, EnableSyncForNewAccountWithNoTab) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  ExpectNoSigninStartedHistograms(histogram_tester);
  EXPECT_EQ(
      0, user_action_tester.GetActionCount("Signin_Signin_FromBookmarkBubble"));

  EnableSync(AccountInfo(), false /* is_default_promo_account (not used)*/);
  ASSERT_FALSE(mock_create_turn_sync_on_helper_.WasCalled());

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
  EXPECT_EQ(
      signin::GetChromeSyncURLForDice("", google_util::kGoogleHomepageURL),
      active_contents->GetVisibleURL());
}

TEST_F(SigninUiUtilTest, EnableSyncForNewAccountWithNoTabWithExisting) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  GetIdentityManager()->GetAccountsMutator()->AddOrUpdateAccount(
      kMainGaiaID, kMainEmail, "refresh_token", false,
      signin_metrics::SourceForRefreshTokenOperation::kUnknown);

  ExpectNoSigninStartedHistograms(histogram_tester);
  EXPECT_EQ(
      0, user_action_tester.GetActionCount("Signin_Signin_FromBookmarkBubble"));

  EnableSync(AccountInfo(), false /* is_default_promo_account (not used)*/);
  ASSERT_FALSE(mock_create_turn_sync_on_helper_.WasCalled());

  ExpectOneSigninStartedHistograms(
      histogram_tester,
      signin_metrics::PromoAction::PROMO_ACTION_NEW_ACCOUNT_EXISTING_ACCOUNT);
  EXPECT_EQ(
      1, user_action_tester.GetActionCount("Signin_Signin_FromBookmarkBubble"));
  EXPECT_EQ(1,
            user_action_tester.GetActionCount(
                "Signin_SigninNewAccountExistingAccount_FromBookmarkBubble"));
}

TEST_F(SigninUiUtilTest, EnableSyncForNewAccountWithOneTab) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;
  AddTab(browser(), GURL("http://foo/1"));

  ExpectNoSigninStartedHistograms(histogram_tester);
  EXPECT_EQ(
      0, user_action_tester.GetActionCount("Signin_Signin_FromBookmarkBubble"));

  EnableSync(AccountInfo(), false /* is_default_promo_account (not used)*/);
  ASSERT_FALSE(mock_create_turn_sync_on_helper_.WasCalled());

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
  EXPECT_EQ(
      signin::GetChromeSyncURLForDice("", google_util::kGoogleHomepageURL),
      active_contents->GetVisibleURL());
}

TEST_F(SigninUiUtilTest, GetOrderedAccountsForDisplay) {
  // Should start off with no accounts.
  std::vector<AccountInfo> accounts = GetOrderedAccountsForDisplay(
      profile(), /*restrict_to_accounts_eligible_for_sync=*/true);
  EXPECT_TRUE(accounts.empty());

  // TODO(tangltom): Flesh out this test.
}

TEST_F(SigninUiUtilTest, MergeDiceSigninTab) {
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
  tab_strip->ActivateTabAt(0, {TabStripModel::GestureType::kOther});
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
      browser(),
      signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN);

  // Verify that the active tab has the correct DICE sign-in URL.
  TabStripModel* tab_strip = browser()->tab_strip_model();
  content::WebContents* active_contents = tab_strip->GetActiveWebContents();
  ASSERT_TRUE(active_contents);
  EXPECT_EQ(signin::GetChromeSyncURLForDice(account_info.email,
                                            google_util::kGoogleHomepageURL),
            active_contents->GetVisibleURL());
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
      signin_metrics::SourceForRefreshTokenOperation::kUnknown);
  GetIdentityManager()->GetAccountsMutator()->AddOrUpdateAccount(
      kSecondaryGaiaID, kSecondaryEmail, "refresh_token", false,
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
      signin_metrics::SourceForRefreshTokenOperation::kUnknown);

  EXPECT_FALSE(ShouldShowAnimatedIdentityOnOpeningWindow(
      *profile_manager()->profile_attributes_storage(), profile()));
}

TEST_F(SigninUiUtilTest, ShowExtensionSigninPrompt) {
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
  EXPECT_TRUE(base::StartsWith(
      tab->GetVisibleURL().spec(),
      GaiaUrls::GetInstance()->signin_chrome_sync_dice().spec(),
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
  EXPECT_TRUE(
      base::StartsWith(tab->GetVisibleURL().spec(),
                       GaiaUrls::GetInstance()->add_account_url().spec(),
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
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
class MirrorSigninUiUtilTest : public BrowserWithTestWindowTest {
 public:
  MirrorSigninUiUtilTest() = default;
  ~MirrorSigninUiUtilTest() override = default;

  // BrowserWithTestWindowTest:
  TestingProfile::TestingFactories GetTestingFactories() override {
    return IdentityTestEnvironmentProfileAdaptor::
        GetIdentityTestEnvironmentFactories();
  }
};

TEST_F(MirrorSigninUiUtilTest, ShowReauthDialog) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile());
  const std::string kEmail = "foo@example.com";
  AccountInfo account_info = signin::MakePrimaryAccountAvailable(
      identity_manager, kEmail, signin::ConsentLevel::kSync);

  // Add an account and then put its refresh token into an error state to
  // require a reauth before enabling sync.
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_manager, account_info.account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));

  account_manager::MockAccountManagerFacade mock_facade;

  EXPECT_CALL(mock_facade,
              ShowReauthAccountDialog(
                  account_manager::AccountManagerFacade::AccountAdditionSource::
                      kAvatarBubbleReauthAccountButton,
                  kEmail));
  internal::ShowReauthForPrimaryAccountWithAuthErrorLacros(
      browser(),
      signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN,
      &mock_facade);
}

TEST_F(MirrorSigninUiUtilTest, ShowExtensionSigninPrompt_Signin) {
  account_manager::MockAccountManagerFacade mock_facade;
  base::MockOnceCallback<void(internal::OnAccountAddedCallback)>
      mock_add_account_callback;
  MockCreateTurnSyncOnHelper mock_create_turn_sync_on_helper;
  internal::OnAccountAddedCallback on_account_added_callback;

  EXPECT_CALL(mock_facade, ShowReauthAccountDialog(testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(mock_add_account_callback, Run(testing::_))
      .WillOnce(MoveArg<0>(&on_account_added_callback));
  internal::ShowExtensionSigninPrompt(
      browser()->profile(), &mock_facade, mock_add_account_callback.Get(),
      mock_create_turn_sync_on_helper.GetCallback(),
      /*enable_sync=*/false, std::string());
  // No tabs should be opened.
  EXPECT_EQ(0, browser()->tab_strip_model()->count());
  EXPECT_FALSE(mock_create_turn_sync_on_helper.WasCalled());

  std::move(on_account_added_callback).Run(CoreAccountId("test"));
  // TurnSyncOnHelper is not created because `enable_sync` is false.
  EXPECT_FALSE(mock_create_turn_sync_on_helper.WasCalled());
}

TEST_F(MirrorSigninUiUtilTest, ShowExtensionSigninPrompt_SigninCanceled) {
  account_manager::MockAccountManagerFacade mock_facade;
  base::MockOnceCallback<void(internal::OnAccountAddedCallback)>
      mock_add_account_callback;
  MockCreateTurnSyncOnHelper mock_create_turn_sync_on_helper;
  internal::OnAccountAddedCallback on_account_added_callback;

  EXPECT_CALL(mock_facade, ShowReauthAccountDialog(testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(mock_add_account_callback, Run(testing::_))
      .WillOnce(MoveArg<0>(&on_account_added_callback));
  internal::ShowExtensionSigninPrompt(
      browser()->profile(), &mock_facade, mock_add_account_callback.Get(),
      mock_create_turn_sync_on_helper.GetCallback(),
      /*enable_sync=*/true, std::string());
  // No tabs should be opened.
  EXPECT_EQ(0, browser()->tab_strip_model()->count());
  EXPECT_FALSE(mock_create_turn_sync_on_helper.WasCalled());

  std::move(on_account_added_callback).Run(CoreAccountId());
  // TurnSyncOnHelper is not created because an account wasn't added.
  EXPECT_FALSE(mock_create_turn_sync_on_helper.WasCalled());
}

TEST_F(MirrorSigninUiUtilTest, ShowExtensionSigninPrompt_Signin_EnableSync) {
  account_manager::MockAccountManagerFacade mock_facade;
  base::MockOnceCallback<void(internal::OnAccountAddedCallback)>
      mock_add_account_callback;
  MockCreateTurnSyncOnHelper mock_create_turn_sync_on_helper;
  internal::OnAccountAddedCallback on_account_added_callback;

  EXPECT_CALL(mock_facade, ShowReauthAccountDialog(testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(mock_add_account_callback, Run(testing::_))
      .WillOnce(MoveArg<0>(&on_account_added_callback));
  internal::ShowExtensionSigninPrompt(
      browser()->profile(), &mock_facade, mock_add_account_callback.Get(),
      mock_create_turn_sync_on_helper.GetCallback(),
      /*enable_sync=*/true, std::string());
  // No tabs should be opened.
  EXPECT_EQ(0, browser()->tab_strip_model()->count());
  EXPECT_FALSE(mock_create_turn_sync_on_helper.WasCalled());

  CoreAccountId account_id("test");
  std::move(on_account_added_callback).Run(account_id);

  // Verify that the helper to enable sync is created with the expected
  // params.
  EXPECT_TRUE(mock_create_turn_sync_on_helper.WasCalled());
  EXPECT_EQ(browser(), mock_create_turn_sync_on_helper.Params().browser);
  EXPECT_EQ(browser()->profile(),
            mock_create_turn_sync_on_helper.Params().profile);
  EXPECT_EQ(account_id, mock_create_turn_sync_on_helper.Params().account_id);
  EXPECT_EQ(signin_metrics::AccessPoint::ACCESS_POINT_EXTENSIONS,
            mock_create_turn_sync_on_helper.Params().signin_access_point);
  EXPECT_EQ(signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO,
            mock_create_turn_sync_on_helper.Params().signin_promo_action);
  EXPECT_EQ(signin_metrics::Reason::kSigninPrimaryAccount,
            mock_create_turn_sync_on_helper.Params().signin_reason);
  EXPECT_EQ(TurnSyncOnHelper::SigninAbortedMode::KEEP_ACCOUNT,
            mock_create_turn_sync_on_helper.Params().signin_aborted_mode);
}

TEST_F(MirrorSigninUiUtilTest, ShowExtensionSigninPrompt_Reauth) {
  const std::string kEmail = "foo@example.com";
  account_manager::MockAccountManagerFacade mock_facade;
  base::MockOnceCallback<void(internal::OnAccountAddedCallback)>
      mock_add_account_callback;
  MockCreateTurnSyncOnHelper mock_create_turn_sync_on_helper;

  EXPECT_CALL(
      mock_facade,
      ShowReauthAccountDialog(account_manager::AccountManagerFacade::
                                  AccountAdditionSource::kChromeExtensionReauth,
                              kEmail));
  EXPECT_CALL(mock_add_account_callback, Run(testing::_)).Times(0);
  internal::ShowExtensionSigninPrompt(
      browser()->profile(), &mock_facade, mock_add_account_callback.Get(),
      mock_create_turn_sync_on_helper.GetCallback(),
      /*enable_sync=*/true, kEmail);
  // No tabs should be opened.
  EXPECT_EQ(0, browser()->tab_strip_model()->count());
  EXPECT_FALSE(mock_create_turn_sync_on_helper.WasCalled());
}

TEST_F(MirrorSigninUiUtilTest,
       ShowExtensionSigninPrompt_Reauth_AsLockedProfile) {
  signin_util::ScopedForceSigninSetterForTesting force_signin_setter(true);
  Profile* profile = browser()->profile();
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());
  ASSERT_NE(entry, nullptr);
  entry->LockForceSigninProfile(true);

  const std::string kEmail = "foo@example.com";
  TabStripModel* tab_strip = browser()->tab_strip_model();
  account_manager::MockAccountManagerFacade mock_facade;
  base::MockOnceCallback<void(internal::OnAccountAddedCallback)>
      mock_add_account_callback;
  MockCreateTurnSyncOnHelper mock_create_turn_sync_on_helper;

  EXPECT_CALL(mock_facade, ShowReauthAccountDialog(testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(mock_add_account_callback, Run(testing::_)).Times(0);
  internal::ShowExtensionSigninPrompt(
      browser()->profile(), &mock_facade, mock_add_account_callback.Get(),
      mock_create_turn_sync_on_helper.GetCallback(),
      /*enable_sync=*/true, kEmail);
  // No dialogs and tabs should be opened.
  EXPECT_EQ(0, tab_strip->count());
  EXPECT_FALSE(mock_create_turn_sync_on_helper.WasCalled());
}

TEST_F(MirrorSigninUiUtilTest, ShowSigninPromptAndMaybeEnableSync_Signin) {
  base::MockOnceCallback<void(internal::OnAccountAddedCallback)>
      mock_add_account_callback;
  MockCreateTurnSyncOnHelper mock_create_turn_sync_on_helper;
  internal::OnAccountAddedCallback on_account_added_callback;

  auto access_point = signin_metrics::AccessPoint::ACCESS_POINT_MENU;
  auto promo_action = signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO;

  EXPECT_CALL(mock_add_account_callback, Run(testing::_))
      .WillOnce(MoveArg<0>(&on_account_added_callback));
  internal::ShowSigninPromptAndMaybeEnableSync(
      browser(), browser()->profile(), mock_add_account_callback.Get(),
      mock_create_turn_sync_on_helper.GetCallback(),
      /*enable_sync=*/false, access_point, promo_action);
  // No tabs should be opened.
  EXPECT_EQ(0, browser()->tab_strip_model()->count());
  EXPECT_FALSE(mock_create_turn_sync_on_helper.WasCalled());

  std::move(on_account_added_callback).Run(CoreAccountId("test"));
  // TurnSyncOnHelper is not created because `enable_sync` is false.
  EXPECT_FALSE(mock_create_turn_sync_on_helper.WasCalled());
}

TEST_F(MirrorSigninUiUtilTest,
       ShowSigninPromptAndMaybeEnableSync_SigninFailed) {
  base::MockOnceCallback<void(internal::OnAccountAddedCallback)>
      mock_add_account_callback;
  MockCreateTurnSyncOnHelper mock_create_turn_sync_on_helper;
  internal::OnAccountAddedCallback on_account_added_callback;

  auto access_point = signin_metrics::AccessPoint::ACCESS_POINT_MENU;
  auto promo_action = signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO;

  EXPECT_CALL(mock_add_account_callback, Run(testing::_))
      .WillOnce(MoveArg<0>(&on_account_added_callback));
  internal::ShowSigninPromptAndMaybeEnableSync(
      browser(), browser()->profile(), mock_add_account_callback.Get(),
      mock_create_turn_sync_on_helper.GetCallback(),
      /*enable_sync=*/false, access_point, promo_action);
  // No tabs should be opened.
  EXPECT_EQ(0, browser()->tab_strip_model()->count());
  EXPECT_FALSE(mock_create_turn_sync_on_helper.WasCalled());

  std::move(on_account_added_callback).Run(CoreAccountId());
  // TurnSyncOnHelper is not created because an account wasn't added.
  EXPECT_FALSE(mock_create_turn_sync_on_helper.WasCalled());
}

TEST_F(MirrorSigninUiUtilTest, ShowSigninPromptAndMaybeEnableSync_Sync) {
  base::MockOnceCallback<void(internal::OnAccountAddedCallback)>
      mock_add_account_callback;
  MockCreateTurnSyncOnHelper mock_create_turn_sync_on_helper;
  internal::OnAccountAddedCallback on_account_added_callback;

  auto access_point = signin_metrics::AccessPoint::ACCESS_POINT_MENU;
  auto promo_action = signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO;

  EXPECT_CALL(mock_add_account_callback, Run(testing::_))
      .WillOnce(MoveArg<0>(&on_account_added_callback));
  internal::ShowSigninPromptAndMaybeEnableSync(
      browser(), browser()->profile(), mock_add_account_callback.Get(),
      mock_create_turn_sync_on_helper.GetCallback(),
      /*enable_sync=*/true, access_point, promo_action);
  // No tabs should be opened.
  EXPECT_EQ(0, browser()->tab_strip_model()->count());
  EXPECT_FALSE(mock_create_turn_sync_on_helper.WasCalled());

  CoreAccountId account_id("test");
  std::move(on_account_added_callback).Run(account_id);

  // Verify that the helper to enable sync is created with the expected
  // params.
  EXPECT_TRUE(mock_create_turn_sync_on_helper.WasCalled());
  EXPECT_EQ(browser(), mock_create_turn_sync_on_helper.Params().browser);
  EXPECT_EQ(browser()->profile(),
            mock_create_turn_sync_on_helper.Params().profile);
  EXPECT_EQ(account_id, mock_create_turn_sync_on_helper.Params().account_id);
  EXPECT_EQ(access_point,
            mock_create_turn_sync_on_helper.Params().signin_access_point);
  EXPECT_EQ(promo_action,
            mock_create_turn_sync_on_helper.Params().signin_promo_action);
  EXPECT_EQ(signin_metrics::Reason::kSigninPrimaryAccount,
            mock_create_turn_sync_on_helper.Params().signin_reason);
  EXPECT_EQ(TurnSyncOnHelper::SigninAbortedMode::KEEP_ACCOUNT,
            mock_create_turn_sync_on_helper.Params().signin_aborted_mode);
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
      signin_metrics::SourceForRefreshTokenOperation::kUnknown);
  identity_manager->GetAccountsMutator()->AddOrUpdateAccount(
      kSecondaryGaiaID, kSecondaryEmail, "refresh_token", false,
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
