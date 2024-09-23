// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/network_diagnostics/dns_latency_routine.h"

#include <iterator>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/rand_util.h"
#include "base/time/default_tick_clock.h"
#include "chrome/browser/ash/net/network_diagnostics/network_diagnostics_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "services/network/public/cpp/simple_host_resolver.h"

namespace base {
class TimeTicks;
}

namespace ash::network_diagnostics {

namespace {

namespace mojom = ::chromeos::network_diagnostics::mojom;

constexpr int kHttpPort = 80;
constexpr int kTotalHostsToQuery = 3;
// The length of a random eight letter prefix obtained by the characters from
// |kPossibleChars|.
constexpr int kHostPrefixLength = 8;
constexpr base::TimeDelta kBadLatencyMs =
    base::Milliseconds(util::kDnsPotentialProblemLatencyMs);
constexpr base::TimeDelta kVeryBadLatencyMs =
    base::Milliseconds(util::kDnsProblemLatencyMs);
constexpr char kHostSuffix[] = "-ccd-testing-v4.metric.gstatic.com";

const std::string GetRandomString(int length) {
  std::string prefix;
  for (int i = 0; i < length; i++) {
    prefix += ('a' + base::RandInt(0, 25));
  }
  return prefix;
}

// Use GetRandomString() to retrieve a random prefix. This random prefix is
// prepended to the |kHostSuffix|, resulting in a complete hostname. By
// including a random prefix, we ensure with a very high probability that the
// DNS queries are done on unique hosts.
std::vector<std::string> GetRandomHostnamesToQuery() {
  std::vector<std::string> hostnames_to_query;
  for (int i = 0; i < kTotalHostsToQuery; i++) {
    hostnames_to_query.emplace_back(GetRandomString(kHostPrefixLength) +
                                    kHostSuffix);
  }
  return hostnames_to_query;
}

Profile* GetUserProfile() {
  // Use sign-in profile if user has not logged in
  if (session_manager::SessionManager::Get()->IsUserSessionBlocked()) {
    return ProfileHelper::GetSigninProfile();
  }
  // Use primary profile if user is logged in
  return ProfileManager::GetPrimaryUserProfile();
}

double AverageLatency(const std::vector<base::TimeDelta>& latencies) {
  double total_latency = 0.0;
  for (base::TimeDelta latency : latencies) {
    total_latency += latency.InMillisecondsF();
  }
  return total_latency / latencies.size();
}

}  // namespace

DnsLatencyRoutine::DnsLatencyRoutine(mojom::RoutineCallSource source)
    : NetworkDiagnosticsRoutine(source),
      tick_clock_(base::DefaultTickClock::GetInstance()) {
  profile_ = GetUserProfile();
  network_context_ =
      profile_->GetDefaultStoragePartition()->GetNetworkContext();
  DCHECK(network_context_);
  set_verdict(mojom::RoutineVerdict::kNotRun);
}

DnsLatencyRoutine::~DnsLatencyRoutine() = default;

mojom::RoutineType DnsLatencyRoutine::Type() {
  return mojom::RoutineType::kDnsLatency;
}

void DnsLatencyRoutine::Run() {
  CreateHostResolver();
  hostnames_to_query_ = GetRandomHostnamesToQuery();
  AttemptNextResolution();
}

void DnsLatencyRoutine::AnalyzeResultsAndExecuteCallback() {
  double average_latency = AverageLatency(latencies_);
  if (!successfully_resolved_all_addresses_ || average_latency == 0.0) {
    set_verdict(mojom::RoutineVerdict::kProblem);
    problems_.emplace_back(mojom::DnsLatencyProblem::kHostResolutionFailure);
  } else if (average_latency > kBadLatencyMs.InMillisecondsF() &&
             average_latency <= kVeryBadLatencyMs.InMillisecondsF()) {
    set_verdict(mojom::RoutineVerdict::kProblem);
    problems_.emplace_back(mojom::DnsLatencyProblem::kSlightlyAboveThreshold);
  } else if (average_latency > kVeryBadLatencyMs.InMillisecondsF()) {
    set_verdict(mojom::RoutineVerdict::kProblem);
    problems_.emplace_back(
        mojom::DnsLatencyProblem::kSignificantlyAboveThreshold);
  } else {
    set_verdict(mojom::RoutineVerdict::kNoProblem);
  }

  set_problems(mojom::RoutineProblems::NewDnsLatencyProblems(problems_));
  ExecuteCallback();
}

void DnsLatencyRoutine::CreateHostResolver() {
  CHECK(!host_resolver_);
  host_resolver_ = network::SimpleHostResolver::Create(network_context());
}

void DnsLatencyRoutine::AttemptNextResolution() {
  CHECK(host_resolver_);

  std::string hostname = hostnames_to_query_.back();
  hostnames_to_query_.pop_back();

  // Resolver host parameter source must be unset or set to ANY in order for DNS
  // queries with BuiltInDnsClientEnabled policy disabled to work (b/353448388).
  network::mojom::ResolveHostParametersPtr parameters =
      network::mojom::ResolveHostParameters::New();
  parameters->dns_query_type = net::DnsQueryType::A;
  parameters->cache_usage =
      network::mojom::ResolveHostParameters::CacheUsage::DISALLOWED;

  start_resolution_time_ = tick_clock_->NowTicks();

  // Intentionally using a HostPortPair not to trigger ERR_DNS_NAME_HTTPS_ONLY
  // error while resolving http:// scheme host when a HTTPS resource record
  // exists.
  // Unretained(this) is safe here because the callback is invoked directly by
  // |host_resolver_| which is owned by |this|.
  host_resolver_->ResolveHost(
      network::mojom::HostResolverHost::NewHostPortPair(
          net::HostPortPair(hostname, kHttpPort)),
      net::NetworkAnonymizationKey::CreateTransient(), std::move(parameters),
      base::BindOnce(&DnsLatencyRoutine::OnComplete, base::Unretained(this)));
}

void DnsLatencyRoutine::OnComplete(
    int result,
    const net::ResolveErrorInfo& resolve_error_info,
    const std::optional<net::AddressList>& resolved_addresses,
    const std::optional<net::HostResolverEndpointResults>&
        endpoint_results_with_metadata) {
  resolution_complete_time_ = tick_clock_->NowTicks();
  const base::TimeDelta latency =
      resolution_complete_time_ - start_resolution_time_;

  if (result != net::OK) {
    CHECK(!resolved_addresses);
    // Failed to get resolved address of host
    AnalyzeResultsAndExecuteCallback();
  } else if (hostnames_to_query_.size() > 0) {
    latencies_.emplace_back(latency);
    AttemptNextResolution();
  } else {
    latencies_.emplace_back(latency);
    successfully_resolved_all_addresses_ = true;
    AnalyzeResultsAndExecuteCallback();
  }
}

}  // namespace ash::network_diagnostics
