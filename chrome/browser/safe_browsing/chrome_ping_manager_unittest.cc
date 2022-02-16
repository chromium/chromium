// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/safe_browsing/chrome_ping_manager_factory.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/safe_browsing/core/browser/ping_manager.h"
#include "components/safe_browsing/core/common/features.h"
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
  void SetUp() override {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    ASSERT_TRUE(g_browser_process->profile_manager());

    sb_service_ =
        base::MakeRefCounted<safe_browsing::TestSafeBrowsingService>();
    TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(
        sb_service_.get());
    g_browser_process->safe_browsing_service()->Initialize();
  }

  void TearDown() override {
    base::RunLoop().RunUntilIdle();

    if (TestingBrowserProcess::GetGlobal()->safe_browsing_service()) {
      TestingBrowserProcess::GetGlobal()->safe_browsing_service()->ShutDown();
      TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(nullptr);
    }
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  scoped_refptr<safe_browsing::SafeBrowsingService> sb_service_;
};

TEST_F(ChromePingManagerTest, ReportThreatDetails) {
  raw_ptr<TestingProfile> profile =
      profile_manager_->CreateTestingProfile("testing_profile");

  auto* ping_manager = ChromePingManagerFactory::GetForBrowserContext(profile);

  std::string report_content = "testing_report_content";
  network::TestURLLoaderFactory test_url_loader_factory;
  test_url_loader_factory.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        EXPECT_EQ(GetUploadData(request), report_content);
      }));
  ping_manager->SetURLLoaderFactoryForTesting(
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory));

  ping_manager->ReportThreatDetails(report_content);
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
