// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/network_diagnostics/captive_portal_routine.h"

#include <utility>

#include "base/bind.h"
#include "chromeos/services/network_config/in_process_instance.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_util.h"

namespace chromeos {
namespace network_diagnostics {

CaptivePortalRoutine::CaptivePortalRoutine() {
  network_config::BindToInProcessInstance(
      remote_cros_network_config_.BindNewPipeAndPassReceiver());
}

CaptivePortalRoutine::~CaptivePortalRoutine() = default;

void CaptivePortalRoutine::RunRoutine(CaptivePortalRoutineCallback callback) {
  if (!CanRun()) {
    std::move(callback).Run(verdict(), std::move(problems_));
    return;
  }
  routine_completed_callback_ = std::move(callback);
  FetchActiveNetworks();
}

void CaptivePortalRoutine::AnalyzeResultsAndExecuteCallback() {
  if (no_active_networks_) {
    set_verdict(mojom::RoutineVerdict::kProblem);
    problems_.emplace_back(mojom::CaptivePortalProblem::kNoActiveNetworks);
  } else if (restricted_connectivity_) {
    set_verdict(mojom::RoutineVerdict::kProblem);
    problems_.emplace_back(
        mojom::CaptivePortalProblem::kRestrictedConnectivity);
  } else if (state_is_captive_portal_) {
    set_verdict(mojom::RoutineVerdict::kProblem);
    problems_.emplace_back(mojom::CaptivePortalProblem::kCaptivePortalState);
  } else {
    set_verdict(mojom::RoutineVerdict::kNoProblem);
  }
  std::move(routine_completed_callback_).Run(verdict(), problems_);
}

void CaptivePortalRoutine::FetchActiveNetworks() {
  remote_cros_network_config_->GetNetworkStateList(
      network_config::mojom::NetworkFilter::New(
          network_config::mojom::FilterType::kActive,
          network_config::mojom::NetworkType::kAll,
          network_config::mojom::kNoLimit),
      base::BindOnce(&CaptivePortalRoutine::OnNetworkStateListReceived,
                     base::Unretained(this)));
}

void CaptivePortalRoutine::FetchManagedProperties(const std::string& guid) {
  remote_cros_network_config_->GetManagedProperties(
      guid, base::BindOnce(&CaptivePortalRoutine::OnManagedPropertiesReceived,
                           base::Unretained(this)));
}

// Process the network interface information.
void CaptivePortalRoutine::OnNetworkStateListReceived(
    std::vector<network_config::mojom::NetworkStatePropertiesPtr> networks) {
  if (networks.size() == 0) {
    no_active_networks_ = true;
    AnalyzeResultsAndExecuteCallback();
  } else {
    state_is_captive_portal_ =
        networks[0]->connection_state ==
        network_config::mojom::ConnectionStateType::kPortal;
    FetchManagedProperties(networks[0]->guid);
  }
}

void CaptivePortalRoutine::OnManagedPropertiesReceived(
    network_config::mojom::ManagedPropertiesPtr managed_properties) {
  if (!managed_properties) {
    AnalyzeResultsAndExecuteCallback();
    return;
  }
  switch (managed_properties->portal_state) {
    case network_config::mojom::PortalState::kUnknown:
    case network_config::mojom::PortalState::kOnline:
      break;
    case network_config::mojom::PortalState::kPortalSuspected:
    case network_config::mojom::PortalState::kPortal:
    case network_config::mojom::PortalState::kProxyAuthRequired:
    case network_config::mojom::PortalState::kNoInternet:
      restricted_connectivity_ = true;
      break;
  }
  AnalyzeResultsAndExecuteCallback();
}

}  // namespace network_diagnostics
}  // namespace chromeos
