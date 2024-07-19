// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_share_logger.h"

#include "chrome/browser/nearby_sharing/nearby_share_metrics.h"
#include "chrome/browser/nearby_sharing/transfer_metadata.h"
#include "components/cross_device/logging/logging.h"

namespace {

std::ostream& operator<<(std::ostream& os, const ShareTarget& share_target) {
  os << "[" << share_target.id.ToString() << "] ";
  return os;
}

}  // namespace

NearbyShareLogger::NearbyShareLogger() {
  CD_LOG(INFO, Feature::NS) << "Nearby Share logging initialized.";
}

NearbyShareLogger::~NearbyShareLogger() {
  CD_LOG(INFO, Feature::NS) << "Nearby Share logging shutdown.";
}

void NearbyShareLogger::OnShareTargetDiscoveryStarted() {
  CD_LOG(INFO, Feature::NS) << "Discovery started.";
}

void NearbyShareLogger::OnShareTargetDiscoveryStopped() {
  CD_LOG(INFO, Feature::NS) << "Discovery stopped.";
}

void NearbyShareLogger::OnShareTargetAdded(const ShareTarget& share_target) {
  CD_LOG(INFO, Feature::NS) << share_target << "Share target added.";
}

void NearbyShareLogger::OnShareTargetRemoved(const ShareTarget& share_target) {
  CD_LOG(INFO, Feature::NS) << share_target << "Share target removed.";
}

void NearbyShareLogger::OnShareTargetSelected(const ShareTarget& share_target) {
  CD_LOG(INFO, Feature::NS) << share_target << "Share target selected.";
}

void NearbyShareLogger::OnShareTargetConnected(
    const ShareTarget& share_target) {
  CD_LOG(INFO, Feature::NS) << share_target << "Share target connected.";
}

void NearbyShareLogger::OnTransferAccepted(const ShareTarget& share_target) {
  CD_LOG(INFO, Feature::NS)
      << share_target << "Share target accepted transfer.";
}

void NearbyShareLogger::OnTransferStarted(const ShareTarget& share_target,
                                          long total_bytes) {
  CD_LOG(INFO, Feature::NS)
      << share_target << "Transfer started: " << total_bytes << " bytes.";
}

void NearbyShareLogger::OnTransferUpdated(const ShareTarget& share_target,
                                          float percentage_complete) {
  CD_LOG(INFO, Feature::NS)
      << share_target << "Transfer updated: " << percentage_complete << "%.";
}

void NearbyShareLogger::OnTransferCompleted(const ShareTarget& share_target,
                                            TransferMetadata::Status status) {
  CD_LOG(INFO, Feature::NS)
      << share_target << "Transfer completed with status: "
      << TransferMetadata::StatusToString(status);
}

void NearbyShareLogger::OnInitialMedium(
    const ShareTarget& share_target,
    nearby::connections::mojom::Medium medium) {
  CD_LOG(INFO, Feature::NS) << share_target << "Initial connection medium is "
                            << GetMediumName(medium) << ".";
}

void NearbyShareLogger::OnBandwidthUpgrade(
    const ShareTarget& share_target,
    nearby::connections::mojom::Medium medium) {
  CD_LOG(INFO, Feature::NS)
      << share_target << "Upgraded to " << GetMediumName(medium) << ".";
}
