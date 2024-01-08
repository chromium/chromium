// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_METRICS_NETWORK_CONNECTIVITY_METRICS_SERVICE_H_
#define CHROME_BROWSER_ASH_APP_MODE_METRICS_NETWORK_CONNECTIVITY_METRICS_SERVICE_H_

#include <utility>

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "components/prefs/pref_service.h"

namespace ash {

extern const char kKioskNetworkDrops[];
extern const char kKioskNetworkDropsPerSessionHistogram[];

// NetworkConnectivityMetricsService counts and reports network connectivity
// drop events.
class NetworkConnectivityMetricsService : public NetworkStateHandlerObserver {
 public:
  NetworkConnectivityMetricsService();
  NetworkConnectivityMetricsService(NetworkConnectivityMetricsService&) =
      delete;
  NetworkConnectivityMetricsService& operator=(
      const NetworkConnectivityMetricsService&) = delete;
  ~NetworkConnectivityMetricsService() override;

  static std::unique_ptr<NetworkConnectivityMetricsService> CreateForTesting(
      PrefService* pref);

  bool is_online() const { return is_online_; }

 private:
  explicit NetworkConnectivityMetricsService(PrefService* prefs);

  // NetworkStateHandlerObserver:
  void NetworkConnectionStateChanged(const NetworkState* network) override;

  // Update a number of network connectivity drops for the current session.
  void LogNetworkDrops(int network_drops);

  // Report a number of network connectivity drops during the previous session.
  void ReportPreviousSessionNetworkDrops();

  raw_ptr<PrefService> prefs_;
  bool is_online_;
  int network_drops_ = 0;

  raw_ptr<NetworkStateHandler> network_state_handler_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_METRICS_NETWORK_CONNECTIVITY_METRICS_SERVICE_H_
