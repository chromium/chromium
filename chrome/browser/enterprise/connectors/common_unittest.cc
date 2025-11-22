// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/common.h"

#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/strings/to_string.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/mock_download_item.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/common.h"
#include "components/enterprise/connectors/core/features.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/fake_download_item.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/range/range.h"

namespace enterprise_connectors {

namespace {

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
struct CustomMessageTestCase {
  TriggeredRule::Action action;
  std::string message;
};

constexpr char kDmToken[] = "dm_token";
constexpr char kTestUrl[] = "http://example.com/";
constexpr char kTestInvalidUrl[] = "example.com";
constexpr char kTestMessage[] = "test";
constexpr char16_t kU16TestMessage[] = u"test";
constexpr char kTestMessage2[] = "test2";
constexpr char kGoogleServiceProvider[] = R"(
{
  "service_provider": "google",
  "enable": [
    {
      "url_list": ["*"],
      "tags": ["dlp"]
    }
  ],
  "block_large_files": 1
})";
constexpr char kTestEscapedHtmlMessage[] = "&lt;&gt;&amp;&quot;&#39;";
constexpr char16_t kTestUnescapedHtmlMessage[] = u"<>&\"'";
// Offset to first placeholder index for
// IDS_DEEP_SCANNING_DIALOG_CUSTOM_MESSAGE.
constexpr size_t kRuleMessageOffset = 26;
constexpr char kTestLinkedMessage[] = "Learn More";
constexpr char16_t kU16TestLinkedMessage[] = u"Learn More";

ContentAnalysisResponse CreateContentAnalysisResponse(
    const std::vector<CustomMessageTestCase>& triggered_rules,
    const std::string& url) {
  ContentAnalysisResponse response;
  auto* result = response.add_results();
  result->set_tag("dlp");
  result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);

  for (const auto& triggered_rule : triggered_rules) {
    auto* rule = result->add_triggered_rules();
    rule->set_action(triggered_rule.action);
    if (!triggered_rule.message.empty()) {
      ContentAnalysisResponse::Result::TriggeredRule::CustomRuleMessage
          custom_message;
      auto* custom_segment = custom_message.add_message_segments();
      custom_segment->set_text(triggered_rule.message);
      auto* custom_linked_segment = custom_message.add_message_segments();
      custom_linked_segment->set_text(kTestLinkedMessage);
      custom_linked_segment->set_link(url);
      *rule->mutable_custom_rule_message() = custom_message;
    }
  }
  return response;
}

class BaseTest : public testing::Test {
 public:
  BaseTest() : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    EXPECT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("test-user");
  }

  void EnableFeatures() { scoped_feature_list_.Reset(); }

  Profile* profile() { return profile_; }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_;
};
#endif  // BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)

}  // namespace

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
class EnterpriseConnectorsResultShouldAllowDataUseTest
    : public BaseTest,
      public testing::WithParamInterface<bool> {
 public:
  EnterpriseConnectorsResultShouldAllowDataUseTest() = default;

  void SetUp() override {
    BaseTest::SetUp();
    EnableFeatures();

    // Settings can't be returned if no DM token exists.
    SetDMTokenForTesting(policy::DMToken::CreateValidToken(kDmToken));
  }

  bool allowed() const { return !GetParam(); }
  std::string bool_setting() const { return base::ToString(GetParam()); }
  const char* default_action_setting() const {
    return GetParam() ? "block" : "allow";
  }

  AnalysisSettings settings() {
    std::optional<AnalysisSettings> settings =
        ConnectorsServiceFactory::GetForBrowserContext(profile())
            ->GetAnalysisSettings(GURL(kTestUrl), FILE_ATTACHED);
    EXPECT_TRUE(settings.has_value());
    return std::move(settings.value());
  }
};

INSTANTIATE_TEST_SUITE_P(,
                         EnterpriseConnectorsResultShouldAllowDataUseTest,
                         testing::Bool());

TEST_P(EnterpriseConnectorsResultShouldAllowDataUseTest, BlockLargeFile) {
  auto pref = base::StringPrintf(R"(
    {
      "service_provider": "google",
      "enable": [{"url_list": ["*"], "tags": ["dlp"]}],
      "block_large_files": %s
    })",
                                 bool_setting());
  test::SetAnalysisConnector(profile()->GetPrefs(), FILE_ATTACHED, pref);
  EXPECT_EQ(allowed(),
            ResultShouldAllowDataUse(settings(),
                                     ScanRequestUploadResult::FILE_TOO_LARGE));
}

TEST_P(EnterpriseConnectorsResultShouldAllowDataUseTest,
       BlockPasswordProtected) {
  auto pref = base::StringPrintf(R"(
    {
      "service_provider": "google",
      "enable": [{"url_list": ["*"], "tags": ["dlp"]}],
      "block_password_protected": %s
    })",
                                 bool_setting());
  test::SetAnalysisConnector(profile()->GetPrefs(), FILE_ATTACHED, pref);
  EXPECT_EQ(allowed(),
            ResultShouldAllowDataUse(settings(),
                                     ScanRequestUploadResult::FILE_ENCRYPTED));
}

TEST_P(EnterpriseConnectorsResultShouldAllowDataUseTest, BlockUploadFailure) {
  auto pref = base::StringPrintf(R"(
    {
      "service_provider": "google",
      "enable": [{"url_list": ["*"], "tags": ["dlp"]}],
      "default_action": "%s"
    })",
                                 default_action_setting());

  test::SetAnalysisConnector(profile()->GetPrefs(), FILE_ATTACHED, pref);
  EXPECT_EQ(allowed(),
            ResultShouldAllowDataUse(settings(),
                                     ScanRequestUploadResult::UPLOAD_FAILURE));
}

class ContentAnalysisResponseCustomMessageTest
    : public BaseTest,
      public testing::WithParamInterface<
          std::tuple<std::vector<CustomMessageTestCase>, std::u16string>> {
 public:
  ContentAnalysisResponseCustomMessageTest() = default;

  void SetUp() override {
    BaseTest::SetUp();
    EnableFeatures();

    // Settings can't be returned if no DM token exists.
    SetDMTokenForTesting(policy::DMToken::CreateValidToken(kDmToken));
  }

  std::vector<CustomMessageTestCase> triggered_rules() const {
    return std::get<0>(GetParam());
  }
  std::u16string expected_message() const { return std::get<1>(GetParam()); }

  AnalysisSettings settings() {
    std::optional<AnalysisSettings> settings =
        ConnectorsServiceFactory::GetForBrowserContext(profile())
            ->GetAnalysisSettings(GURL(kTestUrl), FILE_ATTACHED);
    EXPECT_TRUE(settings.has_value());
    return std::move(settings.value());
  }
};

TEST_P(ContentAnalysisResponseCustomMessageTest, ValidUrlCustomMessage) {
  test::SetAnalysisConnector(profile()->GetPrefs(), FILE_ATTACHED,
                             kGoogleServiceProvider);
  ContentAnalysisResponse response =
      CreateContentAnalysisResponse(triggered_rules(), kTestUrl);
  RequestHandlerResult result = CalculateRequestHandlerResult(
      settings(), ScanRequestUploadResult::SUCCESS, response);
  std::u16string custom_message =
      GetCustomRuleString(result.custom_rule_message);
  std::vector<std::pair<gfx::Range, GURL>> custom_ranges =
      GetCustomRuleStyles(result.custom_rule_message, kRuleMessageOffset);

  EXPECT_EQ(custom_message,
            custom_message.empty()
                ? std::u16string{}
                : base::StrCat({expected_message(), kU16TestLinkedMessage}));

  if (custom_message.empty()) {
    EXPECT_TRUE(custom_ranges.empty());
  } else {
    EXPECT_EQ(1u, custom_ranges.size());
    EXPECT_EQ(strlen(kTestLinkedMessage),
              custom_ranges.begin()->first.length());
    EXPECT_EQ(custom_ranges.begin()->first.start(),
              expected_message().length() + kRuleMessageOffset);
  }
}

TEST_P(ContentAnalysisResponseCustomMessageTest, InvalidUrlCustomMessage) {
  test::SetAnalysisConnector(profile()->GetPrefs(), FILE_ATTACHED,
                             kGoogleServiceProvider);
  ContentAnalysisResponse response =
      CreateContentAnalysisResponse(triggered_rules(), kTestInvalidUrl);
  RequestHandlerResult result = CalculateRequestHandlerResult(
      settings(), ScanRequestUploadResult::SUCCESS, response);
  std::u16string custom_message =
      GetCustomRuleString(result.custom_rule_message);
  std::vector<std::pair<gfx::Range, GURL>> custom_ranges =
      GetCustomRuleStyles(result.custom_rule_message, kRuleMessageOffset);

  EXPECT_EQ(custom_message,
            custom_message.empty()
                ? std::u16string{}
                : base::StrCat({expected_message(), kU16TestLinkedMessage}));
  EXPECT_TRUE(custom_ranges.empty());
}

TEST_P(ContentAnalysisResponseCustomMessageTest, DownloadsItemCustomMessage) {
  ContentAnalysisResponse response =
      CreateContentAnalysisResponse(triggered_rules(), kTestUrl);
  download::DownloadDangerType danger_type;
  TriggeredRule::Action action = GetHighestPrecedenceAction(response, nullptr);
  if (action == TriggeredRule::WARN) {
    danger_type = download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING;
  } else {
    danger_type = download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK;
  }

  // Create download item
  testing::NiceMock<download::MockDownloadItem> item;
  enterprise_connectors::FileMetadata file_metadata(
      "examplename", "12345678", "fake/mimetype", 1234, response);
  auto scan_result = std::make_unique<enterprise_connectors::ScanResult>(
      std::move(file_metadata));
  item.SetUserData(enterprise_connectors::ScanResult::kKey,
                   std::move(scan_result));

  auto custom_rule_message = GetDownloadsCustomRuleMessage(&item, danger_type);
  if (custom_rule_message.has_value()) {
    EXPECT_EQ(GetCustomRuleString(custom_rule_message.value()),
              base::StrCat({expected_message(), kU16TestLinkedMessage}));
  } else {
    EXPECT_EQ(std::u16string{}, expected_message());
  }
}

INSTANTIATE_TEST_SUITE_P(
    ,
    ContentAnalysisResponseCustomMessageTest,
    testing::Values(
        std::make_tuple(std::vector<CustomMessageTestCase>(),
                        /*expected_message=*/std::u16string{}),
        std::make_tuple(
            std::vector<CustomMessageTestCase>{
                {.action = TriggeredRule::WARN, .message = ""}},
            /*expected_message=*/std::u16string{}),
        std::make_tuple(
            std::vector<CustomMessageTestCase>{
                {.action = TriggeredRule::WARN, .message = kTestMessage}},
            /*expected_message=*/kU16TestMessage),
        std::make_tuple(
            std::vector<CustomMessageTestCase>{
                {.action = TriggeredRule::BLOCK, .message = ""},
                {.action = TriggeredRule::WARN, .message = kTestMessage}},
            /*expected_message=*/std::u16string{}),
        std::make_tuple(
            std::vector<CustomMessageTestCase>{
                {.action = TriggeredRule::BLOCK, .message = ""},
                {.action = TriggeredRule::BLOCK, .message = kTestMessage}},
            /*expected_message=*/kU16TestMessage),
        std::make_tuple(
            std::vector<CustomMessageTestCase>{
                {.action = TriggeredRule::BLOCK, .message = kTestMessage},
                {.action = TriggeredRule::WARN, .message = kTestMessage2}},
            /*expected_message=*/kU16TestMessage),
        std::make_tuple(
            std::vector<CustomMessageTestCase>{
                {.action = TriggeredRule::BLOCK,
                 .message = kTestEscapedHtmlMessage}},
            /*expected_message=*/kTestUnescapedHtmlMessage)));

class CollectFrameUrlsTest : public BaseTest {
 public:
  CollectFrameUrlsTest() = default;

 protected:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(kEnterpriseIframeDlpRulesSupport);
    BaseTest::SetUp();

    // Create test web contents as we need to navigate to a URL to get a valid
    // frame chain.
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        profile(), content::SiteInstance::Create(profile()));
    content::WebContentsTester::For(web_contents_.get())
        ->NavigateAndCommit(GURL(kTestUrl));
  }

  void TearDown() override {
    // `WebContentsTester` needs to be destroyed before the
    // `RenderViewHostTestEnabler`.
    if (web_contents_) {
      web_contents_.reset();
    }
    BaseTest::TearDown();
  }

  content::WebContents* web_contents() { return web_contents_.get(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<content::WebContents> web_contents_;
  // Needed for frame tree and navigation operations.
  content::RenderViewHostTestEnabler rvh_test_enabler_;
};

TEST_F(CollectFrameUrlsTest, NestedFramesWithUninterestingUrl) {
  base::HistogramTester histogram_tester;

  // Create test URLs.
  const GURL child_frame_url1("http://child1.example.com/");
  const GURL child_frame_url2("http://child2.example.com/");
  const GURL uninteresting_url("about:blank");

  // Create main frame.
  content::RenderFrameHost* main_frame = web_contents()->GetPrimaryMainFrame();
  content::RenderFrameHostTester* main_frame_tester =
      content::RenderFrameHostTester::For(main_frame);

  // Create and navigate the first child frame.
  content::RenderFrameHost* child_frame1 =
      main_frame_tester->AppendChild("child1");
  content::NavigationSimulator::NavigateAndCommitFromDocument(child_frame_url1,
                                                              child_frame1);

  // Create and navigate the second (nested) child frame.
  content::RenderFrameHostTester* child_frame1_tester =
      content::RenderFrameHostTester::For(child_frame1);
  content::RenderFrameHost* child_frame2 =
      child_frame1_tester->AppendChild("child2");
  content::NavigationSimulator::NavigateAndCommitFromDocument(child_frame_url2,
                                                              child_frame2);

  // Create and navigate the third (nested) child frame with uninteresting URL.
  content::RenderFrameHostTester* child_frame2_tester =
      content::RenderFrameHostTester::For(child_frame2);
  content::RenderFrameHost* child_frame3 =
      child_frame2_tester->AppendChild("child3");
  content::NavigationSimulator::NavigateAndCommitFromDocument(uninteresting_url,
                                                              child_frame3);

  // Set focus on the innermost frame.
  content::FocusWebContentsOnFrame(web_contents(), child_frame3);

  google::protobuf::RepeatedPtrField<std::string> frame_urls =
      CollectFrameUrls(web_contents(), DeepScanAccessPoint::DOWNLOAD);

  // Verify that the URL chain is listed in the right order and chain size
  // histogram is recorded properly. The uninteresting URL and the tab URL
  // should not be included in the chain.
  ASSERT_EQ(2, frame_urls.size());
  EXPECT_EQ(child_frame_url2.spec(), frame_urls[0]);
  EXPECT_EQ(child_frame_url1.spec(), frame_urls[1]);

  // We still include the tab URL in the histogram chain size.
  histogram_tester.ExpectTotalCount(
      "Enterprise.IframeDlpRulesSupport.Download.UrlChainSize", 1);
  histogram_tester.ExpectBucketCount(
      "Enterprise.IframeDlpRulesSupport.Download.UrlChainSize", 3, 1);
}

TEST_F(CollectFrameUrlsTest, TabUrlOnly) {
  base::HistogramTester histogram_tester;

  google::protobuf::RepeatedPtrField<std::string> frame_urls =
      CollectFrameUrls(web_contents(), DeepScanAccessPoint::DOWNLOAD);

  // The URL chain should be empty since there are no iframes.
  EXPECT_EQ(0, frame_urls.size());

  // The histogram should have a value of 1 to account for the tab's URL.
  histogram_tester.ExpectTotalCount(
      "Enterprise.IframeDlpRulesSupport.Download.UrlChainSize", 1);
  histogram_tester.ExpectBucketCount(
      "Enterprise.IframeDlpRulesSupport.Download.UrlChainSize", 1, 1);
}

TEST_F(CollectFrameUrlsTest, NoWebContents) {
  base::HistogramTester histogram_tester;

  google::protobuf::RepeatedPtrField<std::string> frame_urls =
      CollectFrameUrls(nullptr, DeepScanAccessPoint::DOWNLOAD);

  // Since there are no tabs, there should not be any URLs recorded.
  EXPECT_EQ(0, frame_urls.size());

  histogram_tester.ExpectTotalCount(
      "Enterprise.IframeDlpRulesSupport.Download.UrlChainSize", 1);
  histogram_tester.ExpectBucketCount(
      "Enterprise.IframeDlpRulesSupport.Download.UrlChainSize", 0, 1);
}
#endif  // BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)

}  // namespace enterprise_connectors
