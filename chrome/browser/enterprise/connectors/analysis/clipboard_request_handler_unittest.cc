// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/clipboard_request_handler.h"

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_info.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/cloud_binary_upload_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/cloud_binary_upload_service_factory.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/test_binary_upload_service.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/enterprise/connectors/core/analysis_settings.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

namespace {

constexpr char kAnalysisUrl[] = "https://scan.com/";
constexpr char kUrl[] = "https://bar.com/";
constexpr char kSourceUrl[] = "https://baz.com/";
constexpr char kTabTitle[] = "tab_title";
constexpr char kMessage[] = "message";
constexpr char kMethod[] = "METHOD";
constexpr char16_t kJustification[] = u"justification";
constexpr size_t kMaxSize = 1000;

std::string CreateTestData(size_t size) {
  return std::string(size, 'a');
}

ContentAnalysisResponse::Result CreateResult(
    ContentAnalysisResponse::Result::TriggeredRule::Action action) {
  ContentAnalysisResponse::Result result;
  result.set_tag("dlp");
  result.set_status(ContentAnalysisResponse::Result::SUCCESS);

  if (action !=
      ContentAnalysisResponse::Result::TriggeredRule::ACTION_UNSPECIFIED) {
    auto* rule = result.add_triggered_rules();
    rule->set_rule_name("clipboard_rule_name");
    rule->set_action(action);

    auto* message = rule->mutable_custom_rule_message();
    message->add_message_segments()->set_text(kMessage);
  }
  return result;
}

ContentMetaData::CopiedTextSource GetSource() {
  ContentMetaData::CopiedTextSource source;
  source.set_url(kSourceUrl);
  return source;
}

class TestContentAnalysisInfo : public ContentAnalysisInfo {
 public:
  explicit TestContentAnalysisInfo(AnalysisSettings settings)
      : settings_(std::move(settings)) {}

  const AnalysisSettings& settings() const override { return settings_; }

  signin::IdentityManager* identity_manager() const override { return nullptr; }

  int user_action_requests_count() const override { return 1; }

  std::string tab_title() const override { return kTabTitle; }

  std::string user_action_id() const override { return "action_id"; }

  std::string email() const override { return "test@user.com"; }

  std::string url() const override { return kUrl; }

  const GURL& tab_url() const override { return tab_url_; }

  ContentAnalysisRequest::Reason reason() const override {
    return ContentAnalysisRequest::PRINT_PREVIEW_PRINT;
  }

  google::protobuf::RepeatedPtrField<safe_browsing::ReferrerChainEntry>
  referrer_chain() const override {
    return google::protobuf::RepeatedPtrField<
        safe_browsing::ReferrerChainEntry>();
  }

  google::protobuf::RepeatedPtrField<std::string> frame_url_chain()
      const override {
    return {};
  }

 private:
  GURL tab_url_{kUrl};
  AnalysisSettings settings_;
};

class ClipboardRequestHandlerTest : public testing::Test {
 public:
  ClipboardRequestHandlerTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    EXPECT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("test-user-1");
  }

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        safe_browsing::kEnhancedFieldsForSecOps);
    helper_ = std::make_unique<test::EventReportValidatorHelper>(profile_);

    ContentAnalysisResponse response;
    *response.add_results() =
        CreateResult(ContentAnalysisResponse::Result::TriggeredRule::BLOCK);
    binary_upload_service_.SetResponse(
        safe_browsing::CloudBinaryUploadService::Result::SUCCESS,
        std::move(response));
  }

  AnalysisSettings cloud_settings() {
    CloudAnalysisSettings cloud_analysis_settings;
    cloud_analysis_settings.analysis_url = GURL(kAnalysisUrl);
    cloud_analysis_settings.dm_token = "dm_token";
    cloud_analysis_settings.max_file_size = kMaxSize;

    TagSettings tag_settings;
    tag_settings.requires_justification = true;

    AnalysisSettings settings;
    settings.cloud_or_local_settings =
        CloudOrLocalAnalysisSettings(std::move(cloud_analysis_settings));
    settings.tags = {{"dlp", tag_settings}};
    settings.block_until_verdict = BlockUntilVerdict::kBlock;

    return settings;
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_;
  std::unique_ptr<test::EventReportValidatorHelper> helper_;
  safe_browsing::TestBinaryUploadService binary_upload_service_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace

TEST_F(ClipboardRequestHandlerTest, Text) {
  TestContentAnalysisInfo info(cloud_settings());

  auto handler = ClipboardRequestHandler::Create(
      &info, &binary_upload_service_, profile_.get(), GURL(kUrl),
      ClipboardRequestHandler::Type::kText,
      safe_browsing::DeepScanAccessPoint::PASTE, GetSource(), kMethod,
      CreateTestData(kMaxSize), base::BindOnce([](RequestHandlerResult result) {
        EXPECT_EQ(result.final_result, FinalContentAnalysisResult::FAILURE);
        EXPECT_EQ(result.complies, false);
        EXPECT_EQ(result.custom_rule_message.message_segments_size(), 1);
        EXPECT_EQ(result.custom_rule_message.message_segments(0).text(),
                  kMessage);
        EXPECT_EQ(result.tag, "dlp");
      }));

  base::RunLoop run_loop;
  auto validator = helper_->CreateValidator();
  validator.SetDoneClosure(run_loop.QuitClosure());
  validator.ExpectSensitiveDataEvent(
      /*url*/
      kUrl,
      /*tab_url*/ kUrl,
      /*source*/ kSourceUrl,
      /*destination*/ kUrl,
      /*filename*/ "Text data",
      /*sha*/ "",
      /*trigger*/ "WEB_CONTENT_UPLOAD",
      /*dlp_verdict*/
      CreateResult(ContentAnalysisResponse::Result::TriggeredRule::BLOCK),
      /*mimetype*/
      []() {
        static std::set<std::string> set = {"text/plain"};
        return &set;
      }(),
      /*size*/ kMaxSize,
      /*result*/ EventResultToString(EventResult::BLOCKED),
      /*username*/ "test-user@chromium.org",
      /*profile_identifier*/ profile_->GetPath().AsUTF8Unsafe(),
      /*scan_id*/ "",
      /*content_transfer_method*/ kMethod,
      /*user_justification*/ std::nullopt);

  EXPECT_TRUE(handler->UploadData());
  run_loop.Run();

  validator.ExpectSensitiveDataEvent(
      /*url*/
      kUrl,
      /*tab_url*/ kUrl,
      /*source*/ kSourceUrl,
      /*destination*/ kUrl,
      /*filename*/ "Text data",
      /*sha*/ "",
      /*trigger*/ "WEB_CONTENT_UPLOAD",
      /*dlp_verdict*/
      CreateResult(ContentAnalysisResponse::Result::TriggeredRule::BLOCK),
      /*mimetype*/
      []() {
        static std::set<std::string> set = {"text/plain"};
        return &set;
      }(),
      /*size*/ kMaxSize,
      /*result*/ EventResultToString(EventResult::BYPASSED),
      /*username*/ "test-user@chromium.org",
      /*profile_identifier*/ profile_->GetPath().AsUTF8Unsafe(),
      /*scan_id*/ "",
      /*content_transfer_method*/ kMethod,
      /*user_justification*/ kJustification);
  handler->ReportWarningBypass(kJustification);
}

TEST_F(ClipboardRequestHandlerTest, Image) {
  TestContentAnalysisInfo info(cloud_settings());

  auto handler = ClipboardRequestHandler::Create(
      &info, &binary_upload_service_, profile_.get(), GURL(kUrl),
      ClipboardRequestHandler::Type::kImage,
      safe_browsing::DeepScanAccessPoint::DRAG_AND_DROP, GetSource(), kMethod,
      CreateTestData(kMaxSize), base::BindOnce([](RequestHandlerResult result) {
        EXPECT_EQ(result.final_result, FinalContentAnalysisResult::FAILURE);
        EXPECT_EQ(result.complies, false);
        EXPECT_EQ(result.custom_rule_message.message_segments_size(), 1);
        EXPECT_EQ(result.custom_rule_message.message_segments(0).text(),
                  kMessage);
        EXPECT_EQ(result.tag, "dlp");
      }));

  base::RunLoop run_loop;
  auto validator = helper_->CreateValidator();
  validator.SetDoneClosure(run_loop.QuitClosure());
  validator.ExpectSensitiveDataEvent(
      /*url*/
      kUrl,
      /*tab_url*/ kUrl,
      /*source*/ kSourceUrl,
      /*destination*/ kUrl,
      /*filename*/ "Image data",
      /*sha*/ "",
      /*trigger*/ "WEB_CONTENT_UPLOAD",
      /*dlp_verdict*/
      CreateResult(ContentAnalysisResponse::Result::TriggeredRule::BLOCK),
      /*mimetype*/
      []() {
        static std::set<std::string> set = {""};
        return &set;
      }(),
      /*size*/ kMaxSize,
      /*result*/ EventResultToString(EventResult::BLOCKED),
      /*username*/ "test-user@chromium.org",
      /*profile_identifier*/ profile_->GetPath().AsUTF8Unsafe(),
      /*scan_id*/ "",
      /*content_transfer_method*/ kMethod,
      /*user_justification*/ std::nullopt);

  EXPECT_TRUE(handler->UploadData());
  run_loop.Run();

  validator.ExpectSensitiveDataEvent(
      /*url*/
      kUrl,
      /*tab_url*/ kUrl,
      /*source*/ kSourceUrl,
      /*destination*/ kUrl,
      /*filename*/ "Image data",
      /*sha*/ "",
      /*trigger*/ "WEB_CONTENT_UPLOAD",
      /*dlp_verdict*/
      CreateResult(ContentAnalysisResponse::Result::TriggeredRule::BLOCK),
      /*mimetype*/
      []() {
        static std::set<std::string> set = {""};
        return &set;
      }(),
      /*size*/ kMaxSize,
      /*result*/ EventResultToString(EventResult::BYPASSED),
      /*username*/ "test-user@chromium.org",
      /*profile_identifier*/ profile_->GetPath().AsUTF8Unsafe(),
      /*scan_id*/ "",
      /*content_transfer_method*/ kMethod,
      /*user_justification*/ kJustification);
  handler->ReportWarningBypass(kJustification);
}

}  // namespace enterprise_connectors
