// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility_annotator/content_annotator/content_annotator_service_factory.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/page_content_annotations/page_content_annotations_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/accessibility_annotator/core/public/accessibility_annotator_features.h"
#include "components/page_content_annotations/core/test_page_content_annotations_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace accessibility_annotator {

class ContentAnnotatorServiceFactoryTest : public testing::Test {
 public:
  ContentAnnotatorServiceFactoryTest() = default;
  ~ContentAnnotatorServiceFactoryTest() override = default;

 protected:
  void SetUp() override {
    testing::Test::SetUp();
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &ContentAnnotatorServiceFactoryTest::
                    OnWillCreateBrowserContextKeyedServices));
  }

  static void OnWillCreateBrowserContextKeyedServices(
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
  }

  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  // Used to set up test factories for each browser context.
  base::CallbackListSubscription create_services_subscription_;
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
