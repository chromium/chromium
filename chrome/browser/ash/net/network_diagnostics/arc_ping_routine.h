// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_ARC_PING_ROUTINE_H_
#define CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_ARC_PING_ROUTINE_H_

#include <string>
#include <vector>

#include "ash/components/arc/mojom/net.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/net/network_diagnostics/network_diagnostics_routine.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {
namespace network_diagnostics {

// Performs ICMP echo requests from within ARC to a random set of addresses
// and returns the result.
class ArcPingRoutine : public NetworkDiagnosticsRoutine {
 public:
  using RunArcHttpCallback = chromeos::network_diagnostics::mojom::
      NetworkDiagnosticsRoutines::RunArcPingCallback;

  explicit ArcPingRoutine(
      chromeos::network_diagnostics::mojom::RoutineCallSource source);
  ArcPingRoutine(const ArcPingRoutine&) = delete;
  ArcPingRoutine& operator=(const ArcPingRoutine&) = delete;
  ~ArcPingRoutine() override;

  // NetworkDiagnosticsRoutine:
  chromeos::network_diagnostics::mojom::RoutineType Type() override;
  void Run() override;
  void AnalyzeResultsAndExecuteCallback() override;

  void set_net_instance_for_testing(arc::mojom::NetInstance* net_instance) {
    net_instance_ = net_instance;
  }

 private:
  // Get list of Network properties that are active.
  void FetchActiveNetworks();

  // Fetch the managed properties of each network given GUIDs.
  void FetchManagedProperties(const std::vector<std::string>& guids);

  // Call the PingTest API on each gateway.
  void PingGateways();

  // Gather the gateways from the managed properties of each network.
  void OnManagedPropertiesReceived(
      chromeos::network_config::mojom::ManagedPropertiesPtr managed_properties);

  // Get the managed properties of each network given the network interface
  // information.
  void OnNetworkStateListReceived(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
          networks);

  // Attempts the next ICMP echo request.
  void AttemptNextRequest();

  // Gets NetInstance service if it is not already set for testing
  // purposes as |net_instance_|.
  arc::mojom::NetInstance* GetNetInstance();

  // Gets the transport name of the network that is acceptable by ARC
  // from the ManagedProperties name.
  std::string GetTransportName(const std::string& managed_properties_name);

  // Processes the |result| of an PingTest API call.
  void OnRequestComplete(bool is_default_network_ping_result,
                         arc::mojom::ArcPingTestResultPtr result);

  // Handles timeout for GetManagedProperties.
  void HandleTimeout();

  mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>
      remote_cros_network_config_;
  std::vector<chromeos::network_diagnostics::mojom::ArcPingProblem> problems_;
  std::vector<std::string> gateways_;
  std::vector<std::string> gateways_transport_names_;
  bool get_managed_properties_timeout_failure_ = false;
  bool unreachable_gateways_ = true;
  bool non_default_network_unsuccessful_ = false;
  bool pingable_default_network_ = false;
  bool failed_to_get_arc_service_manager_ = false;
  bool failed_to_get_net_instance_service_for_ping_test_ = false;
  int64_t default_network_latency_ = 0;
  int64_t non_default_max_latency_ = 0;
  std::string default_network_guid_;
  std::string default_network_gateway_;
  int guids_remaining_ = 0;
  int gateways_remaining_ = 0;
  raw_ptr<arc::mojom::NetInstance, DanglingUntriaged> net_instance_ = nullptr;
  base::WeakPtrFactory<ArcPingRoutine> weak_ptr_factory_{this};
};

}  // namespace network_diagnostics
}  // namespace ash

#endif  //  CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_ARC_PING_ROUTINE_H_
