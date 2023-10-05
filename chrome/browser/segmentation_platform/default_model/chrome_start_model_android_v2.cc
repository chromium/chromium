// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/segmentation_platform/default_model/chrome_start_model_android_v2.h"

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
constexpr int kModelVersion = 2;
constexpr SegmentId kChromeStartSegmentId =
    SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_CHROME_START_ANDROID_V2;
constexpr int64_t kChromeStartSignalStorageLength = 28;
constexpr int64_t kChromeStartMinSignalCollectionLength = 1;

// InputFeatures.
constexpr std::array<MetadataWriter::UMAFeature, 3> kChromeStartUMAFeatures = {
    MetadataWriter::UMAFeature::FromValueHistogram(
        "ContentSuggestions.Feed.EngagementType",
        7,
        proto::Aggregation::COUNT),
    MetadataWriter::UMAFeature::FromUserAction("MobileNewTabOpened", 7),
    MetadataWriter::UMAFeature::FromUserAction("MobileNTPMostVisited", 7),
};

}  // namespace

// static
std::unique_ptr<Config> ChromeStartModelV2::GetConfig() {
  if (!base::GetFieldTrialParamByFeatureAsBool(
          chrome::android::kStartSurfaceReturnTime, kDefaultModelEnabledParam,
          true)) {
    return nullptr;
  }
  auto config = std::make_unique<Config>();
  config->segmentation_key = kChromeStartAndroidV2SegmentationKey;
  config->segmentation_uma_name = kChromeStartAndroidV2UmaName;
  config->AddSegmentId(
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_CHROME_START_ANDROID_V2,
      std::make_unique<ChromeStartModelV2>());
  config->auto_execute_and_cache = true;
  return config;
}

ChromeStartModelV2::ChromeStartModelV2()
    : DefaultModelProvider(kChromeStartSegmentId) {}

std::unique_ptr<DefaultModelProvider::ModelConfig>
ChromeStartModelV2::GetModelConfig() {
  proto::SegmentationModelMetadata chrome_start_metadata;
  MetadataWriter writer(&chrome_start_metadata);
  writer.SetDefaultSegmentationMetadataConfig(
      kChromeStartMinSignalCollectionLength, kChromeStartSignalStorageLength);

  // Set OutputConfig.
  writer.AddOutputConfigForBinnedClassifier(
      /*bins=*/{{3600, kChromeStartAndroidV2Label1HourInMs},
                {7200, kChromeStartAndroidV2Label2HourInMs},
                {14400, kChromeStartAndroidV2Label4HourInMs},
                {28800, kChromeStartAndroidV2Label8HourInMs}},
      /*underflow_label=*/kChromeStartAndroidV2LabelNone);
  writer.AddPredictedResultTTLInOutputConfig(
      /*top_label_to_ttl_list=*/{}, /*default_ttl=*/30,
      /*time_unit=*/proto::TimeUnit::DAY);

  // Set features.
  writer.AddUmaFeatures(kChromeStartUMAFeatures.data(),
                        kChromeStartUMAFeatures.size());

  return std::make_unique<ModelConfig>(std::move(chrome_start_metadata),
                                       kModelVersion);
}

void ChromeStartModelV2::ExecuteModelWithInput(
    const ModelProvider::Request& inputs,
    ExecutionCallback callback) {
  // Invalid inputs.
  if (inputs.size() != kChromeStartUMAFeatures.size()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), absl::nullopt));
    return;
  }

  // Do not use the inputs, return the current default value defined.
  // Defined in TAB_SWITCHER_ON_RETURN_MS.
  // TODO(ssid): Consider getting the param value from field trials and use it
  // here instead.
  float return_time_in_seconds = 28800;  // 8 hours
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback),
                     ModelProvider::Response(1, return_time_in_seconds)));
}

}  // namespace segmentation_platform
