// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/supervised_user/child_accounts/child_account_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_browser_utils.h"
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
#include "chrome/test/supervised_user/supervision_mixin.h"
#include "components/google/core/common/google_util.h"
#include "components/grit/components_resources.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/strings/grit/components_strings.h"
#include "components/supervised_user/core/browser/child_account_service.h"
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

class SupervisedUserPendingStateNavigationTest
    : public MixinBasedInProcessBrowserTest {
 public:
  SupervisedUserPendingStateNavigationTest() {
    scoped_feature_list_.InitAndEnableFeature(
        supervised_user::kForceSupervisedUserReauthenticationForBlockedSites);
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

  void SignInSupervisedUserAndWaitForInterstitialReload() {
    ASSERT_TRUE(
        identity_manager()->HasAccountWithRefreshTokenInPersistentErrorState(
            identity_manager()->GetPrimaryAccountId(
                signin::ConsentLevel::kSignin)));
    signin::AccountsInCookieJarInfo cookie_jar =
        identity_manager()->GetAccountsInCookieJar();
    ASSERT_FALSE(
        identity_manager()->GetAccountsInCookieJar().accounts_are_fresh);

    content::TestNavigationObserver observer(contents(), 1);
    kids_management_api_mock().AllowSubsequentClassifyUrl();
    supervision_mixin_.SignIn(
        supervised_user::SupervisionMixin::SignInMode::kSupervised);

    ASSERT_FALSE(
        identity_manager()->HasAccountWithRefreshTokenInPersistentErrorState(
            identity_manager()->GetPrimaryAccountId(
                signin::ConsentLevel::kSignin)));

    ASSERT_TRUE(
        identity_manager()->GetAccountsInCookieJar().accounts_are_fresh);

    // Wait for the re-auth page to be asynchronously reloaded.
    observer.WaitForNavigationFinished();
  }

  supervised_user::KidsManagementApiServerMock& kids_management_api_mock() {
    return supervision_mixin_.api_mock_setup_mixin().api_mock();
  }

  supervised_user::SupervisionMixin supervision_mixin_{
      mixin_host_,
      this,
      embedded_test_server(),
      {.sign_in_mode =
           supervised_user::SupervisionMixin::SignInMode::kSupervised,
       .embedded_test_server_options = {.resolver_rules_map_host_list =
                                            "*.example.com"}}};

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
  WaitForPageTitle(l10n_util::GetStringUTF16(IDS_BLOCK_INTERSTITIAL_TITLE));
  EXPECT_EQ(ui_test_utils::FindInPage(
                contents(),
                l10n_util::GetStringUTF16(IDS_CHILD_BLOCK_INTERSTITIAL_HEADER),
                /*forward=*/true, /*case_sensitive=*/true, /*ordinal=*/nullptr,
                /*selection_rect=*/nullptr),
            1);

  // Interact with the "Next" button, starting re-authentication.
  ASSERT_TRUE(content::ExecJs(
      contents(), "window.certificateErrorPageController.openLogin();"));

  // Sign in a supervised user, which completes re-authentication.
  SignInSupervisedUserAndWaitForInterstitialReload();

  // UKM should not be recorded for the blocked site interstitial.
  EXPECT_EQ(GetReauthInterstitialUKMTotalCount(), 0);
}

}  // namespace
