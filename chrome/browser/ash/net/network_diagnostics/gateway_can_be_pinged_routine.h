// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_GATEWAY_CAN_BE_PINGED_ROUTINE_H_
#define CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_GATEWAY_CAN_BE_PINGED_ROUTINE_H_

#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ash/net/network_diagnostics/network_diagnostics_routine.h"
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {
namespace network_diagnostics {

// Tests whether a device can ping all the gateways it is connected to.
class GatewayCanBePingedRoutine : public NetworkDiagnosticsRoutine {
 public:
  GatewayCanBePingedRoutine(
      chromeos::network_diagnostics::mojom::RoutineCallSource source,
      DebugDaemonClient* debug_daemon_client);
  GatewayCanBePingedRoutine(const GatewayCanBePingedRoutine&) = delete;
  GatewayCanBePingedRoutine& operator=(const GatewayCanBePingedRoutine&) =
      delete;
  ~GatewayCanBePingedRoutine() override;

  // NetworkDiagnosticsRoutine:
  bool CanRun() override;
  chromeos::network_diagnostics::mojom::RoutineType Type() override;
  void Run() override;
  void AnalyzeResultsAndExecuteCallback() override;

 private:
  void FetchActiveNetworks();
  void FetchManagedProperties(const std::vector<std::string>& guids);
  void PingGateways();
  // Determines whether the ICMP result for any of the non-default networks were
  // above the threshold.
  bool BelowLatencyThreshold();
  bool ParseICMPResult(const std::string&,
                       std::string* ip,
                       base::TimeDelta* latency);
  void OnNetworkStateListReceived(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
          networks);
  void OnManagedPropertiesReceived(
      chromeos::network_config::mojom::ManagedPropertiesPtr managed_properties);
  // |is_default_network_ping_result| is true if the ICMP result, stored in
  // |status| corresponds to that of the default network.
  void OnTestICMPCompleted(bool is_default_network_ping_result,
                           const std::optional<std::string> status);
  DebugDaemonClient* debug_daemon_client() const {
    DCHECK(debug_daemon_client_);
    return debug_daemon_client_;
  }

  mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>
      remote_cros_network_config_;
  std::vector<chromeos::network_diagnostics::mojom::GatewayCanBePingedProblem>
      problems_;
  // An unowned pointer to the DebugDaemonClient instance.
  raw_ptr<DebugDaemonClient, DanglingUntriaged> debug_daemon_client_;
  std::vector<std::string> gateways_;
  bool unreachable_gateways_ = true;
  int non_default_network_unsuccessful_ping_count_ = 0;
  // |non_default_network_latencies_| is responsible for storing the ping
  // latencies for all networks except the default network. The default network
  // latency is stored separately in |default_network_latency_|.
  std::vector<base::TimeDelta> non_default_network_latencies_;
  bool pingable_default_network_ = false;
  base::TimeDelta default_network_latency_;
  std::string default_network_guid_;
  std::string default_network_gateway_;
  int guids_remaining_ = 0;
  int gateways_remaining_ = 0;
};

}  // namespace network_diagnostics
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_GATEWAY_CAN_BE_PINGED_ROUTINE_H_
