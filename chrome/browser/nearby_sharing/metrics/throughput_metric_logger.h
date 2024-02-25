// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_METRICS_THROUGHPUT_METRIC_LOGGER_H_
#define CHROME_BROWSER_NEARBY_SHARING_METRICS_THROUGHPUT_METRIC_LOGGER_H_

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service.h"
#include "chrome/browser/nearby_sharing/transfer_metadata.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_connections_types.mojom-shared.h"

namespace nearby::share::metrics {

class ThroughputMetricLogger : public NearbySharingService::Observer {
 public:
  ThroughputMetricLogger();
  ~ThroughputMetricLogger() override;

  // NearbySharingService::Observer
  void OnTransferStarted(const ShareTarget& share_target,
                         long total_bytes) override;
  void OnTransferUpdated(const ShareTarget& share_target,
                         float percentage_complete) override;
  void OnTransferCompleted(const ShareTarget& share_target,
                           TransferMetadata::Status status) override;
  void OnBandwidthUpgrade(const ShareTarget& share_target,
                          connections::mojom::Medium medium) override;

 private:
  base::flat_map<base::UnguessableToken, long> transfer_sizes_;
  base::flat_map<base::UnguessableToken, base::TimeTicks> last_update_;
  base::flat_map<base::UnguessableToken, float> last_percentage_;
  base::flat_map<base::UnguessableToken, nearby::connections::mojom::Medium>
      last_medium_;
  base::flat_set<base::UnguessableToken> recently_upgraded_;
};

}  // namespace nearby::share::metrics

#endif  // CHROME_BROWSER_NEARBY_SHARING_METRICS_THROUGHPUT_METRIC_LOGGER_H_
