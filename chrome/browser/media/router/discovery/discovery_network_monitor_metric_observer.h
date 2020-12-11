// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DISCOVERY_NETWORK_MONITOR_METRIC_OBSERVER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DISCOVERY_NETWORK_MONITOR_METRIC_OBSERVER_H_

#include "chrome/browser/media/router/discovery/discovery_network_monitor.h"

#include "base/optional.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/media/router/discovery/discovery_network_monitor_metrics.h"

namespace media_router {

class DiscoveryNetworkMonitorMetricObserver final
    : public DiscoveryNetworkMonitor::Observer {
 public:
  DiscoveryNetworkMonitorMetricObserver(
      const base::TickClock* tick_clock,
      std::unique_ptr<DiscoveryNetworkMonitorMetrics> metrics);
  ~DiscoveryNetworkMonitorMetricObserver();

  // DiscoveryNetworkMonitor::Observer implementation.
  void OnNetworksChanged(const std::string& network_id) override;

 private:
  // This method will be scheduled to execute after a constant timeout when we
  // see a kNetworkIdDisconnected network ID.  If the execution is not
  // cancelled, we report that we are in a disconnected state.  The timeout is
  // to avoid spurious reports due to the way DiscoveryNetworkMonitor reports
  // disconnections between every network change.  |disconnect_time| is the time
  // that kNetworkIdDisconnected was seen so we can report the original time
  // value instead of the time that this later check runs.
  void ConfirmDisconnectedToReportMetrics(base::TimeTicks disconnect_time);

  const base::TickClock* tick_clock_;
  std::unique_ptr<DiscoveryNetworkMonitorMetrics> metrics_;
  base::Optional<base::TimeTicks> last_event_time_;
  base::OneShotTimer disconnect_timer_;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DISCOVERY_NETWORK_MONITOR_METRIC_OBSERVER_H_
