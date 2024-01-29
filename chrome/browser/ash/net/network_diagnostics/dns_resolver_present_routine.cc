// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/network_diagnostics/dns_resolver_present_routine.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "chromeos/ash/services/network_config/in_process_instance.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_util.h"
#include "components/onc/onc_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/ip_address.h"

namespace ash {
namespace network_diagnostics {

namespace {

namespace mojom = ::chromeos::network_diagnostics::mojom;

// Filters the list of |name_servers| and returns those that are not
// empty/default values.
std::vector<std::string> GetNonEmptyNameServers(
    const std::vector<std::string>& name_servers) {
  std::vector<std::string> non_empty_name_servers;
  for (const auto& name_server : name_servers) {
    if (name_server.empty() || name_server == "0.0.0.0" ||
        name_server == "::/0") {
      continue;
    }
    non_empty_name_servers.push_back(name_server);
  }
  return non_empty_name_servers;
}

// Checks that at least one name server IP address is valid depending on the IP
// config type. If the type is not set, IPv4 is assumed.
bool NameServersHaveValidAddresses(
    const std::vector<std::string>& name_servers,
    chromeos::network_config::mojom::IPConfigType type) {
  for (const auto& name_server : name_servers) {
    net::IPAddress ip_address;
    if (!ip_address.AssignFromIPLiteral(name_server)) {
      continue;
    }

    switch (type) {
      case chromeos::network_config::mojom::IPConfigType::kIPv4:
        if (ip_address.IsIPv4())
          return true;
        break;
      case chromeos::network_config::mojom::IPConfigType::kIPv6:
        if (ip_address.IsIPv6() || ip_address.IsIPv4MappedIPv6())
          return true;
        break;
    }
  }

  return false;
}

}  // namespace

DnsResolverPresentRoutine::DnsResolverPresentRoutine(
    chromeos::network_diagnostics::mojom::RoutineCallSource source)
    : NetworkDiagnosticsRoutine(source) {
  set_verdict(mojom::RoutineVerdict::kNotRun);
  network_config::BindToInProcessInstance(
      remote_cros_network_config_.BindNewPipeAndPassReceiver());
}

DnsResolverPresentRoutine::~DnsResolverPresentRoutine() = default;

bool DnsResolverPresentRoutine::CanRun() {
  DCHECK(remote_cros_network_config_);
  return true;
}

mojom::RoutineType DnsResolverPresentRoutine::Type() {
  return mojom::RoutineType::kDnsResolverPresent;
}

void DnsResolverPresentRoutine::Run() {
  FetchActiveNetworks();
}

void DnsResolverPresentRoutine::AnalyzeResultsAndExecuteCallback() {
  if (!connected_network_) {
    set_verdict(mojom::RoutineVerdict::kNotRun);
  } else if (!name_servers_are_found_) {
    set_verdict(mojom::RoutineVerdict::kProblem);
    problems_.emplace_back(
        mojom::DnsResolverPresentProblem::kNoNameServersFound);
  } else if (!name_servers_are_valid_) {
    set_verdict(mojom::RoutineVerdict::kProblem);
    problems_.emplace_back(
        mojom::DnsResolverPresentProblem::kMalformedNameServers);
  } else {
    // The availability of non-empty, well-formed nameservers ensures that DNS
    // resolution should be possible.
    set_verdict(mojom::RoutineVerdict::kNoProblem);
  }
  set_problems(
      mojom::RoutineProblems::NewDnsResolverPresentProblems(problems_));
  ExecuteCallback();
}

void DnsResolverPresentRoutine::FetchActiveNetworks() {
  DCHECK(remote_cros_network_config_);
  remote_cros_network_config_->GetNetworkStateList(
      chromeos::network_config::mojom::NetworkFilter::New(
          chromeos::network_config::mojom::FilterType::kActive,
          chromeos::network_config::mojom::NetworkType::kAll,
          chromeos::network_config::mojom::kNoLimit),
      base::BindOnce(&DnsResolverPresentRoutine::OnNetworkStateListReceived,
                     base::Unretained(this)));
}

void DnsResolverPresentRoutine::FetchManagedProperties(
    const std::string& guid) {
  remote_cros_network_config_->GetManagedProperties(
      guid,
      base::BindOnce(&DnsResolverPresentRoutine::OnManagedPropertiesReceived,
                     base::Unretained(this)));
}

void DnsResolverPresentRoutine::OnManagedPropertiesReceived(
    chromeos::network_config::mojom::ManagedPropertiesPtr managed_properties) {
  if (!managed_properties || !managed_properties->ip_configs.has_value()) {
    AnalyzeResultsAndExecuteCallback();
    return;
  }
  for (const auto& ip_config : managed_properties->ip_configs.value()) {
    if (ip_config->name_servers.has_value() &&
        ip_config->name_servers->size() != 0) {
      // Check that there are at least one set name server across the IPConfigs.
      std::vector<std::string> non_empty_name_servers =
          GetNonEmptyNameServers(ip_config->name_servers.value());
      if (non_empty_name_servers.empty()) {
        break;
      }
      name_servers_are_found_ = true;

      // Check that the name servers are valid
      if (NameServersHaveValidAddresses(non_empty_name_servers,
                                        ip_config->type)) {
        name_servers_are_valid_ = true;
        break;
      }
    }
  }
  AnalyzeResultsAndExecuteCallback();
}

// Process the network interface information.
void DnsResolverPresentRoutine::OnNetworkStateListReceived(
    std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
        networks) {
  std::string default_guid;
  for (const auto& network : networks) {
    if (chromeos::network_config::StateIsConnected(network->connection_state)) {
      default_guid = network->guid;
      break;
    }
  }
  // Since we are not connected, proceed to analyzing the results and executing
  // the completion callback.
  if (default_guid.empty()) {
    AnalyzeResultsAndExecuteCallback();
  } else {
    connected_network_ = true;
    FetchManagedProperties(default_guid);
  }
}

}  // namespace network_diagnostics
}  // namespace ash
