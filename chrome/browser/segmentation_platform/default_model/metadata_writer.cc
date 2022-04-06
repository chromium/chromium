// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/segmentation_platform/default_model/metadata_writer.h"

#include "base/metrics/metrics_hashes.h"
#include "components/segmentation_platform/internal/proto/model_metadata.pb.h"

namespace segmentation_platform {

MetadataWriter::MetadataWriter(proto::SegmentationModelMetadata* metadata)
    : metadata_(metadata) {}
MetadataWriter::~MetadataWriter() = default;

void MetadataWriter::AddUmaFeatures(const UMAFeature features[],
                                    size_t features_size) {
  for (size_t i = 0; i < features_size; i++) {
    const auto& feature = features[i];
    auto* input_feature = metadata_->add_input_features();
    proto::UMAFeature* uma_feature = input_feature->mutable_uma_feature();
    uma_feature->set_type(feature.signal_type);
    uma_feature->set_name(feature.name);
    uma_feature->set_name_hash(base::HashMetricName(feature.name));
    uma_feature->set_bucket_count(feature.bucket_count);
    uma_feature->set_tensor_length(feature.tensor_length);
    uma_feature->set_aggregation(feature.aggregation);

    for (size_t j = 0; j < feature.enum_ids_size; j++) {
      uma_feature->add_enum_ids(feature.accepted_enum_ids[j]);
    }
  }
}

void MetadataWriter::AddDiscreteMappingEntries(
    const std::string& key,
    const std::pair<float, int>* mappings,
    size_t mappings_size) {
  auto* discrete_mappings = metadata_->mutable_discrete_mappings();
  for (size_t i = 0; i < mappings_size; i++) {
    auto* discrete_mapping_entry = (*discrete_mappings)[key].add_entries();
    discrete_mapping_entry->set_min_result(mappings[i].first);
    discrete_mapping_entry->set_rank(mappings[i].second);
  }
}

void MetadataWriter::SetSegmentationMetadataConfig(
    proto::TimeUnit time_unit,
    uint64_t bucket_duration,
    int64_t signal_storage_length,
    int64_t min_signal_collection_length,
    int64_t result_time_to_live) {
  metadata_->set_time_unit(time_unit);
  metadata_->set_bucket_duration(bucket_duration);
  metadata_->set_signal_storage_length(signal_storage_length);
  metadata_->set_min_signal_collection_length(min_signal_collection_length);
  metadata_->set_result_time_to_live(result_time_to_live);
}

}  // namespace segmentation_platform
