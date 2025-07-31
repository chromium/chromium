// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/contextual_cueing_helper.h"

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_features.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service_factory.h"
#include "chrome/browser/contextual_cueing/mock_contextual_cueing_service.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service_factory.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "third_party/blink/public/common/features.h"

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
  return std::make_unique<testing::NiceMock<MockContextualCueingService>>();
}

class ContextualCueingHelperTest : public ChromeRenderViewHostTestHarness {
 public:
  ContextualCueingHelperTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kGlic, features::kTabstripComboButton, kContextualCueing},
        {contextual_cueing::kGlicZeroStateSuggestions});
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

 private:
  std::unique_ptr<TestingProfileManager> profile_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ContextualCueingHelperTest, TabHelperStartsUp) {
  ContextualCueingHelper::MaybeCreateForWebContents(web_contents());
  auto* contextual_cueing_helper =
      ContextualCueingHelper::FromWebContents(web_contents());
  EXPECT_NE(nullptr, contextual_cueing_helper);
}

class ContextualCueingHelperResponseCodeTest
    : public ContextualCueingHelperTest,
      public testing::WithParamInterface<bool> {
 public:
  ContextualCueingHelperResponseCodeTest() {
    std::vector<base::test::FeatureRef> enabled_features = {
        features::kGlic, features::kTabstripComboButton, kContextualCueing};
    std::vector<base::test::FeatureRef> disabled_features = {
        contextual_cueing::kGlicZeroStateSuggestions};

    const bool are_404_navigations_saved_to_history = GetParam();
    if (are_404_navigations_saved_to_history) {
      enabled_features.push_back(
          blink::features::kVisitedLinksOnErrorNavigation);
    } else {
      disabled_features.push_back(
          blink::features::kVisitedLinksOnErrorNavigation);
    }

    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(ContextualCueingHelperResponseCodeTest, Committed404Page) {
  ContextualCueingHelper::MaybeCreateForWebContents(web_contents());
  auto* contextual_cueing_helper =
      ContextualCueingHelper::FromWebContents(web_contents());
  ASSERT_NE(contextual_cueing_helper, nullptr);
  auto* mock_contextual_cueing_service =
      static_cast<testing::NiceMock<MockContextualCueingService>*>(
          ContextualCueingServiceFactory::GetForProfile(profile()));
  ASSERT_NE(mock_contextual_cueing_service, nullptr);

  // Navigate to a URL that returns a 404 with a body.
  auto navigation_simulator =
      content::NavigationSimulator::CreateBrowserInitiated(
          GURL("http://www.foo.com/custom404"), web_contents());
  navigation_simulator->Start();
  std::string raw_response_headers = "HTTP/1.1 404 Not Found\r\n\r\n";
  scoped_refptr<net::HttpResponseHeaders> response_headers =
      net::HttpResponseHeaders::TryToCreate(raw_response_headers);
  navigation_simulator->SetResponseHeaders(response_headers);
  std::string response_body = "Not found, sorry";
  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  ASSERT_EQ(MOJO_RESULT_OK,
            mojo::CreateDataPipe(response_body.size(), producer_handle,
                                 consumer_handle));
  navigation_simulator->SetResponseBody(std::move(consumer_handle));
  size_t actually_written_bytes = 0;
  ASSERT_EQ(MOJO_RESULT_OK,
            producer_handle->WriteData(base::as_byte_span(response_body),
                                       MOJO_WRITE_DATA_FLAG_NONE,
                                       actually_written_bytes));
  EXPECT_EQ(actually_written_bytes, response_body.size());

  // If 404 navigations are saved to history, we should filter them out. If they
  // aren't saved to history, we still won't report it, because we only report
  // page loads for navigations that are saved to history.
  EXPECT_CALL(*mock_contextual_cueing_service, ReportPageLoad()).Times(0);
  navigation_simulator->Commit();
}

INSTANTIATE_TEST_SUITE_P(All,
                         ContextualCueingHelperResponseCodeTest,
                         ::testing::Bool());

#endif  // BUILDFLAG(ENABLE_GLIC)

}  // namespace
}  // namespace contextual_cueing
