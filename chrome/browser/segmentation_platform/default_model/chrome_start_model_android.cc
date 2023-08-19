// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/segmentation_platform/default_model/chrome_start_model_android.h"

#include <array>

#include "base/metrics/field_trial_params.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/ui/android/start_surface/start_surface_android.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace segmentation_platform {

namespace {
using proto::SegmentId;

// Default parameters for Chrome Start model.
constexpr SegmentId kChromeStartSegmentId =
    SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_CHROME_START_ANDROID;
constexpr int64_t kChromeStartSignalStorageLength = 28;
constexpr int64_t kChromeStartMinSignalCollectionLength = 1;

constexpr int kChromeStartDefaultSelectionTTLDays = 30;
constexpr int kChromeStartDefaultUnknownTTLDays = 7;

// InputFeatures.
constexpr int32_t kProfileSigninStatusEnums[] = {0 /* All profiles syncing */,
                                                 2 /* Mixed sync status */};
constexpr std::array<MetadataWriter::UMAFeature, 8> kChromeStartUMAFeatures = {
    MetadataWriter::UMAFeature::FromUserAction(
        "ContentSuggestions.Feed.CardAction.Open",
        7),
    MetadataWriter::UMAFeature::FromUserAction(
        "ContentSuggestions.Feed.CardAction.OpenInNewIncognitoTab",
        7),
    MetadataWriter::UMAFeature::FromUserAction(
        "ContentSuggestions.Feed.CardAction.OpenInNewTab",
        7),
    MetadataWriter::UMAFeature::FromUserAction("MobileNTPMostVisited", 7),
    MetadataWriter::UMAFeature::FromUserAction("MobileNewTabOpened", 7),
    MetadataWriter::UMAFeature::FromUserAction("MobileMenuRecentTabs", 7),
    MetadataWriter::UMAFeature::FromUserAction("MobileMenuHistory", 7),
    MetadataWriter::UMAFeature::FromEnumHistogram("UMA.ProfileSignInStatus",
                                                  7,
                                                  kProfileSigninStatusEnums,
                                                  2)};

std::unique_ptr<DefaultModelProvider> GetChromeStartAndroidModel() {
  if (!base::GetFieldTrialParamByFeatureAsBool(
          chrome::android::kStartSurfaceAndroid, kDefaultModelEnabledParam,
          false)) {
    return nullptr;
  }
  return std::make_unique<ChromeStartModel>();
}

}  // namespace

// static
std::unique_ptr<Config> ChromeStartModel::GetConfig() {
  if (!IsStartSurfaceBehaviouralTargetingEnabled()) {
    return nullptr;
  }

  auto config = std::make_unique<Config>();
  config->segmentation_key = kChromeStartAndroidSegmentationKey;
  config->segmentation_uma_name = kChromeStartAndroidUmaName;
  config->AddSegmentId(
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_CHROME_START_ANDROID,
      GetChromeStartAndroidModel());
  config->auto_execute_and_cache = true;

  int segment_selection_ttl_days = base::GetFieldTrialParamByFeatureAsInt(
      chrome::android::kStartSurfaceAndroid,
      kVariationsParamNameSegmentSelectionTTLDays,
      kChromeStartDefaultSelectionTTLDays);
  int unknown_selection_ttl_days = base::GetFieldTrialParamByFeatureAsInt(
      chrome::android::kStartSurfaceAndroid,
      "segment_unknown_selection_ttl_days", kChromeStartDefaultUnknownTTLDays);
  config->segment_selection_ttl = base::Days(segment_selection_ttl_days);
  config->unknown_selection_ttl = base::Days(unknown_selection_ttl_days);
  config->is_boolean_segment = true;

  return config;
}

ChromeStartModel::ChromeStartModel()
    : DefaultModelProvider(kChromeStartSegmentId) {}

std::unique_ptr<DefaultModelProvider::ModelConfig>
ChromeStartModel::GetModelConfig() {
  proto::SegmentationModelMetadata chrome_start_metadata;
  MetadataWriter writer(&chrome_start_metadata);
  writer.SetDefaultSegmentationMetadataConfig(
      kChromeStartMinSignalCollectionLength, kChromeStartSignalStorageLength);

  // Set discrete mapping.
  writer.AddBooleanSegmentDiscreteMapping(kChromeStartAndroidSegmentationKey);

  // Set features.
  writer.AddUmaFeatures(kChromeStartUMAFeatures.data(),
                        kChromeStartUMAFeatures.size());

  constexpr int kModelVersion = 1;
  return std::make_unique<ModelConfig>(std::move(chrome_start_metadata),
                                       kModelVersion);
}

void ChromeStartModel::ExecuteModelWithInput(
    const ModelProvider::Request& inputs,
    ExecutionCallback callback) {
  // Invalid inputs.
  if (inputs.size() != kChromeStartUMAFeatures.size()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), absl::nullopt));
    return;
  }

  const int mv_clicks = inputs[3];
  float result = 0;
  int threshold = 1;

  // StartSurfaceConfiguration.USER_CLICK_THRESHOLD_PARAM
  threshold = base::GetFieldTrialParamByFeatureAsInt(
      chrome::android::kStartSurfaceAndroid, "user_clicks_threshold", 1);

  if (mv_clicks >= threshold) {
    result = 1;  // Enable Start
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), ModelProvider::Response(1, result)));
}

}  // namespace segmentation_platform
