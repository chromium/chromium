// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_NETWORKING_LOG_H_
#define ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_NETWORKING_LOG_H_

#include <string>
#include <vector>

#include "ash/webui/diagnostics_ui/mojom/network_health_provider.mojom.h"

namespace ash {
namespace diagnostics {

class NetworkingLog {
 public:
  NetworkingLog();

  NetworkingLog(const NetworkingLog&) = delete;
  NetworkingLog& operator=(const NetworkingLog&) = delete;

  ~NetworkingLog();

  // Returns the networking log as a string.
  std::string GetNetworkInfo() const;

  // Updates the list of valid networks and which is active.
  void UpdateNetworkList(const std::vector<std::string>& observer_guids,
                         std::string active_guid);

  // Update the state of the network. Networks can be identified by
  // the `observer_guid` property.
  void UpdateNetworkState(mojom::NetworkPtr network);

 private:
  std::string active_guid_;
  base::flat_map<std::string, mojom::NetworkPtr> latest_network_states_;
};

}  // namespace diagnostics
}  // namespace ash

#endif  // ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_NETWORKING_LOG_H_
