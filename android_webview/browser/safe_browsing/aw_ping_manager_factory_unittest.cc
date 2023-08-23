// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/safe_browsing/aw_ping_manager_factory.h"
#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_browser_context_store.h"
#include "android_webview/browser/aw_browser_process.h"
#include "android_webview/browser/aw_feature_list_creator.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
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

class AwPingManagerFactoryTest : public testing::Test {
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

TEST_F(AwPingManagerFactoryTest, ReportThreatDetails) {
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
      }));
  auto ref_counted_url_loader_factory =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory);
  safe_browsing::AwPingManagerFactory::GetInstance()
      ->SetURLLoaderFactoryForTesting(ref_counted_url_loader_factory);

  AwBrowserContext context(
      AwBrowserContextStore::kDefaultContextName,
      base::FilePath(AwBrowserContextStore::kDefaultContextPath),
      /*is_default=*/true);
  EXPECT_EQ(safe_browsing::AwPingManagerFactory::GetForBrowserContext(&context)
                ->ReportThreatDetails(std::move(report)),
            ReportThreatDetailsResult::SUCCESS);
}

}  // namespace android_webview
