// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/contextual_cueing_helper.h"

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_features.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service_factory.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"

namespace contextual_cueing {
namespace {

#if BUILDFLAG(ENABLE_GLIC)

using ::testing::Return;

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
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<ContextualCueingService>(
      page_content_annotations::PageContentExtractionServiceFactory::
          GetForProfile(profile),
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile),
      /*loading_predictor=*/nullptr, profile->GetPrefs());
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

    // Bypass glic eligibility check.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(::switches::kGlicDev);
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

  void SetUpAllowedExpectation(bool allowed) {
    auto* ogks =
        static_cast<testing::NiceMock<MockOptimizationGuideKeyedService>*>(
            OptimizationGuideKeyedServiceFactory::GetForProfile(profile()));

    EXPECT_CALL(*ogks, ShouldModelExecutionBeAllowedForUser)
        .WillOnce(Return(allowed));
  }

 private:
  std::unique_ptr<TestingProfileManager> profile_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ContextualCueingHelperTest, NullTabHelperWithoutModelExecution) {
  SetUpAllowedExpectation(/*allowed=*/false);
  ContextualCueingHelper::MaybeCreateForWebContents(web_contents());
  EXPECT_EQ(nullptr, ContextualCueingHelper::FromWebContents(web_contents()));
}

TEST_F(ContextualCueingHelperTest, TabHelperStartsUp) {
  SetUpAllowedExpectation(/*allowed=*/true);
  ContextualCueingHelper::MaybeCreateForWebContents(web_contents());
  auto* contextual_cueing_helper =
      ContextualCueingHelper::FromWebContents(web_contents());
  EXPECT_NE(nullptr, contextual_cueing_helper);
}
#endif  // BUILDFLAG(ENABLE_GLIC)

}  // namespace
}  // namespace contextual_cueing
