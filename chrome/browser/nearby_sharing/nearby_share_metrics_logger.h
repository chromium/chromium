// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARE_METRICS_LOGGER_H_
#define CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARE_METRICS_LOGGER_H_

#include "base/optional.h"
#include "base/time/time.h"
#include "chrome/browser/nearby_sharing/transfer_metadata.h"
#include "chromeos/services/nearby/public/mojom/nearby_connections_types.mojom.h"
#include "chromeos/services/nearby/public/mojom/nearby_decoder_types.mojom.h"
#include "chromeos/services/nearby/public/mojom/nearby_share_target_types.mojom.h"

class PrefService;

void RecordNearbyShareEnabledMetric(const PrefService* pref_service);

void RecordNearbyShareEstablishConnectionMetrics(
    bool success,
    bool cancelled,
    base::TimeDelta time_to_connect);

void RecordNearbyShareTimeFromInitiateSendToRemoteDeviceNotificationMetric(
    base::TimeDelta time);

void RecordNearbyShareTimeFromLocalAcceptToTransferStartMetric(
    base::TimeDelta time);

void RecordNearbySharePayloadFileAttachmentTypeMetric(
    sharing::mojom::FileMetadata::Type type,
    bool is_incoming,
    location::nearby::connections::mojom::PayloadStatus status);

void RecordNearbySharePayloadTextAttachmentTypeMetric(
    sharing::mojom::TextMetadata::Type type,
    bool is_incoming,
    location::nearby::connections::mojom::PayloadStatus status);

void RecordNearbySharePayloadFinalStatusMetric(
    location::nearby::connections::mojom::PayloadStatus status,
    base::Optional<location::nearby::connections::mojom::Medium> medium);

void RecordNearbySharePayloadMediumMetric(
    base::Optional<location::nearby::connections::mojom::Medium> medium,
    nearby_share::mojom::ShareTargetType type,
    uint64_t num_bytes_transferred);

void RecordNearbySharePayloadNumAttachmentsMetric(size_t num_text_attachments,
                                                  size_t num_file_attachments);

void RecordNearbySharePayloadSizeMetric(
    bool is_incoming,
    nearby_share::mojom::ShareTargetType type,
    base::Optional<location::nearby::connections::mojom::Medium>
        last_upgraded_medium,
    location::nearby::connections::mojom::PayloadStatus status,
    uint64_t payload_size_bytes);

void RecordNearbySharePayloadTransferRateMetric(
    bool is_incoming,
    nearby_share::mojom::ShareTargetType type,
    base::Optional<location::nearby::connections::mojom::Medium>
        last_upgraded_medium,
    location::nearby::connections::mojom::PayloadStatus status,
    uint64_t transferred_payload_bytes,
    base::TimeDelta time_elapsed);

void RecordNearbyShareStartAdvertisingResultMetric(
    bool is_high_visibility,
    location::nearby::connections::mojom::Status status);

void RecordNearbyShareTransferFinalStatusMetric(
    bool is_incoming,
    nearby_share::mojom::ShareTargetType type,
    TransferMetadata::Status status,
    bool is_known);

#endif  // CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARE_METRICS_LOGGER_H_
