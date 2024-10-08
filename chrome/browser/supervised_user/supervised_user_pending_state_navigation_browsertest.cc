// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "build/buildflag.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/supervised_user/child_accounts/child_account_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_browser_utils.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
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
#include "components/supervised_user/core/common/features.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_navigation_observer.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {

static constexpr std::string_view kUmaReauthenticationHistogramName =
    "FamilyLinkUser.BlockedSiteVerifyItsYouInterstitialState";

class SupervisedUserPendingStateNavigationTest
    : public MixinBasedInProcessBrowserTest {
 public:
  SupervisedUserPendingStateNavigationTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {supervised_user::kForceSupervisedUserReauthenticationForBlockedSites,
         supervised_user::kCloseSignTabsFromReauthenticationInterstitial,
         supervised_user::kUncredentialedFilteringFallbackForSupervisedUsers,
         supervised_user::kForceSupervisedUserReauthenticationForYouTube,
         supervised_user::kAllowSupervisedUserReauthenticationForSubframes},
        /*disabled_features=*/{});
  }

 protected:
  void PreRunTestOnMainThread() override {
    InProcessBrowserTest::PreRunTestOnMainThread();
    // TestAutoSetUkmRecorder should be initialized before UKMs are recorded.
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  content::WebContents* contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  signin::IdentityManager* identity_manager() {
    return supervision_mixin_.GetIdentityTestEnvironment()->identity_manager();
  }

  int GetReauthInterstitialUKMTotalCount() {
    auto entries = ukm_recorder_->GetEntriesByName(
        ukm::builders::FamilyLinkUser_ReauthenticationInterstitial::kEntryName);
    return entries.size();
  }

  void WaitForPageTitle(const std::u16string& page_title) {
    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return page_title == contents()->GetTitle(); }));
  }

  void SignInSupervisedUserAndWaitForInterstitialReload(
      content::WebContents* content) {
    Profile* profile =
        Profile::FromBrowserContext(contents()->GetBrowserContext());
    ASSERT_TRUE(ChildAccountServiceFactory::GetForProfile(profile)
                    ->GetGoogleAuthState() !=
                supervised_user::ChildAccountService::AuthState::AUTHENTICATED);

    content::TestNavigationObserver observer(content, 1);
    kids_management_api_mock().AllowSubsequentClassifyUrl();
    supervision_mixin_.SignIn(
        supervised_user::SupervisionMixin::SignInMode::kSupervised);

    ASSERT_FALSE(
        identity_manager()->HasAccountWithRefreshTokenInPersistentErrorState(
            identity_manager()->GetPrimaryAccountId(
                signin::ConsentLevel::kSignin)));

    ASSERT_TRUE(
        identity_manager()->GetAccountsInCookieJar().AreAccountsFresh());

    // Wait for the re-auth page to be asynchronously reloaded.
    observer.WaitForNavigationFinished();
  }

  // Start sign-in flow by clicking on the primary button.
  bool StartSignInFlowFromContent(content::WebContents* interstitial_content) {
    return content::ExecJs(interstitial_content,
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

    // The following string is found only on the the re-authentication interstitial.
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
      { // Syncing is a requirement for getting into pending mode pre-Uno.
        // Once the Uno feature `ExplicitBrowserSigninUIOnDesktop` is fully released
        // this can be set to Sign-in.
       .consent_level = signin::ConsentLevel::kSync,
       .sign_in_mode =
           supervised_user::SupervisionMixin::SignInMode::kSupervised,
       .embedded_test_server_options = {.resolver_rules_map_host_list =
                                            "*.example.com"}}};

  void SetManualHost(GURL url, bool allowlist) {
    supervised_user::SupervisedUserService* supervised_user_service =
        SupervisedUserServiceFactory::GetForProfile(browser()->profile());
    supervised_user::SupervisedUserURLFilter* url_filter =
        supervised_user_service->GetURLFilter();
    std::map<std::string, bool> hosts;
    hosts[url.host()] = allowlist;
    url_filter->SetManualHosts(std::move(hosts));
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

  int GetReauthInterstitialUKMCount(const std::string& metric_name) {
    int count = 0;
    auto entries = ukm_recorder_->GetEntriesByName(
        ukm::builders::FamilyLinkUser_ReauthenticationInterstitial::kEntryName);
    for (const ukm::mojom::UkmEntry* const entry : entries) {
      if (ukm_recorder_->GetEntryMetric(entry, metric_name)) {
        ++count;
      }
    }
    return count;
  }

 private:
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests the blocked site main frame re-authentication interstitial.
IN_PROC_BROWSER_TEST_F(SupervisedUserPendingStateNavigationTest,
                       DISABLED_TestBlockedSiteMainFrameReauthInterstitial) {
  kids_management_api_mock().RestrictSubsequentClassifyUrl();
  supervision_mixin_.SetPendingStateForPrimaryAccount();
  // Navigate to the requested URL and wait for the interstitial.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("https://example.com/")));
  ASSERT_TRUE(WaitForRenderFrameReady(contents()->GetPrimaryMainFrame()));

  // Verify that the blocked site interstitial is displayed.
  WaitForReauthenticationInterstitial();

  // Interact with the "Next" button, starting re-authentication.
  ASSERT_TRUE(content::ExecJs(
      contents(), "window.certificateErrorPageController.openLogin();"));

  // Sign in a supervised user, which completes re-authentication.
  SignInSupervisedUserAndWaitForInterstitialReload(contents());

  // UKM should not be recorded for the blocked site interstitial.
  EXPECT_EQ(GetReauthInterstitialUKMTotalCount(), 0);
}

// Tests that the sign-in tabs opened through the re-auth interstitial
// are closed on re-authentication.
// TODO(https://crbug.com/370115099): This test fails on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_TestReauthInterstitialClosesSignInTabsAndReloads \
  DISABLED_TestReauthInterstitialClosesSignInTabsAndReloads
#else
#define MAYBE_TestReauthInterstitialClosesSignInTabsAndReloads \
  TestReauthInterstitialClosesSignInTabsAndReloads
#endif
IN_PROC_BROWSER_TEST_F(SupervisedUserPendingStateNavigationTest,
                       MAYBE_TestReauthInterstitialClosesSignInTabsAndReloads) {
  base::HistogramTester histogram_tester;

  kids_management_api_mock().RestrictSubsequentClassifyUrl();
  supervision_mixin_.SetPendingStateForPrimaryAccount();
  // Navigate to the requested URL and wait for the interstitial.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("https://example.com/")));
  ASSERT_TRUE(WaitForRenderFrameReady(contents()->GetPrimaryMainFrame()));

  // Wait for the re-authentication interstitial. It should be the only tab.
  WaitForReauthenticationInterstitial();
  histogram_tester.ExpectBucketCount(
      kUmaReauthenticationHistogramName,
      static_cast<int>(SupervisedUserVerificationPage::Status::SHOWN), 1);
  auto* interstitial_contents = contents();
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  // Interact with the "Next" button, starting re-authentication in a new tab, 3
  // times.
  for (int i = 1; i <= 3; i++) {
    ASSERT_TRUE(StartSignInFlowFromContent(interstitial_contents));
    histogram_tester.ExpectBucketCount(
        kUmaReauthenticationHistogramName,
        static_cast<int>(
            SupervisedUserVerificationPage::Status::REAUTH_STARTED),
        i);
    EXPECT_EQ(i + 1, browser()->tab_strip_model()->count());

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
  browser()->tab_strip_model()->ActivateTabAt(3);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL("https://other.google.com/")));

  // Sign in a supervised user, which completes re-authentication.
  // This results in closing the sign-in tabs (2 tabs).
  // Two tabs, the interstitial and the tab where the user performed a
  // navigation, remain open.
  SignInSupervisedUserAndWaitForInterstitialReload(interstitial_contents);
  histogram_tester.ExpectBucketCount(
      kUmaReauthenticationHistogramName,
      static_cast<int>(
          SupervisedUserVerificationPage::Status::REAUTH_COMPLETED),
      1);
  EXPECT_EQ(2, browser()->tab_strip_model()->count());

  // TODO(b/370115099): Re-introduce a check that the blocked url interstitial
  // is shown.
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
  EXPECT_EQ(GetReauthInterstitialUKMCount("InterstitialShown"), 1);

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
  EXPECT_EQ(GetReauthInterstitialUKMCount("ReauthenticationStarted"), 1);
  EXPECT_EQ(2, browser()->tab_strip_model()->count());

  // Sign in a supervised user, which completes re-authentication.
  // This should records UKM metrics and close the sign-in tab.
  supervision_mixin_.SignIn(
      supervised_user::SupervisionMixin::SignInMode::kSupervised);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return GetReauthInterstitialUKMCount("ReauthenticationCompleted") == 1;
  }));
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  EXPECT_EQ(GetReauthInterstitialUKMTotalCount(), 3);
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

  // Check that the sub-frame interstitial contains the correct text.
  EXPECT_THAT(
      GetInnerHTMLString(iframe2),
      testing::HasSubstr(l10n_util::GetStringUTF8(
          IDS_SUPERVISED_USER_VERIFY_PAGE_SUBFRAME_BLOCKED_SITE_HEADING)));

  // UKM should not be recorded for the sub-frame interstitial.
  EXPECT_EQ(GetReauthInterstitialUKMTotalCount(), 0);
}

// Tests the YouTube subframe re-authentication interstitial.
IN_PROC_BROWSER_TEST_F(SupervisedUserPendingStateNavigationTest,
                       TestYouTubeSubFrameReauthInterstitial) {
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

  // Check that the sub-frame interstitial contains the correct text.
  std::string subframe_description = l10n_util::GetStringUTF8(
      IDS_SUPERVISED_USER_VERIFY_PAGE_SUBFRAME_YOUTUBE_HEADING);
  EXPECT_THAT(GetInnerHTMLString(iframe1),
              testing::HasSubstr(subframe_description));
  EXPECT_THAT(GetInnerHTMLString(iframe2),
              testing::HasSubstr(subframe_description));

  // UKM should not be recorded for the sub-frame interstitial.
  EXPECT_EQ(GetReauthInterstitialUKMTotalCount(), 0);
}

}  // namespace
