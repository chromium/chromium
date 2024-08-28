// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_verification_page.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/child_accounts/child_account_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_browser_utils.h"
#include "chrome/browser/supervised_user/supervised_user_verification_controller_client.h"
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
#include "components/supervised_user/core/browser/child_account_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_navigation_observer.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace {

using VerificationPurpose = SupervisedUserVerificationPage::VerificationPurpose;

// A NavigationThrottle that shows the verification interstitial.
class SupervisedUserVerificationPageTestingNavigationThrottle
    : public content::NavigationThrottle {
 public:
  explicit SupervisedUserVerificationPageTestingNavigationThrottle(
      content::NavigationHandle* handle)
      : content::NavigationThrottle(handle) {}

  // content::NavigationThrottle:
  const char* GetNameForLogging() override {
    return "SupervisedUserVerificationPageTestingNavigationThrottle";
  }

 private:
  // content::NavigationThrottle:
  content::NavigationThrottle::ThrottleCheckResult WillFailRequest() override {
    CHECK(navigation_handle()->IsInPrimaryMainFrame());

    content::WebContents* web_contents = navigation_handle()->GetWebContents();
    Profile* profile =
        Profile::FromBrowserContext(web_contents->GetBrowserContext());
    supervised_user::ChildAccountService* child_account_service =
        ChildAccountServiceFactory::GetForProfile(profile);
    CHECK(child_account_service);
    // Do not show the interstitial for authenticated supervised users.
    // TODO(362132528): We are only testing AuthState::AUTHENTICATED and
    // AuthState::NOT_AUTHENTICATED. We will show the interstitial exclusively
    // for AuthState::PENDING once it is set up.
    if (child_account_service->GetGoogleAuthState() ==
        supervised_user::ChildAccountService::AuthState::AUTHENTICATED) {
      return content::NavigationThrottle::PROCEED;
    }

    VerificationPurpose verification_purpose =
        VerificationPurpose::BLOCKED_SITE;

    // Show the appropriate re-auth interstitial for YouTube navigation.
    if (google_util::IsYoutubeDomainUrl(
            navigation_handle()->GetURL(), google_util::ALLOW_SUBDOMAIN,
            google_util::ALLOW_NON_STANDARD_PORTS)) {
      verification_purpose = VerificationPurpose::REAUTH_REQUIRED_SITE;
    }

    std::string interstitial_html =
        supervised_user::CreateReauthenticationInterstitial(
            *navigation_handle(), verification_purpose);
    return {CANCEL, net::ERR_CERT_COMMON_NAME_INVALID, interstitial_html};
  }
};

// A WebContentsObserver which installs a navigation throttle that creates
// SupervisedUserVerificationPage.
class TestingThrottleInstaller : public content::WebContentsObserver {
 public:
  explicit TestingThrottleInstaller(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override {
    navigation_handle->RegisterThrottleForTesting(
        std::make_unique<
            SupervisedUserVerificationPageTestingNavigationThrottle>(
            navigation_handle));
  }
};

class SupervisedUserVerificationPageTest
    : public MixinBasedInProcessBrowserTest {
 protected:
  void TestInterstitial(const GURL& request_url, bool should_record_ukm);

  void PreRunTestOnMainThread() override {
    InProcessBrowserTest::PreRunTestOnMainThread();
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  supervised_user::SupervisionMixin supervision_mixin_{
      mixin_host_,
      this,
      {.sign_in_mode =
           supervised_user::SupervisionMixin::SignInMode::kSignedOut}};

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

  supervised_user::KidsManagementApiServerMock& kids_management_api_mock() {
    return supervision_mixin_.api_mock_setup_mixin().api_mock();
  }

 private:
  std::unique_ptr<TestingThrottleInstaller> testing_throttle_installer_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
};

void SupervisedUserVerificationPageTest::TestInterstitial(
    const GURL& request_url,
    bool should_record_ukm) {
  int expected_individual_count = int(should_record_ukm);
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  CHECK(contents);
  testing_throttle_installer_ =
      std::make_unique<TestingThrottleInstaller>(contents);

  // Navigate to the requested URL and wait for the interstitial.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), request_url));
  content::RenderFrameHost* frame = contents->GetPrimaryMainFrame();
  ASSERT_TRUE(WaitForRenderFrameReady(frame));
  EXPECT_EQ(GetReauthInterstitialUKMCount("InterstitialShown"),
            expected_individual_count);

  // Interact with the "Next" button, starting re-authentication.
  ASSERT_TRUE(content::ExecJs(
      contents, "window.certificateErrorPageController.openLogin();"));
  EXPECT_EQ(GetReauthInterstitialUKMCount("ReauthenticationStarted"),
            expected_individual_count);

  // Sign in a supervised user, which completes re-authentication.
  content::TestNavigationObserver observer(contents, 1);
  kids_management_api_mock().AllowSubsequentClassifyUrl();
  supervision_mixin_.SignIn(
      supervised_user::SupervisionMixin::SignInMode::kSupervised);
  // Wait for the re-auth page to be asynchronously reloaded.
  observer.WaitForNavigationFinished();
  EXPECT_EQ(GetReauthInterstitialUKMCount("ReauthenticationCompleted"),
            expected_individual_count);
}

// Tests that UKMs are recorded for the YouTube main frame re-authentication
// interstitial.
IN_PROC_BROWSER_TEST_F(SupervisedUserVerificationPageTest,
                       UkmRecordedForYouTubeReauthInterstitial) {
  TestInterstitial(GURL("https://youtube.com/"), /*should_record_ukm=*/true);
}

// Tests that UKMs are not recorded for the blocked sites main frame
// re-authentication interstitial.
IN_PROC_BROWSER_TEST_F(SupervisedUserVerificationPageTest,
                       UkmNotRecordedForBlockingInterstitial) {
  TestInterstitial(GURL("https://example.com/"),
                   /*should_record_ukm=*/false);
}

}  // namespace
