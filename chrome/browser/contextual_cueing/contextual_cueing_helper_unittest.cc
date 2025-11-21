// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/contextual_cueing_helper.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_features.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service_factory.h"
#include "chrome/browser/contextual_cueing/mock_contextual_cueing_service.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
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
#include "components/history/core/browser/features.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/test/glic_user_session_test_helper.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

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
      page_content_annotations::PageContentExtractionService>(
      /*os_crypt_async=*/nullptr, context->GetPath());
}

std::unique_ptr<KeyedService> CreateContextualCueingService(
    content::BrowserContext* context) {
  return std::make_unique<testing::NiceMock<MockContextualCueingService>>();
}

class ContextualCueingHelperTest : public ChromeRenderViewHostTestHarness {
 public:
  ContextualCueingHelperTest() {
    scoped_feature_list_.InitWithFeatures(
        {kContextualCueing}, {contextual_cueing::kGlicZeroStateSuggestions});
  }

  void SetUp() override {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    TestingBrowserProcess::GetGlobal()->CreateGlobalFeaturesForTesting();
#if BUILDFLAG(IS_CHROMEOS)
    glic_user_session_test_helper_.PreProfileSetUp(
        profile_manager_->profile_manager());
#endif  // BUILDFLAG(IS_CHROMEOS)

    ChromeRenderViewHostTestHarness::SetUp();

    // Bypass glic eligibility check.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(::switches::kGlicDev);

    glic_test_env_.SetupProfile(profile());
  }

  void TearDown() override {
    // Delete profile earlier since it must be destroyed before TaskEnvironment
    // is destroyed. NOTE: In production profile is deleted with ProfileManager.
    {
      DeleteContents();
      profile_ = nullptr;
      profile_manager_->DeleteAllTestingProfiles();
    }

    ChromeRenderViewHostTestHarness::TearDown();
    TestingBrowserProcess::GetGlobal()->GetFeatures()->Shutdown();

    profile_manager_.reset();

#if BUILDFLAG(IS_CHROMEOS)
    glic_user_session_test_helper_.PostProfileTearDown();
#endif  // BUILDFLAG(IS_CHROMEOS)
  }

  std::unique_ptr<TestingProfile> CreateTestingProfile() override {
    CHECK(!profile_);
    profile_ = profile_manager_->CreateTestingProfile(
        TestingProfile::kDefaultProfileUserName, GetTestingFactories());
    CHECK(profile_);

    // NOTE: The new profile is owned by TestingProfileManager, so this cannot
    // return it. It is returned by `GetBrowserContext()` instead.
    return nullptr;
  }

  // content::RenderViewHostTestHarness override:
  content::BrowserContext* GetBrowserContext() override {
    CHECK(profile_);
    return profile_.get();
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
  glic::GlicUnitTestEnvironment glic_test_env_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
#if BUILDFLAG(IS_CHROMEOS)
  ash::GlicUserSessionTestHelper glic_user_session_test_helper_;
#endif  // BUILDFLAG(IS_CHROMEOS)
  raw_ptr<TestingProfile> profile_ = nullptr;
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
    std::vector<base::test::FeatureRef> enabled_features = {kContextualCueing};
    std::vector<base::test::FeatureRef> disabled_features = {
        contextual_cueing::kGlicZeroStateSuggestions};

    const bool are_404_navigations_saved_to_history = GetParam();
    if (are_404_navigations_saved_to_history) {
      enabled_features.push_back(history::kVisitedLinksOn404);
    } else {
      disabled_features.push_back(history::kVisitedLinksOn404);
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
