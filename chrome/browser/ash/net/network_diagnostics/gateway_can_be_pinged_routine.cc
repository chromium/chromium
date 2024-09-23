// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/network_diagnostics/gateway_can_be_pinged_routine.h"

#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/ash/services/network_config/in_process_instance.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_util.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"

namespace ash {
namespace network_diagnostics {

namespace {

namespace mojom = ::chromeos::network_diagnostics::mojom;
using chromeos::network_config::mojom::CrosNetworkConfig;
using chromeos::network_config::mojom::FilterType;
using chromeos::network_config::mojom::ManagedPropertiesPtr;
using chromeos::network_config::mojom::NetworkFilter;
using chromeos::network_config::mojom::NetworkStatePropertiesPtr;
using chromeos::network_config::mojom::NetworkType;

void GetNetworkConfigService(
    mojo::PendingReceiver<CrosNetworkConfig> receiver) {
  network_config::BindToInProcessInstance(std::move(receiver));
}

// The maximum latency threshold (in milliseconds) for pinging the gateway.
constexpr base::TimeDelta kMaxAllowedLatencyMs = base::Milliseconds(1500);

}  // namespace

GatewayCanBePingedRoutine::GatewayCanBePingedRoutine(
    chromeos::network_diagnostics::mojom::RoutineCallSource source,
    DebugDaemonClient* debug_daemon_client)
    : NetworkDiagnosticsRoutine(source),
      debug_daemon_client_(debug_daemon_client) {
  set_verdict(mojom::RoutineVerdict::kNotRun);
  GetNetworkConfigService(
      remote_cros_network_config_.BindNewPipeAndPassReceiver());
}

GatewayCanBePingedRoutine::~GatewayCanBePingedRoutine() = default;

bool GatewayCanBePingedRoutine::CanRun() {
  DCHECK(remote_cros_network_config_);
  return true;
}

mojom::RoutineType GatewayCanBePingedRoutine::Type() {
  return mojom::RoutineType::kGatewayCanBePinged;
}

void GatewayCanBePingedRoutine::Run() {
  FetchActiveNetworks();
}

void GatewayCanBePingedRoutine::AnalyzeResultsAndExecuteCallback() {
  if (unreachable_gateways_) {
    set_verdict(mojom::RoutineVerdict::kProblem);
    problems_.emplace_back(
        mojom::GatewayCanBePingedProblem::kUnreachableGateway);
  } else if (!pingable_default_network_) {
    set_verdict(mojom::RoutineVerdict::kProblem);
    problems_.emplace_back(
        mojom::GatewayCanBePingedProblem::kFailedToPingDefaultNetwork);
  } else if (default_network_latency_ > kMaxAllowedLatencyMs) {
    set_verdict(mojom::RoutineVerdict::kProblem);
    problems_.emplace_back(
        mojom::GatewayCanBePingedProblem::kDefaultNetworkAboveLatencyThreshold);
  } else if (non_default_network_unsuccessful_ping_count_ > 0) {
    set_verdict(mojom::RoutineVerdict::kProblem);
    problems_.emplace_back(
        mojom::GatewayCanBePingedProblem::kUnsuccessfulNonDefaultNetworksPings);
  } else if (!BelowLatencyThreshold()) {
    set_verdict(mojom::RoutineVerdict::kProblem);
    problems_.emplace_back(mojom::GatewayCanBePingedProblem::
                               kNonDefaultNetworksAboveLatencyThreshold);
  } else {
    set_verdict(mojom::RoutineVerdict::kNoProblem);
  }

  set_problems(
      mojom::RoutineProblems::NewGatewayCanBePingedProblems(problems_));
  ExecuteCallback();
}

bool GatewayCanBePingedRoutine::BelowLatencyThreshold() {
  for (base::TimeDelta latency : non_default_network_latencies_) {
    if (latency > kMaxAllowedLatencyMs) {
      return false;
    }
  }
  return true;
}

void GatewayCanBePingedRoutine::FetchActiveNetworks() {
  DCHECK(remote_cros_network_config_);
  remote_cros_network_config_->GetNetworkStateList(
      NetworkFilter::New(FilterType::kActive, NetworkType::kAll,
                         chromeos::network_config::mojom::kNoLimit),
      base::BindOnce(&GatewayCanBePingedRoutine::OnNetworkStateListReceived,
                     base::Unretained(this)));
}

void GatewayCanBePingedRoutine::FetchManagedProperties(
    const std::vector<std::string>& guids) {
  DCHECK(remote_cros_network_config_);
  guids_remaining_ = guids.size();
  for (const std::string& guid : guids) {
    remote_cros_network_config_->GetManagedProperties(
        guid,
        base::BindOnce(&GatewayCanBePingedRoutine::OnManagedPropertiesReceived,
                       base::Unretained(this)));
  }
}

void GatewayCanBePingedRoutine::PingGateways() {
  for (const std::string& gateway : gateways_) {
    debug_daemon_client()->TestICMP(
        gateway, base::BindOnce(&GatewayCanBePingedRoutine::OnTestICMPCompleted,
                                base::Unretained(this),
                                gateway == default_network_gateway_));
  }
}

// Parses |status| and returns the IP and latency. For details about |status|,
// please refer to:
// https://gerrit.chromium.org/gerrit/#/c/30310/2/src/helpers/icmp.cc.
bool GatewayCanBePingedRoutine::ParseICMPResult(const std::string& status,
                                                std::string* ip,
                                                base::TimeDelta* latency) {
  std::optional<base::Value> parsed_value(base::JSONReader::Read(status));
  if (!parsed_value.has_value()) {
    return false;
  }
  const base::Value::Dict* parsed_value_dict = parsed_value->GetIfDict();
  if (!parsed_value_dict || parsed_value_dict->size() != 1) {
    return false;
  }
  auto iter = parsed_value_dict->begin();
  const std::string& ip_addr = iter->first;
  const base::Value::Dict* info = iter->second.GetIfDict();
  if (!info) {
    return false;
  }
  const std::optional<int> recvd_value = info->FindInt("recvd");
  if (!recvd_value || recvd_value.value() < 1) {
    return false;
  }

  const std::optional<double> avg_value = info->FindDouble("avg");
  if (!avg_value) {
    return false;
  }
  *latency = base::Milliseconds(avg_value.value());
  *ip = ip_addr;

  return true;
}

// Process the network interface information.
void GatewayCanBePingedRoutine::OnNetworkStateListReceived(
    std::vector<NetworkStatePropertiesPtr> networks) {
  bool connected = false;
  std::vector<std::string> guids;
  for (const auto& network : networks) {
    if (!chromeos::network_config::StateIsConnected(
            network->connection_state)) {
      continue;
    }
    connected = true;
    const std::string& guid = network->guid;
    if (default_network_guid_.empty()) {
      default_network_guid_ = guid;
    }
    guids.emplace_back(guid);
  }
  if (!connected || guids.empty()) {
    // Since we are not connected at all, directly analyze the results.
    AnalyzeResultsAndExecuteCallback();
  } else {
    FetchManagedProperties(guids);
  }
}

void GatewayCanBePingedRoutine::OnManagedPropertiesReceived(
    ManagedPropertiesPtr managed_properties) {
  DCHECK(guids_remaining_ > 0);
  if (managed_properties) {
    if (managed_properties->ip_configs.has_value() &&
        managed_properties->ip_configs->size() != 0) {
      for (const auto& ip_config : managed_properties->ip_configs.value()) {
        // TODO(b/277696397): Reaching a link-local address needs to specify the
        // interface. Currently we don't have a good way to get the interface
        // here, so skip link-local addresses instead of always reporting a
        // failure here. Revisit this part when we can get the interface name,
        // or ideally we should rely on the layer 2 link monitor signal for the
        // diagnostic.
        if (ip_config->gateway.has_value() &&
            !ip_config->gateway->starts_with("fe80::")) {
          const std::string& gateway = ip_config->gateway.value();
          if (managed_properties->guid == default_network_guid_) {
            default_network_gateway_ = gateway;
          }
          gateways_.emplace_back(gateway);
        }
      }
    }
  }
  guids_remaining_--;
  if (guids_remaining_ == 0) {
    if (gateways_.size() == 0) {
      // Since we cannot ping the gateway, directly analyze the results.
      AnalyzeResultsAndExecuteCallback();
    } else {
      unreachable_gateways_ = false;
      gateways_remaining_ = gateways_.size();
      PingGateways();
    }
  }
}

void GatewayCanBePingedRoutine::OnTestICMPCompleted(
    bool is_default_network_ping_result,
    const std::optional<std::string> status) {
  DCHECK(gateways_remaining_ > 0);
  std::string result_ip;
  base::TimeDelta result_latency;
  bool failed_ping =
      !status.has_value() ||
      !ParseICMPResult(status.value(), &result_ip, &result_latency);
  if (failed_ping) {
    if (!is_default_network_ping_result) {
      non_default_network_unsuccessful_ping_count_++;
    }
  } else {
    if (is_default_network_ping_result) {
      pingable_default_network_ = true;
      default_network_latency_ = result_latency;
    } else {
      non_default_network_latencies_.emplace_back(result_latency);
    }
  }
  gateways_remaining_--;
  if (gateways_remaining_ == 0) {
    AnalyzeResultsAndExecuteCallback();
  }
}

}  // namespace network_diagnostics
}  // namespace ash
