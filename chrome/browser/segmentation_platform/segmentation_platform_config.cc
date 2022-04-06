// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/segmentation_platform/segmentation_platform_config.h"

#include <memory>

#include "base/feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/model_provider.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/metrics/field_trial_params.h"
#include "chrome/browser/feature_guide/notifications/feature_notification_guide_service.h"
#include "chrome/browser/flags/android/cached_feature_flags.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/segmentation_platform/default_model/chrome_start_model_android.h"
#include "chrome/browser/segmentation_platform/default_model/query_tiles_model.h"
#include "chrome/browser/ui/android/start_surface/start_surface_android.h"
#include "components/query_tiles/switches.h"
#endif

using optimization_guide::proto::OptimizationTarget;

namespace segmentation_platform {

namespace {

// Default TTL for segment selection and unknown selection:

constexpr int kDummyFeatureSelectionTTLDays = 1;

#if BUILDFLAG(IS_ANDROID)

constexpr int kAdaptiveToolbarDefaultSelectionTTLDays = 28;

constexpr int kChromeStartDefaultSelectionTTLDays = 30;
constexpr int kChromeStartDefaultUnknownTTLDays = 7;
constexpr char kChromeStartDefaultModelEnabledParam[] = "enable_default_model";

constexpr int kChromeLowUserEngagementSelectionTTLDays = 30;

constexpr char kQueryTilesDefaultModelEnabledParam[] = "enable_default_model";
// See
// https://source.chromium.org/chromium/chromium/src/+/main:chrome/android/java/src/org/chromium/chrome/browser/query_tiles/QueryTileUtils.java
const char kNumDaysKeepShowingQueryTiles[] =
    "num_days_keep_showing_query_tiles";
const char kNumDaysMVCkicksBelowThreshold[] =
    "num_days_mv_clicks_below_threshold";

// DEFAULT_NUM_DAYS_KEEP_SHOWING_QUERY_TILES
constexpr int kQueryTilesDefaultSelectionTTLDays = 28;
// DEFAULT_NUM_DAYS_MV_CLICKS_BELOW_THRESHOLD
constexpr int kQueryTilesDefaultUnknownTTLDays = 7;
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
std::unique_ptr<Config> GetConfigForAdaptiveToolbar() {
  auto config = std::make_unique<Config>();
  config->segmentation_key = kAdaptiveToolbarSegmentationKey;

  int segment_selection_ttl_days = base::GetFieldTrialParamByFeatureAsInt(
      chrome::android::kAdaptiveButtonInTopToolbarCustomizationV2,
      "segment_selection_ttl_days", kAdaptiveToolbarDefaultSelectionTTLDays);
  config->segment_selection_ttl = base::Days(segment_selection_ttl_days);
  // Do not set unknown TTL so that the platform ignores unknown results.

  // A hardcoded list of segment IDs known to the segmentation platform.
  config->segment_ids = {
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB,
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_SHARE,
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_VOICE,
  };

  return config;
}
#endif

std::unique_ptr<Config> GetConfigForDummyFeature() {
  auto config = std::make_unique<Config>();
  config->segmentation_key = kDummySegmentationKey;
  config->segment_ids = {
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_DUMMY,
  };
  config->segment_selection_ttl = base::Days(kDummyFeatureSelectionTTLDays);
  config->unknown_selection_ttl = base::Days(kDummyFeatureSelectionTTLDays);
  return config;
}

#if BUILDFLAG(IS_ANDROID)
std::unique_ptr<ModelProvider> GetChromeStartAndroidModel() {
  if (!base::GetFieldTrialParamByFeatureAsBool(
          chrome::android::kStartSurfaceAndroid,
          kChromeStartDefaultModelEnabledParam, false)) {
    return nullptr;
  }
  return std::make_unique<ChromeStartModel>();
}

std::unique_ptr<Config> GetConfigForChromeStartAndroid() {
  auto config = std::make_unique<Config>();
  config->segmentation_key = kChromeStartAndroidSegmentationKey;
  config->segment_ids = {
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_CHROME_START_ANDROID,
  };

  int segment_selection_ttl_days = base::GetFieldTrialParamByFeatureAsInt(
      chrome::android::kStartSurfaceAndroid, "segment_selection_ttl_days",
      kChromeStartDefaultSelectionTTLDays);
  int unknown_selection_ttl_days = base::GetFieldTrialParamByFeatureAsInt(
      chrome::android::kStartSurfaceAndroid,
      "segment_unknown_selection_ttl_days", kChromeStartDefaultUnknownTTLDays);
  config->segment_selection_ttl = base::Days(segment_selection_ttl_days);
  config->unknown_selection_ttl = base::Days(unknown_selection_ttl_days);

  return config;
}

std::unique_ptr<ModelProvider> GetQueryTilesDefaultModel() {
  if (!base::GetFieldTrialParamByFeatureAsBool(
          query_tiles::features::kQueryTilesSegmentation,
          kQueryTilesDefaultModelEnabledParam, false)) {
    return nullptr;
  }
  return std::make_unique<QueryTilesModel>();
}

std::unique_ptr<Config> GetConfigForQueryTiles() {
  auto config = std::make_unique<Config>();
  config->segmentation_key = kQueryTilesSegmentationKey;
  config->segment_ids = {
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_QUERY_TILES,
  };

  int segment_selection_ttl_days = base::GetFieldTrialParamByFeatureAsInt(
      query_tiles::features::kQueryTilesSegmentation,
      kNumDaysKeepShowingQueryTiles, kQueryTilesDefaultSelectionTTLDays);
  int unknown_selection_ttl_days = base::GetFieldTrialParamByFeatureAsInt(
      query_tiles::features::kQueryTilesSegmentation,
      kNumDaysMVCkicksBelowThreshold, kQueryTilesDefaultUnknownTTLDays);
  config->segment_selection_ttl = base::Days(segment_selection_ttl_days);
  config->unknown_selection_ttl = base::Days(unknown_selection_ttl_days);
  return config;
}

std::unique_ptr<Config> GetConfigForChromeLowUserEngagement() {
  auto config = std::make_unique<Config>();
  config->segmentation_key = kChromeLowUserEngagementSegmentationKey;
  config->segment_ids = {
      OptimizationTarget::
          OPTIMIZATION_TARGET_SEGMENTATION_CHROME_LOW_USER_ENGAGEMENT,
  };

  int segment_selection_ttl_days = base::GetFieldTrialParamByFeatureAsInt(
      feature_guide::features::kSegmentationModelLowEngagedUsers,
      "segment_selection_ttl_days", kChromeLowUserEngagementSelectionTTLDays);
  config->segment_selection_ttl = base::Days(segment_selection_ttl_days);
  config->unknown_selection_ttl = base::Days(segment_selection_ttl_days);
  return config;
}

#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace

std::vector<std::unique_ptr<Config>> GetSegmentationPlatformConfig() {
  std::vector<std::unique_ptr<Config>> configs;
  if (base::FeatureList::IsEnabled(
          segmentation_platform::features::kSegmentationPlatformDummyFeature)) {
    configs.emplace_back(GetConfigForDummyFeature());
  }
#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(
          chrome::android::kAdaptiveButtonInTopToolbarCustomizationV2)) {
    configs.emplace_back(GetConfigForAdaptiveToolbar());
  }
  if (IsStartSurfaceBehaviouralTargetingEnabled()) {
    configs.emplace_back(GetConfigForChromeStartAndroid());
  }
  if (base::FeatureList::IsEnabled(
          query_tiles::features::kQueryTilesSegmentation)) {
    configs.emplace_back(GetConfigForQueryTiles());
  }
  if (base::FeatureList::IsEnabled(
          feature_guide::features::kSegmentationModelLowEngagedUsers)) {
    configs.emplace_back(GetConfigForChromeLowUserEngagement());
  }
#endif
  return configs;
}

std::unique_ptr<ModelProvider> GetSegmentationDefaultModelProvider(
    optimization_guide::proto::OptimizationTarget target) {
#if BUILDFLAG(IS_ANDROID)
  if (target ==
      optimization_guide::proto::OPTIMIZATION_TARGET_SEGMENTATION_QUERY_TILES) {
    return GetQueryTilesDefaultModel();
  }
  if (target == optimization_guide::proto::
                    OPTIMIZATION_TARGET_SEGMENTATION_CHROME_START_ANDROID) {
    return GetChromeStartAndroidModel();
  }
#endif
  return nullptr;
}

}  // namespace segmentation_platform
