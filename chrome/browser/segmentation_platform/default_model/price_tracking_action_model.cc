// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/segmentation_platform/default_model/price_tracking_action_model.h"

#include "base/threading/sequenced_task_runner_handle.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/internal/proto/model_metadata.pb.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/model_provider.h"

namespace segmentation_platform {

namespace {
using proto::SegmentId;

// Default parameters for price tracking action model.
constexpr SegmentId kPriceTrackingSegmentId =
    SegmentId::OPTIMIZATION_TARGET_CONTEXTUAL_PAGE_ACTION_PRICE_TRACKING;

// TODO(shaktisahu): Are these required for on-demand model?
constexpr proto::TimeUnit kTimeUnit = proto::TimeUnit::DAY;
constexpr uint64_t kBucketDuration = 1;
constexpr int64_t kSignalStorageLength = 1;
constexpr int64_t kMinSignalCollectionLength = 1;
constexpr int64_t kResultTTL = 1;

// Discrete mapping parameters.
constexpr float kDiscreteMappingMinResult = 1;
constexpr int64_t kDiscreteMappingRank = 1;
constexpr std::pair<float, int> kDiscreteMappings[] = {
    {kDiscreteMappingMinResult, kDiscreteMappingRank}};

}  // namespace

PriceTrackingActionModel::PriceTrackingActionModel()
    : ModelProvider(kPriceTrackingSegmentId) {}

void PriceTrackingActionModel::InitAndFetchModel(
    const ModelUpdatedCallback& model_updated_callback) {
  proto::SegmentationModelMetadata metadata;
  MetadataWriter writer(&metadata);
  writer.SetSegmentationMetadataConfig(kTimeUnit, kBucketDuration,
                                       kSignalStorageLength,
                                       kMinSignalCollectionLength, kResultTTL);

  // Add price tracking custom input.
  writer.AddCustomInput(MetadataWriter::CustomInput{
      .tensor_length = 1,
      .fill_policy = proto::CustomInput_FillPolicy_PRICE_TRACKING_HINTS,
      .name = "price_tracking"});

  // Set discrete mapping.
  writer.AddDiscreteMappingEntries(kContextualPageActionsKey, kDiscreteMappings,
                                   1);

  constexpr int kModelVersion = 1;
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindRepeating(model_updated_callback, kPriceTrackingSegmentId,
                          std::move(metadata), kModelVersion));
}

void PriceTrackingActionModel::ExecuteModelWithInput(
    const std::vector<float>& inputs,
    ExecutionCallback callback) {
  // Invalid inputs.
  if (inputs.size() != 1) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), absl::nullopt));
    return;
  }

  // Input[0] is price tracking enabled, which is also the result.
  float result = inputs[0];

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

bool PriceTrackingActionModel::ModelAvailable() {
  return true;
}

}  // namespace segmentation_platform
