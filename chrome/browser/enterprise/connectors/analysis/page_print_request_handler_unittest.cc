// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/page_print_request_handler.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_info.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/cloud_binary_upload_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/cloud_binary_upload_service_factory.h"
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
constexpr char kTabUrl[] = "https://baz.com/";
constexpr char kTabTitle[] = "tab_title";
constexpr char kMessage[] = "message";
constexpr char16_t kJustification[] = u"justification";
constexpr size_t kMaxSize = 1000;

base::MappedReadOnlyRegion CreatePageRegion(size_t size) {
  base::MappedReadOnlyRegion page =
      base::ReadOnlySharedMemoryRegion::Create(size);
  std::ranges::fill(base::span(page.mapping), 'a');
  return page;
}

ContentAnalysisResponse::Result CreateResult(
    ContentAnalysisResponse::Result::TriggeredRule::Action action) {
  ContentAnalysisResponse::Result result;
  result.set_tag("dlp");
  result.set_status(ContentAnalysisResponse::Result::SUCCESS);

  if (action !=
      ContentAnalysisResponse::Result::TriggeredRule::ACTION_UNSPECIFIED) {
    auto* rule = result.add_triggered_rules();
    rule->set_rule_name("print_rule_name");
    rule->set_action(action);

    auto* message = rule->mutable_custom_rule_message();
    message->add_message_segments()->set_text(kMessage);
  }
  return result;
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

  google::protobuf::RepeatedPtrField<::safe_browsing::ReferrerChainEntry>
  referrer_chain() const override {
    return google::protobuf::RepeatedPtrField<
        ::safe_browsing::ReferrerChainEntry>();
  }

  google::protobuf::RepeatedPtrField<std::string> frame_url_chain()
      const override {
    return {};
  }

 private:
  GURL tab_url_{kTabUrl};
  AnalysisSettings settings_;
};

class PagePrintRequestHandlerTest : public testing::Test {
 public:
  PagePrintRequestHandlerTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    EXPECT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("test-user-1");
  }

  void SetUp() override {
    helper_ = std::make_unique<test::EventReportValidatorHelper>(profile_);

    ContentAnalysisResponse response;
    *response.add_results() =
        CreateResult(ContentAnalysisResponse::Result::TriggeredRule::BLOCK);
    binary_upload_service_.SetResponse(
        safe_browsing::CloudBinaryUploadService::Result::SUCCESS,
        std::move(response));

    scoped_feature_list_.InitAndEnableFeature(
        safe_browsing::kEnhancedFieldsForSecOps);
  }

  AnalysisSettings cloud_settings() {
    CloudAnalysisSettings cloud_analysis_settings;
    cloud_analysis_settings.analysis_url = GURL(kAnalysisUrl);
    cloud_analysis_settings.dm_token = "dm_token";
    cloud_analysis_settings.max_file_size = kMaxSize;

    AnalysisSettings settings;
    settings.cloud_or_local_settings =
        CloudOrLocalAnalysisSettings(std::move(cloud_analysis_settings));
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

TEST_F(PagePrintRequestHandlerTest, Test) {
  TestContentAnalysisInfo info(cloud_settings());

  auto page = CreatePageRegion(kMaxSize);
  auto handler = PagePrintRequestHandler::Create(
      &info, &binary_upload_service_, profile_.get(), GURL(kUrl),
      "printer_name", "page_content_type", std::move(page.region),
      base::BindOnce([](RequestHandlerResult result) {
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
      /*tab_url*/ kTabUrl,
      /*source*/ "",
      /*destination*/ "printer_name",
      /*filename*/ "tab_title",
      /*sha*/ "",
      /*trigger*/ "PAGE_PRINT",
      /*dlp_verdict*/
      CreateResult(ContentAnalysisResponse::Result::TriggeredRule::BLOCK),
      /*mimetype*/
      []() {
        static std::set<std::string> set = {""};
        return &set;
      }(),
      /*size*/ std::nullopt,
      /*result*/ EventResultToString(EventResult::BLOCKED),
      /*username*/ "test-user@chromium.org",
      /*profile_identifier*/ profile_->GetPath().AsUTF8Unsafe(),
      /*scan_id*/ "",
      /*content_transfer_method*/ std::nullopt,
      /*user_justification*/ std::nullopt);

  EXPECT_TRUE(handler->UploadData());
  run_loop.Run();

  validator.ExpectSensitiveDataEvent(
      /*url*/
      kUrl,
      /*tab_url*/ kTabUrl,
      /*source*/ "",
      /*destination*/ "printer_name",
      /*filename*/ "tab_title",
      /*sha*/ "",
      /*trigger*/ "PAGE_PRINT",
      /*dlp_verdict*/
      CreateResult(ContentAnalysisResponse::Result::TriggeredRule::BLOCK),
      /*mimetype*/
      []() {
        static std::set<std::string> set = {""};
        return &set;
      }(),
      /*size*/ std::nullopt,
      /*result*/ EventResultToString(EventResult::BYPASSED),
      /*username*/ "test-user@chromium.org",
      /*profile_identifier*/ profile_->GetPath().AsUTF8Unsafe(),
      /*scan_id*/ "",
      /*content_transfer_method*/ std::nullopt,
      /*user_justification*/ kJustification);
  handler->ReportWarningBypass(kJustification);
}

}  // namespace enterprise_connectors
