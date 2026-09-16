// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tips/core/tips_service_test_base.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/user_metrics.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"
#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/tips/core/tips_prefs.h"
#endif  // BUILDFLAG(IS_ANDROID)
#include "components/segmentation_platform/internal/database/ukm_database.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/service_proxy.h"

namespace tips {

namespace {

class WaitServiceInitializedObserver
    : public segmentation_platform::ServiceProxy::Observer {
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

}  // namespace

TestDatabaseClient::TestDatabaseClient(
    segmentation_platform::SegmentationPlatformService* real_service)
    : real_service_(real_service) {}

TestDatabaseClient::~TestDatabaseClient() = default;

void TestDatabaseClient::SetOverrideSignalValues(
    const std::vector<float>& override_values) {
  override_values_ = override_values;
}

void TestDatabaseClient::SetSimulateError(bool simulate_error) {
  simulate_error_ = simulate_error;
}

void TestDatabaseClient::ProcessFeatures(
    const segmentation_platform::proto::SegmentationModelMetadata& metadata,
    base::Time end_time,
    FeaturesCallback callback) {
  last_metadata_ = metadata;
  segmentation_platform::DatabaseClient* real_client =
      real_service_ ? real_service_->GetDatabaseClient() : nullptr;
  if (!real_client || simulate_error_) {
    std::move(callback).Run(ResultStatus::kError, {});
    return;
  }

  real_client->ProcessFeatures(
      metadata, end_time,
      base::BindOnce(
          [](FeaturesCallback original_callback,
             std::optional<std::vector<float>> override_vals,
             ResultStatus status,
             const segmentation_platform::ModelProvider::Request& inputs) {
            EXPECT_EQ(status, ResultStatus::kSuccess);
            if (override_vals.has_value()) {
              std::move(original_callback)
                  .Run(ResultStatus::kSuccess, *override_vals);
            } else {
              std::move(original_callback).Run(status, inputs);
            }
          },
          std::move(callback), override_values_));
}

void TestDatabaseClient::AddEvent(const StructuredEvent& event) {
  segmentation_platform::DatabaseClient* real_client =
      real_service_ ? real_service_->GetDatabaseClient() : nullptr;
  if (real_client) {
    real_client->AddEvent(event);
  }
}

TestSegmentationPlatformService::TestSegmentationPlatformService(
    segmentation_platform::SegmentationPlatformService* real_service)
    : real_service_(real_service) {
  if (real_service_) {
    test_database_client_ = std::make_unique<TestDatabaseClient>(real_service_);
  }
}

TestSegmentationPlatformService::~TestSegmentationPlatformService() = default;

TestDatabaseClient* TestSegmentationPlatformService::test_database_client() {
  return test_database_client_.get();
}

segmentation_platform::DatabaseClient*
TestSegmentationPlatformService::GetDatabaseClient() {
  return test_database_client_.get();
}

TipsServiceTestBase::TipsServiceTestBase() = default;
TipsServiceTestBase::~TipsServiceTestBase() = default;

void TipsServiceTestBase::SetUp() {
  base::SetRecordActionTaskRunner(task_environment_.GetMainThreadTaskRunner());
  scoped_feature_list_.InitWithFeatures(
      {segmentation_platform::features::kSegmentationPlatformFeature,
       segmentation_platform::features::kSegmentationPlatformUkmEngine,
       segmentation_platform::features::kAndroidTipsNotifications},
      {});
  test_utils_ =
      std::make_unique<segmentation_platform::UkmDataManagerTestUtils>(
          &ukm_recorder_);
  test_utils_->PreProfileInit({});
  profile_ = std::make_unique<TestingProfile>();
  test_utils_->SetupForProfile(profile_.get());
  real_segmentation_service_ =
      segmentation_platform::SegmentationPlatformServiceFactory::GetForProfile(
          profile_.get());
  if (real_segmentation_service_ &&
      !real_segmentation_service_->IsPlatformInitialized()) {
    base::RunLoop wait_for_init;
    WaitServiceInitializedObserver wait_observer(wait_for_init.QuitClosure());
    real_segmentation_service_->GetServiceProxy()->AddObserver(&wait_observer);
    wait_for_init.Run();
    real_segmentation_service_->GetServiceProxy()->RemoveObserver(
        &wait_observer);
  }
  if (real_segmentation_service_) {
    real_segmentation_service_->EnableMetrics(true);
  }
  test_segmentation_service_ =
      std::make_unique<TestSegmentationPlatformService>(
          real_segmentation_service_);
  service_ = std::make_unique<TipsService>(&pref_service_,
                                           test_segmentation_service_.get());
#if BUILDFLAG(IS_ANDROID)
  tips::prefs::RegisterProfilePrefs(pref_service_.registry());
#endif  // BUILDFLAG(IS_ANDROID)
}

void TipsServiceTestBase::TearDown() {
  service_.reset();
  test_segmentation_service_.reset();
  real_segmentation_service_ = nullptr;
  test_utils_->WillDestroyProfile(profile_.get());
  profile_.reset();
  test_utils_.reset();
}

void TipsServiceTestBase::RecordUserAction(const std::string& action_name) {
  base::RecordAction(base::UserMetricsAction(action_name.c_str()));
  {
    base::RunLoop run_loop;
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }
  segmentation_platform::UkmDatabase* ukm_db =
      test_utils_->ukm_database_client()->GetUkmDataManager()->GetUkmDatabase();
  ASSERT_TRUE(ukm_db);
  ukm_db->CommitTransactionForTesting();
  {
    base::RunLoop run_loop;
    ukm_db->RunReadOnlyQueries(
        {}, base::BindOnce(
                [](base::OnceClosure quit,
                   std::optional<
                       segmentation_platform::processing::IndexedTensors>) {
                  std::move(quit).Run();
                },
                run_loop.QuitClosure()));
    run_loop.Run();
  }
}

std::optional<TipsNotificationsFeatureType>
TipsServiceTestBase::DetermineBestTipSync() {
  std::optional<TipsNotificationsFeatureType> actual_best_tip;
  base::RunLoop run_loop;
  service_->DetermineBestTip(base::BindOnce(
      [](std::optional<TipsNotificationsFeatureType>* out,
         base::OnceClosure quit,
         std::optional<TipsNotificationsFeatureType> res) {
        *out = res;
        std::move(quit).Run();
      },
      &actual_best_tip, run_loop.QuitClosure()));
  run_loop.Run();
  return actual_best_tip;
}

void TipsServiceTestBase::RunDetermineBestTipTest(
    std::vector<std::unique_ptr<TipsFeature>> features,
    std::optional<TipsNotificationsFeatureType> expected_best_tip) {
  for (auto& feature : features) {
    service_->RegisterFeature(std::move(feature));
  }

  std::optional<TipsNotificationsFeatureType> actual_best_tip;
  base::RunLoop run_loop;
  service_->DetermineBestTip(base::BindOnce(
      [](std::optional<TipsNotificationsFeatureType>* out,
         base::OnceClosure quit,
         std::optional<TipsNotificationsFeatureType> res) {
        *out = res;
        std::move(quit).Run();
      },
      &actual_best_tip, run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_EQ(actual_best_tip, expected_best_tip);
}

void TipsServiceTestBase::RunDetermineBestTipTestWithOverrides(
    std::vector<FeatureTestConfig> configs,
    std::optional<TipsNotificationsFeatureType> expected_best_tip) {
  std::vector<float> flat_inputs;
  std::vector<std::unique_ptr<TipsFeature>> features;

  for (auto& config : configs) {
    auto signals = config.feature->GetRequiredSignals();
    for (const auto& signal : signals) {
      float value = 0.0f;
      if (config.mock_signal_values.has_value()) {
        auto it = config.mock_signal_values->find(signal.name);
        if (it != config.mock_signal_values->end()) {
          value = it->second;
        }
      }
      flat_inputs.push_back(value);
    }
    features.push_back(std::move(config.feature));
  }

  if (test_segmentation_service_->test_database_client()) {
    test_segmentation_service_->test_database_client()->SetOverrideSignalValues(
        flat_inputs);
  }

  RunDetermineBestTipTest(std::move(features), expected_best_tip);
}

}  // namespace tips
