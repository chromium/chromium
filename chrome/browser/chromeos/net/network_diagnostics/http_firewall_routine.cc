// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/network_diagnostics/http_firewall_routine.h"

#include <iterator>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/no_destructor.h"
#include "base/optional.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/sequence_checker.h"
#include "base/time/default_tick_clock.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/base/network_isolation_key.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/transport_client_socket.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace chromeos {
namespace network_diagnostics {
namespace {

constexpr int kHttpPort = 80;
// The threshold describing the number of DNS resolution failures permitted.
// E.g. If 10 host resolution attempts are made, any less than eight
// successfully resolved hosts would result in a problem.
constexpr double kDnsResolutionSuccessRateThreshold = 0.8;
constexpr int kTotalAdditionalHostsToQuery = 3;
// The length of a random eight letter prefix obtained by the characters from
// |kPossibleChars|.
constexpr int kHostPrefixLength = 8;
constexpr char kHostSuffix[] = "-ccd-testing-v4.metric.gstatic.com";
// The threshold describing number of socket connection failures permitted. E.g.
// If connections to 10 sockets are attempted, any more than two failures would
// result in a problem.
constexpr double kSocketConnectionFailureRateThreshold = 0.2;
// For an explanation of error codes, see "net/base/net_error_list.h".
constexpr int kRetryResponseCodes[] = {net::ERR_TIMED_OUT,
                                       net::ERR_DNS_TIMED_OUT};

const std::string GetRandomString(int length) {
  std::string prefix;
  for (int i = 0; i < length; i++) {
    prefix += ('a' + base::RandInt(0, 25));
  }
  return prefix;
}

// Returns a list of random prefixes to prepend to |kHostSuffix| to create a
// complete hostname. By including a random prefix, we ensure with a very high
// probability that the DNS queries are done on unique hosts.
std::vector<std::string> GetHostnamesToQuery() {
  static const base::NoDestructor<std::vector<std::string>> fixed_hostnames(
      {"www.google.com", "mail.google.com", "drive.google.com",
       "accounts.google.com", "plus.google.com", "groups.google.com"});
  std::vector<std::string> hostnames_to_query(fixed_hostnames->begin(),
                                              fixed_hostnames->end());
  for (int i = 0; i < kTotalAdditionalHostsToQuery; i++) {
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

}  // namespace

class HttpFirewallRoutine::HostResolver
    : public network::ResolveHostClientBase {
 public:
  explicit HostResolver(HttpFirewallRoutine* http_firewall_routine);
  HostResolver(const HostResolver&) = delete;
  HostResolver& operator=(const HostResolver&) = delete;
  ~HostResolver() override;

  // network::mojom::ResolveHostClient:
  void OnComplete(
      int result,
      const net::ResolveErrorInfo& resolve_error_info,
      const base::Optional<net::AddressList>& resolved_addresses) override;

  // Performs the DNS resolution.
  void Run(const std::string& hostname);

  network::mojom::NetworkContext* network_context() { return network_context_; }
  Profile* profile() { return profile_; }
  void set_network_context_for_testing(
      network::mojom::NetworkContext* network_context) {
    network_context_ = network_context;
  }
  void set_profile_for_testing(Profile* profile) { profile_ = profile; }

 private:
  void CreateHostResolver();
  void OnMojoConnectionError();

  Profile* profile_ = nullptr;                                 // Unowned
  network::mojom::NetworkContext* network_context_ = nullptr;  // Unowned
  HttpFirewallRoutine* http_firewall_routine_;                 // Unowned
  mojo::Receiver<network::mojom::ResolveHostClient> receiver_{this};
  mojo::Remote<network::mojom::HostResolver> host_resolver_;
};

HttpFirewallRoutine::HostResolver::HostResolver(
    HttpFirewallRoutine* http_firewall_routine)
    : http_firewall_routine_(http_firewall_routine) {
  DCHECK(http_firewall_routine);

  profile_ = GetUserProfile();
  network_context_ =
      content::BrowserContext::GetDefaultStoragePartition(profile_)
          ->GetNetworkContext();
  DCHECK(network_context_);
}

HttpFirewallRoutine::HostResolver::~HostResolver() = default;

void HttpFirewallRoutine::HostResolver::OnComplete(
    int result,
    const net::ResolveErrorInfo& resolve_error_info,
    const base::Optional<net::AddressList>& resolved_addresses) {
  receiver_.reset();

  http_firewall_routine_->OnHostResolutionComplete(result, resolve_error_info,
                                                   resolved_addresses);
}

void HttpFirewallRoutine::HostResolver::Run(const std::string& hostname) {
  if (!host_resolver_) {
    CreateHostResolver();
  }
  DCHECK(host_resolver_);
  DCHECK(!receiver_.is_bound());

  network::mojom::ResolveHostParametersPtr parameters =
      network::mojom::ResolveHostParameters::New();
  parameters->dns_query_type = net::DnsQueryType::A;
  parameters->source = net::HostResolverSource::DNS;
  parameters->cache_usage =
      network::mojom::ResolveHostParameters::CacheUsage::DISALLOWED;

  host_resolver_->ResolveHost(net::HostPortPair(hostname, kHttpPort),
                              net::NetworkIsolationKey::CreateTransient(),
                              std::move(parameters),
                              receiver_.BindNewPipeAndPassRemote());
  receiver_.set_disconnect_handler(base::BindOnce(
      &HostResolver::OnMojoConnectionError, base::Unretained(this)));
}

void HttpFirewallRoutine::HostResolver::CreateHostResolver() {
  host_resolver_.reset();
  network_context()->CreateHostResolver(
      net::DnsConfigOverrides(), host_resolver_.BindNewPipeAndPassReceiver());
}

void HttpFirewallRoutine::HostResolver::OnMojoConnectionError() {
  CreateHostResolver();
  OnComplete(net::ERR_NAME_NOT_RESOLVED, net::ResolveErrorInfo(net::ERR_FAILED),
             base::nullopt);
}

HttpFirewallRoutine::HttpFirewallRoutine() {
  DETACH_FROM_SEQUENCE(sequence_checker_);

  host_resolver_ = std::make_unique<HostResolver>(this);
  hostnames_to_query_ = GetHostnamesToQuery();
  num_hostnames_to_query_ = hostnames_to_query_.size();
  client_socket_factory_ = net::ClientSocketFactory::GetDefaultFactory();
  set_verdict(mojom::RoutineVerdict::kNotRun);
}

HttpFirewallRoutine::~HttpFirewallRoutine() {
  hostnames_to_query_.clear();
}

void HttpFirewallRoutine::RunRoutine(HttpFirewallRoutineCallback callback) {
  if (!CanRun()) {
    std::move(callback).Run(verdict(), std::move(problems_));
    return;
  }
  routine_completed_callback_ = std::move(callback);
  AttemptNextResolution();
}

void HttpFirewallRoutine::AnalyzeResultsAndExecuteCallback() {
  double dns_resolution_success_rate =
      static_cast<double>(resolved_addresses_.size()) /
      static_cast<double>(num_hostnames_to_query_);
  double socket_connections_failure_rate =
      static_cast<double>(socket_connection_failures_) /
      static_cast<double>(num_hostnames_to_query_);

  if (dns_resolution_success_rate < kDnsResolutionSuccessRateThreshold) {
    set_verdict(mojom::RoutineVerdict::kProblem);
    problems_.emplace_back(
        mojom::HttpFirewallProblem::kDnsResolutionFailuresAboveThreshold);
  } else if (socket_connections_failure_rate <
             kSocketConnectionFailureRateThreshold) {
    set_verdict(mojom::RoutineVerdict::kNoProblem);
  } else if (socket_connection_failures_ == num_hostnames_to_query_) {
    set_verdict(mojom::RoutineVerdict::kProblem);
    problems_.emplace_back(mojom::HttpFirewallProblem::kFirewallDetected);
  } else {
    // It cannot be conclusively determined whether a firewall exists; however,
    // since reaching this case means socket_connection_failure_rate >
    // kSocketConnectionFailureRateThreshold, a firewall could potentially
    // exist.
    set_verdict(mojom::RoutineVerdict::kProblem);
    problems_.emplace_back(mojom::HttpFirewallProblem::kPotentialFirewall);
  }
  std::move(routine_completed_callback_).Run(verdict(), std::move(problems_));
}

void HttpFirewallRoutine::AttemptNextResolution() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::string hostname = hostnames_to_query_.back();
  hostnames_to_query_.pop_back();
  host_resolver_->Run(hostname);
}

void HttpFirewallRoutine::OnHostResolutionComplete(
    int result,
    const net::ResolveErrorInfo& resolve_error_info,
    const base::Optional<net::AddressList>& resolved_addresses) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool success = result == net::OK && !resolved_addresses->empty() &&
                 resolved_addresses.has_value();
  if (success) {
    resolved_addresses_.emplace_back(resolved_addresses.value());
  }
  if (hostnames_to_query_.size() > 0) {
    AttemptNextResolution();
  } else {
    AttemptSocketConnections();
  }
}

void HttpFirewallRoutine::SetNetworkContextForTesting(
    network::mojom::NetworkContext* network_context) {
  host_resolver_->set_network_context_for_testing(network_context);
}

void HttpFirewallRoutine::SetProfileForTesting(Profile* profile) {
  host_resolver_->set_profile_for_testing(profile);
}

void HttpFirewallRoutine::AttemptSocketConnections() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Verify whether the number of failed DNS resolutions is below the threshold.
  double dns_resolution_success_rate =
      static_cast<double>(resolved_addresses_.size()) /
      static_cast<double>(num_hostnames_to_query_);
  if (dns_resolution_success_rate < kDnsResolutionSuccessRateThreshold) {
    AnalyzeResultsAndExecuteCallback();
    return;
  }

  // Create a socket for each address and port combination.
  for (auto& resolved_address : resolved_addresses_) {
    // Ensure the IP addresses stored for every address list (denoted by
    // resolved_address) is using port 80.
    resolved_address =
        net::AddressList::CopyWithPort(resolved_address, kHttpPort);

    // TODO(crbug.com/1123197): Pass non-null NetworkQualityEstimator.
    net::NetworkQualityEstimator* network_quality_estimator = nullptr;

    sockets_.emplace_back(client_socket_factory_->CreateTransportClientSocket(
        resolved_address, nullptr, network_quality_estimator,
        net_log_.net_log(), net_log_.source()));
  }

  // Connect the sockets.
  for (int i = 0; i < static_cast<int>(sockets_.size()); i++) {
    Connect(i);
  }
}

void HttpFirewallRoutine::Connect(int socket_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  int result = sockets_[socket_index]->Connect(
      base::BindOnce(&HttpFirewallRoutine::OnSocketConnected,
                     base::Unretained(this), socket_index));
  if (result != net::ERR_IO_PENDING) {
    OnSocketConnected(socket_index, result);
  }
}
void HttpFirewallRoutine::OnSocketConnected(int socket_index, int result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto* iter = base::ranges::find(kRetryResponseCodes, result);
  if (iter != std::end(kRetryResponseCodes) && num_retries_ > 0) {
    num_retries_--;
    // Disconnect the socket in case there is any data in the incoming buffer.
    sockets_[socket_index]->Disconnect();
    Connect(socket_index);
    return;
  }

  if (result < 0) {
    socket_connection_failures_++;
  }
  num_tcp_connections_attempted_++;
  if (num_tcp_connections_attempted_ == num_hostnames_to_query_) {
    AnalyzeResultsAndExecuteCallback();
  }
}

}  // namespace network_diagnostics
}  // namespace chromeos
