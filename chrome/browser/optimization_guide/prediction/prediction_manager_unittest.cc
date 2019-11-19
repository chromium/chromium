// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/prediction/prediction_manager.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/optimization_guide/optimization_guide_navigation_data.h"
#include "chrome/browser/optimization_guide/optimization_guide_web_contents_observer.h"
#include "chrome/browser/optimization_guide/prediction/prediction_model.h"
#include "chrome/browser/optimization_guide/prediction/prediction_model_fetcher.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/optimization_guide/optimization_guide_features.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/optimization_guide/top_host_provider.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/web_contents_tester.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"

namespace optimization_guide {

std::unique_ptr<proto::PredictionModel> CreatePredictionModel() {
  std::unique_ptr<optimization_guide::proto::PredictionModel> prediction_model =
      std::make_unique<optimization_guide::proto::PredictionModel>();

  optimization_guide::proto::ModelInfo* model_info =
      prediction_model->mutable_model_info();
  model_info->set_version(1);
  model_info->add_supported_model_features(
      proto::CLIENT_MODEL_FEATURE_EFFECTIVE_CONNECTION_TYPE);
  model_info->set_optimization_target(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
  model_info->add_supported_model_types(
      proto::ModelType::MODEL_TYPE_DECISION_TREE);
  return prediction_model;
}

std::unique_ptr<proto::GetModelsResponse> BuildGetModelsResponse(
    const std::vector<std::string>& hosts,
    const std::vector<proto::ClientModelFeature>& client_model_features) {
  std::unique_ptr<proto::GetModelsResponse> get_models_response =
      std::make_unique<proto::GetModelsResponse>();

  for (const auto& host : hosts) {
    proto::HostModelFeatures* host_model_features =
        get_models_response->add_host_model_features();
    host_model_features->set_host(host);
    proto::ModelFeature* model_feature =
        host_model_features->add_model_features();
    model_feature->set_feature_name("host_feat1");
    model_feature->set_double_value(2.0);
  }

  std::unique_ptr<proto::PredictionModel> prediction_model =
      CreatePredictionModel();
  for (const auto& client_model_feature : client_model_features) {
    prediction_model->mutable_model_info()->add_supported_model_features(
        client_model_feature);
  }
  prediction_model->mutable_model_info()->set_version(2);
  *get_models_response->add_models() = *prediction_model.get();

  return get_models_response;
}

class TestPredictionModel : public PredictionModel {
 public:
  TestPredictionModel(std::unique_ptr<proto::PredictionModel> prediction_model,
                      const base::flat_set<std::string>& host_model_features)
      : PredictionModel(std::move(prediction_model), host_model_features) {}
  ~TestPredictionModel() override = default;

  optimization_guide::OptimizationTargetDecision Predict(
      const base::flat_map<std::string, float>& model_features,
      double* prediction_score) override {
    *prediction_score = 0.0;
    // Check to make sure the all model_features were provided.
    for (const auto& model_feature : GetModelFeatures()) {
      if (!model_features.contains(model_feature))
        return OptimizationTargetDecision::kUnknown;
    }
    *prediction_score = 0.6;
    model_evaluated_ = true;
    return OptimizationTargetDecision::kPageLoadMatches;
  }

  bool WasModelEvaluated() { return model_evaluated_; }

  void ResetModelEvaluationState() { model_evaluated_ = false; }

 private:
  bool ValidatePredictionModel() const override { return true; }

  bool model_evaluated_ = false;
};

// A mock class implementation of TopHostProvider.
class FakeTopHostProvider : public TopHostProvider {
 public:
  explicit FakeTopHostProvider(const std::vector<std::string>& top_hosts)
      : top_hosts_(top_hosts) {}

  std::vector<std::string> GetTopHosts() override {
    num_top_hosts_called_++;
    return top_hosts_;
  }

  int num_top_hosts_called() const { return num_top_hosts_called_; }

 private:
  std::vector<std::string> top_hosts_;
  int num_top_hosts_called_ = 0;
};

enum class PredictionModelFetcherEndState {
  kFetchFailed = 0,
  kFetchSuccessWithModelsAndHostsModelFeatures = 1,
  kFetchSuccessWithEmptyResponse = 2,
};

// A mock class implementation of PredictionModelFetcher.
class TestPredictionModelFetcher : public PredictionModelFetcher {
 public:
  TestPredictionModelFetcher(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      GURL optimization_guide_service_get_models_url,
      PredictionModelFetcherEndState fetch_state)
      : PredictionModelFetcher(url_loader_factory,
                               optimization_guide_service_get_models_url),
        fetch_state_(fetch_state) {}

  bool FetchOptimizationGuideServiceModels(
      const std::vector<optimization_guide::proto::ModelInfo>&
          models_request_info,
      const std::vector<std::string>& hosts,
      optimization_guide::proto::RequestContext request_context,
      ModelsFetchedCallback models_fetched_callback) override {
    if (!ValidateModelsInfoForFetch(models_request_info)) {
      std::move(models_fetched_callback).Run(base::nullopt);
      return false;
    }

    switch (fetch_state_) {
      case PredictionModelFetcherEndState::kFetchFailed:
        std::move(models_fetched_callback).Run(base::nullopt);
        return false;
      case PredictionModelFetcherEndState::
          kFetchSuccessWithModelsAndHostsModelFeatures:
        models_fetched_ = true;
        std::move(models_fetched_callback)
            .Run(BuildGetModelsResponse(hosts, {}));
        return true;
      case PredictionModelFetcherEndState::kFetchSuccessWithEmptyResponse:
        models_fetched_ = true;
        std::move(models_fetched_callback)
            .Run(BuildGetModelsResponse({} /* hosts */,
                                        {} /* client model features */));
        return true;
    }
    return true;
  }

  bool ValidateModelsInfoForFetch(
      const std::vector<proto::ModelInfo>& models_request_info) {
    for (const auto& model_info : models_request_info) {
      if (model_info.supported_model_types_size() == 0 ||
          !proto::ModelType_IsValid(model_info.supported_model_types(0))) {
        return false;
      }
      if (!model_info.has_optimization_target() ||
          !proto::OptimizationTarget_IsValid(
              model_info.optimization_target())) {
        return false;
      }
    }
    return true;
  }

  bool models_fetched() { return models_fetched_; }

 private:
  bool models_fetched_ = false;
  // The desired behavior of the TestPredictionModelFetcher.
  PredictionModelFetcherEndState fetch_state_;
};

class TestPredictionManager : public PredictionManager {
 public:
  TestPredictionManager(
      const std::vector<optimization_guide::proto::OptimizationTarget>&
          optimization_targets_at_initialization,
      TopHostProvider* top_host_provider,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : PredictionManager(optimization_targets_at_initialization,
                          top_host_provider,
                          url_loader_factory) {}
  ~TestPredictionManager() override = default;

  std::unique_ptr<PredictionModel> CreatePredictionModel(
      const proto::PredictionModel& model,
      const base::flat_set<std::string>& host_model_features) const override {
    std::unique_ptr<PredictionModel> prediction_model =
        std::make_unique<TestPredictionModel>(
            std::make_unique<proto::PredictionModel>(model),
            host_model_features);
    return prediction_model;
  }

  using PredictionManager::GetHostModelFeaturesForTesting;
  using PredictionManager::GetPredictionModelForTesting;

  void UpdateHostModelFeaturesForTesting(
      proto::GetModelsResponse* get_models_response) {
    UpdateHostModelFeatures(get_models_response->host_model_features());
  }

  void UpdatePredictionModelsForTesting(
      proto::GetModelsResponse* get_models_response) {
    UpdatePredictionModels(get_models_response->mutable_models(), {});
  }
};

class PredictionManagerTest
    : public ChromeRenderViewHostTestHarness,
      public testing::WithParamInterface<proto::ClientModelFeature> {
 public:
  PredictionManagerTest() = default;
  ~PredictionManagerTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    top_host_provider_ = std::make_unique<FakeTopHostProvider>(
        std::vector<std::string>({"example1.com", "example2.com"}));

    url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    prediction_manager_ = std::make_unique<TestPredictionManager>(
        std::vector<optimization_guide::proto::OptimizationTarget>({}),
        top_host_provider_.get(), url_loader_factory_);
  }

  void CreatePredictionManager(
      const std::vector<optimization_guide::proto::OptimizationTarget>&
          optimization_targets_at_initialization) {
    if (prediction_manager_) {
      prediction_manager_.reset();
    }

    prediction_manager_ = std::make_unique<TestPredictionManager>(
        optimization_targets_at_initialization, top_host_provider_.get(),
        url_loader_factory_);
  }

  TestPredictionManager* prediction_manager() const {
    return prediction_manager_.get();
  }

  bool IsSameOriginNavigationFeature() {
    return GetParam() == proto::CLIENT_MODEL_FEATURE_SAME_ORIGIN_NAVIGATION;
  }

  void TearDown() override { ChromeRenderViewHostTestHarness::TearDown(); }

  FakeTopHostProvider* top_host_provider() const {
    return top_host_provider_.get();
  }

  std::unique_ptr<TestPredictionModelFetcher> BuildTestPredictionModelFetcher(
      PredictionModelFetcherEndState end_state) {
    std::unique_ptr<TestPredictionModelFetcher> prediction_model_fetcher =
        std::make_unique<TestPredictionModelFetcher>(
            url_loader_factory_, GURL("https://hintsserver.com"), end_state);
    return prediction_model_fetcher;
  }

  TestPredictionModelFetcher* prediction_model_fetcher() const {
    return static_cast<TestPredictionModelFetcher*>(
        prediction_manager()->prediction_model_fetcher());
  }

 private:
  std::unique_ptr<TestPredictionManager> prediction_manager_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<FakeTopHostProvider> top_host_provider_;

  DISALLOW_COPY_AND_ASSIGN(PredictionManagerTest);
};

TEST_F(PredictionManagerTest,
       OptimizationTargetProvidedAtInitializationHasModelFetched) {
  std::vector<optimization_guide::proto::OptimizationTarget>
      optimization_targets_at_initialization = {
          optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD};
  CreatePredictionManager(optimization_targets_at_initialization);

  // Given that we cannot inject a fetcher at initialization unless we create a
  // constructor just for testing, we will just simulate this with a call to
  // the top host provider.
  EXPECT_EQ(1, top_host_provider()->num_top_hosts_called());
}

TEST_F(PredictionManagerTest, OptimizationTargetNotRegisteredForNavigation) {
  OptimizationGuideWebContentsObserver::CreateForWebContents(web_contents());
  content::MockNavigationHandle navigation_handle(web_contents());
  navigation_handle.set_url(GURL("https://foo.com"));

  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::
              kFetchSuccessWithModelsAndHostsModelFeatures));

  prediction_manager()->RegisterOptimizationTargets(
      {proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD});
  EXPECT_TRUE(prediction_model_fetcher()->models_fetched());

  EXPECT_EQ(OptimizationTargetDecision::kUnknown,
            prediction_manager()->ShouldTargetNavigation(
                &navigation_handle, proto::OPTIMIZATION_TARGET_UNKNOWN));
  // OptimizationGuideNavData should not be populated.
  OptimizationGuideNavigationData* nav_data =
      OptimizationGuideNavigationData::GetFromNavigationHandle(
          &navigation_handle);
  EXPECT_FALSE(nav_data
                   ->GetModelVersionForOptimizationTarget(
                       optimization_guide::proto::OPTIMIZATION_TARGET_UNKNOWN)
                   .has_value());
  EXPECT_FALSE(nav_data
                   ->GetModelPredictionScoreForOptimizationTarget(
                       optimization_guide::proto::OPTIMIZATION_TARGET_UNKNOWN)
                   .has_value());
}

TEST_F(PredictionManagerTest,
       NoPredictionModelForRegisteredOptimizationTarget) {
  OptimizationGuideWebContentsObserver::CreateForWebContents(web_contents());
  content::MockNavigationHandle navigation_handle(web_contents());
  navigation_handle.set_url(GURL("https://foo.com"));

  prediction_manager()->RegisterOptimizationTargets(
      {proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD});
  EXPECT_EQ(
      OptimizationTargetDecision::kModelNotAvailableOnClient,
      prediction_manager()->ShouldTargetNavigation(
          &navigation_handle, proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));

  // OptimizationGuideNavData should not be populated.
  OptimizationGuideNavigationData* nav_data =
      OptimizationGuideNavigationData::GetFromNavigationHandle(
          &navigation_handle);
  EXPECT_FALSE(
      nav_data
          ->GetModelVersionForOptimizationTarget(
              optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD)
          .has_value());
  EXPECT_FALSE(
      nav_data
          ->GetModelPredictionScoreForOptimizationTarget(
              optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD)
          .has_value());
}

TEST_F(PredictionManagerTest, EvaluatePredictionModel) {
  content::MockNavigationHandle navigation_handle(web_contents());
  navigation_handle.set_url(GURL("https://foo.com"));

  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::
              kFetchSuccessWithModelsAndHostsModelFeatures));

  prediction_manager()->RegisterOptimizationTargets(
      {proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD});

  EXPECT_TRUE(prediction_model_fetcher()->models_fetched());

  EXPECT_EQ(
      OptimizationTargetDecision::kPageLoadMatches,
      prediction_manager()->ShouldTargetNavigation(
          &navigation_handle, proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));

  TestPredictionModel* test_prediction_model =
      static_cast<TestPredictionModel*>(
          prediction_manager()->GetPredictionModelForTesting(
              proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));
  EXPECT_TRUE(test_prediction_model);
  EXPECT_TRUE(test_prediction_model->WasModelEvaluated());
}

TEST_F(PredictionManagerTest, UpdateModelWithSameVersion) {
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchFailed));

  prediction_manager()->RegisterOptimizationTargets(
      {proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD});

  // Seed the PredictionManager with a prediction model with a higher version
  // to try to be updated.
  std::unique_ptr<proto::GetModelsResponse> get_models_response =
      BuildGetModelsResponse({} /* hosts */, {} /* client features */);
  get_models_response->mutable_models(0)->mutable_model_info()->set_version(3);

  prediction_manager()->UpdatePredictionModelsForTesting(
      get_models_response.get());

  get_models_response =
      BuildGetModelsResponse({} /* hosts */, {} /* client features */);

  get_models_response->mutable_models(0)->mutable_model_info()->set_version(3);
  prediction_manager()->UpdatePredictionModelsForTesting(
      get_models_response.get());

  TestPredictionModel* stored_prediction_model =
      static_cast<TestPredictionModel*>(
          prediction_manager()->GetPredictionModelForTesting(
              proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));
  EXPECT_TRUE(stored_prediction_model);
  EXPECT_EQ(3, stored_prediction_model->GetVersion());
}

TEST_F(PredictionManagerTest, EvaluatePredictionModelPopulatesNavData) {
  OptimizationGuideWebContentsObserver::CreateForWebContents(web_contents());
  content::MockNavigationHandle navigation_handle(web_contents());
  navigation_handle.set_url(GURL("https://foo.com"));

  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::
              kFetchSuccessWithModelsAndHostsModelFeatures));

  prediction_manager()->RegisterOptimizationTargets(
      {proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD});

  EXPECT_TRUE(prediction_model_fetcher()->models_fetched());

  EXPECT_EQ(
      OptimizationTargetDecision::kPageLoadMatches,
      prediction_manager()->ShouldTargetNavigation(
          &navigation_handle, proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));

  TestPredictionModel* test_prediction_model =
      static_cast<TestPredictionModel*>(
          prediction_manager()->GetPredictionModelForTesting(
              proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));
  EXPECT_TRUE(test_prediction_model);
  EXPECT_TRUE(test_prediction_model->WasModelEvaluated());

  OptimizationGuideNavigationData* nav_data =
      OptimizationGuideNavigationData::GetFromNavigationHandle(
          &navigation_handle);
  EXPECT_EQ(2, *nav_data->GetModelVersionForOptimizationTarget(
                   proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));
  EXPECT_EQ(0.6, *nav_data->GetModelPredictionScoreForOptimizationTarget(
                     proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));
}

TEST_F(PredictionManagerTest,
       EvaluatePredictionModelPopulatesNavDataEvenWithHoldback) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      {base::test::ScopedFeatureList::FeatureAndParams(
          features::kOptimizationTargetPrediction,
          {{"painful_page_load_metrics_only", "true"}})},
      {});

  OptimizationGuideWebContentsObserver::CreateForWebContents(web_contents());
  content::MockNavigationHandle navigation_handle(web_contents());
  navigation_handle.set_url(GURL("https://foo.com"));

  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::
              kFetchSuccessWithModelsAndHostsModelFeatures));

  prediction_manager()->RegisterOptimizationTargets(
      {proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD});

  EXPECT_TRUE(prediction_model_fetcher()->models_fetched());

  EXPECT_EQ(
      OptimizationTargetDecision::kModelPredictionHoldback,
      prediction_manager()->ShouldTargetNavigation(
          &navigation_handle, proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));

  TestPredictionModel* test_prediction_model =
      static_cast<TestPredictionModel*>(
          prediction_manager()->GetPredictionModelForTesting(
              proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));
  EXPECT_TRUE(test_prediction_model);
  EXPECT_TRUE(test_prediction_model->WasModelEvaluated());

  OptimizationGuideNavigationData* nav_data =
      OptimizationGuideNavigationData::GetFromNavigationHandle(
          &navigation_handle);
  EXPECT_EQ(2, *nav_data->GetModelVersionForOptimizationTarget(
                   proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));
  EXPECT_EQ(0.6, *nav_data->GetModelPredictionScoreForOptimizationTarget(
                     proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));
}

TEST_F(PredictionManagerTest, UpdateModelForUnregisteredTarget) {
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::
              kFetchSuccessWithModelsAndHostsModelFeatures));

  prediction_manager()->RegisterOptimizationTargets({});

  EXPECT_FALSE(prediction_model_fetcher()->models_fetched());

  std::unique_ptr<proto::GetModelsResponse> get_models_response =
      BuildGetModelsResponse({} /* hosts */, {} /* client features */);

  prediction_manager()->UpdatePredictionModelsForTesting(
      get_models_response.get());

  TestPredictionModel* test_prediction_model =
      static_cast<TestPredictionModel*>(
          prediction_manager()->GetPredictionModelForTesting(
              proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));
  EXPECT_FALSE(test_prediction_model);
}

TEST_F(PredictionManagerTest, UpdateModelWithUnsupportedOptimizationTarget) {
  content::MockNavigationHandle navigation_handle(web_contents());
  navigation_handle.set_url(GURL("https://foo.com"));

  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchFailed));

  prediction_manager()->RegisterOptimizationTargets(
      {proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD});

  EXPECT_FALSE(prediction_model_fetcher()->models_fetched());

  std::unique_ptr<proto::GetModelsResponse> get_models_response =
      BuildGetModelsResponse({}, {});
  get_models_response->mutable_models(0)
      ->mutable_model_info()
      ->clear_optimization_target();
  prediction_manager()->UpdatePredictionModelsForTesting(
      get_models_response.get());

  EXPECT_EQ(
      OptimizationTargetDecision::kModelNotAvailableOnClient,
      prediction_manager()->ShouldTargetNavigation(
          &navigation_handle, proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));

  TestPredictionModel* test_prediction_model =
      static_cast<TestPredictionModel*>(
          prediction_manager()->GetPredictionModelForTesting(
              proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));
  EXPECT_FALSE(test_prediction_model);
}

TEST_F(PredictionManagerTest, HasHostModelFeaturesForHost) {
  base::HistogramTester histogram_tester;

  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchFailed));

  prediction_manager()->RegisterOptimizationTargets(
      {proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD});

  std::unique_ptr<proto::GetModelsResponse> get_models_response =
      BuildGetModelsResponse({"example1.com", "example2.com"}, {});
  prediction_manager()->UpdateHostModelFeaturesForTesting(
      get_models_response.get());

  EXPECT_TRUE(prediction_manager()->GetHostModelFeaturesForTesting().contains(
      "example2.com"));
  base::flat_map<std::string, base::flat_map<std::string, float>>
      host_model_features_map =
          prediction_manager()->GetHostModelFeaturesForTesting();
  EXPECT_TRUE(host_model_features_map.contains("example1.com"));
  EXPECT_TRUE(host_model_features_map.contains("example2.com"));
  auto it = host_model_features_map.find("example1.com");
  EXPECT_TRUE(it->second.contains("host_feat1"));
  EXPECT_EQ(2.0, it->second["host_feat1"]);
  it = host_model_features_map.find("example2.com");
  EXPECT_TRUE(it->second.contains("host_feat1"));
  EXPECT_EQ(2.0, it->second["host_feat1"]);
}

TEST_F(PredictionManagerTest, NoHostModelFeaturesForHost) {
  base::HistogramTester histogram_tester;
  content::MockNavigationHandle navigation_handle(web_contents());
  navigation_handle.set_url(GURL("https://foo.com"));

  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchFailed));

  prediction_manager()->RegisterOptimizationTargets(
      {proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD});

  std::unique_ptr<proto::GetModelsResponse> get_models_response =
      BuildGetModelsResponse({"example1.com", "example2.com"}, {});
  prediction_manager()->UpdateHostModelFeaturesForTesting(
      get_models_response.get());
  prediction_manager()->UpdatePredictionModelsForTesting(
      get_models_response.get());

  EXPECT_EQ(
      OptimizationTargetDecision::kPageLoadMatches,
      prediction_manager()->ShouldTargetNavigation(
          &navigation_handle, proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));

  TestPredictionModel* test_prediction_model =
      static_cast<TestPredictionModel*>(
          prediction_manager()->GetPredictionModelForTesting(
              proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));

  EXPECT_TRUE(test_prediction_model);
  EXPECT_TRUE(test_prediction_model->WasModelEvaluated());
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionManager."
      "HasHostModelFeaturesForHost",
      false, 1);

  EXPECT_FALSE(prediction_manager()->GetHostModelFeaturesForTesting().contains(
      "foo.com"));
  EXPECT_EQ(2u, prediction_manager()->GetHostModelFeaturesForTesting().size());
}

TEST_F(PredictionManagerTest, UpdateHostModelFeaturesMissingHost) {
  base::HistogramTester histogram_tester;

  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchFailed));

  prediction_manager()->RegisterOptimizationTargets(
      {proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD});

  std::unique_ptr<proto::GetModelsResponse> get_models_response =
      BuildGetModelsResponse({"example1.com"}, {});
  get_models_response->mutable_host_model_features(0)->clear_host();

  prediction_manager()->UpdateHostModelFeaturesForTesting(
      get_models_response.get());

  EXPECT_FALSE(prediction_manager()->GetHostModelFeaturesForTesting().contains(
      "example1.com"));
  EXPECT_EQ(0u, prediction_manager()->GetHostModelFeaturesForTesting().size());
}

TEST_F(PredictionManagerTest, UpdateHostModelFeaturesNoFeature) {
  base::HistogramTester histogram_tester;

  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchFailed));

  prediction_manager()->RegisterOptimizationTargets(
      {proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD});

  std::unique_ptr<proto::GetModelsResponse> get_models_response =
      BuildGetModelsResponse({"example1.com"}, {});
  get_models_response->mutable_host_model_features(0)->clear_model_features();

  prediction_manager()->UpdateHostModelFeaturesForTesting(
      get_models_response.get());

  EXPECT_FALSE(prediction_manager()->GetHostModelFeaturesForTesting().contains(
      "example1.com"));
  EXPECT_EQ(0u, prediction_manager()->GetHostModelFeaturesForTesting().size());
}

TEST_F(PredictionManagerTest, UpdateHostModelFeaturesNoFeatureName) {
  base::HistogramTester histogram_tester;

  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchFailed));

  prediction_manager()->RegisterOptimizationTargets(
      {proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD});

  std::unique_ptr<proto::GetModelsResponse> get_models_response =
      BuildGetModelsResponse({"example1.com"}, {});
  get_models_response->mutable_host_model_features(0)
      ->mutable_model_features(0)
      ->clear_feature_name();

  prediction_manager()->UpdateHostModelFeaturesForTesting(
      get_models_response.get());

  EXPECT_FALSE(prediction_manager()->GetHostModelFeaturesForTesting().contains(
      "example1.com"));
  EXPECT_EQ(0u, prediction_manager()->GetHostModelFeaturesForTesting().size());
}

TEST_F(PredictionManagerTest, UpdateHostModelFeaturesDoubleValue) {
  base::HistogramTester histogram_tester;

  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchFailed));

  prediction_manager()->RegisterOptimizationTargets(
      {proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD});

  std::unique_ptr<proto::GetModelsResponse> get_models_response =
      BuildGetModelsResponse({"example1.com"}, {});
  get_models_response->mutable_host_model_features(0)
      ->mutable_model_features(0)
      ->set_double_value(3.0);

  prediction_manager()->UpdateHostModelFeaturesForTesting(
      get_models_response.get());

  EXPECT_TRUE(prediction_manager()->GetHostModelFeaturesForTesting().contains(
      "example1.com"));
  EXPECT_EQ(
      3.0,
      prediction_manager()
          ->GetHostModelFeaturesForTesting()["example1.com"]["host_feat1"]);
}

TEST_F(PredictionManagerTest, UpdateHostModelFeaturesIntValue) {
  base::HistogramTester histogram_tester;

  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchFailed));

  prediction_manager()->RegisterOptimizationTargets(
      {proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD});

  std::unique_ptr<proto::GetModelsResponse> get_models_response =
      BuildGetModelsResponse({"example1.com"}, {});
  get_models_response->mutable_host_model_features(0)
      ->mutable_model_features(0)
      ->set_int64_value(4);

  prediction_manager()->UpdateHostModelFeaturesForTesting(
      get_models_response.get());

  EXPECT_TRUE(prediction_manager()->GetHostModelFeaturesForTesting().contains(
      "example1.com"));
  // We expect the value to be stored as a float but is created from an int64
  // value.
  EXPECT_EQ(
      4.0,
      prediction_manager()
          ->GetHostModelFeaturesForTesting()["example1.com"]["host_feat1"]);
}

TEST_F(PredictionManagerTest, UpdateHostModelFeaturesUpdateDataInMap) {
  base::HistogramTester histogram_tester;

  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchFailed));

  prediction_manager()->RegisterOptimizationTargets(
      {proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD});

  std::unique_ptr<proto::GetModelsResponse> get_models_response =
      BuildGetModelsResponse({"example1.com"}, {});
  get_models_response->mutable_host_model_features(0)
      ->mutable_model_features(0)
      ->set_int64_value(4);

  prediction_manager()->UpdateHostModelFeaturesForTesting(
      get_models_response.get());

  EXPECT_TRUE(prediction_manager()->GetHostModelFeaturesForTesting().contains(
      "example1.com"));
  // We expect the value to be stored as a float but is created from an int64
  // value.
  EXPECT_EQ(
      4.0,
      prediction_manager()
          ->GetHostModelFeaturesForTesting()["example1.com"]["host_feat1"]);

  get_models_response = BuildGetModelsResponse({"example1.com"}, {});
  get_models_response->mutable_host_model_features(0)
      ->mutable_model_features(0)
      ->set_int64_value(5);
  proto::ModelFeature* model_feature =
      get_models_response->mutable_host_model_features(0)->add_model_features();
  model_feature->set_feature_name("host_feat_added");
  model_feature->set_double_value(6.0);

  prediction_manager()->UpdateHostModelFeaturesForTesting(
      get_models_response.get());

  EXPECT_TRUE(prediction_manager()->GetHostModelFeaturesForTesting().contains(
      "example1.com"));
  // We expect the value to be stored as a float but is created from an int64
  // value.
  EXPECT_EQ(
      5.0,
      prediction_manager()
          ->GetHostModelFeaturesForTesting()["example1.com"]["host_feat1"]);
  EXPECT_TRUE(prediction_manager()
                  ->GetHostModelFeaturesForTesting()["example1.com"]
                  .contains("host_feat_added"));
  EXPECT_EQ(6.0, prediction_manager()
                     ->GetHostModelFeaturesForTesting()["example1.com"]
                                                       ["host_feat_added"]);
}

TEST_P(PredictionManagerTest, ClientFeature) {
  base::HistogramTester histogram_tester;
  content::MockNavigationHandle navigation_handle(web_contents());
  GURL previous_url = GURL("https://foo.com");
  navigation_handle.set_url(previous_url);
  navigation_handle.set_page_transition(
      ui::PageTransition::PAGE_TRANSITION_RELOAD);
  ON_CALL(navigation_handle, GetPreviousURL())
      .WillByDefault(testing::ReturnRef(previous_url));

  if (IsSameOriginNavigationFeature()) {
    EXPECT_CALL(navigation_handle, GetPreviousURL()).Times(1);
  }

  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchFailed));

  prediction_manager()->RegisterOptimizationTargets(
      {proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD});

  std::unique_ptr<proto::GetModelsResponse> get_models_response =
      BuildGetModelsResponse({}, {GetParam()});
  prediction_manager()->UpdateHostModelFeaturesForTesting(
      get_models_response.get());
  prediction_manager()->UpdatePredictionModelsForTesting(
      get_models_response.get());

  EXPECT_EQ(
      OptimizationTargetDecision::kPageLoadMatches,
      prediction_manager()->ShouldTargetNavigation(
          &navigation_handle, proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));

  TestPredictionModel* test_prediction_model =
      static_cast<TestPredictionModel*>(
          prediction_manager()->GetPredictionModelForTesting(
              proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));

  EXPECT_TRUE(test_prediction_model);
  EXPECT_TRUE(test_prediction_model->WasModelEvaluated());
}

INSTANTIATE_TEST_SUITE_P(ClientFeature,
                         PredictionManagerTest,
                         testing::Range(proto::ClientModelFeature_MIN,
                                        proto::ClientModelFeature_MAX));

}  // namespace optimization_guide
