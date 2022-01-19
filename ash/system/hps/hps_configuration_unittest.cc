// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/hps/hps_configuration.h"

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/dbus/hps/hps_service.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

// Expect two protos to be equal if they are serialized into the same strings.
MATCHER_P(ProtoEquals, expected_message, "") {
  std::string expected_serialized, actual_serialized;
  expected_message.SerializeToString(&expected_serialized);
  arg.SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}

TEST(HpsFeatureConfigTest, EmptyParamsValid) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {features::kQuickDim, features::kSnoopingProtection},
      {} /* disabled_features */);

  EXPECT_TRUE(GetEnableHpsSenseConfig().has_value());
  EXPECT_TRUE(GetEnableHpsNotifyConfig().has_value());
}

TEST(HpsFeatureConfigTest, ReturnNullIfTypeIsNotRecognizable) {
  const base::FieldTrialParams params = {{"filter_config_case", "0"}};
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kQuickDim, params}, {features::kSnoopingProtection, params}},
      {});

  EXPECT_FALSE(GetEnableHpsSenseConfig().has_value());
  EXPECT_FALSE(GetEnableHpsNotifyConfig().has_value());
}

TEST(HpsFeatureConfigTest, VerifyBasicFilterConfig) {
  const std::map<std::string, std::string> params = {
      {"filter_config_case", "1"}};
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kQuickDim, params}, {features::kSnoopingProtection, params}},
      {});

  hps::FeatureConfig expected_config;
  expected_config.mutable_basic_filter_config();

  EXPECT_THAT(GetEnableHpsSenseConfig().value(), ProtoEquals(expected_config));
  EXPECT_THAT(GetEnableHpsNotifyConfig().value(), ProtoEquals(expected_config));
}

TEST(HpsFeatureConfigTest, VerifyConsecutiveResultsFilterConfig) {
  const std::map<std::string, std::string> params = {
      {"filter_config_case", "2"},
      {"count", "3"},
      {"threshold", "4"},
      {"initial_state", "false"}};
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kQuickDim, params}, {features::kSnoopingProtection, params}},
      {});

  hps::FeatureConfig expected_config;
  auto& consecutive_results_filter_config =
      *expected_config.mutable_consecutive_results_filter_config();
  consecutive_results_filter_config.set_count(3);
  consecutive_results_filter_config.set_threshold(4);
  consecutive_results_filter_config.set_initial_state(false);

  const auto hps_sense_config = GetEnableHpsSenseConfig();
  ASSERT_TRUE(hps_sense_config.has_value());
  EXPECT_THAT(*hps_sense_config, ProtoEquals(expected_config));

  const auto hps_notify_config = GetEnableHpsNotifyConfig();
  ASSERT_TRUE(hps_notify_config.has_value());
  EXPECT_THAT(*hps_notify_config, ProtoEquals(expected_config));
}

TEST(HpsFeatureConfigTest, VerifyAverageFilterConfig) {
  const std::map<std::string, std::string> params = {
      {"filter_config_case", "3"},
      {"average_window_size", "4"},
      {"positive_score_threshold", "5"},
      {"negative_score_threshold", "6"},
      {"default_uncertain_score", "7"}};
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kQuickDim, params}, {features::kSnoopingProtection, params}},
      {});

  hps::FeatureConfig expected_config;
  auto& average_filter_config =
      *expected_config.mutable_average_filter_config();
  average_filter_config.set_average_window_size(4);
  average_filter_config.set_positive_score_threshold(5);
  average_filter_config.set_negative_score_threshold(6);
  average_filter_config.set_default_uncertain_score(7);

  const auto hps_sense_config = GetEnableHpsSenseConfig();
  ASSERT_TRUE(hps_sense_config.has_value());
  EXPECT_THAT(*hps_sense_config, ProtoEquals(expected_config));

  const auto hps_notify_config = GetEnableHpsNotifyConfig();
  ASSERT_TRUE(hps_notify_config.has_value());
  EXPECT_THAT(*hps_notify_config, ProtoEquals(expected_config));
}

TEST(HpsFeatureConfigTest, ExtraParamsInvalid) {
  // Valid params for the consecutive results filter, plus one extraneous param.
  const std::map<std::string, std::string> params = {
      {"filter_config_case", "2"},
      {"count", "3"},
      {"threshold", "4"},
      {"initial_state", "false"},
      {"extra_param", ""}};
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kQuickDim, params}, {features::kSnoopingProtection, params}},
      {});

  EXPECT_FALSE(GetEnableHpsSenseConfig().has_value());
  EXPECT_FALSE(GetEnableHpsNotifyConfig().has_value());
}

}  // namespace ash
