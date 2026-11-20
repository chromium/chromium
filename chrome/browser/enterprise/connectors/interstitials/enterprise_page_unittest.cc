// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/enterprise/connectors/interstitials/enterprise_block_controller_client.h"
#include "chrome/browser/enterprise/connectors/interstitials/enterprise_block_page.h"
#include "chrome/browser/enterprise/connectors/interstitials/enterprise_warn_controller_client.h"
#include "chrome/browser/enterprise/connectors/interstitials/enterprise_warn_page.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/safe_browsing/content/browser/interstitials/base_ui_manager.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/security_interstitials/core/controller_client.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "components/security_interstitials/core/unsafe_resource.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"

namespace {

class TestEnterpriseBlockPage : public EnterpriseBlockPage {
 public:
  using EnterpriseBlockPage::CommandReceived;
  using EnterpriseBlockPage::EnterpriseBlockPage;
  using EnterpriseBlockPage::OnInterstitialClosing;
};

class TestEnterpriseWarnPage : public EnterpriseWarnPage {
 public:
  using EnterpriseWarnPage::CommandReceived;
  using EnterpriseWarnPage::EnterpriseWarnPage;
  using EnterpriseWarnPage::OnInterstitialClosing;
};

class TestEnterpriseBlockControllerClient
    : public EnterpriseBlockControllerClient {
 public:
  using EnterpriseBlockControllerClient::EnterpriseBlockControllerClient;
  void GoBack() override {}
};

class TestEnterpriseWarnControllerClient
    : public EnterpriseWarnControllerClient {
 public:
  using EnterpriseWarnControllerClient::EnterpriseWarnControllerClient;
  void GoBack() override {}
  void Proceed() override {}
};

constexpr char kBlockDecisionHistogram[] =
    "interstitial.enterprise_block.decision";
constexpr char kBlockRepeatVisitDecisionHistogram[] =
    "interstitial.enterprise_block.decision.repeat_visit";
constexpr char kWarnDecisionHistogram[] =
    "interstitial.enterprise_warn.decision";
constexpr char kWarnRepeatVisitDecisionHistogram[] =
    "interstitial.enterprise_warn.decision.repeat_visit";
constexpr char kTestUrl[] = "http://example.com";
constexpr char kTestBlockMessage[] = "Test block message";
constexpr char kTestWarnMessage[] = "Test warn message";

void AddCustomMessageToResource(
    security_interstitials::UnsafeResource& unsafe_resource,
    const std::string& message,
    const std::string& url,
    safe_browsing::RTLookupResponse::ThreatInfo::VerdictType verdict_type) {
  safe_browsing::MatchedUrlNavigationRule_CustomMessage cm;
  auto* custom_segments = cm.add_message_segments();
  custom_segments->set_text(message);
  custom_segments->set_link(url);

  auto* threat_info = unsafe_resource.rt_lookup_response.add_threat_info();
  threat_info->set_verdict_type(verdict_type);
  *threat_info->mutable_matched_url_navigation_rule()
       ->mutable_custom_message() = cm;
}

class EnterprisePageTest : public testing::Test {
 public:
  EnterprisePageTest() : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    EXPECT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile(
        "test-user", {TestingProfile::TestingFactory{
                         HistoryServiceFactory::GetInstance(),
                         HistoryServiceFactory::GetDefaultFactory()}});
  }

  content::WebContents* web_contents() {
    if (!web_contents_) {
      content::WebContents::CreateParams params(profile_);
      web_contents_ = content::WebContents::Create(params);
    }
    return web_contents_.get();
  }

  history::HistoryService* history_service() {
    return HistoryServiceFactory::GetForProfile(
        profile_, ServiceAccessType::EXPLICIT_ACCESS);
  }

  // Waits until the history backend has handled all pending requests and their
  // replies (e.g. the visit count lookup started by `MetricsHelper`) have run.
  void WaitForHistory() {
    history::BlockUntilHistoryProcessesPendingRequests(history_service());
    task_environment_.RunUntilIdle();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_;
  std::unique_ptr<content::WebContents> web_contents_;
};

TEST_F(EnterprisePageTest, EnterpriseBlock_ShownAndMetricsRecorded) {
  base::HistogramTester histograms;
  auto unsafe_resources =
      safe_browsing::SafeBrowsingBlockingPage::UnsafeResourceList();

  histograms.ExpectTotalCount(kBlockDecisionHistogram, 0);

  EnterpriseBlockPage test_page = EnterpriseBlockPage(
      nullptr, web_contents(), GURL("exampleurl.net"), unsafe_resources,
      std::make_unique<EnterpriseBlockControllerClient>(
          web_contents(), GURL("exampleurl.net")));

  // Total count = pages shown + proceeding disabled on the page that was shown
  histograms.ExpectTotalCount(kBlockDecisionHistogram, 2);
  histograms.ExpectBucketCount(kBlockDecisionHistogram,
                               security_interstitials::MetricsHelper::SHOW, 1);
  histograms.ExpectBucketCount(
      kBlockDecisionHistogram,
      security_interstitials::MetricsHelper::PROCEEDING_DISABLED, 1);
}

TEST_F(EnterprisePageTest, EnterpriseWarn_ShownAndMetricsRecorded) {
  base::HistogramTester histograms;
  auto unsafe_resources =
      safe_browsing::SafeBrowsingBlockingPage::UnsafeResourceList();

  histograms.ExpectTotalCount(kWarnDecisionHistogram, 0);

  EnterpriseWarnPage test_page = EnterpriseWarnPage(
      nullptr, web_contents(), GURL("exampleurl.net"), unsafe_resources,
      std::make_unique<EnterpriseWarnControllerClient>(web_contents(),
                                                       GURL("exampleurl.net")));

  histograms.ExpectTotalCount(kWarnDecisionHistogram, 1);
  histograms.ExpectBucketCount(kWarnDecisionHistogram,
                               security_interstitials::MetricsHelper::SHOW, 1);
}

TEST_F(EnterprisePageTest, EnterpriseWarn_CustomMessageDisplayed) {
  auto unsafe_resources =
      safe_browsing::SafeBrowsingBlockingPage::UnsafeResourceList();
  security_interstitials::UnsafeResource resource;
  AddCustomMessageToResource(resource, kTestWarnMessage, kTestUrl,
                             safe_browsing::RTLookupResponse::ThreatInfo::WARN);
  unsafe_resources.emplace_back(resource);

  EnterpriseWarnPage test_page = EnterpriseWarnPage(
      nullptr, web_contents(), GURL("exampleurl.net"), unsafe_resources,
      std::make_unique<EnterpriseWarnControllerClient>(web_contents(),
                                                       GURL("exampleurl.net")));

  base::DictValue load_time_data;
  std::string final_message = test_page.GetCustomMessageForTesting();
  std::string expected_message = base::StrCat(
      {"Your organization says: ", "\"<a target=\"_blank\" href=\"", kTestUrl,
       "\">", kTestWarnMessage, "</a>\""});
  EXPECT_EQ(expected_message, final_message);
}

TEST_F(EnterprisePageTest, EnterpriseBlock_CustomMessageDisplayed) {
  auto unsafe_resources =
      safe_browsing::SafeBrowsingBlockingPage::UnsafeResourceList();
  security_interstitials::UnsafeResource resource;
  AddCustomMessageToResource(
      resource, kTestBlockMessage, kTestUrl,
      safe_browsing::RTLookupResponse::ThreatInfo::DANGEROUS);
  unsafe_resources.emplace_back(resource);

  EnterpriseBlockPage test_page = EnterpriseBlockPage(
      nullptr, web_contents(), GURL("exampleurl.net"), unsafe_resources,
      std::make_unique<EnterpriseBlockControllerClient>(
          web_contents(), GURL("exampleurl.net")));

  base::DictValue load_time_data;
  std::string final_message = test_page.GetCustomMessageForTesting();
  std::string expected_message = base::StrCat(
      {"Your organization says: ", "\"<a target=\"_blank\" href=\"", kTestUrl,
       "\">", kTestBlockMessage, "</a>\""});
  EXPECT_EQ(expected_message, final_message);
}

TEST_F(EnterprisePageTest, EnterpriseBlock_CustomMessagePrioritization) {
  auto unsafe_resources =
      safe_browsing::SafeBrowsingBlockingPage::UnsafeResourceList();
  security_interstitials::UnsafeResource resource;

  // Higher severity message should be displayed over lower.
  AddCustomMessageToResource(resource, kTestWarnMessage, kTestUrl,
                             safe_browsing::RTLookupResponse::ThreatInfo::WARN);
  AddCustomMessageToResource(
      resource, kTestBlockMessage, kTestUrl,
      safe_browsing::RTLookupResponse::ThreatInfo::DANGEROUS);
  unsafe_resources.emplace_back(resource);

  EnterpriseBlockPage test_page1(
      nullptr, web_contents(), GURL(kTestUrl), unsafe_resources,
      std::make_unique<EnterpriseBlockControllerClient>(web_contents(),
                                                        GURL(kTestUrl)));

  std::string expected_message = base::StrCat(
      {"Your organization says: ", "\"<a target=\"_blank\" href=\"", kTestUrl,
       "\">", kTestBlockMessage, "</a>\""});
  EXPECT_EQ(expected_message, test_page1.GetCustomMessageForTesting());

  // For equal severity, last non-empty message is shown.
  unsafe_resources.clear();
  resource = security_interstitials::UnsafeResource();
  AddCustomMessageToResource(
      resource, "", kTestUrl,
      safe_browsing::RTLookupResponse::ThreatInfo::DANGEROUS);
  AddCustomMessageToResource(
      resource, kTestBlockMessage, kTestUrl,
      safe_browsing::RTLookupResponse::ThreatInfo::DANGEROUS);
  unsafe_resources.emplace_back(resource);

  EnterpriseBlockPage test_page2(
      nullptr, web_contents(), GURL(kTestUrl), unsafe_resources,
      std::make_unique<EnterpriseBlockControllerClient>(web_contents(),
                                                        GURL(kTestUrl)));

  expected_message = base::StrCat(
      {"Your organization says: ", "\"<a target=\"_blank\" href=\"", kTestUrl,
       "\">", kTestBlockMessage, "</a>\""});
  EXPECT_EQ(expected_message, test_page2.GetCustomMessageForTesting());

  // Empty message with higher severity overrides lower severity with message.
  unsafe_resources.clear();
  resource = security_interstitials::UnsafeResource();

  AddCustomMessageToResource(resource, kTestWarnMessage, kTestUrl,
                             safe_browsing::RTLookupResponse::ThreatInfo::WARN);
  AddCustomMessageToResource(
      resource, "", kTestUrl,
      safe_browsing::RTLookupResponse::ThreatInfo::DANGEROUS);
  unsafe_resources.emplace_back(resource);

  EnterpriseBlockPage test_page3(
      nullptr, web_contents(), GURL(kTestUrl), unsafe_resources,
      std::make_unique<EnterpriseBlockControllerClient>(web_contents(),
                                                        GURL(kTestUrl)));

  expected_message = base::StrCat(
      {"Your organization says: ", "\"<a target=\"_blank\" href=\"", kTestUrl,
       "\">", "", "</a>\""});
  EXPECT_EQ(expected_message, test_page3.GetCustomMessageForTesting());
}

TEST_F(
    EnterprisePageTest,
    EnterpriseBlock_ClearsPendingAllowlistOnInterstitialClosingAndDestruction) {
  auto ui_manager = base::MakeRefCounted<safe_browsing::BaseUIManager>();
  const GURL url("https://exampleurl.net");

  security_interstitials::UnsafeResource resource;
  resource.url = url;
  resource.navigation_id = 1;
  resource.threat_type =
      safe_browsing::SBThreatType::SB_THREAT_TYPE_MANAGED_POLICY_BLOCK;
  ui_manager->AddToAllowlistUrlSet(url, resource.navigation_id, web_contents(),
                                   /*is_pending=*/true, resource.threat_type);

  safe_browsing::SBThreatType threat_type =
      safe_browsing::SBThreatType::SB_THREAT_TYPE_SAFE;
  ASSERT_TRUE(ui_manager->IsUrlAllowlistedOrPendingForWebContents(
      url, /*entry=*/nullptr, web_contents(), /*allowlist_only=*/false,
      &threat_type));
  EXPECT_EQ(threat_type,
            safe_browsing::SBThreatType::SB_THREAT_TYPE_MANAGED_POLICY_BLOCK);

  {
    TestEnterpriseBlockPage test_page(
        ui_manager.get(), web_contents(), url, {resource},
        std::make_unique<EnterpriseBlockControllerClient>(web_contents(), url));
    test_page.OnInterstitialClosing();
    EXPECT_FALSE(ui_manager->IsUrlAllowlistedOrPendingForWebContents(
        url, /*entry=*/nullptr, web_contents(), /*allowlist_only=*/false,
        &threat_type));
  }

  // Also verify destructor fallback when destroyed before committing
  // (without `OnInterstitialClosing()` being called).
  resource.navigation_id = 2;
  ui_manager->AddToAllowlistUrlSet(url, resource.navigation_id, web_contents(),
                                   /*is_pending=*/true, resource.threat_type);
  ASSERT_TRUE(ui_manager->IsUrlAllowlistedOrPendingForWebContents(
      url, /*entry=*/nullptr, web_contents(), /*allowlist_only=*/false,
      &threat_type));
  {
    TestEnterpriseBlockPage test_page(
        ui_manager.get(), web_contents(), url, {resource},
        std::make_unique<EnterpriseBlockControllerClient>(web_contents(), url));
  }
  EXPECT_FALSE(ui_manager->IsUrlAllowlistedOrPendingForWebContents(
      url, /*entry=*/nullptr, web_contents(), /*allowlist_only=*/false,
      &threat_type));
}

TEST_F(EnterprisePageTest,
       EnterpriseWarn_ClearsPendingAllowlistUnlessProceeded) {
  auto ui_manager = base::MakeRefCounted<safe_browsing::BaseUIManager>();
  const GURL url("https://exampleurl.net");

  security_interstitials::UnsafeResource resource;
  resource.url = url;
  resource.navigation_id = 1;
  resource.threat_type =
      safe_browsing::SBThreatType::SB_THREAT_TYPE_MANAGED_POLICY_WARN;
  ui_manager->AddToAllowlistUrlSet(url, resource.navigation_id, web_contents(),
                                   /*is_pending=*/true, resource.threat_type);

  safe_browsing::SBThreatType threat_type =
      safe_browsing::SBThreatType::SB_THREAT_TYPE_SAFE;
  ASSERT_TRUE(ui_manager->IsUrlAllowlistedOrPendingForWebContents(
      url, /*entry=*/nullptr, web_contents(), /*allowlist_only=*/false,
      &threat_type));

  // Closing without proceeding removes the pending entry.
  {
    TestEnterpriseWarnPage test_page(
        ui_manager.get(), web_contents(), url, {resource},
        std::make_unique<TestEnterpriseWarnControllerClient>(web_contents(),
                                                             url));
    test_page.OnInterstitialClosing();
    EXPECT_FALSE(ui_manager->IsUrlAllowlistedOrPendingForWebContents(
        url, /*entry=*/nullptr, web_contents(), /*allowlist_only=*/false,
        &threat_type));
  }

  // Proceeding promotes the entry to the permanent allowlist, and subsequent
  // closing/destruction keeps it allowlisted.
  resource.navigation_id = 2;
  ui_manager->AddToAllowlistUrlSet(url, resource.navigation_id, web_contents(),
                                   /*is_pending=*/true, resource.threat_type);
  {
    TestEnterpriseWarnPage test_page(
        ui_manager.get(), web_contents(), url, {resource},
        std::make_unique<TestEnterpriseWarnControllerClient>(web_contents(),
                                                             url));
    test_page.CommandReceived(
        base::NumberToString(security_interstitials::CMD_PROCEED));
    test_page.OnInterstitialClosing();
  }
  EXPECT_TRUE(ui_manager->IsUrlAllowlistedOrPendingForWebContents(
      url, /*entry=*/nullptr, web_contents(), /*allowlist_only=*/true,
      &threat_type));
  EXPECT_EQ(threat_type,
            safe_browsing::SBThreatType::SB_THREAT_TYPE_MANAGED_POLICY_WARN);
}

TEST_F(EnterprisePageTest, EnterpriseWarn_DontProceedRecordedOnceOnClose) {
  base::HistogramTester histograms;
  const GURL url("https://exampleurl.net");

  TestEnterpriseWarnPage test_page(
      nullptr, web_contents(), url,
      safe_browsing::SafeBrowsingBlockingPage::UnsafeResourceList(),
      std::make_unique<TestEnterpriseWarnControllerClient>(web_contents(),
                                                           url));

  histograms.ExpectTotalCount(kWarnDecisionHistogram, 1);
  histograms.ExpectBucketCount(kWarnDecisionHistogram,
                               security_interstitials::MetricsHelper::SHOW, 1);

  test_page.OnInterstitialClosing();
  test_page.OnInterstitialClosing();

  histograms.ExpectTotalCount(kWarnDecisionHistogram, 2);
  histograms.ExpectBucketCount(kWarnDecisionHistogram,
                               security_interstitials::MetricsHelper::SHOW, 1);
  histograms.ExpectBucketCount(
      kWarnDecisionHistogram,
      security_interstitials::MetricsHelper::DONT_PROCEED, 1);
}

TEST_F(EnterprisePageTest, EnterpriseWarn_DontProceedRecordedOnceAfterGoBack) {
  base::HistogramTester histograms;
  const GURL url("https://exampleurl.net");

  TestEnterpriseWarnPage test_page(
      nullptr, web_contents(), url,
      safe_browsing::SafeBrowsingBlockingPage::UnsafeResourceList(),
      std::make_unique<TestEnterpriseWarnControllerClient>(web_contents(),
                                                           url));

  // Going back navigates away from the interstitial, which then closes it.
  test_page.CommandReceived(
      base::NumberToString(security_interstitials::CMD_DONT_PROCEED));
  test_page.OnInterstitialClosing();

  histograms.ExpectTotalCount(kWarnDecisionHistogram, 2);
  histograms.ExpectBucketCount(kWarnDecisionHistogram,
                               security_interstitials::MetricsHelper::SHOW, 1);
  histograms.ExpectBucketCount(
      kWarnDecisionHistogram,
      security_interstitials::MetricsHelper::DONT_PROCEED, 1);
}

TEST_F(EnterprisePageTest, EnterpriseWarn_DontProceedNotRecordedAfterProceed) {
  base::HistogramTester histograms;
  const GURL url("https://exampleurl.net");

  TestEnterpriseWarnPage test_page(
      nullptr, web_contents(), url,
      safe_browsing::SafeBrowsingBlockingPage::UnsafeResourceList(),
      std::make_unique<TestEnterpriseWarnControllerClient>(web_contents(),
                                                           url));

  test_page.CommandReceived(
      base::NumberToString(security_interstitials::CMD_PROCEED));
  test_page.OnInterstitialClosing();

  histograms.ExpectTotalCount(kWarnDecisionHistogram, 2);
  histograms.ExpectBucketCount(kWarnDecisionHistogram,
                               security_interstitials::MetricsHelper::SHOW, 1);
  histograms.ExpectBucketCount(kWarnDecisionHistogram,
                               security_interstitials::MetricsHelper::PROCEED,
                               1);
  histograms.ExpectBucketCount(
      kWarnDecisionHistogram,
      security_interstitials::MetricsHelper::DONT_PROCEED, 0);
}

TEST_F(EnterprisePageTest, EnterpriseWarn_RepeatVisitRecordedOnDontProceed) {
  base::HistogramTester histograms;
  const GURL url("https://exampleurl.net");

  history_service()->AddPage(url, base::Time::Now() - base::Days(1),
                             history::SOURCE_BROWSED);
  WaitForHistory();

  TestEnterpriseWarnPage test_page(
      nullptr, web_contents(), url,
      safe_browsing::SafeBrowsingBlockingPage::UnsafeResourceList(),
      std::make_unique<TestEnterpriseWarnControllerClient>(web_contents(),
                                                           url));
  WaitForHistory();

  test_page.OnInterstitialClosing();

  histograms.ExpectTotalCount(kWarnRepeatVisitDecisionHistogram, 2);
  histograms.ExpectBucketCount(kWarnRepeatVisitDecisionHistogram,
                               security_interstitials::MetricsHelper::SHOW, 1);
  histograms.ExpectBucketCount(
      kWarnRepeatVisitDecisionHistogram,
      security_interstitials::MetricsHelper::DONT_PROCEED, 1);
}

TEST_F(EnterprisePageTest, EnterpriseWarn_RepeatVisitRecordedOnProceed) {
  base::HistogramTester histograms;
  const GURL url("https://exampleurl.net");

  history_service()->AddPage(url, base::Time::Now() - base::Days(1),
                             history::SOURCE_BROWSED);
  WaitForHistory();

  TestEnterpriseWarnPage test_page(
      nullptr, web_contents(), url,
      safe_browsing::SafeBrowsingBlockingPage::UnsafeResourceList(),
      std::make_unique<TestEnterpriseWarnControllerClient>(web_contents(),
                                                           url));
  WaitForHistory();

  test_page.CommandReceived(
      base::NumberToString(security_interstitials::CMD_PROCEED));
  test_page.OnInterstitialClosing();

  histograms.ExpectTotalCount(kWarnRepeatVisitDecisionHistogram, 2);
  histograms.ExpectBucketCount(kWarnRepeatVisitDecisionHistogram,
                               security_interstitials::MetricsHelper::SHOW, 1);
  histograms.ExpectBucketCount(kWarnRepeatVisitDecisionHistogram,
                               security_interstitials::MetricsHelper::PROCEED,
                               1);
}

TEST_F(EnterprisePageTest, EnterpriseWarn_RepeatVisitNotRecordedOnFirstVisit) {
  base::HistogramTester histograms;
  const GURL url("https://exampleurl.net");

  TestEnterpriseWarnPage test_page(
      nullptr, web_contents(), url,
      safe_browsing::SafeBrowsingBlockingPage::UnsafeResourceList(),
      std::make_unique<TestEnterpriseWarnControllerClient>(web_contents(),
                                                           url));
  WaitForHistory();

  test_page.OnInterstitialClosing();

  histograms.ExpectBucketCount(
      kWarnDecisionHistogram,
      security_interstitials::MetricsHelper::DONT_PROCEED, 1);
  histograms.ExpectTotalCount(kWarnRepeatVisitDecisionHistogram, 0);
}

TEST_F(EnterprisePageTest, EnterpriseBlock_RepeatVisitRecordedOnDontProceed) {
  base::HistogramTester histograms;
  const GURL url("https://exampleurl.net");

  // Simulate a visit to the host from before it was blocked.
  history_service()->AddPage(url, base::Time::Now() - base::Days(1),
                             history::SOURCE_BROWSED);
  WaitForHistory();

  TestEnterpriseBlockPage test_page(
      nullptr, web_contents(), url,
      safe_browsing::SafeBrowsingBlockingPage::UnsafeResourceList(),
      std::make_unique<TestEnterpriseBlockControllerClient>(web_contents(),
                                                            url));
  WaitForHistory();

  test_page.CommandReceived(
      base::NumberToString(security_interstitials::CMD_DONT_PROCEED));

  // `PROCEEDING_DISABLED` is not a repeat visit decision, so only the paired
  // `SHOW` and `DONT_PROCEED` are recorded.
  histograms.ExpectTotalCount(kBlockRepeatVisitDecisionHistogram, 2);
  histograms.ExpectBucketCount(kBlockRepeatVisitDecisionHistogram,
                               security_interstitials::MetricsHelper::SHOW, 1);
  histograms.ExpectBucketCount(
      kBlockRepeatVisitDecisionHistogram,
      security_interstitials::MetricsHelper::DONT_PROCEED, 1);
}

TEST_F(EnterprisePageTest, EnterpriseBlock_RepeatVisitNotRecordedOnFirstVisit) {
  base::HistogramTester histograms;
  const GURL url("https://exampleurl.net");

  TestEnterpriseBlockPage test_page(
      nullptr, web_contents(), url,
      safe_browsing::SafeBrowsingBlockingPage::UnsafeResourceList(),
      std::make_unique<TestEnterpriseBlockControllerClient>(web_contents(),
                                                            url));
  WaitForHistory();

  test_page.CommandReceived(
      base::NumberToString(security_interstitials::CMD_DONT_PROCEED));

  histograms.ExpectBucketCount(
      kBlockDecisionHistogram,
      security_interstitials::MetricsHelper::DONT_PROCEED, 1);
  histograms.ExpectTotalCount(kBlockRepeatVisitDecisionHistogram, 0);
}

}  // namespace
