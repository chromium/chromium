// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/safe_browsing/chrome_ping_manager_factory.h"
#include "chrome/browser/safe_browsing/chrome_user_population_helper.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/safe_browsing/content/browser/web_ui/safe_browsing_ui.h"
#include "components/safe_browsing/core/browser/ping_manager.h"
#include "components/safe_browsing/core/browser/test_safe_browsing_token_fetcher.h"
#include "components/safe_browsing/core/common/features.h"
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

class ChromePingManagerTest : public testing::Test {
 protected:
  void SetUp() override;
  void TearDown() override;
  void RunReportThreatDetailsTest(bool is_enhanced_protection,
                                  bool is_signed_in,
                                  bool is_remove_cookies_feature_enabled,
                                  bool expect_access_token,
                                  bool expect_cookies_removed,
                                  bool is_csbrr_page_load_token_enabled);

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfileManager> profile_manager_;

 private:
  void SetUpFeatureList(bool should_enable_remove_cookies,
                        bool should_enable_csbrr_page_load_token);
  raw_ptr<TestingProfile> SetUpProfile(bool is_enhanced_protection,
                                       bool is_signed_in);
  TestSafeBrowsingTokenFetcher* SetUpTokenFetcher(PingManager* ping_manager);

  scoped_refptr<safe_browsing::SafeBrowsingService> sb_service_;
  base::test::ScopedFeatureList feature_list_;
};

void ChromePingManagerTest::SetUp() {
  profile_manager_ = std::make_unique<TestingProfileManager>(
      TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(profile_manager_->SetUp());
  ASSERT_TRUE(g_browser_process->profile_manager());

  sb_service_ = base::MakeRefCounted<safe_browsing::TestSafeBrowsingService>();
  TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(sb_service_.get());
  g_browser_process->safe_browsing_service()->Initialize();
  WebUIInfoSingleton::GetInstance()->AddListenerForTesting();
}

void ChromePingManagerTest::TearDown() {
  base::RunLoop().RunUntilIdle();

  if (TestingBrowserProcess::GetGlobal()->safe_browsing_service()) {
    TestingBrowserProcess::GetGlobal()->safe_browsing_service()->ShutDown();
    TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(nullptr);
  }

  feature_list_.Reset();
  WebUIInfoSingleton::GetInstance()->ClearListenerForTesting();
}

void ChromePingManagerTest::SetUpFeatureList(
    bool should_enable_remove_cookies,
    bool should_enable_csbrr_page_load_token) {
  std::vector<base::test::FeatureRef> enabled_features = {};
  std::vector<base::test::FeatureRef> disabled_features = {};
  if (should_enable_remove_cookies) {
    enabled_features.push_back(kSafeBrowsingRemoveCookiesInAuthRequests);
  } else {
    disabled_features.push_back(kSafeBrowsingRemoveCookiesInAuthRequests);
  }
  if (should_enable_csbrr_page_load_token) {
    enabled_features.push_back(kAddPageLoadTokenToClientSafeBrowsingReport);
  } else {
    disabled_features.push_back(kAddPageLoadTokenToClientSafeBrowsingReport);
  }
  feature_list_.InitWithFeatures(enabled_features, disabled_features);
}

raw_ptr<TestingProfile> ChromePingManagerTest::SetUpProfile(
    bool is_enhanced_protection,
    bool is_signed_in) {
  raw_ptr<TestingProfile> profile = profile_manager_->CreateTestingProfile(
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

TestSafeBrowsingTokenFetcher* ChromePingManagerTest::SetUpTokenFetcher(
    PingManager* ping_manager) {
  auto token_fetcher = std::make_unique<TestSafeBrowsingTokenFetcher>();
  auto* raw_token_fetcher = token_fetcher.get();
  ping_manager->SetTokenFetcherForTesting(std::move(token_fetcher));
  return raw_token_fetcher;
}

void ChromePingManagerTest::RunReportThreatDetailsTest(
    bool is_enhanced_protection,
    bool is_signed_in,
    bool is_remove_cookies_feature_enabled,
    bool expect_access_token,
    bool expect_cookies_removed,
    bool is_csbrr_page_load_token_enabled) {
  base::RunLoop csbrr_logged_run_loop;
  base::HistogramTester histogram_tester;
  SetUpFeatureList(is_remove_cookies_feature_enabled,
                   is_csbrr_page_load_token_enabled);
  raw_ptr<TestingProfile> profile =
      SetUpProfile(is_enhanced_protection, is_signed_in);
  auto* ping_manager = ChromePingManagerFactory::GetForBrowserContext(profile);
  auto* raw_token_fetcher = SetUpTokenFetcher(ping_manager);
  safe_browsing::WebUIInfoSingleton::GetInstance()
      ->SetOnCSBRRLoggedCallbackForTesting(csbrr_logged_run_loop.QuitClosure());

  std::string access_token = "testing_access_token";
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
  if (is_csbrr_page_load_token_enabled) {
    ChromeUserPopulation::PageLoadToken token =
        safe_browsing::GetPageLoadTokenForURL(profile, GURL(""));
    expected_report.mutable_population()
        ->mutable_page_load_tokens()
        ->Add()
        ->Swap(&token);
  }
  std::string expected_report_content;
  EXPECT_TRUE(expected_report.SerializeToString(&expected_report_content));
  EXPECT_NE(input_report_content, expected_report_content);

  network::TestURLLoaderFactory test_url_loader_factory;
  test_url_loader_factory.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        EXPECT_EQ(GetUploadData(request), expected_report_content);
        std::string header_value;
        bool found_header = request.headers.GetHeader(
            net::HttpRequestHeaders::kAuthorization, &header_value);
        EXPECT_EQ(found_header, expect_access_token);
        if (expect_access_token) {
          EXPECT_EQ(header_value, "Bearer " + access_token);
        }
        EXPECT_EQ(request.credentials_mode,
                  expect_cookies_removed
                      ? network::mojom::CredentialsMode::kOmit
                      : network::mojom::CredentialsMode::kInclude);
        histogram_tester.ExpectUniqueSample(
            "SafeBrowsing.ClientSafeBrowsingReport.RequestHasToken",
            /*sample=*/expect_access_token,
            /*expected_bucket_count=*/1);
      }));
  ping_manager->SetURLLoaderFactoryForTesting(
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory));

  PingManager::ReportThreatDetailsResult result =
      ping_manager->ReportThreatDetails(std::move(report));
  EXPECT_EQ(result, PingManager::ReportThreatDetailsResult::SUCCESS);
  EXPECT_EQ(raw_token_fetcher->WasStartCalled(), expect_access_token);
  if (expect_access_token) {
    raw_token_fetcher->RunAccessTokenCallback(access_token);
  }
  csbrr_logged_run_loop.Run();
  EXPECT_EQ(WebUIInfoSingleton::GetInstance()->csbrrs_sent().size(), 1u);
}

TEST_F(ChromePingManagerTest, ReportThreatDetailsWithAccessToken) {
  RunReportThreatDetailsTest(/*is_enhanced_protection=*/true,
                             /*is_signed_in=*/true,
                             /*is_remove_cookies_feature_enabled=*/true,
                             /*expect_access_token=*/true,
                             /*expect_cookies_removed=*/true,
                             /*is_csbrr_page_load_token_enabled=*/false);
}
TEST_F(ChromePingManagerTest,
       ReportThreatDetailsWithAccessToken_RemoveCookiesFeatureDisabled) {
  RunReportThreatDetailsTest(/*is_enhanced_protection=*/true,
                             /*is_signed_in=*/true,
                             /*is_remove_cookies_feature_enabled=*/false,
                             /*expect_access_token=*/true,
                             /*expect_cookies_removed=*/false,
                             /*is_csbrr_page_load_token_enabled*/ false);
}
TEST_F(ChromePingManagerTest,
       ReportThreatDetailsWithoutAccessToken_NotSignedIn) {
  RunReportThreatDetailsTest(/*is_enhanced_protection=*/true,
                             /*is_signed_in=*/false,
                             /*is_remove_cookies_feature_enabled=*/true,
                             /*expect_access_token=*/false,
                             /*expect_cookies_removed=*/false,
                             /*is_csbrr_page_load_token_enabled*/ false);
}
TEST_F(ChromePingManagerTest,
       ReportThreatDetailsWithoutAccessToken_NotEnhancedProtection) {
  RunReportThreatDetailsTest(/*is_enhanced_protection=*/false,
                             /*is_signed_in=*/true,
                             /*is_remove_cookies_feature_enabled=*/true,
                             /*expect_access_token=*/false,
                             /*expect_cookies_removed=*/false,
                             /*is_csbrr_page_load_token_enabled*/ false);
}
TEST_F(ChromePingManagerTest, ReportThreatDetailsWithoutAccessToken_Incognito) {
  raw_ptr<TestingProfile> profile = TestingProfile::Builder().BuildIncognito(
      profile_manager_->CreateTestingProfile("testing_profile"));
  EXPECT_EQ(ChromePingManagerFactory::GetForBrowserContext(profile), nullptr);
}

// TODO(crbug.com/1413210): remove test case when deprecating
// kAddPageLoadTokenToClientSafeBrowsingReport feature
TEST_F(ChromePingManagerTest,
       ReportThreatDetailsWithPageLoadToken_PageLoadTokenFeatureEnabled) {
  RunReportThreatDetailsTest(/*is_enhanced_protection=*/true,
                             /*is_signed_in=*/true,
                             /*is_remove_cookies_feature_enabled=*/true,
                             /*expect_access_token=*/true,
                             /*expect_cookies_removed=*/true,
                             /*is_csbrr_page_load_token_enabled*/ true);
}

TEST_F(ChromePingManagerTest, ReportSafeBrowsingHit) {
  raw_ptr<TestingProfile> profile =
      profile_manager_->CreateTestingProfile("testing_profile");
  auto* ping_manager = ChromePingManagerFactory::GetForBrowserContext(profile);

  HitReport hit_report;
  hit_report.post_data = "testing_hit_report_post_data";
  // Threat type and source are arbitrary but specified so that determining the
  // URL does not does throw an error due to input validation.
  hit_report.threat_type = SB_THREAT_TYPE_URL_PHISHING;
  hit_report.threat_source = ThreatSource::LOCAL_PVER4;

  network::TestURLLoaderFactory test_url_loader_factory;
  test_url_loader_factory.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        EXPECT_EQ(GetUploadData(request), hit_report.post_data);
      }));
  ping_manager->SetURLLoaderFactoryForTesting(
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory));

  ping_manager->ReportSafeBrowsingHit(hit_report);
}

}  // namespace safe_browsing
