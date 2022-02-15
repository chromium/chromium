// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_browser_process.h"
#include "android_webview/browser/aw_feature_list_creator.h"
#include "android_webview/browser/safe_browsing/aw_ping_manager_factory.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "components/safe_browsing/core/browser/ping_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_content_client_initializer.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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
  }

  void TearDown() override { base::RunLoop().RunUntilIdle(); }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<content::TestContentClientInitializer>
      test_content_client_initializer_;
  std::unique_ptr<AwBrowserProcess> browser_process_;
  std::unique_ptr<AwFeatureListCreator> aw_feature_list_creator_;
};

TEST_F(AwPingManagerTest, ReportThreatDetails) {
  std::string report_content = "testing_report_content";
  network::TestURLLoaderFactory test_url_loader_factory;
  test_url_loader_factory.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        EXPECT_EQ(GetUploadData(request), report_content);
      }));
  auto ref_counted_url_loader_factory =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory);

  AwBrowserContext context;
  safe_browsing::AwPingManagerFactory::GetForBrowserContext(&context)
      ->ReportThreatDetails(ref_counted_url_loader_factory, report_content);
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

  AwBrowserContext context;
  safe_browsing::AwPingManagerFactory::GetForBrowserContext(&context)
      ->ReportSafeBrowsingHit(ref_counted_url_loader_factory, hit_report);
}

}  // namespace android_webview
