// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include <memory>

#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/browser/download/download_item_warning_data.h"
#include "chrome/browser/safe_browsing/chrome_ping_manager_factory.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/mock_download_item.h"
#include "components/safe_browsing/content/browser/safe_browsing_service_interface.h"
#include "components/safe_browsing/core/browser/ping_manager.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"

using network::GetUploadData;
using testing::Return;
using testing::ReturnRef;

using WarningSurface = DownloadItemWarningData::WarningSurface;
using WarningAction = DownloadItemWarningData::WarningAction;
using WarningActionEvent = DownloadItemWarningData::WarningActionEvent;
using DownloadWarningAction =
    safe_browsing::ClientSafeBrowsingReportRequest::DownloadWarningAction;

namespace safe_browsing {

namespace {
const char kTestDownloadUrl[] = "https://example.com";
}

class SafeBrowsingServiceTest : public testing::Test {
 public:
  SafeBrowsingServiceTest() {
    feature_list_.InitAndEnableFeature(
        safe_browsing::kSafeBrowsingCsbrrNewDownloadTrigger);
  }

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
  void SetUpDownload() {
    content::DownloadItemUtils::AttachInfoForTesting(&download_item_, profile(),
                                                     /*web_contents=*/nullptr);
    EXPECT_CALL(download_item_, GetDangerType())
        .WillRepeatedly(Return(download::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST));
    EXPECT_CALL(download_item_, GetURL())
        .WillRepeatedly(ReturnRef(download_url_));

    DownloadProtectionService::SetDownloadProtectionData(
        &download_item_, "download_token",
        ClientDownloadResponse::DANGEROUS_HOST,
        ClientDownloadResponse::TailoredVerdict());
    DownloadItemWarningData::AddWarningActionEvent(
        &download_item_, WarningSurface::BUBBLE_MAINPAGE, WarningAction::SHOWN);
    DownloadItemWarningData::AddWarningActionEvent(
        &download_item_, WarningSurface::BUBBLE_SUBPAGE, WarningAction::CLOSE);
    DownloadItemWarningData::AddWarningActionEvent(
        &download_item_, WarningSurface::DOWNLOADS_PAGE,
        WarningAction::DISCARD);
  }

  std::unique_ptr<ClientSafeBrowsingReportRequest> GetActualRequest(
      const network::ResourceRequest& request) {
    std::string request_string = GetUploadData(request);
    auto actual_request = std::make_unique<ClientSafeBrowsingReportRequest>();
    actual_request->ParseFromString(request_string);
    return actual_request;
  }

  void VerifyDownloadReportRequest(
      ClientSafeBrowsingReportRequest* actual_request) {
    EXPECT_EQ(actual_request->type(),
              ClientSafeBrowsingReportRequest::DANGEROUS_DOWNLOAD_OPENED);
    EXPECT_EQ(actual_request->download_verdict(),
              ClientDownloadResponse::DANGEROUS_HOST);
    EXPECT_EQ(actual_request->url(), download_url_.spec());
    EXPECT_TRUE(actual_request->did_proceed());
    EXPECT_TRUE(actual_request->show_download_in_folder());
    EXPECT_EQ(actual_request->token(), "download_token");
  }

  void VerifyInteractionOccurrenceCount(
      const ClientSafeBrowsingReportRequest& report,
      const ClientSafeBrowsingReportRequest::PhishySiteInteraction::
          PhishySiteInteractionType& expected_interaction_type,
      const int& expected_occurrence_count) {
    // Find the interaction within the report by comparing
    // security_interstitial_interaction.
    for (auto interaction : report.phishy_site_interactions()) {
      if (interaction.phishy_site_interaction_type() ==
          expected_interaction_type) {
        EXPECT_EQ(interaction.occurrence_count(), expected_occurrence_count);
        break;
      }
    }
  }

  void VerifyPhishySiteReportRequest(
      ClientSafeBrowsingReportRequest* actual_request,
      GURL& expected_url,
      int expected_interaction_size,
      int expected_click_occurrences,
      int expected_key_occurrences,
      int expected_paste_occurrences) {
    EXPECT_EQ(actual_request->type(),
              ClientSafeBrowsingReportRequest::PHISHY_SITE_INTERACTIONS);
    EXPECT_EQ(actual_request->url(), expected_url.spec());
    EXPECT_EQ(actual_request->phishy_site_interactions_size(),
              expected_interaction_size);
    VerifyInteractionOccurrenceCount(
        *actual_request,
        ClientSafeBrowsingReportRequest::PhishySiteInteraction::
            PHISHY_CLICK_EVENT,
        expected_click_occurrences);
    VerifyInteractionOccurrenceCount(
        *actual_request,
        ClientSafeBrowsingReportRequest::PhishySiteInteraction::
            PHISHY_KEY_EVENT,
        expected_key_occurrences);
    VerifyInteractionOccurrenceCount(
        *actual_request,
        ClientSafeBrowsingReportRequest::PhishySiteInteraction::
            PHISHY_PASTE_EVENT,
        expected_paste_occurrences);
  }

  void VerifyDownloadWarningAction(
      const ClientSafeBrowsingReportRequest::DownloadWarningAction&
          warning_action,
      DownloadWarningAction::Surface surface,
      DownloadWarningAction::Action action,
      bool is_terminal_action,
      int64_t interval_msec) {
    EXPECT_EQ(warning_action.surface(), surface);
    EXPECT_EQ(warning_action.action(), action);
    EXPECT_EQ(warning_action.is_terminal_action(), is_terminal_action);
    EXPECT_EQ(warning_action.interval_msec(), interval_msec);
  }

  content::BrowserTaskEnvironment task_environment_;
  raw_ptr<TestingBrowserProcess> browser_process_;
  scoped_refptr<SafeBrowsingService> sb_service_;
  std::unique_ptr<TestingProfile> profile_;

  ::testing::NiceMock<download::MockDownloadItem> download_item_;
  GURL download_url_ = GURL(kTestDownloadUrl);

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(SafeBrowsingServiceTest, SendDownloadReport_Success) {
  SetUpDownload();
  SetExtendedReportingPrefForTests(profile_->GetPrefs(), true);

  auto* ping_manager =
      ChromePingManagerFactory::GetForBrowserContext(profile());
  network::TestURLLoaderFactory test_url_loader_factory;
  test_url_loader_factory.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        std::unique_ptr<ClientSafeBrowsingReportRequest> actual_request =
            GetActualRequest(request);
        VerifyDownloadReportRequest(actual_request.get());
        ASSERT_EQ(actual_request->download_warning_actions().size(), 2);
        VerifyDownloadWarningAction(
            actual_request->download_warning_actions().Get(0),
            DownloadWarningAction::BUBBLE_SUBPAGE, DownloadWarningAction::CLOSE,
            /*is_terminal_action=*/false, /*interval_msec=*/0);
        VerifyDownloadWarningAction(
            actual_request->download_warning_actions().Get(1),
            DownloadWarningAction::DOWNLOADS_PAGE,
            DownloadWarningAction::DISCARD,
            /*is_terminal_action=*/true, /*interval_msec=*/0);
      }));
  ping_manager->SetURLLoaderFactoryForTesting(
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory));

  EXPECT_TRUE(sb_service_->SendDownloadReport(
      &download_item_,
      ClientSafeBrowsingReportRequest::DANGEROUS_DOWNLOAD_OPENED,
      /*did_proceed=*/true,
      /*show_download_in_folder=*/true));
}

TEST_F(
    SafeBrowsingServiceTest,
    SendDownloadReport_NoDownloadWarningActionWhenExtendedReportingDisabled) {
  SetUpDownload();
  SetExtendedReportingPrefForTests(profile_->GetPrefs(), false);

  auto* ping_manager =
      ChromePingManagerFactory::GetForBrowserContext(profile());
  network::TestURLLoaderFactory test_url_loader_factory;
  test_url_loader_factory.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        std::unique_ptr<ClientSafeBrowsingReportRequest> actual_request =
            GetActualRequest(request);
        EXPECT_TRUE(actual_request->download_warning_actions().empty());
      }));
  ping_manager->SetURLLoaderFactoryForTesting(
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory));

  EXPECT_TRUE(sb_service_->SendDownloadReport(
      &download_item_,
      ClientSafeBrowsingReportRequest::DANGEROUS_DOWNLOAD_OPENED,
      /*did_proceed=*/true,
      /*show_download_in_folder=*/true));
}

class SafeBrowsingServiceTestWithCsbrrNewTriggerDisabled
    : public SafeBrowsingServiceTest {
 public:
  SafeBrowsingServiceTestWithCsbrrNewTriggerDisabled() {
    feature_list_.InitAndDisableFeature(
        safe_browsing::kSafeBrowsingCsbrrNewDownloadTrigger);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(SafeBrowsingServiceTestWithCsbrrNewTriggerDisabled,
       SendDownloadReport_NoDownloadWarningActionWhenFeatureFlagDisabled) {
  SetUpDownload();
  SetExtendedReportingPrefForTests(profile_->GetPrefs(), true);

  auto* ping_manager =
      ChromePingManagerFactory::GetForBrowserContext(profile());
  network::TestURLLoaderFactory test_url_loader_factory;
  test_url_loader_factory.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        std::unique_ptr<ClientSafeBrowsingReportRequest> actual_request =
            GetActualRequest(request);
        EXPECT_TRUE(actual_request->download_warning_actions().empty());
      }));
  ping_manager->SetURLLoaderFactoryForTesting(
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory));

  EXPECT_TRUE(sb_service_->SendDownloadReport(
      &download_item_,
      ClientSafeBrowsingReportRequest::DANGEROUS_DOWNLOAD_OPENED,
      /*did_proceed=*/true,
      /*show_download_in_folder=*/true));
}

class SafeBrowsingServiceTestWithAntiPhishingTelemetryEnabled
    : public SafeBrowsingServiceTest {
 public:
  SafeBrowsingServiceTestWithAntiPhishingTelemetryEnabled() {
    feature_list_.InitAndEnableFeature(safe_browsing::kAntiPhishingTelemetry);
  }

 protected:
  PhishySiteInteractionMap SetUpPhishyInteractionMap(
      int expected_click_occurrences,
      int expected_key_occurrences,
      int expected_paste_occurrences) {
    PhishySiteInteractionMap new_map;
    if (expected_click_occurrences > 0) {
      new_map.insert_or_assign(
          ClientSafeBrowsingReportRequest::PhishySiteInteraction::
              PHISHY_CLICK_EVENT,
          PhishyPageInteractionDetails(
              expected_click_occurrences,
              base::Time::Now().InMillisecondsSinceUnixEpoch(),
              base::Time::Now().InMillisecondsSinceUnixEpoch()));
    }
    if (expected_key_occurrences > 0) {
      new_map.insert_or_assign(
          ClientSafeBrowsingReportRequest::PhishySiteInteraction::
              PHISHY_KEY_EVENT,
          PhishyPageInteractionDetails(
              expected_key_occurrences,
              base::Time::Now().InMillisecondsSinceUnixEpoch(),
              base::Time::Now().InMillisecondsSinceUnixEpoch()));
    }
    if (expected_paste_occurrences > 0) {
      new_map.insert_or_assign(
          ClientSafeBrowsingReportRequest::PhishySiteInteraction::
              PHISHY_PASTE_EVENT,
          PhishyPageInteractionDetails(
              expected_paste_occurrences,
              base::Time::Now().InMillisecondsSinceUnixEpoch(),
              base::Time::Now().InMillisecondsSinceUnixEpoch()));
    }
    return new_map;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(SafeBrowsingServiceTestWithAntiPhishingTelemetryEnabled,
       SendPhishyInteractionsReport_Success) {
  const int kExpectedClickEventCount = 5;
  const int kExpectedKeyEventCount = 2;
  const int kExpectedPasteEventCount = 0;
  SetExtendedReportingPrefForTests(profile_->GetPrefs(), true);
  GURL test_url("http://phishing.com");
  GURL test_page_url("http://page_url.com");
  PhishySiteInteractionMap test_map = SetUpPhishyInteractionMap(
      kExpectedClickEventCount, kExpectedKeyEventCount,
      kExpectedPasteEventCount);

  auto* ping_manager =
      ChromePingManagerFactory::GetForBrowserContext(profile());
  network::TestURLLoaderFactory test_url_loader_factory;
  test_url_loader_factory.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        std::unique_ptr<ClientSafeBrowsingReportRequest> actual_request =
            GetActualRequest(request);
        VerifyPhishySiteReportRequest(
            actual_request.get(), test_url, 2, kExpectedClickEventCount,
            kExpectedKeyEventCount, kExpectedPasteEventCount);
      }));
  ping_manager->SetURLLoaderFactoryForTesting(
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory));

  EXPECT_TRUE(sb_service_->SendPhishyInteractionsReport(
      profile(), test_url, test_page_url, test_map));
}

}  // namespace safe_browsing
