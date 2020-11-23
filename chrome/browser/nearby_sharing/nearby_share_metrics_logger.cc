// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_share_metrics_logger.h"

#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_prefs.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"

namespace {

const char kStartAdvertisingResultMetricPrefix[] =
    "Nearby.Share.StartAdvertising.Result";
const char kStartAdvertisingResultFailureReasonMetricPrefix[] =
    "Nearby.Share.StartAdvertising.Result.FailureReason";
const char kTransferMetricPrefix[] = "Nearby.Share.Transfer";
const size_t kBytesPerKilobyte = 1024;

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

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. If entries are added, kMaxValue should
// be updated.
enum class StartAdvertisingFailureReason {
  kUnknown = 0,
  kError = 1,
  kOutOfOrderApiCall = 2,
  kAlreadyHaveActiveStrategy = 3,
  kAlreadyAdvertising = 4,
  kBluetoothError = 5,
  kBleError = 6,
  kWifiLanError = 7,
  kMaxValue = kWifiLanError
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. If entries are added, kMaxValue should
// be updated.
enum class FinalPayloadStatus {
  kSuccess = 0,
  kFailure = 1,
  kCanceled = 2,
  kMaxValue = kCanceled
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

StartAdvertisingFailureReason
NearbyConnectionsStatusToStartAdvertisingFailureReason(
    location::nearby::connections::mojom::Status status) {
  switch (status) {
    case location::nearby::connections::mojom::Status::kError:
      return StartAdvertisingFailureReason::kError;
    case location::nearby::connections::mojom::Status::kOutOfOrderApiCall:
      return StartAdvertisingFailureReason::kOutOfOrderApiCall;
    case location::nearby::connections::mojom::Status::
        kAlreadyHaveActiveStrategy:
      return StartAdvertisingFailureReason::kAlreadyHaveActiveStrategy;
    case location::nearby::connections::mojom::Status::kAlreadyAdvertising:
      return StartAdvertisingFailureReason::kAlreadyAdvertising;
    case location::nearby::connections::mojom::Status::kBluetoothError:
      return StartAdvertisingFailureReason::kBluetoothError;
    case location::nearby::connections::mojom::Status::kBleError:
      return StartAdvertisingFailureReason::kBleError;
    case location::nearby::connections::mojom::Status::kWifiLanError:
      return StartAdvertisingFailureReason::kWifiLanError;
    case location::nearby::connections::mojom::Status::kSuccess:
      NOTREACHED();
      FALLTHROUGH;
    case location::nearby::connections::mojom::Status::kAlreadyDiscovering:
    case location::nearby::connections::mojom::Status::kEndpointIOError:
    case location::nearby::connections::mojom::Status::kEndpointUnknown:
    case location::nearby::connections::mojom::Status::kConnectionRejected:
    case location::nearby::connections::mojom::Status::
        kAlreadyConnectedToEndpoint:
    case location::nearby::connections::mojom::Status::kNotConnectedToEndpoint:
    case location::nearby::connections::mojom::Status::kPayloadUnknown:
      return StartAdvertisingFailureReason::kUnknown;
  }
}

FinalPayloadStatus PayloadStatusToFinalPayloadStatus(
    location::nearby::connections::mojom::PayloadStatus status) {
  switch (status) {
    case location::nearby::connections::mojom::PayloadStatus::kSuccess:
      return FinalPayloadStatus::kSuccess;
    case location::nearby::connections::mojom::PayloadStatus::kFailure:
      return FinalPayloadStatus::kFailure;
    case location::nearby::connections::mojom::PayloadStatus::kCanceled:
      return FinalPayloadStatus::kCanceled;
    case location::nearby::connections::mojom::PayloadStatus::kInProgress:
      NOTREACHED();
      return FinalPayloadStatus::kCanceled;
  }
}

std::string GetDirectionSubcategoryName(bool is_incoming) {
  return is_incoming ? ".Receive" : ".Send";
}

std::string GetShareTargetTypeSubcategoryName(
    nearby_share::mojom::ShareTargetType type) {
  switch (type) {
    case nearby_share::mojom::ShareTargetType::kUnknown:
      return ".Unknown";
    case nearby_share::mojom::ShareTargetType::kPhone:
      return ".Phone";
    case nearby_share::mojom::ShareTargetType::kTablet:
      return ".Tablet";
    case nearby_share::mojom::ShareTargetType::kLaptop:
      return ".Laptop";
  }
}

std::string GetPayloadStatusSubcategoryName(
    location::nearby::connections::mojom::PayloadStatus status) {
  switch (status) {
    case location::nearby::connections::mojom::PayloadStatus::kSuccess:
      return ".Succeeded";
    case location::nearby::connections::mojom::PayloadStatus::kFailure:
      return ".Failed";
    case location::nearby::connections::mojom::PayloadStatus::kCanceled:
      return ".Cancelled";
    case location::nearby::connections::mojom::PayloadStatus::kInProgress:
      NOTREACHED();
      return ".Cancelled";
  }
}

std::string GetUpgradedMediumSubcategoryName(
    base::Optional<location::nearby::connections::mojom::Medium>
        last_upgraded_medium) {
  if (!last_upgraded_medium) {
    return ".NoMediumUpgrade";
  }

  switch (*last_upgraded_medium) {
    case location::nearby::connections::mojom::Medium::kWebRtc:
      return ".WebRtcUpgrade";
    case location::nearby::connections::mojom::Medium::kUnknown:
    case location::nearby::connections::mojom::Medium::kMdns:
    case location::nearby::connections::mojom::Medium::kBluetooth:
    case location::nearby::connections::mojom::Medium::kWifiHotspot:
    case location::nearby::connections::mojom::Medium::kBle:
    case location::nearby::connections::mojom::Medium::kWifiLan:
    case location::nearby::connections::mojom::Medium::kWifiAware:
    case location::nearby::connections::mojom::Medium::kNfc:
    case location::nearby::connections::mojom::Medium::kWifiDirect:
      return ".UnknownMediumUpgrade";
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

  const std::string kPrefix =
      kTransferMetricPrefix + std::string(".CompletionStatus");
  std::string send_or_receive = GetDirectionSubcategoryName(is_incoming);
  std::string share_target_type = GetShareTargetTypeSubcategoryName(type);

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

void RecordNearbyShareTransferSizeMetric(
    bool is_incoming,
    nearby_share::mojom::ShareTargetType type,
    base::Optional<location::nearby::connections::mojom::Medium>
        last_upgraded_medium,
    location::nearby::connections::mojom::PayloadStatus status,
    uint64_t payload_size_bytes) {
  DCHECK_NE(status,
            location::nearby::connections::mojom::PayloadStatus::kInProgress);

  int kilobytes =
      base::saturated_cast<int>(payload_size_bytes / kBytesPerKilobyte);
  for (const std::string& direction_name :
       {std::string(), GetDirectionSubcategoryName(is_incoming)}) {
    for (const std::string& share_target_type_name :
         {std::string(), GetShareTargetTypeSubcategoryName(type)}) {
      for (const std::string& last_upgraded_medium_name :
           {std::string(),
            GetUpgradedMediumSubcategoryName(last_upgraded_medium)}) {
        for (const std::string& payload_status_name :
             {std::string(), GetPayloadStatusSubcategoryName(status)}) {
          base::UmaHistogramCounts1M(
              kTransferMetricPrefix + std::string(".TotalSize") +
                  direction_name + share_target_type_name +
                  last_upgraded_medium_name + payload_status_name,
              kilobytes);
        }
      }
    }
  }
}

void RecordNearbyShareTransferRateMetric(
    bool is_incoming,
    nearby_share::mojom::ShareTargetType type,
    base::Optional<location::nearby::connections::mojom::Medium>
        last_upgraded_medium,
    location::nearby::connections::mojom::PayloadStatus status,
    uint64_t transferred_payload_bytes,
    base::TimeDelta time_elapsed) {
  DCHECK_NE(status,
            location::nearby::connections::mojom::PayloadStatus::kInProgress);

  int kilobytes_per_second = base::saturated_cast<int>(base::ClampDiv(
      base::ClampDiv(transferred_payload_bytes, time_elapsed.InSecondsF()),
      kBytesPerKilobyte));
  for (const std::string& direction_name :
       {std::string(), GetDirectionSubcategoryName(is_incoming)}) {
    for (const std::string& share_target_type_name :
         {std::string(), GetShareTargetTypeSubcategoryName(type)}) {
      for (const std::string& last_upgraded_medium_name :
           {std::string(),
            GetUpgradedMediumSubcategoryName(last_upgraded_medium)}) {
        for (const std::string& payload_status_name :
             {std::string(), GetPayloadStatusSubcategoryName(status)}) {
          base::UmaHistogramCounts100000(
              kTransferMetricPrefix + std::string(".Rate") + direction_name +
                  share_target_type_name + last_upgraded_medium_name +
                  payload_status_name,
              kilobytes_per_second);
        }
      }
    }
  }
}

void RecordNearbyShareTransferNumAttachmentsMetric(
    size_t num_text_attachments,
    size_t num_file_attachments) {
  const std::string kAttachmentInfix = ".NumAttachments";
  base::UmaHistogramCounts100(kTransferMetricPrefix + kAttachmentInfix,
                              num_text_attachments + num_file_attachments);
  base::UmaHistogramCounts100(
      kTransferMetricPrefix + kAttachmentInfix + ".Text", num_text_attachments);
  base::UmaHistogramCounts100(
      kTransferMetricPrefix + kAttachmentInfix + ".File", num_file_attachments);
}

void RecordNearbyShareStartAdvertisingResultMetric(
    bool is_high_visibility,
    location::nearby::connections::mojom::Status status) {
  const std::string mode_suffix =
      is_high_visibility ? ".HighVisibility" : ".BLE";
  const bool success =
      status == location::nearby::connections::mojom::Status::kSuccess;

  base::UmaHistogramBoolean(kStartAdvertisingResultMetricPrefix, success);
  base::UmaHistogramBoolean(kStartAdvertisingResultMetricPrefix + mode_suffix,
                            success);
  if (!success) {
    StartAdvertisingFailureReason reason =
        NearbyConnectionsStatusToStartAdvertisingFailureReason(status);
    base::UmaHistogramEnumeration(
        kStartAdvertisingResultFailureReasonMetricPrefix, reason);
    base::UmaHistogramEnumeration(
        kStartAdvertisingResultFailureReasonMetricPrefix + mode_suffix, reason);
  }
}

void RecordNearbyShareFinalPayloadStatusForUpgradedMedium(
    location::nearby::connections::mojom::PayloadStatus status,
    base::Optional<location::nearby::connections::mojom::Medium> medium) {
  DCHECK_NE(status,
            location::nearby::connections::mojom::PayloadStatus::kInProgress);
  base::UmaHistogramEnumeration("Nearby.Share.Medium.FinalPayloadStatus" +
                                    GetUpgradedMediumSubcategoryName(medium),
                                PayloadStatusToFinalPayloadStatus(status));
}
