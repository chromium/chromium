// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility_annotator/content_annotator/content_annotator_tab_helper.h"

#include "chrome/browser/accessibility_annotator/content_annotator/content_annotator_service_factory.h"
#include "chrome/browser/page_content_annotations/page_content_annotations_service_factory.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/accessibility_annotator/content/content_annotator/content_annotator_service.h"
#include "components/accessibility_annotator/content/content_annotator/content_classifier.h"
#include "components/history/core/browser/history_service.h"
#include "components/optimization_guide/core/delivery/test_optimization_guide_model_provider.h"
#include "components/optimization_guide/core/model_execution/test/mock_remote_model_executor.h"
#include "components/page_content_annotations/content/page_content_extraction_service.h"
#include "components/page_content_annotations/core/test_page_content_annotations_service.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/translate/core/common/language_detection_details.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace accessibility_annotator {

class MockContentAnnotatorService : public ContentAnnotatorService {
 public:
  explicit MockContentAnnotatorService(
      page_content_annotations::PageContentAnnotationsService&
          page_content_annotations_service,
      page_content_annotations::PageContentExtractionService&
          page_content_extraction_service,
      optimization_guide::RemoteModelExecutor&
          optimization_guide_remote_model_executor,
      std::unique_ptr<ContentClassifier> content_classifier)
      : ContentAnnotatorService(page_content_annotations_service,
                                page_content_extraction_service,
                                optimization_guide_remote_model_executor,
                                std::move(content_classifier)) {}
  ~MockContentAnnotatorService() override = default;

  MOCK_METHOD(void,
              OnLanguageDetermined,
              (const translate::LanguageDetectionDetails&),
              (override));
};

class ContentAnnotatorTabHelperTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    page_content_annotations_service_ =
        page_content_annotations::TestPageContentAnnotationsService::Create(
            &optimization_guide_model_provider_, &history_service_);

    page_content_annotations::PageContentExtractionService*
        page_content_extraction_service = page_content_annotations::
            PageContentExtractionServiceFactory::GetForProfile(profile());
    ASSERT_TRUE(page_content_extraction_service);

    std::unique_ptr<ContentClassifier> content_classifier_ =
        ContentClassifier::Create();
    ASSERT_TRUE(content_classifier_);

    mock_service_ =
        std::make_unique<testing::StrictMock<MockContentAnnotatorService>>(
            *page_content_annotations_service_,
            *page_content_extraction_service, mock_remote_model_executor_,
            std::move(content_classifier_));

    tab_interface_ = std::make_unique<tabs::MockTabInterface>();
    EXPECT_CALL(*tab_interface_, GetContents())
        .WillRepeatedly(testing::Return(web_contents()));
    helper_ = std::make_unique<ContentAnnotatorTabHelper>(
        *tab_interface_, *mock_service_.get(), nullptr);
  }

  void TearDown() override {
    helper_.reset();
    tab_interface_.reset();
    mock_service_.reset();
    page_content_annotations_service_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  ContentAnnotatorTabHelper* helper() { return helper_.get(); }

  MockContentAnnotatorService* mock_service() { return mock_service_.get(); }

  history::HistoryService history_service_;
  optimization_guide::TestOptimizationGuideModelProvider
      optimization_guide_model_provider_;
  optimization_guide::MockRemoteModelExecutor mock_remote_model_executor_;
  std::unique_ptr<page_content_annotations::TestPageContentAnnotationsService>
      page_content_annotations_service_;
  std::unique_ptr<MockContentAnnotatorService> mock_service_;
  std::unique_ptr<tabs::MockTabInterface> tab_interface_;
  std::unique_ptr<ContentAnnotatorTabHelper> helper_;
};

TEST_F(ContentAnnotatorTabHelperTest, PropagatesLanguageEvent) {
  translate::LanguageDetectionDetails details;
  details.adopted_language = "fr";

  EXPECT_CALL(
      *mock_service(),
      OnLanguageDetermined(testing::Field(
          &translate::LanguageDetectionDetails::adopted_language, "fr")));

  // Manually trigger the event on the helper
  helper()->OnLanguageDetermined(details);
}
}  // namespace accessibility_annotator
