// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/metrics/network_connectivity_metrics_service.h"

#include <algorithm>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "chromeos/ash/components/network/network_type_pattern.h"
#include "components/prefs/pref_service.h"
#include "services/preferences/public/cpp/dictionary_value_update.h"
#include "services/preferences/public/cpp/scoped_pref_update.h"

namespace ash {

const char kKioskNetworkDropsPerSessionHistogram[] =
    "Kiosk.Session.NetworkDrops";
const char kKioskNetworkDrops[] = "network-drops";

NetworkConnectivityMetricsService::NetworkConnectivityMetricsService()
    : NetworkConnectivityMetricsService(g_browser_process->local_state()) {}

// static
std::unique_ptr<NetworkConnectivityMetricsService>
NetworkConnectivityMetricsService::CreateForTesting(PrefService* pref) {
  return base::WrapUnique(new NetworkConnectivityMetricsService(pref));
}

NetworkConnectivityMetricsService::NetworkConnectivityMetricsService(
    PrefService* prefs)
    : prefs_(prefs) {
  // This check is needed only for tests without initialized network stubs.
  if (!NetworkHandler::IsInitialized()) {
    return;
  }
  network_state_handler_ = NetworkHandler::Get()->network_state_handler();
  is_online_ = (network_state_handler_->ConnectedNetworkByType(
                    NetworkTypePattern::Default()) != nullptr);
  network_state_handler_->AddObserver(this, FROM_HERE);
  ReportPreviousSessionNetworkDrops();
}
NetworkConnectivityMetricsService::~NetworkConnectivityMetricsService() {
  if (!NetworkHandler::IsInitialized()) {
    return;
  }
  network_state_handler_->RemoveObserver(this, FROM_HERE);
}

void NetworkConnectivityMetricsService::NetworkConnectionStateChanged(
    const NetworkState* network) {
  // If there is at least one connected network, the device is online.
  if (network_state_handler_->ConnectedNetworkByType(
          NetworkTypePattern::Default())) {
    is_online_ = true;
    return;
  }

  // The device is offline, record a network drop only when it was online.
  if (is_online_) {
    network_drops_++;
    LogNetworkDrops(network_drops_);
    is_online_ = false;
  }
}

void NetworkConnectivityMetricsService::LogNetworkDrops(int network_drops) {
  prefs::ScopedDictionaryPrefUpdate update(prefs_, prefs::kKioskMetrics);

  update->SetInteger(kKioskNetworkDrops, network_drops);
}

void NetworkConnectivityMetricsService::ReportPreviousSessionNetworkDrops() {
  const auto& metrics_dict = prefs_->GetDict(prefs::kKioskMetrics);
  const auto* network_drops_value = metrics_dict.Find(kKioskNetworkDrops);
  if (!network_drops_value) {
    LogNetworkDrops(0);
    return;
  }

  auto network_drops = network_drops_value->GetIfInt();
  if (!network_drops.has_value()) {
    LogNetworkDrops(0);
    return;
  }
  base::UmaHistogramCounts100(kKioskNetworkDropsPerSessionHistogram,
                              std::min(100, network_drops.value()));
  LogNetworkDrops(0);
}

}  // namespace ash
