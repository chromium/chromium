// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/network_diagnostics/dns_resolver_present_routine.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "chromeos/services/network_config/in_process_instance.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_util.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromeos {
namespace network_diagnostics {
namespace {

bool NameServersAreWellFormed(const std::vector<std::string>& name_servers) {
  for (const auto& name_server : name_servers) {
    if (name_server == "0.0.0.0" || name_server == "::/0") {
      return false;
    }
  }
  return true;
}

bool NameServersAreNonEmpty(const std::vector<std::string>& name_servers) {
  for (const auto& name_server : name_servers) {
    if (name_server.empty()) {
      return false;
    }
  }
  return true;
}

}  // namespace

DnsResolverPresentRoutine::DnsResolverPresentRoutine() {
  set_verdict(mojom::RoutineVerdict::kNotRun);
  network_config::BindToInProcessInstance(
      remote_cros_network_config_.BindNewPipeAndPassReceiver());
}

DnsResolverPresentRoutine::~DnsResolverPresentRoutine() = default;

bool DnsResolverPresentRoutine::CanRun() {
  DCHECK(remote_cros_network_config_);
  return true;
}

void DnsResolverPresentRoutine::RunRoutine(
    mojom::NetworkDiagnosticsRoutines::DnsResolverPresentCallback callback) {
  if (!CanRun()) {
    std::move(callback).Run(verdict(), std::move(problems_));
    return;
  }
  routine_completed_callback_ = std::move(callback);
  FetchActiveNetworks();
}

void DnsResolverPresentRoutine::AnalyzeResultsAndExecuteCallback() {
  if (!name_servers_found_) {
    set_verdict(mojom::RoutineVerdict::kProblem);
    problems_.emplace_back(
        mojom::DnsResolverPresentProblem::kNoNameServersFound);
  } else if (!non_empty_name_servers_) {
    set_verdict(mojom::RoutineVerdict::kProblem);
    problems_.emplace_back(mojom::DnsResolverPresentProblem::kEmptyNameServers);
  } else if (!well_formed_name_servers_) {
    set_verdict(mojom::RoutineVerdict::kProblem);
    problems_.emplace_back(
        mojom::DnsResolverPresentProblem::kMalformedNameServers);
  } else {
    // The availability of non-empty, well-formed nameservers ensures that DNS
    // resolution should be possible.
    set_verdict(mojom::RoutineVerdict::kNoProblem);
  }
  std::move(routine_completed_callback_).Run(verdict(), std::move(problems_));
}

void DnsResolverPresentRoutine::FetchActiveNetworks() {
  DCHECK(remote_cros_network_config_);
  remote_cros_network_config_->GetNetworkStateList(
      network_config::mojom::NetworkFilter::New(
          network_config::mojom::FilterType::kActive,
          network_config::mojom::NetworkType::kAll,
          network_config::mojom::kNoLimit),
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
    network_config::mojom::ManagedPropertiesPtr managed_properties) {
  if (!managed_properties || !managed_properties->ip_configs.has_value()) {
    AnalyzeResultsAndExecuteCallback();
    return;
  }
  for (const auto& ip_config : managed_properties->ip_configs.value()) {
    if (ip_config->name_servers.has_value() &&
        ip_config->name_servers->size() != 0) {
      name_servers_found_ = true;
      if (NameServersAreNonEmpty(ip_config->name_servers.value())) {
        non_empty_name_servers_ = true;
      }
      if (NameServersAreWellFormed(ip_config->name_servers.value())) {
        well_formed_name_servers_ = true;
        break;
      }
    }
  }
  AnalyzeResultsAndExecuteCallback();
}

// Process the network interface information.
void DnsResolverPresentRoutine::OnNetworkStateListReceived(
    std::vector<network_config::mojom::NetworkStatePropertiesPtr> networks) {
  std::string default_guid;
  for (const auto& network : networks) {
    if (network_config::StateIsConnected(network->connection_state)) {
      default_guid = network->guid;
      break;
    }
  }
  // Since we are not connected, proceed to analyzing the results and executing
  // the completion callback.
  if (default_guid.empty()) {
    AnalyzeResultsAndExecuteCallback();
  } else {
    FetchManagedProperties(default_guid);
  }
}

}  // namespace network_diagnostics
}  // namespace chromeos
