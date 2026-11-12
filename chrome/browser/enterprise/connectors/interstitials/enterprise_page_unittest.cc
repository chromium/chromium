// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/enterprise/connectors/interstitials/enterprise_block_controller_client.h"
#include "chrome/browser/enterprise/connectors/interstitials/enterprise_block_page.h"
#include "chrome/browser/enterprise/connectors/interstitials/enterprise_warn_controller_client.h"
#include "chrome/browser/enterprise/connectors/interstitials/enterprise_warn_page.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
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

class TestEnterpriseWarnControllerClient
    : public EnterpriseWarnControllerClient {
 public:
  using EnterpriseWarnControllerClient::EnterpriseWarnControllerClient;
  void Proceed() override {}
};

constexpr char kBlockDecisionHistogram[] =
    "interstitial.enterprise_block.decision";
constexpr char kWarnDecisionHistogram[] =
    "interstitial.enterprise_warn.decision";
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
    profile_ = profile_manager_.CreateTestingProfile("test-user");
  }

  content::WebContents* web_contents() {
    if (!web_contents_) {
      content::WebContents::CreateParams params(profile_);
      web_contents_ = content::WebContents::Create(params);
    }
    return web_contents_.get();
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
}  // namespace
