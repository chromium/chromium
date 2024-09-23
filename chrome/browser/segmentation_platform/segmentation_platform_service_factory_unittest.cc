// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"

#include <memory>
#include <string_view>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/segmentation_platform/ukm_data_manager_test_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "components/commerce/core/mock_shopping_service.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_observer.h"
#include "components/prefs/pref_service.h"
#include "components/segmentation_platform/embedder/default_model/contextual_page_actions_model.h"
#include "components/segmentation_platform/embedder/default_model/metrics_clustering.h"
#include "components/segmentation_platform/embedder/default_model/most_visited_tiles_user.h"
#include "components/segmentation_platform/embedder/home_modules/ephemeral_home_module_backend.h"
#include "components/segmentation_platform/internal/constants.h"
#include "components/segmentation_platform/internal/database/client_result_prefs.h"
#include "components/segmentation_platform/internal/segmentation_ukm_helper.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/prediction_options.h"
#include "components/segmentation_platform/public/result.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"
#include "components/segmentation_platform/public/service_proxy.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/visited_url_ranking/public/test_support.h"
#include "components/visited_url_ranking/public/url_visit_schema.h"
#include "components/visited_url_ranking/public/url_visit_util.h"
#include "content/public/test/browser_task_environment.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace segmentation_platform {
namespace {

using Segmentation_ModelExecutionUkmRecorder =
    ::ukm::builders::Segmentation_ModelExecution;

// Observer that waits for service_ initialization.
class WaitServiceInitializedObserver : public ServiceProxy::Observer {
 public:
  explicit WaitServiceInitializedObserver(base::OnceClosure closure)
      : closure_(std::move(closure)) {}
  void OnServiceStatusChanged(bool initialized, int status_flags) override {
    if (initialized) {
      std::move(closure_).Run();
    }
  }

 private:
  base::OnceClosure closure_;
};

class SegmentationPlatformServiceFactoryTest : public testing::Test {
 protected:
  SegmentationPlatformServiceFactoryTest()
      : task_environment_{base::test::TaskEnvironment::TimeSource::MOCK_TIME},
        test_utils_(std::make_unique<UkmDataManagerTestUtils>(&ukm_recorder_)) {
    scoped_command_line_.GetProcessCommandLine()->AppendSwitch(
        kSegmentationPlatformRefreshResultsSwitch);
    scoped_command_line_.GetProcessCommandLine()->AppendSwitch(
        kSegmentationPlatformDisableModelExecutionDelaySwitch);
  }

  ~SegmentationPlatformServiceFactoryTest() override = default;

  void SetUp() override { test_utils_->PreProfileInit({}); }

  void TearDown() override {
    task_environment_.RunUntilIdle();
    profile_.reset();
    test_utils_.reset();
  }

  void InitServiceAndCacheResults(const std::string& segmentation_key) {
    InitService();
    WaitForClientResultPrefUpdate(segmentation_key);
    // Getting the updated prefs from this session to be copied to the next
    // session. In the test environment, new session doesn't have prefs from
    // previous session, hence copying is required to get the cached result from
    // last session.
    const std::string output = profile_->profile->GetPrefs()->GetString(
        kSegmentationClientResultPrefs);

    // TODO(b/297091996): Remove this when leak is fixed.
    task_environment_.RunUntilIdle();

    profile_.reset();

    // Creating profile and initialising segmentation service again with prefs
    // from the last session.
    profile_ = std::make_unique<ProfileData>(test_utils_.get(), output);
    // Copying the prefs from last session.
    WaitForServiceInit();
    // TODO(b/297091996): Remove this when leak is fixed.
    task_environment_.RunUntilIdle();
  }

  void InitService() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{optimization_guide::features::kOptimizationTargetPrediction, {}},
         {features::kSegmentationPlatformFeature, {}},
         {features::kSegmentationPlatformUkmEngine, {}},
         {features::kContextualPageActionShareModel, {}},
         {features::kSegmentationPlatformTimeDelaySampling,
          {{"SamplingRate", "1"}}},
         {features::kSegmentationPlatformTabResumptionRanker, {}},
         {features::kSegmentationPlatformAndroidHomeModuleRanker, {}},
         {features::kSegmentationPlatformURLVisitResumptionRanker, {}},
         {features::kSegmentationPlatformEphemeralCardRanker, {}},
         {features::kSegmentationSurveyPage, {}}},
        {});

    // Creating profile and initialising segmentation service.
    profile_ = std::make_unique<ProfileData>(test_utils_.get(), "");
    WaitForServiceInit();
    clock_.SetNow(base::Time::Now());
    // TODO(b/297091996): Remove this when leak is fixed.
    task_environment_.RunUntilIdle();
  }

  void ExpectGetClassificationResult(
      const std::string& segmentation_key,
      const PredictionOptions& prediction_options,
      scoped_refptr<InputContext> input_context,
      PredictionStatus expected_status,
      std::optional<std::vector<std::string>> expected_labels) {
    base::RunLoop loop;
    profile_->service->GetClassificationResult(
        segmentation_key, prediction_options, input_context,
        base::BindOnce(
            &SegmentationPlatformServiceFactoryTest::OnGetClassificationResult,
            base::Unretained(this), loop.QuitClosure(), expected_status,
            expected_labels));
    loop.Run();
  }

  void OnGetClassificationResult(
      base::RepeatingClosure closure,
      PredictionStatus expected_status,
      std::optional<std::vector<std::string>> expected_labels,
      const ClassificationResult& actual_result) {
    EXPECT_EQ(static_cast<int>(actual_result.status),
              static_cast<int>(expected_status));
    if (expected_labels.has_value()) {
      EXPECT_EQ(actual_result.ordered_labels, expected_labels.value());
    }
    std::move(closure).Run();
  }

  void ExpectGetAnnotatedNumericResult(
      const std::string& segmentation_key,
      const PredictionOptions& prediction_options,
      scoped_refptr<InputContext> input_context,
      PredictionStatus expected_status) {
    base::RunLoop loop;
    profile_->service->GetAnnotatedNumericResult(
        segmentation_key, prediction_options, input_context,
        base::BindOnce(&SegmentationPlatformServiceFactoryTest::
                           OnGetAnnotatedNumericResult,
                       base::Unretained(this), loop.QuitClosure(),
                       expected_status));
    loop.Run();
  }

  void OnGetAnnotatedNumericResult(
      base::RepeatingClosure closure,
      PredictionStatus expected_status,
      const AnnotatedNumericResult& actual_result) {
    ASSERT_EQ(expected_status, actual_result.status);
    std::move(closure).Run();
  }

  void WaitForServiceInit() {
    base::RunLoop wait_for_init;
    WaitServiceInitializedObserver wait_observer(wait_for_init.QuitClosure());
    profile_->service->GetServiceProxy()->AddObserver(&wait_observer);

    wait_for_init.Run();
    ASSERT_TRUE(profile_->service->IsPlatformInitialized());
    profile_->service->GetServiceProxy()->RemoveObserver(&wait_observer);
  }

  bool HasClientResultPref(const std::string& segmentation_key) {
    PrefService* pref_service_ = profile_->profile->GetPrefs();
    std::unique_ptr<ClientResultPrefs> result_prefs_ =
        std::make_unique<ClientResultPrefs>(pref_service_);
    return result_prefs_->ReadClientResultFromPrefs(segmentation_key) !=
           nullptr;
  }

  void OnClientResultPrefUpdated(const std::string& segmentation_key) {
    if (!wait_for_pref_callback_.is_null() &&
        HasClientResultPref(segmentation_key)) {
      std::move(wait_for_pref_callback_).Run();
    }
  }

  void WaitForClientResultPrefUpdate(const std::string& segmentation_key) {
    if (HasClientResultPref(segmentation_key)) {
      return;
    }

    base::RunLoop wait_for_pref;
    wait_for_pref_callback_ = wait_for_pref.QuitClosure();
    pref_registrar_.Init(profile_->profile->GetPrefs());
    pref_registrar_.Add(
        kSegmentationClientResultPrefs,
        base::BindRepeating(
            &SegmentationPlatformServiceFactoryTest::OnClientResultPrefUpdated,
            base::Unretained(this), segmentation_key));
    wait_for_pref.Run();

    pref_registrar_.RemoveAll();
  }

  void ExpectUkm(std::vector<std::string_view> metric_names,
                 std::vector<int64_t> expected_values) {
    const auto& entries = test_recorder_.GetEntriesByName(
        Segmentation_ModelExecutionUkmRecorder::kEntryName);
    ASSERT_EQ(1u, entries.size());
    for (size_t i = 0; i < metric_names.size(); ++i) {
      test_recorder_.ExpectEntryMetric(entries[0], metric_names[i],
                                       expected_values[i]);
    }
  }
  void ExpectUkmCount(size_t count) {
    const auto& entries = test_recorder_.GetEntriesByName(
        Segmentation_ModelExecutionUkmRecorder::kEntryName);
    ASSERT_EQ(count, entries.size());
  }

  void WaitForUkmRecord(proto::SegmentId segment_id) {
    base::RunLoop run_loop;
    test_recorder()->SetOnAddEntryCallback(
        Segmentation_ModelExecutionUkmRecorder::kEntryName,
        base::BindRepeating(
            [](proto::SegmentId id, ukm::TestAutoSetUkmRecorder* test_recorder,
               base::OnceClosure loop) {
              const auto& entries = test_recorder->GetEntriesByName(
                  Segmentation_ModelExecutionUkmRecorder::kEntryName);
              if (entries.size() == 1u) {
                const int64_t* metric = test_recorder->GetEntryMetric(
                    entries[0], Segmentation_ModelExecutionUkmRecorder::
                                    kOptimizationTargetName);
                if (metric && *metric == id) {
                  std::move(loop).Run();
                }
              }
            },
            segment_id, base::Unretained(test_recorder()),
            run_loop.QuitClosure()));
    run_loop.Run();
  }

  // This only checks for training data ukm records. Model execution UKM records
  // are not collected for default model.
  void WaitAndCheckUkmRecord(proto::SegmentId segment_id,
                             std::vector<int64_t> inputs,
                             std::vector<int64_t> outputs) {
    WaitForUkmRecord(segment_id);
    ExpectUkm({Segmentation_ModelExecutionUkmRecorder::kOptimizationTargetName},
              {segment_id});

    // Check for inputs in the model.
    // Append more if required.
    std::vector<std::string_view> inputs_ukm_metrics = {
        Segmentation_ModelExecutionUkmRecorder::kInput0Name,
        Segmentation_ModelExecutionUkmRecorder::kInput1Name,
        Segmentation_ModelExecutionUkmRecorder::kInput2Name,
        Segmentation_ModelExecutionUkmRecorder::kInput3Name,
        Segmentation_ModelExecutionUkmRecorder::kInput4Name,
        Segmentation_ModelExecutionUkmRecorder::kInput5Name,
        Segmentation_ModelExecutionUkmRecorder::kInput6Name,
        Segmentation_ModelExecutionUkmRecorder::kInput7Name,
        Segmentation_ModelExecutionUkmRecorder::kInput8Name,
        Segmentation_ModelExecutionUkmRecorder::kInput9Name,
        Segmentation_ModelExecutionUkmRecorder::kInput10Name,
    };
    if (inputs.size() > 0) {
      std::vector<std::string_view> input_metric_name(
          inputs_ukm_metrics.begin(),
          inputs_ukm_metrics.begin() + inputs.size());
      ExpectUkm({input_metric_name}, {inputs});
    }

    // Check for output in the model.
    // Append more if required.
    std::vector<std::string_view> outputs_ukm_metrics = {
        Segmentation_ModelExecutionUkmRecorder::kActualResultName,
        Segmentation_ModelExecutionUkmRecorder::kActualResult2Name,
        Segmentation_ModelExecutionUkmRecorder::kActualResult3Name,
        Segmentation_ModelExecutionUkmRecorder::kActualResult4Name,
        Segmentation_ModelExecutionUkmRecorder::kActualResult5Name,
        Segmentation_ModelExecutionUkmRecorder::kActualResult6Name};
    if (outputs.size() > 0) {
      std::vector<std::string_view> output_metric_name(
          outputs_ukm_metrics.begin(),
          outputs_ukm_metrics.begin() + outputs.size());
      ExpectUkm({output_metric_name}, {outputs});
    }
  }

  struct ProfileData {
    explicit ProfileData(UkmDataManagerTestUtils* test_utils,
                         const std::string& result_pref)
        : test_utils(test_utils) {
      TestingProfile::Builder profile_builder;
      profile_builder.AddTestingFactory(
          commerce::ShoppingServiceFactory::GetInstance(),
          base::BindRepeating([](content::BrowserContext* context) {
            return commerce::MockShoppingService::Build();
          }));
      profile = profile_builder.Build();

      profile->GetPrefs()->SetString(kSegmentationClientResultPrefs,
                                     result_pref);
      test_utils->SetupForProfile(profile.get());
      service =
          SegmentationPlatformServiceFactory::GetForProfile(profile.get());
    }

    ~ProfileData() { test_utils->WillDestroyProfile(profile.get()); }

    ProfileData(ProfileData&) = delete;

    const raw_ptr<UkmDataManagerTestUtils> test_utils;
    std::unique_ptr<TestingProfile> profile;
    raw_ptr<SegmentationPlatformService> service;
  };

  base::SimpleTestClock* clock() { return &clock_; }

  ukm::TestAutoSetUkmRecorder* test_recorder() { return &test_recorder_; }

  base::SimpleTestClock clock_;
  content::BrowserTaskEnvironment task_environment_;
  ukm::TestAutoSetUkmRecorder test_recorder_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::ScopedCommandLine scoped_command_line_;
  ukm::TestUkmRecorder ukm_recorder_;
  std::unique_ptr<UkmDataManagerTestUtils> test_utils_;
  PrefChangeRegistrar pref_registrar_;
  base::OnceClosure wait_for_pref_callback_;
  std::unique_ptr<ProfileData> profile_;
};

TEST_F(SegmentationPlatformServiceFactoryTest, TestPasswordManagerUserSegment) {
  InitServiceAndCacheResults(kPasswordManagerUserKey);

  PredictionOptions prediction_options;

  ExpectGetClassificationResult(
      kPasswordManagerUserKey, prediction_options, nullptr,
      /*expected_status=*/PredictionStatus::kSucceeded,
      /*expected_labels=*/
      std::vector<std::string>(1, "Not_PasswordManagerUser"));
}

// Segmentation Ukm Engine is disabled on CrOS.
#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(SegmentationPlatformServiceFactoryTest, TestSearchUserModel) {
  InitServiceAndCacheResults(kSearchUserKey);

  PredictionOptions prediction_options;

  ExpectGetClassificationResult(
      kSearchUserKey, prediction_options, nullptr,
      /*expected_status=*/PredictionStatus::kSucceeded,
      /*expected_labels=*/
      std::vector<std::string>(1, kSearchUserModelLabelNone));
}
#endif  //! BUILDFLAG(IS_CHROMEOS)

TEST_F(SegmentationPlatformServiceFactoryTest, TestShoppingUserModel) {
  InitServiceAndCacheResults(kShoppingUserSegmentationKey);

  PredictionOptions prediction_options;

  ExpectGetClassificationResult(
      kShoppingUserSegmentationKey, prediction_options, nullptr,
      /*expected_status=*/PredictionStatus::kSucceeded,
      /*expected_labels=*/
      std::vector<std::string>(1, kLegacyNegativeLabel));
}

TEST_F(SegmentationPlatformServiceFactoryTest, TestResumeHeavyUserModel) {
  InitServiceAndCacheResults(kResumeHeavyUserKey);

  PredictionOptions prediction_options;

  ExpectGetClassificationResult(
      kResumeHeavyUserKey, prediction_options, nullptr,
      /*expected_status=*/PredictionStatus::kSucceeded,
      /*expected_labels=*/
      std::vector<std::string>(1, kLegacyNegativeLabel));
}

TEST_F(SegmentationPlatformServiceFactoryTest, TestLowUserEngagementModel) {
  InitServiceAndCacheResults(kChromeLowUserEngagementSegmentationKey);

  PredictionOptions prediction_options;

  ExpectGetClassificationResult(
      kChromeLowUserEngagementSegmentationKey, prediction_options, nullptr,
      /*expected_status=*/PredictionStatus::kSucceeded,
      /*expected_labels=*/
      std::vector<std::string>(1, kChromeLowUserEngagementUmaName));
}

TEST_F(SegmentationPlatformServiceFactoryTest, TestCrossDeviceModel) {
  InitServiceAndCacheResults(segmentation_platform::kCrossDeviceUserKey);
  segmentation_platform::PredictionOptions prediction_options;

  ExpectGetClassificationResult(
      segmentation_platform::kCrossDeviceUserKey, prediction_options, nullptr,
      /*expected_status=*/segmentation_platform::PredictionStatus::kSucceeded,
      /*expected_labels=*/
      std::vector<std::string>(1, segmentation_platform::kNoCrossDeviceUsage));
}

TEST_F(SegmentationPlatformServiceFactoryTest, TestDeviceSwitcherModel) {
  InitService();

  PredictionOptions prediction_options;
  prediction_options.on_demand_execution = true;

  auto input_context = base::MakeRefCounted<InputContext>();
  input_context->metadata_args.emplace("wait_for_device_info_in_seconds",
                                       processing::ProcessedValue(0));

  ExpectGetClassificationResult(
      kDeviceSwitcherKey, prediction_options, input_context,
      /*expected_status=*/PredictionStatus::kSucceeded,
      /*expected_labels=*/std::vector<std::string>(1, "NotSynced"));
}

TEST_F(SegmentationPlatformServiceFactoryTest, URLVisitResumptionRanker) {
  InitService();

  visited_url_ranking::URLVisitAggregate visit_aggregate =
      visited_url_ranking::CreateSampleURLVisitAggregate(
          GURL("https://google.com/search?q=sample"));
  scoped_refptr<InputContext> input_context =
      visited_url_ranking::AsInputContext(
          visited_url_ranking::kURLVisitAggregateSchema, visit_aggregate);

  PredictionOptions prediction_options;
  prediction_options.on_demand_execution = true;
  ExpectGetAnnotatedNumericResult(kURLVisitResumptionRankerKey,
                                  prediction_options, input_context,
                                  PredictionStatus::kSucceeded);
}

// Segmentation Ukm Engine is disabled on CrOS.
#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(SegmentationPlatformServiceFactoryTest, TabResupmtionRanker) {
  InitService();

  PredictionOptions prediction_options;
  prediction_options.on_demand_execution = true;
  scoped_refptr<InputContext> input_context =
      base::MakeRefCounted<InputContext>();
  input_context->metadata_args.emplace(
      "session_tag", processing::ProcessedValue(std::string("")));
  input_context->metadata_args.emplace("tab_id", processing::ProcessedValue(1));
  input_context->metadata_args.emplace(
      "origin", processing::ProcessedValue(GURL("https://www.google.com")));
  ExpectGetAnnotatedNumericResult(kTabResumptionClassifierKey,
                                  prediction_options, input_context,
                                  PredictionStatus::kSucceeded);
}
#endif  //! BUILDFLAG(IS_CHROMEOS)

TEST_F(SegmentationPlatformServiceFactoryTest, MetricsClustering) {
  InitServiceAndCacheResults(
      segmentation_platform::MetricsClustering::kMetricsClusteringKey);

  segmentation_platform::PredictionOptions prediction_options =
      PredictionOptions::ForCached();

  ExpectGetAnnotatedNumericResult(
      segmentation_platform::MetricsClustering::kMetricsClusteringKey,
      prediction_options, nullptr, PredictionStatus::kSucceeded);
}

#if BUILDFLAG(IS_ANDROID)
// Tests for models in android platform.
TEST_F(SegmentationPlatformServiceFactoryTest, TestDeviceTierSegment) {
  InitServiceAndCacheResults(kDeviceTierKey);

  PredictionOptions prediction_options;

  ExpectGetClassificationResult(
      kDeviceTierKey, prediction_options, nullptr,
      /*expected_status=*/PredictionStatus::kSucceeded,
      /*expected_labels=*/std::nullopt);
}

TEST_F(SegmentationPlatformServiceFactoryTest,
       TestTabletProductivityUserModel) {
  InitServiceAndCacheResults(kTabletProductivityUserKey);

  PredictionOptions prediction_options;

  ExpectGetClassificationResult(
      kTabletProductivityUserKey, prediction_options, nullptr,
      /*expected_status=*/PredictionStatus::kSucceeded,
      /*expected_labels=*/
      std::vector<std::string>(1, kTabletProductivityUserModelLabelNone));
}

TEST_F(SegmentationPlatformServiceFactoryTest, TestContextualPageActionsShare) {
  InitService();

  PredictionOptions prediction_options;
  prediction_options.on_demand_execution = true;

  auto input_context = base::MakeRefCounted<InputContext>();
  input_context->metadata_args.emplace(
      segmentation_platform::kContextualPageActionModelInputDiscounts,
      segmentation_platform::processing::ProcessedValue::FromFloat(1));
  input_context->metadata_args.emplace(
      segmentation_platform::kContextualPageActionModelInputPriceInsights,
      segmentation_platform::processing::ProcessedValue::FromFloat(0));
  input_context->metadata_args.emplace(
      segmentation_platform::kContextualPageActionModelInputPriceTracking,
      segmentation_platform::processing::ProcessedValue::FromFloat(0));
  input_context->metadata_args.emplace(
      segmentation_platform::kContextualPageActionModelInputReaderMode,
      segmentation_platform::processing::ProcessedValue::FromFloat(0));

  ExpectGetClassificationResult(
      kContextualPageActionsKey, prediction_options, input_context,
      /*expected_status=*/PredictionStatus::kSucceeded,
      /*expected_labels=*/
      std::vector<std::string>(1, kContextualPageActionModelLabelDiscounts));
  clock()->Advance(base::Seconds(
      ContextualPageActionsModel::kShareOutputCollectionDelayInSec));

  // TODO(crbug.com/40254472): Clean this up.
  WaitAndCheckUkmRecord(
      proto::OPTIMIZATION_TARGET_CONTEXTUAL_PAGE_ACTION_PRICE_TRACKING,
      /*inputs=*/
      {SegmentationUkmHelper::FloatToInt64(1.f), 0, 0, 0, 0, 0, 0, 0},
      /*outputs=*/{0, 0, 0, 0, 0, 0});
}

TEST_F(SegmentationPlatformServiceFactoryTest, TestFrequentFeatureModel) {
  InitServiceAndCacheResults(kFrequentFeatureUserKey);

  PredictionOptions prediction_options;

  ExpectGetClassificationResult(
      kFrequentFeatureUserKey, prediction_options, nullptr,
      /*expected_status=*/PredictionStatus::kSucceeded,
      /*expected_labels=*/
      std::vector<std::string>{kLegacyNegativeLabel});
}

TEST_F(SegmentationPlatformServiceFactoryTest, TestIntentionalUserModel) {
  InitServiceAndCacheResults(segmentation_platform::kIntentionalUserKey);

  segmentation_platform::PredictionOptions prediction_options;

  ExpectGetClassificationResult(
      segmentation_platform::kIntentionalUserKey, prediction_options, nullptr,
      /*expected_status=*/segmentation_platform::PredictionStatus::kSucceeded,
      /*expected_labels=*/
      std::vector<std::string>(1, kLegacyNegativeLabel));
}

TEST_F(SegmentationPlatformServiceFactoryTest, TestPowerUserSegment) {
  InitServiceAndCacheResults(kPowerUserKey);

  PredictionOptions prediction_options;

  ExpectGetClassificationResult(
      kPowerUserKey, prediction_options, nullptr,
      /*expected_status=*/PredictionStatus::kSucceeded,
      /*expected_labels=*/
      std::vector<std::string>{"None"});
}

TEST_F(SegmentationPlatformServiceFactoryTest, MostVisitedTilesUser) {
  InitServiceAndCacheResults(
      segmentation_platform::MostVisitedTilesUser::kMostVisitedTilesUserKey);

  segmentation_platform::PredictionOptions prediction_options;

  ExpectGetClassificationResult(
      segmentation_platform::MostVisitedTilesUser::kMostVisitedTilesUserKey,
      prediction_options, nullptr,
      /*expected_status=*/segmentation_platform::PredictionStatus::kSucceeded,
      /*expected_labels=*/
      std::vector<std::string>(1, "None"));
}

TEST_F(SegmentationPlatformServiceFactoryTest, TestFeedUserModel) {
  InitServiceAndCacheResults(segmentation_platform::kFeedUserSegmentationKey);
  segmentation_platform::PredictionOptions prediction_options;

  ExpectGetClassificationResult(
      segmentation_platform::kFeedUserSegmentationKey, prediction_options,
      nullptr,
      /*expected_status=*/segmentation_platform::PredictionStatus::kSucceeded,
      /*expected_labels=*/
      std::vector<std::string>(1, kLegacyNegativeLabel));
}

TEST_F(SegmentationPlatformServiceFactoryTest, TestAndroidHomeModuleRanker) {
  InitService();
  PredictionOptions prediction_options;
  prediction_options.on_demand_execution = true;

  auto input_context = base::MakeRefCounted<InputContext>();
  input_context->metadata_args.emplace(
      segmentation_platform::kSingleTabFreshness,
      segmentation_platform::processing::ProcessedValue::FromFloat(-1));
  input_context->metadata_args.emplace(
      segmentation_platform::kPriceChangeFreshness,
      segmentation_platform::processing::ProcessedValue::FromFloat(-1));
  input_context->metadata_args.emplace(
      segmentation_platform::kTabResumptionForAndroidHomeFreshness,
      segmentation_platform::processing::ProcessedValue::FromFloat(-1));
  input_context->metadata_args.emplace(
      segmentation_platform::kSafetyHubFreshness,
      segmentation_platform::processing::ProcessedValue::FromFloat(-1));

  std::vector<std::string> result = {kPriceChange, kSingleTab,
                                     kTabResumptionForAndroidHome, kSafetyHub};
  ExpectGetClassificationResult(
      segmentation_platform::kAndroidHomeModuleRankerKey, prediction_options,
      input_context,
      /*expected_status=*/segmentation_platform::PredictionStatus::kSucceeded,
      /*expected_labels=*/result);
}

#endif  // BUILDFLAG(IS_ANDROID)

TEST_F(SegmentationPlatformServiceFactoryTest, EphemeralHomeMdouleBackend) {
  InitService();

  home_modules::HomeModulesCardRegistry* registry =
      SegmentationPlatformServiceFactory::GetHomeModulesCardRegistry(
          profile_->profile.get());
  ASSERT_TRUE(registry);
  // Update this test when adding new cards with inputs.
  // Each card's feature flag should be enabled by test framework for this
  // integration test.
  EXPECT_TRUE(registry->get_all_cards_by_priority().empty());

  PredictionOptions prediction_options;
  prediction_options.on_demand_execution = true;

  auto input_context = base::MakeRefCounted<InputContext>();

  // No cards are added, the model fetches no results and fails.
  std::vector<std::string> result = {};
  ExpectGetClassificationResult(
      kEphemeralHomeModuleBackendKey, prediction_options, input_context,
      /*expected_status=*/segmentation_platform::PredictionStatus::kSucceeded,
      /*expected_labels=*/result);
}

}  // namespace
}  // namespace segmentation_platform
