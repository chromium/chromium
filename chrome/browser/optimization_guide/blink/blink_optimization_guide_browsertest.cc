// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "chrome/browser/optimization_guide/blink/blink_optimization_guide_inquirer.h"
#include "chrome/browser/optimization_guide/blink/blink_optimization_guide_web_contents_observer.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/delay_async_script_execution_metadata.pb.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/notification_types.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/prerender_test_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/features.h"

namespace optimization_guide {

// The base class of various browser tests for the Blink optimization guide.
// This provides the common test utilities.
class BlinkOptimizationGuideBrowserTestBase : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    InProcessBrowserTest::TearDownOnMainThread();
  }

  BlinkOptimizationGuideWebContentsObserver* GetObserverForActiveWebContents() {
    return content::WebContentsUserData<
        BlinkOptimizationGuideWebContentsObserver>::
        FromWebContents(browser()->tab_strip_model()->GetActiveWebContents());
  }

  BlinkOptimizationGuideInquirer* GetCurrentInquirer() {
    return GetObserverForActiveWebContents()->current_inquirer();
  }

  GURL GetURLWithMockHost(const std::string& relative_url) const {
    // The optimization guide service doesn't work with the localhost. Instead,
    // resolve the relative url with the mock host.
    return embedded_test_server()->GetURL("mock.host", relative_url);
  }
};

// The BlinkOptimizationGuideBrowserTest tests common behavior of optimization
// types for Blink (e.g., DELAY_ASYNC_SCRIPT_EXECUTION).
//
// This is designed to be optimization type independent. Add optimization type
// specific things to helper functions like ConstructionMetadata() instead of
// in the test body.
//
// To add a new optimization type, add cases to all of the switch statements in
// this class that handle optimization-specific values, so the existing tests
// work for the new optimization type. Specifically for the Ukm test:
//   - Add a new metric under the Perfect Heuristics Ukm event, and log it when
//     the feature is exercised.
//   - Add a new test file to chrome/test/data/optimization_guide/ that
//     exercises the optimization (when enabled). The test should also expose a
//     `WaitForOptimizationToFinish()` async function that lets the Ukm test
//     know when to destroy the WebContents, and test that the Ukm was logged
//     correctly. See
//     chrome/test/data/optimization_guide/delay-async-script-execution.html as
//     an example.
class BlinkOptimizationGuideBrowserTest
    : public BlinkOptimizationGuideBrowserTestBase,
      public testing::WithParamInterface<
          std::tuple<proto::OptimizationType, bool>> {
 public:
  void SetUpOnMainThread() override {
    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
    BlinkOptimizationGuideBrowserTestBase::SetUpOnMainThread();
  }

  BlinkOptimizationGuideBrowserTest() {
    std::vector<base::test::ScopedFeatureList::FeatureAndParams>
        enabled_features{{features::kOptimizationHints, {{}}}};
    std::vector<base::Feature> disabled_features;

    // Initialize feature flags based on the optimization type.
    switch (GetOptimizationType()) {
      case proto::OptimizationType::DELAY_ASYNC_SCRIPT_EXECUTION:
        if (IsFeatureFlagEnabled()) {
          std::map<std::string, std::string> parameters;
          parameters["delay_type"] = "use_optimization_guide";
          enabled_features.emplace_back(
              blink::features::kDelayAsyncScriptExecution, parameters);
        } else {
          disabled_features.push_back(
              blink::features::kDelayAsyncScriptExecution);
        }
        break;
      default:
        NOTREACHED();
        break;
    }

    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                       disabled_features);
  }

  // Constructs a fake optimization metadata based on the optimization type.
  OptimizationMetadata ConstructMetadata() const {
    OptimizationMetadata optimization_guide_metadata;
    switch (GetOptimizationType()) {
      case proto::OptimizationType::DELAY_ASYNC_SCRIPT_EXECUTION: {
        proto::DelayAsyncScriptExecutionMetadata metadata;
        metadata.set_delay_type(
            proto::PerfectHeuristicsDelayType::DELAY_TYPE_FINISHED_PARSING);
        optimization_guide_metadata.SetAnyMetadataForTesting(metadata);
        break;
      }
      default:
        NOTREACHED();
        break;
    }
    return optimization_guide_metadata;
  }

  // Returns the optimization type provided as the gtest parameter.
  proto::OptimizationType GetOptimizationType() const {
    return std::get<0>(GetParam());
  }

  // Returns true if the feature flag for the optimization type is enabled. If
  // the optimization type doesn't have the feature flag, returns true. See
  // comments on instantiation for details.
  bool IsFeatureFlagEnabled() const { return std::get<1>(GetParam()); }

  // Returns true if the hints for the optimization type is available.
  bool CheckIfHintsAvailable(
      blink::mojom::BlinkOptimizationGuideHints& hints) const {
    switch (GetOptimizationType()) {
      case proto::OptimizationType::DELAY_ASYNC_SCRIPT_EXECUTION:
        if (hints.delay_async_script_execution_hints) {
          EXPECT_EQ(blink::mojom::DelayAsyncScriptExecutionDelayType::
                        kFinishedParsing,
                    hints.delay_async_script_execution_hints->delay_type);
        }
        return !!hints.delay_async_script_execution_hints;
      default:
        NOTREACHED();
        return false;
    }
  }

  GURL GetExperimentSpecificURL() const {
    switch (GetOptimizationType()) {
      case proto::OptimizationType::DELAY_ASYNC_SCRIPT_EXECUTION:
        return GetURLWithMockHost(
            "/optimization_guide/delay-async-script-execution.html");
      default:
        NOTREACHED();
        return GURL();
    }
  }

  void CheckUkmEntryForOptimization() {
    const auto& entries = test_ukm_recorder_->GetEntriesByName(
        ukm::builders::PerfectHeuristics::kEntryName);
    if (IsFeatureFlagEnabled()) {
      switch (GetOptimizationType()) {
        case proto::OptimizationType::DELAY_ASYNC_SCRIPT_EXECUTION:
          EXPECT_EQ(1u, entries.size());
          test_ukm_recorder_->ExpectEntryMetric(
              entries.front(),
              ukm::builders::PerfectHeuristics::
                  kdelay_async_script_execution_before_finished_parsingName,
              1u);
          break;
        default:
          NOTREACHED();
          return;
      }
    } else {
      EXPECT_EQ(0u, entries.size());
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;
};

// Instantiates test cases for each optimization type.
INSTANTIATE_TEST_SUITE_P(
    All,
    BlinkOptimizationGuideBrowserTest,
    testing::Combine(
        // The optimization type.
        testing::Values(proto::OptimizationType::DELAY_ASYNC_SCRIPT_EXECUTION),
        // Whether the feature flag for the optimization type is enabled.
        testing::Bool()));

IN_PROC_BROWSER_TEST_P(BlinkOptimizationGuideBrowserTest, Basic) {
  // Set up a fake optimization hints for simple.html.
  OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
      ->AddHintForTesting(GetURLWithMockHost("/simple.html"),
                          GetOptimizationType(), ConstructMetadata());

  // Navigation to the URL should see the hints as long as the optimization type
  // is enabled.
  ui_test_utils::NavigateToURL(browser(), GetURLWithMockHost("/simple.html"));
  {
    blink::mojom::BlinkOptimizationGuideHintsPtr hints =
        GetCurrentInquirer()->GetHints();
    if (IsFeatureFlagEnabled()) {
      EXPECT_TRUE(CheckIfHintsAvailable(*hints));
    } else {
      EXPECT_FALSE(CheckIfHintsAvailable(*hints));
    }
  }

  // Navigation to the different URL shouldn't see the hints.
  ui_test_utils::NavigateToURL(browser(),
                               GetURLWithMockHost("/simple.html?different"));
  {
    blink::mojom::BlinkOptimizationGuideHintsPtr hints =
        GetCurrentInquirer()->GetHints();
    EXPECT_FALSE(CheckIfHintsAvailable(*hints));
  }

  // Navigation to the URL again should see the same hints as long as the
  // optimization guide is enabled.
  ui_test_utils::NavigateToURL(browser(), GetURLWithMockHost("/simple.html"));
  {
    blink::mojom::BlinkOptimizationGuideHintsPtr hints =
        GetCurrentInquirer()->GetHints();
    if (IsFeatureFlagEnabled()) {
      EXPECT_TRUE(CheckIfHintsAvailable(*hints));
    } else {
      EXPECT_FALSE(CheckIfHintsAvailable(*hints));
    }
  }
}

IN_PROC_BROWSER_TEST_P(BlinkOptimizationGuideBrowserTest, NoMetadata) {
  // Set up a fake optimization hints without metadata for simple.html.
  OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
      ->AddHintForTesting(GetURLWithMockHost("/simple.html"),
                          GetOptimizationType(), absl::nullopt);

  // Navigation to the URL shouldn't see the hints.
  ui_test_utils::NavigateToURL(browser(), GetURLWithMockHost("/simple.html"));
  blink::mojom::BlinkOptimizationGuideHintsPtr hints =
      GetCurrentInquirer()->GetHints();
  EXPECT_FALSE(CheckIfHintsAvailable(*hints));
}

IN_PROC_BROWSER_TEST_P(BlinkOptimizationGuideBrowserTest, Ukm) {
  OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
      ->AddHintForTesting(GetExperimentSpecificURL(), GetOptimizationType(),
                          ConstructMetadata());

  EXPECT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GetExperimentSpecificURL()));
  EXPECT_EQ("DONE", EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                           "WaitForOptimizationToFinish()"));

  blink::mojom::BlinkOptimizationGuideHintsPtr hints =
      GetCurrentInquirer()->GetHints();
  if (IsFeatureFlagEnabled()) {
    EXPECT_TRUE(CheckIfHintsAvailable(*hints));
  } else {
    EXPECT_FALSE(CheckIfHintsAvailable(*hints));
  }

  // Force navigation to another page, which should force logging of histograms
  // persisted at the end of the page load lifetime.
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));

  CheckUkmEntryForOptimization();
}

// Tests behavior when the optimization guide service is disabled.
class BlinkOptimizationGuideDisabledBrowserTest
    : public BlinkOptimizationGuideBrowserTestBase {
 public:
  BlinkOptimizationGuideDisabledBrowserTest() {
    // Disable the optimization guide service.
    scoped_feature_list_.InitAndDisableFeature(features::kOptimizationHints);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(BlinkOptimizationGuideDisabledBrowserTest,
                       OptimizationGuideIsDisabled) {
  // The optimization guide service shouldn't be available.
  EXPECT_FALSE(OptimizationGuideKeyedServiceFactory::GetForProfile(
      browser()->profile()));

  ui_test_utils::NavigateToURL(browser(), GetURLWithMockHost("/simple.html"));

  // Navigation started, but the web contents observer for the Blink
  // optimization guide shouldn't be created.
  EXPECT_FALSE(GetObserverForActiveWebContents());
}

class BlinkOptimizationGuidePrerenderBrowserTest
    : public BlinkOptimizationGuideBrowserTest {
 public:
  BlinkOptimizationGuidePrerenderBrowserTest() = default;
  ~BlinkOptimizationGuidePrerenderBrowserTest() override = default;
  BlinkOptimizationGuidePrerenderBrowserTest(
      const BlinkOptimizationGuidePrerenderBrowserTest&) = delete;

  BlinkOptimizationGuidePrerenderBrowserTest& operator=(
      const BlinkOptimizationGuidePrerenderBrowserTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    prerender_helper_ = std::make_unique<content::test::PrerenderTestHelper>(
        base::BindRepeating(
            &BlinkOptimizationGuidePrerenderBrowserTest::GetWebContents,
            base::Unretained(this)));
  }

  void SetUpOnMainThread() override {
    // TODO(crbug.com/846380): move BlinkOptimizationGuideBrowserTest
    // ScopedFeatureList init to the constructor.  We set up here rather than in
    // SetUp since the prerender_helper_ is created in SetUpCommandLine to
    // ensure correct construction/destruction order of the ScopedFeatureList in
    // the PrerenderTestHelper and the BlinkOptimizationGuideBrowserTest.
    prerender_helper_->SetUp(embedded_test_server());
    BlinkOptimizationGuideBrowserTest::SetUpOnMainThread();
  }

  content::test::PrerenderTestHelper& prerender_test_helper() {
    return *prerender_helper_;
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 private:
  std::unique_ptr<content::test::PrerenderTestHelper> prerender_helper_;
};

// Instantiates test cases for each optimization type.
INSTANTIATE_TEST_SUITE_P(
    All,
    BlinkOptimizationGuidePrerenderBrowserTest,
    testing::Combine(
        // The optimization type.
        testing::Values(proto::OptimizationType::DELAY_ASYNC_SCRIPT_EXECUTION),
        // Whether the feature flag for the optimization type is enabled.
        testing::Bool()));

IN_PROC_BROWSER_TEST_P(BlinkOptimizationGuidePrerenderBrowserTest,
                       PrerenderingDontCreateBlinkOptimizationGuideInquirer) {
  GURL initial_url = GetURLWithMockHost("/empty.html");
  GURL prerender_url = GetURLWithMockHost("/simple.html");
  ASSERT_NE(ui_test_utils::NavigateToURL(browser(), initial_url), nullptr);
  auto weak_prev_inquirer = GetCurrentInquirer()->GetWeakPtrForTesting();
  EXPECT_TRUE(weak_prev_inquirer);

  // Load a page in the prerender. This shouldn't create a new inquirer yet.
  int host_id = prerender_test_helper().AddPrerender(prerender_url);
  content::test::PrerenderHostObserver host_observer(*GetWebContents(),
                                                     host_id);
  EXPECT_FALSE(host_observer.was_activated());
  EXPECT_TRUE(weak_prev_inquirer);

  // Activate the prerendered page. This should create a new inquirer.
  prerender_test_helper().NavigatePrimaryPage(prerender_url);
  EXPECT_TRUE(host_observer.was_activated());
  EXPECT_FALSE(weak_prev_inquirer);
  EXPECT_TRUE(GetCurrentInquirer()->GetWeakPtrForTesting());
}

}  // namespace optimization_guide
