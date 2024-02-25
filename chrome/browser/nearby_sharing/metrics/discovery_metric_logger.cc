// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/metrics/discovery_metric_logger.h"

#include <utility>

#include "chrome/browser/nearby_sharing/metrics/metric_common.h"
#include "components/metrics/structured/structured_events.h"
#include "components/metrics/structured/structured_metrics_client.h"

namespace nearby::share::metrics {

DiscoveryMetricLogger::DiscoveryMetricLogger() = default;
DiscoveryMetricLogger::~DiscoveryMetricLogger() {}

void DiscoveryMetricLogger::OnShareTargetDiscoveryStarted() {
  discovery_start_ = base::TimeTicks::Now();
}

void DiscoveryMetricLogger::OnShareTargetDiscoveryStopped() {
  discovery_start_ = std::nullopt;
}

// TODO(b/266739400): Test this once there is Structured Metrics unittesting
// infrastructure available.
void DiscoveryMetricLogger::OnShareTargetAdded(
    const ShareTarget& share_target) {
  // Only log metrics when there is an active discovery session.
  if (!discovery_start_.has_value()) {
    return;
  }

  auto delta = base::TimeTicks::Now() - discovery_start_.value();
  auto platform = GetPlatform(share_target);
  auto relationship = GetDeviceRelationship(share_target);

  ::metrics::structured::StructuredMetricsClient::Record(
      std::move(::metrics::structured::events::v2::nearby_share::Discovery()
                    .SetPlatform(static_cast<int>(platform))
                    .SetDeviceRelationship(static_cast<int>(relationship))
                    .SetTimeToDiscovery(delta.InMilliseconds())));
}

}  // namespace nearby::share::metrics
