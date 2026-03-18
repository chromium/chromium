// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility_annotator/content_annotator/content_annotator_service_factory.h"

#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/accessibility_annotator/accessibility_annotator_backend_factory.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/page_content_annotations/page_content_annotations_service_factory.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service_factory.h"
#include "chrome/browser/page_content_annotations/page_embeddings_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/accessibility_annotator/core/accessibility_annotator_features.h"
#include "components/accessibility_annotator/core/storage/accessibility_annotator_backend.h"
#include "components/page_content_annotations/content/page_content_extraction_service.h"
#include "components/page_content_annotations/content/page_embeddings_service.h"
#include "components/page_content_annotations/core/test_page_content_annotations_service.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "components/version_info/channel.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace accessibility_annotator {

class TestPageEmbeddingsService
    : public page_content_annotations::PageEmbeddingsService {
 public:
  explicit TestPageEmbeddingsService(
      page_content_annotations::PageContentExtractionService*
          page_content_extraction_service)
      : PageEmbeddingsService(page_content_extraction_service) {}
};

class ContentAnnotatorServiceFactoryTest : public testing::Test {
 public:
  ContentAnnotatorServiceFactoryTest() = default;
  ~ContentAnnotatorServiceFactoryTest() override = default;

 protected:
  void SetUp() override {
    testing::Test::SetUp();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&ContentAnnotatorServiceFactoryTest::
                                        OnWillCreateBrowserContextKeyedServices,
                                    base::Unretained(this)));
  }

  void OnWillCreateBrowserContextKeyedServices(
      content::BrowserContext* browser_context) {
    PageContentAnnotationsServiceFactory::GetInstance()
        ->SetTestingFactoryAndUse(
            browser_context,
            base::BindRepeating([](content::BrowserContext* context)
                                    -> std::unique_ptr<KeyedService> {
              return page_content_annotations::
                  TestPageContentAnnotationsService::Create(
                      /*optimization_guide_model_provider=*/nullptr,
                      /*history_service=*/nullptr);
            }));
    page_content_annotations::PageContentExtractionServiceFactory::GetInstance()
        ->SetTestingFactoryAndUse(
            browser_context,
            base::BindRepeating([](content::BrowserContext* context)
                                    -> std::unique_ptr<KeyedService> {
              return std::make_unique<
                  page_content_annotations::PageContentExtractionService>(
                  /*os_crypt_async=*/nullptr, context->GetPath(),
                  /*tracker=*/nullptr);
            }));
    OptimizationGuideKeyedServiceFactory::GetInstance()
        ->SetTestingFactoryAndUse(
            browser_context,
            base::BindRepeating([](content::BrowserContext* context)
                                    -> std::unique_ptr<KeyedService> {
              return std::make_unique<MockOptimizationGuideKeyedService>();
            }));
    page_content_annotations::PageEmbeddingsServiceFactory::GetInstance()
        ->SetTestingFactoryAndUse(
            browser_context,
            base::BindRepeating([](content::BrowserContext* context)
                                    -> std::unique_ptr<KeyedService> {
              return std::make_unique<TestPageEmbeddingsService>(
                  page_content_annotations::
                      PageContentExtractionServiceFactory::GetForProfile(
                          Profile::FromBrowserContext(context)));
            }));
    AccessibilityAnnotatorBackendFactory::GetInstance()
        ->SetTestingFactoryAndUse(
            browser_context,
            base::BindRepeating(
                [](base::FilePath path, content::BrowserContext* context)
                    -> std::unique_ptr<KeyedService> {
                  return std::make_unique<AccessibilityAnnotatorBackend>(
                      version_info::Channel::UNKNOWN,
                      syncer::DataTypeStoreTestUtil::
                          FactoryForInMemoryStoreForTest(),
                      path.Append(
                          FILE_PATH_LITERAL("AccessibilityAnnotatorDatabase")));
                },
                temp_dir_.GetPath()));
  }

  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  // Used to set up test factories for each browser context.
  base::CallbackListSubscription create_services_subscription_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(ContentAnnotatorServiceFactoryTest, CreatesServiceWithFlagEnabled) {
  scoped_feature_list_.InitAndEnableFeature(
      accessibility_annotator::kContentAnnotator);
  TestingProfile profile;
  EXPECT_NE(nullptr, ContentAnnotatorServiceFactory::GetForProfile(&profile));
}

TEST_F(ContentAnnotatorServiceFactoryTest, NoServiceWithFlagDisabled) {
  scoped_feature_list_.InitAndDisableFeature(
      accessibility_annotator::kContentAnnotator);
  TestingProfile profile;
  EXPECT_EQ(nullptr, ContentAnnotatorServiceFactory::GetForProfile(&profile));
}

TEST_F(ContentAnnotatorServiceFactoryTest,
       NoServiceForIncognitoWithFlagEnabled) {
  scoped_feature_list_.InitAndEnableFeature(
      accessibility_annotator::kContentAnnotator);
  TestingProfile profile;
  Profile* otr_profile = profile.GetOffTheRecordProfile(
      Profile::OTRProfileID::PrimaryID(), /*create_if_needed=*/true);
  EXPECT_EQ(nullptr,
            ContentAnnotatorServiceFactory::GetForProfile(otr_profile));
}

}  // namespace accessibility_annotator
