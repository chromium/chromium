// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>
#include <utility>

#include "ash/components/arc/session/arc_service_manager.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ash/net/network_diagnostics/arc_ping_routine.h"
#include "chrome/browser/ash/net/network_diagnostics/network_diagnostics_util.h"
#include "chromeos/ash/services/network_config/in_process_instance.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_util.h"

namespace ash {
namespace network_diagnostics {

namespace {

namespace mojom = ::chromeos::network_diagnostics::mojom;
using ::chromeos::network_config::mojom::CrosNetworkConfig;
using ::chromeos::network_config::mojom::FilterType;
using ::chromeos::network_config::mojom::ManagedPropertiesPtr;
using ::chromeos::network_config::mojom::NetworkFilter;
using ::chromeos::network_config::mojom::NetworkStatePropertiesPtr;
using ::chromeos::network_config::mojom::NetworkType;

void GetNetworkConfigService(
    mojo::PendingReceiver<CrosNetworkConfig> receiver) {
  network_config::BindToInProcessInstance(std::move(receiver));
}

// Requests taking longer than 1500 ms are problematic.
constexpr int kProblemLatencyMs = 1500;

// Timeout for GetManagedProperties.
constexpr int kTimeoutGetManagedPropertiesSeconds = 180;

}  // namespace

ArcPingRoutine::ArcPingRoutine(mojom::RoutineCallSource source)
    : NetworkDiagnosticsRoutine(source) {
  set_verdict(mojom::RoutineVerdict::kNotRun);
  GetNetworkConfigService(
      remote_cros_network_config_.BindNewPipeAndPassReceiver());
}

ArcPingRoutine::~ArcPingRoutine() = default;

mojom::RoutineType ArcPingRoutine::Type() {
  return mojom::RoutineType::kArcPing;
}

void ArcPingRoutine::Run() {
  FetchActiveNetworks();
}

void ArcPingRoutine::AnalyzeResultsAndExecuteCallback() {
  if (unreachable_gateways_) {
    set_verdict(mojom::RoutineVerdict::kProblem);
    problems_.push_back(mojom::ArcPingProblem::kUnreachableGateway);
  } else if (failed_to_get_arc_service_manager_) {
    set_verdict(mojom::RoutineVerdict::kNotRun);
    problems_.push_back(mojom::ArcPingProblem::kFailedToGetArcServiceManager);
  } else if (failed_to_get_net_instance_service_for_ping_test_) {
    set_verdict(mojom::RoutineVerdict::kNotRun);
    problems_.push_back(
        mojom::ArcPingProblem::kFailedToGetNetInstanceForPingTest);
  } else if (get_managed_properties_timeout_failure_) {
    set_verdict(mojom::RoutineVerdict::kNotRun);
    problems_.push_back(
        mojom::ArcPingProblem::kGetManagedPropertiesTimeoutFailure);
  } else if (!pingable_default_network_) {
    set_verdict(mojom::RoutineVerdict::kProblem);
    problems_.push_back(mojom::ArcPingProblem::kFailedToPingDefaultNetwork);
  } else if (default_network_latency_ > kProblemLatencyMs) {
    set_verdict(mojom::RoutineVerdict::kProblem);
    problems_.push_back(
        mojom::ArcPingProblem::kDefaultNetworkAboveLatencyThreshold);
  } else if (non_default_network_unsuccessful_) {
    set_verdict(mojom::RoutineVerdict::kProblem);
    problems_.push_back(
        mojom::ArcPingProblem::kUnsuccessfulNonDefaultNetworksPings);
  } else if (non_default_max_latency_ > kProblemLatencyMs) {
    set_verdict(mojom::RoutineVerdict::kProblem);
    problems_.push_back(
        mojom::ArcPingProblem::kNonDefaultNetworksAboveLatencyThreshold);
  } else {
    set_verdict(mojom::RoutineVerdict::kNoProblem);
  }
  set_problems(mojom::RoutineProblems::NewArcPingProblems(problems_));
  ExecuteCallback();
}

void ArcPingRoutine::FetchActiveNetworks() {
  DCHECK(remote_cros_network_config_);
  remote_cros_network_config_->GetNetworkStateList(
      NetworkFilter::New(FilterType::kActive, NetworkType::kAll,
                         chromeos::network_config::mojom::kNoLimit),
      base::BindOnce(&ArcPingRoutine::OnNetworkStateListReceived,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcPingRoutine::FetchManagedProperties(
    const std::vector<std::string>& guids) {
  DCHECK(remote_cros_network_config_);
  guids_remaining_ = guids.size();

  // Post delayed task to handle timeout error on GetManagedProperties.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ArcPingRoutine::HandleTimeout,
                     weak_ptr_factory_.GetWeakPtr()),
      base::Seconds(kTimeoutGetManagedPropertiesSeconds));

  for (const std::string& guid : guids) {
    remote_cros_network_config_->GetManagedProperties(
        guid, base::BindOnce(&ArcPingRoutine::OnManagedPropertiesReceived,
                             weak_ptr_factory_.GetWeakPtr()));
  }
}

void ArcPingRoutine::PingGateways() {
  arc::mojom::NetInstance* net_instance = GetNetInstance();
  if (net_instance) {
    for (size_t i = 0; i < gateways_.size(); i++) {
      net_instance->PingTest(
          gateways_transport_names_[i], gateways_[i],
          base::BindOnce(&ArcPingRoutine::OnRequestComplete,
                         weak_ptr_factory_.GetWeakPtr(),
                         gateways_[i] == default_network_gateway_));
    }
  }
}

void ArcPingRoutine::OnNetworkStateListReceived(
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
    guids.push_back(guid);
  }
  if (!connected || guids.empty()) {
    // Since we are not connected at all, directly analyze the results.
    AnalyzeResultsAndExecuteCallback();
  } else {
    FetchManagedProperties(guids);
  }
}

void ArcPingRoutine::OnManagedPropertiesReceived(
    ManagedPropertiesPtr managed_properties) {
  DCHECK(guids_remaining_ > 0);
  if (managed_properties && managed_properties->ip_configs.has_value() &&
      managed_properties->ip_configs->size() != 0) {
    for (const auto& ip_config : managed_properties->ip_configs.value()) {
      // Link-local addresses are not reachable from ARC, so skip them here.
      // TODO(b/277696397): Find a better signal.
      if (ip_config->gateway.has_value() &&
          !ip_config->gateway->starts_with("fe80::")) {
        const std::string& gateway = ip_config->gateway.value();
        if (managed_properties->guid == default_network_guid_) {
          default_network_gateway_ = gateway;
        }
        gateways_.push_back(gateway);
        gateways_transport_names_.push_back(
            GetTransportName(managed_properties->name->active_value));
      }
    }
  }
  guids_remaining_--;
  if (guids_remaining_ == 0) {
    weak_ptr_factory_.InvalidateWeakPtrs();
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

void ArcPingRoutine::OnRequestComplete(
    bool is_default_network_ping_result,
    arc::mojom::ArcPingTestResultPtr result) {
  DCHECK(gateways_remaining_ > 0);
  if (!result->is_successful) {
    if (!is_default_network_ping_result) {
      non_default_network_unsuccessful_ = true;
    }
  } else {
    if (is_default_network_ping_result) {
      pingable_default_network_ = true;
      default_network_latency_ = result->duration_ms;
    } else {
      non_default_max_latency_ =
          std::max(non_default_max_latency_, result->duration_ms);
    }
  }

  gateways_remaining_--;
  if (gateways_remaining_ == 0) {
    AnalyzeResultsAndExecuteCallback();
  }
}

void ArcPingRoutine::HandleTimeout() {
  // Destroy pending callbacks in case they end up being unexpectedly invoked
  // after the timer expires.
  weak_ptr_factory_.InvalidateWeakPtrs();
  get_managed_properties_timeout_failure_ = true;
  AnalyzeResultsAndExecuteCallback();
}

arc::mojom::NetInstance* ArcPingRoutine::GetNetInstance() {
  // If |net_instance_| is not already set for testing purposes, get instance
  // of NetInstance service.
  if (net_instance_) {
    return net_instance_;
  }

  // Call the singleton for ArcServiceManager and check if it is null.
  auto* arc_service_manager = arc::ArcServiceManager::Get();
  if (!arc_service_manager) {
    failed_to_get_arc_service_manager_ = true;
    AnalyzeResultsAndExecuteCallback();
    return nullptr;
  }

  // Get an instance of the NetInstance service and check if it is null.
  auto* arc_bridge_service = arc_service_manager->arc_bridge_service();
  arc::mojom::NetInstance* net_instance =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service->net(), PingTest);
  if (!net_instance) {
    failed_to_get_net_instance_service_for_ping_test_ = true;
    AnalyzeResultsAndExecuteCallback();
    return nullptr;
  }
  return net_instance;
}

std::string ArcPingRoutine::GetTransportName(
    const std::string& managed_properties_name) {
  if (managed_properties_name == "wifi_guid") {
    return "wifi";
  } else if (managed_properties_name == "eth_guid") {
    return "eth";
  } else if (managed_properties_name == "cell_guid") {
    return "cell";
  } else if (managed_properties_name == "vpn_guid") {
    return "vpn";
  } else {
    return "";
  }
}

}  // namespace network_diagnostics
}  // namespace ash
