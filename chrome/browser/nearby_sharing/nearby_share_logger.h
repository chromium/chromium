// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARE_LOGGER_H_
#define CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARE_LOGGER_H_

#include "chrome/browser/nearby_sharing/nearby_sharing_service.h"
#include "chrome/browser/nearby_sharing/transfer_metadata.h"

class NearbyShareLogger : public NearbySharingService::Observer {
 public:
  NearbyShareLogger();
  ~NearbyShareLogger() override;

  // NearbySharingService::Observer
  void OnShareTargetDiscoveryStarted() override;
  void OnShareTargetDiscoveryStopped() override;
  void OnShareTargetAdded(const ShareTarget& share_target) override;
  void OnShareTargetRemoved(const ShareTarget& share_target) override;
  void OnShareTargetSelected(const ShareTarget& share_target) override;
  void OnShareTargetConnected(const ShareTarget& share_target) override;
  void OnTransferAccepted(const ShareTarget& share_target) override;
  void OnTransferStarted(const ShareTarget& share_target,
                         long total_bytes) override;
  void OnTransferUpdated(const ShareTarget& share_target,
                         float progress_complete) override;
  void OnTransferCompleted(const ShareTarget& share_target,
                           TransferMetadata::Status status) override;
  void OnInitialMedium(const ShareTarget& share_target,
                       nearby::connections::mojom::Medium medium) override;
  void OnBandwidthUpgrade(const ShareTarget& share_target,
                          nearby::connections::mojom::Medium medium) override;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARE_LOGGER_H_
