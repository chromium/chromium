// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_DIAGNOSTICS_NETWORKING_LOG_H_
#define ASH_SYSTEM_DIAGNOSTICS_NETWORKING_LOG_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/system/diagnostics/async_log.h"
#include "ash/webui/diagnostics_ui/mojom/network_health_provider.mojom.h"
#include "base/gtest_prod_util.h"

namespace ash {
namespace diagnostics {

class ASH_EXPORT NetworkingLog {
 public:
  explicit NetworkingLog(const base::FilePath& log_base_path);

  NetworkingLog(const NetworkingLog&) = delete;
  NetworkingLog& operator=(const NetworkingLog&) = delete;

  ~NetworkingLog();

  // Returns the networking info section as a string.
  std::string GetNetworkInfo() const;

  // Returns the networking events section as a string.
  std::string GetNetworkEvents() const;

  // Updates the list of valid networks and which is active.
  void UpdateNetworkList(const std::vector<std::string>& observer_guids,
                         std::string active_guid);

  // Update the state of the network. Networks can be identified by
  // the `observer_guid` property.
  void UpdateNetworkState(mojom::NetworkPtr network);

 private:
  FRIEND_TEST_ALL_PREFIXES(NetworkHealthProviderTest, NetworkingLog);
  friend class NetworkHealthProviderTest;

  // Test only. Get the count of UpdateNetworkList() being called.
  size_t update_network_list_call_count_for_testing() const;

  // Writes the `event_string` to the `event_log_`.
  void LogEvent(const std::string& event_string);

  // Logs an event whenever a new network is seen in the network list
  // sent to UpdateNetworkList().
  void LogNetworkAdded(const mojom::NetworkPtr& network);

  // Logs an event whenever a network is removed from the network list
  // sent to UpdateNetworkList().
  void LogNetworkRemoved(const mojom::NetworkPtr& network);

  // Top level function that determines which state changes should be
  // logged and calls the appropriate specialized method below.
  void LogNetworkChanges(const mojom::NetworkPtr& network);

  // Logs when the state of the network changes.
  void LogNetworkStateChanged(const mojom::NetworkPtr& old_state,
                              const mojom::NetworkPtr& new_state);

  // Logs when a WiFi network is joined.
  void LogJoinedWiFiNetwork(const mojom::NetworkPtr& network);

  // Logs when a WiFi network is left.
  void LogLeftWiFiNetwork(const mojom::NetworkPtr& network,
                          const std::string& old_ssid);

  // Logs when a WiFi network roams between access points.
  void LogWiFiRoamedAccessPoint(const mojom::NetworkPtr& network,
                                const std::string& old_bssid);

  AsyncLog event_log_;
  std::string active_guid_;
  base::flat_map<std::string, mojom::NetworkPtr> latest_network_states_;
  size_t update_network_list_call_count_for_testing_ = 0;
};

}  // namespace diagnostics
}  // namespace ash

#endif  // ASH_SYSTEM_DIAGNOSTICS_NETWORKING_LOG_H_
