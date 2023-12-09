// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/network/wifi_signal_strength_rssi_fetcher.h"

#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/queue.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/values.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/network/managed_network_configuration_handler.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "components/onc/onc_constants.h"

using ::onc::network_config::kWiFi;
using ::onc::wifi::kSignalStrengthRssi;

namespace reporting {
namespace {

void FetchNextWifiSignalStrengthRssi(
    base::queue<std::string> service_path_queue,
    base::flat_map<std::string, int> path_rssi_map,
    WifiSignalStrengthRssiCallback cb);

void OnGetProperties(base::queue<std::string> service_path_queue,
                     base::flat_map<std::string, int> path_rssi_map,
                     WifiSignalStrengthRssiCallback cb,
                     const std::string& service_path,
                     std::optional<base::Value::Dict> properties,
                     std::optional<std::string> error) {
  if (!properties.has_value() || !properties->FindDict(kWiFi) ||
      !properties->FindDict(kWiFi)->FindInt(kSignalStrengthRssi)) {
    DVLOG(1) << "Fetching signal strength RSSI value of service "
             << service_path << " failed. ";
    if (error.has_value()) {
      DVLOG(1) << "Error: " << error.value();
    }
  } else {
    CHECK(!base::Contains(path_rssi_map, service_path));

    std::optional<int> rssi =
        properties->FindDict(kWiFi)->FindInt(kSignalStrengthRssi);
    path_rssi_map[service_path] = rssi.value();
  }

  FetchNextWifiSignalStrengthRssi(std::move(service_path_queue),
                                  std::move(path_rssi_map), std::move(cb));
}

void FetchNextWifiSignalStrengthRssi(
    base::queue<std::string> service_path_queue,
    base::flat_map<std::string, int> path_rssi_map,
    WifiSignalStrengthRssiCallback cb) {
  if (service_path_queue.empty()) {
    std::move(cb).Run(std::move(path_rssi_map));
    return;
  }

  std::string service_path = std::move(service_path_queue.front());
  service_path_queue.pop();
  ::ash::NetworkHandler::Get()
      ->managed_network_configuration_handler()
      ->GetProperties(
          ash::LoginState::Get()->primary_user_hash(), service_path,
          base::BindOnce(&OnGetProperties, std::move(service_path_queue),
                         std::move(path_rssi_map), std::move(cb)));
}
}  // namespace

void FetchWifiSignalStrengthRssi(base::queue<std::string> service_path_queue,
                                 WifiSignalStrengthRssiCallback cb) {
  FetchNextWifiSignalStrengthRssi(std::move(service_path_queue),
                                  /*path_rssi_map=*/{}, std::move(cb));
}

}  // namespace reporting
