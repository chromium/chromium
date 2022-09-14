// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include <memory>

#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/browser/safe_browsing/chrome_ping_manager_factory.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/mock_download_item.h"
#include "components/safe_browsing/content/browser/safe_browsing_service_interface.h"
#include "components/safe_browsing/core/browser/ping_manager.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using network::GetUploadData;
using testing::Return;
using testing::ReturnRef;

namespace safe_browsing {

class SafeBrowsingServiceTest : public testing::Test {
 public:
  SafeBrowsingServiceTest() = default;

  void SetUp() override {
    browser_process_ = TestingBrowserProcess::GetGlobal();

    safe_browsing::SafeBrowsingServiceInterface::RegisterFactory(
        GetSafeBrowsingServiceFactory());
    // TODO(crbug/925153): Port consumers of the |sb_service_| to use
    // the interface in components/safe_browsing, and remove this cast.
    sb_service_ = static_cast<SafeBrowsingService*>(
        safe_browsing::SafeBrowsingService::CreateSafeBrowsingService());
    browser_process_->SetSafeBrowsingService(sb_service_.get());
    sb_service_->Initialize();
    base::RunLoop().RunUntilIdle();

    profile_ = std::make_unique<TestingProfile>();
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Local state is needed to construct ProxyConfigService, which is a
    // dependency of PingManager on ChromeOS.
    TestingBrowserProcess::GetGlobal()->SetLocalState(profile_->GetPrefs());
#endif
  }

  void TearDown() override {
    browser_process_->safe_browsing_service()->ShutDown();
    browser_process_->SetSafeBrowsingService(nullptr);
    safe_browsing::SafeBrowsingServiceInterface::RegisterFactory(nullptr);
#if BUILDFLAG(IS_CHROMEOS_ASH)
    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
#endif
    base::RunLoop().RunUntilIdle();
  }

  Profile* profile() { return profile_.get(); }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  raw_ptr<TestingBrowserProcess> browser_process_;
  scoped_refptr<SafeBrowsingService> sb_service_;
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(SafeBrowsingServiceTest, SendDownloadReport_Success) {
  std::unique_ptr<download::MockDownloadItem> download_item =
      std::make_unique<::testing::NiceMock<download::MockDownloadItem>>();
  const GURL url("http://example.com/");
  ClientSafeBrowsingReportRequest::ReportType report_type =
      ClientSafeBrowsingReportRequest::DANGEROUS_DOWNLOAD_OPENED;
  ClientDownloadResponse::Verdict download_verdict =
      ClientDownloadResponse::DANGEROUS_HOST;
  download::DownloadDangerType danger_type =
      download::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST;
  bool did_proceed = true;
  bool show_download_in_folder = true;
  std::string token = "download_token";

  content::DownloadItemUtils::AttachInfo(download_item.get(), profile(),
                                         /*web_contents=*/nullptr,
                                         content::GlobalRenderFrameHostId());
  EXPECT_CALL(*download_item, GetDangerType())
      .WillRepeatedly(Return(danger_type));
  EXPECT_CALL(*download_item, GetURL()).WillRepeatedly(ReturnRef(url));

  DownloadProtectionService::SetDownloadProtectionData(
      download_item.get(), token, download_verdict,
      ClientDownloadResponse::TailoredVerdict());

  auto* ping_manager =
      ChromePingManagerFactory::GetForBrowserContext(profile());
  network::TestURLLoaderFactory test_url_loader_factory;
  test_url_loader_factory.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        std::string request_string = GetUploadData(request);
        auto actual_request =
            std::make_unique<ClientSafeBrowsingReportRequest>();
        actual_request->ParseFromString(request_string);
        EXPECT_EQ(actual_request->type(), report_type);
        EXPECT_EQ(actual_request->download_verdict(), download_verdict);
        EXPECT_EQ(actual_request->url(), url.spec());
        EXPECT_EQ(actual_request->did_proceed(), did_proceed);
        EXPECT_EQ(actual_request->show_download_in_folder(),
                  show_download_in_folder);
        EXPECT_EQ(actual_request->token(), token);
      }));
  ping_manager->SetURLLoaderFactoryForTesting(
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory));

  bool is_successful = sb_service_->SendDownloadReport(
      download_item.get(), report_type, did_proceed, show_download_in_folder);
  EXPECT_TRUE(is_successful);
}

}  // namespace safe_browsing
