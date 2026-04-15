// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility_annotator/content_annotator/content_annotator_tab_helper.h"

#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/accessibility_annotator/accessibility_annotator_backend_factory.h"
#include "chrome/browser/accessibility_annotator/content_annotator/content_annotator_service_factory.h"
#include "chrome/browser/page_content_annotations/page_content_annotations_service_factory.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service_factory.h"
#include "chrome/browser/page_content_annotations/page_embeddings_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/constants/chromeos_features.h"
#endif
#include "chrome/test/base/testing_profile.h"
#include "components/accessibility_annotator/content/content_annotator/content_annotator_service.h"
#include "components/accessibility_annotator/content/content_annotator/content_classifier.h"
#include "components/accessibility_annotator/core/accessibility_annotator_features.h"
#include "components/accessibility_annotator/core/storage/accessibility_annotator_backend.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/history/core/test/test_history_database.h"
#include "components/optimization_guide/core/delivery/test_optimization_guide_model_provider.h"
#include "components/optimization_guide/core/model_execution/test/mock_remote_model_executor.h"
#include "components/page_content_annotations/content/page_content_extraction_service.h"
#include "components/page_content_annotations/content/page_embeddings_service.h"
#include "components/page_content_annotations/core/test_page_content_annotations_service.h"
#include "components/passage_embeddings/core/passage_embeddings_test_util.h"
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
      page_content_annotations::PageEmbeddingsService& page_embeddings_service,
      AccessibilityAnnotatorBackend& accessibility_annotator_backend,
      passage_embeddings::Embedder* embedder,
      passage_embeddings::EmbedderMetadataProvider* embedder_metadata_provider,
      std::unique_ptr<ContentClassifier> content_classifier)
      : ContentAnnotatorService(page_content_annotations_service,
                                page_content_extraction_service,
                                optimization_guide_remote_model_executor,
                                page_embeddings_service,
                                accessibility_annotator_backend,
                                embedder,
                                embedder_metadata_provider,
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
    std::vector<base::test::FeatureRef> enabled_features;
    enabled_features.push_back(features::kContentAnnotator);
#if BUILDFLAG(IS_CHROMEOS)
    enabled_features.push_back(
        chromeos::features::kFeatureManagementPassageEmbedder);
#endif
    feature_list_.InitWithFeatures(enabled_features, {});

    ChromeRenderViewHostTestHarness::SetUp();

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    history_service_ = std::make_unique<history::HistoryService>();
    history_service_->Init(
        history::TestHistoryDatabaseParamsForPath(temp_dir_.GetPath()));

    page_content_annotations_service_ =
        page_content_annotations::TestPageContentAnnotationsService::Create(
            &optimization_guide_model_provider_, history_service_.get());

    page_content_annotations::PageContentExtractionService*
        page_content_extraction_service = page_content_annotations::
            PageContentExtractionServiceFactory::GetForProfile(profile());
    ASSERT_TRUE(page_content_extraction_service);

    page_content_annotations::PageEmbeddingsService* page_embeddings_service =
        page_content_annotations::PageEmbeddingsServiceFactory::GetForProfile(
            profile());
    ASSERT_TRUE(page_embeddings_service);

    accessibility_annotator::AccessibilityAnnotatorBackend*
        accessibility_annotator_backend =
            AccessibilityAnnotatorBackendFactory::GetForProfile(profile());
    ASSERT_TRUE(accessibility_annotator_backend);

    std::unique_ptr<ContentClassifier> content_classifier_ =
        ContentClassifier::Create(mock_embedder_.get());
    ASSERT_TRUE(content_classifier_);

    mock_service_ =
        std::make_unique<testing::StrictMock<MockContentAnnotatorService>>(
            *page_content_annotations_service_,
            *page_content_extraction_service, mock_remote_model_executor_,
            *page_embeddings_service, *accessibility_annotator_backend,
            mock_embedder_.get(), mock_embedder_metadata_provider_.get(),
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
    mock_embedder_.reset();
    mock_embedder_metadata_provider_.reset();
    page_content_annotations_service_.reset();
    history::BlockUntilHistoryProcessesPendingRequests(history_service_.get());
    history_service_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  ContentAnnotatorTabHelper* helper() { return helper_.get(); }

  MockContentAnnotatorService* mock_service() { return mock_service_.get(); }

  base::test::ScopedFeatureList feature_list_;
  base::ScopedTempDir temp_dir_;

  std::unique_ptr<history::HistoryService> history_service_;
  optimization_guide::TestOptimizationGuideModelProvider
      optimization_guide_model_provider_;
  optimization_guide::MockRemoteModelExecutor mock_remote_model_executor_;
  std::unique_ptr<passage_embeddings::TestEmbedder> mock_embedder_;
  std::unique_ptr<passage_embeddings::TestEmbedderMetadataProvider>
      mock_embedder_metadata_provider_;
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
