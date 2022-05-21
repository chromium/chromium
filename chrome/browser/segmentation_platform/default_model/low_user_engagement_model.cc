// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/segmentation_platform/default_model/low_user_engagement_model.h"

#include <array>

#include "base/logging.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"

namespace segmentation_platform {

namespace {

using proto::SegmentId;

// Default parameters for Chrome Start model.
constexpr SegmentId kChromeStartSegmentId =
    SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_CHROME_LOW_USER_ENGAGEMENT;
constexpr proto::TimeUnit kChromeStartTimeUnit = proto::TimeUnit::DAY;
constexpr uint64_t kChromeStartBucketDuration = 1;
constexpr int64_t kChromeStartSignalStorageLength = 28;
constexpr int64_t kChromeStartMinSignalCollectionLength = 28;
constexpr int64_t kChromeStartResultTTL = 1;

// Discrete mapping parameters.
constexpr char kChromeStartDiscreteMappingKey[] = "chrome_low_user_engagement";
constexpr float kChromeStartDiscreteMappingMinResult = 1;
constexpr int64_t kChromeStartDiscreteMappingRank = 1;
constexpr std::pair<float, int> kDiscreteMappings[] = {
    {kChromeStartDiscreteMappingMinResult, kChromeStartDiscreteMappingRank}};

// InputFeatures.
constexpr std::array<MetadataWriter::UMAFeature, 1> kChromeStartUMAFeatures = {
    MetadataWriter::UMAFeature{
        .signal_type = proto::SignalType::HISTOGRAM_VALUE,
        .name = "Session.TotalDuration",
        .bucket_count = 28,
        .tensor_length = 28,
        .aggregation = proto::Aggregation::BUCKETED_COUNT,
        .enum_ids_size = 0}};

}  // namespace

LowUserEngagementModel::LowUserEngagementModel()
    : ModelProvider(kChromeStartSegmentId) {}

void LowUserEngagementModel::InitAndFetchModel(
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
  writer.AddUmaFeatures(kChromeStartUMAFeatures.data(),
                        kChromeStartUMAFeatures.size());

  constexpr int kModelVersion = 1;
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindRepeating(model_updated_callback, kChromeStartSegmentId,
                          std::move(chrome_start_metadata), kModelVersion));
}

void LowUserEngagementModel::ExecuteModelWithInput(
    const std::vector<float>& inputs,
    ExecutionCallback callback) {
  // Invalid inputs.
  if (inputs.size() != 28) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), absl::nullopt));
    return;
  }

  float result = 0;
  bool weeks[4]{};
  for (unsigned i = 0; i < 28; ++i) {
    int week_idx = i / 7;
    weeks[week_idx] = weeks[week_idx] || inputs[i];
  }
  if (!weeks[0] || !weeks[1] || !weeks[2] || !weeks[3])
    result = 1;

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

bool LowUserEngagementModel::ModelAvailable() {
  return true;
}

}  // namespace segmentation_platform
