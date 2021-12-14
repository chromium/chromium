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

#if defined(OS_ANDROID)
#include "chrome/browser/feature_guide/notifications/feature_notification_guide_service.h"
#include "chrome/browser/flags/android/cached_feature_flags.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/ui/android/start_surface/start_surface_android.h"
#include "components/query_tiles/switches.h"
#endif

using optimization_guide::proto::OptimizationTarget;

namespace segmentation_platform {

namespace {

// Default TTL for segment selection and unknown selection:

constexpr int kDummyFeatureSelectionTTLDays = 1;

#if defined(OS_ANDROID)

constexpr int kAdaptiveToolbarDefaultSelectionTTLDays = 28;

constexpr int kChromeStartDefaultSelectionTTLDays = 30;
constexpr int kChromeStartDefaultUnknownTTLDays = 7;

constexpr int kChromeLowUserEngagementSelectionTTLDays = 30;

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
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
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

#if defined(OS_ANDROID)
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
      feature_guide::features::kFeatureNotificationGuide,
      "segment_selection_ttl_days", kChromeLowUserEngagementSelectionTTLDays);
  config->segment_selection_ttl = base::Days(segment_selection_ttl_days);
  config->unknown_selection_ttl = base::Days(segment_selection_ttl_days);
  return config;
}

#endif  // defined(OS_ANDROID)

}  // namespace

std::vector<std::unique_ptr<Config>> GetSegmentationPlatformConfig() {
  std::vector<std::unique_ptr<Config>> configs;
  if (base::FeatureList::IsEnabled(
          segmentation_platform::features::kSegmentationPlatformDummyFeature)) {
    configs.emplace_back(GetConfigForDummyFeature());
  }
#if defined(OS_ANDROID)
  if (base::FeatureList::IsEnabled(
          chrome::android::kAdaptiveButtonInTopToolbarCustomizationV2)) {
    configs.emplace_back(GetConfigForAdaptiveToolbar());
  }
  if (IsStartSurfaceBehaviouralTargetingEnabled()) {
    configs.emplace_back(GetConfigForChromeStartAndroid());
  }
  if (base::FeatureList::IsEnabled(
          segmentation_platform::features::
              kSegmentationPlatformQueryTilesFeature)) {
    configs.emplace_back(GetConfigForQueryTiles());
  }
  if (base::FeatureList::IsEnabled(
          feature_guide::features::kFeatureNotificationGuide)) {
    configs.emplace_back(GetConfigForChromeLowUserEngagement());
  }
#endif
  return configs;
}

}  // namespace segmentation_platform
