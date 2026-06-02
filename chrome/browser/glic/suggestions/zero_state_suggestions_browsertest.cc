// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <tuple>

#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/suggestions/contextual_cueing_features.h"
#include "chrome/browser/glic/suggestions/contextual_cueing_service.h"
#include "chrome/browser/glic/suggestions/contextual_cueing_service_factory.h"
#include "chrome/browser/glic/suggestions/zero_state_suggestions_page_data.h"
#include "chrome/browser/optimization_guide/browser_test_util.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/optimization_guide/core/model_execution/remote_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/proto/contextual_cueing_metadata.pb.h"
#include "components/optimization_guide/proto/features/zero_state_suggestions.pb.h"
#include "components/page_content_annotations/core/page_content_annotations_features.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"

namespace glic {

enum class ContentExtraction {
  kFetchInnerTextOnly,
  kFetchAnnotatedPageContentOnly,
  kFetchInnerTextAndAnnotatedPageContent,
};

class ZeroStateSuggestionsBrowserTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<
          std::tuple<ContentExtraction, bool>> {
 public:
  ZeroStateSuggestionsBrowserTest() {
    base::FieldTrialParams zss_params;
    switch (GetContentExtraction()) {
      case ContentExtraction::kFetchInnerTextOnly:
        zss_params = {{"ZSSExtractInnerText", "true"},
                      {"ZSSExtractAnnotatedPageContent", "false"}};
        break;
      case ContentExtraction::kFetchAnnotatedPageContentOnly:
        zss_params = {{"ZSSExtractInnerText", "false"},
                      {"ZSSExtractAnnotatedPageContent", "true"}};
        break;
      case ContentExtraction::kFetchInnerTextAndAnnotatedPageContent:
        zss_params = {{"ZSSExtractInnerText", "true"},
                      {"ZSSExtractAnnotatedPageContent", "true"}};
        break;
    }
    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {glic::kContextualCueing, {}},
        {glic::kGlicZeroStateSuggestions, zss_params}};
    std::vector<base::test::FeatureRef> disabled_features;

    if (IsPageSettledMonitorEnabled()) {
      enabled_features.push_back(
          {page_content_annotations::features::
               kPageContentExtractionUsingPageSettledMonitor,
           {{"capture_delay", "3s"}}});
    } else {
      disabled_features.push_back(
          page_content_annotations::features::
              kPageContentExtractionUsingPageSettledMonitor);
    }

    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                       disabled_features);
  }

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->Start());
    url_ = embedded_test_server()->GetURL("/optimization_guide/zss_page.html");
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    // Override glic tab context sharing to be always on.
    browser()->profile()->GetPrefs()->SetBoolean(
        glic::prefs::kGlicTabContextEnabled, true);
  }

  void DisableOptimizationPermissionCheck() {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        optimization_guide::switches::
            kDisableCheckingUserPermissionsForTesting);
  }

  GURL url() { return url_; }

  ContentExtraction GetContentExtraction() const {
    return std::get<0>(GetParam());
  }

  bool IsPageSettledMonitorEnabled() const { return std::get<1>(GetParam()); }

  void SetUpOnDemandHints(const GURL& url,
                          bool allow_contextual,
                          const std::vector<std::string>& suggestions) {
    optimization_guide::proto::GlicZeroStateSuggestionsMetadata metadata;
    metadata.set_contextual_suggestions_eligible(allow_contextual);
    *metadata.mutable_contextual_suggestions() = {suggestions.begin(),
                                                  suggestions.end()};
    optimization_guide::OptimizationGuideDecisionWithMetadata
        decision_with_metadata;
    decision_with_metadata.decision =
        optimization_guide::OptimizationGuideDecision::kTrue;
    decision_with_metadata.metadata.set_any_metadata(
        optimization_guide::AnyWrapProto(metadata));

    OptimizationGuideKeyedServiceFactory::GetInstance()
        ->GetForProfile(browser()->profile())
        ->AddOnDemandHintForTesting(
            url, optimization_guide::proto::GLIC_ZERO_STATE_SUGGESTIONS,
            decision_with_metadata);
  }

  void SetUpHints(const GURL& url,
                  bool allow_contextual,
                  const std::vector<std::string>& suggestions) {
    optimization_guide::proto::GlicZeroStateSuggestionsMetadata metadata;
    metadata.set_contextual_suggestions_eligible(allow_contextual);
    *metadata.mutable_contextual_suggestions() = {suggestions.begin(),
                                                  suggestions.end()};
    optimization_guide::OptimizationMetadata og_metadata;
    og_metadata.set_any_metadata(optimization_guide::AnyWrapProto(metadata));

    OptimizationGuideKeyedServiceFactory::GetInstance()
        ->GetForProfile(browser()->profile())
        ->AddHintForTesting(
            url, optimization_guide::proto::GLIC_ZERO_STATE_SUGGESTIONS,
            og_metadata);
  }

  void SetUpHintsNoResult(const GURL& url) {
    OptimizationGuideKeyedServiceFactory::GetInstance()
        ->GetForProfile(browser()->profile())
        ->AddHintForTesting(
            url, optimization_guide::proto::GLIC_ZERO_STATE_SUGGESTIONS,
            std::nullopt);
  }

  void SetUpSuccessfulModelExecution() {
    optimization_guide::proto::ZeroStateSuggestionsResponse response;
    response.add_suggestions()->set_label("suggestion 1");
    response.add_suggestions()->set_label("suggestion 2");
    response.add_suggestions()->set_label("suggestion 3");
    std::string serialized_metadata;
    response.SerializeToString(&serialized_metadata);
    optimization_guide::proto::Any any_result;
    any_result.set_type_url(
        base::StrCat({"type.googleapis.com/", response.GetTypeName()}));
    any_result.set_value(serialized_metadata);

    OptimizationGuideKeyedServiceFactory::GetInstance()
        ->GetForProfile(browser()->profile())
        ->AddExecutionResultForTesting(
            optimization_guide::ModelBasedCapabilityKey::kZeroStateSuggestions,
            optimization_guide::OptimizationGuideModelExecutionResult(
                any_result, nullptr));
  }

  void SetUpEmptyModelExecutionResult() {
    optimization_guide::proto::Any any_result;
    OptimizationGuideKeyedServiceFactory::GetInstance()
        ->GetForProfile(browser()->profile())
        ->AddExecutionResultForTesting(
            optimization_guide::ModelBasedCapabilityKey::kZeroStateSuggestions,
            optimization_guide::OptimizationGuideModelExecutionResult(
                any_result, nullptr));
  }

 private:
  GURL url_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    WithContentExtraction,
    ZeroStateSuggestionsBrowserTest,
    ::testing::Combine(
        ::testing::Values(
            ContentExtraction::kFetchInnerTextOnly,
            ContentExtraction::kFetchAnnotatedPageContentOnly,
            ContentExtraction::kFetchInnerTextAndAnnotatedPageContent),
        ::testing::Bool()),
    [](const testing::TestParamInfo<std::tuple<ContentExtraction, bool>>&
           info) {
      std::string extraction_name;
      switch (std::get<0>(info.param)) {
        case ContentExtraction::kFetchInnerTextOnly:
          extraction_name = "FetchInnerTextOnly";
          break;
        case ContentExtraction::kFetchAnnotatedPageContentOnly:
          extraction_name = "FetchAnnotatedPageContentOnly";
          break;
        case ContentExtraction::kFetchInnerTextAndAnnotatedPageContent:
          extraction_name = "FetchInnerTextAndAnnotatedPageContent";
          break;
      }
      return base::StrCat({extraction_name, "_",
                           std::get<1>(info.param)
                               ? "PageSettledMonitorEnabled"
                               : "PageSettledMonitorDisabled"});
    });

IN_PROC_BROWSER_TEST_P(ZeroStateSuggestionsBrowserTest, BasicFlow) {
  base::HistogramTester histogram_tester;

  DisableOptimizationPermissionCheck();
  SetUpSuccessfulModelExecution();

  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  SetUpHints(url(), /*allow_contextual=*/true, /*suggestions=*/{});
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url()));

  base::test::TestFuture<std::vector<std::string>> future;

  ContextualCueingServiceFactory::GetForProfile(browser()->profile())
      ->GetContextualGlicZeroStateSuggestionsForFocusedTab(
          web_contents, /*is_fre=*/false, /*supported_tools=*/{},
          future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(3u, future.Get().size());
  EXPECT_EQ("suggestion 1", future.Get()[0]);
  EXPECT_EQ("suggestion 2", future.Get()[1]);
  EXPECT_EQ("suggestion 3", future.Get()[2]);
  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.ZeroStateSuggestions.ContextExtractionDone", true, 1);
  histogram_tester.ExpectTotalCount(
      "ContextualCueing.GlicSuggestions.MesFetchLatency", 1);

  histogram_tester.ExpectTotalCount(
      "ContextualCueing.GlicSuggestions.SuggestionsFetchLatency."
      "ValidSuggestions",
      1);
  histogram_tester.ExpectTotalCount(
      "ContextualCueing.GlicSuggestions.SuggestionsFetchLatency."
      "ValidSuggestions.Reengagement",
      1);
  histogram_tester.ExpectTotalCount(
      "ContextualCueing.GlicSuggestions.SuggestionsFetchLatency."
      "ValidSuggestions.FRE",
      0);
  histogram_tester.ExpectTotalCount(
      "ContextualCueing.GlicSuggestions.SuggestionsFetchLatency."
      "EmptySuggestions",
      0);
  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.GlicSuggestions.FocusedTabEligibleForSuggestions", true,
      1);
  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.GlicSuggestions.PageContextIneligibilityReason",
      PageContextIneligibilityType::kNone, 1);
}

IN_PROC_BROWSER_TEST_P(ZeroStateSuggestionsBrowserTest,
                       HoldsOntoSuccessiveRequests) {
  base::HistogramTester histogram_tester;

  DisableOptimizationPermissionCheck();

  SetUpHints(url(), /*allow_contextual=*/true, /*suggestions=*/{});
  SetUpSuccessfulModelExecution();
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url()));

  ContextualCueingService* contextual_cueing_service =
      ContextualCueingServiceFactory::GetForProfile(browser()->profile());

  // Set up two concurrent calls (simulates mouse down and then on load).
  base::test::TestFuture<std::vector<std::string>> future;
  contextual_cueing_service->GetContextualGlicZeroStateSuggestionsForFocusedTab(
      web_contents, /*is_fre=*/false, /*supported_tools=*/{},
      future.GetCallback());
  base::test::TestFuture<std::vector<std::string>> future2;
  contextual_cueing_service->GetContextualGlicZeroStateSuggestionsForFocusedTab(
      web_contents, /*is_fre=*/false, /*supported_tools=*/{},
      future2.GetCallback());

  // Wait until page is extracted.
  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester,
      "ContextualCueing.ZeroStateSuggestions.ContextExtractionDone", 1);

  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.ZeroStateSuggestions.ContextExtractionDone", true, 1);

  // Both calls should be fulfilled using the same response.
  ASSERT_TRUE(future.Wait());
  ASSERT_TRUE(future2.Wait());

  EXPECT_EQ(3u, future.Get().size());
  EXPECT_EQ("suggestion 1", future.Get()[0]);
  EXPECT_EQ("suggestion 2", future.Get()[1]);
  EXPECT_EQ("suggestion 3", future.Get()[2]);

  EXPECT_EQ(3u, future2.Get().size());
  EXPECT_EQ("suggestion 1", future2.Get()[0]);
  EXPECT_EQ("suggestion 2", future2.Get()[1]);
  EXPECT_EQ("suggestion 3", future2.Get()[2]);

  histogram_tester.ExpectTotalCount(
      "ContextualCueing.GlicSuggestions.MesFetchLatency", 1);
}

IN_PROC_BROWSER_TEST_P(ZeroStateSuggestionsBrowserTest,
                       CreateDataDoesNotFetchWithoutExplicitCall) {
  base::HistogramTester histogram_tester;

  DisableOptimizationPermissionCheck();

  SetUpHintsNoResult(url());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url()));

  ZeroStateSuggestionsPageData::CreateForPage(web_contents->GetPrimaryPage());

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester,
      "ContextualCueing.ZeroStateSuggestions.ContextExtractionDone", 1);

  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.ZeroStateSuggestions.ContextExtractionDone", true, 1);
  histogram_tester.ExpectTotalCount(
      "ContextualCueing.GlicSuggestions.MesFetchLatency", 0);
}

IN_PROC_BROWSER_TEST_P(ZeroStateSuggestionsBrowserTest,
                       ContextualSuggestionsNotAllowed) {
  base::HistogramTester histogram_tester;

  DisableOptimizationPermissionCheck();

  SetUpHints(url(), /*allow_contextual=*/false, /*suggestions=*/{});
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url()));

  base::test::TestFuture<std::vector<std::string>> future;
  ContextualCueingServiceFactory::GetForProfile(browser()->profile())
      ->GetContextualGlicZeroStateSuggestionsForFocusedTab(
          web_contents, /*is_fre=*/true, /*supported_tools=*/{},
          future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_TRUE(future.Get().empty());

  histogram_tester.ExpectTotalCount(
      "ContextualCueing.GlicSuggestions.SuggestionsFetchLatency."
      "EmptySuggestions",
      1);
  histogram_tester.ExpectTotalCount(
      "ContextualCueing.GlicSuggestions.SuggestionsFetchLatency."
      "EmptySuggestions.Reengagement",
      0);
  histogram_tester.ExpectTotalCount(
      "ContextualCueing.GlicSuggestions.SuggestionsFetchLatency."
      "EmptySuggestions.FRE",
      1);
  histogram_tester.ExpectTotalCount(
      "ContextualCueing.GlicSuggestions.SuggestionsFetchLatency."
      "ValidSuggestions",
      0);
  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.GlicSuggestions.PageContextIneligibilityReason",
      PageContextIneligibilityType::kOptimizationMetadata, 1);
  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.GlicSuggestions.PageContextIneligibilityReason.FRE",
      PageContextIneligibilityType::kOptimizationMetadata, 1);
}

IN_PROC_BROWSER_TEST_P(ZeroStateSuggestionsBrowserTest, NoResultFromHints) {
  DisableOptimizationPermissionCheck();

  // Assumes page is eligible for contextual suggestions without hints result.
  SetUpHintsNoResult(url());
  SetUpSuccessfulModelExecution();
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url()));

  base::test::TestFuture<std::vector<std::string>> future;
  ContextualCueingServiceFactory::GetForProfile(browser()->profile())
      ->GetContextualGlicZeroStateSuggestionsForFocusedTab(
          web_contents, /*is_fre=*/false, /*supported_tools=*/{},
          future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(3u, future.Get().size());
  EXPECT_EQ("suggestion 1", future.Get()[0]);
  EXPECT_EQ("suggestion 2", future.Get()[1]);
  EXPECT_EQ("suggestion 3", future.Get()[2]);
}

IN_PROC_BROWSER_TEST_P(ZeroStateSuggestionsBrowserTest, CacheBehavior) {
  DisableOptimizationPermissionCheck();

  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url()));

  // Set up initial flow.
  {
    base::test::TestFuture<std::vector<std::string>> future;
    base::HistogramTester histogram_tester;

    SetUpHints(url(), /*allow_contextual=*/true, /*suggestions=*/{});
    SetUpSuccessfulModelExecution();

    ContextualCueingServiceFactory::GetForProfile(browser()->profile())
        ->GetContextualGlicZeroStateSuggestionsForFocusedTab(
            web_contents, /*is_fre=*/false, /*supported_tools=*/{},
            future.GetCallback());
    ASSERT_TRUE(future.Wait());
    EXPECT_EQ(3u, future.Get().size());
    EXPECT_EQ("suggestion 1", future.Get()[0]);
    EXPECT_EQ("suggestion 2", future.Get()[1]);
    EXPECT_EQ("suggestion 3", future.Get()[2]);
    histogram_tester.ExpectTotalCount(
        "ContextualCueing.GlicSuggestions.MesFetchLatency", 1);
  }

  // Make sure model execution not called.
  {
    base::HistogramTester histogram_tester;
    base::test::TestFuture<std::vector<std::string>> future;

    ContextualCueingServiceFactory::GetForProfile(browser()->profile())
        ->GetContextualGlicZeroStateSuggestionsForFocusedTab(
            web_contents, /*is_fre=*/false, /*supported_tools=*/{},
            future.GetCallback());
    ASSERT_TRUE(future.Wait());
    EXPECT_EQ(3u, future.Get().size());
    EXPECT_EQ("suggestion 1", future.Get()[0]);
    EXPECT_EQ("suggestion 2", future.Get()[1]);
    EXPECT_EQ("suggestion 3", future.Get()[2]);
    histogram_tester.ExpectTotalCount(
        "ContextualCueing.GlicSuggestions.MesFetchLatency", 0);
  }
}

IN_PROC_BROWSER_TEST_P(ZeroStateSuggestionsBrowserTest, CacheBehaviorError) {
  DisableOptimizationPermissionCheck();

  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url()));

  // Set up initial flow.
  {
    base::HistogramTester histogram_tester;
    SetUpHints(url(), /*allow_contextual=*/true, /*suggestions=*/{});
    base::test::TestFuture<std::vector<std::string>> future;

    // Set up non-transient error.
    optimization_guide::proto::ErrorResponse error_response;
    error_response.set_error_state(
        optimization_guide::proto::ErrorState::
            ERROR_STATE_INTERNAL_SERVER_ERROR_NO_RETRY);

    OptimizationGuideKeyedServiceFactory::GetInstance()
        ->GetForProfile(browser()->profile())
        ->AddExecutionResultForTesting(
            optimization_guide::ModelBasedCapabilityKey::kZeroStateSuggestions,
            optimization_guide::OptimizationGuideModelExecutionResult(
                base::unexpected(
                    optimization_guide::OptimizationGuideModelExecutionError::
                        FromModelExecutionServerError(error_response)),
                nullptr));

    ContextualCueingServiceFactory::GetForProfile(browser()->profile())
        ->GetContextualGlicZeroStateSuggestionsForFocusedTab(
            web_contents, /*is_fre=*/false, /*supported_tools=*/{},
            future.GetCallback());
    ASSERT_TRUE(future.Wait());
    EXPECT_TRUE(future.Get().empty());
    histogram_tester.ExpectTotalCount(
        "ContextualCueing.GlicSuggestions.MesFetchLatency", 1);
  }

  // Make sure model execution not called.
  {
    base::HistogramTester histogram_tester;

    base::test::TestFuture<std::vector<std::string>> future;

    ContextualCueingServiceFactory::GetForProfile(browser()->profile())
        ->GetContextualGlicZeroStateSuggestionsForFocusedTab(
            web_contents, /*is_fre=*/false, /*supported_tools=*/{},
            future.GetCallback());
    ASSERT_TRUE(future.Wait());
    EXPECT_TRUE(future.Get().empty());
    histogram_tester.ExpectTotalCount(
        "ContextualCueing.GlicSuggestions.MesFetchLatency", 0);
  }
}

IN_PROC_BROWSER_TEST_P(ZeroStateSuggestionsBrowserTest,
                       NonMSBBFlowContextualNotAllowed) {
  base::HistogramTester histogram_tester;

  SetUpOnDemandHints(url(), /*allow_contextual=*/false, /*suggestions=*/{});
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url()));

  base::test::TestFuture<std::vector<std::string>> future;

  ContextualCueingServiceFactory::GetForProfile(browser()->profile())
      ->GetContextualGlicZeroStateSuggestionsForFocusedTab(
          web_contents, /*is_fre=*/false, /*supported_tools=*/{},
          future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_TRUE(future.Get().empty());

  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.GlicSuggestions.PageContextIneligibilityReason",
      PageContextIneligibilityType::kOptimizationMetadata, 1);
  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.GlicSuggestions.PageContextIneligibilityReason."
      "Reengagement",
      PageContextIneligibilityType::kOptimizationMetadata, 1);
}

IN_PROC_BROWSER_TEST_P(ZeroStateSuggestionsBrowserTest,
                       NonMSBBFlowContextualNotAllowedForAllPinnedTabs) {
  base::HistogramTester histogram_tester;

  SetUpOnDemandHints(url(), /*allow_contextual=*/false, /*suggestions=*/{});
  GURL url2 =
      embedded_test_server()->GetURL("/optimization_guide/hellow_world.html");
  SetUpOnDemandHints(url2, /*allow_contextual=*/false, /*suggestions=*/{});
  SetUpSuccessfulModelExecution();

  // Navigate in one tab.
  auto* initial_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url()));

  // Navigate to a new URL in a second tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url2, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  auto* web_contents2 = browser()->tab_strip_model()->GetActiveWebContents();

  base::test::TestFuture<std::vector<std::string>> future;

  // This is true since we do not know the answer yet.
  EXPECT_TRUE(
      ContextualCueingServiceFactory::GetForProfile(browser()->profile())
          ->GetContextualGlicZeroStateSuggestionsForPinnedTabs(
              {initial_web_contents, web_contents2}, /*is_fre=*/false,
              /*supported_tools=*/{}, initial_web_contents,
              future.GetCallback()));
  ASSERT_TRUE(future.Wait());
  EXPECT_TRUE(future.Get().empty());

  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.GlicSuggestions.PageContextIneligibilityReason",
      PageContextIneligibilityType::kOptimizationMetadata, 1);
  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.GlicSuggestions.PageContextIneligibilityReason."
      "Reengagement",
      PageContextIneligibilityType::kOptimizationMetadata, 1);
}

IN_PROC_BROWSER_TEST_P(ZeroStateSuggestionsBrowserTest,
                       NonMSBBFlowContextualAllowedForOnePinnedTab) {
  base::HistogramTester histogram_tester;

  SetUpOnDemandHints(url(), /*allow_contextual=*/true, /*suggestions=*/{});
  GURL url2 =
      embedded_test_server()->GetURL("/optimization_guide/hellow_world.html");
  SetUpOnDemandHints(url2, /*allow_contextual=*/false, /*suggestions=*/{});
  SetUpSuccessfulModelExecution();

  // Navigate in one tab.
  auto* initial_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url()));

  // Navigate to a new URL in a second tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url2, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  auto* web_contents2 = browser()->tab_strip_model()->GetActiveWebContents();

  base::test::TestFuture<std::vector<std::string>> future;

  EXPECT_TRUE(
      ContextualCueingServiceFactory::GetForProfile(browser()->profile())
          ->GetContextualGlicZeroStateSuggestionsForPinnedTabs(
              {initial_web_contents, web_contents2}, /*is_fre=*/false,
              /*supported_tools=*/{}, initial_web_contents,
              future.GetCallback()));
  EXPECT_EQ(3u, future.Get().size());
  EXPECT_EQ("suggestion 1", future.Get()[0]);
  EXPECT_EQ("suggestion 2", future.Get()[1]);
  EXPECT_EQ("suggestion 3", future.Get()[2]);
  histogram_tester.ExpectTotalCount(
      "ContextualCueing.GlicSuggestions.MesFetchLatency", 1);
  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.GlicSuggestions.PinnedTabsEligibleForSuggestions", true,
      1);
}

IN_PROC_BROWSER_TEST_P(ZeroStateSuggestionsBrowserTest,
                       AllPinnedTabsIneligibleForContextual) {
  base::HistogramTester histogram_tester;

  // Open 2 tabs with new tab page.
  chrome::NewTab(browser(), NewTabTypes::kNoUserAction);
  auto* initial_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  chrome::NewTab(browser(), NewTabTypes::kNoUserAction);
  auto* web_contents2 = browser()->tab_strip_model()->GetActiveWebContents();

  SetUpSuccessfulModelExecution();

  base::test::TestFuture<std::vector<std::string>> future;
  EXPECT_FALSE(
      ContextualCueingServiceFactory::GetForProfile(browser()->profile())
          ->GetContextualGlicZeroStateSuggestionsForPinnedTabs(
              {initial_web_contents, web_contents2}, /*is_fre=*/false,
              /*supported_tools=*/{}, nullptr, future.GetCallback()));
  ASSERT_TRUE(future.Wait());
  EXPECT_TRUE(future.Get().empty());

  histogram_tester.ExpectTotalCount(
      "ContextualCueing.ZeroStateSuggestions.ContextExtractionDone", 0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecutionFetcher.RequestStatus."
      "ZeroStateSuggestions",
      0);
  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.GlicSuggestions.PinnedTabsEligibleForSuggestions",
      false, 1);
}

IN_PROC_BROWSER_TEST_P(ZeroStateSuggestionsBrowserTest, BasicPinnedTabsFlow) {
  base::HistogramTester histogram_tester;

  GURL url2 =
      embedded_test_server()->GetURL("/optimization_guide/hellow_world.html");
  SetUpSuccessfulModelExecution();

  // Navigate in one tab.
  auto* initial_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url()));

  // Navigate to a new URL in a second tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url2, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  auto* web_contents2 = browser()->tab_strip_model()->GetActiveWebContents();

  base::test::TestFuture<std::vector<std::string>> future;

  EXPECT_TRUE(
      ContextualCueingServiceFactory::GetForProfile(browser()->profile())
          ->GetContextualGlicZeroStateSuggestionsForPinnedTabs(
              {initial_web_contents, web_contents2}, /*is_fre=*/false,
              /*supported_tools=*/{}, initial_web_contents,
              future.GetCallback()));
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(3u, future.Get().size());
  EXPECT_EQ("suggestion 1", future.Get()[0]);
  EXPECT_EQ("suggestion 2", future.Get()[1]);
  EXPECT_EQ("suggestion 3", future.Get()[2]);
  histogram_tester.ExpectTotalCount(
      "ContextualCueing.GlicSuggestions.MesFetchLatency", 1);
  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.GlicSuggestions.PinnedTabsEligibleForSuggestions", true,
      1);
  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.GlicSuggestions.PageContextIneligibilityReason",
      PageContextIneligibilityType::kNone, 1);
  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.GlicSuggestions.PageContextIneligibilityReason."
      "Reengagement",
      PageContextIneligibilityType::kNone, 1);
}

// =============================================================================
// POC: BFCached attacker page content joined with victim primary-page URL/title
// =============================================================================
//
// ZeroStateSuggestionsPageData is a PageUserData. Its
// ConstructPageContextProto()/GetUrl() read per-WebContents (primary-page)
// state via WebContents::GetLastCommittedURL()/GetTitle() without checking
// whether `page()` is still the primary page. When the bound page has been
// BFCached and the tab now shows a different (victim) primary page, the
// produced ZeroStatePageContext proto carries:
//   url   = victim primary-page URL
//   title = victim primary-page title
//   inner_text / annotated_page_content = ATTACKER page content
//
// In production this fires on the late OptimizationGuide / inner-text callback
// path (page_context_callbacks_.Notify -> ConstructPageContextProto) while the
// attacker page sits in BFCache, and the mismatched proto is uploaded to the
// ZSS model server attributed to the victim origin.
class ZeroStateSuggestionsBFCacheConfusionBrowserTest
    : public InProcessBrowserTest {
 public:
  ZeroStateSuggestionsBFCacheConfusionBrowserTest() {
    // Default-on inner-text extraction; APC off to keep the test deterministic.
    scoped_feature_list_.InitWithFeaturesAndParameters(
        content::GetDefaultEnabledBackForwardCacheFeaturesForTesting(
            {{glic::kContextualCueing, {}},
             {glic::kGlicZeroStateSuggestions,
              {{"ZSSExtractInnerText", "true"},
               {"ZSSExtractAnnotatedPageContent", "false"}}}}),
        content::GetDefaultDisabledBackForwardCacheFeaturesForTesting(
            {page_content_annotations::features::
                 kPageContentExtractionUsingPageSettledMonitor}));
  }

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->Start());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    browser()->profile()->GetPrefs()->SetBoolean(
        glic::prefs::kGlicTabContextEnabled, true);
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        optimization_guide::switches::
            kDisableCheckingUserPermissionsForTesting);
  }

  void SetUpHintsNoResult(const GURL& url) {
    OptimizationGuideKeyedServiceFactory::GetInstance()
        ->GetForProfile(browser()->profile())
        ->AddHintForTesting(
            url, optimization_guide::proto::GLIC_ZERO_STATE_SUGGESTIONS,
            std::nullopt);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ZeroStateSuggestionsBFCacheConfusionBrowserTest,
                       BFCacheRegressionTest) {
  base::HistogramTester histogram_tester;

  const GURL attacker_url = embedded_test_server()->GetURL(
      "attacker.test", "/optimization_guide/zss_attacker_page.html");
  const GURL victim_url = embedded_test_server()->GetURL(
      "victim.test", "/optimization_guide/zss_victim_page.html");

  // Allow the attacker URL's optimization-metadata callback to resolve so that
  // work_done() becomes true and inner_text_result_ is captured (not cleared
  // by the GiveUp() timeout).
  SetUpHintsNoResult(attacker_url);

  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();

  // Step 1: Load the attacker page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), attacker_url));
  ASSERT_EQ(attacker_url, web_contents->GetLastCommittedURL());

  content::RenderFrameHost* attacker_rfh = web_contents->GetPrimaryMainFrame();

  // Step 2: Simulate the user opening Glic on the attacker tab. This is the
  // exact call performed by ContextualCueingService::
  // PrepareToFetchContextualGlicZeroStateSuggestions().
  ZeroStateSuggestionsPageData::CreateForPage(web_contents->GetPrimaryPage());
  base::WeakPtr<ZeroStateSuggestionsPageData> attacker_zss_data =
      ZeroStateSuggestionsPageData::GetForPage(web_contents->GetPrimaryPage())
          ->AsWeakPtr();
  ASSERT_TRUE(attacker_zss_data);

  // Wait for inner_text + optimization metadata to resolve so the attacker
  // body text is captured into `inner_text_result_`.
  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester,
      "ContextualCueing.ZeroStateSuggestions.ContextExtractionDone", 1);

  // Step 3: Attacker navigates the tab cross-origin to the victim page (this
  // is fully attacker-controlled via `location.href = ...`). Use a
  // renderer-initiated navigation to mirror the real attack.
  ASSERT_TRUE(content::NavigateToURLFromRenderer(web_contents, victim_url));
  ASSERT_EQ(victim_url, web_contents->GetLastCommittedURL());

  // The attacker Page (and its PageUserData) survives in BFCache (or, when
  // BFCache is unavailable, in the kPendingDeletion window).
  ASSERT_TRUE(attacker_zss_data)
      << "attacker ZeroStateSuggestionsPageData was destroyed; expected it to "
         "survive in BFCache";
  EXPECT_EQ(attacker_rfh->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  // Ensure that GetPageContext() fails to return an eligible page context.
  base::test::TestFuture<
      base::expected<optimization_guide::proto::ZeroStatePageContext,
                     PageContextIneligibilityType>>
      proto_future;
  attacker_zss_data->GetPageContext(proto_future.GetCallback());
  ASSERT_FALSE(proto_future.Get().has_value());
}

}  // namespace glic
