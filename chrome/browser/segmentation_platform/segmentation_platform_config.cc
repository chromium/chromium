// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/segmentation_platform/segmentation_platform_config.h"

#include <memory>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/segmentation_platform/default_model/low_user_engagement_model.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/model_provider.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/feature_guide/notifications/feature_notification_guide_service.h"
#include "chrome/browser/flags/android/cached_feature_flags.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/segmentation_platform/default_model/chrome_start_model_android.h"
#endif

using optimization_guide::proto::OptimizationTarget;

namespace segmentation_platform {

namespace {

constexpr char kDefaultModelEnabledParam[] = "enable_default_model";

// Default TTL for segment selection and unknown selection:

constexpr int kDummyFeatureSelectionTTLDays = 1;

constexpr int kChromeLowUserEngagementSelectionTTLDays = 30;

#if BUILDFLAG(IS_ANDROID)

constexpr int kAdaptiveToolbarDefaultSelectionTTLDays = 28;

constexpr int kChromeStartDefaultSelectionTTLDays = 30;
constexpr int kChromeStartDefaultUnknownTTLDays = 7;

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
          chrome::android::kStartSurfaceAndroid, kDefaultModelEnabledParam,
          false)) {
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

#endif  // BUILDFLAG(IS_ANDROID)

std::unique_ptr<ModelProvider> GetLowEngagementDefaultModel() {
  if (!base::GetFieldTrialParamByFeatureAsBool(
          features::kSegmentationPlatformLowEngagementFeature,
          kDefaultModelEnabledParam, false)) {
    return nullptr;
  }
  return std::make_unique<LowUserEngagementModel>();
}

bool IsLowEngagementFeatureEnabled() {
  // TODO(ssid): Remove this extra feature and change feature guide to use the
  // segmentation defined feature.
#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(
          feature_guide::features::kSegmentationModelLowEngagedUsers)) {
    return true;
  }
#endif
  return base::FeatureList::IsEnabled(
      features::kSegmentationPlatformLowEngagementFeature);
}

std::unique_ptr<Config> GetConfigForChromeLowUserEngagement() {
  auto config = std::make_unique<Config>();
  config->segmentation_key = kChromeLowUserEngagementSegmentationKey;
  config->segment_ids = {
      OptimizationTarget::
          OPTIMIZATION_TARGET_SEGMENTATION_CHROME_LOW_USER_ENGAGEMENT,
  };

#if BUILDFLAG(IS_ANDROID)
  int segment_selection_ttl_days = base::GetFieldTrialParamByFeatureAsInt(
      feature_guide::features::kSegmentationModelLowEngagedUsers,
      "segment_selection_ttl_days", kChromeLowUserEngagementSelectionTTLDays);
#else
  int segment_selection_ttl_days = kChromeLowUserEngagementSelectionTTLDays;
#endif

  config->segment_selection_ttl = base::Days(segment_selection_ttl_days);
  config->unknown_selection_ttl = base::Days(segment_selection_ttl_days);
  return config;
}

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
#endif
  if (IsLowEngagementFeatureEnabled()) {
    configs.emplace_back(GetConfigForChromeLowUserEngagement());
  }
  return configs;
}

std::unique_ptr<ModelProvider> GetSegmentationDefaultModelProvider(
    optimization_guide::proto::OptimizationTarget target) {
#if BUILDFLAG(IS_ANDROID)
  if (target == optimization_guide::proto::
                    OPTIMIZATION_TARGET_SEGMENTATION_CHROME_START_ANDROID) {
    return GetChromeStartAndroidModel();
  }
#endif
  if (target ==
      optimization_guide::proto::
          OPTIMIZATION_TARGET_SEGMENTATION_CHROME_LOW_USER_ENGAGEMENT) {
    return GetLowEngagementDefaultModel();
  }
  return nullptr;
}

}  // namespace segmentation_platform
