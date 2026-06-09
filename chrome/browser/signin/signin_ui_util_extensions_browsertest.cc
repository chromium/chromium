// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_ui_util_extensions.h"

#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/with_feature_override.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_browser_test_base.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/signin/signin_ui_delegate.h"
#include "chrome/browser/signin/signin_ui_delegate_impl_dice.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/sync/base/features.h"
#include "content/public/test/browser_test.h"
#include "google_apis/gaia/gaia_urls.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(ENABLE_DICE_SUPPORT)
#error This file only contains DICE browser tests for now.
#endif

namespace signin_ui_util_extensions {
namespace {
const char kMainEmail[] = "main_email@example.com";
const GaiaId::Literal kMainGaiaID("main_gaia_id");
}  // namespace

using testing::_;

namespace {

class MockSigninUiDelegate : public signin_ui_util::SigninUiDelegateImplDice {
 public:
  MOCK_METHOD(void,
              ShowReauthUI,
              (Profile * profile,
               const std::string& email,
               bool enable_sync,
               signin_metrics::AccessPoint access_point,
               signin_metrics::PromoAction promo_action),
              ());
};
}  // namespace

class SigninUiUtilExtensionsTestBase : public ::SigninBrowserTestBase {
 public:
  SigninUiUtilExtensionsTestBase()
      : delegate_auto_reset_(
            SetSigninUiDelegateForExtensionsTesting(&mock_delegate_)) {
    ON_CALL(mock_delegate_, ShowReauthUI)
        .WillByDefault([this](Profile* profile, const std::string& email,
                              bool enable_sync,
                              signin_metrics::AccessPoint access_point,
                              signin_metrics::PromoAction promo_action) {
          mock_delegate_.SigninUiDelegateImplDice::ShowReauthUI(
              profile, email, enable_sync, access_point, promo_action);
        });
  }

 protected:
  // Returns the identity manager.
  signin::IdentityManager* GetIdentityManager() {
    return IdentityManagerFactory::GetForProfile(browser()->profile());
  }

  testing::StrictMock<MockSigninUiDelegate> mock_delegate_;
  base::AutoReset<signin_ui_util::SigninUiDelegate*> delegate_auto_reset_;
};

class SigninUiUtilExtensionsTest : public SigninUiUtilExtensionsTestBase {
 public:
  SigninUiUtilExtensionsTest() = default;
};

class SigninUiUtilExtensionsTest_ReplaceSyncPromosWithSignInPromos
    : public base::test::WithFeatureOverride,
      public SigninUiUtilExtensionsTestBase {
 public:
  SigninUiUtilExtensionsTest_ReplaceSyncPromosWithSignInPromos()
      : base::test::WithFeatureOverride(
            syncer::kReplaceSyncPromosWithSignInPromos) {
    feature_list_.InitWithFeatureState(
        syncer::kReplaceSyncPromosWithSigninPromosNewSignin,
        IsParamFeatureEnabled());
  }

  bool IsReplaceSyncPromosWithSignInPromosEnabled() const {
    return IsParamFeatureEnabled();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(
    SigninUiUtilExtensionsTest_ReplaceSyncPromosWithSignInPromos);

IN_PROC_BROWSER_TEST_P(
    SigninUiUtilExtensionsTest_ReplaceSyncPromosWithSignInPromos,
    ShowExtensionSigninPrompt) {
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
  EXPECT_NE(tab->GetVisibleURL().GetQuery().find("flow=promo"),
            std::string::npos);
}

IN_PROC_BROWSER_TEST_F(SigninUiUtilExtensionsTest,
                       ShowExtensionSigninPrompt_AsLockedProfile) {
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
  EXPECT_EQ(1, tab_strip->count());
  ShowExtensionSigninPrompt(profile, /*enable_sync=*/false,
                            /*email_hint=*/std::string());
  EXPECT_EQ(1, tab_strip->count());
}

IN_PROC_BROWSER_TEST_F(SigninUiUtilExtensionsTest,
                       ShowExtensionSigninPromptReauth) {
  CoreAccountId account_id =
      GetIdentityManager()->GetAccountsMutator()->AddOrUpdateAccount(
          kMainGaiaID, kMainEmail, "refresh_token", false,
          signin_metrics::AccessPoint::kStartPage,
          signin_metrics::SourceForRefreshTokenOperation::kUnknown);
  GetIdentityManager()->GetPrimaryAccountMutator()->SetPrimaryAccount(
      account_id, signin::ConsentLevel::kSignin,
      signin_metrics::AccessPoint::kStartPage);
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      GetIdentityManager(), account_id,
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::UNKNOWN));

  Profile* profile = browser()->profile();
  TabStripModel* tab_strip = browser()->tab_strip_model();
  EXPECT_CALL(
      mock_delegate_,
      ShowReauthUI(profile, kMainEmail, /*enable_sync=*/false,
                   signin_metrics::AccessPoint::kExtensions,
                   signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO));
  ShowExtensionSigninPrompt(profile, /*enable_sync=*/false, kMainEmail);
  EXPECT_EQ(1, tab_strip->count());

  content::WebContents* tab = tab_strip->GetWebContentsAt(0);
  ASSERT_TRUE(tab);
  EXPECT_TRUE(
      base::StartsWith(tab->GetVisibleURL().spec(),
                       GaiaUrls::GetInstance()->add_account_url().spec(),
                       base::CompareCase::INSENSITIVE_ASCII));
}

class DiceSigninUiUtilBrowserTest : public InProcessBrowserTest {
 public:
  DiceSigninUiUtilBrowserTest() = default;
  ~DiceSigninUiUtilBrowserTest() override = default;

  Profile* CreateProfile() {
    Profile* new_profile = nullptr;
    base::RunLoop run_loop;
    ProfileManager::CreateMultiProfileAsync(
        u"test_profile", /*icon_index=*/0, /*is_hidden=*/false,
        base::BindLambdaForTesting([&new_profile, &run_loop](Profile* profile) {
          ASSERT_TRUE(profile);
          new_profile = profile;
          run_loop.Quit();
        }));
    run_loop.Run();
    return new_profile;
  }
};

// Tests that `ShowExtensionSigninPrompt()` doesn't crash when it cannot create
// a new browser. Regression test for https://crbug.com/40806926.
IN_PROC_BROWSER_TEST_F(DiceSigninUiUtilBrowserTest,
                       ShowExtensionSigninPrompt_NoBrowser) {
  Profile* new_profile = CreateProfile();

  // New profile should not have any browser windows.
  EXPECT_FALSE(ProfileBrowserCollection::GetForProfile(new_profile)
                   ->GetLastActiveBrowser());

  ShowExtensionSigninPrompt(new_profile, /*enable_sync=*/false,
                            /*email_hint=*/std::string());
  // `ShowExtensionSigninPrompt()` creates a new browser.
  BrowserWindowInterface* browser =
      ProfileBrowserCollection::GetForProfile(new_profile)
          ->GetLastActiveBrowser();
  ASSERT_TRUE(browser);
  EXPECT_EQ(1, browser->GetTabStripModel()->count());

  // Scheduling a profile for deletion closes the browser. Prevent Profile from
  // being destroyed before we attempt to show the signin prompt.
  ScopedProfileKeepAlive profile_keep_alive(
      new_profile, ProfileKeepAliveOrigin::kBackgroundMode);
  ui_test_utils::BrowserDestroyedObserver observer(browser);
  g_browser_process->profile_manager()
      ->GetDeleteProfileHelper()
      .MaybeScheduleProfileForDeletion(
          new_profile->GetPath(), base::DoNothing(),
          ProfileMetrics::DELETE_PROFILE_USER_MANAGER);
  observer.Wait();
  EXPECT_FALSE(ProfileBrowserCollection::GetForProfile(new_profile)
                   ->GetLastActiveBrowser());

  // `ShowExtensionSigninPrompt()` does nothing for deleted profile.
  ShowExtensionSigninPrompt(new_profile, /*enable_sync=*/false,
                            /*email_hint=*/std::string());
  EXPECT_FALSE(ProfileBrowserCollection::GetForProfile(new_profile)
                   ->GetLastActiveBrowser());
}

}  // namespace signin_ui_util_extensions
