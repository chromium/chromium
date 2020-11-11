// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_share_metrics_logger.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_prefs.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. If entries are added, kMaxValue should
// be updated.
enum class NearbyShareEnabledState {
  kEnabledAndOnboarded = 0,
  kEnabledAndNotOnboarded = 1,
  kDisabledAndOnboarded = 2,
  kDisabledAndNotOnboarded = 3,
  kDisallowedByPolicy = 4,
  kMaxValue = kDisallowedByPolicy
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. If entries are added, kMaxValue should
// be updated.
enum class TransferNotCompletedReason {
  kUnknown = 0,
  kAwaitingRemoteAcceptanceFailed = 1,
  kFailed = 2,
  kRejected = 3,
  kCancelled = 4,
  kTimedOut = 5,
  kMediaUnavailable = 6,
  kNotEnoughSpace = 7,
  kUnsupportedAttachmentType = 8,
  kMaxValue = kUnsupportedAttachmentType
};

TransferNotCompletedReason TransferMetadataStatusToTransferNotCompletedReason(
    TransferMetadata::Status status) {
  switch (status) {
    case TransferMetadata::Status::kAwaitingRemoteAcceptanceFailed:
      return TransferNotCompletedReason::kAwaitingRemoteAcceptanceFailed;
    case TransferMetadata::Status::kFailed:
      return TransferNotCompletedReason::kFailed;
    case TransferMetadata::Status::kRejected:
      return TransferNotCompletedReason::kRejected;
    case TransferMetadata::Status::kCancelled:
      return TransferNotCompletedReason::kCancelled;
    case TransferMetadata::Status::kTimedOut:
      return TransferNotCompletedReason::kTimedOut;
    case TransferMetadata::Status::kMediaUnavailable:
      return TransferNotCompletedReason::kMediaUnavailable;
    case TransferMetadata::Status::kNotEnoughSpace:
      return TransferNotCompletedReason::kNotEnoughSpace;
    case TransferMetadata::Status::kUnsupportedAttachmentType:
      return TransferNotCompletedReason::kUnsupportedAttachmentType;
    case TransferMetadata::Status::kUnknown:
    case TransferMetadata::Status::kConnecting:
    case TransferMetadata::Status::kAwaitingLocalConfirmation:
    case TransferMetadata::Status::kAwaitingRemoteAcceptance:
    case TransferMetadata::Status::kInProgress:
    case TransferMetadata::Status::kComplete:
    case TransferMetadata::Status::kMediaDownloading:
    case TransferMetadata::Status::kExternalProviderLaunched:
      NOTREACHED();
      return TransferNotCompletedReason::kUnknown;
  }
}

}  // namespace

void RecordNearbyShareEnabledMetric(const PrefService* pref_service) {
  NearbyShareEnabledState state;

  bool is_managed =
      pref_service->IsManagedPreference(prefs::kNearbySharingEnabledPrefName);
  bool is_enabled =
      pref_service->GetBoolean(prefs::kNearbySharingEnabledPrefName);
  bool is_onboarded =
      pref_service->GetBoolean(prefs::kNearbySharingOnboardingCompletePrefName);

  if (is_enabled) {
    state = is_onboarded ? NearbyShareEnabledState::kEnabledAndOnboarded
                         : NearbyShareEnabledState::kEnabledAndNotOnboarded;
  } else if (is_managed) {
    state = NearbyShareEnabledState::kDisallowedByPolicy;
  } else {  // !is_enabled && !is_managed
    state = is_onboarded ? NearbyShareEnabledState::kDisabledAndOnboarded
                         : NearbyShareEnabledState::kDisabledAndNotOnboarded;
  }

  base::UmaHistogramEnumeration("Nearby.Share.Enabled", state);
}

void RecordNearbyShareTransferCompletionStatusMetric(
    bool is_incoming,
    nearby_share::mojom::ShareTargetType type,
    TransferMetadata::Status status) {
  DCHECK(TransferMetadata::IsFinalStatus(status));

  const std::string kPrefix = "Nearby.Share.Transfer.CompletionStatus";
  std::string send_or_receive = is_incoming ? ".Receive" : ".Send";
  std::string share_target_type;
  switch (type) {
    case nearby_share::mojom::ShareTargetType::kUnknown:
      share_target_type = ".Unknown";
      break;
    case nearby_share::mojom::ShareTargetType::kPhone:
      share_target_type = ".Phone";
      break;
    case nearby_share::mojom::ShareTargetType::kTablet:
      share_target_type = ".Tablet";
      break;
    case nearby_share::mojom::ShareTargetType::kLaptop:
      share_target_type = ".Laptop";
      break;
  }

  bool is_complete = status == TransferMetadata::Status::kComplete;
  base::UmaHistogramBoolean(kPrefix, is_complete);
  base::UmaHistogramBoolean(kPrefix + send_or_receive, is_complete);
  base::UmaHistogramBoolean(kPrefix + share_target_type, is_complete);
  base::UmaHistogramBoolean(kPrefix + send_or_receive + share_target_type,
                            is_complete);
  if (!is_complete) {
    const std::string kReasonInfix = ".NotCompletedReason";
    TransferNotCompletedReason reason =
        TransferMetadataStatusToTransferNotCompletedReason(status);
    base::UmaHistogramEnumeration(kPrefix + kReasonInfix, reason);
    base::UmaHistogramEnumeration(kPrefix + kReasonInfix + send_or_receive,
                                  reason);
    base::UmaHistogramEnumeration(kPrefix + kReasonInfix + share_target_type,
                                  reason);
    base::UmaHistogramEnumeration(
        kPrefix + kReasonInfix + send_or_receive + share_target_type, reason);
  }
}
