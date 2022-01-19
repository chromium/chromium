// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/hps/hps_configuration.h"

#include "ash/constants/ash_features.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chromeos/dbus/hps/hps_service.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
namespace {

// Default quick dim delay to configure power_manager.
constexpr base::TimeDelta kQuickDimDelayDefault = base::Seconds(45);

// Default value determines whether send feedback to configure power_manager.
constexpr int kShouldSendFeedbackIfUndimmed = false;

// Returns either the integer parameter with the given name, or nullopt if the
// parameter can't be found or parsed.
absl::optional<int> GetIntParam(const base::FieldTrialParams& params,
                                const std::string& name) {
  const auto it = params.find(name);
  if (it == params.end())
    return absl::nullopt;

  int result;
  if (!base::StringToInt(it->second, &result))
    return absl::nullopt;

  return result;
}

// Returns either the boolean parameter with the given name, or nullopt if the
// parameter can't be found or parsed.
absl::optional<bool> GetBoolParam(const base::FieldTrialParams& params,
                                  const std::string& name) {
  const auto it = params.find(name);
  if (it == params.end())
    return absl::nullopt;

  if (it->second == "true") {
    return true;
  } else if (it->second == "false") {
    return false;
  } else {
    return absl::nullopt;
  }
}

// The config to use when no parameters are specified. We need to ensure that
// that empty params induce a working config (as required for, e.g., a
// chrome:://flags entry).
hps::FeatureConfig GetDefaultFeatureConfig() {
  hps::FeatureConfig config;

  auto& filter_config = *config.mutable_average_filter_config();
  filter_config.set_average_window_size(3);
  filter_config.set_positive_score_threshold(40);
  filter_config.set_negative_score_threshold(-40);
  filter_config.set_default_uncertain_score(0);

  return config;
}

// Returns true if the given set of keys is the entire set of keys in the params
// map.
bool ParamKeysAre(const base::FieldTrialParams& params,
                  const std::set<std::string> keys) {
  for (const std::string& key : keys) {
    if (params.find(key) == params.end())
      return false;
  }

  return params.size() == keys.size();
}

// This function constructs a FeatureConfig proto from feature parameters..
// The FeatureConfig contains one type of FilterConfig that will be used for
// enabling a Hps feature.
//
// If empty parameters are provided, a reasonable default is used.
//
// More details can be found at:
// src/platform2/hps/daemon/filters/filter_factory.h
absl::optional<hps::FeatureConfig> ConstructHpsFilterConfigFromFeatureParams(
    const base::Feature& feature) {
  // Load current params map for the feature.
  base::FieldTrialParams params;
  base::GetFieldTrialParamsByFeature(feature, &params);

  // Valid default case.
  if (params.empty())
    return GetDefaultFeatureConfig();

  const absl::optional<int> filter_config_case =
      GetIntParam(params, "filter_config_case");
  if (!filter_config_case.has_value())
    return absl::nullopt;

  switch (*filter_config_case) {
    case hps::FeatureConfig::kBasicFilterConfig: {
      // There are no parameters for the basic filter.
      if (!ParamKeysAre(params, {"filter_config_case"}))
        return absl::nullopt;

      hps::FeatureConfig config;
      config.mutable_basic_filter_config();
      return config;
    }

    case hps::FeatureConfig::kConsecutiveResultsFilterConfig: {
      const absl::optional<int> count = GetIntParam(params, "count");
      const absl::optional<int> threshold = GetIntParam(params, "threshold");
      const absl::optional<bool> initial_state =
          GetBoolParam(params, "initial_state");

      if (!count.has_value() || !threshold.has_value() ||
          !initial_state.has_value() ||
          !ParamKeysAre(params, {"filter_config_case", "count", "threshold",
                                 "initial_state"})) {
        return absl::nullopt;
      }

      hps::FeatureConfig config;
      auto& filter_config = *config.mutable_consecutive_results_filter_config();
      filter_config.set_count(*count);
      filter_config.set_threshold(*threshold);
      filter_config.set_initial_state(*initial_state);
      return config;
    }

    case hps::FeatureConfig::kAverageFilterConfig: {
      const absl::optional<int> average_window_size =
          GetIntParam(params, "average_window_size");
      const absl::optional<int> positive_score_threshold =
          GetIntParam(params, "positive_score_threshold");
      const absl::optional<int> negative_score_threshold =
          GetIntParam(params, "negative_score_threshold");
      const absl::optional<int> default_uncertain_score =
          GetIntParam(params, "default_uncertain_score");

      if (!average_window_size.has_value() ||
          !positive_score_threshold.has_value() ||
          !negative_score_threshold.has_value() ||
          !default_uncertain_score.has_value() ||
          !ParamKeysAre(params,
                        {"filter_config_case", "average_window_size",
                         "positive_score_threshold", "negative_score_threshold",
                         "default_uncertain_score"})) {
        return absl::nullopt;
      }

      hps::FeatureConfig config;
      auto& filter_config = *config.mutable_average_filter_config();
      filter_config.set_average_window_size(*average_window_size);
      filter_config.set_positive_score_threshold(*positive_score_threshold);
      filter_config.set_negative_score_threshold(*negative_score_threshold);
      filter_config.set_default_uncertain_score(*default_uncertain_score);
      return config;
    }

    default:
      return absl::nullopt;
  }
}

}  // namespace

absl::optional<hps::FeatureConfig> GetEnableHpsSenseConfig() {
  return ConstructHpsFilterConfigFromFeatureParams(features::kQuickDim);
}

absl::optional<hps::FeatureConfig> GetEnableHpsNotifyConfig() {
  return ConstructHpsFilterConfigFromFeatureParams(
      features::kSnoopingProtection);
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
