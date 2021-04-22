// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/prediction/prediction_manager.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/optimization_guide/optimization_guide_web_contents_observer.h"
#include "chrome/browser/optimization_guide/prediction/prediction_model_download_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/optimization_guide/core/optimization_guide_store.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/core/prediction_model.h"
#include "components/optimization_guide/core/prediction_model_fetcher.h"
#include "components/optimization_guide/core/proto_database_provider_test_base.h"
#include "components/optimization_guide/core/top_host_provider.h"
#include "components/optimization_guide/proto/hint_cache.pb.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/public/test/web_contents_tester.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"

using leveldb_proto::test::FakeDB;

namespace {
// Retry delay is 2 minutes to allow for fetch retry delay + some random delay
// to pass.
constexpr int kTestFetchRetryDelaySecs = 60 * 2 + 62;
// 24 hours + random fetch delay.
constexpr int kUpdateFetchModelAndFeaturesTimeSecs = 24 * 60 * 60 + 62;

}  // namespace

namespace optimization_guide {

proto::PredictionModel CreatePredictionModel(
    bool output_model_as_download_url = false) {
  proto::PredictionModel prediction_model;

  proto::ModelInfo* model_info = prediction_model.mutable_model_info();
  model_info->set_version(1);
  model_info->add_supported_host_model_features("host_feat1");
  model_info->set_optimization_target(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
  model_info->add_supported_model_types(
      proto::ModelType::MODEL_TYPE_DECISION_TREE);
  if (output_model_as_download_url) {
    prediction_model.mutable_model()->set_download_url(
        "https://example.com/model");
  } else {
    prediction_model.mutable_model()->mutable_threshold()->set_value(5.0);
  }
  return prediction_model;
}

std::unique_ptr<proto::GetModelsResponse> BuildGetModelsResponse(
    const std::vector<std::string>& hosts,
    bool output_model_as_download_url = false) {
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

  proto::PredictionModel prediction_model =
      CreatePredictionModel(output_model_as_download_url);
  prediction_model.mutable_model_info()->add_supported_host_model_features(
      "host_feat1");
  prediction_model.mutable_model_info()->set_version(2);
  *get_models_response->add_models() = std::move(prediction_model);

  return get_models_response;
}

class TestPredictionModel : public PredictionModel {
 public:
  explicit TestPredictionModel(const proto::PredictionModel& prediction_model)
      : PredictionModel(prediction_model) {}
  ~TestPredictionModel() override = default;

  OptimizationTargetDecision Predict(
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
    last_evaluated_features_ =
        base::flat_map<std::string, float>(model_features);
    return OptimizationTargetDecision::kPageLoadMatches;
  }

  bool WasModelEvaluated() { return model_evaluated_; }

  void ResetModelEvaluationState() { model_evaluated_ = false; }

  base::flat_map<std::string, float> last_evaluated_features() {
    return last_evaluated_features_;
  }

 private:
  bool ValidatePredictionModel() const override { return true; }

  bool model_evaluated_ = false;
  base::flat_map<std::string, float> last_evaluated_features_;
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

class FakeOptimizationTargetModelObserver
    : public OptimizationTargetModelObserver {
 public:
  void OnModelFileUpdated(proto::OptimizationTarget optimization_target,
                          const base::Optional<proto::Any>& model_metadata,
                          const base::FilePath& file_path) override {
    last_received_models_[optimization_target] =
        std::make_pair(model_metadata, file_path);
  }

  base::Optional<std::pair<base::Optional<proto::Any>, base::FilePath>>
  last_received_model_for_target(
      proto::OptimizationTarget optimization_target) {
    auto model_it = last_received_models_.find(optimization_target);
    if (model_it == last_received_models_.end())
      return base::nullopt;
    return model_it->second;
  }

  // Resets the state of the observer.
  void Reset() { last_received_models_.clear(); }

 private:
  base::flat_map<proto::OptimizationTarget,
                 std::pair<base::Optional<proto::Any>, base::FilePath>>
      last_received_models_;
};

class FakePredictionModelDownloadManager
    : public PredictionModelDownloadManager {
 public:
  FakePredictionModelDownloadManager(
      scoped_refptr<base::SequencedTaskRunner> task_runner)
      : PredictionModelDownloadManager(/*download_service=*/nullptr,
                                       base::FilePath(),
                                       task_runner) {}
  ~FakePredictionModelDownloadManager() override = default;

  void StartDownload(const GURL& url) override {
    last_requested_download_ = url;
  }

  GURL last_requested_download() const { return last_requested_download_; }

  void CancelAllPendingDownloads() override { cancel_downloads_called_ = true; }
  bool cancel_downloads_called() const { return cancel_downloads_called_; }

  bool IsAvailableForDownloads() const override { return is_available_; }
  void SetAvailableForDownloads(bool is_available) {
    is_available_ = is_available;
  }

 private:
  GURL last_requested_download_;
  bool cancel_downloads_called_ = false;
  bool is_available_ = true;
};

enum class PredictionModelFetcherEndState {
  kFetchFailed = 0,
  kFetchSuccessWithModelsAndHostsModelFeatures = 1,
  kFetchSuccessWithEmptyResponse = 2,
  kFetchSuccessWithModelDownloadUrls = 3,
};

void RunGetModelsCallback(
    ModelsFetchedCallback callback,
    std::unique_ptr<proto::GetModelsResponse> get_models_response) {
  if (get_models_response) {
    std::move(callback).Run(std::move(get_models_response));
    return;
  }
  std::move(callback).Run(base::nullopt);
}

// A mock class implementation of PredictionModelFetcher.
class TestPredictionModelFetcher : public PredictionModelFetcher {
 public:
  TestPredictionModelFetcher(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& optimization_guide_service_get_models_url,
      network::NetworkConnectionTracker* network_connection_tracker,
      PredictionModelFetcherEndState fetch_state)
      : PredictionModelFetcher(url_loader_factory,
                               optimization_guide_service_get_models_url,
                               network_connection_tracker),
        fetch_state_(fetch_state) {}

  bool FetchOptimizationGuideServiceModels(
      const std::vector<proto::ModelInfo>& models_request_info,
      const std::vector<std::string>& hosts,
      const std::vector<proto::FieldTrial>& active_field_trials,
      proto::RequestContext request_context,
      const std::string& locale,
      ModelsFetchedCallback models_fetched_callback) override {
    if (!ValidateModelsInfoForFetch(models_request_info)) {
      std::move(models_fetched_callback).Run(base::nullopt);
      return false;
    }

    std::unique_ptr<proto::GetModelsResponse> get_models_response;
    count_hosts_fetched_ = hosts.size();
    locale_requested_ = locale;
    switch (fetch_state_) {
      case PredictionModelFetcherEndState::kFetchFailed:
        get_models_response = nullptr;
        break;
      case PredictionModelFetcherEndState::
          kFetchSuccessWithModelsAndHostsModelFeatures:
        models_fetched_ = true;
        get_models_response = BuildGetModelsResponse(hosts);
        break;
      case PredictionModelFetcherEndState::kFetchSuccessWithEmptyResponse:
        models_fetched_ = true;
        get_models_response = BuildGetModelsResponse(/*hosts=*/{});
        break;
      case PredictionModelFetcherEndState::kFetchSuccessWithModelDownloadUrls:
        models_fetched_ = true;
        get_models_response =
            BuildGetModelsResponse(hosts,
                                   /*output_model_as_download_url=*/true);
        break;
    }
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&RunGetModelsCallback,
                                  std::move(models_fetched_callback),
                                  std::move(get_models_response)));
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

      if (check_expected_version_) {
        auto version_it =
            expected_version_.find(model_info.optimization_target());
        if (model_info.has_version() !=
            (version_it != expected_version_.end())) {
          return false;
        }
        if (model_info.has_version() &&
            model_info.version() != version_it->second) {
          return false;
        }
      }

      auto it = expected_metadata_.find(model_info.optimization_target());
      if (model_info.has_model_metadata() != (it != expected_metadata_.end()))
        return false;
      if (model_info.has_model_metadata()) {
        proto::Any expected_metadata = it->second;
        if (model_info.model_metadata().type_url() !=
            expected_metadata.type_url()) {
          return false;
        }
        if (model_info.model_metadata().value() != expected_metadata.value())
          return false;
      }
    }
    return true;
  }

  void SetExpectedModelMetadataForOptimizationTarget(
      proto::OptimizationTarget optimization_target,
      const proto::Any& model_metadata) {
    expected_metadata_[optimization_target] = model_metadata;
  }

  void SetExpectedVersionForOptimizationTarget(
      proto::OptimizationTarget optimization_target,
      int64_t version) {
    expected_version_[optimization_target] = version;
  }

  void SetCheckExpectedVersion() { check_expected_version_ = true; }

  void Reset() {
    models_fetched_ = false;
    count_hosts_fetched_ = false;
  }

  bool models_fetched() const { return models_fetched_; }
  size_t hosts_fetched() const { return count_hosts_fetched_; }

  std::string locale_requested() const { return locale_requested_; }

 private:
  bool models_fetched_ = false;
  size_t count_hosts_fetched_ = 0;
  bool check_expected_version_ = false;
  std::string locale_requested_;
  // The desired behavior of the TestPredictionModelFetcher.
  PredictionModelFetcherEndState fetch_state_;
  base::flat_map<proto::OptimizationTarget, proto::Any> expected_metadata_;
  base::flat_map<proto::OptimizationTarget, int64_t> expected_version_;
};

class TestOptimizationGuideStore : public OptimizationGuideStore {
 public:
  TestOptimizationGuideStore(
      std::unique_ptr<StoreEntryProtoDatabase> database,
      scoped_refptr<base::SequencedTaskRunner> store_task_runner)
      : OptimizationGuideStore(std::move(database), store_task_runner) {}

  ~TestOptimizationGuideStore() override = default;

  void Initialize(bool purge_existing_data,
                  base::OnceClosure callback) override {
    init_callback_ = std::move(callback);
    status_ = Status::kAvailable;
  }

  void RunInitCallback(bool load_models = true,
                       bool load_host_model_features = true,
                       bool have_models_in_store = true) {
    load_models_ = load_models;
    load_host_model_features_ = load_host_model_features;
    have_models_in_store_ = have_models_in_store;
    std::move(init_callback_).Run();
  }

  void RunUpdateHostModelFeaturesCallback() {
    std::move(update_host_models_callback_).Run();
  }

  void LoadPredictionModel(const EntryKey& prediction_model_entry_key,
                           PredictionModelLoadedCallback callback) override {
    model_loaded_ = true;
    if (load_models_) {
      std::move(callback).Run(
          std::make_unique<proto::PredictionModel>(CreatePredictionModel()));
    } else {
      std::move(callback).Run(nullptr);
    }
  }

  void LoadAllHostModelFeatures(
      AllHostModelFeaturesLoadedCallback callback) override {
    host_model_features_loaded_ = true;
    if (load_host_model_features_) {
      proto::HostModelFeatures host_model_features;
      host_model_features.set_host("foo.com");
      proto::ModelFeature* model_feature =
          host_model_features.add_model_features();
      model_feature->set_feature_name("host_feat1");
      model_feature->set_double_value(2.0);
      std::unique_ptr<std::vector<proto::HostModelFeatures>>
          all_host_model_features =
              std::make_unique<std::vector<proto::HostModelFeatures>>();
      all_host_model_features->emplace_back(host_model_features);
      std::move(callback).Run(std::move(all_host_model_features));
    } else {
      std::move(callback).Run(nullptr);
    }
  }

  bool FindPredictionModelEntryKey(
      proto::OptimizationTarget optimization_target,
      OptimizationGuideStore::EntryKey* out_prediction_model_entry_key)
      override {
    if (optimization_target ==
        proto::OptimizationTarget::OPTIMIZATION_TARGET_UNKNOWN) {
      return false;
    }
    if (have_models_in_store_) {
      *out_prediction_model_entry_key =
          "4_" + base::NumberToString(static_cast<int>(optimization_target));
      return true;
    }
    return false;
  }

  void UpdateHostModelFeatures(
      std::unique_ptr<StoreUpdateData> host_model_features_update_data,
      base::OnceClosure callback) override {
    host_model_features_update_time_ =
        *host_model_features_update_data->update_time();
    update_host_models_callback_ = std::move(callback);
  }

  void UpdatePredictionModels(
      std::unique_ptr<StoreUpdateData> prediction_models_update_data,
      base::OnceClosure callback) override {
    std::move(callback).Run();
  }

  bool WasModelLoaded() const { return model_loaded_; }
  bool WasHostModelFeaturesLoaded() const {
    return host_model_features_loaded_;
  }

 private:
  base::OnceClosure init_callback_;
  base::OnceClosure update_host_models_callback_;
  bool model_loaded_ = false;
  bool host_model_features_loaded_ = false;
  bool load_models_ = true;
  bool load_host_model_features_ = true;
  bool have_models_in_store_ = true;
};

class TestPredictionManager : public PredictionManager {
 public:
  TestPredictionManager(
      OptimizationGuideStore* model_and_features_store,
      TopHostProvider* top_host_provider,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      PrefService* pref_service,
      Profile* profile)
      : PredictionManager(model_and_features_store,
                          top_host_provider,
                          url_loader_factory,
                          pref_service,
                          profile) {}

  ~TestPredictionManager() override = default;

  std::unique_ptr<PredictionModel> CreatePredictionModel(
      const proto::PredictionModel& model) const override {
    if (!create_valid_prediction_model_)
      return nullptr;
    return std::make_unique<TestPredictionModel>(model);
  }

  void set_create_valid_prediction_model(bool create_valid_prediction_model) {
    create_valid_prediction_model_ = create_valid_prediction_model;
  }

  using PredictionManager::GetHostModelFeaturesForHost;
  using PredictionManager::GetHostModelFeaturesForTesting;
  using PredictionManager::GetPredictionModelForTesting;

  void UpdateHostModelFeaturesForTesting(
      proto::GetModelsResponse* get_models_response) {
    UpdateHostModelFeatures(get_models_response->host_model_features());
  }

  void UpdatePredictionModelsForTesting(
      proto::GetModelsResponse* get_models_response) {
    UpdatePredictionModels(get_models_response->models());
  }

 private:
  bool create_valid_prediction_model_ = true;
};

class PredictionManagerTestBase : public ProtoDatabaseProviderTestBase {
 public:
  using StoreEntry = proto::StoreEntry;
  using StoreEntryMap = std::map<OptimizationGuideStore::EntryKey, StoreEntry>;
  PredictionManagerTestBase() = default;
  ~PredictionManagerTestBase() override = default;

  PredictionManagerTestBase(const PredictionManagerTestBase&) = delete;
  PredictionManagerTestBase& operator=(const PredictionManagerTestBase&) =
      delete;

  void SetUp() override {
    ProtoDatabaseProviderTestBase::SetUp();
    web_contents_factory_ = std::make_unique<content::TestWebContentsFactory>();

    top_host_provider_ = std::make_unique<FakeTopHostProvider>(
        std::vector<std::string>({"example1.com", "example2.com"}));

    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    prefs::RegisterProfilePrefs(pref_service_->registry());

    url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kDisableCheckingUserPermissionsForTesting);
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kFetchModelsAndHostModelFeaturesOverrideTimer);
  }

  void CreatePredictionManager() {
    if (prediction_manager_) {
      db_store_.clear();
      model_and_features_store_.reset();
      prediction_manager_.reset();
    }

    model_and_features_store_ = CreateModelAndHostModelFeaturesStore();
    prediction_manager_ = std::make_unique<TestPredictionManager>(
        model_and_features_store_.get(), top_host_provider_.get(),
        url_loader_factory_, pref_service_.get(), &testing_profile_);
    prediction_manager_->SetClockForTesting(task_environment_.GetMockClock());
  }

  void CreatePredictionManagerWithoutTopHostProvider() {
    if (prediction_manager_) {
      db_store_.clear();
      model_and_features_store_.reset();
      prediction_manager_.reset();
    }

    model_and_features_store_ = CreateModelAndHostModelFeaturesStore();
    prediction_manager_ = std::make_unique<TestPredictionManager>(
        model_and_features_store_.get(), nullptr, url_loader_factory_,
        pref_service_.get(), &testing_profile_);
    prediction_manager_->SetClockForTesting(task_environment_.GetMockClock());
  }

  std::unique_ptr<TestOptimizationGuideStore>
  CreateModelAndHostModelFeaturesStore() {
    // Setup the fake db and the class under test.
    auto db = std::make_unique<FakeDB<StoreEntry>>(&db_store_);

    return std::make_unique<TestOptimizationGuideStore>(
        std::move(db), task_environment_.GetMainThreadTaskRunner());
  }

  TestPredictionManager* prediction_manager() const {
    return prediction_manager_.get();
  }

  // Creates a navigation handle with the OptimizationGuideWebContentsObserver
  // attached.
  std::unique_ptr<content::MockNavigationHandle>
  CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
      const GURL& url) {
    content::WebContents* web_contents =
        web_contents_factory_->CreateWebContents(&testing_profile_);
    OptimizationGuideWebContentsObserver::CreateForWebContents(web_contents);
    std::unique_ptr<content::MockNavigationHandle> navigation_handle =
        std::make_unique<content::MockNavigationHandle>(web_contents);
    navigation_handle->set_url(url);
    return navigation_handle;
  }

  void TearDown() override { ProtoDatabaseProviderTestBase::TearDown(); }

  FakeTopHostProvider* top_host_provider() const {
    return top_host_provider_.get();
  }

  std::unique_ptr<TestPredictionModelFetcher> BuildTestPredictionModelFetcher(
      PredictionModelFetcherEndState end_state) {
    std::unique_ptr<TestPredictionModelFetcher> prediction_model_fetcher =
        std::make_unique<TestPredictionModelFetcher>(
            url_loader_factory_, GURL("https://hintsserver.com"),
            network::TestNetworkConnectionTracker::GetInstance(), end_state);
    return prediction_model_fetcher;
  }

  void SetStoreInitialized(bool load_models = true,
                           bool load_host_model_features = true,
                           bool have_models_in_store = true) {
    models_and_features_store()->RunInitCallback(
        load_models, load_host_model_features, have_models_in_store);
    RunUntilIdle();
    // Move clock forward for any short delays added for the fetcher.
    MoveClockForwardBy(base::TimeDelta::FromSeconds(2));
  }

  void MoveClockForwardBy(base::TimeDelta time_delta) {
    task_environment_.FastForwardBy(time_delta);
    RunUntilIdle();
  }

  TestPredictionModelFetcher* prediction_model_fetcher() const {
    return static_cast<TestPredictionModelFetcher*>(
        prediction_manager()->prediction_model_fetcher());
  }

  FakePredictionModelDownloadManager* prediction_model_download_manager()
      const {
    return static_cast<FakePredictionModelDownloadManager*>(
        prediction_manager()->prediction_model_download_manager());
  }

  TestOptimizationGuideStore* models_and_features_store() const {
    return static_cast<TestOptimizationGuideStore*>(
        prediction_manager()->model_and_features_store());
  }

  base::FilePath temp_dir() const { return temp_dir_.GetPath(); }

  TestingPrefServiceSimple* pref_service() const { return pref_service_.get(); }

  TestingProfile* profile() { return &testing_profile_; }

  void RunUntilIdle() {
    task_environment_.RunUntilIdle();
    base::RunLoop().RunUntilIdle();
  }

  content::BrowserTaskEnvironment* task_environment() {
    return &task_environment_;
  }

 protected:
  // |feature_list_| needs to be destroyed after |task_environment_|, to avoid
  // tsan flakes caused by other tasks running while |feature_list_| is
  // destroyed.
  base::test::ScopedFeatureList feature_list_;

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  StoreEntryMap db_store_;
  std::unique_ptr<TestOptimizationGuideStore> model_and_features_store_;
  std::unique_ptr<TestPredictionManager> prediction_manager_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<FakeTopHostProvider> top_host_provider_;
  TestingProfile testing_profile_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  std::unique_ptr<content::TestWebContentsFactory> web_contents_factory_;
};

class PredictionManagerRemoteFetchingDisabledTest
    : public PredictionManagerTestBase {
 public:
  PredictionManagerRemoteFetchingDisabledTest() {
    // This needs to be done before any tasks are run that might check if a
    // feature is enabled, to avoid tsan errors.
    feature_list_.InitAndDisableFeature(
        features::kRemoteOptimizationGuideFetching);
  }
};

TEST_F(PredictionManagerRemoteFetchingDisabledTest, RemoteFetchingDisabled) {
  CreatePredictionManager();

  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::
              kFetchSuccessWithModelsAndHostsModelFeatures));

  prediction_manager()->RegisterOptimizationTargets(
      {{proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, base::nullopt}});
  SetStoreInitialized();

  EXPECT_FALSE(prediction_model_fetcher()->models_fetched());
}

class PredictionManagerTest : public PredictionManagerTestBase {
 public:
  PredictionManagerTest() {
    // This needs to be done before any tasks are run that might check if a
    // feature is enabled, to avoid tsan errors.
    feature_list_.InitAndEnableFeature(
        features::kRemoteOptimizationGuideFetching);
  }
};

TEST_F(PredictionManagerTest, OptimizationTargetNotRegisteredForNavigation) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          GURL("https://foo.com"));

  CreatePredictionManager();

  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::
              kFetchSuccessWithModelsAndHostsModelFeatures));

  prediction_manager()->RegisterOptimizationTargets(
      {{proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, base::nullopt}});
  SetStoreInitialized();

  EXPECT_TRUE(prediction_model_fetcher()->models_fetched());

  EXPECT_EQ(OptimizationTargetDecision::kUnknown,
            prediction_manager()->ShouldTargetNavigation(
                navigation_handle.get(), proto::OPTIMIZATION_TARGET_UNKNOWN));
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelEvaluationLatency." +
          GetStringNameForOptimizationTarget(
              proto::OPTIMIZATION_TARGET_UNKNOWN),
      0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelEvaluationLatency." +
          GetStringNameForOptimizationTarget(
              proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      0);
}

TEST_F(PredictionManagerTest, AddObserverForOptimizationTargetModel) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          GURL("https://foo.com"));

  CreatePredictionManager();

  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchSuccessWithEmptyResponse));
  proto::Any model_metadata;
  model_metadata.set_type_url("whatever");
  prediction_model_fetcher()->SetExpectedModelMetadataForOptimizationTarget(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, model_metadata);

  FakeOptimizationTargetModelObserver observer;
  prediction_manager()->AddObserverForOptimizationTargetModel(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, model_metadata, &observer);
  SetStoreInitialized(/* load_models= */ false,
                      /* load_host_model_features= */ false,
                      /* have_models_in_store= */ false);

  EXPECT_TRUE(prediction_model_fetcher()->models_fetched());

  EXPECT_EQ(OptimizationTargetDecision::kModelNotAvailableOnClient,
            prediction_manager()->ShouldTargetNavigation(
                navigation_handle.get(),
                proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelEvaluationLatency." +
          GetStringNameForOptimizationTarget(
              proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      0);

  EXPECT_TRUE(prediction_manager()->GetRegisteredOptimizationTargets().contains(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));
  EXPECT_FALSE(observer
                   .last_received_model_for_target(
                       proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD)
                   .has_value());

  proto::ModelInfo model_info;
  model_info.set_optimization_target(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
  model_info.set_version(1);
  model_info.mutable_model_metadata()->set_type_url("sometypeurl");

  // Ensure observer is hooked up.
  proto::PredictionModel model1;
  *model1.mutable_model_info() = model_info;
  SetFilePathInPredictionModel(temp_dir().AppendASCII("whatever"), &model1);
  prediction_manager()->OnModelReady(model1);
  RunUntilIdle();

  base::Optional<std::pair<base::Optional<proto::Any>, base::FilePath>>
      received_model = observer.last_received_model_for_target(
          proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
  EXPECT_EQ(received_model->first->type_url(), "sometypeurl");
  EXPECT_EQ(received_model->second.BaseName().value(),
            FILE_PATH_LITERAL("whatever"));

  // Reset fetcher and make sure version is sent in the new request and not
  // counted as re-loaded or updated.
  {
    base::HistogramTester histogram_tester2;

    prediction_model_fetcher()->Reset();
    prediction_model_fetcher()->SetCheckExpectedVersion();
    prediction_model_fetcher()->SetExpectedVersionForOptimizationTarget(
        proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, 1);
    MoveClockForwardBy(
        base::TimeDelta::FromSeconds(kUpdateFetchModelAndFeaturesTimeSecs));
    EXPECT_TRUE(prediction_model_fetcher()->models_fetched());
    histogram_tester2.ExpectTotalCount(
        "OptimizationGuide.PredictionModelUpdateVersion.PainfulPageLoad", 0);
    histogram_tester2.ExpectTotalCount(
        "OptimizationGuide.PredictionModelLoadedVersion.PainfulPageLoad", 0);
    histogram_tester2.ExpectTotalCount(
        "OptimizationGuide.PredictionModelRemoved.PainfulPageLoad", 0);
  }

  // Now remove and reset observer.
  prediction_manager()->RemoveObserverForOptimizationTargetModel(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, &observer);
  observer.Reset();
  proto::PredictionModel model2;
  *model2.mutable_model_info() = model_info;
  model2.mutable_model_info()->set_version(2);
  SetFilePathInPredictionModel(temp_dir().AppendASCII("whatever2"), &model2);
  prediction_manager()->OnModelReady(model2);
  RunUntilIdle();

  // Last received path should not have been updated since the observer was
  // removed.
  EXPECT_FALSE(observer
                   .last_received_model_for_target(
                       proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD)
                   .has_value());
}

TEST_F(PredictionManagerTest,
       AddObserverForOptimizationTargetModelAddAnotherObserverForSameTarget) {
  // Fails under "threadsafe" mode.
  testing::GTEST_FLAG(death_test_style) = "fast";

  CreatePredictionManager();

  FakeOptimizationTargetModelObserver observer1;
  prediction_manager()->AddObserverForOptimizationTargetModel(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
      /*model_metadata=*/base::nullopt, &observer1);
  SetStoreInitialized(/* load_models= */ false,
                      /* load_host_model_features= */ false,
                      /* have_models_in_store= */ false);

  proto::ModelInfo model_info;
  model_info.set_optimization_target(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
  model_info.set_version(1);

  // Ensure observer is hooked up.
  proto::PredictionModel model1;
  *model1.mutable_model_info() = model_info;
  SetFilePathInPredictionModel(temp_dir().AppendASCII("whatever"), &model1);
  prediction_manager()->OnModelReady(model1);
  RunUntilIdle();

  EXPECT_EQ(observer1
                .last_received_model_for_target(
                    proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD)
                ->second.BaseName()
                .value(),
            FILE_PATH_LITERAL("whatever"));

#if !defined(OS_WIN)
  // Do not run the DCHECK death test on Windows since there's some weird
  // behavior there.

  // Now, register a new observer - it should die.
  FakeOptimizationTargetModelObserver observer2;
  EXPECT_DCHECK_DEATH(
      prediction_manager()->AddObserverForOptimizationTargetModel(
          proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
          /*model_metadata=*/base::nullopt, &observer2));
  RunUntilIdle();
#endif
}

TEST_F(PredictionManagerTest,
       AddObserverForOptimizationTargetModelCommandLineOverride) {
  optimization_guide::proto::Any metadata;
  metadata.set_type_url("sometypeurl");
  std::string encoded_metadata;
  metadata.SerializeToString(&encoded_metadata);
  base::Base64Encode(encoded_metadata, &encoded_metadata);
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kModelOverride,
      "OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD:somefilepath:" + encoded_metadata);

  CreatePredictionManager();

  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchSuccessWithEmptyResponse));
  proto::Any model_metadata;
  model_metadata.set_type_url("whatever");
  prediction_model_fetcher()->SetExpectedModelMetadataForOptimizationTarget(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, model_metadata);

  FakeOptimizationTargetModelObserver observer;
  prediction_manager()->AddObserverForOptimizationTargetModel(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, model_metadata, &observer);
  SetStoreInitialized(/* load_models= */ false,
                      /* load_host_model_features= */ false,
                      /* have_models_in_store= */ false);

  // Make sure no models are fetched.
  EXPECT_FALSE(prediction_model_fetcher()->models_fetched());

  EXPECT_TRUE(prediction_manager()->GetRegisteredOptimizationTargets().contains(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));
  EXPECT_EQ(observer
                .last_received_model_for_target(
                    proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD)
                ->first.value()
                .type_url(),
            "sometypeurl");
  EXPECT_EQ(observer
                .last_received_model_for_target(
                    proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD)
                ->second.value(),
            FILE_PATH_LITERAL("somefilepath"));

  // Now reset observer. New model downloads should not update the observer.
  observer.Reset();
  proto::PredictionModel model;
  model.mutable_model_info()->set_optimization_target(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
  model.mutable_model_info()->set_version(1);
  SetFilePathInPredictionModel(temp_dir().AppendASCII("whatever2"), &model);
  prediction_manager()->OnModelReady(model);
  RunUntilIdle();

  // Last received path should not have been updated since the observer was
  // reset and override is in place.
  EXPECT_FALSE(observer
                   .last_received_model_for_target(
                       proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD)
                   .has_value());
}

TEST_F(PredictionManagerTest,
       NoPredictionModelForRegisteredOptimizationTarget) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          GURL("https://foo.com"));

  CreatePredictionManager();
  prediction_manager()->RegisterOptimizationTargets(
      {{proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, base::nullopt}});

  EXPECT_EQ(OptimizationTargetDecision::kModelNotAvailableOnClient,
            prediction_manager()->ShouldTargetNavigation(
                navigation_handle.get(),
                proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelEvaluationLatency." +
          GetStringNameForOptimizationTarget(
              proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      0);
}

TEST_F(PredictionManagerTest, EvaluatePredictionModel) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          GURL("https://foo.com"));

  CreatePredictionManager();
  // The model will be loaded from the store.
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchSuccessWithEmptyResponse));

  prediction_manager()->RegisterOptimizationTargets(
      {{proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, base::nullopt}});
  SetStoreInitialized();
  EXPECT_TRUE(prediction_model_fetcher()->models_fetched());

  EXPECT_EQ(OptimizationTargetDecision::kPageLoadMatches,
            prediction_manager()->ShouldTargetNavigation(
                navigation_handle.get(),
                proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));
  TestPredictionModel* test_prediction_model =
      static_cast<TestPredictionModel*>(
          prediction_manager()->GetPredictionModelForTesting(
              proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));
  EXPECT_TRUE(test_prediction_model);
  EXPECT_TRUE(test_prediction_model->WasModelEvaluated());

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelEvaluationLatency." +
          GetStringNameForOptimizationTarget(
              proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      1);

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.IsPredictionModelValid." +
          GetStringNameForOptimizationTarget(
              proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      true, 1);

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.IsPredictionModelValid", true, 1);

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelValidationLatency." +
          GetStringNameForOptimizationTarget(
              proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelValidationLatency", 1);
}

TEST_F(PredictionManagerTest, UpdatePredictionModelsWithInvalidModel) {
  base::HistogramTester histogram_tester;
  CreatePredictionManager();
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchFailed));

  prediction_manager()->RegisterOptimizationTargets(
      {{proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, base::nullopt}});

  std::unique_ptr<proto::GetModelsResponse> get_models_response =
      BuildGetModelsResponse({} /* hosts */, {} /* client features */);

  // Override the manager so that any prediction model updates will be seen as
  // invalid.
  prediction_manager()->set_create_valid_prediction_model(false);
  prediction_manager()->UpdatePredictionModelsForTesting(
      get_models_response.get());

  histogram_tester.ExpectBucketCount("OptimizationGuide.IsPredictionModelValid",
                                     false, 1);

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelValidationLatency", 0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelUpdateVersion.PainfulPageLoad", 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelLoadedVersion.PainfulPageLoad", 0);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelRemoved.PainfulPageLoad", true, 1);
}

TEST_F(PredictionManagerTest, UpdateModelWithSameVersion) {
  base::HistogramTester histogram_tester;
  CreatePredictionManager();
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchFailed));

  prediction_manager()->RegisterOptimizationTargets(
      {{proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, base::nullopt}});

  // Seed the PredictionManager with a prediction model with a higher version
  // to try to be updated.
  std::unique_ptr<proto::GetModelsResponse> get_models_response =
      BuildGetModelsResponse({} /* hosts */, {} /* client features */);
  get_models_response->mutable_models(0)->mutable_model_info()->set_version(3);

  prediction_manager()->UpdatePredictionModelsForTesting(
      get_models_response.get());
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionManager.PredictionModelsStored", true, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelUpdateVersion.PainfulPageLoad", 3, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionManager.ModelTypeChanged.PainfulPageLoad",
      false, 1);

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
  histogram_tester.ExpectBucketCount("OptimizationGuide.IsPredictionModelValid",
                                     true, 2);
}

TEST_F(PredictionManagerTest, UpdateModelFileWithSameVersion) {
  base::HistogramTester histogram_tester;

  CreatePredictionManager();

  FakeOptimizationTargetModelObserver observer;
  prediction_manager()->AddObserverForOptimizationTargetModel(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
      /*model_metadata=*/base::nullopt, &observer);

  proto::PredictionModel model;
  model.mutable_model_info()->set_optimization_target(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
  model.mutable_model_info()->set_version(3);
  SetFilePathInPredictionModel(temp_dir().AppendASCII("whatever"), &model);
  prediction_manager()->OnModelReady(model);
  RunUntilIdle();

  EXPECT_TRUE(observer
                  .last_received_model_for_target(
                      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD)
                  .has_value());

  // Now reset the observer state.
  observer.Reset();

  // Send the same model again.
  prediction_manager()->OnModelReady(model);

  // The observer should not have received an update.
  EXPECT_FALSE(observer
                   .last_received_model_for_target(
                       proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD)
                   .has_value());

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionManager.ModelTypeChanged.PainfulPageLoad",
      false, 1);
}

TEST_F(PredictionManagerTest, DownloadManagerUnavailableShouldNotFetch) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          GURL("https://foo.com"));

  CreatePredictionManager();
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchSuccessWithModelDownloadUrls));
  prediction_manager()->SetPredictionModelDownloadManagerForTesting(
      std::make_unique<FakePredictionModelDownloadManager>(
          task_environment()->GetMainThreadTaskRunner()));
  prediction_model_download_manager()->SetAvailableForDownloads(false);

  prediction_manager()->RegisterOptimizationTargets(
      {{proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, base::nullopt}});

  SetStoreInitialized();
  EXPECT_FALSE(prediction_model_fetcher()->models_fetched());

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionManager."
      "DownloadServiceAvailabilityBlockedFetch",
      true, 1);
}

TEST_F(PredictionManagerTest, UpdateModelWithDownloadUrl) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          GURL("https://foo.com"));

  CreatePredictionManager();
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchSuccessWithModelDownloadUrls));
  prediction_manager()->SetPredictionModelDownloadManagerForTesting(
      std::make_unique<FakePredictionModelDownloadManager>(
          task_environment()->GetMainThreadTaskRunner()));

  prediction_manager()->RegisterOptimizationTargets(
      {{proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, base::nullopt}});

  SetStoreInitialized();
  EXPECT_TRUE(prediction_model_fetcher()->models_fetched());
  EXPECT_TRUE(prediction_model_download_manager()->cancel_downloads_called());

  models_and_features_store()->RunUpdateHostModelFeaturesCallback();
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionManager.HostModelFeaturesStored", true, 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionManager.PredictionModelsStored", 0);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionManager."
      "DownloadServiceAvailabilityBlockedFetch",
      false, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionManager.IsDownloadUrlValid", true, 1);

  EXPECT_EQ(prediction_model_download_manager()->last_requested_download(),
            GURL("https://example.com/model"));
}

TEST_F(PredictionManagerTest, ShouldTargetNavigationStoreAvailableNoModel) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          GURL("https://foo.com"));

  CreatePredictionManager();
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchSuccessWithEmptyResponse));

  prediction_manager()->RegisterOptimizationTargets(
      {{proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, base::nullopt}});

  SetStoreInitialized(/* load_models= */ false,
                      /* load_host_model_features= */ true,
                      /* have_models_in_store= */ false);

  EXPECT_EQ(OptimizationTargetDecision::kModelNotAvailableOnClient,
            prediction_manager()->ShouldTargetNavigation(
                navigation_handle.get(),
                proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.ShouldTargetNavigation.PredictionModelStatus",
      PredictionManagerModelStatus::kStoreAvailableNoModelForTarget, 1);

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.ShouldTargetNavigation.PredictionModelStatus." +
          GetStringNameForOptimizationTarget(
              proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      PredictionManagerModelStatus::kStoreAvailableNoModelForTarget, 1);
}

TEST_F(PredictionManagerTest,
       ShouldTargetNavigationStoreAvailableModelNotLoaded) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          GURL("https://foo.com"));

  CreatePredictionManager();
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchSuccessWithEmptyResponse));

  prediction_manager()->RegisterOptimizationTargets(
      {{proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, base::nullopt}});

  SetStoreInitialized(/* load_models= */ false,
                      /* load_host_model_features= */ true,
                      /* have_models_in_store= */ true);

  EXPECT_EQ(OptimizationTargetDecision::kModelNotAvailableOnClient,
            prediction_manager()->ShouldTargetNavigation(
                navigation_handle.get(),
                proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.ShouldTargetNavigation.PredictionModelStatus",
      PredictionManagerModelStatus::kStoreAvailableModelNotLoaded, 1);

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.ShouldTargetNavigation.PredictionModelStatus." +
          GetStringNameForOptimizationTarget(
              proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      PredictionManagerModelStatus::kStoreAvailableModelNotLoaded, 1);

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelLoadedVersion.PainfulPageLoad", 0);
}

TEST_F(PredictionManagerTest,
       ShouldTargetNavigationStoreUnavailableModelUnknown) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          GURL("https://foo.com"));

  CreatePredictionManager();
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchSuccessWithEmptyResponse));

  prediction_manager()->RegisterOptimizationTargets(
      {{proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, base::nullopt}});

  EXPECT_EQ(OptimizationTargetDecision::kModelNotAvailableOnClient,
            prediction_manager()->ShouldTargetNavigation(
                navigation_handle.get(),
                proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.ShouldTargetNavigation.PredictionModelStatus",
      PredictionManagerModelStatus::kStoreUnavailableModelUnknown, 1);

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.ShouldTargetNavigation.PredictionModelStatus." +
          GetStringNameForOptimizationTarget(
              proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      PredictionManagerModelStatus::kStoreUnavailableModelUnknown, 1);
}

TEST_F(PredictionManagerTest, UpdateModelForUnregisteredTarget) {
  base::HistogramTester histogram_tester;
  CreatePredictionManager();
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::
              kFetchSuccessWithModelsAndHostsModelFeatures));

  prediction_manager()->RegisterOptimizationTargets({});
  SetStoreInitialized();

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

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionManager.PredictionModelsStored", 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionManager.HostModelFeaturesStored", 0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelLoadedVersion.PainfulPageLoad", 0);
}

TEST_F(PredictionManagerTest, UpdateModelForUnregisteredTargetOnModelReady) {
  base::HistogramTester histogram_tester;
  CreatePredictionManager();

  prediction_manager()->RegisterOptimizationTargets({});
  SetStoreInitialized();

  proto::PredictionModel model;
  model.mutable_model_info()->set_optimization_target(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
  model.mutable_model_info()->set_version(3);
  SetFilePathInPredictionModel(temp_dir().AppendASCII("whatever"), &model);
  prediction_manager()->OnModelReady(model);

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionManager.PredictionModelsStored", 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionManager.HostModelFeaturesStored", 0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelLoadedVersion.PainfulPageLoad", 0);
}

TEST_F(PredictionManagerTest, UpdateModelForRegisteredTargetButNowFile) {
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          GURL("https://foo.com"));

  base::HistogramTester histogram_tester;
  CreatePredictionManager();

  prediction_manager()->RegisterOptimizationTargets(
      {{proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, base::nullopt}});
  SetStoreInitialized();
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelLoadedVersion.PainfulPageLoad", 1, 1);

  EXPECT_EQ(OptimizationTargetDecision::kPageLoadMatches,
            prediction_manager()->ShouldTargetNavigation(
                navigation_handle.get(),
                proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));

  // Now, update the model to be a file.
  proto::PredictionModel model;
  model.mutable_model_info()->set_optimization_target(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
  model.mutable_model_info()->set_version(3);
  SetFilePathInPredictionModel(temp_dir().AppendASCII("whatever"), &model);
  prediction_manager()->OnModelReady(model);

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionManager.PredictionModelsStored", 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionManager.HostModelFeaturesStored", 0);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.PredictionModelLoadedVersion.PainfulPageLoad", 3, 1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.PredictionManager.ModelTypeChanged.PainfulPageLoad",
      true, 1);

  // Expect that the old decision tree should not be used.
  EXPECT_EQ(OptimizationTargetDecision::kModelNotAvailableOnClient,
            prediction_manager()->ShouldTargetNavigation(
                navigation_handle.get(),
                proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));
}

TEST_F(PredictionManagerTest, UpdateModelWithUnsupportedOptimizationTarget) {
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          GURL("https://foo.com"));

  CreatePredictionManager();
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchFailed));

  prediction_manager()->RegisterOptimizationTargets(
      {{proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, base::nullopt}});

  EXPECT_FALSE(prediction_model_fetcher()->models_fetched());
  EXPECT_FALSE(models_and_features_store()->WasModelLoaded());

  std::unique_ptr<proto::GetModelsResponse> get_models_response =
      BuildGetModelsResponse({}, {});
  get_models_response->mutable_models(0)
      ->mutable_model_info()
      ->clear_optimization_target();
  prediction_manager()->UpdatePredictionModelsForTesting(
      get_models_response.get());

  EXPECT_EQ(OptimizationTargetDecision::kModelNotAvailableOnClient,
            prediction_manager()->ShouldTargetNavigation(
                navigation_handle.get(),
                proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));

  TestPredictionModel* test_prediction_model =
      static_cast<TestPredictionModel*>(
          prediction_manager()->GetPredictionModelForTesting(
              proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));
  EXPECT_FALSE(test_prediction_model);
  EXPECT_FALSE(models_and_features_store()->WasModelLoaded());
}

TEST_F(PredictionManagerTest, HasHostModelFeaturesForHost) {
  base::HistogramTester histogram_tester;

  CreatePredictionManager();
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchFailed));

  prediction_manager()->RegisterOptimizationTargets(
      {{proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, base::nullopt}});
  SetStoreInitialized();

  std::unique_ptr<proto::GetModelsResponse> get_models_response =
      BuildGetModelsResponse({"example1.com", "example2.com"}, {});
  prediction_manager()->UpdateHostModelFeaturesForTesting(
      get_models_response.get());

  base::Optional<base::flat_map<std::string, float>> host_model_features_map =
      prediction_manager()->GetHostModelFeaturesForHost("example1.com");
  EXPECT_TRUE(host_model_features_map);
  EXPECT_TRUE(host_model_features_map->contains("host_feat1"));
  EXPECT_EQ(2.0, (*host_model_features_map)["host_feat1"]);

  host_model_features_map =
      prediction_manager()->GetHostModelFeaturesForHost("example2.com");
  EXPECT_TRUE(host_model_features_map);
  EXPECT_TRUE(host_model_features_map->contains("host_feat1"));
  EXPECT_EQ(2.0, (*host_model_features_map)["host_feat1"]);
}

TEST_F(PredictionManagerTest, NoHostModelFeaturesForHost) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          GURL("https://bar.com"));

  CreatePredictionManager();
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchFailed));

  prediction_manager()->RegisterOptimizationTargets(
      {{proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, base::nullopt}});

  SetStoreInitialized();

  EXPECT_EQ(OptimizationTargetDecision::kPageLoadMatches,
            prediction_manager()->ShouldTargetNavigation(
                navigation_handle.get(),
                proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));

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
  EXPECT_LT(test_prediction_model->last_evaluated_features()["host_feat1"], 0);

  EXPECT_FALSE(prediction_manager()->GetHostModelFeaturesForHost("bar.com"));
  // One item loaded from the store when initialized.
  EXPECT_EQ(1u, prediction_manager()->GetHostModelFeaturesForTesting()->size());
}

TEST_F(PredictionManagerTest, UpdateHostModelFeaturesMissingHost) {
  CreatePredictionManager();
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchFailed));

  prediction_manager()->RegisterOptimizationTargets(
      {{proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, base::nullopt}});

  SetStoreInitialized();

  std::unique_ptr<proto::GetModelsResponse> get_models_response =
      BuildGetModelsResponse({"example1.com"}, {});
  get_models_response->mutable_host_model_features(0)->clear_host();

  prediction_manager()->UpdateHostModelFeaturesForTesting(
      get_models_response.get());

  EXPECT_FALSE(
      prediction_manager()->GetHostModelFeaturesForHost("example1.com"));
  // One item loaded from the store when initialized.
  EXPECT_EQ(1u, prediction_manager()->GetHostModelFeaturesForTesting()->size());
}

TEST_F(PredictionManagerTest, UpdateHostModelFeaturesNoFeature) {
  CreatePredictionManager();
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchFailed));

  prediction_manager()->RegisterOptimizationTargets(
      {{proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, base::nullopt}});
  SetStoreInitialized();

  std::unique_ptr<proto::GetModelsResponse> get_models_response =
      BuildGetModelsResponse({"example1.com"}, {});
  get_models_response->mutable_host_model_features(0)->clear_model_features();

  prediction_manager()->UpdateHostModelFeaturesForTesting(
      get_models_response.get());

  EXPECT_FALSE(
      prediction_manager()->GetHostModelFeaturesForHost("example1.com"));
  // One item loaded from the store when initialized.
  EXPECT_EQ(1u, prediction_manager()->GetHostModelFeaturesForTesting()->size());
}

TEST_F(PredictionManagerTest, UpdateHostModelFeaturesNoFeatureName) {
  CreatePredictionManager();
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchFailed));

  prediction_manager()->RegisterOptimizationTargets(
      {{proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, base::nullopt}});

  SetStoreInitialized();

  std::unique_ptr<proto::GetModelsResponse> get_models_response =
      BuildGetModelsResponse({"example1.com"}, {});
  get_models_response->mutable_host_model_features(0)
      ->mutable_model_features(0)
      ->clear_feature_name();

  prediction_manager()->UpdateHostModelFeaturesForTesting(
      get_models_response.get());

  EXPECT_FALSE(
      prediction_manager()->GetHostModelFeaturesForHost("example1.com"));
  // One item loaded from the store when initialized.
  EXPECT_EQ(1u, prediction_manager()->GetHostModelFeaturesForTesting()->size());
}

TEST_F(PredictionManagerTest, UpdateHostModelFeaturesDoubleValue) {
  CreatePredictionManager();
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchFailed));

  prediction_manager()->RegisterOptimizationTargets(
      {{proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, base::nullopt}});

  SetStoreInitialized();
  std::unique_ptr<proto::GetModelsResponse> get_models_response =
      BuildGetModelsResponse({"example1.com"}, {});
  get_models_response->mutable_host_model_features(0)
      ->mutable_model_features(0)
      ->set_double_value(3.0);

  prediction_manager()->UpdateHostModelFeaturesForTesting(
      get_models_response.get());

  auto host_model_features =
      prediction_manager()->GetHostModelFeaturesForHost("example1.com");
  EXPECT_TRUE(host_model_features);
  EXPECT_EQ(3.0, (*host_model_features)["host_feat1"]);
}

TEST_F(PredictionManagerTest, UpdateHostModelFeaturesIntValue) {
  CreatePredictionManager();
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchFailed));

  prediction_manager()->RegisterOptimizationTargets(
      {{proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, base::nullopt}});

  SetStoreInitialized();
  std::unique_ptr<proto::GetModelsResponse> get_models_response =
      BuildGetModelsResponse({"example1.com"}, {});
  get_models_response->mutable_host_model_features(0)
      ->mutable_model_features(0)
      ->set_int64_value(4);

  prediction_manager()->UpdateHostModelFeaturesForTesting(
      get_models_response.get());

  auto host_model_features =
      prediction_manager()->GetHostModelFeaturesForHost("example1.com");
  EXPECT_TRUE(host_model_features);
  // We expect the value to be stored as a float but is created from an int64
  // value.
  EXPECT_EQ(4.0, (*host_model_features)["host_feat1"]);
}

TEST_F(PredictionManagerTest, RestrictHostModelFeaturesCacheSize) {
  CreatePredictionManager();
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchFailed));

  prediction_manager()->RegisterOptimizationTargets(
      {{proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, base::nullopt}});

  SetStoreInitialized();
  std::vector<std::string> hosts;
  for (size_t i = 0; i <= features::MaxHostModelFeaturesCacheSize() + 1; i++)
    hosts.push_back("host" + base::NumberToString(i) + ".com");
  std::unique_ptr<proto::GetModelsResponse> get_models_response =
      BuildGetModelsResponse(hosts, {});

  prediction_manager()->UpdateHostModelFeaturesForTesting(
      get_models_response.get());

  auto* host_model_features_cache =
      prediction_manager()->GetHostModelFeaturesForTesting();
  EXPECT_EQ(features::MaxHostModelFeaturesCacheSize(),
            host_model_features_cache->size());
}

TEST_F(PredictionManagerTest, FetchWithoutTopHostProvider) {
  base::HistogramTester histogram_tester;

  CreatePredictionManagerWithoutTopHostProvider();
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::
              kFetchSuccessWithModelsAndHostsModelFeatures));

  prediction_manager()->RegisterOptimizationTargets(
      {{proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, base::nullopt}});

  SetStoreInitialized();

  EXPECT_TRUE(prediction_model_fetcher()->models_fetched());

  // No hosts should be included in the fetch as the top host provider is not
  // available.
  EXPECT_EQ(prediction_model_fetcher()->hosts_fetched(), 0ul);
}

TEST_F(PredictionManagerTest, UpdateHostModelFeaturesUpdateDataInMap) {
  base::HistogramTester histogram_tester;

  CreatePredictionManager();
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchFailed));

  prediction_manager()->RegisterOptimizationTargets(
      {{proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, base::nullopt}});

  SetStoreInitialized();
  std::unique_ptr<proto::GetModelsResponse> get_models_response =
      BuildGetModelsResponse({"example1.com"}, {});
  get_models_response->mutable_host_model_features(0)
      ->mutable_model_features(0)
      ->set_int64_value(4);

  prediction_manager()->UpdateHostModelFeaturesForTesting(
      get_models_response.get());

  auto host_model_features =
      prediction_manager()->GetHostModelFeaturesForHost("example1.com");
  EXPECT_TRUE(host_model_features);
  // We expect the value to be stored as a float but is created from an int64
  // value.
  EXPECT_EQ(4.0, (*host_model_features)["host_feat1"]);

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

  host_model_features =
      prediction_manager()->GetHostModelFeaturesForHost("example1.com");
  EXPECT_TRUE(host_model_features);

  // We expect the value to be stored as a float but is created from an int64
  // value.
  EXPECT_EQ(5.0, (*host_model_features)["host_feat1"]);
  EXPECT_TRUE((*host_model_features).contains("host_feat_added"));
  EXPECT_EQ(6.0, (*host_model_features)["host_feat_added"]);
}

TEST_F(PredictionManagerTest,
       StoreInitializedAfterOptimizationTargetRegistered) {
  base::HistogramTester histogram_tester;
  CreatePredictionManager();
  // Ensure that the fetch does not cause any models or features to load.
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchFailed));
  prediction_manager()->RegisterOptimizationTargets(
      {{proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, base::nullopt}});
  EXPECT_FALSE(models_and_features_store()->WasHostModelFeaturesLoaded());
  EXPECT_FALSE(models_and_features_store()->WasModelLoaded());
  EXPECT_FALSE(prediction_manager()->GetHostModelFeaturesForHost("foo.com"));

  SetStoreInitialized();
  EXPECT_TRUE(models_and_features_store()->WasHostModelFeaturesLoaded());
  EXPECT_TRUE(models_and_features_store()->WasModelLoaded());
  EXPECT_TRUE(prediction_manager()->GetHostModelFeaturesForHost("foo.com"));

  EXPECT_FALSE(prediction_model_fetcher()->models_fetched());
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelLoadedVersion.PainfulPageLoad", 1, 1);
}

TEST_F(PredictionManagerTest,
       StoreInitializedBeforeOptimizationTargetRegistered) {
  base::HistogramTester histogram_tester;
  CreatePredictionManager();
  // Ensure that the fetch does not cause any models or features to load.
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchFailed));
  SetStoreInitialized();

  EXPECT_FALSE(models_and_features_store()->WasHostModelFeaturesLoaded());
  EXPECT_FALSE(models_and_features_store()->WasModelLoaded());
  EXPECT_FALSE(prediction_manager()->GetHostModelFeaturesForHost("foo.com"));
  prediction_manager()->RegisterOptimizationTargets(
      {{proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, base::nullopt}});
  RunUntilIdle();

  EXPECT_TRUE(models_and_features_store()->WasHostModelFeaturesLoaded());
  EXPECT_TRUE(models_and_features_store()->WasModelLoaded());
  EXPECT_TRUE(prediction_manager()->GetHostModelFeaturesForHost("foo.com"));

  EXPECT_FALSE(prediction_model_fetcher()->models_fetched());
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelLoadedVersion.PainfulPageLoad", 1, 1);
}

TEST_F(PredictionManagerTest, ModelFetcherTimerRetryDelay) {
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      switches::kFetchModelsAndHostModelFeaturesOverrideTimer);

  CreatePredictionManager();
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchFailed));

  prediction_manager()->RegisterOptimizationTargets(
      {{proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, base::nullopt}});

  SetStoreInitialized();
  EXPECT_FALSE(prediction_model_fetcher()->models_fetched());

  MoveClockForwardBy(base::TimeDelta::FromSeconds(kTestFetchRetryDelaySecs));
  EXPECT_FALSE(prediction_model_fetcher()->models_fetched());

  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::
              kFetchSuccessWithModelsAndHostsModelFeatures));

  MoveClockForwardBy(base::TimeDelta::FromSeconds(kTestFetchRetryDelaySecs));
  EXPECT_TRUE(prediction_model_fetcher()->models_fetched());
}

TEST_F(PredictionManagerTest, ModelFetcherTimerFetchSucceeds) {
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      switches::kFetchModelsAndHostModelFeaturesOverrideTimer);

  CreatePredictionManager();
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::
              kFetchSuccessWithModelsAndHostsModelFeatures));

  g_browser_process->SetApplicationLocale("en-US");

  prediction_manager()->RegisterOptimizationTargets(
      {{proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, base::nullopt}});

  SetStoreInitialized();
  EXPECT_FALSE(prediction_model_fetcher()->models_fetched());
  MoveClockForwardBy(base::TimeDelta::FromSeconds(kTestFetchRetryDelaySecs));
  EXPECT_TRUE(prediction_model_fetcher()->models_fetched());
  EXPECT_EQ("en-US", prediction_model_fetcher()->locale_requested());

  // Reset the prediction model fetcher to detect when the next fetch occurs.
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::
              kFetchSuccessWithModelsAndHostsModelFeatures));
  MoveClockForwardBy(base::TimeDelta::FromSeconds(kTestFetchRetryDelaySecs));
  EXPECT_FALSE(prediction_model_fetcher()->models_fetched());
  MoveClockForwardBy(
      base::TimeDelta::FromSeconds(kUpdateFetchModelAndFeaturesTimeSecs));
  EXPECT_TRUE(prediction_model_fetcher()->models_fetched());
}

}  // namespace optimization_guide
