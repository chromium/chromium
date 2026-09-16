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
#include "base/hash/hash.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/metrics_hashes.h"
#include "base/metrics/user_metrics.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/notifications/scheduler/public/notification_params.h"
#include "chrome/browser/tips/core/tips_feature.h"
#include "chrome/browser/tips/core/tips_prefs.h"
#include "chrome/browser/tips/core/tips_service_test_base.h"
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

}  // namespace

class TipsServiceTest : public TipsServiceTestBase {};

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
