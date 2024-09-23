// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_METRICS_NEARBY_SHARE_METRIC_LOGGER_H_
#define CHROME_BROWSER_NEARBY_SHARING_METRICS_NEARBY_SHARE_METRIC_LOGGER_H_

#include "base/containers/flat_map.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service.h"
#include "chrome/browser/nearby_sharing/transfer_metadata.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_connections_types.mojom-shared.h"

namespace nearby::share::metrics {

class NearbyShareMetricLogger : public NearbySharingService::Observer {
 public:
  NearbyShareMetricLogger();
  ~NearbyShareMetricLogger() override;

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
                         float percentage_complete) override;
  void OnTransferCompleted(const ShareTarget& share_target,
                           TransferMetadata::Status status) override;
  void OnInitialMedium(const ShareTarget& share_target,
                       nearby::connections::mojom::Medium medium) override;
  void OnBandwidthUpgrade(const ShareTarget& share_target,
                          nearby::connections::mojom::Medium medium) override;

 private:
  std::optional<base::TimeTicks> discovery_start_time_;
  base::flat_map<base::UnguessableToken, base::TimeDelta>
      share_target_scan_to_discover_delta_;
  base::flat_map<base::UnguessableToken, base::TimeTicks>
      share_target_discover_time_;
  base::flat_map<base::UnguessableToken, base::TimeTicks>
      share_target_select_time_;
  base::flat_map<base::UnguessableToken, base::TimeTicks>
      share_target_connect_time_;
  base::flat_map<base::UnguessableToken, base::TimeTicks>
      share_target_accept_time_;
  base::flat_map<base::UnguessableToken, base::TimeTicks>
      share_target_upgrade_time_;
  // The initial medium map reflects the medium that connection started
  // over, while the medium map tracks the current medium (reflecting any
  // bandwidth upgrades).
  base::flat_map<base::UnguessableToken, nearby::connections::mojom::Medium>
      share_target_initial_medium_;
  base::flat_map<base::UnguessableToken, nearby::connections::mojom::Medium>
      share_target_medium_;
  base::flat_map<base::UnguessableToken, int64_t> transfer_size_;
  base::flat_map<base::UnguessableToken, float> transfer_progress_;
};

}  // namespace nearby::share::metrics

#endif  // CHROME_BROWSER_NEARBY_SHARING_METRICS_NEARBY_SHARE_METRIC_LOGGER_H_
