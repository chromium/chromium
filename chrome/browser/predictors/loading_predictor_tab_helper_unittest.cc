// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/loading_predictor_tab_helper.h"

#include "base/memory/raw_ptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_test_util.h"
#include "chrome/browser/predictors/predictors_enums.h"
#include "chrome/browser/predictors/predictors_features.h"
#include "chrome/browser/sessions/session_tab_helper_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom.h"

using ::testing::_;
using ::testing::An;
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
  MOCK_METHOD4(RecordStartNavigation,
               void(NavigationId, ukm::SourceId, const GURL&, base::TimeTicks));

  MOCK_METHOD3(RecordFinishNavigation, void(NavigationId, const GURL&, bool));
  MOCK_METHOD2(RecordResourceLoadComplete,
               void(NavigationId, const blink::mojom::ResourceLoadInfo&));
  MOCK_METHOD1(RecordMainFrameLoadComplete, void(NavigationId));
  MOCK_METHOD2(RecordPageDestroyed,
               void(NavigationId,
                    const std::optional<OptimizationGuidePrediction>&));
};

MockLoadingDataCollector::MockLoadingDataCollector(
    const LoadingPredictorConfig& config)
    : LoadingDataCollector(nullptr, nullptr, config) {}

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
  raw_ptr<StrictMock<MockLoadingDataCollector>> mock_collector_;
  // Owned elsewhere.
  raw_ptr<NiceMock<MockOptimizationGuideKeyedService>, DanglingUntriaged>
      mock_optimization_guide_keyed_service_;
  // Owned by |web_contents()|.
  raw_ptr<LoadingPredictorTabHelper, DanglingUntriaged> tab_helper_;
};

void LoadingPredictorTabHelperTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();
  CreateSessionServiceTabHelper(web_contents());
  mock_optimization_guide_keyed_service_ =
      static_cast<NiceMock<MockOptimizationGuideKeyedService>*>(
          OptimizationGuideKeyedServiceFactory::GetInstance()
              ->SetTestingFactoryAndUse(
                  profile(),
                  base::BindRepeating([](content::BrowserContext* context)
                                          -> std::unique_ptr<KeyedService> {
                    return std::make_unique<
                        NiceMock<MockOptimizationGuideKeyedService>>();
                  })));
  LoadingPredictorTabHelper::CreateForWebContents(web_contents());
  tab_helper_ = LoadingPredictorTabHelper::FromWebContents(web_contents());

  LoadingPredictorConfig config;
  PopulateTestConfig(&config);
  loading_predictor_ = std::make_unique<LoadingPredictor>(config, profile());

  auto mock_collector =
      std::make_unique<StrictMock<MockLoadingDataCollector>>(config);
  mock_collector_ = mock_collector.get();
  // RecordPageDestroyed will be called at the end of all tests when a page
  // is destroyed, so we add a default catch-all expectation here. Tests can
  // override this with more specific expectations inside the test.
  EXPECT_CALL(*mock_collector_, RecordPageDestroyed)
      .Times(testing::AnyNumber());
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
  ukm::SourceId ukm_source_id;
  GURL main_frame_url;
  GURL new_main_frame_url;
  EXPECT_CALL(*mock_collector_, RecordStartNavigation(_, _, _, _))
      .WillOnce(DoAll(SaveArg<1>(&ukm_source_id), SaveArg<2>(&main_frame_url)));
  EXPECT_CALL(*mock_collector_,
              RecordFinishNavigation(_, _,
                                     /* is_error_page */ false))
      .WillOnce(DoAll(SaveArg<1>(&new_main_frame_url)));

  NavigateAndCommitInFrame(url, main_rfh());

  EXPECT_EQ(ukm_source_id,
            web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId());
  GURL gurl(url);
  EXPECT_EQ(gurl, main_frame_url);
  EXPECT_EQ(gurl, new_main_frame_url);
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
  // TODO(crbug.com/40276923): Consider refactoring to rely on load
  // events dispatched by NavigationSimulator.
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
  GURL main_frame_url("http://test.org");
  auto navigation = content::NavigationSimulator::CreateRendererInitiated(
      main_frame_url, main_rfh());
  // The problem here is that mock_collector_ is a strict mock, which expects
  // a particular set of loading events and fails when extra is present.
  // TOOO(https://crbug.com/1467792): Consider refactoring this to rely on
  // loading events in NavigationSimulator.
  navigation->SetKeepLoading(true);
  ukm::SourceId ukm_source_id;
  EXPECT_CALL(*mock_collector_, RecordStartNavigation(_, _, main_frame_url, _))
      .WillOnce(SaveArg<1>(&ukm_source_id));
  navigation->Start();
  navigation->Redirect(GURL("http://test2.org"));
  navigation->Redirect(GURL("http://test3.org"));
  GURL expected_main_frame_url("http://test3.org");
  EXPECT_CALL(*mock_collector_,
              RecordFinishNavigation(_, expected_main_frame_url, _));
  navigation->Commit();

  EXPECT_EQ(web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId(),
            ukm_source_id);
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
  GURL url("http://test.org");
  auto navigation =
      content::NavigationSimulator::CreateRendererInitiated(url, main_rfh());
  navigation->SetKeepLoading(true);
  // The problem here is that mock_collector_ is a strict mock, which expects
  // a particular set of loading events and fails when extra is present.
  // TOOO(https://crbug.com/1467792): Consider refactoring this to rely on
  // loading events in NavigationSimulator.
  ukm::SourceId ukm_source_id;
  EXPECT_CALL(*mock_collector_, RecordStartNavigation(_, _, url, _))
      .WillOnce(SaveArg<1>(&ukm_source_id));
  navigation->Start();

  EXPECT_CALL(*mock_collector_,
              RecordFinishNavigation(_, url,
                                     /* is_error_page */ true));
  navigation->Fail(net::ERR_TIMED_OUT);
  navigation->CommitErrorPage();

  EXPECT_EQ(ukm_source_id,
            web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId());
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

  // Adding subframe navigation to ensure that the committed main frame url will
  // be used.
  auto* subframe =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");
  NavigateAndCommitInFrame("http://sub.test.org", subframe);

  EXPECT_CALL(*mock_collector_, RecordMainFrameLoadComplete(_));
  tab_helper_->DocumentOnLoadCompletedInPrimaryMainFrame();
}

// Tests that a resource load is correctly recorded.
TEST_F(LoadingPredictorTabHelperTest, ResourceLoadComplete) {
  NavigateAndCommitInMainFrameAndVerifyMetrics("http://test.org");

  auto resource_load_info = CreateResourceLoadInfo(
      "http://test.org/script.js", network::mojom::RequestDestination::kScript);
  EXPECT_CALL(*mock_collector_,
              RecordResourceLoadComplete(_, Eq(ByRef(*resource_load_info))));
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

  auto resource_load_info = CreateResourceLoadInfo(
      "http://test.org/script.js", network::mojom::RequestDestination::kScript,
      false);
  resource_load_info->mime_type = "application/javascript";
  resource_load_info->network_info->network_accessed = false;
  EXPECT_CALL(*mock_collector_,
              RecordResourceLoadComplete(_, Eq(ByRef(*resource_load_info))));
  tab_helper_->DidLoadResourceFromMemoryCache(
      main_rfh(), GURL("http://test.org/script.js"), "application/javascript",
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

        {
            // Disabling prefetch here to test preconnect passthrough. Prefetch
            // is tested in following test class.
            features::kLoadingPredictorPrefetch,
            // Disable local predictions to ensure that opt guide logic is
            // consulted.
            features::kLoadingPredictorUseLocalPredictions,
        });
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that document on load completed is recorded with correct navigation
// id and that optimization guide is not consulted when from same-origin.
TEST_F(LoadingPredictorTabHelperOptimizationGuideDeciderTest,
       DocumentOnLoadCompletedOptimizationGuideSameOrigin) {
  NavigateAndCommitInMainFrameAndVerifyMetrics("http://test.org");

  base::HistogramTester histogram_tester;

  EXPECT_CALL(*mock_collector_, RecordPageDestroyed(_, Eq(std::nullopt)));
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              CanApplyOptimization(
                  _, optimization_guide::proto::LOADING_PREDICTOR,
                  An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .Times(0);
  NavigateAndCommitInMainFrameAndVerifyMetrics("http://test.org/otherpage");

  EXPECT_CALL(*mock_collector_, RecordMainFrameLoadComplete(_));
  tab_helper_->DocumentOnLoadCompletedInPrimaryMainFrame();

  histogram_tester.ExpectTotalCount(
      "LoadingPredictor.OptimizationHintsReceiveStatus", 0);
}

// Tests that document on load completed is recorded.
TEST_F(LoadingPredictorTabHelperOptimizationGuideDeciderTest,
       DocumentOnLoadCompletedOptimizationGuide) {
  base::HistogramTester histogram_tester;

  optimization_guide::OptimizationMetadata optimization_metadata;
  optimization_guide::proto::LoadingPredictorMetadata lp_metadata;
  lp_metadata.add_subresources()->set_url("http://test.org/resource1");
  lp_metadata.add_subresources()->set_url("http://other.org/resource2");
  lp_metadata.add_subresources()->set_url("http://other.org/resource3");
  optimization_metadata.set_loading_predictor_metadata(lp_metadata);
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              CanApplyOptimization(
                  _, optimization_guide::proto::LOADING_PREDICTOR,
                  An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .WillOnce(base::test::RunOnceCallback<2>(
          optimization_guide::OptimizationGuideDecision::kTrue,
          ByRef(optimization_metadata)));
  NavigateAndCommitInMainFrameAndVerifyMetrics("http://test.org");

  EXPECT_CALL(*mock_collector_, RecordMainFrameLoadComplete(_));
  tab_helper_->DocumentOnLoadCompletedInPrimaryMainFrame();

  histogram_tester.ExpectUniqueSample(
      "LoadingPredictor.OptimizationHintsReceiveStatus",
      OptimizationHintsReceiveStatus::kBeforeNavigationFinish, 1);
}

// Tests that page destruction is recorded with the correct navigation id and
// optimization guide prediction.
TEST_F(LoadingPredictorTabHelperOptimizationGuideDeciderTest,
       PageDestroyedOptimizationGuide) {
  base::HistogramTester histogram_tester;

  optimization_guide::OptimizationMetadata optimization_metadata;
  optimization_guide::proto::LoadingPredictorMetadata lp_metadata;
  lp_metadata.add_subresources()->set_url("http://test.org/resource1");
  lp_metadata.add_subresources()->set_url("http://other.org/resource2");
  lp_metadata.add_subresources()->set_url("http://other.org/resource3");
  optimization_metadata.set_loading_predictor_metadata(lp_metadata);
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              CanApplyOptimization(
                  _, optimization_guide::proto::LOADING_PREDICTOR,
                  An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .WillOnce(base::test::RunOnceCallback<2>(
          optimization_guide::OptimizationGuideDecision::kTrue,
          ByRef(optimization_metadata)));
  NavigateAndCommitInMainFrameAndVerifyMetrics("http://test.org");

  std::optional<OptimizationGuidePrediction> prediction =
      OptimizationGuidePrediction();
  prediction->decision = optimization_guide::OptimizationGuideDecision::kTrue;
  net::SchemefulSite main_frame_site =
      net::SchemefulSite(GURL("http://test.org"));
  PreconnectPrediction preconnect_prediction = CreatePreconnectPrediction(
      "", false,
      {{url::Origin::Create(GURL("http://other.org")), 1,
        net::NetworkAnonymizationKey::CreateSameSite(main_frame_site)}});
  prediction->preconnect_prediction = preconnect_prediction;
  prediction->predicted_subresources = {GURL("http://test.org/resource1"),
                                        GURL("http://other.org/resource2"),
                                        GURL("http://other.org/resource3")};

  histogram_tester.ExpectUniqueSample(
      "LoadingPredictor.OptimizationHintsReceiveStatus",
      OptimizationHintsReceiveStatus::kBeforeNavigationFinish, 1);

  // Called when the frame is destroyed by the test destructor.
  EXPECT_CALL(*mock_collector_, RecordPageDestroyed(_, prediction));
}

// Tests that predictions are recorded correctly when they come after the
// navigation commits.
TEST_F(LoadingPredictorTabHelperOptimizationGuideDeciderTest,
       PageDestroyedOptimizationGuidePredictionComesAfterCommit) {
  base::HistogramTester histogram_tester;

  optimization_guide::OptimizationMetadata optimization_metadata;
  optimization_guide::proto::LoadingPredictorMetadata lp_metadata;
  lp_metadata.add_subresources()->set_url("http://test.org/resource1");
  lp_metadata.add_subresources()->set_url("http://other.org/resource2");
  lp_metadata.add_subresources()->set_url("http://other.org/resource3");
  optimization_metadata.set_loading_predictor_metadata(lp_metadata);
  optimization_guide::OptimizationGuideDecisionCallback callback;
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              CanApplyOptimization(
                  _, optimization_guide::proto::LOADING_PREDICTOR,
                  An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .WillOnce(WithArg<2>(
          Invoke([&](optimization_guide::OptimizationGuideDecisionCallback
                         got_callback) -> void {
            callback = std::move(got_callback);
          })));
  NavigateAndCommitInMainFrameAndVerifyMetrics("http://test.org");

  // Invoke callback after commit.
  std::move(callback).Run(optimization_guide::OptimizationGuideDecision::kTrue,
                          optimization_metadata);

  // Optimization guide predictions came after commit.
  histogram_tester.ExpectUniqueSample(
      "LoadingPredictor.OptimizationHintsReceiveStatus",
      OptimizationHintsReceiveStatus::kAfterNavigationFinish, 1);

  std::optional<OptimizationGuidePrediction> prediction =
      OptimizationGuidePrediction();
  prediction->decision = optimization_guide::OptimizationGuideDecision::kTrue;
  net::SchemefulSite main_frame_site =
      net::SchemefulSite(GURL("http://test.org"));
  PreconnectPrediction preconnect_prediction = CreatePreconnectPrediction(
      "", false,
      {{url::Origin::Create(GURL("http://other.org")), 1,
        net::NetworkAnonymizationKey::CreateSameSite(main_frame_site)}});
  prediction->preconnect_prediction = preconnect_prediction;
  prediction->predicted_subresources = {GURL("http://test.org/resource1"),
                                        GURL("http://other.org/resource2"),
                                        GURL("http://other.org/resource3")};
  // Called when the frame is destroyed by the test destructor.
  EXPECT_CALL(*mock_collector_, RecordPageDestroyed(_, prediction));
}

// Tests that predictions are recorded correctly when they arrive after a
// redirect.
TEST_F(LoadingPredictorTabHelperOptimizationGuideDeciderTest,
       PageDestroyedOptimizationGuidePredictionArrivedAfterRedirect) {
  base::HistogramTester histogram_tester;

  auto navigation = content::NavigationSimulator::CreateRendererInitiated(
      GURL("http://test.org"), main_rfh());
  // The problem here is that mock_collector_ is a strict mock, which expects
  // a particular set of loading events and fails when extra is present.
  // TOOO(ahemery): Consider refactoring this to rely on loading events
  // in NavigationSimulator.
  navigation->SetKeepLoading(true);
  EXPECT_CALL(*mock_collector_, RecordStartNavigation(_, _, _, _));
  optimization_guide::OptimizationMetadata optimization_metadata;
  optimization_guide::proto::LoadingPredictorMetadata lp_metadata;
  lp_metadata.add_subresources()->set_url("http://test.org/resource1");
  lp_metadata.add_subresources()->set_url("http://other.org/resource2");
  lp_metadata.add_subresources()->set_url("http://other.org/resource3");
  optimization_metadata.set_loading_predictor_metadata(lp_metadata);
  optimization_guide::OptimizationGuideDecisionCallback callback;
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              CanApplyOptimization(
                  _, optimization_guide::proto::LOADING_PREDICTOR,
                  An<optimization_guide::OptimizationGuideDecisionCallback>()))
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

  EXPECT_CALL(*mock_collector_,
              RecordFinishNavigation(_, _,
                                     /* is_error_page */ false));
  navigation->Commit();

  histogram_tester.ExpectUniqueSample(
      "LoadingPredictor.OptimizationHintsReceiveStatus",
      OptimizationHintsReceiveStatus::kAfterRedirectOrNextNavigationStart, 1);

  // Prediction decision should be unknown since what came in was for the wrong
  // navigation ID.
  std::optional<OptimizationGuidePrediction> optimization_guide_prediction =
      OptimizationGuidePrediction();
  optimization_guide_prediction->decision =
      optimization_guide::OptimizationGuideDecision::kUnknown;
  EXPECT_CALL(*mock_collector_,
              RecordPageDestroyed(_, optimization_guide_prediction));
  optimization_guide::OptimizationMetadata optimization_metadata_2;
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              CanApplyOptimization(
                  _, optimization_guide::proto::LOADING_PREDICTOR,
                  An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .WillOnce(base::test::RunOnceCallback<2>(
          optimization_guide::OptimizationGuideDecision::kFalse,
          ByRef(optimization_metadata_2)));
  NavigateAndCommitInMainFrameAndVerifyMetrics("http://test.org/next.html");
}

// Tests that page destruction is recorded with correct navigation id and
// optimization guide prediction when the prediction has not arrived.
TEST_F(LoadingPredictorTabHelperOptimizationGuideDeciderTest,
       PageDestroyedOptimizationGuidePredictionHasNotArrived) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              CanApplyOptimization(
                  _, optimization_guide::proto::LOADING_PREDICTOR,
                  An<optimization_guide::OptimizationGuideDecisionCallback>()));
  NavigateAndCommitInMainFrameAndVerifyMetrics("http://test.org");

  // Histogram should not be recorded since prediction did not come back.
  histogram_tester.ExpectTotalCount(
      "LoadingPredictor.OptimizationHintsReceiveStatus", 0);

  std::optional<OptimizationGuidePrediction> prediction =
      OptimizationGuidePrediction();
  prediction->decision =
      optimization_guide::OptimizationGuideDecision::kUnknown;
  // Called when the frame is destroyed by the test destructor.
  EXPECT_CALL(*mock_collector_, RecordPageDestroyed(_, prediction));
}

// Tests that page destroyed is recorded with correct navigation id and
// optimization guide prediction and does not crash if callback comes after
// everything has been recorded.
TEST_F(LoadingPredictorTabHelperOptimizationGuideDeciderTest,
       PageDestroyedOptimizationGuidePredictionComesAfterPageDestroyed) {
  base::HistogramTester histogram_tester;

  optimization_guide::OptimizationMetadata optimization_metadata;
  optimization_guide::proto::LoadingPredictorMetadata lp_metadata;
  lp_metadata.add_subresources()->set_url("http://test.org/resource1");
  lp_metadata.add_subresources()->set_url("http://other.org/resource2");
  lp_metadata.add_subresources()->set_url("http://other.org/resource3");
  optimization_metadata.set_loading_predictor_metadata(lp_metadata);
  optimization_guide::OptimizationGuideDecisionCallback callback;
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              CanApplyOptimization(
                  _, optimization_guide::proto::LOADING_PREDICTOR,
                  An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .WillOnce(WithArg<2>(
          Invoke([&](optimization_guide::OptimizationGuideDecisionCallback
                         got_callback) -> void {
            callback = std::move(got_callback);
          })));
  NavigateAndCommitInMainFrameAndVerifyMetrics("http://test.org");

  std::optional<OptimizationGuidePrediction> prediction =
      OptimizationGuidePrediction();
  prediction->decision =
      optimization_guide::OptimizationGuideDecision::kUnknown;
  EXPECT_CALL(*mock_collector_, RecordPageDestroyed(_, prediction));
  optimization_guide::OptimizationMetadata optimization_metadata_2;
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              CanApplyOptimization(
                  _, optimization_guide::proto::LOADING_PREDICTOR,
                  An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .WillOnce(base::test::RunOnceCallback<2>(
          optimization_guide::OptimizationGuideDecision::kFalse,
          ByRef(optimization_metadata_2)));
  NavigateAndCommitInMainFrameAndVerifyMetrics("http://site.net");

  // Invoke callback after page destroyed.
  std::move(callback).Run(optimization_guide::OptimizationGuideDecision::kTrue,
                          optimization_metadata);

  // Optimization guide predictions came after commit.
  histogram_tester.ExpectBucketCount(
      "LoadingPredictor.OptimizationHintsReceiveStatus",
      OptimizationHintsReceiveStatus::kAfterNavigationFinish, 1);
}

// Tests that page destruction is recorded with correct navigation and
// optimization guide prediction with no prediction..
TEST_F(LoadingPredictorTabHelperOptimizationGuideDeciderTest,
       PageDestroyedOptimizationGuidePredictionArrivedNoPrediction) {
  base::HistogramTester histogram_tester;

  // The problem here is that mock_collector_ is a strict mock, which expects
  // a particular set of loading events and fails when extra is present.
  // TOOO(ahemery): Consider refactoring this to rely on loading events
  // in NavigationSimulator.
  optimization_guide::OptimizationMetadata optimization_metadata;
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              CanApplyOptimization(
                  _, optimization_guide::proto::LOADING_PREDICTOR,
                  An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .WillOnce(base::test::RunOnceCallback<2>(
          optimization_guide::OptimizationGuideDecision::kFalse,
          ByRef(optimization_metadata)));
  NavigateAndCommitInMainFrameAndVerifyMetrics("http://test.org");

  // Histogram should still be recorded even though no predictions were
  // returned.
  histogram_tester.ExpectUniqueSample(
      "LoadingPredictor.OptimizationHintsReceiveStatus",
      OptimizationHintsReceiveStatus::kBeforeNavigationFinish, 1);

  std::optional<OptimizationGuidePrediction> prediction =
      OptimizationGuidePrediction();
  prediction->decision = optimization_guide::OptimizationGuideDecision::kFalse;
  // Called when the frame is destroyed by the test destructor.
  EXPECT_CALL(*mock_collector_, RecordPageDestroyed(_, prediction));
}

// Tests that page destruction is recorded with correct navigation id and
// optimization guide prediction with no prediction..
TEST_F(
    LoadingPredictorTabHelperOptimizationGuideDeciderTest,
    PageDestroyedOptimizationGuidePredictionArrivedNoLoadingPredictorMetadata) {
  base::HistogramTester histogram_tester;

  // The problem here is that mock_collector_ is a strict mock, which expects
  // a particular set of loading events and fails when extra is present.
  // TOOO(ahemery): Consider refactoring this to rely on loading events
  // in NavigationSimulator.
  optimization_guide::OptimizationMetadata optimization_metadata;
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              CanApplyOptimization(
                  _, optimization_guide::proto::LOADING_PREDICTOR,
                  An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .WillOnce(base::test::RunOnceCallback<2>(
          optimization_guide::OptimizationGuideDecision::kTrue,
          ByRef(optimization_metadata)));
  NavigateAndCommitInMainFrameAndVerifyMetrics("http://test.org");

  // Histogram should still be recorded even though no predictions were
  // returned.
  histogram_tester.ExpectUniqueSample(
      "LoadingPredictor.OptimizationHintsReceiveStatus",
      OptimizationHintsReceiveStatus::kBeforeNavigationFinish, 1);

  // Decision should be unknown since we got invalid data.
  std::optional<OptimizationGuidePrediction> optimization_guide_prediction =
      OptimizationGuidePrediction();
  optimization_guide_prediction->decision =
      optimization_guide::OptimizationGuideDecision::kUnknown;
  // Called when the frame is destroyed by the test destructor.
  EXPECT_CALL(*mock_collector_,
              RecordPageDestroyed(_, optimization_guide_prediction));
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
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
};

// Tests that page destruction is recorded with correct navigation id and
// optimization guide prediction.
TEST_F(LoadingPredictorTabHelperOptimizationGuideDeciderWithPrefetchTest,
       PageDestroyedOptimizationGuide) {
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
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              CanApplyOptimization(
                  _, optimization_guide::proto::LOADING_PREDICTOR,
                  An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .WillOnce(base::test::RunOnceCallback<2>(
          optimization_guide::OptimizationGuideDecision::kTrue,
          ByRef(optimization_metadata)));
  NavigateAndCommitInMainFrameAndVerifyMetrics("http://test.org");

  histogram_tester.ExpectUniqueSample(
      "LoadingPredictor.OptimizationHintsReceiveStatus",
      OptimizationHintsReceiveStatus::kBeforeNavigationFinish, 1);

  std::optional<OptimizationGuidePrediction> prediction =
      OptimizationGuidePrediction();
  prediction->decision = optimization_guide::OptimizationGuideDecision::kTrue;
  net::SchemefulSite main_frame_site =
      net::SchemefulSite(GURL("http://test.org"));
  auto network_anonymization_key =
      net::NetworkAnonymizationKey::CreateSameSite(main_frame_site);
  network::mojom::RequestDestination destination =
      network::mojom::RequestDestination::kEmpty;
  PreconnectPrediction preconnect_prediction = CreatePreconnectPrediction(
      "", false,
      {{url::Origin::Create(GURL("http://preconnectonly.com/")), 1,
        network_anonymization_key}});
  preconnect_prediction.prefetch_requests.emplace_back(
      GURL("http://test.org/resource1"), network_anonymization_key,
      destination);
  preconnect_prediction.prefetch_requests.emplace_back(
      GURL("http://other.org/resource1"), network_anonymization_key,
      destination);
  preconnect_prediction.prefetch_requests.emplace_back(
      GURL("http://other.org/resource2"), network_anonymization_key,
      destination);
  prediction->preconnect_prediction = preconnect_prediction;
  prediction->predicted_subresources = {
      GURL("http://test.org/resource1"), GURL("http://other.org/resource2"),
      GURL("http://other.org/resource3"), GURL("http://preconnectonly.com/")};

  // Called when frame is destroyed by test destructor.
  EXPECT_CALL(*mock_collector_, RecordPageDestroyed(_, prediction));
}

class TestLoadingDataCollector : public LoadingDataCollector {
 public:
  explicit TestLoadingDataCollector(const LoadingPredictorConfig& config);

  void RecordStartNavigation(NavigationId navigation_id,
                             ukm::SourceId ukm_source_id,
                             const GURL& main_frame_url,
                             base::TimeTicks creation_time) override {}
  void RecordFinishNavigation(NavigationId navigation_id,
                              const GURL& new_main_frame_url,
                              bool is_error_page) override {}
  void RecordResourceLoadComplete(
      NavigationId navigation_id,
      const blink::mojom::ResourceLoadInfo& resource_load_info) override {
    ++count_resource_loads_completed_;
    EXPECT_EQ(expected_request_priority_, resource_load_info.request_priority);
  }

  void RecordMainFrameLoadComplete(NavigationId navigation_id) override {}

  void RecordPageDestroyed(NavigationId navigation_id,
                           const std::optional<OptimizationGuidePrediction>&
                               optimization_guide_prediction) override {}

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
  raw_ptr<TestLoadingDataCollector> test_collector_;
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

class LoadingPredictorTabHelperTestCollectorFencedFramesTest
    : public LoadingPredictorTabHelperTestCollectorTest {
 public:
  LoadingPredictorTabHelperTestCollectorFencedFramesTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kFencedFrames, {{"implementation_type", "mparch"}});
  }
  ~LoadingPredictorTabHelperTestCollectorFencedFramesTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(LoadingPredictorTabHelperTestCollectorFencedFramesTest,
       DoNotRecordResourceLoadComplete) {
  NavigateAndCommitInFrame("http://test.org", main_rfh());
  content::RenderFrameHost* fenced_frame_root =
      content::RenderFrameHostTester::For(main_rfh())->AppendFencedFrame();

  // Navigate a fenced frame.
  GURL fenced_frame_url = GURL("https://fencedframe.com");
  std::unique_ptr<content::NavigationSimulator> navigation_simulator =
      content::NavigationSimulator::CreateRendererInitiated(fenced_frame_url,
                                                            fenced_frame_root);
  navigation_simulator->Commit();
  fenced_frame_root = navigation_simulator->GetFinalRenderFrameHost();

  EXPECT_EQ(0u, test_collector_->count_resource_loads_completed());

  // Load a sub resource on the main frame and record it.
  auto resource_load_info = CreateResourceLoadInfo(
      "http://test.org/script.js", network::mojom::RequestDestination::kScript);
  tab_helper_->ResourceLoadComplete(main_rfh(), content::GlobalRequestID(),
                                    *resource_load_info);
  EXPECT_EQ(1u, test_collector_->count_resource_loads_completed());

  // Load a sub resource on the fenced frame and do not record it.
  tab_helper_->ResourceLoadComplete(
      fenced_frame_root, content::GlobalRequestID(), *resource_load_info);
  EXPECT_EQ(1u, test_collector_->count_resource_loads_completed());
}

}  // namespace predictors
