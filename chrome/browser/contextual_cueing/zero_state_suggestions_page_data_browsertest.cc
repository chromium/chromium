// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/zero_state_suggestions_page_data.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_features.h"
#include "chrome/browser/optimization_guide/browser_test_util.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/proto/contextual_cueing_metadata.pb.h"
#include "components/optimization_guide/proto/features/zero_state_suggestions.pb.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace contextual_cueing {

using ::testing::_;
using ::testing::An;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::WithArgs;

enum class ContentExtraction {
  kFetchInnerTextOnly,
  kFetchAnnotatedPageContentOnly,
  kFetchInnerTextAndAnnotatedPageContent,
};

class MockOGKS : public MockOptimizationGuideKeyedService {
 public:
  MockOGKS() = default;
  ~MockOGKS() override = default;

  MOCK_METHOD(
      void,
      ExecuteModel,
      (optimization_guide::ModelBasedCapabilityKey,
       const google::protobuf::MessageLite&,
       const std::optional<base::TimeDelta>&,
       optimization_guide::OptimizationGuideModelExecutionResultCallback));

  void CaptureExecutionCallback(
      optimization_guide::ModelBasedCapabilityKey feature,
      const google::protobuf::MessageLite& request_metadata,
      const std::optional<base::TimeDelta>& execution_timeout,
      optimization_guide::OptimizationGuideModelExecutionResultCallback
          callback) {
    execution_callback_ = std::move(callback);
  }

  // Runs the last execution callback received with `response`.
  void RunExecuteCallbackWithResponse(
      const optimization_guide::proto::ZeroStateSuggestionsResponse& response) {
    std::string serialized_metadata;
    response.SerializeToString(&serialized_metadata);
    optimization_guide::proto::Any any_result;
    any_result.set_type_url(
        base::StrCat({"type.googleapis.com/", response.GetTypeName()}));
    any_result.set_value(serialized_metadata);

    std::move(execution_callback_)
        .Run(optimization_guide::OptimizationGuideModelExecutionResult(
                 any_result, nullptr),
             nullptr);
  }

 private:
  optimization_guide::OptimizationGuideModelExecutionResultCallback
      execution_callback_;
};

class ZeroStateSuggestionsPageDataBrowserTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<ContentExtraction> {
 public:
  ZeroStateSuggestionsPageDataBrowserTest() {
    base::FieldTrialParams zss_params;
    switch (GetParam()) {
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
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{contextual_cueing::kContextualCueing, {}},
         {contextual_cueing::kGlicZeroStateSuggestions, zss_params},
         {optimization_guide::features::kRemoteOptimizationGuideFetching, {}}},
        /*disabled_features=*/{});
  }

  void DisableOptimizationPermissionCheck() {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        optimization_guide::switches::
            kDisableCheckingUserPermissionsForTesting);
  }

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* browser_context) override {
    mock_optimization_guide_keyed_service_ =
        static_cast<testing::NiceMock<MockOGKS>*>(
            OptimizationGuideKeyedServiceFactory::GetInstance()
                ->SetTestingFactoryAndUse(
                    browser_context,
                    base::BindRepeating([](content::BrowserContext* context)
                                            -> std::unique_ptr<KeyedService> {
                      return std::make_unique<testing::NiceMock<MockOGKS>>();
                    })));
  }

  void PostRunTestOnMainThread() override {
    mock_optimization_guide_keyed_service_ = nullptr;
    InProcessBrowserTest::PostRunTestOnMainThread();
  }

  ContentExtraction GetContentExtraction() const { return GetParam(); }

  MockOGKS& mock_optimization_guide_keyed_service() {
    return *mock_optimization_guide_keyed_service_;
  }

  void SetUpOnDemandHints(bool allow_contextual,
                          const std::vector<std::string>& suggestions) {
    EXPECT_CALL(
        mock_optimization_guide_keyed_service(),
        CanApplyOptimizationOnDemand(
            _,
            ElementsAre(optimization_guide::proto::GLIC_ZERO_STATE_SUGGESTIONS),
            optimization_guide::proto::RequestContext::
                CONTEXT_GLIC_ZERO_STATE_SUGGESTIONS,
            _, _))
        .WillOnce(WithArgs<3>(
            [=](optimization_guide::
                    OnDemandOptimizationGuideDecisionRepeatingCallback
                        callback) {
              optimization_guide::proto::GlicZeroStateSuggestionsMetadata
                  metadata;
              metadata.set_contextual_suggestions_eligible(allow_contextual);
              *metadata.mutable_contextual_suggestions() = {suggestions.begin(),
                                                            suggestions.end()};

              optimization_guide::OptimizationMetadata og_metadata;
              og_metadata.SetAnyMetadataForTesting(metadata);

              optimization_guide::OptimizationGuideDecisionWithMetadata
                  decision_with_metadata;
              decision_with_metadata.decision =
                  optimization_guide::OptimizationGuideDecision::kTrue;
              decision_with_metadata.metadata = og_metadata;
              callback.Run(
                  GURL(),
                  {{optimization_guide::proto::GLIC_ZERO_STATE_SUGGESTIONS,
                    decision_with_metadata}});
            }));
  }

  void SetUpHints(bool allow_contextual,
                  const std::vector<std::string>& suggestions) {
    EXPECT_CALL(
        mock_optimization_guide_keyed_service(),
        CanApplyOptimization(
            _, _, An<optimization_guide::OptimizationGuideDecisionCallback>()))
        .WillRepeatedly(WithArgs<2>(
            [=](optimization_guide::OptimizationGuideDecisionCallback
                    callback) {
              std::move(callback).Run(
                  optimization_guide::OptimizationGuideDecision::kFalse, {});
            }));
    EXPECT_CALL(
        mock_optimization_guide_keyed_service(),
        CanApplyOptimization(
            _, optimization_guide::proto::GLIC_ZERO_STATE_SUGGESTIONS,
            An<optimization_guide::OptimizationGuideDecisionCallback>()))
        .WillOnce(WithArgs<2>(
            [=](optimization_guide::OptimizationGuideDecisionCallback
                    callback) {
              optimization_guide::proto::GlicZeroStateSuggestionsMetadata
                  metadata;
              metadata.set_contextual_suggestions_eligible(allow_contextual);
              *metadata.mutable_contextual_suggestions() = {suggestions.begin(),
                                                            suggestions.end()};

              optimization_guide::OptimizationMetadata og_metadata;
              og_metadata.SetAnyMetadataForTesting(metadata);
              std::move(callback).Run(
                  optimization_guide::OptimizationGuideDecision::kTrue,
                  og_metadata);
            }));
  }

  void SetUpHintsNoResult() {
    EXPECT_CALL(
        mock_optimization_guide_keyed_service(),
        CanApplyOptimization(
            _, _, An<optimization_guide::OptimizationGuideDecisionCallback>()))
        .WillRepeatedly(WithArgs<2>(
            [=](optimization_guide::OptimizationGuideDecisionCallback
                    callback) {
              std::move(callback).Run(
                  optimization_guide::OptimizationGuideDecision::kFalse, {});
            }));
    EXPECT_CALL(
        mock_optimization_guide_keyed_service(),
        CanApplyOptimization(
            _, optimization_guide::proto::GLIC_ZERO_STATE_SUGGESTIONS,
            An<optimization_guide::OptimizationGuideDecisionCallback>()))
        .WillOnce(WithArgs<2>(
            [=](optimization_guide::OptimizationGuideDecisionCallback
                    callback) {
              std::move(callback).Run(
                  optimization_guide::OptimizationGuideDecision::kFalse, {});
            }));
  }

  void SetUpSuccessfulModelExecution() {
    EXPECT_CALL(mock_optimization_guide_keyed_service(),
                ExecuteModel(_, _, _, _))
        .WillOnce(WithArgs<1, 3>(
            [&](const google::protobuf::MessageLite& request_metadata,
                optimization_guide::
                    OptimizationGuideModelExecutionResultCallback callback) {
              const auto* request =
                  reinterpret_cast<const optimization_guide::proto::
                                       ZeroStateSuggestionsRequest*>(
                      &request_metadata);
              EXPECT_EQ(request->page_context().inner_text().empty(),
                        GetContentExtraction() ==
                            ContentExtraction::kFetchAnnotatedPageContentOnly);
              EXPECT_EQ(
                  request->page_context().annotated_page_content().version() ==
                      optimization_guide::proto::
                          ANNOTATED_PAGE_CONTENT_VERSION_UNKNOWN,
                  GetContentExtraction() ==
                      ContentExtraction::kFetchInnerTextOnly);
              EXPECT_EQ("title", request->page_context().title());
              EXPECT_NE(std::string::npos,
                        request->page_context().url().find(
                            "/optimization_guide/zss_page.html"));

              optimization_guide::proto::ZeroStateSuggestionsResponse response;
              response.add_suggestions()->set_label("suggestion 1");
              if (!request->is_fre()) {
                response.add_suggestions()->set_label("suggestion 2");
                response.add_suggestions()->set_label("suggestion 3");
              }
              std::string serialized_metadata;
              response.SerializeToString(&serialized_metadata);

              optimization_guide::proto::Any any_result;
              any_result.set_type_url(base::StrCat(
                  {"type.googleapis.com/", response.GetTypeName()}));
              any_result.set_value(serialized_metadata);

              std::move(callback).Run(
                  optimization_guide::OptimizationGuideModelExecutionResult(
                      any_result, nullptr),
                  nullptr);
            }));
  }

  void SetUpEmptyModelExecutionResult() {
    ON_CALL(mock_optimization_guide_keyed_service(), ExecuteModel(_, _, _, _))
        .WillByDefault(WithArgs<3>(
            [&](optimization_guide::
                    OptimizationGuideModelExecutionResultCallback callback) {
              optimization_guide::proto::Any any_result;
              std::move(callback).Run(
                  optimization_guide::OptimizationGuideModelExecutionResult(
                      any_result, nullptr),
                  nullptr);
            }));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<testing::NiceMock<MockOGKS>> mock_optimization_guide_keyed_service_;
};

INSTANTIATE_TEST_SUITE_P(
    WithContentExtraction,
    ZeroStateSuggestionsPageDataBrowserTest,
    ::testing::Values(
        ContentExtraction::kFetchInnerTextOnly,
        ContentExtraction::kFetchAnnotatedPageContentOnly,
        ContentExtraction::kFetchInnerTextAndAnnotatedPageContent));

IN_PROC_BROWSER_TEST_P(ZeroStateSuggestionsPageDataBrowserTest, BasicFlow) {
  base::HistogramTester histogram_tester;

  DisableOptimizationPermissionCheck();
  SetUpSuccessfulModelExecution();
  SetUpHints(/*allow_contextual=*/true, /*suggestions=*/{});
  EXPECT_CALL(mock_optimization_guide_keyed_service(),
              CanApplyOptimizationOnDemand)
      .Times(0);

  ASSERT_TRUE(embedded_test_server()->Start());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/optimization_guide/zss_page.html")));

  base::test::TestFuture<std::optional<std::vector<std::string>>> future;

  auto* page_data = ZeroStateSuggestionsPageData::GetOrCreateForPage(
      web_contents->GetPrimaryPage());
  page_data->FetchSuggestions(/*is_fre=*/false, future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(3u, future.Get().value().size());
  EXPECT_EQ("suggestion 1", future.Get().value()[0]);
  EXPECT_EQ("suggestion 2", future.Get().value()[1]);
  EXPECT_EQ("suggestion 3", future.Get().value()[2]);
  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.ZeroStateSuggestions.ContextExtractionDone", true, 1);
  histogram_tester.ExpectTotalCount(
      "ContextualCueing.GlicSuggestions.MesFetchLatency", 1);
}

IN_PROC_BROWSER_TEST_P(ZeroStateSuggestionsPageDataBrowserTest,
                       HoldsOntoSuccessiveRequests) {
  base::HistogramTester histogram_tester;

  DisableOptimizationPermissionCheck();
  SetUpHints(/*allow_contextual=*/true, /*suggestions=*/{});
  EXPECT_CALL(mock_optimization_guide_keyed_service(), ExecuteModel(_, _, _, _))
      .WillOnce(Invoke(&mock_optimization_guide_keyed_service(),
                       &MockOGKS::CaptureExecutionCallback));

  ASSERT_TRUE(embedded_test_server()->Start());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/optimization_guide/zss_page.html")));

  auto* page_data = ZeroStateSuggestionsPageData::GetOrCreateForPage(
      web_contents->GetPrimaryPage());

  // Set up two concurrent calls (simulates mouse down and then on load).
  base::test::TestFuture<std::optional<std::vector<std::string>>> future;
  page_data->FetchSuggestions(/*is_fre=*/false, future.GetCallback());
  base::test::TestFuture<std::optional<std::vector<std::string>>> future2;
  page_data->FetchSuggestions(/*is_fre=*/false, future2.GetCallback());

  // Wait until page is extracted.
  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester,
      "ContextualCueing.ZeroStateSuggestions.ContextExtractionDone", 1);

  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.ZeroStateSuggestions.ContextExtractionDone", true, 1);

  // Fire capture execution callback.
  optimization_guide::proto::ZeroStateSuggestionsResponse response;
  response.add_suggestions()->set_label("suggestion 1");
  response.add_suggestions()->set_label("suggestion 2");
  response.add_suggestions()->set_label("suggestion 3");
  mock_optimization_guide_keyed_service().RunExecuteCallbackWithResponse(
      response);

  // Both calls should be fulfilled using the same response.
  ASSERT_TRUE(future.Wait());
  ASSERT_TRUE(future2.Wait());

  EXPECT_EQ(3u, future.Get().value().size());
  EXPECT_EQ("suggestion 1", future.Get().value()[0]);
  EXPECT_EQ("suggestion 2", future.Get().value()[1]);
  EXPECT_EQ("suggestion 3", future.Get().value()[2]);

  EXPECT_EQ(3u, future2.Get().value().size());
  EXPECT_EQ("suggestion 1", future2.Get().value()[0]);
  EXPECT_EQ("suggestion 2", future2.Get().value()[1]);
  EXPECT_EQ("suggestion 3", future2.Get().value()[2]);

  histogram_tester.ExpectTotalCount(
      "ContextualCueing.GlicSuggestions.MesFetchLatency", 1);
}

IN_PROC_BROWSER_TEST_P(ZeroStateSuggestionsPageDataBrowserTest,
                       CreateDataDoesNotFetchWithoutExplicitCall) {
  base::HistogramTester histogram_tester;

  DisableOptimizationPermissionCheck();
  EXPECT_CALL(mock_optimization_guide_keyed_service(), ExecuteModel).Times(0);

  ASSERT_TRUE(embedded_test_server()->Start());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/optimization_guide/zss_page.html")));

  ZeroStateSuggestionsPageData::CreateForPage(web_contents->GetPrimaryPage());

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester,
      "ContextualCueing.ZeroStateSuggestions.ContextExtractionDone", 1);

  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.ZeroStateSuggestions.ContextExtractionDone", true, 1);
  histogram_tester.ExpectTotalCount(
      "ContextualCueing.GlicSuggestions.MesFetchLatency", 0);
}

IN_PROC_BROWSER_TEST_P(ZeroStateSuggestionsPageDataBrowserTest,
                       UseHintsSuggestions) {
  DisableOptimizationPermissionCheck();
  SetUpHints(/*allow_contextual=*/true,
             /*suggestions=*/{"hints 1", "hints 2", "hints 3"});
  EXPECT_CALL(mock_optimization_guide_keyed_service(),
              CanApplyOptimizationOnDemand)
      .Times(0);

  ASSERT_TRUE(embedded_test_server()->Start());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/optimization_guide/zss_page.html")));

  base::test::TestFuture<std::optional<std::vector<std::string>>> future;

  auto* page_data = ZeroStateSuggestionsPageData::GetOrCreateForPage(
      web_contents->GetPrimaryPage());
  page_data->FetchSuggestions(/*is_fre=*/false, future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(3u, future.Get().value().size());
  EXPECT_EQ("hints 1", future.Get().value()[0]);
  EXPECT_EQ("hints 2", future.Get().value()[1]);
  EXPECT_EQ("hints 3", future.Get().value()[2]);
}

IN_PROC_BROWSER_TEST_P(ZeroStateSuggestionsPageDataBrowserTest,
                       ContextualSuggestionsNotAllowed) {
  DisableOptimizationPermissionCheck();
  SetUpHints(/*allow_contextual=*/false, /*suggestions=*/{});
  EXPECT_CALL(mock_optimization_guide_keyed_service(),
              CanApplyOptimizationOnDemand)
      .Times(0);

  ASSERT_TRUE(embedded_test_server()->Start());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/optimization_guide/zss_page.html")));

  base::test::TestFuture<std::optional<std::vector<std::string>>> future;

  auto* page_data = ZeroStateSuggestionsPageData::GetOrCreateForPage(
      web_contents->GetPrimaryPage());
  page_data->FetchSuggestions(/*is_fre=*/false, future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(std::nullopt, future.Get());
}

IN_PROC_BROWSER_TEST_P(ZeroStateSuggestionsPageDataBrowserTest,
                       NoResultFromHints) {
  DisableOptimizationPermissionCheck();
  // Assumes page is eligible for contextual suggestions without hints result.
  EXPECT_CALL(mock_optimization_guide_keyed_service(), ExecuteModel).Times(0);
  SetUpHintsNoResult();
  SetUpSuccessfulModelExecution();

  ASSERT_TRUE(embedded_test_server()->Start());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/optimization_guide/zss_page.html")));

  base::test::TestFuture<std::optional<std::vector<std::string>>> future;

  auto* page_data = ZeroStateSuggestionsPageData::GetOrCreateForPage(
      web_contents->GetPrimaryPage());
  page_data->FetchSuggestions(/*is_fre=*/false, future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(3u, future.Get().value().size());
  EXPECT_EQ("suggestion 1", future.Get().value()[0]);
  EXPECT_EQ("suggestion 2", future.Get().value()[1]);
  EXPECT_EQ("suggestion 3", future.Get().value()[2]);
}

IN_PROC_BROWSER_TEST_P(ZeroStateSuggestionsPageDataBrowserTest, CacheBehavior) {
  DisableOptimizationPermissionCheck();
  ASSERT_TRUE(embedded_test_server()->Start());

  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/optimization_guide/zss_page.html")));

  // Set up initial flow.
  {
    base::test::TestFuture<std::optional<std::vector<std::string>>> future;
    base::HistogramTester histogram_tester;

    SetUpHints(/*allow_contextual=*/true, /*suggestions=*/{});
    SetUpSuccessfulModelExecution();

    auto* page_data = ZeroStateSuggestionsPageData::GetOrCreateForPage(
        web_contents->GetPrimaryPage());
    page_data->FetchSuggestions(/*is_fre=*/false, future.GetCallback());
    ASSERT_TRUE(future.Wait());
    EXPECT_EQ(3u, future.Get().value().size());
    EXPECT_EQ("suggestion 1", future.Get().value()[0]);
    EXPECT_EQ("suggestion 2", future.Get().value()[1]);
    EXPECT_EQ("suggestion 3", future.Get().value()[2]);
    histogram_tester.ExpectTotalCount(
        "ContextualCueing.GlicSuggestions.MesFetchLatency", 1);
  }

  testing::Mock::VerifyAndClearExpectations(
      &mock_optimization_guide_keyed_service());

  // Make sure model execution not called.
  {
    EXPECT_CALL(mock_optimization_guide_keyed_service(), ExecuteModel).Times(0);
    base::HistogramTester histogram_tester;

    base::test::TestFuture<std::optional<std::vector<std::string>>> future;

    auto* page_data = ZeroStateSuggestionsPageData::GetOrCreateForPage(
        web_contents->GetPrimaryPage());
    page_data->FetchSuggestions(/*is_fre=*/false, future.GetCallback());
    ASSERT_TRUE(future.Wait());
    EXPECT_EQ(3u, future.Get().value().size());
    EXPECT_EQ("suggestion 1", future.Get().value()[0]);
    EXPECT_EQ("suggestion 2", future.Get().value()[1]);
    EXPECT_EQ("suggestion 3", future.Get().value()[2]);
    histogram_tester.ExpectTotalCount(
        "ContextualCueing.GlicSuggestions.MesFetchLatency", 0);
  }
}

IN_PROC_BROWSER_TEST_P(ZeroStateSuggestionsPageDataBrowserTest,
                       CacheBehaviorNonTransientError) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/optimization_guide/zss_page.html")));

  // Set up initial flow.
  {
    base::HistogramTester histogram_tester;
    base::test::TestFuture<std::optional<std::vector<std::string>>> future;

    EXPECT_CALL(mock_optimization_guide_keyed_service(),
                ExecuteModel(_, _, _, _))
        .WillOnce(WithArgs<1, 3>(
            [&](const google::protobuf::MessageLite& request_metadata,
                optimization_guide::
                    OptimizationGuideModelExecutionResultCallback callback) {
              optimization_guide::proto::ErrorResponse error_response;
              error_response.set_error_state(
                  optimization_guide::proto::ErrorState::
                      ERROR_STATE_INTERNAL_SERVER_ERROR_NO_RETRY);
              optimization_guide::OptimizationGuideModelExecutionResult result;
              result.response = base::unexpected(
                  optimization_guide::OptimizationGuideModelExecutionError::
                      FromModelExecutionServerError(error_response));
              std::move(callback).Run(std::move(result), nullptr);
            }));

    auto* page_data = ZeroStateSuggestionsPageData::GetOrCreateForPage(
        web_contents->GetPrimaryPage());
    page_data->FetchSuggestions(/*is_fre=*/false, future.GetCallback());
    ASSERT_TRUE(future.Wait());
    EXPECT_FALSE(future.Get().has_value());
    histogram_tester.ExpectTotalCount(
        "ContextualCueing.GlicSuggestions.MesFetchLatency", 1);
  }

  testing::Mock::VerifyAndClearExpectations(
      &mock_optimization_guide_keyed_service());

  // Make sure model execution not called.
  {
    base::HistogramTester histogram_tester;
    EXPECT_CALL(mock_optimization_guide_keyed_service(), ExecuteModel).Times(0);

    base::test::TestFuture<std::optional<std::vector<std::string>>> future;

    auto* page_data = ZeroStateSuggestionsPageData::GetOrCreateForPage(
        web_contents->GetPrimaryPage());
    page_data->FetchSuggestions(/*is_fre=*/false, future.GetCallback());
    ASSERT_TRUE(future.Wait());
    EXPECT_FALSE(future.Get().has_value());
    histogram_tester.ExpectTotalCount(
        "ContextualCueing.GlicSuggestions.MesFetchLatency", 0);
  }
}

IN_PROC_BROWSER_TEST_P(ZeroStateSuggestionsPageDataBrowserTest,
                       CacheBehaviorTransientError) {
  DisableOptimizationPermissionCheck();
  ASSERT_TRUE(embedded_test_server()->Start());

  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/optimization_guide/zss_page.html")));

  // Set up initial flow.
  {
    base::HistogramTester histogram_tester;
    base::test::TestFuture<std::optional<std::vector<std::string>>> future;

    SetUpHints(/*allow_contextual=*/true, /*suggestions=*/{});
    EXPECT_CALL(mock_optimization_guide_keyed_service(),
                ExecuteModel(_, _, _, _))
        .WillOnce(WithArgs<1, 3>(
            [&](const google::protobuf::MessageLite& request_metadata,
                optimization_guide::
                    OptimizationGuideModelExecutionResultCallback callback) {
              optimization_guide::proto::ErrorResponse error_response;
              error_response.set_error_state(
                  optimization_guide::proto::ErrorState::
                      ERROR_STATE_INTERNAL_SERVER_ERROR_RETRY);
              optimization_guide::OptimizationGuideModelExecutionResult result;
              result.response = base::unexpected(
                  optimization_guide::OptimizationGuideModelExecutionError::
                      FromModelExecutionServerError(error_response));
              std::move(callback).Run(std::move(result), nullptr);
            }));

    auto* page_data = ZeroStateSuggestionsPageData::GetOrCreateForPage(
        web_contents->GetPrimaryPage());
    page_data->FetchSuggestions(/*is_fre=*/false, future.GetCallback());
    ASSERT_TRUE(future.Wait());
    EXPECT_FALSE(future.Get().has_value());
    histogram_tester.ExpectTotalCount(
        "ContextualCueing.GlicSuggestions.MesFetchLatency", 1);
  }

  testing::Mock::VerifyAndClearExpectations(
      &mock_optimization_guide_keyed_service());

  // Make sure model execution called after a transient error.
  {
    base::HistogramTester histogram_tester;
    SetUpSuccessfulModelExecution();

    base::test::TestFuture<std::optional<std::vector<std::string>>> future;

    auto* page_data = ZeroStateSuggestionsPageData::GetOrCreateForPage(
        web_contents->GetPrimaryPage());
    page_data->FetchSuggestions(/*is_fre=*/false, future.GetCallback());
    ASSERT_TRUE(future.Wait());
    EXPECT_EQ(3u, future.Get().value().size());
    EXPECT_EQ("suggestion 1", future.Get().value()[0]);
    EXPECT_EQ("suggestion 2", future.Get().value()[1]);
    EXPECT_EQ("suggestion 3", future.Get().value()[2]);
    // The count should increase because MES should be queried again.
    histogram_tester.ExpectTotalCount(
        "ContextualCueing.GlicSuggestions.MesFetchLatency", 1);
  }
}

IN_PROC_BROWSER_TEST_P(ZeroStateSuggestionsPageDataBrowserTest, NonMSBBFlow) {
  base::HistogramTester histogram_tester;

  SetUpEmptyModelExecutionResult();
  SetUpOnDemandHints(/*allow_contextual=*/true, /*suggestions=*/{
                         "on demand 1", "on demand 2", "on demand 3"});

  ASSERT_TRUE(embedded_test_server()->Start());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/optimization_guide/zss_page.html")));

  base::test::TestFuture<std::optional<std::vector<std::string>>> future;

  auto* page_data = ZeroStateSuggestionsPageData::GetOrCreateForPage(
      web_contents->GetPrimaryPage());
  page_data->FetchSuggestions(/*is_fre=*/false, future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(3u, future.Get().value().size());
  EXPECT_EQ("on demand 1", future.Get().value()[0]);
  EXPECT_EQ("on demand 2", future.Get().value()[1]);
  EXPECT_EQ("on demand 3", future.Get().value()[2]);
  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.ZeroStateSuggestions.ContextExtractionDone", true, 1);
}

IN_PROC_BROWSER_TEST_P(ZeroStateSuggestionsPageDataBrowserTest,
                       NonMSBBFlowContextualNotAllowed) {
  base::HistogramTester histogram_tester;

  SetUpEmptyModelExecutionResult();
  SetUpOnDemandHints(/*allow_contextual=*/false, /*suggestions=*/{});

  ASSERT_TRUE(embedded_test_server()->Start());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/optimization_guide/zss_page.html")));

  base::test::TestFuture<std::optional<std::vector<std::string>>> future;

  auto* page_data = ZeroStateSuggestionsPageData::GetOrCreateForPage(
      web_contents->GetPrimaryPage());
  page_data->FetchSuggestions(/*is_fre=*/false, future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(std::nullopt, future.Get());
}

IN_PROC_BROWSER_TEST_P(ZeroStateSuggestionsPageDataBrowserTest,
                       OnlyRequestsHintsOnDemandAfterFetchCall) {
  base::HistogramTester histogram_tester;

  ASSERT_TRUE(embedded_test_server()->Start());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/optimization_guide/zss_page.html")));

  // Make sure on demand fetch does not start when creating page data.
  EXPECT_CALL(mock_optimization_guide_keyed_service(),
              CanApplyOptimizationOnDemand)
      .Times(0);
  auto* page_data = ZeroStateSuggestionsPageData::GetOrCreateForPage(
      web_contents->GetPrimaryPage());
  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester,
      "ContextualCueing.ZeroStateSuggestions.ContextExtractionDone", 1);

  testing::Mock::VerifyAndClearExpectations(
      &mock_optimization_guide_keyed_service());

  // Make sure on demand fetch starts after FetchSuggestions is invoked.
  SetUpEmptyModelExecutionResult();
  base::test::TestFuture<std::optional<std::vector<std::string>>> future;
  SetUpOnDemandHints(/*allow_contextual=*/false, /*suggestions=*/{});
  page_data->FetchSuggestions(/*is_fre=*/false, future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(std::nullopt, future.Get());
}

}  // namespace contextual_cueing
