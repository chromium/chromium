// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NET_NETWORK_DIAGNOSTICS_HTTP_FIREWALL_ROUTINE_H_
#define CHROME_BROWSER_CHROMEOS_NET_NETWORK_DIAGNOSTICS_HTTP_FIREWALL_ROUTINE_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/optional.h"
#include "chrome/browser/chromeos/net/network_diagnostics/network_diagnostics_routine.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/address_list.h"
#include "net/log/net_log_with_source.h"
#include "services/network/public/cpp/resolve_host_client_base.h"
#include "services/network/public/mojom/host_resolver.mojom.h"

class Profile;

namespace net {
class ClientSocketFactory;
class TransportClientSocket;
}  // namespace net

namespace network {
namespace mojom {
class NetworkContext;
}
}  // namespace network

namespace chromeos {
namespace network_diagnostics {

// Tests whether a firewall is blocking HTTP port 80.
class HttpFirewallRoutine : public NetworkDiagnosticsRoutine {
 public:
  class HostResolver;
  using HttpFirewallRoutineCallback =
      mojom::NetworkDiagnosticsRoutines::HttpFirewallCallback;

  HttpFirewallRoutine();
  HttpFirewallRoutine(const HttpFirewallRoutine&) = delete;
  HttpFirewallRoutine& operator=(const HttpFirewallRoutine&) = delete;
  ~HttpFirewallRoutine() override;

  // NetworkDiagnosticsRoutine:
  void AnalyzeResultsAndExecuteCallback() override;

  // Run the core logic of this routine. Set |callback| to
  // |routine_completed_callback_|, which is to be executed in
  // AnalyzeResultsAndExecuteCallback().
  void RunRoutine(HttpFirewallRoutineCallback callback);

  // Processes the results of the DNS resolution done by |host_resolver_|.
  void OnHostResolutionComplete(
      int result,
      const net::ResolveErrorInfo& resolve_error_info,
      const base::Optional<net::AddressList>& resolved_addresses);

  void SetNetworkContextForTesting(
      network::mojom::NetworkContext* network_context);
  void SetProfileForTesting(Profile* profile);

  void set_client_socket_factory_for_testing(
      net::ClientSocketFactory* client_socket_factory) {
    client_socket_factory_ = client_socket_factory;
  }
  net::ClientSocketFactory* client_socket_factory() {
    return client_socket_factory_;
  }

 private:
  void AttemptNextResolution();
  void AttemptSocketConnections();
  void Connect(int socket_index);
  void OnSocketConnected(int socket_index, int result);

  // Unowned
  net::ClientSocketFactory* client_socket_factory_ = nullptr;
  int num_hostnames_to_query_;
  std::vector<std::string> hostnames_to_query_;
  static constexpr int kTotalNumRetries = 3;
  int num_retries_ = kTotalNumRetries;
  int socket_connection_failures_ = 0;
  int num_tcp_connections_attempted_ = 0;
  net::NetLogWithSource net_log_;
  std::vector<std::unique_ptr<net::TransportClientSocket>> sockets_;
  std::vector<net::AddressList> resolved_addresses_;
  std::vector<mojom::HttpFirewallProblem> problems_;
  std::unique_ptr<HostResolver> host_resolver_;
  HttpFirewallRoutineCallback routine_completed_callback_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace network_diagnostics
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NET_NETWORK_DIAGNOSTICS_HTTP_FIREWALL_ROUTINE_H_
