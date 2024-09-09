// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_verification_page.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/supervised_user/child_accounts/child_account_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_verification_controller_client.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

// Wrapper class for the class under test.
// Useful for triggering protected methods.
class SupervisedUserVerificationPageForTest
    : public SupervisedUserVerificationPage {
 public:
  SupervisedUserVerificationPageForTest(
      content::WebContents* web_contents,
      const std::string& email_to_reauth,
      const GURL& request_url,
      VerificationPurpose verification_purpose,
      supervised_user::ChildAccountService* child_account_service,
      ukm::SourceId source_id,
      std::unique_ptr<
          security_interstitials::SecurityInterstitialControllerClient>
          controller_client)
      : SupervisedUserVerificationPage(web_contents,
                                       email_to_reauth,
                                       request_url,
                                       verification_purpose,
                                       child_account_service,
                                       source_id,
                                       std::move(controller_client)) {}

  // Trigger the interstitial command for starting a sign-in.
  void TriggerSignInRequestReceived() {
    // Triggers protected method.
    CommandReceived(base::NumberToString(
        static_cast<int>(security_interstitials::CMD_OPEN_LOGIN)));
  }
};

class SupervisedUserVerificationPageTest : public ::testing::Test {
 public:
  SupervisedUserVerificationPageTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    EXPECT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("test-user");
  }

  content::WebContents* web_contents() {
    if (!web_contents_) {
      content::WebContents::CreateParams params(profile_);
      web_contents_ = content::WebContents::Create(params);
    }
    return web_contents_.get();
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_;
  std::unique_ptr<content::WebContents> web_contents_;
};

TEST_F(SupervisedUserVerificationPageTest, TestHistograms) {
  base::HistogramTester histogram_tester;

  const GURL request_url("http://www.url.com/");
  supervised_user::ChildAccountService* child_account_service =
      ChildAccountServiceFactory::GetForProfile(profile_);
  CHECK(child_account_service);

  auto test_page = std::make_unique<SupervisedUserVerificationPageForTest>(
      web_contents(), profile_->GetProfileUserName(), request_url,
      SupervisedUserVerificationPage::VerificationPurpose::BLOCKED_SITE,
      child_account_service, ukm::kInvalidSourceId,
      std::make_unique<SupervisedUserVerificationControllerClient>(
          web_contents(), profile_->GetPrefs(),
          g_browser_process->GetApplicationLocale(),
          GURL(chrome::kChromeUINewTabURL), request_url));
  histogram_tester.ExpectBucketCount(
      "FamilyLinkUser.BlockedSiteVerifyItsYouInterstitialState",
      static_cast<int>(
          FamilyLinkUserReauthenticationInterstitialState::kInterstitialShown),
      /*expected_count=*/1);

  test_page->TriggerSignInRequestReceived();
  histogram_tester.ExpectBucketCount(
      "FamilyLinkUser.BlockedSiteVerifyItsYouInterstitialState",
      static_cast<int>(FamilyLinkUserReauthenticationInterstitialState::
                           kReauthenticationStarted),
      /*expected_count=*/1);

  test_page->OnReauthenticationCompleted();
  histogram_tester.ExpectBucketCount(
      "FamilyLinkUser.BlockedSiteVerifyItsYouInterstitialState",
      static_cast<int>(FamilyLinkUserReauthenticationInterstitialState::
                           kReauthenticationCompleted),
      /*expected_count=*/1);

  histogram_tester.ExpectTotalCount(
      "FamilyLinkUser.BlockedSiteVerifyItsYouInterstitialState", 3);
}
