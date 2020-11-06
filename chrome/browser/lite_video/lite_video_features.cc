// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lite_video/lite_video_features.h"

#include "base/feature_list.h"
#include "base/json/json_reader.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/values.h"
#include "chrome/common/chrome_features.h"
#include "net/nqe/effective_connection_type.h"

namespace lite_video {
namespace features {

bool IsLiteVideoEnabled() {
  return base::FeatureList::IsEnabled(::features::kLiteVideo);
}

bool IsCoinflipExperimentEnabled() {
  return base::GetFieldTrialParamByFeatureAsBool(::features::kLiteVideo,
                                                 "is_coinflip_exp", false);
}

bool LiteVideoUseOptimizationGuide() {
  return base::GetFieldTrialParamByFeatureAsBool(
      ::features::kLiteVideo, "use_optimization_guide", false);
}

base::Optional<base::Value> GetLiteVideoOriginHintsFromFieldTrial() {
  if (!IsLiteVideoEnabled())
    return base::nullopt;

  const std::string lite_video_origin_hints_json =
      base::GetFieldTrialParamValueByFeature(::features::kLiteVideo,
                                             "lite_video_origin_hints");
  if (lite_video_origin_hints_json.empty())
    return base::nullopt;

  base::Optional<base::Value> lite_video_origin_hints =
      base::JSONReader::Read(lite_video_origin_hints_json);

  UMA_HISTOGRAM_BOOLEAN(
      "LiteVideo.OriginHints.ParseResult",
      lite_video_origin_hints && lite_video_origin_hints->is_dict());

  return lite_video_origin_hints;
}

base::TimeDelta LiteVideoTargetDownlinkRTTLatency() {
  return base::TimeDelta::FromMilliseconds(GetFieldTrialParamByFeatureAsInt(
      ::features::kLiteVideo, "target_downlink_rtt_latency_ms", 500));
}

int LiteVideoKilobytesToBufferBeforeThrottle() {
  return GetFieldTrialParamByFeatureAsInt(
      ::features::kLiteVideo, "kilobyte_to_buffer_before_throttle", 10);
}

base::TimeDelta LiteVideoMaxThrottlingDelay() {
  return base::TimeDelta::FromMilliseconds(GetFieldTrialParamByFeatureAsInt(
      ::features::kLiteVideo, "max_throttling_delay_ms", 5000));
}

size_t MaxUserBlocklistHosts() {
  return GetFieldTrialParamByFeatureAsInt(::features::kLiteVideo,
                                          "max_user_blocklist_hosts", 50);
}

base::TimeDelta UserBlocklistHostDuration() {
  return base::TimeDelta::FromDays(GetFieldTrialParamByFeatureAsInt(
      ::features::kLiteVideo, "user_blocklist_host_duration_in_days", 1));
}

int UserBlocklistOptOutHistoryThreshold() {
  return GetFieldTrialParamByFeatureAsInt(
      ::features::kLiteVideo, "user_blocklist_opt_out_history_threshold", 5);
}

int LiteVideoBlocklistVersion() {
  return GetFieldTrialParamByFeatureAsInt(::features::kLiteVideo, "version", 0);
}

net::EffectiveConnectionType MinLiteVideoECT() {
  return net::GetEffectiveConnectionTypeForName(
             base::GetFieldTrialParamValueByFeature(::features::kLiteVideo,
                                                    "min_lite_video_ect"))
      .value_or(net::EFFECTIVE_CONNECTION_TYPE_4G);
}

int MaxOptimizationGuideHintCacheSize() {
  return LiteVideoUseOptimizationGuide()
             ? GetFieldTrialParamByFeatureAsInt(
                   ::features::kLiteVideo, "max_opt_guide_hint_cache_size", 10)
             : 1;
}

base::flat_set<std::string> GetLiteVideoPermanentBlocklist() {
  if (!IsLiteVideoEnabled())
    return {};

  const std::string permanent_host_blocklist_json =
      base::GetFieldTrialParamValueByFeature(::features::kLiteVideo,
                                             "permanent_host_blocklist");
  if (permanent_host_blocklist_json.empty())
    return {};

  base::Optional<base::Value> permanent_host_blocklist_parsed =
      base::JSONReader::Read(permanent_host_blocklist_json);

  if (!permanent_host_blocklist_parsed ||
      !permanent_host_blocklist_parsed->is_list())
    return {};

  base::flat_set<std::string> permanent_host_blocklist;
  permanent_host_blocklist.reserve(
      permanent_host_blocklist_parsed->GetList().size());
  for (const auto& host : permanent_host_blocklist_parsed->GetList()) {
    if (!host.is_string())
      continue;
    permanent_host_blocklist.insert(host.GetString());
  }
  return permanent_host_blocklist;
}

bool IsLiteVideoNotAllowedForPageTransition(
    ui::PageTransition page_transition) {
  if (!(page_transition & ui::PAGE_TRANSITION_FORWARD_BACK))
    return false;
  return !base::GetFieldTrialParamByFeatureAsBool(
      ::features::kLiteVideo, "allow_on_forward_back", false);
}

int GetMaxRebuffersPerFrame() {
  return GetFieldTrialParamByFeatureAsInt(::features::kLiteVideo,
                                          "max_rebuffers_per_frame", 1);
}

}  // namespace features
}  // namespace lite_video
