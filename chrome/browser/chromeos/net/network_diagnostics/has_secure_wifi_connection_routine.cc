// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/network_diagnostics/has_secure_wifi_connection_routine.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "base/bind.h"
#include "chromeos/services/network_config/in_process_instance.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_util.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"

namespace chromeos {
namespace network_diagnostics {
namespace {

using chromeos::network_config::mojom::CrosNetworkConfig;
using chromeos::network_config::mojom::FilterType;
using chromeos::network_config::mojom::NetworkFilter;
using chromeos::network_config::mojom::NetworkStatePropertiesPtr;
using chromeos::network_config::mojom::NetworkType;
using chromeos::network_config::mojom::SecurityType;

void GetNetworkConfigService(
    mojo::PendingReceiver<CrosNetworkConfig> receiver) {
  chromeos::network_config::BindToInProcessInstance(std::move(receiver));
}

constexpr SecurityType kSecureWiFiEncryptions[] = {SecurityType::kWpaEap,
                                                   SecurityType::kWpaPsk};
constexpr SecurityType kInsecureWiFiEncryptions[] = {
    SecurityType::kNone, SecurityType::kWep8021x, SecurityType::kWepPsk};

bool IsSecureWiFiSecurityType(SecurityType security_type) {
  return std::find(std::begin(kSecureWiFiEncryptions),
                   std::end(kSecureWiFiEncryptions),
                   security_type) != std::end(kSecureWiFiEncryptions);
}

bool IsInsecureSecurityType(SecurityType security_type) {
  return std::find(std::begin(kInsecureWiFiEncryptions),
                   std::end(kInsecureWiFiEncryptions),
                   security_type) != std::end(kInsecureWiFiEncryptions);
}

}  // namespace

HasSecureWiFiConnectionRoutine::HasSecureWiFiConnectionRoutine() {
  set_verdict(mojom::RoutineVerdict::kNotRun);
  GetNetworkConfigService(
      remote_cros_network_config_.BindNewPipeAndPassReceiver());
}

HasSecureWiFiConnectionRoutine::~HasSecureWiFiConnectionRoutine() = default;

bool HasSecureWiFiConnectionRoutine::CanRun() {
  DCHECK(remote_cros_network_config_);
  return true;
}

void HasSecureWiFiConnectionRoutine::RunRoutine(
    HasSecureWiFiConnectionRoutineCallback callback) {
  if (!CanRun()) {
    std::move(callback).Run(verdict(), std::move(problems_));
    return;
  }
  routine_completed_callback_ = std::move(callback);
  FetchActiveWiFiNetworks();
}

void HasSecureWiFiConnectionRoutine::AnalyzeResultsAndExecuteCallback() {
  if (!wifi_connected_) {
    set_verdict(mojom::RoutineVerdict::kNotRun);
  } else if (IsInsecureSecurityType(wifi_security_)) {
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
  } else if (IsSecureWiFiSecurityType(wifi_security_)) {
    set_verdict(mojom::RoutineVerdict::kNoProblem);
  } else {
    set_verdict(mojom::RoutineVerdict::kProblem);
    problems_.emplace_back(
        mojom::HasSecureWiFiConnectionProblem::kUnknownSecurityType);
  }
  std::move(routine_completed_callback_).Run(verdict(), std::move(problems_));
}

void HasSecureWiFiConnectionRoutine::FetchActiveWiFiNetworks() {
  DCHECK(remote_cros_network_config_);
  remote_cros_network_config_->GetNetworkStateList(
      NetworkFilter::New(FilterType::kActive, NetworkType::kWiFi,
                         network_config::mojom::kNoLimit),
      base::BindOnce(
          &HasSecureWiFiConnectionRoutine::OnNetworkStateListReceived,
          base::Unretained(this)));
}

// Process the network interface information.
void HasSecureWiFiConnectionRoutine::OnNetworkStateListReceived(
    std::vector<NetworkStatePropertiesPtr> networks) {
  for (const NetworkStatePropertiesPtr& network : networks) {
    if (network_config::StateIsConnected(network->connection_state)) {
      wifi_connected_ = true;
      wifi_security_ = network->type_state->get_wifi()->security;
      break;
    }
  }
  AnalyzeResultsAndExecuteCallback();
}

}  // namespace network_diagnostics
}  // namespace chromeos
