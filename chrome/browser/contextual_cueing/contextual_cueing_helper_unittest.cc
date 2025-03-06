// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/contextual_cueing_helper.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_features.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service_factory.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/component_updater/pref_names.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"

namespace contextual_cueing {

#if BUILDFLAG(ENABLE_GLIC)

std::unique_ptr<KeyedService> CreateOptimizationGuideKeyedService(
    content::BrowserContext* context) {
  return std::make_unique<
      testing::NiceMock<MockOptimizationGuideKeyedService>>();
}

std::unique_ptr<KeyedService> CreatePageContentExtractionService(
    content::BrowserContext* context) {
  return std::make_unique<
      page_content_annotations::PageContentExtractionService>();
}

std::unique_ptr<KeyedService> CreateContextualCueingService(
    content::BrowserContext* context) {
  return std::make_unique<ContextualCueingService>(
      page_content_annotations::PageContentExtractionServiceFactory::
          GetForProfile(Profile::FromBrowserContext(context)));
}

class ContextualCueingHelperTest : public ChromeRenderViewHostTestHarness {
 public:
  ContextualCueingHelperTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kGlic, features::kTabstripComboButton, kContextualCueing},
        {});
  }

  void SetUp() override {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    TestingBrowserProcess::GetGlobal()->CreateGlobalFeaturesForTesting();
    ChromeRenderViewHostTestHarness::SetUp();
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();

    auto* fake_optimization_guide_keyed_service =
        (testing::NiceMock<MockOptimizationGuideKeyedService>*)
            OptimizationGuideKeyedServiceFactory::GetForProfile(profile());
    ON_CALL(*fake_optimization_guide_keyed_service,
            ShouldModelExecutionBeAllowedForUser)
        .WillByDefault(testing::Return(true));
  }

  void TearDown() override {
    ChromeRenderViewHostTestHarness::TearDown();
    TestingBrowserProcess::GetGlobal()->GetFeatures()->Shutdown();
  }

  TestingProfile::TestingFactories GetTestingFactories() const override {
    return {TestingProfile::TestingFactory{
                OptimizationGuideKeyedServiceFactory::GetInstance(),
                base::BindRepeating(&CreateOptimizationGuideKeyedService)},
            TestingProfile::TestingFactory{
                page_content_annotations::PageContentExtractionServiceFactory::
                    GetInstance(),
                base::BindRepeating(&CreatePageContentExtractionService)},
            TestingProfile::TestingFactory{
                ContextualCueingServiceFactory::GetInstance(),
                base::BindRepeating(&CreateContextualCueingService)}};
  }

 private:
  std::unique_ptr<TestingProfileManager> profile_manager_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ContextualCueingHelperTest, NullTabHelperWithoutModelExecution) {
  auto* fake_optimization_guide_keyed_service =
      (testing::NiceMock<MockOptimizationGuideKeyedService>*)
          OptimizationGuideKeyedServiceFactory::GetForProfile(profile());
  ON_CALL(*fake_optimization_guide_keyed_service,
          ShouldModelExecutionBeAllowedForUser)
      .WillByDefault(testing::Return(false));

  ContextualCueingHelper::MaybeCreateForWebContents(web_contents());
  EXPECT_EQ(nullptr, ContextualCueingHelper::FromWebContents(web_contents()));
}

TEST_F(ContextualCueingHelperTest, TabHelperStartsUp) {
  ContextualCueingHelper::MaybeCreateForWebContents(web_contents());
  auto* contextual_cueing_helper =
      ContextualCueingHelper::FromWebContents(web_contents());
  EXPECT_NE(nullptr, contextual_cueing_helper);
}
#endif  // BUILDFLAG(ENABLE_GLIC)

}  // namespace contextual_cueing
