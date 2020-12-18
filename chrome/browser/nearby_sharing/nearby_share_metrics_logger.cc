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
enum class TransferFinalStatus {
  kComplete = 0,
  kUnknown = 1,
  kAwaitingRemoteAcceptanceFailed = 2,
  kFailed = 3,
  kRejected = 4,
  kCancelled = 5,
  kTimedOut = 6,
  kMediaUnavailable = 7,
  kNotEnoughSpace = 8,
  kUnsupportedAttachmentType = 9,
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
enum class FinalStatus {
  kSuccess = 0,
  kFailure = 1,
  kCanceled = 2,
  kMaxValue = kCanceled
};

TransferFinalStatus TransferMetadataStatusToTransferFinalStatus(
    TransferMetadata::Status status) {
  switch (status) {
    case TransferMetadata::Status::kComplete:
      return TransferFinalStatus::kComplete;
    case TransferMetadata::Status::kAwaitingRemoteAcceptanceFailed:
      return TransferFinalStatus::kAwaitingRemoteAcceptanceFailed;
    case TransferMetadata::Status::kFailed:
      return TransferFinalStatus::kFailed;
    case TransferMetadata::Status::kRejected:
      return TransferFinalStatus::kRejected;
    case TransferMetadata::Status::kCancelled:
      return TransferFinalStatus::kCancelled;
    case TransferMetadata::Status::kTimedOut:
      return TransferFinalStatus::kTimedOut;
    case TransferMetadata::Status::kMediaUnavailable:
      return TransferFinalStatus::kMediaUnavailable;
    case TransferMetadata::Status::kNotEnoughSpace:
      return TransferFinalStatus::kNotEnoughSpace;
    case TransferMetadata::Status::kUnsupportedAttachmentType:
      return TransferFinalStatus::kUnsupportedAttachmentType;
    case TransferMetadata::Status::kUnknown:
    case TransferMetadata::Status::kConnecting:
    case TransferMetadata::Status::kAwaitingLocalConfirmation:
    case TransferMetadata::Status::kAwaitingRemoteAcceptance:
    case TransferMetadata::Status::kInProgress:
    case TransferMetadata::Status::kMediaDownloading:
    case TransferMetadata::Status::kExternalProviderLaunched:
      NOTREACHED();
      return TransferFinalStatus::kUnknown;
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

FinalStatus PayloadStatusToFinalStatus(
    location::nearby::connections::mojom::PayloadStatus status) {
  switch (status) {
    case location::nearby::connections::mojom::PayloadStatus::kSuccess:
      return FinalStatus::kSuccess;
    case location::nearby::connections::mojom::PayloadStatus::kFailure:
      return FinalStatus::kFailure;
    case location::nearby::connections::mojom::PayloadStatus::kCanceled:
      return FinalStatus::kCanceled;
    case location::nearby::connections::mojom::PayloadStatus::kInProgress:
      NOTREACHED();
      return FinalStatus::kFailure;
  }
}

std::string GetDirectionSubcategoryName(bool is_incoming) {
  return is_incoming ? ".Receive" : ".Send";
}

std::string GetIsKnownSubcategoryName(bool is_known) {
  return is_known ? ".Contact" : ".NonContact";
}

std::string GetShareTargetTypeSubcategoryName(
    nearby_share::mojom::ShareTargetType type) {
  switch (type) {
    case nearby_share::mojom::ShareTargetType::kUnknown:
      return ".UnknownDeviceType";
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
      return ".Failed";
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

void RecordNearbyShareEstablishConnectionMetrics(
    bool success,
    bool cancelled,
    base::TimeDelta time_to_connect) {
  FinalStatus status;
  if (success) {
    status = FinalStatus::kSuccess;
    base::UmaHistogramTimes(
        "Nearby.Share.Connection.TimeToEstablishOutgoingConnection",
        time_to_connect);
  } else {
    status = cancelled ? FinalStatus::kCanceled : FinalStatus::kFailure;
  }
  base::UmaHistogramEnumeration(
      "Nearby.Share.Connection.EstablishOutgoingConnectionStatus", status);
}

void RecordNearbySharePayloadFinalStatusMetric(
    location::nearby::connections::mojom::PayloadStatus status,
    base::Optional<location::nearby::connections::mojom::Medium> medium) {
  DCHECK_NE(status,
            location::nearby::connections::mojom::PayloadStatus::kInProgress);
  base::UmaHistogramEnumeration("Nearby.Share.Payload.FinalStatus",
                                PayloadStatusToFinalStatus(status));
  base::UmaHistogramEnumeration("Nearby.Share.Payload.FinalStatus" +
                                    GetUpgradedMediumSubcategoryName(medium),
                                PayloadStatusToFinalStatus(status));
}

void RecordNearbySharePayloadNumAttachmentsMetric(size_t num_text_attachments,
                                                  size_t num_file_attachments) {
  base::UmaHistogramCounts100("Nearby.Share.Payload.NumAttachments",
                              num_text_attachments + num_file_attachments);
  base::UmaHistogramCounts100("Nearby.Share.Payload.NumAttachments.Text",
                              num_text_attachments);
  base::UmaHistogramCounts100("Nearby.Share.Payload.NumAttachments.File",
                              num_file_attachments);
}

void RecordNearbySharePayloadSizeMetric(
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

  const std::string prefix = "Nearby.Share.Payload.TotalSize";
  base::UmaHistogramCounts1M(prefix, kilobytes);
  base::UmaHistogramCounts1M(prefix + GetDirectionSubcategoryName(is_incoming),
                             kilobytes);
  base::UmaHistogramCounts1M(prefix + GetShareTargetTypeSubcategoryName(type),
                             kilobytes);
  base::UmaHistogramCounts1M(
      prefix + GetUpgradedMediumSubcategoryName(last_upgraded_medium),
      kilobytes);
  base::UmaHistogramCounts1M(prefix + GetPayloadStatusSubcategoryName(status),
                             kilobytes);
}

void RecordNearbySharePayloadTransferRateMetric(
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

  const std::string prefix = "Nearby.Share.Payload.TransferRate";
  base::UmaHistogramCounts100000(prefix, kilobytes_per_second);
  base::UmaHistogramCounts100000(
      prefix + GetDirectionSubcategoryName(is_incoming), kilobytes_per_second);
  base::UmaHistogramCounts100000(
      prefix + GetShareTargetTypeSubcategoryName(type), kilobytes_per_second);
  base::UmaHistogramCounts100000(
      prefix + GetUpgradedMediumSubcategoryName(last_upgraded_medium),
      kilobytes_per_second);
  base::UmaHistogramCounts100000(
      prefix + GetPayloadStatusSubcategoryName(status), kilobytes_per_second);
}

void RecordNearbyShareStartAdvertisingResultMetric(
    bool is_high_visibility,
    location::nearby::connections::mojom::Status status) {
  const std::string mode_suffix =
      is_high_visibility ? ".HighVisibility" : ".BLE";
  const bool success =
      status == location::nearby::connections::mojom::Status::kSuccess;

  const std::string result_prefix = "Nearby.Share.StartAdvertising.Result";
  base::UmaHistogramBoolean(result_prefix, success);
  base::UmaHistogramBoolean(result_prefix + mode_suffix, success);

  if (!success) {
    const std::string failure_prefix =
        "Nearby.Share.StartAdvertising.Result.FailureReason";
    StartAdvertisingFailureReason reason =
        NearbyConnectionsStatusToStartAdvertisingFailureReason(status);
    base::UmaHistogramEnumeration(failure_prefix, reason);
    base::UmaHistogramEnumeration(failure_prefix + mode_suffix, reason);
  }
}

void RecordNearbyShareTransferFinalStatusMetric(
    bool is_incoming,
    nearby_share::mojom::ShareTargetType type,
    TransferMetadata::Status status,
    bool is_known) {
  DCHECK(TransferMetadata::IsFinalStatus(status));

  std::string send_or_receive = GetDirectionSubcategoryName(is_incoming);
  std::string share_target_type = GetShareTargetTypeSubcategoryName(type);
  std::string contact_or_not = GetIsKnownSubcategoryName(is_known);

  TransferFinalStatus final_status =
      TransferMetadataStatusToTransferFinalStatus(status);

  const std::string prefix = "Nearby.Share.Transfer.FinalStatus";
  base::UmaHistogramEnumeration(prefix, final_status);
  base::UmaHistogramEnumeration(
      prefix + send_or_receive + share_target_type + contact_or_not,
      final_status);
}
