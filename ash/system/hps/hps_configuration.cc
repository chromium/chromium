// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/hps/hps_configuration.h"

#include "ash/constants/ash_features.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "chromeos/dbus/hps/hps_service.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
namespace {
// Used for indicating which oneof field to construct for `filter_config` inside
// FeatureConfig. We ensure that this is a valid configuration so that passing
// empty params induces a working config (as required for, e.g., a
// chrome:://flags entry).
constexpr int kFilterConfigCaseDefault = 1;

// Default value for `FeatureConfig.consecutive_results_filter_config.count`.
// Default 1 means the change will be notified if the inference result is
// different from last one.
constexpr int kConsecutiveResultsFilterCountDefault = 1;

// Default value for
// `FeatureConfig.consecutive_results_filter_config.threshold`. The inference
// result is a value from [-128, 128), we use 0 as a default threshold.
constexpr int kConsecutiveResultsFilterThresholdDefault = 0;

// Default value for
// `FeatureConfig.consecutive_results_filter_config.initial_state`.
constexpr bool kConsecutiveResultsFilterIntialStateDefault = false;

// Default quick dim delay to configure power_manager.
constexpr base::TimeDelta kQuickDimDelayDefault = base::Seconds(60);

// Default value determines whether send feedback to configure power_manager.
constexpr int kShouldSendFeedbackIfUndimmed = false;

// This function constructs a FeatureConfig proto From Finch.
// The FeatureConfig contains one type of FilterConfig that will be used for
// enabling a Hps feature.
// If filter_config_case is set to 0 (by default), absl::nullopt will be
// returned; otherwise one type of FilterConfig will be returned with each
// field getting its value from the finch params with the same name.
// More details can be found at:
// src/platform2/hps/daemon/filters/filter_factory.h
absl::optional<hps::FeatureConfig> ConstructHpsFilterConfigFromFinch(
    const base::Feature& feature) {
  const int filter_config_case = base::GetFieldTrialParamByFeatureAsInt(
      feature, "filter_config_case", kFilterConfigCaseDefault);
  hps::FeatureConfig config;
  switch (filter_config_case) {
    case hps::FeatureConfig::kBasicFilterConfig: {
      config.mutable_basic_filter_config();
      return config;
    }
    case hps::FeatureConfig::kConsecutiveResultsFilterConfig: {
      auto& consecutive_results_filter_config =
          *config.mutable_consecutive_results_filter_config();

      consecutive_results_filter_config.set_count(
          base::GetFieldTrialParamByFeatureAsInt(
              feature, "count", kConsecutiveResultsFilterCountDefault));
      consecutive_results_filter_config.set_threshold(
          base::GetFieldTrialParamByFeatureAsInt(
              feature, "threshold", kConsecutiveResultsFilterThresholdDefault));
      consecutive_results_filter_config.set_initial_state(
          base::GetFieldTrialParamByFeatureAsBool(
              feature, "initial_state",
              kConsecutiveResultsFilterIntialStateDefault));
      return config;
    }
    default:
      return absl::nullopt;
  }
}
}  // namespace

absl::optional<hps::FeatureConfig> GetEnableHpsSenseConfig() {
  return ConstructHpsFilterConfigFromFinch(features::kQuickDim);
}

absl::optional<hps::FeatureConfig> GetEnableHpsNotifyConfig() {
  return ConstructHpsFilterConfigFromFinch(features::kSnoopingProtection);
}

base::TimeDelta GetQuickDimDelay() {
  const int quick_dim_ms = base::GetFieldTrialParamByFeatureAsInt(
      features::kQuickDim, "quick_dim_ms",
      kQuickDimDelayDefault.InMilliseconds());
  return base::Milliseconds(quick_dim_ms);
}

bool GetQuickDimFeedbackEnabled() {
  return base::GetFieldTrialParamByFeatureAsBool(features::kQuickDim,
                                                 "send_feedback_if_undimmed",
                                                 kShouldSendFeedbackIfUndimmed);
}

}  // namespace ash
