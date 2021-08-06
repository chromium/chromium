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
#include "components/segmentation_platform/public/segmentation_platform_service.h"

#if defined(OS_ANDROID)
#include "chrome/browser/flags/android/chrome_feature_list.h"
#endif

using optimization_guide::proto::OptimizationTarget;

namespace segmentation_platform {

namespace {

#if defined(OS_ANDROID)
// Default TTL for segment selection.
constexpr int kDefaultSegmentSelectionTTLDays = 28;
#endif

// The key to be used for adaptive toolbar feature.
const char kAdaptiveToolbarSegmentationKey[] = "adaptive_toolbar";

}  // namespace

std::unique_ptr<Config> GetSegmentationPlatformConfig() {
  auto config = std::make_unique<Config>();
  config->segmentation_key = kAdaptiveToolbarSegmentationKey;

#if defined(OS_ANDROID)
  int segment_selection_ttl_days = base::GetFieldTrialParamByFeatureAsInt(
      chrome::android::kAdaptiveButtonInTopToolbarCustomizationV2,
      "segment_selection_ttl_days", kDefaultSegmentSelectionTTLDays);
  config->segment_selection_ttl =
      base::TimeDelta::FromDays(segment_selection_ttl_days);
#endif

  // A hardcoded list of segment IDs known to the segmentation platform.
  config->segment_ids = {
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB,
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_SHARE,
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_VOICE,
  };

  return config;
}

}  // namespace segmentation_platform
