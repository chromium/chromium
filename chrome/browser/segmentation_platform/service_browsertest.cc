// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/metrics/metrics_hashes.h"
#include "base/metrics/statistics_recorder.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"
#include "chrome/browser/segmentation_platform/ukm_data_manager_test_utils.h"
#include "chrome/browser/segmentation_platform/ukm_database_client.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/optimization_guide/core/model_info.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_observer.h"
#include "components/prefs/pref_service.h"
#include "components/segmentation_platform/embedder/default_model/database_api_clients.h"
#include "components/segmentation_platform/embedder/default_model/optimization_target_segmentation_dummy.h"
#include "components/segmentation_platform/internal/constants.h"
#include "components/segmentation_platform/internal/database/client_result_prefs.h"
#include "components/segmentation_platform/internal/database/ukm_database.h"
#include "components/segmentation_platform/internal/execution/mock_model_provider.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/internal/segmentation_platform_service_impl.h"
#include "components/segmentation_platform/internal/stats.h"
#include "components/segmentation_platform/internal/ukm_data_manager.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/database_client.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "components/segmentation_platform/public/proto/aggregation.pb.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"
#include "components/segmentation_platform/public/result.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"
#include "components/ukm/ukm_service.h"
#include "content/public/test/browser_test.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace segmentation_platform {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SaveArg;

constexpr SegmentId kSegmentId1 =
    SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_CHROME_LOW_USER_ENGAGEMENT;

constexpr SegmentId kSegmentId2 =
    SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_DUMMY;

constexpr SegmentId kSegmentId3 =
    SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SEARCH_USER;

constexpr char kFeatureProcessingHistogram[] =
    "SegmentationPlatform.FeatureProcessing.Error.";

constexpr char kSqlFeatureQuery[] = "SELECT COUNT(*) from metrics";

class SegmentationPlatformTest : public PlatformBrowserTest {
 public:
  explicit SegmentationPlatformTest(bool setup_feature_list = true) {
    if (!setup_feature_list) {
      return;
    }
    // Low Engagement Segment is used to test segmentation service without multi
    // output. Search User Segment supports  multi output path.
    feature_list_.InitWithFeaturesAndParameters(
        {base::test::FeatureRefAndParams(features::kSegmentationPlatformFeature,
                                         {}),
         base::test::FeatureRefAndParams(
             features::kSegmentationPlatformUkmEngine, {}),
         base::test::FeatureRefAndParams(
             features::kSegmentationPlatformLowEngagementFeature,
             {{"enable_default_model", "true"}}),
         base::test::FeatureRefAndParams(
             features::kSegmentationPlatformSearchUser,
             {{"enable_default_model", "true"}}),
         base::test::FeatureRefAndParams(
             kSegmentationPlatformOptimizationTargetSegmentationDummy, {})},
        {});
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch("segmentation-platform-refresh-results");
    command_line->AppendSwitch(
        "segmentation-platform-disable-model-execution-delay");
  }

  SegmentationPlatformService* GetService() {
    return segmentation_platform::SegmentationPlatformServiceFactory::
        GetForProfile(chrome_test_utils::GetProfile(this));
  }

  bool HasClientResultPref(const std::string& segmentation_key) {
    PrefService* pref_service = chrome_test_utils::GetProfile(this)->GetPrefs();
    std::unique_ptr<ClientResultPrefs> result_prefs_ =
        std::make_unique<ClientResultPrefs>(pref_service);
    return result_prefs_->ReadClientResultFromPrefs(segmentation_key) !=
           nullptr;
  }

  void OnClientResultPrefUpdated() {
    if (!wait_for_pref_callback_.is_null() &&
        HasClientResultPref(kSearchUserKey)) {
      std::move(wait_for_pref_callback_).Run();
    }
  }

  void WaitForClientResultPrefUpdate() {
    if (HasClientResultPref(kSearchUserKey)) {
      return;
    }

    base::RunLoop wait_for_pref;
    wait_for_pref_callback_ = wait_for_pref.QuitClosure();
    pref_registrar_.Init(chrome_test_utils::GetProfile(this)->GetPrefs());
    pref_registrar_.Add(
        kSegmentationClientResultPrefs,
        base::BindRepeating(
            &SegmentationPlatformTest::OnClientResultPrefUpdated,
            weak_ptr_factory_.GetWeakPtr()));
    wait_for_pref.Run();

    pref_registrar_.RemoveAll();
  }

  void WaitForPlatformInit() {
    base::RunLoop wait_for_init;
    SegmentationPlatformService* service = GetService();
    while (!service->IsPlatformInitialized()) {
      wait_for_init.RunUntilIdle();
    }
  }

  void ExpectDatabaseQuery(const std::vector<std::string>& metrics,
                           const ModelProvider::Request& result) {
    DatabaseClient* client = GetService()->GetDatabaseClient();
    ASSERT_TRUE(client);

    proto::SegmentationModelMetadata metadata;
    MetadataWriter writer(&metadata);
    writer.SetDefaultSegmentationMetadataConfig();
    for (const std::string& metric : metrics) {
      DatabaseApiClients::AddSumQuery(writer, metric, /*days=*/1);
    }

    base::RunLoop wait;
    client->ProcessFeatures(
        metadata, base::Time::Now() + base::Minutes(1),
        base::BindOnce(
            [](base::OnceClosure quit,
               const ModelProvider::Request& expected_result,
               DatabaseClient::ResultStatus status,
               const ModelProvider::Request& result) {
              EXPECT_EQ(status, DatabaseClient::ResultStatus::kSuccess);
              EXPECT_EQ(expected_result, result);
              std::move(quit).Run();
            },
            wait.QuitClosure(), result));
    wait.Run();
  }

  void RunProcessFeaturesAndCallback(
      const proto::SegmentationModelMetadata& metadata,
      DatabaseClient::FeaturesCallback callback) {
    DatabaseClient* client = GetService()->GetDatabaseClient();
    ASSERT_TRUE(client);

    base::RunLoop wait;
    client->ProcessFeatures(metadata, base::Time::Now() + base::Minutes(1),
                            base::BindOnce(
                                [](base::OnceClosure quit,
                                   DatabaseClient::FeaturesCallback callback,
                                   DatabaseClient::ResultStatus status,
                                   const ModelProvider::Request& result) {
                                  std::move(callback).Run(status, result);
                                  std::move(quit).Run();
                                },
                                wait.QuitClosure(), std::move(callback)));
    wait.Run();
  }

  void WaitForSegmentInfoDatabaseUpdate(
      SegmentId segment_id,
      const base::HistogramTester& histogram_tester) {
    std::string database_update_histogram =
        "SegmentationPlatform.SegmentInfoDatabase.ProtoDBUpdateResult." +
        SegmentIdToHistogramVariant(segment_id);
    // Wait for model update to be written to disk.
    WaitForHistogram(database_update_histogram, histogram_tester);
    int success_count =
        histogram_tester.GetBucketCount(database_update_histogram, 1);
    ASSERT_GE(success_count, 1);
  }

  void ExpectClassificationResult(const std::string& segmentation_key,
                                  PredictionStatus expected_prediction_status) {
    SegmentationPlatformService* service = GetService();
    PredictionOptions options;
    options.on_demand_execution = false;
    base::RunLoop wait_for_segment;
    service->GetClassificationResult(
        segmentation_key, options, nullptr,
        base::BindOnce(&SegmentationPlatformTest::OnGetClassificationResult,
                       weak_ptr_factory_.GetWeakPtr(),
                       wait_for_segment.QuitClosure(),
                       expected_prediction_status));
    wait_for_segment.Run();
  }

  void OnGetClassificationResult(base::RepeatingClosure closure,
                                 PredictionStatus expected_prediction_status,
                                 const ClassificationResult& actual) {
    EXPECT_EQ(expected_prediction_status, actual.status);
    EXPECT_TRUE(actual.ordered_labels.size() > 0);
    std::move(closure).Run();
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  std::unique_ptr<optimization_guide::ModelInfo>
  CreateOptimizationGuideModelInfo(
      std::optional<proto::SegmentationModelMetadata>
          segmentation_model_metadata) {
    auto model_info_builder = optimization_guide::TestModelInfoBuilder();
    if (segmentation_model_metadata.has_value()) {
      std::string serialized_metadata;
      segmentation_model_metadata.value().SerializeToString(
          &serialized_metadata);
      optimization_guide::proto::Any any_proto;
      auto any = std::make_optional(any_proto);
      any->set_value(serialized_metadata);
      any->set_type_url(
          "type.googleapis.com/"
          "segmentation_platform.proto.SegmentationModelMetadata");
      model_info_builder.SetModelMetadata(any);
    }
    return model_info_builder.Build();
  }

  proto::SegmentationModelMetadata GetSegmentationModelMetadataWithSignals() {
    std::array<MetadataWriter::UMAFeature, 5> uma_features = {
        MetadataWriter::UMAFeature::FromUserAction("Action.Foo", 7),
        MetadataWriter::UMAFeature::FromUserAction("Action.Bar", 7),
        MetadataWriter::UMAFeature::FromUserAction("Action.Baz", 7),
        MetadataWriter::UMAFeature::FromValueHistogram("Histogram.Foo", 7,
                                                       proto::Aggregation::SUM),
        MetadataWriter::UMAFeature::FromValueHistogram("Histogram.Bar", 7,
                                                       proto::Aggregation::SUM),
    };

    proto::SegmentationModelMetadata search_user_metadata;
    MetadataWriter writer = MetadataWriter(&search_user_metadata);
    writer.SetSegmentationMetadataConfig(proto::TimeUnit::DAY, 1, 7, 7, 7);
    writer.AddUmaFeatures(uma_features.data(), uma_features.size());

    return search_user_metadata;
  }

  void WaitForHistogram(const std::string& histogram_name,
                        const base::HistogramTester& histogram_tester) {
    // Continue if histogram was already recorded.
    if (histogram_tester.GetAllSamples(histogram_name).size() > 0) {
      return;
    }

    // Else, wait until the histogram is recorded.
    base::RunLoop run_loop;
    auto histogram_observer = std::make_unique<
        base::StatisticsRecorder::ScopedHistogramSampleObserver>(
        histogram_name,
        base::BindLambdaForTesting(
            [&](const char* histogram_name, uint64_t name_hash,
                base::HistogramBase::Sample sample) { run_loop.Quit(); }));
    run_loop.Run();
  }

 protected:
  base::HistogramTester histogram_tester_;
  base::test::ScopedFeatureList feature_list_;
  PrefChangeRegistrar pref_registrar_;
  base::OnceClosure wait_for_pref_callback_;
  base::WeakPtrFactory<SegmentationPlatformTest> weak_ptr_factory_{this};
};

// https://crbug.com/1257820 -- Tests using "PRE_" don't work on Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_PRE_CachedClassificationModel \
  DISABLED_PRE_CachedClassificationModel
#define MAYBE_CachedClassificationModel DISABLED_CachedClassificationModel
#else
#define MAYBE_PRE_CachedClassificationModel PRE_CachedClassificationModel
#define MAYBE_CachedClassificationModel CachedClassificationModel
#endif

IN_PROC_BROWSER_TEST_F(SegmentationPlatformTest,
                       MAYBE_PRE_CachedClassificationModel) {
  WaitForPlatformInit();
  WaitForClientResultPrefUpdate();
}

IN_PROC_BROWSER_TEST_F(SegmentationPlatformTest,
                       MAYBE_CachedClassificationModel) {
  WaitForPlatformInit();
  // Result is available from previous session's prefs.
  ExpectClassificationResult(
      kSearchUserKey,
      /*expected_prediction_status=*/PredictionStatus::kSucceeded);
}

IN_PROC_BROWSER_TEST_F(SegmentationPlatformTest, RunCachedModelsOnly) {
  WaitForPlatformInit();
  WaitForClientResultPrefUpdate();

  // Feature processing isn't called for ondemand models.
  // Note: There is no definite way to check if on-demand models do not get
  // executed. So we wait until the a default model runs and make sure the
  // on-demand model is not executed.
  histogram_tester().ExpectUniqueSample(
      kFeatureProcessingHistogram + SegmentIdToHistogramVariant(kSegmentId3),
      stats::FeatureProcessingError::kSuccess, 1);
  histogram_tester().ExpectUniqueSample(
      kFeatureProcessingHistogram + SegmentIdToHistogramVariant(kSegmentId2),
      stats::FeatureProcessingError::kSuccess, 0);
}

IN_PROC_BROWSER_TEST_F(SegmentationPlatformTest,
                       ReceiveModelUpdateFromOptimizationGuide) {
  WaitForPlatformInit();

  auto user_actions_tracked_before_model = histogram_tester().GetTotalSum(
      "SegmentationPlatform.Signals.ListeningCount.UserAction");
  auto value_histograms_tracked_before_model = histogram_tester().GetTotalSum(
      "SegmentationPlatform.Signals.ListeningCount.HistogramValue");

  base::HistogramTester histogram_tester_1;
  // Create a model metadata with 5 signals, 3 user actions and 2 histograms.
  proto::SegmentationModelMetadata search_user_metadata =
      GetSegmentationModelMetadataWithSignals();
  OptimizationGuideKeyedServiceFactory::GetForProfile(
      chrome_test_utils::GetProfile(this))
      ->OverrideTargetModelForTesting(
          optimization_guide::proto::
              OPTIMIZATION_TARGET_SEGMENTATION_SEARCH_USER,
          CreateOptimizationGuideModelInfo(search_user_metadata));

  // Wait for model update to be written to disk.
  WaitForSegmentInfoDatabaseUpdate(
      proto::OPTIMIZATION_TARGET_SEGMENTATION_SEARCH_USER, histogram_tester_1);

  // Get the number of signals tracked after receiving the new model. Updating
  // signals happens synchronously, so there's no need to wait for these
  // histograms.
  auto user_actions_tracked_after_model = histogram_tester_1.GetTotalSum(
      "SegmentationPlatform.Signals.ListeningCount.UserAction");
  auto value_histograms_tracked_after_model = histogram_tester_1.GetTotalSum(
      "SegmentationPlatform.Signals.ListeningCount.HistogramValue");

  EXPECT_EQ(
      user_actions_tracked_after_model - user_actions_tracked_before_model, 3);
  EXPECT_EQ(value_histograms_tracked_after_model -
                value_histograms_tracked_before_model,
            2);

  // OptimizationGuideSegmentationModelHandler should have recorded that it
  // received a model with valid SegmentationModelMetadata.
  histogram_tester_1.ExpectUniqueSample(
      "SegmentationPlatform.ModelDelivery.HasMetadata." +
          SegmentIdToHistogramVariant(
              proto::OPTIMIZATION_TARGET_SEGMENTATION_SEARCH_USER),
      1, 1);
  // OptimizationGuideSegmentationModelHandler should have recorded that it
  // received a model with valid metadata.
  histogram_tester_1.ExpectUniqueSample(
      "SegmentationPlatform.ModelAvailability." +
          SegmentIdToHistogramVariant(
              proto::OPTIMIZATION_TARGET_SEGMENTATION_SEARCH_USER),
      stats::SegmentationModelAvailability::kModelAvailable, 1);
  // ModelManagerImpl should have recorded that it received an updated model.
  histogram_tester_1.ExpectUniqueSample(
      "SegmentationPlatform.ModelDelivery.Received",
      proto::OPTIMIZATION_TARGET_SEGMENTATION_SEARCH_USER, 1);
  // ModelManagerImpl should have stored the SegmentInfo.
  histogram_tester_1.ExpectBucketCount(
      "SegmentationPlatform.ModelDelivery.SaveResult." +
          SegmentIdToHistogramVariant(
              proto::OPTIMIZATION_TARGET_SEGMENTATION_SEARCH_USER),
      1, 1);
}

IN_PROC_BROWSER_TEST_F(SegmentationPlatformTest,
                       ReceiveNullModelUpdateFromOptimizationGuide) {
  WaitForPlatformInit();

  base::HistogramTester histogram_tester_1;
  // Create a model metadata with 5 signals, 3 user actions and 2 histograms.
  proto::SegmentationModelMetadata search_user_metadata =
      GetSegmentationModelMetadataWithSignals();
  // Send a model update event from Optimization Guide to segmentation platform.
  OptimizationGuideKeyedServiceFactory::GetForProfile(
      chrome_test_utils::GetProfile(this))
      ->OverrideTargetModelForTesting(
          optimization_guide::proto::
              OPTIMIZATION_TARGET_SEGMENTATION_SEARCH_USER,
          CreateOptimizationGuideModelInfo(search_user_metadata));
  // Count how many user actions and histograms are tracked with this new model,
  // updating signals happens synchronously, so there's no need to wait for
  // these histograms.
  auto user_actions_tracked_before_model_deletion =
      histogram_tester_1.GetTotalSum(
          "SegmentationPlatform.Signals.ListeningCount.UserAction");
  auto value_histograms_tracked_before_model_deletion =
      histogram_tester_1.GetTotalSum(
          "SegmentationPlatform.Signals.ListeningCount.HistogramValue");

  // Wait for model update to be written to disk.
  WaitForSegmentInfoDatabaseUpdate(
      proto::OPTIMIZATION_TARGET_SEGMENTATION_SEARCH_USER, histogram_tester_1);

  // Create a new HistogramTester to only count histograms recorded after
  // removing the model.
  base::HistogramTester histogram_tester_2;
  // Send another model update, this time indicating the model is no longer
  // being served.
  OptimizationGuideKeyedServiceFactory::GetForProfile(
      chrome_test_utils::GetProfile(this))
      ->OverrideTargetModelForTesting(
          optimization_guide::proto::
              OPTIMIZATION_TARGET_SEGMENTATION_SEARCH_USER,
          nullptr);
  // Count how many user actions and histgrams are tracked after removing this
  // model. Updating signals happens synchronously, so there's no need to wait
  // for these histograms.
  auto user_actions_tracked_after_model_deletion =
      histogram_tester_2.GetTotalSum(
          "SegmentationPlatform.Signals.ListeningCount.UserAction");
  auto value_histograms_tracked_after_model_deletion =
      histogram_tester_2.GetTotalSum(
          "SegmentationPlatform.Signals.ListeningCount.HistogramValue");

  // Wait for model to be removed to disk.
  WaitForSegmentInfoDatabaseUpdate(
      proto::OPTIMIZATION_TARGET_SEGMENTATION_SEARCH_USER, histogram_tester_2);

  // OptimizationGuideSegmentationModelHandler should not record the HasMetadata
  // histogram, as it only applies to the SegmentationModelMetadata inside
  // ModelInfo.
  histogram_tester_2.ExpectUniqueSample(
      "SegmentationPlatform.ModelDelivery.HasMetadata." +
          SegmentIdToHistogramVariant(
              proto::OPTIMIZATION_TARGET_SEGMENTATION_SEARCH_USER),
      1, 0);
  // OptimizationGuideSegmentationModelHandler should have recorded that the
  // optimization target has no model available.
  histogram_tester_2.ExpectUniqueSample(
      "SegmentationPlatform.ModelAvailability." +
          SegmentIdToHistogramVariant(
              proto::OPTIMIZATION_TARGET_SEGMENTATION_SEARCH_USER),
      stats::SegmentationModelAvailability::kNoModelAvailable, 1);

  // ModelManagerImpl should have recorded that it received an updated model.
  histogram_tester_2.ExpectUniqueSample(
      "SegmentationPlatform.ModelDelivery.Received",
      proto::OPTIMIZATION_TARGET_SEGMENTATION_SEARCH_USER, 1);
  // ModelManagerImpl should have deleted the previous SegmentInfo.
  histogram_tester_2.ExpectUniqueSample(
      "SegmentationPlatform.ModelDelivery.DeleteResult." +
          SegmentIdToHistogramVariant(
              proto::OPTIMIZATION_TARGET_SEGMENTATION_SEARCH_USER),
      1, 1);

  // SignalFilterProcessor should be tracking 3 fewer user actions after
  // removing this model.
  EXPECT_EQ(user_actions_tracked_before_model_deletion -
                user_actions_tracked_after_model_deletion,
            3);
  // SignalFilterProcessor should be tracking 2 fewer value histograms after
  // removing this model.
  EXPECT_EQ(value_histograms_tracked_before_model_deletion -
                value_histograms_tracked_after_model_deletion,
            2);

  // DatabaseMaintenanceImpl should have started a cleanup process, wait for it
  // to complete.
  WaitForHistogram("SegmentationPlatform.Maintenance.CleanupSignalSuccessCount",
                   histogram_tester_2);
  // DatabaseMaintenanceImpl should have cleaned 5 signals from the database.
  histogram_tester_2.ExpectUniqueSample(
      "SegmentationPlatform.Maintenance.CleanupSignalSuccessCount", 5, 1);
}

IN_PROC_BROWSER_TEST_F(SegmentationPlatformTest,
                       NullModelUpdateForUnknownModelShouldBeNoOp) {
  WaitForPlatformInit();

  // Create a new HistogramTester to only count histograms recorded after
  // removing the model.
  base::HistogramTester histogram_tester_2;
  // Send a model update for an optimization target that wasn't registered.
  OptimizationGuideKeyedServiceFactory::GetForProfile(
      chrome_test_utils::GetProfile(this))
      ->OverrideTargetModelForTesting(
          optimization_guide::proto::
              OPTIMIZATION_TARGET_SEGMENTATION_ADAPTIVE_TOOLBAR,
          nullptr);

  histogram_tester_2.ExpectUniqueSample(
      "SegmentationPlatform.ModelDelivery.HasMetadata." +
          SegmentIdToHistogramVariant(
              proto::OPTIMIZATION_TARGET_SEGMENTATION_ADAPTIVE_TOOLBAR),
      1, 0);
}

class SegmentationPlatformUkmModelTest : public SegmentationPlatformTest {
 public:
  SegmentationPlatformUkmModelTest()
      : utils_(&ukm_recorder_, /*owned_db_client=*/false) {}

  void CreatedBrowserMainParts(content::BrowserMainParts* parts) override {
    PlatformBrowserTest::CreatedBrowserMainParts(parts);
    utils_.PreProfileInit(
        {{kSegmentId1, utils_.GetSamplePageLoadMetadata(kSqlFeatureQuery)}});
    MockDefaultModelProvider* provider = utils_.GetDefaultOverride(kSegmentId1);
    EXPECT_CALL(*provider, ExecuteModelWithInput(_, _))
        .WillRepeatedly(Invoke([&](const ModelProvider::Request& inputs,
                                   ModelProvider::ExecutionCallback callback) {
          input_feature_in_last_execution_ = inputs;
          std::move(callback).Run(ModelProvider::Response(1, 0.5));
        }));
  }

  void PreRunTestOnMainThread() override {
    SegmentationPlatformTest::PreRunTestOnMainThread();
    utils_.set_history_service(HistoryServiceFactory::GetForProfile(
        chrome_test_utils::GetProfile(this),
        ServiceAccessType::IMPLICIT_ACCESS));
  }

 protected:
  ukm::TestUkmRecorder ukm_recorder_;
  UkmDataManagerTestUtils utils_;
  std::optional<ModelProvider::Request> input_feature_in_last_execution_;
};

// This test is disabled in CrOS because CrOS creates a signin profile that uses
// incognito mode. This disables the segmentation platform data collection.
// TODO(ssid): Fix this test for CrOS by waiting for signin profile to be
// deleted at startup before adding metrics.
// https://crbug.com/1467530 -- Flaky on Mac
// https://crbug.com/1257820 -- Tests using "PRE_" don't work on Android.
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
#define MAYBE_PRE_RunUkmBasedModel DISABLED_PRE_RunUkmBasedModel
#define MAYBE_RunUkmBasedModel DISABLED_RunUkmBasedModel
#else
#define MAYBE_PRE_RunUkmBasedModel PRE_RunUkmBasedModel
#define MAYBE_RunUkmBasedModel RunUkmBasedModel
#endif

IN_PROC_BROWSER_TEST_F(SegmentationPlatformUkmModelTest,
                       MAYBE_PRE_RunUkmBasedModel) {
  const GURL kUrl1("https://www.url1.com");

  WaitForPlatformInit();

  utils_.WaitForUkmObserverRegistration();

  // Wait for the default model to run and save results to prefs.
  WaitForClientResultPrefUpdate();

  // Record page load UKM that should be recorded in the database, persisted
  // across sessions.
  utils_.RecordPageLoadUkm(kUrl1, base::Time::Now());
  while (!utils_.IsUrlInDatabase(kUrl1)) {
    base::RunLoop().RunUntilIdle();
  }
  UkmDatabaseClientHolder::GetClientInstance(
      chrome_test_utils::GetProfile(this))
      .GetUkmDataManager()
      ->GetUkmDatabase()
      ->CommitTransactionForTesting();
  // There are no UKM metrics written to the database, count = 0.
  EXPECT_EQ(ModelProvider::Request({0}), input_feature_in_last_execution_);
}

IN_PROC_BROWSER_TEST_F(SegmentationPlatformUkmModelTest,
                       MAYBE_RunUkmBasedModel) {
  const GURL kUrl1("https://www.url1.com");

  WaitForPlatformInit();

  // Verify that the URL recorded in last session is still in database.
  EXPECT_TRUE(utils_.IsUrlInDatabase(kUrl1));

  // Result is available from previous session's selection.
  ExpectClassificationResult(
      kChromeLowUserEngagementSegmentationKey,
      /*expected_prediction_status=*/PredictionStatus::kSucceeded);

  utils_.WaitForUkmObserverRegistration();
  WaitForClientResultPrefUpdate();

  // There are 2 UKM metrics written to the database, count = 2.
  EXPECT_EQ(ModelProvider::Request({2}), input_feature_in_last_execution_);
}

IN_PROC_BROWSER_TEST_F(SegmentationPlatformUkmModelTest, DatabaseApi) {
  WaitForPlatformInit();

  ExpectDatabaseQuery({}, {});
  ExpectDatabaseQuery({"test1"}, {0});

  DatabaseClient::StructuredEvent e1("TestEvent", {{"test1", 1}, {"test2", 2}});

  DatabaseClient::StructuredEvent e2("TestEvent",
                                     {{"test1", 10}, {"test2", 20}});

  SegmentationPlatformService* service = GetService();
  DatabaseClient* client = service->GetDatabaseClient();
  client->AddEvent(e1);
  client->AddEvent(e2);
  ExpectDatabaseQuery({}, {});
  ExpectDatabaseQuery({"test1"}, {11});
  ExpectDatabaseQuery({"test1", "test2"}, {11, 22});
}

IN_PROC_BROWSER_TEST_F(SegmentationPlatformUkmModelTest, SumGroupDatabaseApi) {
  WaitForPlatformInit();

  constexpr char kSampleEventName[] = "TestEvent";
  constexpr char kSampleTestMetric1[] = "test1";
  constexpr char kSampleTestMetric2[] = "test2";
  SegmentationPlatformService* service = GetService();
  DatabaseClient* client = service->GetDatabaseClient();
  client->AddEvent(
      {kSampleEventName, {{kSampleTestMetric1, 1}, {kSampleTestMetric2, 2}}});
  client->AddEvent(
      {kSampleEventName, {{kSampleTestMetric1, 10}, {kSampleTestMetric2, 20}}});

  constexpr char kSampleTestMetric0[] = "test0";
  proto::SegmentationModelMetadata metadata;
  MetadataWriter writer(&metadata);
  writer.SetDefaultSegmentationMetadataConfig();
  DatabaseApiClients::AddSumGroupQuery(
      writer, kSampleEventName,
      {kSampleTestMetric0, kSampleTestMetric1, kSampleTestMetric2},
      /*days=*/1);
  RunProcessFeaturesAndCallback(
      metadata, base::BindOnce([](DatabaseClient::ResultStatus status,
                                  const ModelProvider::Request& result) {
        EXPECT_EQ(status, DatabaseClient::ResultStatus::kSuccess);
        const std::vector<float> kExpectedResults = {0, 11, 22};
        EXPECT_EQ(result, kExpectedResults);
      }));
}

class SegmentationPlatformUkmDisabledTest : public SegmentationPlatformTest {
 public:
  SegmentationPlatformUkmDisabledTest()
      : SegmentationPlatformTest(/*setup_feature_list=*/false) {
    feature_list_.InitWithFeaturesAndParameters(
        {base::test::FeatureRefAndParams(features::kSegmentationPlatformFeature,
                                         {}),
         base::test::FeatureRefAndParams(
             kSegmentationPlatformOptimizationTargetSegmentationDummy, {})},
        /*disabled_features=*/{
            features::kSegmentationPlatformUkmEngine,
            features::kSegmentationPlatformUmaFromSqlDb,
        });
  }
};

IN_PROC_BROWSER_TEST_F(SegmentationPlatformUkmDisabledTest, DatabaseApi) {
  WaitForPlatformInit();

  SegmentationPlatformService* service = GetService();
  DatabaseClient* client = service->GetDatabaseClient();
  EXPECT_FALSE(client);
}

}  // namespace segmentation_platform
