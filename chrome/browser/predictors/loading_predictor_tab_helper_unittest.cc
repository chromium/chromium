// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/loading_predictor_tab_helper.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_test_util.h"
#include "chrome/browser/predictors/predictors_enums.h"
#include "chrome/browser/predictors/predictors_features.h"
#include "chrome/browser/sessions/session_tab_helper_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/optimization_guide/optimization_guide_features.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom.h"

using ::testing::_;
using ::testing::ByRef;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrictMock;
using ::testing::WithArg;

namespace predictors {

class MockLoadingDataCollector : public LoadingDataCollector {
 public:
  explicit MockLoadingDataCollector(const LoadingPredictorConfig& config);

  MOCK_METHOD1(RecordStartNavigation, void(const NavigationID&));
  MOCK_METHOD3(RecordFinishNavigation,
               void(const NavigationID&, const NavigationID&, bool));
  MOCK_METHOD2(RecordResourceLoadComplete,
               void(const NavigationID&,
                    const blink::mojom::ResourceLoadInfo&));
  MOCK_METHOD2(RecordMainFrameLoadComplete,
               void(const NavigationID&,
                    const base::Optional<OptimizationGuidePrediction>&));
  MOCK_METHOD2(RecordFirstContentfulPaint,
               void(const NavigationID&, const base::TimeTicks&));
};

MockLoadingDataCollector::MockLoadingDataCollector(
    const LoadingPredictorConfig& config)
    : LoadingDataCollector(nullptr, nullptr, config) {}

// TODO(crbug/1035698): Migrate to TestOptimizationGuideDecider when provided.
class MockOptimizationGuideKeyedService : public OptimizationGuideKeyedService {
 public:
  explicit MockOptimizationGuideKeyedService(
      content::BrowserContext* browser_context)
      : OptimizationGuideKeyedService(browser_context) {}
  ~MockOptimizationGuideKeyedService() override = default;

  MOCK_METHOD1(
      RegisterOptimizationTypes,
      void(const std::vector<optimization_guide::proto::OptimizationType>&));
  MOCK_METHOD3(CanApplyOptimizationAsync,
               void(content::NavigationHandle*,
                    optimization_guide::proto::OptimizationType,
                    optimization_guide::OptimizationGuideDecisionCallback));
};

class LoadingPredictorTabHelperTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override;
  void TearDown() override;

  SessionID GetTabID();
  void NavigateAndCommitInFrame(const std::string& url,
                                content::RenderFrameHost* rfh);
  void NavigateAndCommitInMainFrameAndVerifyMetrics(const std::string& url);

 protected:
  std::unique_ptr<LoadingPredictor> loading_predictor_;
  // Owned by |loading_predictor_|.
  StrictMock<MockLoadingDataCollector>* mock_collector_;
  // Owned elsewhere.
  MockOptimizationGuideKeyedService* mock_optimization_guide_keyed_service_;
  // Owned by |web_contents()|.
  LoadingPredictorTabHelper* tab_helper_;
};

void LoadingPredictorTabHelperTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();
  CreateSessionServiceTabHelper(web_contents());
  mock_optimization_guide_keyed_service_ =
      static_cast<MockOptimizationGuideKeyedService*>(
          OptimizationGuideKeyedServiceFactory::GetInstance()
              ->SetTestingFactoryAndUse(
                  profile(),
                  base::BindRepeating([](content::BrowserContext* context)
                                          -> std::unique_ptr<KeyedService> {
                    return std::make_unique<MockOptimizationGuideKeyedService>(
                        context);
                  })));
  LoadingPredictorTabHelper::CreateForWebContents(web_contents());
  tab_helper_ = LoadingPredictorTabHelper::FromWebContents(web_contents());

  LoadingPredictorConfig config;
  PopulateTestConfig(&config);
  loading_predictor_ = std::make_unique<LoadingPredictor>(config, profile());

  auto mock_collector =
      std::make_unique<StrictMock<MockLoadingDataCollector>>(config);
  mock_collector_ = mock_collector.get();
  loading_predictor_->set_mock_loading_data_collector(
      std::move(mock_collector));

  tab_helper_->SetLoadingPredictorForTesting(loading_predictor_->GetWeakPtr());
}

void LoadingPredictorTabHelperTest::TearDown() {
  loading_predictor_->Shutdown();
  ChromeRenderViewHostTestHarness::TearDown();
}

void LoadingPredictorTabHelperTest::
    NavigateAndCommitInMainFrameAndVerifyMetrics(const std::string& url) {
  NavigationID start_navigation_id;
  NavigationID old_finish_navigation_id;
  NavigationID new_finish_navigation_id;
  EXPECT_CALL(*mock_collector_, RecordStartNavigation(_))
      .WillOnce(SaveArg<0>(&start_navigation_id));
  EXPECT_CALL(*mock_collector_,
              RecordFinishNavigation(_, _,
                                     /* is_error_page */ false))
      .WillOnce(DoAll(SaveArg<0>(&old_finish_navigation_id),
                      SaveArg<1>(&new_finish_navigation_id)));

  NavigateAndCommitInFrame(url, main_rfh());

  auto expected_navigation_id = CreateNavigationID(
      GetTabID(), url, web_contents()->GetMainFrame()->GetPageUkmSourceId());
  EXPECT_EQ(start_navigation_id, expected_navigation_id);
  EXPECT_EQ(old_finish_navigation_id, expected_navigation_id);
  EXPECT_EQ(new_finish_navigation_id, expected_navigation_id);
}

SessionID LoadingPredictorTabHelperTest::GetTabID() {
  return sessions::SessionTabHelper::IdForTab(web_contents());
}

void LoadingPredictorTabHelperTest::NavigateAndCommitInFrame(
    const std::string& url,
    content::RenderFrameHost* rfh) {
  auto navigation =
      content::NavigationSimulator::CreateRendererInitiated(GURL(url), rfh);
  // These tests simulate loading events manually.
  // TODO(ahemery): Consider refactoring to rely on load events dispatched by
  // NavigationSimulator.
  navigation->SetKeepLoading(true);
  navigation->Start();
  navigation->Commit();
}

// Tests that a main frame navigation is correctly recorded by the
// LoadingDataCollector.
TEST_F(LoadingPredictorTabHelperTest, MainFrameNavigation) {
  NavigateAndCommitInMainFrameAndVerifyMetrics("http://test.org");
}

// Tests that an old and new navigation ids are correctly set if a navigation
// has redirects.
TEST_F(LoadingPredictorTabHelperTest, MainFrameNavigationWithRedirects) {
  auto navigation = content::NavigationSimulator::CreateRendererInitiated(
      GURL("http://test.org"), main_rfh());
  // The problem here is that mock_collector_ is a strict mock, which expects
  // a particular set of loading events and fails when extra is present.
  // TOOO(ahemery): Consider refactoring this to rely on loading events
  // in NavigationSimulator.
  navigation->SetKeepLoading(true);
  NavigationID start_navigation_id;
  EXPECT_CALL(*mock_collector_, RecordStartNavigation(_))
      .WillOnce(SaveArg<0>(&start_navigation_id));
  navigation->Start();
  navigation->Redirect(GURL("http://test2.org"));
  navigation->Redirect(GURL("http://test3.org"));
  auto new_navigation_id = start_navigation_id;
  new_navigation_id.main_frame_url = GURL("http://test3.org");
  EXPECT_CALL(*mock_collector_,
              RecordFinishNavigation(start_navigation_id, new_navigation_id,
                                     /* is_error_page */ false));
  navigation->Commit();

  EXPECT_EQ(
      start_navigation_id,
      CreateNavigationID(GetTabID(), "http://test.org",
                         web_contents()->GetMainFrame()->GetPageUkmSourceId()));
}

// Tests that a subframe navigation is not recorded.
TEST_F(LoadingPredictorTabHelperTest, SubframeNavigation) {
  // We need to have a committed main frame navigation before navigating in sub
  // frames.
  NavigateAndCommitInMainFrameAndVerifyMetrics("http://test.org");

  auto* subframe =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");
  // Subframe navigation shouldn't be recorded.
  NavigateAndCommitInFrame("http://sub.test.org", subframe);
}

// Tests that a failed navigation is correctly recorded.
TEST_F(LoadingPredictorTabHelperTest, MainFrameNavigationFailed) {
  auto navigation = content::NavigationSimulator::CreateRendererInitiated(
      GURL("http://test.org"), main_rfh());
  navigation->SetKeepLoading(true);
  // The problem here is that mock_collector_ is a strict mock, which expects
  // a particular set of loading events and fails when extra is present.
  // TOOO(ahemery): Consider refactoring this to rely on loading events
  // in NavigationSimulator.
  NavigationID navigation_id;
  EXPECT_CALL(*mock_collector_, RecordStartNavigation(_))
      .WillOnce(SaveArg<0>(&navigation_id));
  navigation->Start();

  EXPECT_CALL(*mock_collector_,
              RecordFinishNavigation(navigation_id, navigation_id,
                                     /* is_error_page */ true));
  navigation->Fail(net::ERR_TIMED_OUT);
  navigation->CommitErrorPage();

  EXPECT_EQ(
      navigation_id,
      CreateNavigationID(GetTabID(), "http://test.org",
                         web_contents()->GetMainFrame()->GetPageUkmSourceId()));
}

// Tests that a same document navigation is not recorded.
TEST_F(LoadingPredictorTabHelperTest, MainFrameNavigationSameDocument) {
  NavigateAndCommitInMainFrameAndVerifyMetrics("http://test.org");

  // Same document navigation shouldn't be recorded.
  content::NavigationSimulator::CreateRendererInitiated(GURL("http://test.org"),
                                                        main_rfh())
      ->CommitSameDocument();
}

// Tests that document on load completed is recorded with correct navigation
// id.
TEST_F(LoadingPredictorTabHelperTest, DocumentOnLoadCompleted) {
  NavigateAndCommitInMainFrameAndVerifyMetrics("http://test.org");
  auto navigation_id =
      CreateNavigationID(GetTabID(), "http://test.org",
                         web_contents()->GetMainFrame()->GetPageUkmSourceId());

  // Adding subframe navigation to ensure that the committed main frame url will
  // be used.
  auto* subframe =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");
  NavigateAndCommitInFrame("http://sub.test.org", subframe);

  const base::Optional<OptimizationGuidePrediction>
      null_optimization_guide_prediction;
  EXPECT_CALL(*mock_collector_,
              RecordMainFrameLoadComplete(navigation_id,
                                          null_optimization_guide_prediction));
  tab_helper_->DocumentOnLoadCompletedInMainFrame();
}

// Tests that a resource load is correctly recorded.
TEST_F(LoadingPredictorTabHelperTest, ResourceLoadComplete) {
  NavigateAndCommitInMainFrameAndVerifyMetrics("http://test.org");
  auto navigation_id =
      CreateNavigationID(GetTabID(), "http://test.org",
                         web_contents()->GetMainFrame()->GetPageUkmSourceId());

  auto resource_load_info = CreateResourceLoadInfo(
      "http://test.org/script.js", network::mojom::RequestDestination::kScript);
  EXPECT_CALL(*mock_collector_,
              RecordResourceLoadComplete(navigation_id,
                                         Eq(ByRef(*resource_load_info))));
  tab_helper_->ResourceLoadComplete(main_rfh(), content::GlobalRequestID(),
                                    *resource_load_info);
}

// Tests that a resource loaded in a subframe is not recorded.
TEST_F(LoadingPredictorTabHelperTest, ResourceLoadCompleteInSubFrame) {
  NavigateAndCommitInMainFrameAndVerifyMetrics("http://test.org");

  auto* subframe =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");
  NavigateAndCommitInFrame("http://sub.test.org", subframe);

  // Resource loaded in subframe shouldn't be recorded.
  auto resource_load_info =
      CreateResourceLoadInfo("http://sub.test.org/script.js",
                             network::mojom::RequestDestination::kScript,
                             /*always_access_network=*/false);
  tab_helper_->ResourceLoadComplete(subframe, content::GlobalRequestID(),
                                    *resource_load_info);
}

// Tests that a resource load from the memory cache is correctly recorded.
TEST_F(LoadingPredictorTabHelperTest, LoadResourceFromMemoryCache) {
  NavigateAndCommitInMainFrameAndVerifyMetrics("http://test.org");

  auto navigation_id =
      CreateNavigationID(GetTabID(), "http://test.org",
                         web_contents()->GetMainFrame()->GetPageUkmSourceId());

  auto resource_load_info = CreateResourceLoadInfo(
      "http://test.org/script.js", network::mojom::RequestDestination::kScript,
      false);
  resource_load_info->mime_type = "application/javascript";
  resource_load_info->network_info->network_accessed = false;
  EXPECT_CALL(*mock_collector_,
              RecordResourceLoadComplete(navigation_id,
                                         Eq(ByRef(*resource_load_info))));
  tab_helper_->DidLoadResourceFromMemoryCache(
      GURL("http://test.org/script.js"), "application/javascript",
      network::mojom::RequestDestination::kScript);
}

class LoadingPredictorTabHelperOptimizationGuideDeciderTest
    : public LoadingPredictorTabHelperTest {
 public:
  LoadingPredictorTabHelperOptimizationGuideDeciderTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kLoadingPredictorUseOptimizationGuide,
         // Need to add otherwise GetForProfile() returns null.
         optimization_guide::features::kOptimizationHints},
        {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that document on load completed is recorded with correct navigation
// id and optimization guide prediction.
TEST_F(LoadingPredictorTabHelperOptimizationGuideDeciderTest,
       DocumentOnLoadCompletedOptimizationGuide) {
  base::HistogramTester histogram_tester;

  optimization_guide::OptimizationMetadata optimization_metadata;
  optimization_guide::proto::LoadingPredictorMetadata lp_metadata;
  lp_metadata.add_subresources()->set_url("http://test.org/resource1");
  lp_metadata.add_subresources()->set_url("http://other.org/resource2");
  lp_metadata.add_subresources()->set_url("http://other.org/resource3");
  optimization_metadata.set_loading_predictor_metadata(lp_metadata);
  EXPECT_CALL(
      *mock_optimization_guide_keyed_service_,
      CanApplyOptimizationAsync(_, optimization_guide::proto::LOADING_PREDICTOR,
                                base::test::IsNotNullCallback()))
      .WillOnce(base::test::RunOnceCallback<2>(
          optimization_guide::OptimizationGuideDecision::kTrue,
          ByRef(optimization_metadata)));
  NavigateAndCommitInMainFrameAndVerifyMetrics("http://test.org");
  auto navigation_id =
      CreateNavigationID(GetTabID(), "http://test.org",
                         web_contents()->GetMainFrame()->GetPageUkmSourceId());

  // Adding subframe navigation to ensure that the committed main frame url will
  // be used.
  auto* subframe =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");
  NavigateAndCommitInFrame("http://sub.test.org", subframe);

  base::Optional<OptimizationGuidePrediction> prediction =
      OptimizationGuidePrediction();
  prediction->decision = optimization_guide::OptimizationGuideDecision::kTrue;
  url::Origin main_frame_origin = url::Origin::Create(GURL("http://test.org"));
  PreconnectPrediction preconnect_prediction = CreatePreconnectPrediction(
      "", false,
      {{url::Origin::Create(GURL("http://other.org")), 1,
        net::NetworkIsolationKey(main_frame_origin, main_frame_origin)}});
  prediction->preconnect_prediction = preconnect_prediction;
  prediction->predicted_subresources = {GURL("http://test.org/resource1"),
                                        GURL("http://other.org/resource2"),
                                        GURL("http://other.org/resource3")};
  EXPECT_CALL(*mock_collector_,
              RecordMainFrameLoadComplete(navigation_id, prediction));
  tab_helper_->DocumentOnLoadCompletedInMainFrame();

  histogram_tester.ExpectUniqueSample(
      "LoadingPredictor.OptimizationHintsReceiveStatus",
      OptimizationHintsReceiveStatus::kBeforeNavigationFinish, 1);
}

// Tests that document on load completed is recorded with correct navigation
// id and optimization guide prediction.
TEST_F(LoadingPredictorTabHelperOptimizationGuideDeciderTest,
       DocumentOnLoadCompletedOptimizationGuidePredictionComesAfterCommit) {
  base::HistogramTester histogram_tester;

  optimization_guide::OptimizationMetadata optimization_metadata;
  optimization_guide::proto::LoadingPredictorMetadata lp_metadata;
  lp_metadata.add_subresources()->set_url("http://test.org/resource1");
  lp_metadata.add_subresources()->set_url("http://other.org/resource2");
  lp_metadata.add_subresources()->set_url("http://other.org/resource3");
  optimization_metadata.set_loading_predictor_metadata(lp_metadata);
  optimization_guide::OptimizationGuideDecisionCallback callback;
  EXPECT_CALL(
      *mock_optimization_guide_keyed_service_,
      CanApplyOptimizationAsync(_, optimization_guide::proto::LOADING_PREDICTOR,
                                base::test::IsNotNullCallback()))
      .WillOnce(WithArg<2>(
          Invoke([&](optimization_guide::OptimizationGuideDecisionCallback
                         got_callback) -> void {
            callback = std::move(got_callback);
          })));
  NavigateAndCommitInMainFrameAndVerifyMetrics("http://test.org");
  auto navigation_id =
      CreateNavigationID(GetTabID(), "http://test.org",
                         web_contents()->GetMainFrame()->GetPageUkmSourceId());

  // Invoke callback after commit.
  std::move(callback).Run(optimization_guide::OptimizationGuideDecision::kTrue,
                          optimization_metadata);

  // Adding subframe navigation to ensure that the committed main frame url will
  // be used.
  auto* subframe =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");
  NavigateAndCommitInFrame("http://sub.test.org", subframe);

  base::Optional<OptimizationGuidePrediction> prediction =
      OptimizationGuidePrediction();
  prediction->decision = optimization_guide::OptimizationGuideDecision::kTrue;
  url::Origin main_frame_origin = url::Origin::Create(GURL("http://test.org"));
  PreconnectPrediction preconnect_prediction = CreatePreconnectPrediction(
      "", false,
      {{url::Origin::Create(GURL("http://other.org")), 1,
        net::NetworkIsolationKey(main_frame_origin, main_frame_origin)}});
  prediction->preconnect_prediction = preconnect_prediction;
  prediction->predicted_subresources = {GURL("http://test.org/resource1"),
                                        GURL("http://other.org/resource2"),
                                        GURL("http://other.org/resource3")};
  EXPECT_CALL(*mock_collector_,
              RecordMainFrameLoadComplete(navigation_id, prediction));
  tab_helper_->DocumentOnLoadCompletedInMainFrame();

  // Optimization guide predictions came after commit.
  histogram_tester.ExpectUniqueSample(
      "LoadingPredictor.OptimizationHintsReceiveStatus",
      OptimizationHintsReceiveStatus::kAfterNavigationFinish, 1);
}

// Tests that an old and new navigation ids are correctly set if a navigation
// has redirects.
TEST_F(LoadingPredictorTabHelperOptimizationGuideDeciderTest,
       DocumentOnLoadCompletedOptimizationGuidePredictionArrivedAfterRedirect) {
  base::HistogramTester histogram_tester;

  auto navigation = content::NavigationSimulator::CreateRendererInitiated(
      GURL("http://test.org"), main_rfh());
  // The problem here is that mock_collector_ is a strict mock, which expects
  // a particular set of loading events and fails when extra is present.
  // TOOO(ahemery): Consider refactoring this to rely on loading events
  // in NavigationSimulator.
  navigation->SetKeepLoading(true);
  NavigationID initial_navigation_id;
  EXPECT_CALL(*mock_collector_, RecordStartNavigation(_))
      .WillOnce(SaveArg<0>(&initial_navigation_id));
  optimization_guide::OptimizationMetadata optimization_metadata;
  optimization_guide::proto::LoadingPredictorMetadata lp_metadata;
  lp_metadata.add_subresources()->set_url("http://test.org/resource1");
  lp_metadata.add_subresources()->set_url("http://other.org/resource2");
  lp_metadata.add_subresources()->set_url("http://other.org/resource3");
  optimization_metadata.set_loading_predictor_metadata(lp_metadata);
  optimization_guide::OptimizationGuideDecisionCallback callback;
  EXPECT_CALL(
      *mock_optimization_guide_keyed_service_,
      CanApplyOptimizationAsync(_, optimization_guide::proto::LOADING_PREDICTOR,
                                base::test::IsNotNullCallback()))
      .Times(3)
      .WillOnce(WithArg<2>(
          Invoke([&](optimization_guide::OptimizationGuideDecisionCallback
                         got_callback) -> void {
            callback = std::move(got_callback);
          })))
      .WillRepeatedly(Return());
  navigation->Start();
  navigation->Redirect(GURL("http://test2.org"));
  navigation->Redirect(GURL("http://test3.org"));

  std::move(callback).Run(optimization_guide::OptimizationGuideDecision::kTrue,
                          optimization_metadata);

  NavigationID new_navigation_id;
  EXPECT_CALL(*mock_collector_,
              RecordFinishNavigation(initial_navigation_id, _,
                                     /* is_error_page */ false))
      .WillOnce(SaveArg<1>(&new_navigation_id));
  navigation->Commit();

  // Prediction decision should be unknown since what came in was for the wrong
  // navigation ID.
  base::Optional<OptimizationGuidePrediction> optimization_guide_prediction =
      OptimizationGuidePrediction();
  optimization_guide_prediction->decision =
      optimization_guide::OptimizationGuideDecision::kUnknown;
  EXPECT_CALL(*mock_collector_,
              RecordMainFrameLoadComplete(new_navigation_id,
                                          optimization_guide_prediction));
  tab_helper_->DocumentOnLoadCompletedInMainFrame();

  histogram_tester.ExpectUniqueSample(
      "LoadingPredictor.OptimizationHintsReceiveStatus",
      OptimizationHintsReceiveStatus::kAfterRedirectOrNextNavigationStart, 1);
}

// Tests that document on load completed is recorded with correct navigation
// id and optimization guide prediction when the prediction has not arrived.
TEST_F(LoadingPredictorTabHelperOptimizationGuideDeciderTest,
       DocumentOnLoadCompletedOptimizationGuidePredictionHasNotArrived) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(
      *mock_optimization_guide_keyed_service_,
      CanApplyOptimizationAsync(_, optimization_guide::proto::LOADING_PREDICTOR,
                                base::test::IsNotNullCallback()));
  NavigateAndCommitInMainFrameAndVerifyMetrics("http://test.org");
  auto navigation_id =
      CreateNavigationID(GetTabID(), "http://test.org",
                         web_contents()->GetMainFrame()->GetPageUkmSourceId());

  // Adding subframe navigation to ensure that the committed main frame url will
  // be used.
  auto* subframe =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");
  NavigateAndCommitInFrame("http://sub.test.org", subframe);

  base::Optional<OptimizationGuidePrediction> prediction =
      OptimizationGuidePrediction();
  prediction->decision =
      optimization_guide::OptimizationGuideDecision::kUnknown;
  EXPECT_CALL(*mock_collector_,
              RecordMainFrameLoadComplete(navigation_id, prediction));
  tab_helper_->DocumentOnLoadCompletedInMainFrame();

  // Histogram should not be recorded since prediction did not come back.
  histogram_tester.ExpectTotalCount(
      "LoadingPredictor.OptimizationHintsReceiveStatus", 0);
}

// Tests that document on load completed is recorded with correct navigation
// id and optimization guide prediction and does not crash if callback comes
// after everything has been recorded.
TEST_F(
    LoadingPredictorTabHelperOptimizationGuideDeciderTest,
    DocumentOnLoadCompletedOptimizationGuidePredictionComesAfterDocumentOnLoad) {
  base::HistogramTester histogram_tester;

  optimization_guide::OptimizationMetadata optimization_metadata;
  optimization_guide::proto::LoadingPredictorMetadata lp_metadata;
  lp_metadata.add_subresources()->set_url("http://test.org/resource1");
  lp_metadata.add_subresources()->set_url("http://other.org/resource2");
  lp_metadata.add_subresources()->set_url("http://other.org/resource3");
  optimization_metadata.set_loading_predictor_metadata(lp_metadata);
  optimization_guide::OptimizationGuideDecisionCallback callback;
  EXPECT_CALL(
      *mock_optimization_guide_keyed_service_,
      CanApplyOptimizationAsync(_, optimization_guide::proto::LOADING_PREDICTOR,
                                base::test::IsNotNullCallback()))
      .WillOnce(WithArg<2>(
          Invoke([&](optimization_guide::OptimizationGuideDecisionCallback
                         got_callback) -> void {
            callback = std::move(got_callback);
          })));
  NavigateAndCommitInMainFrameAndVerifyMetrics("http://test.org");
  auto navigation_id =
      CreateNavigationID(GetTabID(), "http://test.org",
                         web_contents()->GetMainFrame()->GetPageUkmSourceId());

  // Adding subframe navigation to ensure that the committed main frame url will
  // be used.
  auto* subframe =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");
  NavigateAndCommitInFrame("http://sub.test.org", subframe);

  base::Optional<OptimizationGuidePrediction> prediction =
      OptimizationGuidePrediction();
  prediction->decision =
      optimization_guide::OptimizationGuideDecision::kUnknown;
  EXPECT_CALL(*mock_collector_,
              RecordMainFrameLoadComplete(navigation_id, prediction));
  tab_helper_->DocumentOnLoadCompletedInMainFrame();

  // Invoke callback after document completed in main frame..
  std::move(callback).Run(optimization_guide::OptimizationGuideDecision::kTrue,
                          optimization_metadata);

  // Optimization guide predictions came after commit.
  histogram_tester.ExpectUniqueSample(
      "LoadingPredictor.OptimizationHintsReceiveStatus",
      OptimizationHintsReceiveStatus::kAfterNavigationFinish, 1);
}

// Tests that document on load completed is recorded with correct navigation
// id and optimization guide prediction with no prediction..
TEST_F(LoadingPredictorTabHelperOptimizationGuideDeciderTest,
       DocumentOnLoadCompletedOptimizationGuidePredictionArrivedNoPrediction) {
  base::HistogramTester histogram_tester;

  // The problem here is that mock_collector_ is a strict mock, which expects
  // a particular set of loading events and fails when extra is present.
  // TOOO(ahemery): Consider refactoring this to rely on loading events
  // in NavigationSimulator.
  optimization_guide::OptimizationMetadata optimization_metadata;
  EXPECT_CALL(
      *mock_optimization_guide_keyed_service_,
      CanApplyOptimizationAsync(_, optimization_guide::proto::LOADING_PREDICTOR,
                                base::test::IsNotNullCallback()))
      .WillOnce(base::test::RunOnceCallback<2>(
          optimization_guide::OptimizationGuideDecision::kFalse,
          ByRef(optimization_metadata)));
  NavigateAndCommitInMainFrameAndVerifyMetrics("http://test.org");
  auto navigation_id =
      CreateNavigationID(GetTabID(), "http://test.org",
                         web_contents()->GetMainFrame()->GetPageUkmSourceId());

  // Adding subframe navigation to ensure that the committed main frame url will
  // be used.
  auto* subframe =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");
  NavigateAndCommitInFrame("http://sub.test.org", subframe);

  base::Optional<OptimizationGuidePrediction> prediction =
      OptimizationGuidePrediction();
  prediction->decision = optimization_guide::OptimizationGuideDecision::kFalse;
  EXPECT_CALL(*mock_collector_,
              RecordMainFrameLoadComplete(navigation_id, prediction));
  tab_helper_->DocumentOnLoadCompletedInMainFrame();

  // Histogram should still be recorded even though no predictions were
  // returned.
  histogram_tester.ExpectUniqueSample(
      "LoadingPredictor.OptimizationHintsReceiveStatus",
      OptimizationHintsReceiveStatus::kBeforeNavigationFinish, 1);
}

// Tests that document on load completed is recorded with correct navigation
// id and optimization guide prediction with no prediction..
TEST_F(
    LoadingPredictorTabHelperOptimizationGuideDeciderTest,
    DocumentOnLoadCompletedOptimizationGuidePredictionArrivedNoLoadingPredictorMetadata) {
  base::HistogramTester histogram_tester;

  // The problem here is that mock_collector_ is a strict mock, which expects
  // a particular set of loading events and fails when extra is present.
  // TOOO(ahemery): Consider refactoring this to rely on loading events
  // in NavigationSimulator.
  optimization_guide::OptimizationMetadata optimization_metadata;
  EXPECT_CALL(
      *mock_optimization_guide_keyed_service_,
      CanApplyOptimizationAsync(_, optimization_guide::proto::LOADING_PREDICTOR,
                                base::test::IsNotNullCallback()))
      .WillOnce(base::test::RunOnceCallback<2>(
          optimization_guide::OptimizationGuideDecision::kTrue,
          ByRef(optimization_metadata)));
  NavigateAndCommitInMainFrameAndVerifyMetrics("http://test.org");
  auto navigation_id =
      CreateNavigationID(GetTabID(), "http://test.org",
                         web_contents()->GetMainFrame()->GetPageUkmSourceId());

  // Adding subframe navigation to ensure that the committed main frame url will
  // be used.
  auto* subframe =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");
  NavigateAndCommitInFrame("http://sub.test.org", subframe);

  // Decision should be unknown since we got invalid data.
  base::Optional<OptimizationGuidePrediction> optimization_guide_prediction =
      OptimizationGuidePrediction();
  optimization_guide_prediction->decision =
      optimization_guide::OptimizationGuideDecision::kUnknown;
  EXPECT_CALL(*mock_collector_,
              RecordMainFrameLoadComplete(navigation_id,
                                          optimization_guide_prediction));
  tab_helper_->DocumentOnLoadCompletedInMainFrame();

  // Histogram should still be recorded even though no predictions were
  // returned.
  histogram_tester.ExpectUniqueSample(
      "LoadingPredictor.OptimizationHintsReceiveStatus",
      OptimizationHintsReceiveStatus::kBeforeNavigationFinish, 1);
}

class LoadingPredictorTabHelperOptimizationGuideDeciderWithPrefetchTest
    : public LoadingPredictorTabHelperOptimizationGuideDeciderTest {
 public:
  LoadingPredictorTabHelperOptimizationGuideDeciderWithPrefetchTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kLoadingPredictorUseOptimizationGuide,
         features::kLoadingPredictorPrefetch,
         // Need to add otherwise GetForProfile() returns null.
         optimization_guide::features::kOptimizationHints},
        {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that document on load completed is recorded with correct navigation
// id and optimization guide prediction.
TEST_F(LoadingPredictorTabHelperOptimizationGuideDeciderWithPrefetchTest,
       DocumentOnLoadCompletedOptimizationGuide) {
  base::HistogramTester histogram_tester;

  optimization_guide::OptimizationMetadata optimization_metadata;
  optimization_guide::proto::LoadingPredictorMetadata lp_metadata;
  lp_metadata.add_subresources()->set_url("http://test.org/resource1");
  lp_metadata.add_subresources()->set_url("http://other.org/resource2");
  lp_metadata.add_subresources()->set_url("http://other.org/resource3");
  auto* preconnect_only_resource = lp_metadata.add_subresources();
  preconnect_only_resource->set_url("http://preconnectonly.com/");
  preconnect_only_resource->set_preconnect_only(true);
  optimization_metadata.set_loading_predictor_metadata(lp_metadata);
  EXPECT_CALL(
      *mock_optimization_guide_keyed_service_,
      CanApplyOptimizationAsync(_, optimization_guide::proto::LOADING_PREDICTOR,
                                base::test::IsNotNullCallback()))
      .WillOnce(base::test::RunOnceCallback<2>(
          optimization_guide::OptimizationGuideDecision::kTrue,
          ByRef(optimization_metadata)));
  NavigateAndCommitInMainFrameAndVerifyMetrics("http://test.org");
  auto navigation_id =
      CreateNavigationID(GetTabID(), "http://test.org",
                         web_contents()->GetMainFrame()->GetPageUkmSourceId());

  // Adding subframe navigation to ensure that the committed main frame url will
  // be used.
  auto* subframe =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");
  NavigateAndCommitInFrame("http://sub.test.org", subframe);

  base::Optional<OptimizationGuidePrediction> prediction =
      OptimizationGuidePrediction();
  prediction->decision = optimization_guide::OptimizationGuideDecision::kTrue;
  url::Origin main_frame_origin = url::Origin::Create(GURL("http://test.org"));
  net::NetworkIsolationKey network_isolation_key(main_frame_origin,
                                                 main_frame_origin);
  network::mojom::RequestDestination destination =
      network::mojom::RequestDestination::kEmpty;
  PreconnectPrediction preconnect_prediction = CreatePreconnectPrediction(
      "", false,
      {{url::Origin::Create(GURL("http://preconnectonly.com/")), 1,
        network_isolation_key}});
  preconnect_prediction.prefetch_requests.emplace_back(
      GURL("http://test.org/resource1"), network_isolation_key, destination);
  preconnect_prediction.prefetch_requests.emplace_back(
      GURL("http://other.org/resource1"), network_isolation_key, destination);
  preconnect_prediction.prefetch_requests.emplace_back(
      GURL("http://other.org/resource2"), network_isolation_key, destination);
  prediction->preconnect_prediction = preconnect_prediction;
  prediction->predicted_subresources = {
      GURL("http://test.org/resource1"), GURL("http://other.org/resource2"),
      GURL("http://other.org/resource3"), GURL("http://preconnectonly.com/")};
  EXPECT_CALL(*mock_collector_,
              RecordMainFrameLoadComplete(navigation_id, prediction));
  tab_helper_->DocumentOnLoadCompletedInMainFrame();

  histogram_tester.ExpectUniqueSample(
      "LoadingPredictor.OptimizationHintsReceiveStatus",
      OptimizationHintsReceiveStatus::kBeforeNavigationFinish, 1);
}

class TestLoadingDataCollector : public LoadingDataCollector {
 public:
  explicit TestLoadingDataCollector(const LoadingPredictorConfig& config);

  void RecordStartNavigation(const NavigationID& navigation_id) override {}
  void RecordFinishNavigation(const NavigationID& old_navigation_id,
                              const NavigationID& new_navigation_id,
                              bool is_error_page) override {}
  void RecordResourceLoadComplete(
      const NavigationID& navigation_id,
      const blink::mojom::ResourceLoadInfo& resource_load_info) override {
    ++count_resource_loads_completed_;
    EXPECT_EQ(expected_request_priority_, resource_load_info.request_priority);
  }

  void RecordMainFrameLoadComplete(
      const NavigationID& navigation_id,
      const base::Optional<OptimizationGuidePrediction>&
          optimization_guide_prediction) override {}

  void RecordFirstContentfulPaint(
      const NavigationID& navigation_id,
      const base::TimeTicks& first_contentful_paint) override {}

  void SetExpectedResourcePriority(net::RequestPriority request_priority) {
    expected_request_priority_ = request_priority;
  }

  size_t count_resource_loads_completed() const {
    return count_resource_loads_completed_;
  }

 private:
  net::RequestPriority expected_request_priority_ = net::HIGHEST;
  size_t count_resource_loads_completed_ = 0;
};

TestLoadingDataCollector::TestLoadingDataCollector(
    const LoadingPredictorConfig& config)
    : LoadingDataCollector(nullptr, nullptr, config) {}

class LoadingPredictorTabHelperTestCollectorTest
    : public LoadingPredictorTabHelperTest {
 public:
  void SetUp() override;

 protected:
  TestLoadingDataCollector* test_collector_;
};

void LoadingPredictorTabHelperTestCollectorTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();
  CreateSessionServiceTabHelper(web_contents());
  LoadingPredictorTabHelper::CreateForWebContents(web_contents());
  tab_helper_ = LoadingPredictorTabHelper::FromWebContents(web_contents());

  LoadingPredictorConfig config;
  PopulateTestConfig(&config);
  loading_predictor_ = std::make_unique<LoadingPredictor>(config, profile());

  auto test_collector = std::make_unique<TestLoadingDataCollector>(config);
  test_collector_ = test_collector.get();
  loading_predictor_->set_mock_loading_data_collector(
      std::move(test_collector));

  tab_helper_->SetLoadingPredictorForTesting(loading_predictor_->GetWeakPtr());
}

// Tests that a resource load is correctly recorded with the correct priority.
TEST_F(LoadingPredictorTabHelperTestCollectorTest, ResourceLoadComplete) {
  NavigateAndCommitInFrame("http://test.org", main_rfh());

  // Set expected priority to HIGHEST and load a HIGHEST priority resource.
  test_collector_->SetExpectedResourcePriority(net::HIGHEST);
  auto resource_load_info = CreateResourceLoadInfo(
      "http://test.org/script.js", network::mojom::RequestDestination::kScript);
  tab_helper_->ResourceLoadComplete(main_rfh(), content::GlobalRequestID(),
                                    *resource_load_info);
  EXPECT_EQ(1u, test_collector_->count_resource_loads_completed());

  // Set expected priority to LOWEST and load a LOWEST priority resource.
  test_collector_->SetExpectedResourcePriority(net::LOWEST);
  resource_load_info = CreateLowPriorityResourceLoadInfo(
      "http://test.org/script.js", network::mojom::RequestDestination::kScript);
  tab_helper_->ResourceLoadComplete(main_rfh(), content::GlobalRequestID(),
                                    *resource_load_info);
  EXPECT_EQ(2u, test_collector_->count_resource_loads_completed());
}

}  // namespace predictors
