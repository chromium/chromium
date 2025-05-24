// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_page_context_eligibility_observer.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/glic/host/context/glic_page_context_eligibility_observer.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/proto/glic_page_context_eligibility_metadata.pb.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace glic {

using ::testing::_;
using ::testing::An;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::WithArgs;

class GlicPageContextEligibilityObserverBrowserTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  GlicPageContextEligibilityObserverBrowserTest() = default;
  ~GlicPageContextEligibilityObserverBrowserTest() = default;

  void SetUp() override {
    InitializeFeatureList();

    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (GetParam()) {
      command_line->AppendSwitch(optimization_guide::switches::
                                     kDisableCheckingUserPermissionsForTesting);
    } else {
      command_line->RemoveSwitch(optimization_guide::switches::
                                     kDisableCheckingUserPermissionsForTesting);
    }
  }

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* browser_context) override {
    if (GetParam()) {
      // With permissions, can just use the seeding properly.
      return;
    }

    mock_optimization_guide_keyed_service_ =
        static_cast<testing::NiceMock<MockOptimizationGuideKeyedService>*>(
            OptimizationGuideKeyedServiceFactory::GetInstance()
                ->SetTestingFactoryAndUse(
                    browser_context,
                    base::BindRepeating([](content::BrowserContext* context)
                                            -> std::unique_ptr<KeyedService> {
                      return std::make_unique<testing::NiceMock<
                          MockOptimizationGuideKeyedService>>();
                    })));
  }

  void PostRunTestOnMainThread() override {
    mock_optimization_guide_keyed_service_ = nullptr;
    InProcessBrowserTest::PostRunTestOnMainThread();
  }

  virtual void InitializeFeatureList() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{features::kGlic, {}},
         {features::kTabstripComboButton, {}},
         {features::kGlicRollout, {}},
         {
             features::kGlicPageContextEligibility,
             {
                 {"glic-page-context-eligibility-allow-no-metadata", "true"},
             },
         }},
        /*disabled_features=*/{});
  }

  GlicPageContextEligibilityObserver* GetObserver() {
    return browser()
        ->GetActiveTabInterface()
        ->GetTabFeatures()
        ->glic_page_context_eligibility_observer();
  }

  void SetUpEligibilityExpectations(const GURL& url, bool is_eligible) {
    optimization_guide::proto::GlicPageContextEligibilityMetadata
        page_context_eligibility_metadata;
    page_context_eligibility_metadata.set_is_eligible(is_eligible);
    optimization_guide::OptimizationMetadata metadata;
    metadata.SetAnyMetadataForTesting(page_context_eligibility_metadata);

    if (GetParam()) {
      OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
          ->AddHintForTesting(
              url, optimization_guide::proto::GLIC_PAGE_CONTEXT_ELIGIBILITY,
              metadata);
    } else {
      EXPECT_CALL(
          *mock_optimization_guide_keyed_service_,
          CanApplyOptimizationOnDemand(
              ElementsAre(url),
              ElementsAre(
                  optimization_guide::proto::GLIC_PAGE_CONTEXT_ELIGIBILITY),
              Eq(optimization_guide::proto::RequestContext::
                     CONTEXT_GLIC_PAGE_CONTEXT),
              _, Eq(std::nullopt)))
          .WillOnce(WithArgs<3>(
              [=](optimization_guide::
                      OnDemandOptimizationGuideDecisionRepeatingCallback
                          callback) {
                optimization_guide::OptimizationGuideDecisionWithMetadata
                    decision_with_metadata;
                decision_with_metadata.decision =
                    optimization_guide::OptimizationGuideDecision::kTrue;
                decision_with_metadata.metadata = metadata;
                callback.Run(
                    url,
                    {{optimization_guide::proto::GLIC_PAGE_CONTEXT_ELIGIBILITY,
                      decision_with_metadata}});
              }));
    }
  }

  void SetUpNoMetadataExpectations(const GURL& url) {
    if (GetParam()) {
      OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
          ->AddHintForTesting(
              url, optimization_guide::proto::GLIC_PAGE_CONTEXT_ELIGIBILITY,
              {});
    } else {
      EXPECT_CALL(
          *mock_optimization_guide_keyed_service_,
          CanApplyOptimizationOnDemand(
              ElementsAre(url),
              ElementsAre(
                  optimization_guide::proto::GLIC_PAGE_CONTEXT_ELIGIBILITY),
              Eq(optimization_guide::proto::RequestContext::
                     CONTEXT_GLIC_PAGE_CONTEXT),
              _, Eq(std::nullopt)))
          .WillOnce(WithArgs<3>(
              [=](optimization_guide::
                      OnDemandOptimizationGuideDecisionRepeatingCallback
                          callback) {
                optimization_guide::OptimizationGuideDecisionWithMetadata
                    decision_with_metadata;
                decision_with_metadata.decision =
                    optimization_guide::OptimizationGuideDecision::kFalse;
                callback.Run(
                    url,
                    {{optimization_guide::proto::GLIC_PAGE_CONTEXT_ELIGIBILITY,
                      decision_with_metadata}});
              }));
    }
  }

  bool HasMockOptimizationGuideKeyedService() const {
    return mock_optimization_guide_keyed_service_;
  }
  MockOptimizationGuideKeyedService& mock_optimization_guide_keyed_service() {
    return *mock_optimization_guide_keyed_service_;
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  raw_ptr<testing::NiceMock<MockOptimizationGuideKeyedService>>
      mock_optimization_guide_keyed_service_;
};

INSTANTIATE_TEST_SUITE_P(OptGuidePermissions,
                         GlicPageContextEligibilityObserverBrowserTest,
                         ::testing::Values(false, true));

IN_PROC_BROWSER_TEST_P(GlicPageContextEligibilityObserverBrowserTest,
                       GetEligibility_NoMetadataAvailable) {
  GURL url("https://foo.com");
  SetUpNoMetadataExpectations(url);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  GlicPageContextEligibilityObserver* observer = GetObserver();
  ASSERT_TRUE(observer);

  base::test::TestFuture<bool> future;
  observer->GetEligibility(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  EXPECT_TRUE(future.Get());
}

IN_PROC_BROWSER_TEST_P(GlicPageContextEligibilityObserverBrowserTest,
                       GetEligibility_MetadataAvailable_PageEligible) {
  GlicPageContextEligibilityObserver* observer = GetObserver();
  ASSERT_TRUE(observer);

  GURL eligible_url("https://eligible.com");
  SetUpEligibilityExpectations(eligible_url, /*is_eligible=*/true);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), eligible_url));

  base::test::TestFuture<bool> future;
  observer->GetEligibility(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  EXPECT_TRUE(future.Get());
}

IN_PROC_BROWSER_TEST_P(GlicPageContextEligibilityObserverBrowserTest,
                       GetEligibility_MetadataAvailable_MultiplePageLoads) {
  GlicPageContextEligibilityObserver* observer = GetObserver();
  ASSERT_TRUE(observer);

  {
    GURL first_url("https://eligible.com");
    SetUpEligibilityExpectations(first_url, /*is_eligible=*/true);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), first_url));

    base::test::TestFuture<bool> future;
    observer->GetEligibility(future.GetCallback());
    ASSERT_TRUE(future.Wait());

    EXPECT_TRUE(future.Get());
  }

  if (HasMockOptimizationGuideKeyedService()) {
    testing::Mock::VerifyAndClearExpectations(
        &mock_optimization_guide_keyed_service());
  }

  GURL second_url("https://ineligible.com");

  {
    SetUpEligibilityExpectations(second_url, /*is_eligible=*/false);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), second_url));

    base::test::TestFuture<bool> future;
    observer->GetEligibility(future.GetCallback());
    ASSERT_TRUE(future.Wait());

    EXPECT_FALSE(future.Get());
  }

  if (HasMockOptimizationGuideKeyedService()) {
    testing::Mock::VerifyAndClearExpectations(
        &mock_optimization_guide_keyed_service());
  }

  {
    // Do not set expectations. Expect that the callback fulfilled by cache
    // state.

    base::test::TestFuture<bool> future;
    observer->GetEligibility(future.GetCallback());
    ASSERT_TRUE(future.Wait());

    EXPECT_FALSE(future.Get());
  }
}

class GlicPageContextEligibilityMetadataAsFalseBrowserTest
    : public GlicPageContextEligibilityObserverBrowserTest {
 public:
  void InitializeFeatureList() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{features::kGlic, {}},
         {features::kTabstripComboButton, {}},
         {features::kGlicRollout, {}},
         {
             features::kGlicPageContextEligibility,
             {
                 {"glic-page-context-eligibility-allow-no-metadata", "false"},
             },
         }},
        /*disabled_features=*/{});
  }
};

INSTANTIATE_TEST_SUITE_P(OptGuidePermissions,
                         GlicPageContextEligibilityMetadataAsFalseBrowserTest,
                         ::testing::Values(false, true));

IN_PROC_BROWSER_TEST_P(GlicPageContextEligibilityMetadataAsFalseBrowserTest,
                       GetEligibility_NoMetadataAvailable) {
  GURL url("https://foo.com");
  SetUpNoMetadataExpectations(url);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  GlicPageContextEligibilityObserver* observer = GetObserver();
  ASSERT_TRUE(observer);

  base::test::TestFuture<bool> future;
  observer->GetEligibility(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  EXPECT_FALSE(future.Get());
}

}  // namespace glic
