// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/segmentation_platform/default_model/feed_user_segment.h"

#include <array>

#include "base/metrics/field_trial_params.h"
#include "base/strings/strcat.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/internal/proto/model_metadata.pb.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/model_provider.h"

namespace segmentation_platform {

namespace {
using proto::SegmentId;

// Default parameters for Chrome Start model.
constexpr SegmentId kFeedUserSegmentId =
    SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_FEED_USER;
constexpr proto::TimeUnit kFeedUserTimeUnit = proto::TimeUnit::DAY;
constexpr uint64_t kFeedUserBucketDuration = 1;
constexpr int64_t kFeedUserSignalStorageLength = 28;
constexpr int64_t kFeedUserMinSignalCollectionLength = 7;
constexpr int64_t kFeedUserResultTTL = 1;

// Discrete mapping parameters.
constexpr char kFeedUserDiscreteMappingKey[] = "feed_user_segment";
constexpr float kFeedUserDiscreteMappingMinResult = 0.8;
constexpr int64_t kFeedUserDiscreteMappingRank = 1;
constexpr std::pair<float, int> kDiscreteMappings[] = {
    {kFeedUserDiscreteMappingMinResult, kFeedUserDiscreteMappingRank}};

#define RANK(x) static_cast<int>(FeedUserSubsegment::x)

static constexpr std::array<std::pair<float, /*FeedUserSubsegment*/ int>, 16>
    kFeedUserScoreToSubGroup = {{
        {1.0, RANK(kDeprecatedActiveOnFeedOnly)},
        {0.98, RANK(kNtpAndFeedEngaged)},
        {0.96, RANK(kNtpAndFeedEngagedSimple)},
        {0.94, RANK(kNtpAndFeedScrolled)},
        {0.92, RANK(kNtpAndFeedInteracted)},
        {0.90, RANK(kNoNtpAndFeedEngaged)},
        {0.88, RANK(kNoNtpAndFeedEngagedSimple)},
        {0.86, RANK(kNoNtpAndFeedScrolled)},
        {0.84, RANK(kNoNtpAndFeedInteracted)},
        {0.8, RANK(kDeprecatedActiveOnFeedAndNtpFeatures)},
        {0.7, RANK(kNoFeedAndNtpFeatures)},
        {0.5, RANK(kMvtOnly)},
        {0.4, RANK(kReturnToCurrentTabOnly)},
        {0.2, RANK(kUsedNtpWithoutModules)},
        {0.1, RANK(kNoNTPOrHomeOpened)},
        {0.0, RANK(kUnknown)},
    }};

// InputFeatures.

// The enum values are based on feed::FeedEngagementType.
constexpr std::array<int32_t, 1> kFeedEngagementEngaged{0};
constexpr std::array<int32_t, 1> kFeedEngagementSimple{1};
constexpr std::array<int32_t, 1> kFeedEngagementInteracted{2};
constexpr std::array<int32_t, 1> kFeedEngagementScrolled{4};

constexpr std::array<MetadataWriter::UMAFeature, 13> kFeedUserUMAFeatures = {
    MetadataWriter::UMAFeature::FromUserAction(
        "ContentSuggestions.Feed.CardAction.Open",
        14),
    MetadataWriter::UMAFeature::FromUserAction(
        "ContentSuggestions.Feed.CardAction.OpenInNewIncognitoTab",
        14),
    MetadataWriter::UMAFeature::FromUserAction(
        "ContentSuggestions.Feed.CardAction.OpenInNewTab",
        14),
    MetadataWriter::UMAFeature::FromUserAction("MobileNTPMostVisited", 14),
    MetadataWriter::UMAFeature::FromUserAction("MobileNewTabOpened", 14),
    MetadataWriter::UMAFeature::FromUserAction("Home", 14),
    MetadataWriter::UMAFeature::FromUserAction("MobileMenuRecentTabs", 14),
    MetadataWriter::UMAFeature::FromUserAction("MobileMenuHistory", 14),
    MetadataWriter::UMAFeature::FromUserAction("MobileTabReturnedToCurrentTab",
                                               14),
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "ContentSuggestions.Feed.EngagementType",
        14,
        kFeedEngagementEngaged.data(),
        kFeedEngagementEngaged.size()),
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "ContentSuggestions.Feed.EngagementType",
        14,
        kFeedEngagementSimple.data(),
        kFeedEngagementSimple.size()),
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "ContentSuggestions.Feed.EngagementType",
        14,
        kFeedEngagementInteracted.data(),
        kFeedEngagementInteracted.size()),
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "ContentSuggestions.Feed.EngagementType",
        14,
        kFeedEngagementScrolled.data(),
        kFeedEngagementScrolled.size()),
};

float GetScoreForSubsegment(FeedUserSubsegment subgroup) {
  for (const auto& score_and_type : kFeedUserScoreToSubGroup) {
    if (score_and_type.second == static_cast<int>(subgroup)) {
      return score_and_type.first;
    }
  }
  NOTREACHED();
  return 0;
}

// Any updates to these strings need to also update the field trials allowlist
// in go/segmentation-field-trials-map.
std::string FeedUserSubsegmentToString(FeedUserSubsegment feed_group) {
  switch (feed_group) {
    case FeedUserSubsegment::kUnknown:
      return "Unknown";
    case FeedUserSubsegment::kOther:
      return "Other";
    case FeedUserSubsegment::kDeprecatedActiveOnFeedOnly:
      return "ActiveOnFeedOnly";
    case FeedUserSubsegment::kDeprecatedActiveOnFeedAndNtpFeatures:
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
    case FeedUserSubsegment::kNtpAndFeedEngaged:
      return "NtpAndFeedEngaged";
    case FeedUserSubsegment::kNtpAndFeedEngagedSimple:
      return "NtpAndFeedEngagedSimple";
    case FeedUserSubsegment::kNtpAndFeedScrolled:
      return "NtpAndFeedScrolled";
    case FeedUserSubsegment::kNtpAndFeedInteracted:
      return "NtpAndFeedInteracted";
    case FeedUserSubsegment::kNoNtpAndFeedEngaged:
      return "NoNtpAndFeedEngaged";
    case FeedUserSubsegment::kNoNtpAndFeedEngagedSimple:
      return "NoNtpAndFeedEngagedSimple";
    case FeedUserSubsegment::kNoNtpAndFeedScrolled:
      return "NoNtpAndFeedScrolled";
    case FeedUserSubsegment::kNoNtpAndFeedInteracted:
      return "NoNtpAndFeedInteracted";
  }
}

}  // namespace

FeedUserSegment::FeedUserSegment() : ModelProvider(kFeedUserSegmentId) {}

absl::optional<std::string> FeedUserSegment::GetSubsegmentName(
    int subsegment_rank) {
  DCHECK(RANK(kUnknown) <= subsegment_rank &&
         subsegment_rank <= RANK(kMaxValue));
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
  writer.AddUmaFeatures(kFeedUserUMAFeatures.data(),
                        kFeedUserUMAFeatures.size());

  constexpr int kModelVersion = 1;
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindRepeating(model_updated_callback, kFeedUserSegmentId,
                          std::move(chrome_start_metadata), kModelVersion));
}

void FeedUserSegment::ExecuteModelWithInput(const std::vector<float>& inputs,
                                            ExecutionCallback callback) {
  // Invalid inputs.
  if (inputs.size() != kFeedUserUMAFeatures.size()) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), absl::nullopt));
    return;
  }

  FeedUserSubsegment segment = FeedUserSubsegment::kNoNTPOrHomeOpened;

  const bool feed_engaged = inputs[9] >= 2;
  const bool feed_engaged_simple = inputs[10] >= 2;
  const bool feed_interacted = inputs[11] >= 2;
  const bool feed_scrolled = inputs[12] >= 2;

  const bool mv_tiles_used = inputs[3] >= 2;
  const bool return_to_tab_used = inputs[8] >= 2;
  const bool ntp_used = mv_tiles_used || return_to_tab_used;
  const bool home_or_ntp_opened = (inputs[4] + inputs[5]) >= 4;

  if (feed_engaged) {
    segment = ntp_used ? FeedUserSubsegment::kNtpAndFeedEngaged
                       : FeedUserSubsegment::kNoNtpAndFeedEngaged;
  } else if (feed_engaged_simple) {
    segment = ntp_used ? FeedUserSubsegment::kNtpAndFeedEngagedSimple
                       : FeedUserSubsegment::kNoNtpAndFeedEngagedSimple;
  } else if (feed_interacted) {
    segment = ntp_used ? FeedUserSubsegment::kNtpAndFeedInteracted
                       : FeedUserSubsegment::kNoNtpAndFeedInteracted;
  } else if (feed_scrolled) {
    segment = ntp_used ? FeedUserSubsegment::kNtpAndFeedScrolled
                       : FeedUserSubsegment::kNoNtpAndFeedScrolled;
  } else if (home_or_ntp_opened) {
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
