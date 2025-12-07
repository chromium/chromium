// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_ping_manager_factory.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "chrome/browser/safe_browsing/chrome_user_population_helper.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/safe_browsing/core/browser/ping_manager.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using network::GetUploadData;

namespace safe_browsing {

class ChromePingManagerFactoryTest : public testing::Test {
 protected:
  void SetUp() override;
  void TearDown() override;
  void RunReportThreatDetailsTest();
  void RunShouldFetchAccessTokenForReportTest(bool is_enhanced_protection,
                                              bool is_signed_in,
                                              bool expect_should_fetch);
  TestingProfile* SetUpProfile(bool is_enhanced_protection, bool is_signed_in);
  bool ShouldSendPersistedReport(Profile* profile);

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfileManager> profile_manager_;

 private:
  scoped_refptr<safe_browsing::SafeBrowsingService> sb_service_;
  ChromePingManagerAllowerForTesting allow_ping_manager_;
};

void ChromePingManagerFactoryTest::SetUp() {
  profile_manager_ = std::make_unique<TestingProfileManager>(
      TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(profile_manager_->SetUp());
  ASSERT_TRUE(g_browser_process->profile_manager());

  sb_service_ = base::MakeRefCounted<safe_browsing::TestSafeBrowsingService>();
  TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(sb_service_.get());
  g_browser_process->safe_browsing_service()->Initialize();
}

void ChromePingManagerFactoryTest::TearDown() {
  base::RunLoop().RunUntilIdle();

  if (TestingBrowserProcess::GetGlobal()->safe_browsing_service()) {
    TestingBrowserProcess::GetGlobal()->safe_browsing_service()->ShutDown();
    TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(nullptr);
  }
}

TestingProfile* ChromePingManagerFactoryTest::SetUpProfile(
    bool is_enhanced_protection,
    bool is_signed_in) {
  TestingProfile* profile = profile_manager_->CreateTestingProfile(
      "testing_profile", IdentityTestEnvironmentProfileAdaptor::
                             GetIdentityTestEnvironmentFactories());
  if (is_enhanced_protection) {
    SetSafeBrowsingState(profile->GetPrefs(),
                         SafeBrowsingState::ENHANCED_PROTECTION);
  }
  if (is_signed_in) {
    IdentityTestEnvironmentProfileAdaptor adaptor(profile);
    adaptor.identity_test_env()->MakePrimaryAccountAvailable(
        "testing@gmail.com", signin::ConsentLevel::kSync);
  }
  return profile;
}

void ChromePingManagerFactoryTest::RunShouldFetchAccessTokenForReportTest(
    bool is_enhanced_protection,
    bool is_signed_in,
    bool expect_should_fetch) {
  TestingProfile* profile = SetUpProfile(is_enhanced_protection, is_signed_in);
  EXPECT_EQ(ChromePingManagerFactory::ShouldFetchAccessTokenForReport(profile),
            expect_should_fetch);
}

void ChromePingManagerFactoryTest::RunReportThreatDetailsTest() {
  TestingProfile* profile =
      SetUpProfile(/*is_enhanced_protection=*/false, /*is_signed_in=*/false);
  auto* ping_manager = ChromePingManagerFactory::GetForBrowserContext(profile);

  std::string input_report_content;
  std::unique_ptr<ClientSafeBrowsingReportRequest> report =
      std::make_unique<ClientSafeBrowsingReportRequest>();
  // The report must be non-empty. The selected property to set is arbitrary.
  report->set_type(ClientSafeBrowsingReportRequest::URL_PHISHING);
  EXPECT_TRUE(report->SerializeToString(&input_report_content));
  ClientSafeBrowsingReportRequest expected_report;
  expected_report.ParseFromString(input_report_content);
  *expected_report.mutable_population() =
      safe_browsing::GetUserPopulationForProfile(profile);
  ChromeUserPopulation::PageLoadToken token =
      safe_browsing::GetPageLoadTokenForURL(profile, GURL(""));
  expected_report.mutable_population()->mutable_page_load_tokens()->Add()->Swap(
      &token);
  std::string expected_report_content;
  EXPECT_TRUE(expected_report.SerializeToString(&expected_report_content));

  network::TestURLLoaderFactory test_url_loader_factory;
  test_url_loader_factory.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        EXPECT_EQ(GetUploadData(request), expected_report_content);
      }));
  ping_manager->SetURLLoaderFactoryForTesting(
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory));

  EXPECT_EQ(ping_manager->ReportThreatDetails(std::move(report)),
            PingManager::ReportThreatDetailsResult::SUCCESS);
}

bool ChromePingManagerFactoryTest::ShouldSendPersistedReport(Profile* profile) {
  return ChromePingManagerFactory::ShouldSendPersistedReport(profile);
}

TEST_F(ChromePingManagerFactoryTest, ReportThreatDetails) {
  RunReportThreatDetailsTest();
}
TEST_F(ChromePingManagerFactoryTest, ShouldFetchAccessTokenForReport_Yes) {
  RunShouldFetchAccessTokenForReportTest(/*is_enhanced_protection=*/true,
                                         /*is_signed_in=*/true,
                                         /*expect_should_fetch=*/true);
}
TEST_F(ChromePingManagerFactoryTest,
       ShouldFetchAccessTokenForReport_NotSignedIn) {
  RunShouldFetchAccessTokenForReportTest(/*is_enhanced_protection=*/true,
                                         /*is_signed_in=*/false,
                                         /*expect_should_fetch=*/false);
}
TEST_F(ChromePingManagerFactoryTest,
       ShouldFetchAccessTokenForReport_NotEnhancedProtection) {
  RunShouldFetchAccessTokenForReportTest(/*is_enhanced_protection=*/false,
                                         /*is_signed_in=*/true,
                                         /*expect_should_fetch=*/false);
}

TEST_F(ChromePingManagerFactoryTest, ShouldSendPersistedReport_Yes) {
  TestingProfile* profile =
      SetUpProfile(/*is_enhanced_protection=*/true, /*is_signed_in=*/false);
  EXPECT_EQ(ShouldSendPersistedReport(profile), true);
}

TEST_F(ChromePingManagerFactoryTest,
       ShouldSendPersistedReport_NotEnhancedProtection) {
  TestingProfile* profile =
      SetUpProfile(/*is_enhanced_protection=*/false, /*is_signed_in=*/false);
  EXPECT_EQ(ShouldSendPersistedReport(profile), false);
}

TEST_F(ChromePingManagerFactoryTest, ShouldSendPersistedReport_Incognito) {
  TestingProfile* profile =
      SetUpProfile(/*is_enhanced_protection=*/true, /*is_signed_in=*/false);
  EXPECT_EQ(ShouldSendPersistedReport(
                TestingProfile::Builder().BuildIncognito(profile)),
            false);
}

TEST_F(ChromePingManagerFactoryTest, NoPingManagerForIncognito) {
  TestingProfile* profile = TestingProfile::Builder().BuildIncognito(
      profile_manager_->CreateTestingProfile("testing_profile"));
  EXPECT_EQ(ChromePingManagerFactory::GetForBrowserContext(profile), nullptr);
}

}  // namespace safe_browsing
