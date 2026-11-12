// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_ui_util_extensions.h"

#include <memory>

#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/dice_tab_helper.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_browser_test_base.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/signin/signin_ui_delegate.h"
#include "chrome/browser/signin/signin_ui_delegate_impl_dice.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/base_window.h"

#if !BUILDFLAG(ENABLE_DICE_SUPPORT)
#error This file only contains DICE browser tests for now.
#endif

namespace signin_ui_util_extensions {
namespace {
const char kMainEmail[] = "main_email@example.com";
const GaiaId::Literal kMainGaiaID("main_gaia_id");
const char kExtensionName[] = "Extension Name";
}  // namespace

using testing::_;

namespace {

class MockSigninUiDelegate : public signin_ui_util::SigninUiDelegateImplDice {
 public:
  MOCK_METHOD(void,
              ShowReauthUI,
              (Profile * profile,
               const std::string& email,
               bool enable_history_sync,
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

  testing::StrictMock<MockSigninUiDelegate> mock_delegate_;
  base::AutoReset<signin_ui_util::SigninUiDelegate*> delegate_auto_reset_;
};

// Extensions may repeat their signin request, e.g. each time they need an
// access token, so showing a browser window for each request would steal the
// focus from the window or the application that the user is using. Hidden
// windows are used to check that `Show()` is not called, as it makes them
// visible synchronously.
class SigninUiUtilExtensionsTest : public SigninUiUtilExtensionsTestBase {
 public:
  SigninUiUtilExtensionsTest() = default;

  void SetUpOnMainThread() override {
    SigninUiUtilExtensionsTestBase::SetUpOnMainThread();
    // Serve the Gaia pages, so that the signin tabs load successfully and
    // remain reusable while the test runs.
    url_loader_interceptor_ =
        std::make_unique<content::URLLoaderInterceptor>(base::BindRepeating(
            [](content::URLLoaderInterceptor::RequestParams* params) {
              if (!gaia::HasGaiaSchemeHostPort(params->url_request.url)) {
                return false;
              }
              content::URLLoaderInterceptor::WriteResponse(
                  "HTTP/1.1 200 OK\nContent-Type: text/html\n\n",
                  "<html><body>Fake signin page</body></html>",
                  params->client.get());
              return true;
            }));
  }

  void TearDownOnMainThread() override {
    url_loader_interceptor_.reset();
    SigninUiUtilExtensionsTestBase::TearDownOnMainThread();
  }

 protected:
  // Requests a signin from an extension, for a new account.
  void ShowExtensionSigninPromptForNewAccount() {
    ShowExtensionSigninPrompt(browser()->GetProfile(),
                              /*enable_history_sync=*/false,
                              /*email_hint=*/std::string(), kExtensionName);
  }

  // Returns the number of tabs of `browser` that are on a Chrome signin page.
  static int CountSigninTabs(BrowserWindowInterface* browser) {
    int count = 0;
    TabStripModel* tab_strip = browser->GetTabStripModel();
    for (int i = 0; i < tab_strip->count(); ++i) {
      DiceTabHelper* tab_helper =
          DiceTabHelper::FromWebContents(tab_strip->GetWebContentsAt(i));
      if (tab_helper && tab_helper->IsChromeSigninPage()) {
        ++count;
      }
    }
    return count;
  }

  // Waits until the active tab of `browser` is loaded.
  static void WaitForActiveTabLoaded(BrowserWindowInterface* browser) {
    EXPECT_TRUE(content::WaitForLoadStop(
        browser->GetTabStripModel()->GetActiveWebContents()));
  }

 private:
  std::unique_ptr<content::URLLoaderInterceptor> url_loader_interceptor_;
};

IN_PROC_BROWSER_TEST_F(SigninUiUtilExtensionsTest, ShowExtensionSigninPrompt) {
  const GURL sync_url = GaiaUrls::GetInstance()->signin_chrome_sync_dice();

  Profile* profile = browser()->GetProfile();
  TabStripModel* tab_strip = browser()->GetTabStripModel();
  ui::BaseWindow* window = browser()->GetWindow();
  window->Hide();
  ASSERT_FALSE(window->IsVisible());
  ShowExtensionSigninPrompt(profile, /*enable_history_sync=*/true,
                            /*email_hint=*/std::string(), kExtensionName);
  EXPECT_EQ(1, tab_strip->count());
  EXPECT_TRUE(window->IsVisible());
  // Calling the function again reuses the tab, and does not show the window.
  WaitForActiveTabLoaded(browser());
  window->Hide();
  ASSERT_FALSE(window->IsVisible());
  ShowExtensionSigninPrompt(profile, /*enable_history_sync=*/true,
                            /*email_hint=*/std::string(), kExtensionName);
  EXPECT_EQ(1, tab_strip->count());
  EXPECT_FALSE(window->IsVisible());

  content::WebContents* tab = tab_strip->GetWebContentsAt(0);
  ASSERT_TRUE(tab);
  EXPECT_TRUE(base::StartsWith(tab->GetVisibleURL().spec(), sync_url.spec(),
                               base::CompareCase::INSENSITIVE_ASCII));

  // Changing the parameter opens a new tab, and shows the window.
  ShowExtensionSigninPrompt(profile, /*enable_history_sync=*/false,
                            /*email_hint=*/std::string(), kExtensionName);
  EXPECT_EQ(2, tab_strip->count());
  EXPECT_TRUE(window->IsVisible());
  // Calling the function again reuses the tab, and does not show the window.
  WaitForActiveTabLoaded(browser());
  window->Hide();
  ASSERT_FALSE(window->IsVisible());
  ShowExtensionSigninPrompt(profile, /*enable_history_sync=*/false,
                            /*email_hint=*/std::string(), kExtensionName);
  EXPECT_EQ(2, tab_strip->count());
  EXPECT_FALSE(window->IsVisible());
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
  Profile* profile = browser()->GetProfile();
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());
  ASSERT_NE(entry, nullptr);
  entry->LockForceSigninProfile(true);
  TabStripModel* tab_strip = browser()->GetTabStripModel();
  ShowExtensionSigninPrompt(profile, /*enable_history_sync=*/true,
                            /*email_hint=*/std::string(), kExtensionName);
  EXPECT_EQ(1, tab_strip->count());
  ShowExtensionSigninPrompt(profile, /*enable_history_sync=*/false,
                            /*email_hint=*/std::string(), kExtensionName);
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

  Profile* profile = browser()->GetProfile();
  TabStripModel* tab_strip = browser()->GetTabStripModel();
  EXPECT_CALL(
      mock_delegate_,
      ShowReauthUI(profile, kMainEmail, /*enable_history_sync=*/false,
                   signin_metrics::AccessPoint::kExtensions,
                   signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO))
      .Times(2);
  ShowExtensionSigninPrompt(profile, /*enable_history_sync=*/false, kMainEmail,
                            kExtensionName);
  EXPECT_EQ(1, tab_strip->count());

  content::WebContents* tab = tab_strip->GetWebContentsAt(0);
  ASSERT_TRUE(tab);
  EXPECT_TRUE(
      base::StartsWith(tab->GetVisibleURL().spec(),
                       GaiaUrls::GetInstance()->add_account_url().spec(),
                       base::CompareCase::INSENSITIVE_ASCII));

  // Calling the function again reuses the tab, and does not show the window.
  WaitForActiveTabLoaded(browser());
  ui::BaseWindow* window = browser()->GetWindow();
  window->Hide();
  ASSERT_FALSE(window->IsVisible());
  ShowExtensionSigninPrompt(profile, /*enable_history_sync=*/false, kMainEmail,
                            kExtensionName);
  EXPECT_EQ(1, tab_strip->count());
  EXPECT_FALSE(window->IsVisible());
}

// The signin tab is reused even if it is not in the last active window, and
// no window is shown.
IN_PROC_BROWSER_TEST_F(SigninUiUtilExtensionsTest,
                       ReusedSigninTabInOtherWindow) {
  ShowExtensionSigninPromptForNewAccount();
  ASSERT_EQ(1, CountSigninTabs(browser()));
  WaitForActiveTabLoaded(browser());
  Profile* profile = browser()->GetProfile();
  BrowserWindowInterface* other_browser = CreateBrowser(profile);
  ASSERT_EQ(0, CountSigninTabs(other_browser));
  browser()->GetWindow()->Hide();
  other_browser->GetWindow()->Hide();
  ASSERT_FALSE(browser()->GetWindow()->IsVisible());
  ASSERT_FALSE(other_browser->GetWindow()->IsVisible());

  ShowExtensionSigninPromptForNewAccount();
  EXPECT_FALSE(browser()->GetWindow()->IsVisible());
  EXPECT_FALSE(other_browser->GetWindow()->IsVisible());
  EXPECT_EQ(1, CountSigninTabs(browser()));
  EXPECT_EQ(0, CountSigninTabs(other_browser));
  EXPECT_EQ(2u, ProfileBrowserCollection::GetForProfile(profile)->GetSize());
}

// A signin tab that the user navigated elsewhere, e.g. from the omnibox, is
// not reused: a new signin tab is opened, and the window is shown.
IN_PROC_BROWSER_TEST_F(SigninUiUtilExtensionsTest,
                       NavigatedAwaySigninTabIsNotReused) {
  ShowExtensionSigninPromptForNewAccount();
  ASSERT_EQ(1, CountSigninTabs(browser()));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUIVersionURL)));
  ASSERT_EQ(0, CountSigninTabs(browser()));
  ui::BaseWindow* window = browser()->GetWindow();
  window->Hide();
  ASSERT_FALSE(window->IsVisible());

  ShowExtensionSigninPromptForNewAccount();
  EXPECT_EQ(2, browser()->GetTabStripModel()->count());
  EXPECT_EQ(1, CountSigninTabs(browser()));
  EXPECT_TRUE(window->IsVisible());
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

  ShowExtensionSigninPrompt(new_profile, /*enable_history_sync=*/false,
                            /*email_hint=*/std::string(), kExtensionName);
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
  ShowExtensionSigninPrompt(new_profile, /*enable_history_sync=*/false,
                            /*email_hint=*/std::string(), kExtensionName);
  EXPECT_FALSE(ProfileBrowserCollection::GetForProfile(new_profile)
                   ->GetLastActiveBrowser());
}

}  // namespace signin_ui_util_extensions
