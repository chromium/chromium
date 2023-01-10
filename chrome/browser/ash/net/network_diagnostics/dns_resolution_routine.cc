// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/network_diagnostics/dns_resolution_routine.h"

#include <iterator>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/values.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
namespace network_diagnostics {

namespace {

namespace mojom = ::chromeos::network_diagnostics::mojom;

constexpr char kHostname[] = "ccd-testing-v4.gstatic.com";
constexpr int kHttpPort = 80;
// For an explanation of error codes, see "net/base/net_error_list.h".
constexpr int kRetryResponseCodes[] = {net::ERR_TIMED_OUT,
                                       net::ERR_DNS_TIMED_OUT};

Profile* GetUserProfile() {
  // Use sign-in profile if user has not logged in
  if (session_manager::SessionManager::Get()->IsUserSessionBlocked()) {
    return ProfileHelper::GetSigninProfile();
  }
  // Use primary profile if user is logged in
  return ProfileManager::GetPrimaryUserProfile();
}

}  // namespace

DnsResolutionRoutine::DnsResolutionRoutine() {
  profile_ = GetUserProfile();
  DCHECK(profile_);
  network_context_ =
      profile_->GetDefaultStoragePartition()->GetNetworkContext();
  DCHECK(network_context_);
  set_verdict(mojom::RoutineVerdict::kNotRun);
}

DnsResolutionRoutine::~DnsResolutionRoutine() = default;

mojom::RoutineType DnsResolutionRoutine::Type() {
  return mojom::RoutineType::kDnsResolution;
}

void DnsResolutionRoutine::Run() {
  CreateHostResolver();
  AttemptResolution();
}

void DnsResolutionRoutine::AnalyzeResultsAndExecuteCallback() {
  if (!resolved_address_received_) {
    set_verdict(mojom::RoutineVerdict::kProblem);
    problems_.emplace_back(mojom::DnsResolutionProblem::kFailedToResolveHost);
  } else {
    set_verdict(mojom::RoutineVerdict::kNoProblem);
  }

  set_problems(mojom::RoutineProblems::NewDnsResolutionProblems(problems_));
  ExecuteCallback();
}

void DnsResolutionRoutine::CreateHostResolver() {
  host_resolver_.reset();
  network_context()->CreateHostResolver(
      net::DnsConfigOverrides(), host_resolver_.BindNewPipeAndPassReceiver());
}

void DnsResolutionRoutine::OnMojoConnectionError() {
  host_resolver_.reset();
  OnComplete(net::ERR_NAME_NOT_RESOLVED, net::ResolveErrorInfo(net::ERR_FAILED),
             /*resolved_addresses=*/absl::nullopt,
             /*endpoint_results_with_metadata=*/absl::nullopt);
}

void DnsResolutionRoutine::AttemptResolution() {
  DCHECK(host_resolver_);
  DCHECK(!receiver_.is_bound());

  network::mojom::ResolveHostParametersPtr parameters =
      network::mojom::ResolveHostParameters::New();
  parameters->dns_query_type = net::DnsQueryType::A;
  parameters->source = net::HostResolverSource::DNS;
  parameters->cache_usage =
      network::mojom::ResolveHostParameters::CacheUsage::DISALLOWED;

  // Intentionally using a HostPortPair not to trigger ERR_DNS_NAME_HTTPS_ONLY
  // error while resolving http:// scheme host when a HTTPS resource record
  // exists.
  host_resolver_->ResolveHost(network::mojom::HostResolverHost::NewHostPortPair(
                                  net::HostPortPair(kHostname, kHttpPort)),
                              net::NetworkAnonymizationKey::CreateTransient(),
                              std::move(parameters),
                              receiver_.BindNewPipeAndPassRemote());
  // The host resolver is part of the network service, which may be run inside
  // the browser process (in-process) or a dedicated utility process
  // (out-of-process). If the network service crashes, the disconnect handler
  // below will be invoked. See README in services/network for more information.
  receiver_.set_disconnect_handler(base::BindOnce(
      &DnsResolutionRoutine::OnMojoConnectionError, base::Unretained(this)));
}

void DnsResolutionRoutine::OnComplete(
    int result,
    const net::ResolveErrorInfo& resolve_error_info,
    const absl::optional<net::AddressList>& resolved_addresses,
    const absl::optional<net::HostResolverEndpointResults>&
        endpoint_results_with_metadata) {
  receiver_.reset();

  if (result == net::OK && !resolved_addresses->empty() &&
      resolved_addresses.has_value()) {
    resolved_address_received_ = true;
    AnalyzeResultsAndExecuteCallback();
    return;
  }
  if (base::Contains(kRetryResponseCodes, result) && num_retries_ > 0) {
    num_retries_--;
    AttemptResolution();
  } else {
    AnalyzeResultsAndExecuteCallback();
  }
}

}  // namespace network_diagnostics
}  // namespace ash
