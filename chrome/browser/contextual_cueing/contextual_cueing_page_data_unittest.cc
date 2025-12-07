// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/contextual_cueing_page_data.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_enums.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"

namespace contextual_cueing {

class ContextualCueingPageDataTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    web_contents_ = CreateTestWebContents();
  }

  void TearDown() override {
    web_contents_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void InvokePdfPageCountReceived(size_t page_count) {
    auto* page_data =
        ContextualCueingPageData::GetForPage(web_contents_->GetPrimaryPage());
    page_data->OnPdfPageCountReceived(
        pdf::mojom::PdfListener::GetPdfBytesStatus::kSuccess, {}, page_count);
  }

 protected:
  std::unique_ptr<content::WebContents> web_contents_;
};

TEST_F(ContextualCueingPageDataTest, Basic) {
  base::test::TestFuture<
      base::expected<CueingResult, contextual_cueing::NudgeDecision>>
      future;
  optimization_guide::proto::GlicContextualCueingMetadata metadata;
  auto* config = metadata.add_cueing_configurations();
  config->set_cue_label("basic label");
  config->set_dynamic_cue_label("should not use this label");

  ContextualCueingPageData::CreateForPage(web_contents_->GetPrimaryPage(),
                                          std::move(metadata),
                                          future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ("basic label", future.Get().value().cue_label);
  EXPECT_FALSE(future.Get().value().is_dynamic);
}

TEST_F(ContextualCueingPageDataTest, EarlyDestruction) {
  base::test::TestFuture<
      base::expected<CueingResult, contextual_cueing::NudgeDecision>>
      future;
  optimization_guide::proto::GlicContextualCueingMetadata metadata;
  auto* config = metadata.add_cueing_configurations();
  config->set_cue_label("basic label");
  // Use a page content condition to prevent the callback from being invoked
  // immediately.
  auto* condition = config->add_conditions();
  condition->set_signal(
      optimization_guide::proto::ContextualCueingClientSignal::
          CONTEXTUAL_CUEING_CLIENT_SIGNAL_CONTENT_LENGTH_WORD_COUNT);
  condition->set_cueing_operator(
      optimization_guide::proto::ContextualCueingOperator::
          CONTEXTUAL_CUEING_OPERATOR_GREATER_THAN_OR_EQUAL_TO);
  condition->set_int64_threshold(1000);

  ContextualCueingPageData::CreateForPage(web_contents_->GetPrimaryPage(),
                                          std::move(metadata),
                                          future.GetCallback());
  ContextualCueingPageData::DeleteForPage(web_contents_->GetPrimaryPage());
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(NudgeDecision::kNudgeDecisionInterrupted, future.Get().error());
}

TEST_F(ContextualCueingPageDataTest, NonPdfPageFails) {
  base::test::TestFuture<
      base::expected<CueingResult, contextual_cueing::NudgeDecision>>
      future;
  optimization_guide::proto::GlicContextualCueingMetadata metadata;
  auto* config = metadata.add_cueing_configurations();
  config->set_cue_label("basic label");
  auto* pdf_condition = config->add_conditions();
  pdf_condition->set_cueing_operator(
      optimization_guide::proto::ContextualCueingOperator::
          CONTEXTUAL_CUEING_OPERATOR_GREATER_THAN_OR_EQUAL_TO);
  pdf_condition->set_int64_threshold(2);

  ContextualCueingPageData::CreateForPage(web_contents_->GetPrimaryPage(),
                                          std::move(metadata),
                                          future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(future.Get().error(),
            contextual_cueing::NudgeDecision::kClientConditionsUnmet);
}

TEST_F(ContextualCueingPageDataTest, PdfPageCountFails) {
  content::WebContentsTester::For(web_contents_.get())
      ->SetMainFrameMimeType(pdf::kPDFMimeType);

  base::test::TestFuture<
      base::expected<CueingResult, contextual_cueing::NudgeDecision>>
      future;
  optimization_guide::proto::GlicContextualCueingMetadata metadata;
  auto* config = metadata.add_cueing_configurations();
  config->set_cue_label("pdf label");
  auto* pdf_condition = config->add_conditions();
  pdf_condition->set_signal(optimization_guide::proto::
                                CONTEXTUAL_CUEING_CLIENT_SIGNAL_PDF_PAGE_COUNT);
  pdf_condition->set_cueing_operator(
      optimization_guide::proto::ContextualCueingOperator::
          CONTEXTUAL_CUEING_OPERATOR_GREATER_THAN_OR_EQUAL_TO);
  pdf_condition->set_int64_threshold(2);

  ContextualCueingPageData::CreateForPage(web_contents_->GetPrimaryPage(),
                                          std::move(metadata),
                                          future.GetCallback());
  InvokePdfPageCountReceived(1);

  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(future.Get().error(),
            contextual_cueing::NudgeDecision::kClientConditionsUnmet);
}

TEST_F(ContextualCueingPageDataTest, PdfPageCountPasses) {
  content::WebContentsTester::For(web_contents_.get())
      ->SetMainFrameMimeType(pdf::kPDFMimeType);

  base::test::TestFuture<
      base::expected<CueingResult, contextual_cueing::NudgeDecision>>
      future;
  optimization_guide::proto::GlicContextualCueingMetadata metadata;
  auto* config = metadata.add_cueing_configurations();
  config->set_cue_label("pdf label");
  auto* pdf_condition = config->add_conditions();
  pdf_condition->set_signal(optimization_guide::proto::
                                CONTEXTUAL_CUEING_CLIENT_SIGNAL_PDF_PAGE_COUNT);
  pdf_condition->set_cueing_operator(
      optimization_guide::proto::ContextualCueingOperator::
          CONTEXTUAL_CUEING_OPERATOR_GREATER_THAN_OR_EQUAL_TO);
  pdf_condition->set_int64_threshold(2);

  ContextualCueingPageData::CreateForPage(web_contents_->GetPrimaryPage(),
                                          std::move(metadata),
                                          future.GetCallback());
  InvokePdfPageCountReceived(4);

  ASSERT_TRUE(future.Wait());
  EXPECT_EQ("pdf label", future.Get().value().cue_label);
  EXPECT_FALSE(future.Get().value().is_dynamic);
}

TEST_F(ContextualCueingPageDataTest, BasicAndPdfPageCountCondition) {
  content::WebContentsTester::For(web_contents_.get())
      ->SetMainFrameMimeType(pdf::kPDFMimeType);

  base::test::TestFuture<
      base::expected<CueingResult, contextual_cueing::NudgeDecision>>
      future;
  optimization_guide::proto::GlicContextualCueingMetadata metadata;
  auto* config = metadata.add_cueing_configurations();
  config->set_cue_label("pdf label");
  auto* pdf_condition = config->add_conditions();
  pdf_condition->set_signal(optimization_guide::proto::
                                CONTEXTUAL_CUEING_CLIENT_SIGNAL_PDF_PAGE_COUNT);
  pdf_condition->set_cueing_operator(
      optimization_guide::proto::ContextualCueingOperator::
          CONTEXTUAL_CUEING_OPERATOR_GREATER_THAN_OR_EQUAL_TO);
  pdf_condition->set_int64_threshold(2);

  // Second basic condition should get picked.
  config = metadata.add_cueing_configurations();
  config->set_cue_label("basic label");

  ContextualCueingPageData::CreateForPage(web_contents_->GetPrimaryPage(),
                                          std::move(metadata),
                                          future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ("basic label", future.Get().value().cue_label);
  EXPECT_FALSE(future.Get().value().is_dynamic);
}

class ContextualCueingPageDataTestDynamicCue
    : public ContextualCueingPageDataTest {
 public:
  void SetUp() override {
    ContextualCueingPageDataTest::SetUp();
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        kContextualCueing, {{"UseDynamicCues", "true"}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ContextualCueingPageDataTestDynamicCue, Basic) {
  base::test::TestFuture<
      base::expected<CueingResult, contextual_cueing::NudgeDecision>>
      future;
  optimization_guide::proto::GlicContextualCueingMetadata metadata;
  auto* config = metadata.add_cueing_configurations();
  config->set_cue_label("should not use this label");
  config->set_dynamic_cue_label("dynamic label");

  ContextualCueingPageData::CreateForPage(web_contents_->GetPrimaryPage(),
                                          std::move(metadata),
                                          future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ("dynamic label", future.Get().value().cue_label);
  EXPECT_TRUE(future.Get().value().is_dynamic);
}

TEST_F(ContextualCueingPageDataTestDynamicCue, DynamicCueNotAvailable) {
  base::test::TestFuture<
      base::expected<CueingResult, contextual_cueing::NudgeDecision>>
      future;
  optimization_guide::proto::GlicContextualCueingMetadata metadata;
  auto* config = metadata.add_cueing_configurations();
  config->set_cue_label("basic label");

  ContextualCueingPageData::CreateForPage(web_contents_->GetPrimaryPage(),
                                          std::move(metadata),
                                          future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ("basic label", future.Get().value().cue_label);
  EXPECT_FALSE(future.Get().value().is_dynamic);
}

TEST_F(ContextualCueingPageDataTestDynamicCue, ReturnsDefaultText) {
  base::test::TestFuture<
      base::expected<CueingResult, contextual_cueing::NudgeDecision>>
      future;
  optimization_guide::proto::GlicContextualCueingMetadata metadata;
  auto* config = metadata.add_cueing_configurations();
  config->set_cue_label("should not use this label");
  config->set_dynamic_cue_label("dynamic label");
  config->set_default_text("prompt suggestion");

  ContextualCueingPageData::CreateForPage(web_contents_->GetPrimaryPage(),
                                          std::move(metadata),
                                          future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ("dynamic label", future.Get().value().cue_label);
  EXPECT_EQ("prompt suggestion", future.Get().value().prompt_suggestion);
  EXPECT_TRUE(future.Get().value().is_dynamic);
}

}  // namespace contextual_cueing
