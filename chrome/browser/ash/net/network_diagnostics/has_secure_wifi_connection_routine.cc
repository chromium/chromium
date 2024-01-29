// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/network_diagnostics/has_secure_wifi_connection_routine.h"

#include <iterator>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
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
using chromeos::network_config::mojom::NetworkFilter;
using chromeos::network_config::mojom::NetworkStatePropertiesPtr;
using chromeos::network_config::mojom::NetworkType;
using chromeos::network_config::mojom::SecurityType;

void GetNetworkConfigService(
    mojo::PendingReceiver<CrosNetworkConfig> receiver) {
  network_config::BindToInProcessInstance(std::move(receiver));
}

constexpr SecurityType kSecureWiFiEncryptions[] = {SecurityType::kWpaEap,
                                                   SecurityType::kWpaPsk};
constexpr SecurityType kInsecureWiFiEncryptions[] = {
    SecurityType::kNone, SecurityType::kWep8021x, SecurityType::kWepPsk};

}  // namespace

HasSecureWiFiConnectionRoutine::HasSecureWiFiConnectionRoutine(
    mojom::RoutineCallSource source)
    : NetworkDiagnosticsRoutine(source) {
  set_verdict(mojom::RoutineVerdict::kNotRun);
  GetNetworkConfigService(
      remote_cros_network_config_.BindNewPipeAndPassReceiver());
}

HasSecureWiFiConnectionRoutine::~HasSecureWiFiConnectionRoutine() = default;

mojom::RoutineType HasSecureWiFiConnectionRoutine::Type() {
  return mojom::RoutineType::kHasSecureWiFiConnection;
}

bool HasSecureWiFiConnectionRoutine::CanRun() {
  DCHECK(remote_cros_network_config_);
  return true;
}

void HasSecureWiFiConnectionRoutine::Run() {
  FetchActiveWiFiNetworks();
}

void HasSecureWiFiConnectionRoutine::AnalyzeResultsAndExecuteCallback() {
  if (!wifi_connected_) {
    set_verdict(mojom::RoutineVerdict::kNotRun);
  } else if (base::Contains(kInsecureWiFiEncryptions, wifi_security_)) {
    set_verdict(mojom::RoutineVerdict::kProblem);
    switch (wifi_security_) {
      case SecurityType::kNone:
        problems_.emplace_back(
            mojom::HasSecureWiFiConnectionProblem::kSecurityTypeNone);
        break;
      case SecurityType::kWep8021x:
        problems_.emplace_back(
            mojom::HasSecureWiFiConnectionProblem::kSecurityTypeWep8021x);
        break;
      case SecurityType::kWepPsk:
        problems_.emplace_back(
            mojom::HasSecureWiFiConnectionProblem::kSecurityTypeWepPsk);
        break;
      case SecurityType::kWpaEap:
      case SecurityType::kWpaPsk:
        break;
    }
  } else if (base::Contains(kSecureWiFiEncryptions, wifi_security_)) {
    set_verdict(mojom::RoutineVerdict::kNoProblem);
  } else {
    set_verdict(mojom::RoutineVerdict::kProblem);
    problems_.emplace_back(
        mojom::HasSecureWiFiConnectionProblem::kUnknownSecurityType);
  }

  set_problems(
      mojom::RoutineProblems::NewHasSecureWifiConnectionProblems(problems_));
  ExecuteCallback();
}

void HasSecureWiFiConnectionRoutine::FetchActiveWiFiNetworks() {
  DCHECK(remote_cros_network_config_);
  remote_cros_network_config_->GetNetworkStateList(
      NetworkFilter::New(FilterType::kActive, NetworkType::kWiFi,
                         chromeos::network_config::mojom::kNoLimit),
      base::BindOnce(
          &HasSecureWiFiConnectionRoutine::OnNetworkStateListReceived,
          base::Unretained(this)));
}

// Process the network interface information.
void HasSecureWiFiConnectionRoutine::OnNetworkStateListReceived(
    std::vector<NetworkStatePropertiesPtr> networks) {
  for (const NetworkStatePropertiesPtr& network : networks) {
    if (chromeos::network_config::StateIsConnected(network->connection_state)) {
      wifi_connected_ = true;
      wifi_security_ = network->type_state->get_wifi()->security;
      break;
    }
  }
  AnalyzeResultsAndExecuteCallback();
}

}  // namespace network_diagnostics
}  // namespace ash
