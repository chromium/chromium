// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/segmentation_platform/default_model/feed_user_segment.h"

#include "base/metrics/field_trial_params.h"
#include "base/strings/strcat.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/segmentation_platform/default_model/metadata_writer.h"
#include "components/segmentation_platform/internal/proto/model_metadata.pb.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/model_provider.h"

namespace segmentation_platform {

namespace {
using optimization_guide::proto::OptimizationTarget;

// Default parameters for Chrome Start model.
constexpr OptimizationTarget kFeedUserOptimizationTarget =
    OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_FEED_USER;
constexpr proto::TimeUnit kFeedUserTimeUnit = proto::TimeUnit::DAY;
constexpr uint64_t kFeedUserBucketDuration = 1;
constexpr int64_t kFeedUserSignalStorageLength = 28;
constexpr int64_t kFeedUserMinSignalCollectionLength = 7;
constexpr int64_t kFeedUserResultTTL = 1;

// Discrete mapping parameters.
constexpr char kFeedUserDiscreteMappingKey[] = "feed_user_segment";
constexpr float kFeedUserDiscreteMappingMinResult = 1;
constexpr int64_t kFeedUserDiscreteMappingRank = 1;
constexpr std::pair<float, int> kDiscreteMappings[] = {
    {kFeedUserDiscreteMappingMinResult, kFeedUserDiscreteMappingRank}};

static constexpr std::array<std::pair<float, /*FeedUserSubsegment*/ int>, 8>
    kFeedUserScoreToSubGroup = {{
        {1.0, static_cast<int>(FeedUserSubsegment::kActiveOnFeedOnly)},
        {0.8,
         static_cast<int>(FeedUserSubsegment::kActiveOnFeedAndNtpFeatures)},
        {0.7, static_cast<int>(FeedUserSubsegment::kNoFeedAndNtpFeatures)},
        {0.5, static_cast<int>(FeedUserSubsegment::kMvtOnly)},
        {0.4, static_cast<int>(FeedUserSubsegment::kReturnToCurrentTabOnly)},
        {0.2, static_cast<int>(FeedUserSubsegment::kUsedNtpWithoutModules)},
        {0.1, static_cast<int>(FeedUserSubsegment::kNoNTPOrHomeOpened)},
        {0.0, static_cast<int>(FeedUserSubsegment::kUnknown)},
    }};

// InputFeatures.
constexpr MetadataWriter::UMAFeature kFeedUserUMAFeatures[] = {
    MetadataWriter::UMAFeature{
        .signal_type = proto::SignalType::USER_ACTION,
        .name = "ContentSuggestions.Feed.CardAction.Open",
        .bucket_count = 14,
        .tensor_length = 1,
        .aggregation = proto::Aggregation::COUNT,
        .enum_ids_size = 0},
    MetadataWriter::UMAFeature{
        .signal_type = proto::SignalType::USER_ACTION,
        .name = "ContentSuggestions.Feed.CardAction.OpenInNewIncognitoTab",
        .bucket_count = 14,
        .tensor_length = 1,
        .aggregation = proto::Aggregation::COUNT,
        .enum_ids_size = 0},
    MetadataWriter::UMAFeature{
        .signal_type = proto::SignalType::USER_ACTION,
        .name = "ContentSuggestions.Feed.CardAction.OpenInNewTab",
        .bucket_count = 14,
        .tensor_length = 1,
        .aggregation = proto::Aggregation::COUNT,
        .enum_ids_size = 0},
    MetadataWriter::UMAFeature{.signal_type = proto::SignalType::USER_ACTION,
                               .name = "MobileNTPMostVisited",
                               .bucket_count = 14,
                               .tensor_length = 1,
                               .aggregation = proto::Aggregation::COUNT,
                               .enum_ids_size = 0},
    MetadataWriter::UMAFeature{.signal_type = proto::SignalType::USER_ACTION,
                               .name = "MobileNewTabOpened",
                               .bucket_count = 14,
                               .tensor_length = 1,
                               .aggregation = proto::Aggregation::COUNT,
                               .enum_ids_size = 0},
    MetadataWriter::UMAFeature{.signal_type = proto::SignalType::USER_ACTION,
                               .name = "Home",
                               .bucket_count = 14,
                               .tensor_length = 1,
                               .aggregation = proto::Aggregation::COUNT,
                               .enum_ids_size = 0},
    MetadataWriter::UMAFeature{.signal_type = proto::SignalType::USER_ACTION,
                               .name = "MobileMenuRecentTabs",
                               .bucket_count = 14,
                               .tensor_length = 1,
                               .aggregation = proto::Aggregation::COUNT,
                               .enum_ids_size = 0},
    MetadataWriter::UMAFeature{.signal_type = proto::SignalType::USER_ACTION,
                               .name = "MobileMenuHistory",
                               .bucket_count = 14,
                               .tensor_length = 1,
                               .aggregation = proto::Aggregation::COUNT,
                               .enum_ids_size = 0},
    MetadataWriter::UMAFeature{.signal_type = proto::SignalType::USER_ACTION,
                               .name = "MobileTabReturnedToCurrentTab",
                               .bucket_count = 14,
                               .tensor_length = 1,
                               .aggregation = proto::Aggregation::COUNT,
                               .enum_ids_size = 0}};

#define ARRAY_SIZE(ar) (sizeof(ar) / sizeof(ar[0]))

float GetScoreForSubsegment(FeedUserSubsegment subgroup) {
  for (const auto& score_and_type : kFeedUserScoreToSubGroup) {
    if (score_and_type.second == static_cast<int>(subgroup)) {
      return score_and_type.first;
    }
  }
  NOTREACHED();
  return 0;
}

std::string FeedUserSubsegmentToString(FeedUserSubsegment feed_group) {
  switch (feed_group) {
    case FeedUserSubsegment::kUnknown:
      return "Unknown";
    case FeedUserSubsegment::kOther:
      return "Other";
    case FeedUserSubsegment::kActiveOnFeedOnly:
      return "ActiveOnFeedOnly";
    case FeedUserSubsegment::kActiveOnFeedAndNtpFeatures:
      return "ActiveOnFeedAndNtpFeatures";
    case FeedUserSubsegment::kNoFeedAndNtpFeatures:
      return "NoFeedAndNtpFeatures";
    case FeedUserSubsegment::kMvtOnly:
      return "MvtOnly";
    case FeedUserSubsegment::kReturnToCurrentTabOnly:
      return "ReturnToCurrentTabOnly";
    case FeedUserSubsegment::kUsedNtpWithoutModules:
      return "UsedNtpWithoutModules";
    case FeedUserSubsegment::kNoNTPOrHomeOpened:
      return "NoNTPOrHomeOpened";
  }
}

}  // namespace

FeedUserSegment::FeedUserSegment()
    : ModelProvider(kFeedUserOptimizationTarget) {}

absl::optional<std::string> FeedUserSegment::GetSubsegmentName(
    int subsegment_rank) {
  DCHECK(static_cast<int>(FeedUserSubsegment::kUnknown) <= subsegment_rank &&
         subsegment_rank <= static_cast<int>(FeedUserSubsegment::kMaxValue));
  FeedUserSubsegment subgroup =
      static_cast<FeedUserSubsegment>(subsegment_rank);
  return FeedUserSubsegmentToString(subgroup);
}

void FeedUserSegment::InitAndFetchModel(
    const ModelUpdatedCallback& model_updated_callback) {
  proto::SegmentationModelMetadata chrome_start_metadata;
  MetadataWriter writer(&chrome_start_metadata);
  writer.SetSegmentationMetadataConfig(
      kFeedUserTimeUnit, kFeedUserBucketDuration, kFeedUserSignalStorageLength,
      kFeedUserMinSignalCollectionLength, kFeedUserResultTTL);

  // Set discrete mapping.
  writer.AddDiscreteMappingEntries(kFeedUserDiscreteMappingKey,
                                   kDiscreteMappings, 1);

  // Add subsegment mapping.
  writer.AddDiscreteMappingEntries(
      base::StrCat(
          {kFeedUserDiscreteMappingKey, kSubsegmentDiscreteMappingSuffix}),
      kFeedUserScoreToSubGroup.data(), kFeedUserScoreToSubGroup.size());

  // Set features.
  writer.AddUmaFeatures(kFeedUserUMAFeatures, ARRAY_SIZE(kFeedUserUMAFeatures));

  constexpr int kModelVersion = 1;
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindRepeating(model_updated_callback, kFeedUserOptimizationTarget,
                          std::move(chrome_start_metadata), kModelVersion));
}

void FeedUserSegment::ExecuteModelWithInput(const std::vector<float>& inputs,
                                            ExecutionCallback callback) {
  // Invalid inputs.
  if (inputs.size() != ARRAY_SIZE(kFeedUserUMAFeatures)) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), absl::nullopt));
    return;
  }

  FeedUserSubsegment segment = FeedUserSubsegment::kNoNTPOrHomeOpened;

  const bool feed_opened = (inputs[0] + inputs[1] + inputs[2]) >= 2;
  const bool mv_tiles_used = inputs[3] >= 2;
  const bool return_to_tab_used = inputs[8] >= 2;
  const bool home_or_ntp_used = (inputs[4] + inputs[5]) >= 4;

  if (feed_opened) {
    if (mv_tiles_used || return_to_tab_used) {
      segment = FeedUserSubsegment::kActiveOnFeedAndNtpFeatures;
    } else {
      segment = FeedUserSubsegment::kActiveOnFeedOnly;
    }
  } else if (home_or_ntp_used) {
    if (mv_tiles_used && return_to_tab_used) {
      segment = FeedUserSubsegment::kNoFeedAndNtpFeatures;
    } else if (mv_tiles_used) {
      segment = FeedUserSubsegment::kMvtOnly;
    } else if (return_to_tab_used) {
      segment = FeedUserSubsegment::kReturnToCurrentTabOnly;
    } else {
      segment = segment = FeedUserSubsegment::kUsedNtpWithoutModules;
    }
  } else {
    segment = segment = FeedUserSubsegment::kNoNTPOrHomeOpened;
  }

  float result = GetScoreForSubsegment(segment);
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

bool FeedUserSegment::ModelAvailable() {
  return true;
}

}  // namespace segmentation_platform
