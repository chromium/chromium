// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/base64.h"
#include "base/callback_helpers.h"
#include "base/run_loop.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/optimization_guide/prediction/prediction_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_store.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/optimization_guide_test_util.h"
#include "components/optimization_guide/core/store_update_data.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/variations/hashing.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/network_connection_change_simulator.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_switches.h"
#endif

namespace {

// Fetch and calculate the total number of samples from all the bins for
// |histogram_name|. Note: from some browertests run (such as chromeos) there
// might be two profiles created, and this will return the total sample count
// across profiles.
int GetTotalHistogramSamples(const base::HistogramTester* histogram_tester,
                             const std::string& histogram_name) {
  std::vector<base::Bucket> buckets =
      histogram_tester->GetAllSamples(histogram_name);
  int total = 0;
  for (const auto& bucket : buckets)
    total += bucket.count;

  return total;
}

// Retries fetching |histogram_name| until it contains at least |count| samples.
void RetryForHistogramUntilCountReached(
    const base::HistogramTester* histogram_tester,
    const std::string& histogram_name,
    int count) {
  while (true) {
    base::ThreadPoolInstance::Get()->FlushForTesting();
    base::RunLoop().RunUntilIdle();

    content::FetchHistogramsFromChildProcesses();
    metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

    int total = GetTotalHistogramSamples(histogram_tester, histogram_name);
    if (total >= count)
      return;
  }
}

std::unique_ptr<optimization_guide::proto::PredictionModel>
GetValidDecisionTreePredictionModel() {
  std::unique_ptr<optimization_guide::proto::PredictionModel> prediction_model =
      optimization_guide::GetMinimalDecisionTreePredictionModel(
          /* threshold= */ 5.0,
          /* weight= */ 2.0);

  optimization_guide::proto::DecisionTree* decision_tree_model =
      prediction_model->mutable_model()->mutable_decision_tree();

  optimization_guide::proto::TreeNode* tree_node =
      decision_tree_model->mutable_nodes(0);
  tree_node->mutable_binary_node()->mutable_left_child_id()->set_value(1);
  tree_node->mutable_binary_node()->mutable_right_child_id()->set_value(2);
  tree_node->mutable_binary_node()
      ->mutable_inequality_left_child_test()
      ->mutable_feature_id()
      ->mutable_id()
      ->set_value("agg1");
  tree_node->mutable_binary_node()
      ->mutable_inequality_left_child_test()
      ->set_type(optimization_guide::proto::InequalityTest::LESS_OR_EQUAL);
  tree_node->mutable_binary_node()
      ->mutable_inequality_left_child_test()
      ->mutable_threshold()
      ->set_float_value(1.0);

  tree_node = decision_tree_model->add_nodes();
  tree_node->mutable_node_id()->set_value(1);
  tree_node->mutable_leaf()->mutable_vector()->add_value()->set_double_value(
      2.);

  tree_node = decision_tree_model->add_nodes();
  tree_node->mutable_node_id()->set_value(2);
  tree_node->mutable_leaf()->mutable_vector()->add_value()->set_double_value(
      4.);

  return prediction_model;
}

std::unique_ptr<optimization_guide::proto::PredictionModel>
GetValidEnsemblePredictionModel() {
  std::unique_ptr<optimization_guide::proto::PredictionModel> prediction_model =
      std::make_unique<optimization_guide::proto::PredictionModel>();
  prediction_model->mutable_model()->mutable_threshold()->set_value(5.0);

  optimization_guide::proto::Model valid_decision_tree_model =
      GetValidDecisionTreePredictionModel()->model();
  optimization_guide::proto::Ensemble* ensemble =
      prediction_model->mutable_model()->mutable_ensemble();
  *ensemble->add_members()->mutable_submodel() = valid_decision_tree_model;
  *ensemble->add_members()->mutable_submodel() = valid_decision_tree_model;
  return prediction_model;
}

std::unique_ptr<optimization_guide::proto::PredictionModel>
CreatePredictionModel() {
  std::unique_ptr<optimization_guide::proto::PredictionModel> prediction_model =
      GetValidEnsemblePredictionModel();

  optimization_guide::proto::ModelInfo* model_info =
      prediction_model->mutable_model_info();
  model_info->set_version(1);
  model_info->add_supported_host_model_features("agg1");
  model_info->set_optimization_target(
      optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
  model_info->add_supported_model_types(
      optimization_guide::proto::ModelType::MODEL_TYPE_DECISION_TREE);
  return prediction_model;
}

std::unique_ptr<optimization_guide::proto::GetModelsResponse>
BuildGetModelsResponse(const std::vector<std::string>& hosts) {
  std::unique_ptr<optimization_guide::proto::GetModelsResponse>
      get_models_response =
          std::make_unique<optimization_guide::proto::GetModelsResponse>();

  for (const auto& host : hosts) {
    optimization_guide::proto::HostModelFeatures* host_model_features =
        get_models_response->add_host_model_features();
    host_model_features->set_host(host);
    optimization_guide::proto::ModelFeature* model_feature =
        host_model_features->add_model_features();
    model_feature->set_feature_name("agg1");
    model_feature->set_double_value(2.0);
  }

  std::unique_ptr<optimization_guide::proto::PredictionModel> prediction_model =
      CreatePredictionModel();
  prediction_model->mutable_model_info()->set_version(2);
  *get_models_response->add_models() = *prediction_model.get();

  return get_models_response;
}

enum class PredictionModelsFetcherRemoteResponseType {
  kSuccessfulWithModelsAndFeatures = 0,
  kSuccessfulWithFeaturesAndNoModels = 1,
  kSuccessfulWithModelsAndNoFeatures = 2,
  kSuccessfulWithValidModelFile = 3,
  kSuccessfulWithInvalidModelFile = 4,
  kUnsuccessful = 5,
};

// A WebContentsObserver that asks whether an optimization target can be
// applied.
class OptimizationGuideConsumerWebContentsObserver
    : public content::WebContentsObserver {
 public:
  explicit OptimizationGuideConsumerWebContentsObserver(
      content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}
  ~OptimizationGuideConsumerWebContentsObserver() override = default;

  // contents::WebContentsObserver implementation:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    OptimizationGuideKeyedService* service =
        OptimizationGuideKeyedServiceFactory::GetForProfile(
            Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
    if (callback_) {
      // Intentionally do not set client model feature values to override to
      // make sure decisions are the same in both sync and async variants.
      service->ShouldTargetNavigationAsync(
          navigation_handle,
          optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
          std::move(callback_));
    }
  }

  void set_callback(
      optimization_guide::OptimizationGuideTargetDecisionCallback callback) {
    callback_ = std::move(callback);
  }

 private:
  optimization_guide::OptimizationGuideTargetDecisionCallback callback_;
};

}  // namespace

namespace optimization_guide {

// Abstract base class for browser testing Prediction Manager.
// Actual class fixtures should implement InitializeFeatureList to set up
// features used in tests.
class PredictionManagerBrowserTestBase : public InProcessBrowserTest {
 public:
  PredictionManagerBrowserTestBase() = default;
  ~PredictionManagerBrowserTestBase() override = default;

  PredictionManagerBrowserTestBase(const PredictionManagerBrowserTestBase&) =
      delete;
  PredictionManagerBrowserTestBase& operator=(
      const PredictionManagerBrowserTestBase&) = delete;

  void SetUp() override {
    InitializeFeatureList();

    models_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    models_server_->ServeFilesFromSourceDirectory(
        "chrome/test/data/optimization_guide");
    models_server_->RegisterRequestHandler(base::BindRepeating(
        &PredictionManagerBrowserTestBase::HandleGetModelsRequest,
        base::Unretained(this)));

    ASSERT_TRUE(models_server_->Start());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    content::NetworkConnectionChangeSimulator().SetConnectionType(
        network::mojom::ConnectionType::CONNECTION_2G);
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_->Start());
    https_url_with_content_ = https_server_->GetURL("/english_page.html");
    https_url_without_content_ = https_server_->GetURL("/empty.html");
    model_file_url_ = models_server_->GetURL("/signed_valid_model.crx3");

    // Set up an OptimizationGuideKeyedService consumer.
    consumer_ = std::make_unique<OptimizationGuideConsumerWebContentsObserver>(
        browser()->tab_strip_model()->GetActiveWebContents());

    InProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(https_server_->ShutdownAndWaitUntilComplete());
    EXPECT_TRUE(models_server_->ShutdownAndWaitUntilComplete());
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* cmd) override {
    cmd->AppendSwitch("enable-spdy-proxy-auth");
    cmd->AppendSwitch(optimization_guide::switches::
                          kFetchModelsAndHostModelFeaturesOverrideTimer);

    cmd->AppendSwitch(optimization_guide::switches::
                          kDisableCheckingUserPermissionsForTesting);
    cmd->AppendSwitchASCII(optimization_guide::switches::kFetchHintsOverride,
                           "whatever.com,somehost.com");
    cmd->AppendSwitchASCII(
        optimization_guide::switches::kOptimizationGuideServiceGetModelsURL,
        models_server_
            ->GetURL(GURL(optimization_guide::
                              kOptimizationGuideServiceGetModelsDefaultURL)
                         .host(),
                     "/")
            .spec());
    cmd->AppendSwitchASCII("host-rules", "MAP * 127.0.0.1");
    cmd->AppendSwitchASCII("force-variation-ids", "4");
  }

  void SetResponseType(
      PredictionModelsFetcherRemoteResponseType response_type) {
    response_type_ = response_type;
  }

  void RegisterWithKeyedService() {
    OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
        ->RegisterOptimizationTargets(
            {optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD});
  }

  // Sets the callback on the consumer of the OptimizationGuideKeyedService. If
  // set, this will call the async version of ShouldTargetNavigation.
  void SetCallbackOnConsumer(
      optimization_guide::OptimizationGuideTargetDecisionCallback callback) {
    ASSERT_TRUE(consumer_);

    consumer_->set_callback(std::move(callback));
  }

  OptimizationGuideConsumerWebContentsObserver* consumer() {
    return consumer_.get();
  }

  PredictionManager* GetPredictionManager() {
    OptimizationGuideKeyedService* optimization_guide_keyed_service =
        OptimizationGuideKeyedServiceFactory::GetForProfile(
            browser()->profile());
    return optimization_guide_keyed_service->GetPredictionManager();
  }

  void SetExpectedFieldTrialNames(
      const base::flat_set<uint32_t>& expected_field_trial_name_hashes) {
    expected_field_trial_name_hashes_ = expected_field_trial_name_hashes;
  }

  void SetExpectedHostsSentInRequest(bool expected_hosts_sent_in_request) {
    expected_hosts_sent_in_request_ = expected_hosts_sent_in_request;
  }

  GURL https_url_with_content() { return https_url_with_content_; }
  GURL https_url_without_content() { return https_url_without_content_; }

 protected:
  // Virtualize for testing different feature configurations.
  virtual void InitializeFeatureList() = 0;

  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleGetModelsRequest(
      const net::test_server::HttpRequest& request) {
    if (request.GetURL() == model_file_url_)
      return nullptr;

    std::unique_ptr<net::test_server::BasicHttpResponse> response;

    response = std::make_unique<net::test_server::BasicHttpResponse>();
    // The request to the remote Optimization Guide Service should always be a
    // POST.
    EXPECT_EQ(request.method, net::test_server::METHOD_POST);
    EXPECT_NE(request.headers.end(), request.headers.find("X-Client-Data"));
    optimization_guide::proto::GetModelsRequest models_request;
    EXPECT_TRUE(models_request.ParseFromString(request.content));
    // Make sure we actually filter field trials appropriately.
    EXPECT_EQ(expected_field_trial_name_hashes_.size(),
              static_cast<size_t>(models_request.active_field_trials_size()));
    base::flat_set<uint32_t> seen_field_trial_name_hashes;
    for (const auto& field_trial : models_request.active_field_trials()) {
      EXPECT_TRUE(
          expected_field_trial_name_hashes_.find(field_trial.name_hash()) !=
          expected_field_trial_name_hashes_.end());
      seen_field_trial_name_hashes.insert(field_trial.name_hash());
    }
    EXPECT_EQ(seen_field_trial_name_hashes.size(),
              expected_field_trial_name_hashes_.size());

    EXPECT_EQ(expected_hosts_sent_in_request_, !models_request.hosts().empty());
    std::vector<std::string> hosts;
    if (expected_hosts_sent_in_request_) {
      hosts = {"example1.com", https_server_->GetURL("/").host()};
    }
    response->set_code(net::HTTP_OK);
    std::unique_ptr<optimization_guide::proto::GetModelsResponse>
        get_models_response = BuildGetModelsResponse(hosts);
    if (response_type_ == PredictionModelsFetcherRemoteResponseType::
                              kSuccessfulWithFeaturesAndNoModels) {
      get_models_response->clear_models();
    } else if (response_type_ == PredictionModelsFetcherRemoteResponseType::
                                     kSuccessfulWithModelsAndNoFeatures) {
      get_models_response->clear_host_model_features();
    } else if (response_type_ == PredictionModelsFetcherRemoteResponseType::
                                     kSuccessfulWithInvalidModelFile) {
      get_models_response->mutable_models(0)->mutable_model()->set_download_url(
          https_url_with_content_.spec());
    } else if (response_type_ == PredictionModelsFetcherRemoteResponseType::
                                     kSuccessfulWithValidModelFile) {
      get_models_response->mutable_models(0)->mutable_model()->set_download_url(
          model_file_url_.spec());
    } else if (response_type_ ==
               PredictionModelsFetcherRemoteResponseType::kUnsuccessful) {
      response->set_code(net::HTTP_NOT_FOUND);
    }

    std::string serialized_response;
    get_models_response->SerializeToString(&serialized_response);
    response->set_content(serialized_response);
    return std::move(response);
  }

  GURL model_file_url_;
  GURL https_url_with_content_, https_url_without_content_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  std::unique_ptr<net::EmbeddedTestServer> models_server_;
  PredictionModelsFetcherRemoteResponseType response_type_ =
      PredictionModelsFetcherRemoteResponseType::
          kSuccessfulWithModelsAndFeatures;
  std::unique_ptr<OptimizationGuideConsumerWebContentsObserver> consumer_;
  base::flat_set<uint32_t> expected_field_trial_name_hashes_;
  bool expected_hosts_sent_in_request_ = true;
};

class PredictionManagerBrowserTest : public PredictionManagerBrowserTestBase {
 public:
  PredictionManagerBrowserTest() = default;
  ~PredictionManagerBrowserTest() override = default;

  PredictionManagerBrowserTest(const PredictionManagerBrowserTest&) = delete;
  PredictionManagerBrowserTest& operator=(const PredictionManagerBrowserTest&) =
      delete;

 private:
  void InitializeFeatureList() override {
    scoped_feature_list_.InitWithFeatures(
        {optimization_guide::features::kOptimizationHints,
         optimization_guide::features::kRemoteOptimizationGuideFetching,
         optimization_guide::features::kOptimizationTargetPrediction},
        {});
  }
};

IN_PROC_BROWSER_TEST_F(PredictionManagerBrowserTest,
                       ModelsAndFeaturesStoreInitialized) {
  base::HistogramTester histogram_tester;
  content::NetworkConnectionChangeSimulator().SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_2G);

  RegisterWithKeyedService();
  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PredictionManager.PredictionModelsStored", 1);
  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PredictionModelLoadedVersion.PainfulPageLoad", 1);

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionManager.HostModelFeaturesStored", true, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionManager.PredictionModelsStored", true, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelUpdateVersion.PainfulPageLoad", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelLoadedVersion.PainfulPageLoad", 2, 1);
}

IN_PROC_BROWSER_TEST_F(PredictionManagerBrowserTest,
                       OnlyHostModelFeaturesInGetModelsResponse) {
  base::HistogramTester histogram_tester;

  SetResponseType(PredictionModelsFetcherRemoteResponseType::
                      kSuccessfulWithFeaturesAndNoModels);
  RegisterWithKeyedService();
  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PredictionManager.HostModelFeaturesStored", 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionManager.HostModelFeaturesStored", true, 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionManager.PredictionModelsStored", 0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelUpdateVersion.PainfulPageLoad", 0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelLoadedVersion.PainfulPageLoad", 0);
}

IN_PROC_BROWSER_TEST_F(PredictionManagerBrowserTest,
                       OnlyPredictionModelsInGetModelsResponse) {
  base::HistogramTester histogram_tester;

  SetResponseType(PredictionModelsFetcherRemoteResponseType::
                      kSuccessfulWithModelsAndNoFeatures);
  RegisterWithKeyedService();
  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PredictionManager.PredictionModelsStored", 1);
  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PredictionModelLoadedVersion.PainfulPageLoad", 1);

  // A metadata entry will always be stored for host model features, regardless
  // of whether any host model features were actually returned.
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionManager.HostModelFeaturesStored", true, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionManager.PredictionModelsStored", true, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelUpdateVersion.PainfulPageLoad", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelLoadedVersion.PainfulPageLoad", 2, 1);
}

IN_PROC_BROWSER_TEST_F(PredictionManagerBrowserTest,
                       PredictionModelFetchFailed) {
  SetResponseType(PredictionModelsFetcherRemoteResponseType::kUnsuccessful);
  base::HistogramTester histogram_tester;

  RegisterWithKeyedService();

  // Wait until histograms have been updated before performing checks for
  // correct behavior based on the response.
  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PredictionModelFetcher.GetModelsResponse.Status", 1);

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.PredictionModelFetcher.GetModelsResponse.Status",
      net::HTTP_NOT_FOUND, 1);

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionManager.HostModelFeaturesStored", 0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionManager.PredictionModelsStored", 0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelUpdateVersion.PainfulPageLoad", 0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelLoadedVersion.PainfulPageLoad", 0);
}

IN_PROC_BROWSER_TEST_F(PredictionManagerBrowserTest,
                       HostModelFeaturesClearedOnHistoryClear) {
  base::HistogramTester histogram_tester;

  RegisterWithKeyedService();

  // Wait until histograms have been updated before performing checks for
  // correct behavior based on the response.
  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PredictionModelFetcher.GetModelsResponse.Status", 1);

  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PredictionManager.HostModelFeaturesStored", 1);

  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PredictionManager.PredictionModelsStored", 1);

  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PredictionModelLoadedVersion.PainfulPageLoad", 1);

  SetCallbackOnConsumer(base::DoNothing());
  ui_test_utils::NavigateToURL(browser(), https_url_with_content());

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionManager.HasHostModelFeaturesForHost", true,
      1);

  // Wipe the browser history - clears all the host model features.
  browser()->profile()->Wipe();
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.ClearHostModelFeatures.StoreAvailable", true, 1);

  SetCallbackOnConsumer(base::DoNothing());
  ui_test_utils::NavigateToURL(browser(), https_url_with_content());
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.PredictionManager.HasHostModelFeaturesForHost", false,
      1);
}

IN_PROC_BROWSER_TEST_F(PredictionManagerBrowserTest, IncognitoCanStillRead) {
  SetResponseType(PredictionModelsFetcherRemoteResponseType::
                      kSuccessfulWithModelsAndFeatures);
  base::HistogramTester histogram_tester;

  // Register with regular profile.
  RegisterWithKeyedService();

  // Wait until model has been fetched via regular profile.
  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PredictionManager.HostModelFeaturesStored", 1);

  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PredictionManager.PredictionModelsStored", 1);

  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PredictionModelLoadedVersion.PainfulPageLoad", 1);

  // Set up incognito browser.
  Browser* otr_browser = CreateIncognitoBrowser(browser()->profile());

  // Register with off the record profile and wait until model is loaded.
  {
    base::HistogramTester otr_histogram_tester;

    OptimizationGuideKeyedServiceFactory::GetForProfile(
        browser()->profile()->GetPrimaryOTRProfile())
        ->RegisterOptimizationTargets(
            {proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD});
    RetryForHistogramUntilCountReached(
        &otr_histogram_tester,
        "OptimizationGuide.PredictionModelLoadedVersion.PainfulPageLoad", 1);
  }

  // Set up an OptimizationGuideKeyedService consumer.
  auto otr_consumer =
      std::make_unique<OptimizationGuideConsumerWebContentsObserver>(
          otr_browser->tab_strip_model()->GetActiveWebContents());
  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  otr_consumer->set_callback(base::BindOnce(
      [](base::RunLoop* run_loop,
         optimization_guide::OptimizationGuideDecision decision) {
        // We should have the model on the client so we have everything to make
        // a decision.
        EXPECT_NE(decision,
                  optimization_guide::OptimizationGuideDecision::kUnknown);
        run_loop->Quit();
      },
      run_loop.get()));

  // Navigate to a URL with a host model feature in incognito.
  ui_test_utils::NavigateToURL(otr_browser, https_url_with_content());
  run_loop->Run();

  // The store should still be able to be read.
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionManager.HasHostModelFeaturesForHost", true,
      1);
}

IN_PROC_BROWSER_TEST_F(PredictionManagerBrowserTest,
                       IncognitoDoesntFetchModels) {
  SetResponseType(PredictionModelsFetcherRemoteResponseType::
                      kSuccessfulWithModelsAndFeatures);
  base::HistogramTester histogram_tester;

  // Set up incognito browser.
  Browser* otr_browser = CreateIncognitoBrowser(browser()->profile());

  // Register with off the record profile.
  OptimizationGuideKeyedServiceFactory::GetForProfile(
      browser()->profile()->GetPrimaryOTRProfile())
      ->RegisterOptimizationTargets(
          {proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD});
  // Wait until logic finishes running.
  base::RunLoop().RunUntilIdle();

  // Ensure that GetModelsRequest did not go out.
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelFetcher.GetModelsRequest.HostCount", 0);

  // Set up an OptimizationGuideKeyedService consumer.
  auto otr_consumer =
      std::make_unique<OptimizationGuideConsumerWebContentsObserver>(
          otr_browser->tab_strip_model()->GetActiveWebContents());
  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  otr_consumer->set_callback(base::BindOnce(
      [](base::RunLoop* run_loop,
         optimization_guide::OptimizationGuideDecision decision) {
        run_loop->Quit();
      },
      run_loop.get()));

  // Navigate to a URL that would normally have a model had we not been in
  // incognito.
  ui_test_utils::NavigateToURL(otr_browser, https_url_with_content());
  run_loop->Run();

  // The model should not be available on the client.
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.TargetDecision.PainfulPageLoad",
      OptimizationTargetDecision::kModelNotAvailableOnClient, 1);
}

class PredictionManagerNoUserPermissionsTest
    : public PredictionManagerBrowserTest {
 public:
  PredictionManagerNoUserPermissionsTest() {
    // Hosts and field trials should not be sent.
    SetExpectedHostsSentInRequest(false);
    SetExpectedFieldTrialNames({});
  }

  ~PredictionManagerNoUserPermissionsTest() override = default;

  void SetUpCommandLine(base::CommandLine* cmd) override {
    PredictionManagerBrowserTest::SetUpCommandLine(cmd);

    // Remove switches that enable user permissions.
    cmd->RemoveSwitch("enable-spdy-proxy-auth");
    cmd->RemoveSwitch(switches::kDisableCheckingUserPermissionsForTesting);
  }

 private:
  void InitializeFeatureList() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {
            {features::kOptimizationHints, {}},
            {features::kRemoteOptimizationGuideFetching, {}},
            {features::kOptimizationTargetPrediction, {}},
            {features::kOptimizationHintsFieldTrials,
             {{"allowed_field_trial_names",
               "scoped_feature_list_trial_for_OptimizationHints,scoped_feature_"
               "list_trial_for_OptimizationHintsFetching"}}},
        },
        {});
  }
};

IN_PROC_BROWSER_TEST_F(PredictionManagerNoUserPermissionsTest,
                       HostsAndFieldTrialsNotPassedWhenNoUserPermissions) {
  base::HistogramTester histogram_tester;

  SetResponseType(PredictionModelsFetcherRemoteResponseType::
                      kSuccessfulWithModelsAndFeatures);
  RegisterWithKeyedService();

  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PredictionManager.HostModelFeaturesStored", 1);

  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PredictionManager.PredictionModelsStored", 1);

  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PredictionModelLoadedVersion.PainfulPageLoad", 1);

  SetCallbackOnConsumer(base::DoNothing());
  ui_test_utils::NavigateToURL(browser(), https_url_with_content());

  // Expect that we did not fetch for host and that we did not get any host
  // model features.
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.PredictionManager.HasHostModelFeaturesForHost", false,
      1);
}

class ModelFileObserver : public OptimizationTargetModelObserver {
 public:
  using ModelFileReceivedCallback =
      base::OnceCallback<void(proto::OptimizationTarget,
                              const base::FilePath&)>;

  ModelFileObserver() = default;
  ~ModelFileObserver() override = default;

  void set_model_file_received_callback(ModelFileReceivedCallback callback) {
    file_received_callback_ = std::move(callback);
  }

  void OnModelFileUpdated(proto::OptimizationTarget optimization_target,
                          const base::Optional<proto::Any>& model_metadata,
                          const base::FilePath& file_path) override {
    if (file_received_callback_)
      std::move(file_received_callback_).Run(optimization_target, file_path);
  }

 private:
  ModelFileReceivedCallback file_received_callback_;
};

class PredictionManagerModelDownloadingBrowserTest
    : public PredictionManagerBrowserTest {
 public:
  PredictionManagerModelDownloadingBrowserTest() = default;
  ~PredictionManagerModelDownloadingBrowserTest() override = default;

  void SetUpOnMainThread() override {
    model_file_observer_ = std::make_unique<ModelFileObserver>();

    PredictionManagerBrowserTest::SetUpOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PredictionManagerBrowserTest::SetUpCommandLine(command_line);
#if BUILDFLAG(IS_CHROMEOS_ASH)
    command_line->AppendSwitch(
        chromeos::switches::kIgnoreUserProfileMappingForTests);
#endif
  }

  void TearDownOnMainThread() override {
    PredictionManagerBrowserTest::TearDownOnMainThread();
  }

  ModelFileObserver* model_file_observer() {
    return model_file_observer_.get();
  }

  void RegisterModelFileObserverWithKeyedService() {
    OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
        ->AddObserverForOptimizationTargetModel(
            proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
            /*model_metadata=*/base::nullopt, model_file_observer_.get());
  }

 private:
  void InitializeFeatureList() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {
            {features::kOptimizationHints, {}},
            {features::kRemoteOptimizationGuideFetching, {}},
            {features::kOptimizationTargetPrediction, {}},
            {features::kOptimizationGuideModelDownloading,
             {{"unrestricted_model_downloading", "true"}}},
            {features::kOptimizationHintsFieldTrials,
             {{"allowed_field_trial_names",
               "scoped_feature_list_trial_for_OptimizationHints,scoped_feature_"
               "list_trial_for_OptimizationHintsFetching"}}},
        },
        {});
    SetExpectedFieldTrialNames(base::flat_set<uint32_t>(
        {variations::HashName(
             "scoped_feature_list_trial_for_OptimizationHints"),
         variations::HashName(
             "scoped_feature_list_trial_for_OptimizationHintsFetching")}));
  }

  std::unique_ptr<ModelFileObserver> model_file_observer_;
};

IN_PROC_BROWSER_TEST_F(PredictionManagerModelDownloadingBrowserTest,
                       TestDownloadUrlAcceptedByDownloadServiceButInvalid) {
  base::HistogramTester histogram_tester;

  SetResponseType(PredictionModelsFetcherRemoteResponseType::
                      kSuccessfulWithInvalidModelFile);

  // Registering should initiate the fetch and receive a response with a model
  // containing a download URL and then subsequently downloaded.
  RegisterModelFileObserverWithKeyedService();

  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PredictionModelDownloadManager.DownloadStatus", 1);

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelDownloadManager.DownloadStatus",
      PredictionModelDownloadStatus::kFailedCrxVerification, 1);
  // An unverified file should not notify us that it's ready.
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelUpdateVersion.PainfulPageLoad", 0);
}

IN_PROC_BROWSER_TEST_F(PredictionManagerModelDownloadingBrowserTest,
                       TestSuccessfulModelFileFlow) {
  base::HistogramTester histogram_tester;

  SetResponseType(
      PredictionModelsFetcherRemoteResponseType::kSuccessfulWithValidModelFile);

  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  model_file_observer()->set_model_file_received_callback(base::BindOnce(
      [](base::RunLoop* run_loop, proto::OptimizationTarget optimization_target,
         const base::FilePath& file_path) {
        EXPECT_EQ(optimization_target,
                  proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
        run_loop->Quit();
      },
      run_loop.get()));

  // Registering should initiate the fetch and receive a response with a model
  // containing a download URL and then subsequently downloaded.
  RegisterModelFileObserverWithKeyedService();

  // Wait until the observer receives the file. We increase the timeout to 60
  // seconds here since the file is on the larger side.
  {
    base::test::ScopedRunLoopTimeout file_download_timeout(
        FROM_HERE, base::TimeDelta::FromSeconds(60));
    run_loop->Run();
  }

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelDownloadManager.DownloadStatus",
      PredictionModelDownloadStatus::kSuccess, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelUpdateVersion.PainfulPageLoad", 123, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelLoadedVersion.PainfulPageLoad", 123, 1);
}

IN_PROC_BROWSER_TEST_F(PredictionManagerModelDownloadingBrowserTest,
                       TestSwitchProfileDoesntCrash) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath other_path =
      profile_manager->GenerateNextProfileDirectoryPath();

  base::RunLoop run_loop;

  // Create an additional profile.
  profile_manager->CreateProfileAsync(
      other_path,
      base::BindLambdaForTesting(
          [&run_loop](Profile* profile, Profile::CreateStatus status) {
            if (status == Profile::CREATE_STATUS_INITIALIZED)
              run_loop.Quit();
          }));

  run_loop.Run();

  Profile* profile = profile_manager->GetProfileByPath(other_path);
  ASSERT_TRUE(profile);
  CreateBrowser(profile);
}

}  // namespace optimization_guide
