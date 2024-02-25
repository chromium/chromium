// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_HTTPS_LATENCY_ROUTINE_H_
#define CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_HTTPS_LATENCY_ROUTINE_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ash/net/network_diagnostics/http_request_manager.h"
#include "chrome/browser/ash/net/network_diagnostics/network_diagnostics_routine.h"
#include "net/base/address_list.h"
#include "net/dns/public/host_resolver_results.h"
#include "net/dns/public/resolve_error_info.h"
#include "services/network/public/cpp/network_context_getter.h"
#include "services/network/public/mojom/host_resolver.mojom.h"
#include "url/gurl.h"

class HttpRequestManager;

namespace base {
class TickClock;
}  // namespace base

namespace network {
class SimpleHostResolver;
}  // namespace network

namespace ash::network_diagnostics {

// Tests whether the HTTPS latency is within established tolerance levels for
// the system.
class HttpsLatencyRoutine : public NetworkDiagnosticsRoutine {
 public:
  using HttpRequestManagerGetter =
      base::RepeatingCallback<std::unique_ptr<HttpRequestManager>()>;

  explicit HttpsLatencyRoutine(
      chromeos::network_diagnostics::mojom::RoutineCallSource source);
  HttpsLatencyRoutine(const HttpsLatencyRoutine&) = delete;
  HttpsLatencyRoutine& operator=(const HttpsLatencyRoutine&) = delete;
  ~HttpsLatencyRoutine() override;

  // NetworkDiagnosticsRoutine:
  chromeos::network_diagnostics::mojom::RoutineType Type() override;
  void Run() override;
  void AnalyzeResultsAndExecuteCallback() override;

  // Processes the results of the DNS resolution done by |host_resolver_|.

  // Sets the NetworkContextGetter for testing.
  void set_network_context_getter(
      network::NetworkContextGetter network_context_getter) {
    network_context_getter_ = std::move(network_context_getter);
  }

  // Sets the HttpRequestManager for testing.
  void set_http_request_manager_getter(
      HttpRequestManagerGetter http_request_manager_getter) {
    http_request_manager_getter_ = std::move(http_request_manager_getter);
  }

  // Mimics actual time conditions.
  void set_tick_clock_for_testing(const base::TickClock* tick_clock) {
    tick_clock_ = tick_clock;
  }

 private:
  void OnHostResolutionComplete(
      int result,
      const net::ResolveErrorInfo&,
      const std::optional<net::AddressList>& resolved_addresses,
      const std::optional<net::HostResolverEndpointResults>&);

  // Attempts the next DNS resolution.
  void AttemptNextResolution();

  // Makes an HTTPS request to the host.
  void MakeHttpsRequest();

  // Processes the results of an HTTPS request.
  void OnHttpsRequestComplete(bool connected);

  // Returns the weak pointer to |this|.
  base::WeakPtr<HttpsLatencyRoutine> weak_ptr() {
    return weak_factory_.GetWeakPtr();
  }

  network::NetworkContextGetter network_context_getter_;
  HttpRequestManagerGetter http_request_manager_getter_;
  bool successfully_resolved_hosts_ = true;
  bool failed_connection_ = false;
  raw_ptr<const base::TickClock, DanglingUntriaged> tick_clock_ =
      nullptr;  // Unowned
  base::TimeTicks request_start_time_;
  base::TimeTicks request_end_time_;
  std::vector<GURL> hostnames_to_query_dns_;
  std::vector<GURL> hostnames_to_query_https_;
  std::vector<base::TimeDelta> latencies_;
  std::unique_ptr<network::SimpleHostResolver> host_resolver_;
  std::unique_ptr<HttpRequestManager> http_request_manager_;
  std::vector<chromeos::network_diagnostics::mojom::HttpsLatencyProblem>
      problems_;
  base::WeakPtrFactory<HttpsLatencyRoutine> weak_factory_{this};
};

}  // namespace ash::network_diagnostics

#endif  // CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_HTTPS_LATENCY_ROUTINE_H_
