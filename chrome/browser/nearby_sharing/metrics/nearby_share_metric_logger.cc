// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/metrics/nearby_share_metric_logger.h"

#include <utility>

#include "base/time/time.h"
#include "chrome/browser/nearby_sharing/metrics/metric_common.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_connections_types.mojom-shared.h"
#include "components/metrics/structured/structured_events.h"
#include "components/metrics/structured/structured_metrics_client.h"

namespace nearby::share::metrics {

NearbyShareMetricLogger::NearbyShareMetricLogger() = default;

NearbyShareMetricLogger::~NearbyShareMetricLogger() {}

void NearbyShareMetricLogger::OnShareTargetDiscoveryStarted() {
  discovery_start_time_ = base::TimeTicks::Now();
}

void NearbyShareMetricLogger::OnShareTargetDiscoveryStopped() {
  discovery_start_time_ = std::nullopt;
}

void NearbyShareMetricLogger::OnShareTargetAdded(
    const ShareTarget& share_target) {
  base::TimeTicks discover_time = base::TimeTicks::Now();
  // Discovery time only applies to senders.
  if (!share_target.is_incoming) {
    CHECK(discovery_start_time_.has_value());
    share_target_scan_to_discover_delta_[share_target.id] =
        discover_time - discovery_start_time_.value();
  }

  share_target_discover_time_[share_target.id] = discover_time;
  // Bluetooth is currently the default medium. Initial medium is
  // set in OnInitialMedium, not here. Note that we only expect
  // OnInitialMedium to be called when sending; luckily this
  // aligns with the only case where the initial medium can be
  // Wifi LAN instead of Bluetooth (thanks to mDNS Discovery).
  share_target_initial_medium_[share_target.id] =
      nearby::connections::mojom::Medium::kBluetooth;
  share_target_medium_[share_target.id] =
      nearby::connections::mojom::Medium::kBluetooth;
}

void NearbyShareMetricLogger::OnShareTargetRemoved(
    const ShareTarget& share_target) {
  share_target_discover_time_.erase(share_target.id);
  share_target_select_time_.erase(share_target.id);
  share_target_connect_time_.erase(share_target.id);
  share_target_accept_time_.erase(share_target.id);
  share_target_upgrade_time_.erase(share_target.id);
  share_target_medium_.erase(share_target.id);
  share_target_initial_medium_.erase(share_target.id);
  transfer_size_.erase(share_target.id);
  transfer_progress_.erase(share_target.id);
}

void NearbyShareMetricLogger::OnShareTargetSelected(
    const ShareTarget& share_target) {
  share_target_select_time_[share_target.id] = base::TimeTicks::Now();
}

void NearbyShareMetricLogger::OnShareTargetConnected(
    const ShareTarget& share_target) {
  share_target_connect_time_[share_target.id] = base::TimeTicks::Now();
}

void NearbyShareMetricLogger::OnTransferAccepted(
    const ShareTarget& share_target) {
  share_target_accept_time_[share_target.id] = base::TimeTicks::Now();
}

void NearbyShareMetricLogger::OnTransferStarted(const ShareTarget& share_target,
                                                long total_bytes) {
  transfer_size_[share_target.id] = total_bytes;
}

void NearbyShareMetricLogger::OnTransferUpdated(const ShareTarget& share_target,
                                                float progress_complete) {
  transfer_progress_[share_target.id] = progress_complete;
}

// TODO(b/266739400): Test this once there is Structured Metrics unittesting
// infrastructure available.
void NearbyShareMetricLogger::OnTransferCompleted(
    const ShareTarget& share_target,
    TransferMetadata::Status status) {
  TransferResult result = GetTransferResult(status);
  if (result == TransferResult::kComplete) {
    transfer_progress_[share_target.id] = 100.0;
  }

  Platform platform = GetPlatform(share_target);
  DeviceRelationship relationship = GetDeviceRelationship(share_target);
  int64_t total_transfer_bytes = transfer_size_[share_target.id];
  float percentage_complete = transfer_progress_[share_target.id];
  int64_t bytes_transferred =
      (percentage_complete / 100) * total_transfer_bytes;
  connections::mojom::Medium initial_medium =
      share_target_initial_medium_[share_target.id];
  connections::mojom::Medium final_medium =
      share_target_medium_[share_target.id];

  auto metric = ::metrics::structured::events::v2::nearby_share::ShareSession();
  metric.SetIsReceiving(share_target.is_incoming);
  metric.SetPlatform(static_cast<int>(platform));
  metric.SetDeviceRelationship(static_cast<int>(relationship));
  metric.SetInitialMedium(static_cast<int>(initial_medium));
  metric.SetFinalMedium(static_cast<int>(final_medium));
  metric.SetNumberOfFiles(share_target.file_attachments.size());
  metric.SetNumberOfTexts(share_target.text_attachments.size());
  metric.SetNumberOfWiFiCredentials(
      share_target.wifi_credentials_attachments.size());
  metric.SetTotalTransferBytes(total_transfer_bytes);
  metric.SetBytesTransferred(bytes_transferred);
  metric.SetResult(static_cast<int>(result));

  // Calculate selection time. This is not set for a receiver.
  if (!share_target.is_incoming &&
      share_target_scan_to_discover_delta_.contains(share_target.id)) {
    base::TimeDelta time =
        share_target_scan_to_discover_delta_[share_target.id];
    metric.SetTimeToDiscovery(time.InMilliseconds());
  }

  // Calculate selection time. This is not set for a receiver.
  if (!share_target.is_incoming &&
      share_target_select_time_.contains(share_target.id)) {
    base::TimeDelta diff = (share_target_select_time_[share_target.id] -
                            share_target_discover_time_[share_target.id]);
    metric.SetTimeToSelect(diff.InMilliseconds());
  }

  // Calculate connect time. This is not set for a receiver.
  if (!share_target.is_incoming &&
      share_target_connect_time_.contains(share_target.id)) {
    base::TimeDelta diff = (share_target_connect_time_[share_target.id] -
                            share_target_discover_time_[share_target.id]);
    metric.SetTimeToConnect(diff.InMilliseconds());
  }

  // Calculate accept time. This is not set for rejected transfers.
  if (share_target_accept_time_.contains(share_target.id)) {
    base::TimeDelta diff = (share_target_accept_time_[share_target.id] -
                            share_target_discover_time_[share_target.id]);
    metric.SetTimeToAccept(diff.InMilliseconds());
  }

  // Calculate upgrade time, if one occurred.
  if (share_target_upgrade_time_.contains(share_target.id)) {
    base::TimeDelta diff = (share_target_upgrade_time_[share_target.id] -
                            share_target_discover_time_[share_target.id]);
    metric.SetTimeToUpgrade(diff.InMilliseconds());
  }

  // Calculate transfer time
  base::TimeDelta complete_time =
      (base::TimeTicks::Now() - share_target_discover_time_[share_target.id]);
  metric.SetTimeToTransferComplete(complete_time.InMilliseconds());

  // Emit the metric.
  ::metrics::structured::StructuredMetricsClient::Record(std::move(metric));
}

void NearbyShareMetricLogger::OnInitialMedium(
    const ShareTarget& share_target,
    nearby::connections::mojom::Medium medium) {
  share_target_initial_medium_[share_target.id] = medium;
  share_target_medium_[share_target.id] = medium;
}

void NearbyShareMetricLogger::OnBandwidthUpgrade(
    const ShareTarget& share_target,
    nearby::connections::mojom::Medium medium) {
  share_target_upgrade_time_[share_target.id] = base::TimeTicks::Now();
  share_target_medium_[share_target.id] = medium;
}

}  // namespace nearby::share::metrics
