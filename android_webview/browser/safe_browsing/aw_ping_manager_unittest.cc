// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_browser_process.h"
#include "android_webview/browser/aw_feature_list_creator.h"
#include "android_webview/browser/safe_browsing/aw_ping_manager_factory.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/safe_browsing/content/browser/web_ui/safe_browsing_ui.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_content_client_initializer.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using safe_browsing::ClientSafeBrowsingReportRequest;
using ReportThreatDetailsResult =
    safe_browsing::PingManager::ReportThreatDetailsResult;

namespace android_webview {

class AwPingManagerTest : public testing::Test {
 protected:
  void SetUp() override {
    test_content_client_initializer_ =
        std::make_unique<content::TestContentClientInitializer>();
    aw_feature_list_creator_ = std::make_unique<AwFeatureListCreator>();
    aw_feature_list_creator_->CreateLocalState();
    browser_process_ =
        std::make_unique<AwBrowserProcess>(aw_feature_list_creator_.get());
    safe_browsing::WebUIInfoSingleton::GetInstance()->AddListenerForTesting();
  }

  void TearDown() override {
    base::RunLoop().RunUntilIdle();
    safe_browsing::WebUIInfoSingleton::GetInstance()->ClearListenerForTesting();
  }

  void RunReportThreatDetailsTest(bool is_remove_cookies_feature_enabled);

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<content::TestContentClientInitializer>
      test_content_client_initializer_;
  std::unique_ptr<AwBrowserProcess> browser_process_;
  std::unique_ptr<AwFeatureListCreator> aw_feature_list_creator_;
};

void AwPingManagerTest::RunReportThreatDetailsTest(
    bool is_remove_cookies_feature_enabled) {
  base::RunLoop csbrr_logged_run_loop;
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_feature_list;
  if (is_remove_cookies_feature_enabled) {
    scoped_feature_list.InitAndEnableFeature(
        safe_browsing::kSafeBrowsingRemoveCookiesInAuthRequests);
  } else {
    scoped_feature_list.InitAndDisableFeature(
        safe_browsing::kSafeBrowsingRemoveCookiesInAuthRequests);
  }
  safe_browsing::WebUIInfoSingleton::GetInstance()
      ->SetOnCSBRRLoggedCallbackForTesting(csbrr_logged_run_loop.QuitClosure());

  std::string report_content;
  std::unique_ptr<ClientSafeBrowsingReportRequest> report =
      std::make_unique<ClientSafeBrowsingReportRequest>();
  // The report must be non-empty. The selected property to set is arbitrary.
  report->set_type(ClientSafeBrowsingReportRequest::URL_PHISHING);
  EXPECT_TRUE(report->SerializeToString(&report_content));

  network::TestURLLoaderFactory test_url_loader_factory;
  test_url_loader_factory.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        EXPECT_EQ(GetUploadData(request), report_content);
        EXPECT_FALSE(
            request.headers.HasHeader(net::HttpRequestHeaders::kAuthorization));
        // Cookies should be attached when token is empty.
        EXPECT_EQ(request.credentials_mode,
                  network::mojom::CredentialsMode::kInclude);
        histogram_tester.ExpectUniqueSample(
            "SafeBrowsing.ClientSafeBrowsingReport.RequestHasToken",
            /*sample=*/false,
            /*expected_bucket_count=*/1);
      }));
  auto ref_counted_url_loader_factory =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory);
  safe_browsing::AwPingManagerFactory::GetInstance()
      ->SetURLLoaderFactoryForTesting(ref_counted_url_loader_factory);

  AwBrowserContext context;
  ReportThreatDetailsResult result =
      safe_browsing::AwPingManagerFactory::GetForBrowserContext(&context)
          ->ReportThreatDetails(std::move(report));
  EXPECT_EQ(result, ReportThreatDetailsResult::SUCCESS);
  csbrr_logged_run_loop.Run();
  EXPECT_EQ(
      safe_browsing::WebUIInfoSingleton::GetInstance()->csbrrs_sent().size(),
      1u);
}

TEST_F(AwPingManagerTest, ReportThreatDetails_RemoveCookiesFeatureEnabled) {
  RunReportThreatDetailsTest(/*is_remove_cookies_feature_enabled=*/true);
}

TEST_F(AwPingManagerTest, ReportThreatDetails_RemoveCookiesFeatureDisabled) {
  RunReportThreatDetailsTest(/*is_remove_cookies_feature_enabled=*/false);
}

TEST_F(AwPingManagerTest, ReportSafeBrowsingHit) {
  safe_browsing::HitReport hit_report;
  hit_report.post_data = "testing_hit_report_post_data";
  // Threat type and source are arbitrary but specified so that determining the
  // URL does not does throw an error due to input validation.
  hit_report.threat_type = safe_browsing::SB_THREAT_TYPE_URL_PHISHING;
  hit_report.threat_source = safe_browsing::ThreatSource::LOCAL_PVER4;

  network::TestURLLoaderFactory test_url_loader_factory;
  test_url_loader_factory.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        EXPECT_EQ(GetUploadData(request), hit_report.post_data);
      }));
  auto ref_counted_url_loader_factory =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory);
  safe_browsing::AwPingManagerFactory::GetInstance()
      ->SetURLLoaderFactoryForTesting(ref_counted_url_loader_factory);

  AwBrowserContext context;
  safe_browsing::AwPingManagerFactory::GetForBrowserContext(&context)
      ->ReportSafeBrowsingHit(hit_report);
}

}  // namespace android_webview
