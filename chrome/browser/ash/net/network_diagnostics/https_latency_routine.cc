// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/network_diagnostics/https_latency_routine.h"

#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/time/default_tick_clock.h"
#include "chrome/browser/ash/net/network_diagnostics/network_diagnostics_util.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/storage_partition.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/simple_host_resolver.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace ash::network_diagnostics {

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

HttpsLatencyRoutine::HttpsLatencyRoutine(mojom::RoutineCallSource source)
    : NetworkDiagnosticsRoutine(source),
      network_context_getter_(base::BindRepeating(&GetNetworkContext)),
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

  host_resolver_ = network::SimpleHostResolver::Create(network_context);

  GURL url = hostnames_to_query_dns_.back();
  hostnames_to_query_dns_.pop_back();

  // Resolver host parameter source must be unset or set to ANY in order for DNS
  // queries with BuiltInDnsClientEnabled policy disabled to work (b/353448388).
  network::mojom::ResolveHostParametersPtr parameters =
      network::mojom::ResolveHostParameters::New();
  parameters->dns_query_type = net::DnsQueryType::A;
  parameters->cache_usage =
      network::mojom::ResolveHostParameters::CacheUsage::DISALLOWED;

  // TODO(crbug.com/40235854): Consider passing a SchemeHostPort to trigger
  // HTTPS DNS resource record query. Unretained(this) is safe here because the
  // callback is invoked directly by |host_resolver_| which is owned by |this|.
  host_resolver_->ResolveHost(
      network::mojom::HostResolverHost::NewHostPortPair(
          net::HostPortPair::FromURL(url)),
      net::NetworkAnonymizationKey::CreateTransient(), std::move(parameters),
      base::BindOnce(&HttpsLatencyRoutine::OnHostResolutionComplete,
                     base::Unretained(this)));
}

void HttpsLatencyRoutine::OnHostResolutionComplete(
    int result,
    const net::ResolveErrorInfo&,
    const std::optional<net::AddressList>& resolved_addresses,
    const std::optional<net::HostResolverEndpointResults>&) {
  if (result != net::OK) {
    CHECK(!resolved_addresses);
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

}  // namespace ash::network_diagnostics
