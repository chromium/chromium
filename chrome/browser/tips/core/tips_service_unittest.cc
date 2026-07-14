// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tips/core/tips_service.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/notifications/scheduler/public/notification_params.h"
#include "chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"
#include "chrome/browser/segmentation_platform/ukm_data_manager_test_utils.h"
#include "chrome/browser/tips/core/tips_feature.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/testing_pref_service.h"
#include "components/segmentation_platform/internal/dummy_segmentation_platform_service.h"
#include "components/segmentation_platform/public/database_client.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"
#include "components/segmentation_platform/public/service_proxy.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;
using ::testing::SaveArg;

namespace tips {

namespace {

class MockTipsFeature : public TipsFeature {
 public:
  MockTipsFeature(TipFeatureRank rank,
                  TipsNotificationsFeatureType type,
                  std::vector<SignalDefinition> signals,
                  bool is_eligible)
      : rank_(rank),
        type_(type),
        signals_(std::move(signals)),
        is_eligible_(is_eligible) {}

  TipFeatureRank GetRank() const override { return rank_; }
  TipsNotificationsFeatureType GetFeatureType() const override { return type_; }
  std::vector<SignalDefinition> GetRequiredSignals() const override {
    return signals_;
  }
  bool IsEligible(const std::map<std::string, float>& signal_values,
                  const PrefService& pref_service) const override {
    last_signal_values_ = signal_values;
    return is_eligible_;
  }
  notifications::NotificationData GetNotificationData() const override {
    return notifications::NotificationData();
  }

  mutable std::map<std::string, float> last_signal_values_;

 private:
  TipFeatureRank rank_;
  TipsNotificationsFeatureType type_;
  std::vector<SignalDefinition> signals_;
  bool is_eligible_;
};

class TestDatabaseClient : public segmentation_platform::DatabaseClient {
 public:
  explicit TestDatabaseClient(
      segmentation_platform::SegmentationPlatformService* real_service)
      : real_service_(real_service) {}

  ~TestDatabaseClient() override = default;

  // Calling this will bypass real database query results and instead return
  // the specified override values.
  void SetOverrideSignalValues(const std::vector<float>& override_values) {
    override_values_ = override_values;
  }

  void SetSimulateError(bool simulate_error) {
    simulate_error_ = simulate_error;
  }

  // Overrides the DatabaseClient's feature processing logic to perform test
  // validations:
  // - It always forwards the metadata to the real service first to ensure the
  //   metadata configuration compiles and processes successfully without crash
  //   or error.
  // - If mock overrides are registered (via SetOverrideSignalValues), they are
  //   returned upon successful validation, bypassing the real database query
  //   results.
  // - If no overrides are set (standard integration test), the real database
  //    query results are returned directly.
  void ProcessFeatures(
      const segmentation_platform::proto::SegmentationModelMetadata& metadata,
      base::Time end_time,
      FeaturesCallback callback) override {
    last_metadata_ = metadata;
    segmentation_platform::DatabaseClient* real_client =
        real_service_ ? real_service_->GetDatabaseClient() : nullptr;
    if (!real_client || simulate_error_) {
      std::move(callback).Run(ResultStatus::kError, {});
      return;
    }

    // Forward metadata to the real segmentation platform service to verify
    // high-fidelity validation of the protobuf and query processing.
    real_client->ProcessFeatures(
        metadata, end_time,
        base::BindOnce(
            [](FeaturesCallback original_callback,
               std::optional<std::vector<float>> override_vals,
               ResultStatus status,
               const segmentation_platform::ModelProvider::Request& inputs) {
              // Verify that the real query processing ran successfully. If it
              // failed, there is likely an issue with the constructed metadata
              // schema (e.g. invalid feature configurations).
              EXPECT_EQ(status, ResultStatus::kSuccess);

              // If override values are provided for mock test features, return
              // them but still report success so that feature ranking logic
              // works.
              if (override_vals.has_value()) {
                std::move(original_callback)
                    .Run(ResultStatus::kSuccess, *override_vals);
              } else {
                std::move(original_callback).Run(status, inputs);
              }
            },
            std::move(callback), override_values_));
  }

  void AddEvent(const StructuredEvent& event) override {
    segmentation_platform::DatabaseClient* real_client =
        real_service_ ? real_service_->GetDatabaseClient() : nullptr;
    if (real_client) {
      real_client->AddEvent(event);
    }
  }

  segmentation_platform::proto::SegmentationModelMetadata last_metadata_;
  raw_ptr<segmentation_platform::SegmentationPlatformService> real_service_;
  std::optional<std::vector<float>> override_values_;
  bool simulate_error_ = false;
};

class TestSegmentationPlatformService
    : public segmentation_platform::DummySegmentationPlatformService {
 public:
  explicit TestSegmentationPlatformService(
      segmentation_platform::SegmentationPlatformService* real_service)
      : real_service_(real_service) {
    if (real_service_) {
      test_database_client_ =
          std::make_unique<TestDatabaseClient>(real_service_);
    }
  }

  ~TestSegmentationPlatformService() override = default;

  TestDatabaseClient* test_database_client() {
    return test_database_client_.get();
  }

  segmentation_platform::DatabaseClient* GetDatabaseClient() override {
    return test_database_client_.get();
  }

 private:
  raw_ptr<segmentation_platform::SegmentationPlatformService> real_service_;
  std::unique_ptr<TestDatabaseClient> test_database_client_;
};

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

struct FeatureTestConfig {
  std::unique_ptr<TipsFeature> feature;
  std::optional<std::map<std::string, float>> mock_signal_values;
};

class TipsServiceTest : public ::testing::Test {
 public:
  TipsServiceTest() = default;
  ~TipsServiceTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {segmentation_platform::features::kSegmentationPlatformFeature,
         segmentation_platform::features::kSegmentationPlatformUkmEngine},
        {});
    test_utils_ =
        std::make_unique<segmentation_platform::UkmDataManagerTestUtils>(
            &ukm_recorder_);
    test_utils_->PreProfileInit({});
    profile_ = std::make_unique<TestingProfile>();
    test_utils_->SetupForProfile(profile_.get());
    real_segmentation_service_ = segmentation_platform::
        SegmentationPlatformServiceFactory::GetForProfile(profile_.get());
    if (real_segmentation_service_ &&
        !real_segmentation_service_->IsPlatformInitialized()) {
      base::RunLoop wait_for_init;
      WaitServiceInitializedObserver wait_observer(wait_for_init.QuitClosure());
      real_segmentation_service_->GetServiceProxy()->AddObserver(
          &wait_observer);
      wait_for_init.Run();
      real_segmentation_service_->GetServiceProxy()->RemoveObserver(
          &wait_observer);
    }
    test_segmentation_service_ =
        std::make_unique<TestSegmentationPlatformService>(
            real_segmentation_service_);
    service_ = std::make_unique<TipsService>(&pref_service_,
                                             test_segmentation_service_.get());
  }

  void TearDown() override {
    service_.reset();
    test_segmentation_service_.reset();
    real_segmentation_service_ = nullptr;
    test_utils_->WillDestroyProfile(profile_.get());
    profile_.reset();
    test_utils_.reset();
  }

 protected:
  void RunDetermineBestTipTest(
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

  void RunDetermineBestTipTestWithOverrides(
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
      test_segmentation_service_->test_database_client()
          ->SetOverrideSignalValues(flat_inputs);
    }

    RunDetermineBestTipTest(std::move(features), expected_best_tip);
  }

  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<TestSegmentationPlatformService> test_segmentation_service_;
  std::unique_ptr<TipsService> service_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  ukm::TestAutoSetUkmRecorder ukm_recorder_;
  std::unique_ptr<segmentation_platform::UkmDataManagerTestUtils> test_utils_;
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<segmentation_platform::SegmentationPlatformService>
      real_segmentation_service_;
};

TEST_F(TipsServiceTest, DetermineBestTip_NoDatabaseClient) {
  service_ = std::make_unique<TipsService>(&pref_service_, nullptr);

  service_->RegisterFeature(std::make_unique<MockTipsFeature>(
      TipFeatureRank::kQuickDelete, TipsNotificationsFeatureType::kQuickDelete,
      std::vector<SignalDefinition>{UserAction("SomeAction", 7)},
      /*is_eligible=*/true));

  std::optional<TipsNotificationsFeatureType> result;
  service_->DetermineBestTip(base::BindOnce(
      [](std::optional<TipsNotificationsFeatureType>* out,
         std::optional<TipsNotificationsFeatureType> res) { *out = res; },
      &result));
  EXPECT_FALSE(result.has_value());
}

TEST_F(TipsServiceTest, DetermineBestTip_SuccessfulEvaluation) {
  auto feature = std::make_unique<MockTipsFeature>(
      TipFeatureRank::kQuickDelete, TipsNotificationsFeatureType::kQuickDelete,
      std::vector<SignalDefinition>{UserAction("SomeAction", 7)},
      /*is_eligible=*/true);
  MockTipsFeature* feature_ptr = feature.get();

  std::vector<FeatureTestConfig> configs;
  configs.push_back(FeatureTestConfig{
      .feature = std::move(feature),
      .mock_signal_values = std::map<std::string, float>{{"SomeAction", 42.0f}},
  });

  RunDetermineBestTipTestWithOverrides(
      std::move(configs), TipsNotificationsFeatureType::kQuickDelete);

  EXPECT_EQ(feature_ptr->last_signal_values_["SomeAction"], 42.0f);
}

TEST_F(TipsServiceTest, DetermineBestTip_DatabaseFailure) {
  ASSERT_TRUE(test_segmentation_service_->test_database_client());
  test_segmentation_service_->test_database_client()->SetSimulateError(true);

  service_->RegisterFeature(std::make_unique<MockTipsFeature>(
      TipFeatureRank::kQuickDelete, TipsNotificationsFeatureType::kQuickDelete,
      std::vector<SignalDefinition>{UserAction("SomeAction", 7)},
      /*is_eligible=*/true));

  std::optional<TipsNotificationsFeatureType> result;
  base::RunLoop run_loop;
  service_->DetermineBestTip(base::BindOnce(
      [](std::optional<TipsNotificationsFeatureType>* out,
         base::OnceClosure quit,
         std::optional<TipsNotificationsFeatureType> res) {
        *out = res;
        std::move(quit).Run();
      },
      &result, run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_FALSE(result.has_value());
}

TEST_F(TipsServiceTest, DetermineBestTip_RankingPriority) {
  std::vector<FeatureTestConfig> configs;

  // Register lower priority feature first (kBottomOmnibox = 3)
  configs.push_back(FeatureTestConfig{
      .feature = std::make_unique<MockTipsFeature>(
          TipFeatureRank::kBottomOmnibox,
          TipsNotificationsFeatureType::kBottomOmnibox,
          std::vector<SignalDefinition>{HistogramSum("SignalA", 1)},
          /*is_eligible=*/true),
      .mock_signal_values = std::map<std::string, float>{{"SignalA", 1.0f}},
  });

  // Register higher priority feature second (kGoogleLens = 2)
  configs.push_back(FeatureTestConfig{
      .feature = std::make_unique<MockTipsFeature>(
          TipFeatureRank::kGoogleLens,
          TipsNotificationsFeatureType::kGoogleLens,
          std::vector<SignalDefinition>{
              HistogramEnum("SignalB", 1, std::vector<int32_t>{1, 2})},
          /*is_eligible=*/true),
      .mock_signal_values = std::map<std::string, float>{{"SignalB", 2.0f}},
  });

  // Should select kGoogleLens because its rank (2) has higher display priority
  // than kBottomOmnibox (3)
  RunDetermineBestTipTestWithOverrides(
      std::move(configs), TipsNotificationsFeatureType::kGoogleLens);
}

TEST_F(TipsServiceTest, VerifyMetadataConstruction) {
  service_->RegisterFeature(std::make_unique<MockTipsFeature>(
      TipFeatureRank::kQuickDelete, TipsNotificationsFeatureType::kQuickDelete,
      std::vector<SignalDefinition>{
          UserAction("Action1", 7),
          HistogramSum("Sum1", 14),
          HistogramEnum("Enum1", 28, std::vector<int32_t>{1, 2}),
      },
      /*is_eligible=*/true));

  std::optional<TipsNotificationsFeatureType> result;
  base::RunLoop run_loop;
  service_->DetermineBestTip(base::BindOnce(
      [](std::optional<TipsNotificationsFeatureType>* out,
         base::OnceClosure quit,
         std::optional<TipsNotificationsFeatureType> res) {
        *out = res;
        std::move(quit).Run();
      },
      &result, run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_TRUE(test_segmentation_service_->test_database_client());
  const auto& captured_metadata =
      test_segmentation_service_->test_database_client()->last_metadata_;

  // Verify captured_metadata
  EXPECT_EQ(captured_metadata.input_features_size(), 3);

  // Verify Action1
  const auto& f1 = captured_metadata.input_features(0).uma_feature();
  EXPECT_EQ(f1.name(), "Action1");
  EXPECT_EQ(f1.type(), segmentation_platform::proto::SignalType::USER_ACTION);

  // Verify Sum1
  const auto& f2 = captured_metadata.input_features(1).uma_feature();
  EXPECT_EQ(f2.name(), "Sum1");
  EXPECT_EQ(f2.type(),
            segmentation_platform::proto::SignalType::HISTOGRAM_VALUE);

  // Verify Enum1
  const auto& f3 = captured_metadata.input_features(2).uma_feature();
  EXPECT_EQ(f3.name(), "Enum1");
  EXPECT_EQ(f3.type(),
            segmentation_platform::proto::SignalType::HISTOGRAM_ENUM);
  ASSERT_EQ(f3.enum_ids_size(), 2);
  EXPECT_EQ(f3.enum_ids(0), 1);
  EXPECT_EQ(f3.enum_ids(1), 2);
}

TEST_F(TipsServiceTest, DetermineBestTip_MultipleFeaturesAndSignals) {
  std::vector<FeatureTestConfig> configs;

  // Feature 1: 2 signals
  auto feature1 = std::make_unique<MockTipsFeature>(
      TipFeatureRank::kQuickDelete, TipsNotificationsFeatureType::kQuickDelete,
      std::vector<SignalDefinition>{UserAction("Action1", 7),
                                    HistogramSum("Sum1", 7)},
      /*is_eligible=*/true);
  MockTipsFeature* feature1_ptr = feature1.get();
  configs.push_back(FeatureTestConfig{
      .feature = std::move(feature1),
      .mock_signal_values =
          std::map<std::string, float>{{"Action1", 10.0f}, {"Sum1", 20.0f}},
  });

  // Feature 2: 1 signal
  auto feature2 = std::make_unique<MockTipsFeature>(
      TipFeatureRank::kGoogleLens, TipsNotificationsFeatureType::kGoogleLens,
      std::vector<SignalDefinition>{
          HistogramEnum("Enum1", 7, std::vector<int32_t>{1})},
      /*is_eligible=*/true);
  MockTipsFeature* feature2_ptr = feature2.get();
  configs.push_back(FeatureTestConfig{
      .feature = std::move(feature2),
      .mock_signal_values = std::map<std::string, float>{{"Enum1", 30.0f}},
  });

  // Both are eligible, Feature 1 (kQuickDelete=1) has higher priority than
  // Feature 2 (kGoogleLens=2)
  RunDetermineBestTipTestWithOverrides(
      std::move(configs), TipsNotificationsFeatureType::kQuickDelete);

  // Verify signals were correctly mapped to each feature
  EXPECT_EQ(feature1_ptr->last_signal_values_["Action1"], 10.0f);
  EXPECT_EQ(feature1_ptr->last_signal_values_["Sum1"], 20.0f);
  EXPECT_EQ(feature2_ptr->last_signal_values_["Enum1"], 30.0f);
}

}  // namespace tips
