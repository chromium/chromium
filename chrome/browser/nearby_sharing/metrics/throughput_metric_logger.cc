// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/metrics/throughput_metric_logger.h"

#include <utility>

#include "chrome/browser/nearby_sharing/metrics/metric_common.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_connections_types.mojom-shared.h"
#include "components/metrics/structured/structured_events.h"
#include "components/metrics/structured/structured_metrics_client.h"

namespace nearby::share::metrics {

ThroughputMetricLogger::ThroughputMetricLogger() = default;
ThroughputMetricLogger::~ThroughputMetricLogger() {}

void ThroughputMetricLogger::OnTransferStarted(const ShareTarget& share_target,
                                               long total_bytes) {
  // Store the file size.
  transfer_sizes_[share_target.id] = total_bytes;

  // An upgrade may have occurred before the transfer starts, so only set the
  // default medium if it has not already been set.
  if (!last_medium_.contains(share_target.id)) {
    last_medium_[share_target.id] = connections::mojom::Medium::kBluetooth;
  }
}

// TODO(b/266739400): Test this once there is Structured Metrics unittesting
// infrastructure available.
void ThroughputMetricLogger::OnTransferUpdated(const ShareTarget& share_target,
                                               float percentage_complete) {
  base::TimeTicks current_update = base::TimeTicks::Now();
  base::TimeTicks previous_update = last_update_[share_target.id];
  float previous_percentage = last_percentage_[share_target.id];

  // Compute the deltas.
  float percentage_delta = percentage_complete - previous_percentage;
  base::TimeDelta update_delta = current_update - previous_update;

  // To prevent spamming of metrics, batch updates to a minimum of 1%.
  if (percentage_delta < 1.0) {
    return;
  }

  // Update the stored state for this share target.
  last_update_[share_target.id] = current_update;
  last_percentage_[share_target.id] = percentage_complete;

  // Do not emit events for recently upgraded share targets to deal with partial
  // updates across the medium upgrade.
  if (recently_upgraded_.contains(share_target.id)) {
    recently_upgraded_.erase(share_target.id);
    return;
  }

  // Do not emit a metric for the first or last update to prevent setup and
  // teardown noise.
  if (previous_percentage <= 0 || percentage_complete >= 100) {
    return;
  }

  // We must have either started the transfer or triggered a bandwidth upgrade.
  CHECK(last_medium_.contains(share_target.id));
  connections::mojom::Medium medium = last_medium_[share_target.id];

  // We must have started a transfer.
  CHECK(transfer_sizes_.contains(share_target.id));
  long transfer_size = transfer_sizes_[share_target.id];

  Platform platform = GetPlatform(share_target);
  DeviceRelationship relationship = GetDeviceRelationship(share_target);
  // Convert percentages to bytes. There might be some loss of accuracy due
  // to the float conversion, but it should be within a few bytes of the actual
  // number.
  long transferred_bytes = (percentage_complete / 100) * transfer_size;
  long update_bytes = (percentage_delta / 100) * transfer_size;

  ::metrics::structured::StructuredMetricsClient::Record(
      std::move(::metrics::structured::events::v2::nearby_share::Throughput()
                    .SetIsReceiving(share_target.is_incoming)
                    .SetPlatform(static_cast<int>(platform))
                    .SetDeviceRelationship(static_cast<int>(relationship))
                    .SetMedium(static_cast<int>(medium))
                    .SetTotalTransferBytes(transfer_size)
                    .SetTransferredBytes(transferred_bytes)
                    .SetUpdateBytes(update_bytes)
                    .SetUpdateMillis(update_delta.InMilliseconds())));
}

void ThroughputMetricLogger::OnTransferCompleted(
    const ShareTarget& share_target,
    TransferMetadata::Status status) {
  // Reset all share target state.
  transfer_sizes_.erase(share_target.id);
  last_update_.erase(share_target.id);
  last_percentage_.erase(share_target.id);
  last_medium_.erase(share_target.id);
  recently_upgraded_.erase(share_target.id);
}

void ThroughputMetricLogger::OnBandwidthUpgrade(
    const ShareTarget& share_target,
    connections::mojom::Medium medium) {
  // Update the medium.
  last_medium_[share_target.id] = medium;

  // Mark the share target as recently upgraded.
  recently_upgraded_.insert(share_target.id);
}

}  // namespace nearby::share::metrics
