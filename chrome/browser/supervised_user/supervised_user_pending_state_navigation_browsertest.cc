// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "build/buildflag.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/supervised_user/child_accounts/child_account_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_browser_utils.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_test_util.h"
#include "chrome/browser/supervised_user/supervised_user_verification_controller_client.h"
#include "chrome/browser/supervised_user/supervised_user_verification_page.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/supervised_user/child_account_test_utils.h"
#include "chrome/test/supervised_user/google_auth_state_waiter_mixin.h"
#include "chrome/test/supervised_user/supervision_mixin.h"
#include "components/google/core/common/google_util.h"
#include "components/grit/components_resources.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/strings/grit/components_strings.h"
#include "components/supervised_user/core/browser/child_account_service.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {

using ::testing::_;

bool IsReauthenticationInterstitialBeingShown(content::WebContents* content) {
  CHECK(content);
  std::string command =
      "document.querySelector('.supervised-user-verify') != null";
  return content::EvalJs(content, command).ExtractBool();
}

bool IsBlockedUrlInterstitialBeingShown(content::WebContents* content) {
  CHECK(content);
  std::string command =
      "document.querySelector('.supervised-user-block') != null";
  return content::EvalJs(content, command).ExtractBool();
}

class SupervisedUserPendingStateNavigationTest
    : public MixinBasedInProcessBrowserTest {
 public:
 protected:
  content::WebContents* contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  signin::IdentityManager* identity_manager() {
    return supervision_mixin_.GetIdentityTestEnvironment()->identity_manager();
  }

  void WaitForPageTitle(const std::u16string& page_title) {
    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return page_title == contents()->GetTitle(); }));
  }

  void SignInSupervisedUserAndWaitForInterstitialReload(
      content::WebContents* content,
      const GURL& url) {
    Profile* profile =
        Profile::FromBrowserContext(contents()->GetBrowserContext());
    ASSERT_TRUE(ChildAccountServiceFactory::GetForProfile(profile)
                    ->GetGoogleAuthState() !=
                supervised_user::ChildAccountService::AuthState::AUTHENTICATED);

    // Before sign-in the user still sees the re-authentication interstitial.
    ASSERT_TRUE(IsReauthenticationInterstitialBeingShown(content));
    ASSERT_FALSE(IsBlockedUrlInterstitialBeingShown(content));

    content::TestNavigationObserver observer(url);
    observer.WatchWebContents(content);
    kids_management_api_mock().AllowSubsequentClassifyUrl();

    supervision_mixin_.SignIn(
        supervised_user::SupervisionMixin::SignInMode::kSupervised);

    ASSERT_FALSE(
        identity_manager()->HasAccountWithRefreshTokenInPersistentErrorState(
            identity_manager()->GetPrimaryAccountId(
                signin::ConsentLevel::kSignin)));

    ASSERT_TRUE(
        identity_manager()->GetAccountsInCookieJar().AreAccountsFresh());

    observer.WaitForNavigationFinished();

    // Wait for the re-auth page to be asynchronously reloaded and replaced by
    // the blocked url interstitial.
    ASSERT_FALSE(IsReauthenticationInterstitialBeingShown(content));
    ASSERT_TRUE(IsBlockedUrlInterstitialBeingShown(content));
  }

  // Start sign-in flow by clicking on the primary button.
  bool StartSignInFlowFromContent(content::WebContents* interstitial_content) {
    return content::ExecJs(interstitial_content,
                           "document.querySelector('#primary-button').click()");
  }

  bool StartSignInFlowFromRenderFrameHost(content::RenderFrameHost* rfh) {
    return content::ExecJs(rfh,
                           "document.querySelector('#primary-button').click()");
  }

  void WaitForReauthenticationInterstitial() {
    WaitForPageTitle(l10n_util::GetStringUTF16(IDS_BLOCK_INTERSTITIAL_TITLE));
    // The "Next" button should be visible.
    EXPECT_FALSE(
        content::EvalJs(contents(),
                        "document.querySelector('#primary-button').hidden;")
            .ExtractBool());

    EXPECT_EQ(
        ui_test_utils::FindInPage(
            contents(),
            l10n_util::GetStringUTF16(IDS_CHILD_BLOCK_INTERSTITIAL_HEADER),
            /*forward=*/true, /*case_sensitive=*/true, /*ordinal=*/nullptr,
            /*selection_rect=*/nullptr),
        1);

    // The following string is found only on the the re-authentication
    // interstitial.
    EXPECT_EQ(
        ui_test_utils::FindInPage(
            contents(),
            l10n_util::GetStringUTF16(
                IDS_CHILD_BLOCK_INTERSTITIAL_MESSAGE_NOT_SIGNED_IN),
            /*forward=*/true, /*case_sensitive=*/true, /*ordinal=*/nullptr,
            /*selection_rect=*/nullptr),
        1);
  }

  supervised_user::KidsManagementApiServerMock& kids_management_api_mock() {
    return supervision_mixin_.api_mock_setup_mixin().api_mock();
  }

  supervised_user::SupervisionMixin supervision_mixin_{
      mixin_host_,
      this,
      embedded_test_server(),
      {.consent_level = signin::ConsentLevel::kSignin,
       .sign_in_mode =
           supervised_user::SupervisionMixin::SignInMode::kSupervised,
       .embedded_test_server_options = {.resolver_rules_map_host_list =
                                            "*.example.com"}}};

  void SetManualHost(const GURL& url, bool allowlist) {
    supervised_user_test_util::SetManualFilterForHost(browser()->profile(),
                                                      url.GetHost(), allowlist);
  }

  content::RenderFrameHost* FindFrameByName(const std::string& name) {
    content::RenderFrameHost* rfh = content::FrameMatchingPredicate(
        contents()->GetPrimaryPage(),
        base::BindRepeating(&content::FrameMatchesName, name));
    CHECK(rfh);
    CHECK(rfh->IsRenderFrameLive());
    return rfh;
  }

  std::string GetInnerHTMLString(
      const content::ToRenderFrameHost& execution_target) {
    return content::EvalJs(execution_target,
                           "document.documentElement.innerHTML")
        .ExtractString();
  }

  int GetTabCount() { return browser()->tab_strip_model()->count(); }
};

// Tests the blocked site main frame re-authentication interstitial.
IN_PROC_BROWSER_TEST_F(SupervisedUserPendingStateNavigationTest,
                       TestBlockedSiteMainFrameReauthInterstitial) {
  kids_management_api_mock().RestrictSubsequentClassifyUrl();
  supervision_mixin_.SetPendingStateForPrimaryAccount();
  // Navigate to the requested URL and wait for the interstitial.
  const auto url =
      embedded_test_server()->GetURL("/supervised_user/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ASSERT_TRUE(WaitForRenderFrameReady(contents()->GetPrimaryMainFrame()));

  // Verify that the blocked site interstitial is displayed.
  WaitForReauthenticationInterstitial();

  auto* interstitial_contents = contents();
  // Interact with the "Next" button, starting re-authentication.
  ASSERT_TRUE(StartSignInFlowFromContent(interstitial_contents));

  // Sign in a supervised user, which completes re-authentication.
  SignInSupervisedUserAndWaitForInterstitialReload(interstitial_contents, url);
}

// Tests that the sign-in tabs opened through the re-auth interstitial
// are closed on re-authentication.
IN_PROC_BROWSER_TEST_F(SupervisedUserPendingStateNavigationTest,
                       TestReauthInterstitialClosesSignInTabsAndReloads) {
  base::HistogramTester histogram_tester;

  kids_management_api_mock().RestrictSubsequentClassifyUrl();
  supervision_mixin_.SetPendingStateForPrimaryAccount();
  // Navigate to the requested URL and wait for the interstitial.
  auto original_tab_target_url =
      embedded_test_server()->GetURL("/supervised_user/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), original_tab_target_url));
  ASSERT_TRUE(WaitForRenderFrameReady(contents()->GetPrimaryMainFrame()));

  // Wait for the re-authentication interstitial. It should be the only tab.
  WaitForReauthenticationInterstitial();
  auto* interstitial_contents = contents();
  EXPECT_EQ(1, GetTabCount());

  // Interact with the "Next" button, starting re-authentication in a new tab, 3
  // times.
  for (int i = 1; i <= 3; i++) {
    ASSERT_TRUE(StartSignInFlowFromContent(interstitial_contents));
    EXPECT_EQ(i + 1, GetTabCount());

    // Wait for the navigation to finish in the sign-in tabs.
    if (browser()
            ->tab_strip_model()
            ->GetWebContentsAt(i)
            ->GetLastCommittedURL()
            .is_empty()) {
      content::TestNavigationObserver observer(
          browser()->tab_strip_model()->GetWebContentsAt(i));
      observer.WaitForNavigationFinished();
    }
    ASSERT_FALSE(browser()
                     ->tab_strip_model()
                     ->GetWebContentsAt(i)
                     ->GetLastCommittedURL()
                     .is_empty());
  }

  // Use one tab to navigate elsewhere.
  // We use a manually allow-listed url to avoid creating more
  // interstitials that would complicate the metrics checks.
  browser()->tab_strip_model()->ActivateTabAt(3);
  const GURL allowlisted_url = GURL("https://example.com/");
  SetManualHost(allowlisted_url, /*allowlist=*/true);
  // The `spawned_tab_url` is used to navigate away from the sign-in
  // content in one of the sign-in spawned tabs.
  // This url is not related to the `original_tab_target_url` that we visit
  // on the first -original- tab.
  const GURL spawned_tab_url = GURL(allowlisted_url.spec() + "/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), spawned_tab_url));

  // Sign in a supervised user, which completes re-authentication.
  // This results in closing the sign-in tabs (2 tabs).
  // Two tabs, the interstitial and the tab where the user performed a
  // navigation, remain open.
  SignInSupervisedUserAndWaitForInterstitialReload(interstitial_contents,
                                                   original_tab_target_url);
  EXPECT_EQ(2, GetTabCount());
}

IN_PROC_BROWSER_TEST_F(SupervisedUserPendingStateNavigationTest,
                       TestManualBlockedSiteMainFrameReauthInterstitial) {
  supervision_mixin_.SetPendingStateForPrimaryAccount();

  // Add exampleURL to the manual blocklist
  GURL exampleURL = GURL("https://example.com/");
  SetManualHost(exampleURL, /*allowlist=*/false);

  // Navigate to the requested URL and wait for the interstitial.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), exampleURL));
  ASSERT_TRUE(WaitForRenderFrameReady(contents()->GetPrimaryMainFrame()));

  WaitForReauthenticationInterstitial();
}

// Tests the YouTube main frame re-authentication interstitial.
IN_PROC_BROWSER_TEST_F(SupervisedUserPendingStateNavigationTest,
                       TestYouTubeMainFrameReauthInterstitial) {
  supervision_mixin_.SetPendingStateForPrimaryAccount();
  kids_management_api_mock().AllowSubsequentClassifyUrl();

  // Navigate to YouTube and wait for the interstitial.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("https://youtube.com/")));
  ASSERT_TRUE(WaitForRenderFrameReady(contents()->GetPrimaryMainFrame()));
  WaitForPageTitle(
      l10n_util::GetStringUTF16(IDS_SUPERVISED_USER_VERIFY_PAGE_TAB_TITLE));

  // Check that the YouTube interstitial contains the correct text.
  EXPECT_EQ(ui_test_utils::FindInPage(
                contents(),
                l10n_util::GetStringUTF16(
                    IDS_SUPERVISED_USER_VERIFY_PAGE_PRIMARY_PARAGRAPH),
                /*forward=*/true, /*case_sensitive=*/true, /*ordinal=*/nullptr,
                /*selection_rect=*/nullptr),
            1);

  // Open re-authentication in a new tab.
  ASSERT_TRUE(StartSignInFlowFromContent(contents()));
  EXPECT_EQ(2, GetTabCount());

  // Sign in a supervised user, which completes re-authentication.
  // This should close the sign-in tab.
  supervision_mixin_.SignIn(
      supervised_user::SupervisionMixin::SignInMode::kSupervised);
  EXPECT_EQ(1, GetTabCount());
}

// Tests the blocked site subframe re-authentication interstitial.
IN_PROC_BROWSER_TEST_F(SupervisedUserPendingStateNavigationTest,
                       TestBlockedSiteSubFrameReauthInterstitial) {
  supervision_mixin_.SetPendingStateForPrimaryAccount();

  // Filter the iframe URL.
  GURL exampleURL = GURL("https://iframe2.com/");
  SetManualHost(exampleURL, /*allowlist=*/false);
  kids_management_api_mock().AllowSubsequentClassifyUrl();

  GURL allowed_url_with_iframes = embedded_test_server()->GetURL(
      "www.example.com", "/supervised_user/with_iframes.html");

  // Navigate to the custom html containing 2 iframes.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), allowed_url_with_iframes));
  ASSERT_TRUE(WaitForRenderFrameReady(contents()->GetPrimaryMainFrame()));

  // Verify that the iframes are loaded.
  WaitForPageTitle(u"Supervised User test: page with iframes");
  EXPECT_TRUE(content::EvalJs(contents(), "loaded1()").ExtractBool());
  EXPECT_TRUE(content::EvalJs(contents(), "loaded2()").ExtractBool());

  SCOPED_TRACE("iframe2");
  content::RenderFrameHost* iframe2 = FindFrameByName("iframe2");

  // Check that the subframe interstitial contains the correct text.
  EXPECT_THAT(
      GetInnerHTMLString(iframe2),
      testing::HasSubstr(l10n_util::GetStringUTF8(
          IDS_SUPERVISED_USER_VERIFY_PAGE_SUBFRAME_BLOCKED_SITE_HEADING)));

  // Click the "Next" button, which should open re-authentication in a new tab.
  ASSERT_TRUE(StartSignInFlowFromRenderFrameHost(iframe2));
  EXPECT_EQ(2, GetTabCount());

  // Sign in a supervised user, which completes re-authentication.
  // This should close the sign-in tab.
  supervision_mixin_.SignIn(
      supervised_user::SupervisionMixin::SignInMode::kSupervised);
  ASSERT_TRUE(base::test::RunUntil([&]() { return GetTabCount() == 1; }));
}

// Tests the YouTube subframe re-authentication interstitial.
IN_PROC_BROWSER_TEST_F(SupervisedUserPendingStateNavigationTest,
                       TestYouTubeSubFrameReauthInterstitial) {
  base::HistogramTester histogram_tester;
  supervision_mixin_.SetPendingStateForPrimaryAccount();
  kids_management_api_mock().AllowSubsequentClassifyUrl();

  GURL url_with_youtube_iframes = embedded_test_server()->GetURL(
      "www.example.com", "/supervised_user/with_embedded_youtube_videos.html");

  // Navigate to the custom html containing 2 YouTube iframes.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), url_with_youtube_iframes));
  ASSERT_TRUE(WaitForRenderFrameReady(contents()->GetPrimaryMainFrame()));

  // Verify that the iframes are loaded.
  WaitForPageTitle(u"Supervised User test: page with embedded YouTube videos");
  EXPECT_TRUE(content::EvalJs(contents(), "loaded1()").ExtractBool());
  EXPECT_TRUE(content::EvalJs(contents(), "loaded2()").ExtractBool());

  content::RenderFrameHost* iframe1 = FindFrameByName("iframe1");
  content::RenderFrameHost* iframe2 = FindFrameByName("iframe2");

  // Check that the subframe interstitial contains the correct text.
  std::string subframe_description = l10n_util::GetStringUTF8(
      IDS_SUPERVISED_USER_VERIFY_PAGE_SUBFRAME_YOUTUBE_HEADING);
  EXPECT_THAT(GetInnerHTMLString(iframe1),
              testing::HasSubstr(subframe_description));
  EXPECT_THAT(GetInnerHTMLString(iframe2),
              testing::HasSubstr(subframe_description));

  // Click the "Next" buttons in both interstitials, which should open
  // re-authentication in two new tabs.
  ASSERT_TRUE(StartSignInFlowFromRenderFrameHost(iframe1));
  ASSERT_TRUE(StartSignInFlowFromRenderFrameHost(iframe2));
  EXPECT_EQ(3, GetTabCount());

  // Sign in a supervised user, which completes re-authentication.
  // This should close the sign-in tabs.
  supervision_mixin_.SignIn(
      supervised_user::SupervisionMixin::SignInMode::kSupervised);
  ASSERT_TRUE(base::test::RunUntil([&]() { return GetTabCount() == 1; }));
}

// Accepts a net::test_server::HttpRequest and checks if the google
// api key is present in the headers.
MATCHER(ContainsGoogleApiKey, "") {
  return base::Contains(arg.headers, "X-Goog-Api-Key");
}

// Tests that when the user doesn't have a valid access token the request is
// sent with an api key and not an access token (i.e an anonymous request).
// TODO(https://crbug.com/385450025): Flaky on Win ASAN.
#if BUILDFLAG(IS_WIN) && defined(ADDRESS_SANITIZER)
#define MAYBE_TestPendingStateRequestHasGoogleApiInHeader \
  DISABLED_TestPendingStateRequestHasGoogleApiInHeader
#else
#define MAYBE_TestPendingStateRequestHasGoogleApiInHeader \
  TestPendingStateRequestHasGoogleApiInHeader
#endif
IN_PROC_BROWSER_TEST_F(SupervisedUserPendingStateNavigationTest,
                       MAYBE_TestPendingStateRequestHasGoogleApiInHeader) {
  // TODO(crbug.com/365529863): Move the methods SetAutomaticIssueOfAccessTokens
  // and WaitForAccessTokenRequestIfNecessaryAndRespondWithError to
  // supervisionMixin::SetPendingStateForPrimaryAccount.
  supervision_mixin_.GetIdentityTestEnvironment()
      ->SetAutomaticIssueOfAccessTokens(false);
  supervision_mixin_.SetPendingStateForPrimaryAccount();
  // Invalidates any pending access token requests.
  supervision_mixin_.GetIdentityTestEnvironment()
      ->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
          supervision_mixin_.GetIdentityTestEnvironment()
              ->identity_manager()
              ->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
          GoogleServiceAuthError(
              GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS));
  kids_management_api_mock().AllowSubsequentClassifyUrl();

  ASSERT_TRUE(
      supervision_mixin_.GetIdentityTestEnvironment()
          ->identity_manager()
          ->HasAccountWithRefreshTokenInPersistentErrorState(
              supervision_mixin_.GetIdentityTestEnvironment()
                  ->identity_manager()
                  ->GetPrimaryAccountId(signin::ConsentLevel::kSignin)));

  EXPECT_CALL(kids_management_api_mock().classify_url_mock(),
              ClassifyUrl(ContainsGoogleApiKey()))
      .Times(1);

  content::TestNavigationObserver observer(contents());
  observer.set_expected_initial_url(GURL("https://example.com/"));

  contents()->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(
          GURL("https://example.com/")));

  // Any pending access token requests should respond with an error since the
  // access token is invalidated.
  supervision_mixin_.GetIdentityTestEnvironment()
      ->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
          supervision_mixin_.GetIdentityTestEnvironment()
              ->identity_manager()
              ->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
          GoogleServiceAuthError(
              GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS));

  ASSERT_TRUE(WaitForRenderFrameReady(contents()->GetPrimaryMainFrame()));

  observer.Wait();
}

}  // namespace
