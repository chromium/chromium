// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/segmentation_platform/default_model/chrome_start_model_android.h"

#include "base/metrics/field_trial_params.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/segmentation_platform/default_model/metadata_writer.h"
#include "chrome/browser/ui/android/start_surface/start_surface_android.h"
#include "components/segmentation_platform/internal/proto/model_metadata.pb.h"
#include "components/segmentation_platform/public/model_provider.h"

namespace segmentation_platform {

namespace {
using optimization_guide::proto::OptimizationTarget;

// Default parameters for Chrome Start model.
constexpr OptimizationTarget kChromeStartOptimizationTarget =
    OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_CHROME_START_ANDROID;
constexpr proto::TimeUnit kChromeStartTimeUnit = proto::TimeUnit::DAY;
constexpr uint64_t kChromeStartBucketDuration = 1;
constexpr int64_t kChromeStartSignalStorageLength = 28;
constexpr int64_t kChromeStartMinSignalCollectionLength = 1;
constexpr int64_t kChromeStartResultTTL = 1;

// Discrete mapping parameters.
constexpr char kChromeStartDiscreteMappingKey[] = "chrome_start_android";
constexpr float kChromeStartDiscreteMappingMinResult = 1;
constexpr int64_t kChromeStartDiscreteMappingRank = 1;
constexpr std::pair<float, int> kDiscreteMappings[] = {
    {kChromeStartDiscreteMappingMinResult, kChromeStartDiscreteMappingRank}};

// InputFeatures.
constexpr int32_t kProfileSigninStatusEnums[] = {0 /* All profiles syncing */,
                                                 2 /* Mixed sync status */};
constexpr MetadataWriter::UMAFeature kChromeStartUMAFeatures[] = {
    MetadataWriter::UMAFeature{
        .signal_type = proto::SignalType::USER_ACTION,
        .name = "ContentSuggestions.Feed.CardAction.Open",
        .bucket_count = 7,
        .tensor_length = 1,
        .aggregation = proto::Aggregation::COUNT,
        .enum_ids_size = 0},
    MetadataWriter::UMAFeature{
        .signal_type = proto::SignalType::USER_ACTION,
        .name = "ContentSuggestions.Feed.CardAction.OpenInNewIncognitoTab",
        .bucket_count = 7,
        .tensor_length = 1,
        .aggregation = proto::Aggregation::COUNT,
        .enum_ids_size = 0},
    MetadataWriter::UMAFeature{
        .signal_type = proto::SignalType::USER_ACTION,
        .name = "ContentSuggestions.Feed.CardAction.OpenInNewTab",
        .bucket_count = 7,
        .tensor_length = 1,
        .aggregation = proto::Aggregation::COUNT,
        .enum_ids_size = 0},
    MetadataWriter::UMAFeature{.signal_type = proto::SignalType::USER_ACTION,
                               .name = "MobileNTPMostVisited",
                               .bucket_count = 7,
                               .tensor_length = 1,
                               .aggregation = proto::Aggregation::COUNT,
                               .enum_ids_size = 0},
    MetadataWriter::UMAFeature{.signal_type = proto::SignalType::USER_ACTION,
                               .name = "MobileNewTabOpened",
                               .bucket_count = 7,
                               .tensor_length = 1,
                               .aggregation = proto::Aggregation::COUNT,
                               .enum_ids_size = 0},
    MetadataWriter::UMAFeature{.signal_type = proto::SignalType::USER_ACTION,
                               .name = "MobileMenuRecentTabs",
                               .bucket_count = 7,
                               .tensor_length = 1,
                               .aggregation = proto::Aggregation::COUNT,
                               .enum_ids_size = 0},
    MetadataWriter::UMAFeature{.signal_type = proto::SignalType::USER_ACTION,
                               .name = "MobileMenuHistory",
                               .bucket_count = 7,
                               .tensor_length = 1,
                               .aggregation = proto::Aggregation::COUNT,
                               .enum_ids_size = 0},
    MetadataWriter::UMAFeature{.signal_type = proto::SignalType::HISTOGRAM_ENUM,
                               .name = "UMA.ProfileSignInStatus",
                               .bucket_count = 7,
                               .tensor_length = 1,
                               .aggregation = proto::Aggregation::COUNT,
                               .enum_ids_size = 2,
                               .accepted_enum_ids = kProfileSigninStatusEnums}};

#define ARRAY_SIZE(ar) (sizeof(ar) / sizeof(ar[0]))

}  // namespace

ChromeStartModel::ChromeStartModel()
    : ModelProvider(kChromeStartOptimizationTarget) {}

void ChromeStartModel::InitAndFetchModel(
    const ModelUpdatedCallback& model_updated_callback) {
  proto::SegmentationModelMetadata chrome_start_metadata;
  MetadataWriter writer(&chrome_start_metadata);
  writer.SetSegmentationMetadataConfig(
      kChromeStartTimeUnit, kChromeStartBucketDuration,
      kChromeStartSignalStorageLength, kChromeStartMinSignalCollectionLength,
      kChromeStartResultTTL);

  // Set discrete mapping.
  writer.AddDiscreteMappingEntries(kChromeStartDiscreteMappingKey,
                                   kDiscreteMappings, 1);

  // Set features.
  writer.AddUmaFeatures(kChromeStartUMAFeatures,
                        ARRAY_SIZE(kChromeStartUMAFeatures));

  constexpr int kModelVersion = 1;
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindRepeating(
                     model_updated_callback, kChromeStartOptimizationTarget,
                     std::move(chrome_start_metadata), kModelVersion));
}

void ChromeStartModel::ExecuteModelWithInput(const std::vector<float>& inputs,
                                             ExecutionCallback callback) {
  // Invalid inputs.
  if (inputs.size() != ARRAY_SIZE(kChromeStartUMAFeatures)) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
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

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

bool ChromeStartModel::ModelAvailable() {
  return true;
}

}  // namespace segmentation_platform
