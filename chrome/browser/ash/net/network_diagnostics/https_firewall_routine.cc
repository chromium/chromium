// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/network_diagnostics/https_firewall_routine.h"

#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/ash/net/network_diagnostics/network_diagnostics_util.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace ash {
namespace network_diagnostics {

namespace {

namespace mojom = ::chromeos::network_diagnostics::mojom;

// Https port number.
constexpr int kHttpsPort = 443;
// Https scheme.
constexpr char kHttpsScheme[] = "https://";
// Effetively, the number of random hosts to query.
// total hosts queried by this routine = random hosts + fixed hosts, where
// information about fixed hosts is found in network_diagnostics_util.cc.
constexpr int kTotalAdditionalHostsToQuery = 3;
// The length of a random eight letter prefix obtained by the characters from
// |kPossibleChars|.
constexpr int kHostPrefixLength = 8;
// The threshold describing the number of DNS resolution failures permitted.
// E.g. If 10 host resolution attempts are made, any more than two DNS failures
// would result in a problem.
constexpr double kDnsResolutionFailureRateThreshold = 0.2;
// The threshold describing number of TLS probe failures permitted. E.g. If 10
// TLS probes are attempted, any more than two failures would result in a
// problem. This number does not take into account the number of TLS probes that
// failed due to unsuccessful DNS resolution.
constexpr double kTlsProbeFailureRateThreshold = 0.2;
// For an explanation of error codes, see "net/base/net_error_list.h".
constexpr int kRetryResponseCodes[] = {net::ERR_TIMED_OUT,
                                       net::ERR_DNS_TIMED_OUT};

}  // namespace

const int kTotalNumRetries = 3;

HttpsFirewallRoutine::HttpsFirewallRoutine(mojom::RoutineCallSource source)
    : NetworkDiagnosticsRoutine(source),
      num_retries_(kTotalNumRetries),
      tls_prober_getter_callback_(base::BindRepeating(
          &HttpsFirewallRoutine::CreateAndExecuteTlsProber)) {
  std::vector<std::string> url_strings =
      util::GetRandomAndFixedHostsWithSchemeAndPort(
          kTotalAdditionalHostsToQuery, kHostPrefixLength, kHttpsScheme,
          kHttpsPort);
  for (const auto& url_string : url_strings) {
    urls_to_query_.push_back(GURL(url_string));
  }
  num_urls_to_query_ = urls_to_query_.size();
}

HttpsFirewallRoutine::~HttpsFirewallRoutine() = default;

mojom::RoutineType HttpsFirewallRoutine::Type() {
  return mojom::RoutineType::kHttpsFirewall;
}

void HttpsFirewallRoutine::Run() {
  ProbeNextUrl();
}

void HttpsFirewallRoutine::AnalyzeResultsAndExecuteCallback() {
  // There should at least `kTotalAdditionalHostsToQuery` (=3) URLs to query.
  DCHECK(num_urls_to_query_);
  double dns_resolution_failure_rate =
      static_cast<double>(dns_resolution_failures_) /
      static_cast<double>(num_urls_to_query_);

  if (dns_resolution_failure_rate > kDnsResolutionFailureRateThreshold) {
    set_verdict(mojom::RoutineVerdict::kProblem);
    problems_.push_back(
        mojom::HttpsFirewallProblem::kHighDnsResolutionFailureRate);
  } else {
    // When `dns_resolution_failure_rate` is below the threshold, there must be
    // probes that is not "DNS failure".
    DCHECK(num_no_dns_failure_tls_probes_attempted_);
    double tls_probe_failure_rate =
        static_cast<double>(tls_probe_failures_) /
        static_cast<double>(num_no_dns_failure_tls_probes_attempted_);
    if (tls_probe_failure_rate <= kTlsProbeFailureRateThreshold) {
      set_verdict(mojom::RoutineVerdict::kNoProblem);
    } else if (tls_probe_failures_ ==
               num_no_dns_failure_tls_probes_attempted_) {
      set_verdict(mojom::RoutineVerdict::kProblem);
      problems_.push_back(mojom::HttpsFirewallProblem::kFirewallDetected);
    } else {
      // It cannot be conclusively determined whether a firewall exists;
      // however, since reaching this case means tls_probe_failure_rate >
      // kTlsProbeFailureRateThreshold, a firewall could potentially
      // exist.
      set_verdict(mojom::RoutineVerdict::kProblem);
      problems_.push_back(mojom::HttpsFirewallProblem::kPotentialFirewall);
    }
  }
  set_problems(mojom::RoutineProblems::NewHttpsFirewallProblems(problems_));
  ExecuteCallback();
}

void HttpsFirewallRoutine::ProbeNextUrl() {
  DCHECK(urls_to_query_.size() > 0);

  auto url = urls_to_query_.back();
  urls_to_query_.pop_back();
  AttemptProbe(url);
}

void HttpsFirewallRoutine::AttemptProbe(const GURL& url) {
  // Store the instance of TlsProber.
  tls_prober_ = tls_prober_getter_callback_.Run(
      base::BindRepeating(&HttpsFirewallRoutine::GetNetworkContext),
      net::HostPortPair::FromURL(url),
      /*negotiate_tls=*/true,
      base::BindOnce(&HttpsFirewallRoutine::OnProbeComplete, weak_ptr(), url));
}

void HttpsFirewallRoutine::OnProbeComplete(
    const GURL& url,
    int result,
    TlsProber::ProbeExitEnum probe_exit_enum) {
  if (probe_exit_enum == TlsProber::ProbeExitEnum::kDnsFailure) {
    dns_resolution_failures_++;
  } else {
    const auto* iter = base::ranges::find(kRetryResponseCodes, result);
    if (iter != std::end(kRetryResponseCodes) && num_retries_ > 0) {
      num_retries_--;
      AttemptProbe(url);
      return;
    }
    if (result < 0) {
      tls_probe_failures_++;
    }
    num_no_dns_failure_tls_probes_attempted_++;
  }
  if (urls_to_query_.size() == 0) {
    AnalyzeResultsAndExecuteCallback();
    return;
  }
  ProbeNextUrl();
}

network::mojom::NetworkContext* HttpsFirewallRoutine::GetNetworkContext() {
  Profile* profile = util::GetUserProfile();
  DCHECK(profile);

  return profile->GetDefaultStoragePartition()->GetNetworkContext();
}

std::unique_ptr<TlsProber> HttpsFirewallRoutine::CreateAndExecuteTlsProber(
    network::NetworkContextGetter network_context_getter,
    net::HostPortPair host_port_pair,
    bool negotiate_tls,
    TlsProber::TlsProbeCompleteCallback callback) {
  return std::make_unique<TlsProber>(std::move(network_context_getter),
                                     host_port_pair, negotiate_tls,
                                     std::move(callback));
}

}  // namespace network_diagnostics
}  // namespace ash
