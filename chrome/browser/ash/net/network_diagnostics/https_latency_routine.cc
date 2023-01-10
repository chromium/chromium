// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/network_diagnostics/https_latency_routine.h"

#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/time/default_tick_clock.h"
#include "chrome/browser/ash/net/network_diagnostics/network_diagnostics_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/storage_partition.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace ash {
namespace network_diagnostics {

namespace {

namespace mojom = ::chromeos::network_diagnostics::mojom;

constexpr int kTotalHostsToQuery = 3;
// The length of a random eight letter prefix.
constexpr int kHostPrefixLength = 8;
constexpr int kHttpsPort = 443;
constexpr char kHttpsScheme[] = "https://";
constexpr base::TimeDelta kRequestTimeoutMs = base::Milliseconds(5 * 1000);
// Requests taking longer than 1000 ms are problematic.
constexpr base::TimeDelta kProblemLatencyMs = base::Milliseconds(1000);
// Requests lasting between 500 ms and 1000 ms are potentially problematic.
constexpr base::TimeDelta kPotentialProblemLatencyMs = base::Milliseconds(500);

base::TimeDelta MedianLatency(std::vector<base::TimeDelta>& latencies) {
  if (latencies.size() == 0) {
    return base::TimeDelta::Max();
  }
  std::sort(latencies.begin(), latencies.end());
  if (latencies.size() % 2 != 0) {
    return latencies[latencies.size() / 2];
  }
  auto sum =
      latencies[latencies.size() / 2] + latencies[(latencies.size() + 1) / 2];
  return sum / 2.0;
}

network::mojom::NetworkContext* GetNetworkContext() {
  Profile* profile = util::GetUserProfile();

  return profile->GetDefaultStoragePartition()->GetNetworkContext();
}

std::unique_ptr<HttpRequestManager> GetHttpRequestManager() {
  return std::make_unique<HttpRequestManager>(util::GetUserProfile());
}

}  // namespace

class HttpsLatencyRoutine::HostResolver
    : public network::ResolveHostClientBase {
 public:
  HostResolver(network::mojom::NetworkContext* network_context,
               HttpsLatencyRoutine* https_latency);
  HostResolver(const HostResolver&) = delete;
  HostResolver& operator=(const HostResolver&) = delete;
  ~HostResolver() override;

  // network::mojom::ResolveHostClient:
  void OnComplete(int result,
                  const net::ResolveErrorInfo& resolve_error_info,
                  const absl::optional<net::AddressList>& resolved_addresses,
                  const absl::optional<net::HostResolverEndpointResults>&
                      endpoint_results_with_metadata) override;

  // Performs the DNS resolution.
  void Run(const GURL& url);

  network::mojom::NetworkContext* network_context() const {
    return network_context_;
  }

 private:
  void CreateHostResolver();
  void OnMojoConnectionError();

  network::mojom::NetworkContext* network_context_ = nullptr;  // Unowned
  HttpsLatencyRoutine* https_latency_;                         // Unowned
  mojo::Receiver<network::mojom::ResolveHostClient> receiver_{this};
  mojo::Remote<network::mojom::HostResolver> host_resolver_;
};

HttpsLatencyRoutine::HostResolver::HostResolver(
    network::mojom::NetworkContext* network_context,
    HttpsLatencyRoutine* https_latency)
    : network_context_(network_context), https_latency_(https_latency) {
  DCHECK(network_context_);
  DCHECK(https_latency_);
}

HttpsLatencyRoutine::HostResolver::~HostResolver() = default;

void HttpsLatencyRoutine::HostResolver::OnComplete(
    int result,
    const net::ResolveErrorInfo& resolve_error_info,
    const absl::optional<net::AddressList>& resolved_addresses,
    const absl::optional<net::HostResolverEndpointResults>&
        endpoint_results_with_metadata) {
  receiver_.reset();
  host_resolver_.reset();

  https_latency_->OnHostResolutionComplete(result, resolve_error_info,
                                           resolved_addresses,
                                           endpoint_results_with_metadata);
}

void HttpsLatencyRoutine::HostResolver::Run(const GURL& url) {
  CreateHostResolver();
  DCHECK(host_resolver_);
  DCHECK(!receiver_.is_bound());

  network::mojom::ResolveHostParametersPtr parameters =
      network::mojom::ResolveHostParameters::New();
  parameters->dns_query_type = net::DnsQueryType::A;
  parameters->source = net::HostResolverSource::DNS;
  parameters->cache_usage =
      network::mojom::ResolveHostParameters::CacheUsage::DISALLOWED;

  // TODO(crbug.com/1355169): Consider passing a SchemeHostPort to trigger HTTPS
  // DNS resource record query.
  host_resolver_->ResolveHost(network::mojom::HostResolverHost::NewHostPortPair(
                                  net::HostPortPair::FromURL(url)),
                              net::NetworkAnonymizationKey::CreateTransient(),
                              std::move(parameters),
                              receiver_.BindNewPipeAndPassRemote());
}

void HttpsLatencyRoutine::HostResolver::CreateHostResolver() {
  network_context()->CreateHostResolver(
      net::DnsConfigOverrides(), host_resolver_.BindNewPipeAndPassReceiver());
  // Disconnect handler will be invoked if the network service crashes.
  host_resolver_.set_disconnect_handler(base::BindOnce(
      &HostResolver::OnMojoConnectionError, base::Unretained(this)));
}

void HttpsLatencyRoutine::HostResolver::OnMojoConnectionError() {
  host_resolver_.reset();
  OnComplete(net::ERR_NAME_NOT_RESOLVED, net::ResolveErrorInfo(net::ERR_FAILED),
             /*resolved_addresses=*/absl::nullopt,
             /*endpoint_results_with_metadata=*/absl::nullopt);
}

HttpsLatencyRoutine::HttpsLatencyRoutine()
    : network_context_getter_(base::BindRepeating(&GetNetworkContext)),
      http_request_manager_getter_(base::BindRepeating(&GetHttpRequestManager)),
      tick_clock_(base::DefaultTickClock::GetInstance()),
      hostnames_to_query_dns_(
          util::GetRandomHostsWithSchemeAndPortAndGenerate204Path(
              kTotalHostsToQuery,
              kHostPrefixLength,
              kHttpsScheme,
              kHttpsPort)),
      hostnames_to_query_https_(hostnames_to_query_dns_) {
  DCHECK(network_context_getter_);
  DCHECK(http_request_manager_getter_);
  DCHECK(tick_clock_);
}

HttpsLatencyRoutine::~HttpsLatencyRoutine() = default;

mojom::RoutineType HttpsLatencyRoutine::Type() {
  return mojom::RoutineType::kHttpsLatency;
}

void HttpsLatencyRoutine::Run() {
  // Before making HTTPS requests to the hosts, add the IP addresses are added
  // to the DNS cache. This ensures the HTTPS latency does not include DNS
  // resolution time, allowing us to identify issues with HTTPS more precisely.
  AttemptNextResolution();
}

void HttpsLatencyRoutine::AnalyzeResultsAndExecuteCallback() {
  base::TimeDelta median_latency = MedianLatency(latencies_);
  if (!successfully_resolved_hosts_) {
    set_verdict(mojom::RoutineVerdict::kProblem);
    problems_.emplace_back(mojom::HttpsLatencyProblem::kFailedDnsResolutions);
  } else if (failed_connection_) {
    set_verdict(mojom::RoutineVerdict::kProblem);
    problems_.emplace_back(mojom::HttpsLatencyProblem::kFailedHttpsRequests);
  } else {
    auto https_latency_result_value =
        mojom::HttpsLatencyResultValue::New(median_latency);
    set_result_value(mojom::RoutineResultValue::NewHttpsLatencyResultValue(
        std::move(https_latency_result_value)));
    if (median_latency <= kProblemLatencyMs &&
        median_latency > kPotentialProblemLatencyMs) {
      set_verdict(mojom::RoutineVerdict::kProblem);
      problems_.emplace_back(mojom::HttpsLatencyProblem::kHighLatency);
    } else if (median_latency > kProblemLatencyMs) {
      set_verdict(mojom::RoutineVerdict::kProblem);
      problems_.emplace_back(mojom::HttpsLatencyProblem::kVeryHighLatency);
    } else {
      set_verdict(mojom::RoutineVerdict::kNoProblem);
    }
  }
  set_problems(mojom::RoutineProblems::NewHttpsLatencyProblems(problems_));
  ExecuteCallback();
}

void HttpsLatencyRoutine::AttemptNextResolution() {
  network::mojom::NetworkContext* network_context =
      network_context_getter_.Run();
  DCHECK(network_context);

  host_resolver_ = std::make_unique<HostResolver>(network_context, this);

  GURL url = hostnames_to_query_dns_.back();
  hostnames_to_query_dns_.pop_back();
  host_resolver_->Run(url);
}

void HttpsLatencyRoutine::OnHostResolutionComplete(
    int result,
    const net::ResolveErrorInfo& resolve_error_info,
    const absl::optional<net::AddressList>& resolved_addresses,
    const absl::optional<net::HostResolverEndpointResults>&
        endpoint_results_with_metadata) {
  bool success = result == net::OK && !resolved_addresses->empty() &&
                 resolved_addresses.has_value();
  if (!success) {
    successfully_resolved_hosts_ = false;
    AnalyzeResultsAndExecuteCallback();
    return;
  }
  if (hostnames_to_query_dns_.size() > 0) {
    AttemptNextResolution();
    return;
  }
  MakeHttpsRequest();
}

void HttpsLatencyRoutine::MakeHttpsRequest() {
  GURL url = hostnames_to_query_https_.back();
  hostnames_to_query_https_.pop_back();
  request_start_time_ = tick_clock_->NowTicks();
  http_request_manager_ = http_request_manager_getter_.Run();
  http_request_manager_->MakeRequest(
      url, kRequestTimeoutMs,
      base::BindOnce(&HttpsLatencyRoutine::OnHttpsRequestComplete, weak_ptr()));
}

void HttpsLatencyRoutine::OnHttpsRequestComplete(bool connected) {
  request_end_time_ = tick_clock_->NowTicks();
  if (!connected) {
    failed_connection_ = true;
    AnalyzeResultsAndExecuteCallback();
    return;
  }
  const base::TimeDelta latency = request_end_time_ - request_start_time_;
  latencies_.emplace_back(latency);
  if (hostnames_to_query_https_.size() > 0) {
    MakeHttpsRequest();
    return;
  }
  AnalyzeResultsAndExecuteCallback();
}

}  // namespace network_diagnostics
}  // namespace ash
