// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/segmentation_platform/default_model/query_tiles_model.h"

#include "base/metrics/metrics_hashes.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/segmentation_platform/internal/proto/model_metadata.pb.h"
#include "components/segmentation_platform/public/model_provider.h"

namespace segmentation_platform {

namespace {
using optimization_guide::proto::OptimizationTarget;

struct UMAFeature {
  const proto::SignalType signal_type{proto::SignalType::UNKNOWN_SIGNAL_TYPE};
  const char* name{nullptr};
  const uint64_t bucket_count{0};
  const uint64_t tensor_length{0};
  const proto::Aggregation aggregation{proto::Aggregation::UNKNOWN};
  const size_t enum_ids_size{0};
  const int32_t accepted_enum_ids[];
};

// Default parameters for query tiles model.
constexpr OptimizationTarget kQueryTilesOptimizationTarget =
    OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_QUERY_TILES;
constexpr proto::TimeUnit kQueryTilesTimeUnit = proto::TimeUnit::DAY;
constexpr uint64_t kQueryTilesBucketDuration = 1;
constexpr int64_t kQueryTilesSignalStorageLength = 28;
constexpr int64_t kQueryTilesMinSignalCollectionLength = 7;
constexpr int64_t kQueryTilesResultTTL = 1;
constexpr int64_t kMvThreshold = 1;

// Discrete mapping parameters.
constexpr char kQueryTilesDiscreteMappingKey[] = "query_tiles";
constexpr float kQueryTilesDiscreteMappingMinResult = 1;
constexpr int64_t kQueryTilesDiscreteMappingRank = 1;
constexpr std::pair<float, int> kDiscreteMappings[] = {
    {kQueryTilesDiscreteMappingMinResult, kQueryTilesDiscreteMappingRank}};

// InputFeatures.
constexpr UMAFeature kQueryTilesUMAFeatures[2] = {
    UMAFeature{.signal_type = proto::SignalType::USER_ACTION,
               .name = "MobileNTPMostVisited",
               .bucket_count = 7,
               .tensor_length = 1,
               .aggregation = proto::Aggregation::COUNT,
               .enum_ids_size = 0},
    UMAFeature{.signal_type = proto::SignalType::USER_ACTION,
               .name = "Search.QueryTiles.NTP.Tile.Clicked",
               .bucket_count = 7,
               .tensor_length = 1,
               .aggregation = proto::Aggregation::COUNT,
               .enum_ids_size = 0}};

void AddUmaFeature(proto::SegmentationModelMetadata* metadata,
                   const UMAFeature features[],
                   size_t features_size) {
  for (size_t i = 0; i < features_size; i++) {
    const auto& feature = features[i];
    auto* input_feature = metadata->add_input_features();
    proto::UMAFeature* uma_feature = input_feature->mutable_uma_feature();
    uma_feature->set_type(feature.signal_type);
    uma_feature->set_name(feature.name);
    uma_feature->set_name_hash(base::HashMetricName(feature.name));
    uma_feature->set_bucket_count(feature.bucket_count);
    uma_feature->set_tensor_length(feature.tensor_length);
    uma_feature->set_aggregation(feature.aggregation);

    for (size_t i = 0; i < feature.enum_ids_size; i++) {
      uma_feature->add_enum_ids(feature.accepted_enum_ids[i]);
    }
  }
}

void AddDiscreteMappingEntry(proto::SegmentationModelMetadata* metadata,
                             const std::string& key,
                             const std::pair<float, int>* mappings,
                             size_t mappings_size) {
  auto* discrete_mappings = metadata->mutable_discrete_mappings();
  for (size_t i = 0; i < mappings_size; i++) {
    auto* discrete_mapping_entry = (*discrete_mappings)[key].add_entries();
    discrete_mapping_entry->set_min_result(mappings[i].first);
    discrete_mapping_entry->set_rank(mappings[i].second);
  }
}

void AddSegmentationMetadataConfig(proto::SegmentationModelMetadata* metadata,
                                   proto::TimeUnit time_unit,
                                   uint64_t bucket_duration,
                                   int64_t signal_storage_length,
                                   int64_t min_signal_collection_length,
                                   int64_t result_time_to_live) {
  metadata->set_time_unit(time_unit);
  metadata->set_bucket_duration(bucket_duration);
  metadata->set_signal_storage_length(signal_storage_length);
  metadata->set_min_signal_collection_length(min_signal_collection_length);
  metadata->set_result_time_to_live(result_time_to_live);
}

}  // namespace

QueryTilesModel::QueryTilesModel()
    : ModelProvider(kQueryTilesOptimizationTarget) {}

void QueryTilesModel::InitAndFetchModel(
    const ModelUpdatedCallback& model_updated_callback) {
  proto::SegmentationModelMetadata query_tiles_metadata;
  AddSegmentationMetadataConfig(
      &query_tiles_metadata, kQueryTilesTimeUnit, kQueryTilesBucketDuration,
      kQueryTilesSignalStorageLength, kQueryTilesMinSignalCollectionLength,
      kQueryTilesResultTTL);

  // Set discrete mapping.
  AddDiscreteMappingEntry(&query_tiles_metadata, kQueryTilesDiscreteMappingKey,
                          kDiscreteMappings, 1);

  // Set features.
  AddUmaFeature(
      &query_tiles_metadata, kQueryTilesUMAFeatures,
      sizeof(kQueryTilesUMAFeatures) / sizeof(kQueryTilesUMAFeatures[0]));

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindRepeating(model_updated_callback, kQueryTilesOptimizationTarget,
                          std::move(query_tiles_metadata), 2));
}

void QueryTilesModel::ExecuteModelWithInput(const std::vector<float>& inputs,
                                            ExecutionCallback callback) {
  // Invalid inputs.
  if (inputs.size() != 2) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), absl::nullopt));
    return;
  }

  const int mv_clicks = inputs[0];
  const int query_tiles_clicks = inputs[1];
  float result = 0;

  // If mv clicks are below the threshold or below the query tiles clicks, query
  // tiles should be enabled.
  if (mv_clicks <= kMvThreshold || mv_clicks <= query_tiles_clicks) {
    result = 1;  // Enable query tiles;
  }

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

bool QueryTilesModel::ModelAvailable() {
  return true;
}

}  // namespace segmentation_platform
