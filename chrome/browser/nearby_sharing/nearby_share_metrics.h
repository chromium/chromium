// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARE_METRICS_H_
#define CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARE_METRICS_H_

#include <optional>

#include "base/time/time.h"
#include "chrome/browser/nearby_sharing/nearby_share_feature_status.h"
#include "chrome/browser/nearby_sharing/nearby_share_feature_usage_metrics.h"
#include "chrome/browser/nearby_sharing/share_target.h"
#include "chrome/browser/nearby_sharing/transfer_metadata.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_connections_types.mojom-shared.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_connections_types.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_decoder_types.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_share_target_types.mojom.h"

enum class NearbyShareError;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. The numbers here correspond to the
// ordering of the flow. This enum should be kept in sync with the
// NearbyShareBackgroundScanningDeviceNearbySharingNotificationFlowEvent enum in
// //tools/metrics/histograms/metadata/nearby/enums.xml.
enum class
    NearbyShareBackgroundScanningDeviceNearbySharingNotificationFlowEvent {
      kNotificationShown = 1,
      kEnable = 12,
      kDismiss = 13,
      kExit = 14,
    };

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. The numbers here correspond to the
// ordering of the flow. This enum should be kept in sync with the
// NearbyShareBackgroundScanningSetupNotificationFlowEvent enum in
// //tools/metrics/histograms/metadata/nearby/enums.xml.
enum class NearbyShareBackgroundScanningSetupNotificationFlowEvent {
  kNotificationShown = 1,
  kSetup = 12,
  kDismiss = 13,
  kExit = 14,
};

enum class PayloadFileOperation {
  kOpen,
  kRead,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. The numbers here correspond to the
// ordering of the flow. This enum should be kept in sync with the
// PairedKeyVerificationError enum in
// //tools/metrics/histograms/metadata/nearby/enums.xml.
enum class NearbySharePairedKeyVerificationError {
  kFailedToReadEncryptionFrame,
  kFailedToReadResultFrame,
  kUnableToVerifyRemotePublicCertificateWhileRestrictedToContacts,
  kMissingOptionalSignature,
  kUnableToVerifyOptionalSignature,
  kMaxValue = kUnableToVerifyOptionalSignature,
};

std::string GetMediumName(nearby::connections::mojom::Medium medium);

void RecordNearbyShareEnabledMetric(NearbyShareEnabledState state);

void RecordNearbyShareEstablishConnectionMetrics(
    bool success,
    bool cancelled,
    base::TimeDelta time_to_connect);

void RecordNearbyShareInitialConnectionMedium(
    nearby::connections::mojom::Medium medium);

void RecordNearbyShareTimeFromInitiateSendToRemoteDeviceNotificationMetric(
    base::TimeDelta time);

void RecordNearbyShareTimeFromLocalAcceptToTransferStartMetric(
    base::TimeDelta time);

void RecordNearbySharePayloadFileAttachmentTypeMetric(
    ::sharing::mojom::FileMetadata::Type type,
    bool is_incoming,
    bool is_contact,
    bool for_self_share,
    nearby::connections::mojom::PayloadStatus status);

void RecordNearbySharePayloadTextAttachmentTypeMetric(
    ::sharing::mojom::TextMetadata::Type type,
    bool is_incoming,
    bool is_contact,
    bool for_self_share,
    nearby::connections::mojom::PayloadStatus status);

void RecordNearbySharePayloadWifiCredentialsAttachmentTypeMetric(
    bool is_incoming,
    bool is_contact,
    bool for_self_share,
    nearby::connections::mojom::PayloadStatus status);

void RecordNearbySharePayloadFileOperationMetrics(
    Profile* profile,
    const ShareTarget& share_target,
    PayloadFileOperation operation,
    const bool success);

void RecordNearbySharePayloadFinalStatusMetric(
    nearby::connections::mojom::PayloadStatus status,
    std::optional<nearby::connections::mojom::Medium> medium);

void RecordNearbySharePayloadMediumMetric(
    std::optional<nearby::connections::mojom::Medium> medium,
    nearby_share::mojom::ShareTargetType type,
    uint64_t num_bytes_transferred);

void RecordNearbySharePayloadNumAttachmentsMetric(
    size_t num_text_attachments,
    size_t num_file_attachments,
    size_t num_wifi_credentials_attachments);

void RecordNearbySharePayloadSizeMetric(
    bool is_incoming,
    nearby_share::mojom::ShareTargetType type,
    std::optional<nearby::connections::mojom::Medium> last_upgraded_medium,
    nearby::connections::mojom::PayloadStatus status,
    uint64_t payload_size_bytes);

void RecordNearbySharePayloadTransferRateMetric(
    bool is_incoming,
    nearby_share::mojom::ShareTargetType type,
    std::optional<nearby::connections::mojom::Medium> last_upgraded_medium,
    nearby::connections::mojom::PayloadStatus status,
    uint64_t transferred_payload_bytes,
    base::TimeDelta time_elapsed);

void RecordNearbyShareStartAdvertisingResultMetric(
    bool is_high_visibility,
    nearby::connections::mojom::Status status);

void RecordNearbyShareTransferFinalStatusMetric(
    NearbyShareFeatureUsageMetrics* feature_usage_metrics,
    bool is_incoming,
    nearby_share::mojom::ShareTargetType type,
    TransferMetadata::Status status,
    bool is_known,
    bool for_self_share,
    bool is_screen_locked);

void RecordNearbyShareDeviceNearbySharingNotificationFlowEvent(
    NearbyShareBackgroundScanningDeviceNearbySharingNotificationFlowEvent
        event);

void RecordNearbyShareDeviceNearbySharingNotificationTimeToAction(
    base::TimeDelta time);

void RecordNearbyShareBackgroundScanningDevicesDetected();

void RecordNearbyShareBackgroundScanningDevicesDetectedDuration(
    base::TimeDelta duration);

void RecordNearbyShareBackgroundScanningSessionStarted(bool success);

void RecordNearbyShareSetupNotificationFlowEvent(
    NearbyShareBackgroundScanningSetupNotificationFlowEvent event);

void RecordNearbyShareSetupNotificationTimeToAction(base::TimeDelta time);

void RecordNearbyShareWifiConfigurationResultMetric(bool success);

void RecordNearbyShareDiscoveredToConnectionEstablishedDuration(
    base::TimeDelta delta);

void RecordNearbyShareInitiatedToSentIntroductionFrameDuration(
    base::TimeDelta delta);

void RecordNearbyShareEndpointDecodedToReceivedIntroductionFrameDuration(
    base::TimeDelta delta);

void RecordNearbyShareConnectionEstablishedToBandwidthUpgradeDuration(
    nearby::connections::mojom::Medium medium,
    base::TimeDelta delta);

void RecordNearbyShareHighVisibilityEndpointDecodedToBandwidthUpgradeDuration(
    nearby::connections::mojom::Medium medium,
    base::TimeDelta delta);

void RecordNearbyShareNonHighVisibilityPairedKeyCompleteToBandwidthUpgradeDuration(
    nearby::connections::mojom::Medium medium,
    base::TimeDelta delta);

void RecordNearbyShareBandwidthUpgradeToAllFilesReceivedDuration(
    nearby::connections::mojom::Medium medium,
    base::TimeDelta delta);

void RecordNearbyShareAcceptedTransferToAllFilesReceivedDuration(
    base::TimeDelta delta);

void RecordNearbyShareReceivedIntroductionFrameToAllFilesReceivedDuration(
    base::TimeDelta delta);

void RecordNearbyShareBandwidthUpgradeToAllFilesSentDuration(
    nearby::connections::mojom::Medium medium,
    base::TimeDelta delta);

void RecordNearbyShareStartSendFilesToAllFilesSentDuration(
    base::TimeDelta delta);

void RecordNearbyShareInitiatedToAllFilesSentDuration(base::TimeDelta delta);

void RecordNearbyShareError(NearbyShareError error_code);

void RecordNearbySharePairedKeyVerificationError(
    NearbySharePairedKeyVerificationError error);

#endif  // CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARE_METRICS_H_
