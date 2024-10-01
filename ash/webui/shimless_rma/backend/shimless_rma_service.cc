// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/shimless_rma/backend/shimless_rma_service.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/network_config_service.h"
#include "ash/system/diagnostics/diagnostics_log_controller.h"
#include "ash/webui/shimless_rma/backend/external_app_dialog.h"
#include "ash/webui/shimless_rma/backend/shimless_rma_delegate.h"
#include "ash/webui/shimless_rma/backend/version_updater.h"
#include "ash/webui/shimless_rma/mojom/shimless_rma.mojom.h"
#include "ash/webui/shimless_rma/mojom/shimless_rma_mojom_traits.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chromeos/ash/components/dbus/rmad/rmad.pb.h"
#include "chromeos/ash/components/dbus/rmad/rmad_client.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_type_pattern.h"
#include "chromeos/ash/components/network/technology_state_controller.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/service_connection.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "chromeos/ash/services/network_config/in_process_instance.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "chromeos/version/version_loader.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace ash {
namespace shimless_rma {

namespace {

namespace network_mojom = ::chromeos::network_config::mojom;

mojom::State RmadStateToMojo(rmad::RmadState::StateCase rmadState) {
  return mojo::EnumTraits<ash::shimless_rma::mojom::State,
                          rmad::RmadState::StateCase>::ToMojom(rmadState);
}

// Returns whether the device is connected to an unmetered network.
// Metered networks are excluded for RMA to avoid any cost to the owner who
// does not have control of the device during RMA.
bool HaveAllowedNetworkConnection() {
  NetworkStateHandler* network_state_handler =
      NetworkHandler::Get()->network_state_handler();
  const NetworkState* network = network_state_handler->DefaultNetwork();
  // TODO(gavindodd): Confirm that metered networks should be excluded. This
  // should only be true for cellular networks which are already blocked.
  const bool metered = network_state_handler->default_network_is_metered();
  // Return true if connected to an unmetered network.
  return network && network->IsConnectedState() && !metered;
}

network_mojom::NetworkFilterPtr GetConfiguredWiFiFilter() {
  return network_mojom::NetworkFilter::New(
      network_mojom::FilterType::kConfigured, network_mojom::NetworkType::kWiFi,
      network_mojom::kNoLimit);
}

}  // namespace

ShimlessRmaService::ShimlessRmaService(
    std::unique_ptr<ShimlessRmaDelegate> shimless_rma_delegate)
    : shimless_rma_delegate_(std::move(shimless_rma_delegate)),
      task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {
  RmadClient::Get()->AddObserver(this);

  // Enable accessibility features.
  shimless_rma_delegate_->RefreshAccessibilityManagerProfile();

  network_config::BindToInProcessInstance(
      remote_cros_network_config_.BindNewPipeAndPassReceiver());

  if (features::IsShimlessRMAOsUpdateEnabled()) {
    version_updater_.SetOsUpdateStatusCallback(
        base::BindRepeating(&ShimlessRmaService::OnOsUpdateStatusCallback,
                            weak_ptr_factory_.GetWeakPtr()));
    // Check if an OS update is available to minimize delays if needed later.
    if (HaveAllowedNetworkConnection()) {
      version_updater_.CheckOsUpdateAvailable();
    }
  }
}

ShimlessRmaService::~ShimlessRmaService() {
  RmadClient::Get()->RemoveObserver(this);
}

void ShimlessRmaService::GetCurrentState(GetCurrentStateCallback callback) {
  RmadClient::Get()->GetCurrentState(base::BindOnce(
      &ShimlessRmaService::OnGetStateResponse<GetCurrentStateCallback>,
      weak_ptr_factory_.GetWeakPtr(), std::move(callback), kGetCurrentState));
}

mojom::StateResultPtr ShimlessRmaService::CreateStateResult(
    mojom::State state,
    bool can_exit,
    bool can_go_back,
    rmad::RmadErrorCode error) {
  return mojom::StateResult::New(state, can_exit, can_go_back, error);
}

mojom::StateResultPtr ShimlessRmaService::CreateStateResultForInvalidRequest() {
  return CreateStateResult(RmadStateToMojo(state_proto_.state_case()),
                           can_abort_, can_go_back_,
                           rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
}

void ShimlessRmaService::TransitionPreviousState(
    TransitionPreviousStateCallback callback) {
  // If current rmad state is rmad::RmadState::kWelcome and the mojom state
  // is kConfigureNetwork or kUpdateOs, we don't call rmad service. Otherwise,
  // it will respond with error.
  if (state_proto_.state_case() == rmad::RmadState::kWelcome &&
      (mojo_state_ == mojom::State::kConfigureNetwork ||
       mojo_state_ == mojom::State::kUpdateOs)) {
    mojo_state_ = mojom::State::kWelcomeScreen;
    std::move(callback).Run(
        CreateStateResult(mojom::State::kWelcomeScreen,
                          /*can_exit=*/true, /*can_go_back=*/false,
                          rmad::RmadErrorCode::RMAD_ERROR_OK));
    return;
  }

  RmadClient::Get()->TransitionPreviousState(base::BindOnce(
      &ShimlessRmaService::OnGetStateResponse<TransitionPreviousStateCallback>,
      weak_ptr_factory_.GetWeakPtr(), std::move(callback),
      kTransitPreviousState));
}

void ShimlessRmaService::AbortRma(AbortRmaCallback callback) {
  RmadClient::Get()->AbortRma(base::BindOnce(
      &ShimlessRmaService::OnAbortRmaResponse, weak_ptr_factory_.GetWeakPtr(),
      std::move(callback), /*reboot=*/true));
}

void ShimlessRmaService::CriticalErrorExitToLogin(
    CriticalErrorExitToLoginCallback callback) {
  if (!critical_error_occurred_) {
    std::move(callback).Run(rmad::RMAD_ERROR_REQUEST_INVALID);
    return;
  }
  RmadClient::Get()->AbortRma(base::BindOnce(
      &ShimlessRmaService::OnAbortRmaResponse, weak_ptr_factory_.GetWeakPtr(),
      std::move(callback), /*reboot=*/false));
}

void ShimlessRmaService::CriticalErrorReboot(
    CriticalErrorRebootCallback callback) {
  if (!critical_error_occurred_) {
    std::move(callback).Run(rmad::RMAD_ERROR_REQUEST_INVALID);
    return;
  }
  RmadClient::Get()->AbortRma(base::BindOnce(
      &ShimlessRmaService::OnAbortRmaResponse, weak_ptr_factory_.GetWeakPtr(),
      std::move(callback), /*reboot=*/true));
}

void ShimlessRmaService::ShutDownAfterHardwareError() {
  if (state_proto_.state_case() != rmad::RmadState::kProvisionDevice &&
      state_proto_.state_case() != rmad::RmadState::kFinalize) {
    LOG(ERROR) << "ShutDownAfterHardwareError called from incorrect state "
               << state_proto_.state_case() << " / " << mojo_state_;
    return;
  }

  chromeos::PowerManagerClient::Get()->RequestShutdown(
      power_manager::REQUEST_SHUTDOWN_FOR_USER,
      "Shutting down after encountering a hardware error.");
}

void ShimlessRmaService::BeginFinalization(BeginFinalizationCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kWelcome ||
      mojo_state_ != mojom::State::kWelcomeScreen) {
    LOG(ERROR) << "FinalizeRepair called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(CreateStateResultForInvalidRequest());
    return;
  }
  state_proto_.mutable_welcome()->set_choice(
      rmad::WelcomeState::RMAD_CHOICE_FINALIZE_REPAIR);

  // Only when the `ShimlessRMAOsUpdate` flag is enabled should the network
  // connection and OS update status be checked.
  if (features::IsShimlessRMAOsUpdateEnabled()) {
    if (!HaveAllowedNetworkConnection()) {
      // Enable WiFi on the device.
      NetworkStateHandler* network_state_handler =
          NetworkHandler::Get()->network_state_handler();
      TechnologyStateController* technology_state_controller =
          NetworkHandler::Get()->technology_state_controller();
      if (!network_state_handler->IsTechnologyEnabled(
              NetworkTypePattern::WiFi())) {
        technology_state_controller->SetTechnologiesEnabled(
            NetworkTypePattern::WiFi(), /*enabled=*/true,
            network_handler::ErrorCallback());
      }

      user_has_seen_network_page_ = true;
      mojo_state_ = mojom::State::kConfigureNetwork;
      std::move(callback).Run(
          CreateStateResult(mojom::State::kConfigureNetwork,
                            /*can_exit=*/true, /*can_go_back=*/true,
                            rmad::RmadErrorCode::RMAD_ERROR_OK));
    } else {
      // This callback is invoked once VersionUpdated determines if an OS Update
      // is available.
      check_os_callback_ =
          base::BindOnce(&ShimlessRmaService::OsUpdateOrNextRmadStateCallback,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback));
      version_updater_.CheckOsUpdateAvailable();
    }
  } else {
    TransitionNextStateGeneric(std::move(callback));
  }
}

void ShimlessRmaService::TrackConfiguredNetworks() {
  if (mojo_state_ != mojom::State::kConfigureNetwork) {
    LOG(ERROR) << "TrackConfiguredNetworks called from incorrect state "
               << state_proto_.state_case() << " / " << mojo_state_;
    return;
  }

  // Only populate `existing_saved_network_guids_` once to avoid treating a new
  // network like an existing network. TrackConfiguredNetworks() can potentially
  // be called twice if the user navigates back to the networking page.
  if (existing_saved_network_guids_.has_value()) {
    LOG(WARNING) << "Already captured configured networks.";
    return;
  }

  remote_cros_network_config_->GetNetworkStateList(
      GetConfiguredWiFiFilter(),
      base::BindOnce(&ShimlessRmaService::OnTrackConfiguredNetworks,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ShimlessRmaService::OnTrackConfiguredNetworks(
    std::vector<network_mojom::NetworkStatePropertiesPtr> networks) {
  DCHECK(!existing_saved_network_guids_.has_value());

  existing_saved_network_guids_ = base::flat_set<std::string>();
  for (auto& network : networks) {
    existing_saved_network_guids_->insert(std::move(network->guid));
  }
}

void ShimlessRmaService::ForgetNewNetworkConnections(
    base::OnceClosure end_rma_callback) {
  // Skip forgetting networks if a saved list of network guids was never
  // created.
  if (!existing_saved_network_guids_.has_value()) {
    std::move(end_rma_callback).Run();
    return;
  }

  end_rma_callback_ = std::move(end_rma_callback);
  remote_cros_network_config_->GetNetworkStateList(
      GetConfiguredWiFiFilter(),
      base::BindOnce(&ShimlessRmaService::OnForgetNewNetworkConnections,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ShimlessRmaService::OnForgetNewNetworkConnections(
    std::vector<network_mojom::NetworkStatePropertiesPtr> networks) {
  DCHECK(existing_saved_network_guids_.has_value());
  DCHECK(pending_network_guids_to_forget_.empty());

  for (auto& network : networks) {
    const std::string& guid = network->guid;
    const bool found_network_guid =
        base::Contains(existing_saved_network_guids_.value(), guid);

    if (!found_network_guid) {
      pending_network_guids_to_forget_.insert(guid);
    }
  }

  // No networks to forget, invoke end RMA callback.
  if (pending_network_guids_to_forget_.empty()) {
    DCHECK(!end_rma_callback_.is_null());

    std::move(end_rma_callback_).Run();
    return;
  }

  for (const auto& guid : pending_network_guids_to_forget_) {
    remote_cros_network_config_->ForgetNetwork(
        guid, base::BindOnce(&ShimlessRmaService::OnForgetNetwork,
                             weak_ptr_factory_.GetWeakPtr(), guid));
  }
}

void ShimlessRmaService::OnForgetNetwork(const std::string& guid,
                                         bool success) {
  pending_network_guids_to_forget_.erase(guid);
  if (!success) {
    LOG(ERROR) << "Failed to forget saved network configuration GUID: " << guid;
  }

  // End RMA once each network has been forgotten.
  if (pending_network_guids_to_forget_.empty()) {
    std::move(end_rma_callback_).Run();
  }
}

void ShimlessRmaService::NetworkSelectionComplete(
    NetworkSelectionCompleteCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kWelcome ||
      mojo_state_ != mojom::State::kConfigureNetwork) {
    LOG(ERROR) << "NetworkSelectionComplete called from incorrect state "
               << state_proto_.state_case() << " / " << mojo_state_;
    std::move(callback).Run(CreateStateResultForInvalidRequest());
    return;
  }
  if (HaveAllowedNetworkConnection() &&
      features::IsShimlessRMAOsUpdateEnabled()) {
    check_os_callback_ =
        base::BindOnce(&ShimlessRmaService::OsUpdateOrNextRmadStateCallback,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback));
    version_updater_.CheckOsUpdateAvailable();
  } else {
    TransitionNextStateGeneric(std::move(callback));
  }
}

void ShimlessRmaService::GetCurrentOsVersion(
    GetCurrentOsVersionCallback callback) {
  DCHECK(features::IsShimlessRMAOsUpdateEnabled());
  // TODO(gavindodd): Decide whether to use full or short Chrome version.
  std::optional<std::string> version = chromeos::version_loader::GetVersion(
      chromeos::version_loader::VERSION_FULL);
  std::move(callback).Run(version);
}

void ShimlessRmaService::CheckForOsUpdates(CheckForOsUpdatesCallback callback) {
  DCHECK(features::IsShimlessRMAOsUpdateEnabled());
  if (state_proto_.state_case() != rmad::RmadState::kWelcome ||
      mojo_state_ != mojom::State::kUpdateOs) {
    LOG(ERROR) << "CheckForOsUpdates called from incorrect state "
               << state_proto_.state_case() << " / " << mojo_state_;
    std::move(callback).Run(false, "");
    return;
  }
  // This should never be called if a check is pending.
  DCHECK(!check_os_callback_);
  check_os_callback_ = base::BindOnce(
      [](CheckForOsUpdatesCallback callback, const std::string& version) {
        std::move(callback).Run(!version.empty(), version);
      },
      std::move(callback));
  version_updater_.CheckOsUpdateAvailable();
}

void ShimlessRmaService::UpdateOs(UpdateOsCallback callback) {
  DCHECK(features::IsShimlessRMAOsUpdateEnabled());
  if (state_proto_.state_case() != rmad::RmadState::kWelcome ||
      mojo_state_ != mojom::State::kUpdateOs) {
    LOG(ERROR) << "UpdateOs called from incorrect state "
               << state_proto_.state_case() << " / " << mojo_state_;
    std::move(callback).Run(false);
    return;
  }
  std::move(callback).Run(version_updater_.UpdateOs());

  SendMetricOnUpdateOs();
}

void ShimlessRmaService::UpdateOsSkipped(UpdateOsSkippedCallback callback) {
  DCHECK(features::IsShimlessRMAOsUpdateEnabled());
  if (state_proto_.state_case() != rmad::RmadState::kWelcome ||
      mojo_state_ != mojom::State::kUpdateOs) {
    LOG(ERROR) << "UpdateOsSkipped called from incorrect state "
               << state_proto_.state_case() << " / " << mojo_state_;
    std::move(callback).Run(CreateStateResultForInvalidRequest());
    return;
  }
  if (!version_updater_.IsUpdateEngineIdle()) {
    LOG(ERROR) << "UpdateOsSkipped called while UpdateEngine active";
    // Override the rmad state (kWelcome) with the mojo sub-state for OS
    // updates.
    std::move(callback).Run(
        CreateStateResult(mojom::State::kUpdateOs,
                          /*can_exit=*/true, /*can_go_back=*/true,
                          rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID));
    return;
  }
  TransitionNextStateGeneric(std::move(callback));
}

VersionUpdater* ShimlessRmaService::GetVersionUpdaterForTesting() {
  return &version_updater_;
}

void ShimlessRmaService::SetSameOwner(SetSameOwnerCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kDeviceDestination) {
    LOG(ERROR) << "SetSameOwner called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(CreateStateResultForInvalidRequest());
    return;
  }
  state_proto_.mutable_device_destination()->set_destination(
      rmad::DeviceDestinationState::RMAD_DESTINATION_SAME);
  TransitionNextStateGeneric(std::move(callback));
}

void ShimlessRmaService::SetDifferentOwner(SetDifferentOwnerCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kDeviceDestination) {
    LOG(ERROR) << "SetDifferentOwner called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(CreateStateResultForInvalidRequest());
    return;
  }
  state_proto_.mutable_device_destination()->set_destination(
      rmad::DeviceDestinationState::RMAD_DESTINATION_DIFFERENT);
  TransitionNextStateGeneric(std::move(callback));
}

void ShimlessRmaService::SetWipeDevice(bool should_wipe_device,
                                       SetWipeDeviceCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kWipeSelection) {
    LOG(ERROR) << "SetWipeDevice called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(CreateStateResultForInvalidRequest());
    return;
  }
  state_proto_.mutable_wipe_selection()->set_wipe_device(should_wipe_device);
  TransitionNextStateGeneric(std::move(callback));
}

void ShimlessRmaService::SetManuallyDisableWriteProtect(
    SetManuallyDisableWriteProtectCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kWpDisableMethod) {
    LOG(ERROR) << "SetManuallyDisableWriteProtect called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(CreateStateResultForInvalidRequest());
    return;
  }
  state_proto_.mutable_wp_disable_method()->set_disable_method(
      rmad::WriteProtectDisableMethodState::RMAD_WP_DISABLE_PHYSICAL);
  TransitionNextStateGeneric(std::move(callback));
}

void ShimlessRmaService::SetRsuDisableWriteProtect(
    SetRsuDisableWriteProtectCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kWpDisableMethod) {
    LOG(ERROR) << "SetRsuDisableWriteProtect called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(CreateStateResultForInvalidRequest());
    return;
  }
  state_proto_.mutable_wp_disable_method()->set_disable_method(
      rmad::WriteProtectDisableMethodState::RMAD_WP_DISABLE_RSU);
  TransitionNextStateGeneric(std::move(callback));
}

void ShimlessRmaService::GetRsuDisableWriteProtectChallenge(
    GetRsuDisableWriteProtectChallengeCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kWpDisableRsu) {
    LOG(ERROR)
        << "GetRsuDisableWriteProtectChallenge called from incorrect state "
        << state_proto_.state_case();
    std::move(callback).Run("");
    return;
  }
  std::move(callback).Run(state_proto_.wp_disable_rsu().challenge_code());
}

void ShimlessRmaService::GetRsuDisableWriteProtectHwid(
    GetRsuDisableWriteProtectHwidCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kWpDisableRsu) {
    LOG(ERROR) << "GetRsuDisableWriteProtectHwid called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run("");
    return;
  }
  std::move(callback).Run(state_proto_.wp_disable_rsu().hwid());
}

void ShimlessRmaService::GetRsuDisableWriteProtectChallengeQrCode(
    GetRsuDisableWriteProtectChallengeQrCodeCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kWpDisableRsu) {
    LOG(ERROR) << "GetRsuDisableWriteProtectChallengeQrCode called from "
                  "incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(std::vector<uint8_t>{});
    return;
  }

  shimless_rma_delegate_->GenerateQrCode(
      state_proto_.wp_disable_rsu().challenge_url(),
      base::BindOnce(&ShimlessRmaService::OnQrCodeGenerated,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ShimlessRmaService::OnQrCodeGenerated(
    GetRsuDisableWriteProtectChallengeQrCodeCallback callback,
    const std::string& qr_code_image) {
  std::move(callback).Run(
      std::vector<uint8_t>(qr_code_image.begin(), qr_code_image.end()));
}

void ShimlessRmaService::SetRsuDisableWriteProtectCode(
    const std::string& code,
    SetRsuDisableWriteProtectCodeCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kWpDisableRsu) {
    LOG(ERROR) << "SetRsuDisableWriteProtectCode called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(CreateStateResultForInvalidRequest());
    return;
  }
  state_proto_.mutable_wp_disable_rsu()->set_unlock_code(code);
  TransitionNextStateGeneric(std::move(callback));
}

void ShimlessRmaService::WriteProtectManuallyDisabled(
    WriteProtectManuallyDisabledCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kWpDisablePhysical) {
    LOG(ERROR) << "WriteProtectManuallyDisabled called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(CreateStateResultForInvalidRequest());
    return;
  }
  TransitionNextStateGeneric(std::move(callback));
}

void ShimlessRmaService::GetWriteProtectDisableCompleteAction(
    GetWriteProtectDisableCompleteActionCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kWpDisableComplete) {
    LOG(ERROR) << "ConfirmManualWpDisableComplete called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(
        rmad::WriteProtectDisableCompleteState::RMAD_WP_DISABLE_UNKNOWN);
    return;
  }
  std::move(callback).Run(state_proto_.wp_disable_complete().action());
}

void ShimlessRmaService::ConfirmManualWpDisableComplete(
    ConfirmManualWpDisableCompleteCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kWpDisableComplete) {
    LOG(ERROR) << "ConfirmManualWpDisableComplete called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(CreateStateResultForInvalidRequest());
    return;
  }
  TransitionNextStateGeneric(std::move(callback));
}

void ShimlessRmaService::GetComponentList(GetComponentListCallback callback) {
  std::vector<::rmad::ComponentsRepairState_ComponentRepairStatus> components;
  if (state_proto_.state_case() != rmad::RmadState::kComponentsRepair) {
    LOG(ERROR) << "GetComponentList called from incorrect state "
               << state_proto_.state_case();
  } else {
    components.reserve(state_proto_.components_repair().components_size());
    components.assign(state_proto_.components_repair().components().begin(),
                      state_proto_.components_repair().components().end());
  }
  std::move(callback).Run(std::move(components));
}

void ShimlessRmaService::SetComponentList(
    const std::vector<::rmad::ComponentsRepairState_ComponentRepairStatus>&
        component_list,
    SetComponentListCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kComponentsRepair) {
    LOG(ERROR) << "SetComponentList called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(CreateStateResultForInvalidRequest());
    return;
  }
  state_proto_.mutable_components_repair()->set_mainboard_rework(false);
  state_proto_.mutable_components_repair()->clear_components();
  state_proto_.mutable_components_repair()->mutable_components()->Reserve(
      component_list.size());
  for (auto& component : component_list) {
    rmad::ComponentsRepairState_ComponentRepairStatus* proto_component =
        state_proto_.mutable_components_repair()->add_components();
    proto_component->set_component(component.component());
    proto_component->set_repair_status(component.repair_status());
    proto_component->set_identifier(component.identifier());
  }
  TransitionNextStateGeneric(std::move(callback));
}

void ShimlessRmaService::ReworkMainboard(ReworkMainboardCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kComponentsRepair) {
    LOG(ERROR) << "ReworkMainboard called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(CreateStateResultForInvalidRequest());
    return;
  }
  state_proto_.mutable_components_repair()->set_mainboard_rework(true);
  TransitionNextStateGeneric(std::move(callback));
}

void ShimlessRmaService::RoFirmwareUpdateComplete(
    RoFirmwareUpdateCompleteCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kUpdateRoFirmware) {
    LOG(ERROR) << "RoFirmwareUpdateComplete called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(CreateStateResultForInvalidRequest());
    return;
  }
  state_proto_.mutable_update_ro_firmware()->set_choice(
      rmad::UpdateRoFirmwareState::RMAD_UPDATE_CHOICE_CONTINUE);
  TransitionNextStateGeneric(std::move(callback));
}

void ShimlessRmaService::ShutdownForRestock(
    ShutdownForRestockCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kRestock) {
    LOG(ERROR) << "ShutdownForRestock called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(CreateStateResultForInvalidRequest());
    return;
  }
  state_proto_.mutable_restock()->set_choice(
      rmad::RestockState::RMAD_RESTOCK_SHUTDOWN_AND_RESTOCK);
  TransitionNextStateGeneric(std::move(callback));
}
void ShimlessRmaService::ContinueFinalizationAfterRestock(
    ContinueFinalizationAfterRestockCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kRestock) {
    LOG(ERROR)
        << "ContinueFinalizationAfterRestock called from incorrect state "
        << state_proto_.state_case();
    std::move(callback).Run(CreateStateResultForInvalidRequest());
    return;
  }
  state_proto_.mutable_restock()->set_choice(
      rmad::RestockState::RMAD_RESTOCK_CONTINUE_RMA);
  TransitionNextStateGeneric(std::move(callback));
}

void ShimlessRmaService::GetRegionList(GetRegionListCallback callback) {
  std::vector<std::string> regions;
  if (state_proto_.state_case() != rmad::RmadState::kUpdateDeviceInfo) {
    LOG(ERROR) << "GetRegionList called from incorrect state "
               << state_proto_.state_case();
  } else {
    regions.reserve(state_proto_.update_device_info().region_list_size());
    regions.assign(state_proto_.update_device_info().region_list().begin(),
                   state_proto_.update_device_info().region_list().end());
  }
  std::move(callback).Run(std::move(regions));
}

void ShimlessRmaService::GetSkuList(GetSkuListCallback callback) {
  std::vector<uint64_t> skus;
  if (state_proto_.state_case() != rmad::RmadState::kUpdateDeviceInfo) {
    LOG(ERROR) << "GetSkuList called from incorrect state "
               << state_proto_.state_case();
  } else {
    skus.reserve(state_proto_.update_device_info().sku_list_size());
    skus.assign(state_proto_.update_device_info().sku_list().begin(),
                state_proto_.update_device_info().sku_list().end());
  }
  std::move(callback).Run(std::move(skus));
}

void ShimlessRmaService::GetCustomLabelList(
    GetCustomLabelListCallback callback) {
  std::vector<std::string> custom_labels;
  if (state_proto_.state_case() != rmad::RmadState::kUpdateDeviceInfo) {
    LOG(ERROR) << "GetCustomLabelList called from incorrect state "
               << state_proto_.state_case();
  } else {
    custom_labels.reserve(
        state_proto_.update_device_info().custom_label_list_size());
    custom_labels.assign(
        state_proto_.update_device_info().custom_label_list().begin(),
        state_proto_.update_device_info().custom_label_list().end());
  }
  std::move(callback).Run(std::move(custom_labels));
}

void ShimlessRmaService::GetSkuDescriptionList(
    GetSkuDescriptionListCallback callback) {
  std::vector<std::string> sku_descriptions;
  if (state_proto_.state_case() != rmad::RmadState::kUpdateDeviceInfo) {
    LOG(ERROR) << "GetSkuDescriptionList called from incorrect state "
               << state_proto_.state_case();
  } else {
    sku_descriptions.reserve(
        state_proto_.update_device_info().sku_description_list_size());
    sku_descriptions.assign(
        state_proto_.update_device_info().sku_description_list().begin(),
        state_proto_.update_device_info().sku_description_list().end());
  }
  std::move(callback).Run(std::move(sku_descriptions));
}

void ShimlessRmaService::GetOriginalSerialNumber(
    GetOriginalSerialNumberCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kUpdateDeviceInfo) {
    LOG(ERROR) << "GetOriginalSerialNumber called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run("");
    return;
  }
  std::move(callback).Run(
      state_proto_.update_device_info().original_serial_number());
}

void ShimlessRmaService::GetOriginalRegion(GetOriginalRegionCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kUpdateDeviceInfo) {
    LOG(ERROR) << "GetOriginalRegion called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(0);
    return;
  }
  std::move(callback).Run(
      state_proto_.update_device_info().original_region_index());
}

void ShimlessRmaService::GetOriginalSku(GetOriginalSkuCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kUpdateDeviceInfo) {
    LOG(ERROR) << "GetOriginalSku called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(0);
    return;
  }
  std::move(callback).Run(
      state_proto_.update_device_info().original_sku_index());
}

void ShimlessRmaService::GetOriginalCustomLabel(
    GetOriginalCustomLabelCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kUpdateDeviceInfo) {
    // TODO(gavindodd): Consider replacing all invalid call handling with
    // mojo::ReportBadMessage("error message");
    LOG(ERROR) << "GetOriginalCustomLabel called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(0);
    return;
  }
  std::move(callback).Run(
      state_proto_.update_device_info().original_custom_label_index());
}

void ShimlessRmaService::GetOriginalDramPartNumber(
    GetOriginalDramPartNumberCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kUpdateDeviceInfo) {
    // TODO(gavindodd): Consider replacing all invalid call handling with
    // mojo::ReportBadMessage("error message");
    LOG(ERROR) << "GetOriginalDramPartNumber called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run("");
    return;
  }
  std::move(callback).Run(
      state_proto_.update_device_info().original_dram_part_number());
}

void ShimlessRmaService::GetOriginalFeatureLevel(
    GetOriginalFeatureLevelCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kUpdateDeviceInfo) {
    LOG(ERROR) << "GetOriginalFeatureLevel called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(
        rmad::UpdateDeviceInfoState::RMAD_FEATURE_LEVEL_UNSUPPORTED);
    return;
  }
  std::move(callback).Run(
      state_proto_.update_device_info().original_feature_level());
}

void ShimlessRmaService::SetDeviceInformation(
    const std::string& serial_number,
    int32_t region_index,
    int32_t sku_index,
    int32_t custom_label_index,
    const std::string& dram_part_number,
    bool is_chassis_branded,
    int32_t hw_compliance_version,
    SetDeviceInformationCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kUpdateDeviceInfo) {
    LOG(ERROR) << "SetDeviceInformation called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(CreateStateResultForInvalidRequest());
    return;
  }
  state_proto_.mutable_update_device_info()->set_serial_number(serial_number);
  state_proto_.mutable_update_device_info()->set_region_index(region_index);
  state_proto_.mutable_update_device_info()->set_sku_index(sku_index);
  state_proto_.mutable_update_device_info()->set_custom_label_index(
      custom_label_index);
  state_proto_.mutable_update_device_info()->set_dram_part_number(
      dram_part_number);
  state_proto_.mutable_update_device_info()->set_is_chassis_branded(
      is_chassis_branded);
  state_proto_.mutable_update_device_info()->set_hw_compliance_version(
      hw_compliance_version);
  TransitionNextStateGeneric(std::move(callback));
}

void ShimlessRmaService::GetCalibrationComponentList(
    GetCalibrationComponentListCallback callback) {
  std::vector<::rmad::CalibrationComponentStatus> components;
  if (state_proto_.state_case() != rmad::RmadState::kCheckCalibration) {
    LOG(ERROR) << "GetCalibrationComponentList called from incorrect state "
               << state_proto_.state_case();
  } else {
    components.reserve(state_proto_.check_calibration().components_size());
    components.assign(state_proto_.check_calibration().components().begin(),
                      state_proto_.check_calibration().components().end());
  }
  std::move(callback).Run(std::move(components));
}

void ShimlessRmaService::GetCalibrationSetupInstructions(
    GetCalibrationSetupInstructionsCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kSetupCalibration) {
    LOG(ERROR) << "GetCalibrationSetupInstructions called from incorrect state "
               << state_proto_.state_case();
    // TODO(gavindodd): Is RMAD_CALIBRATION_INSTRUCTION_UNKNOWN the correct
    // error value? Confirm with rmad team that this is not overloaded.
    std::move(callback).Run(rmad::CalibrationSetupInstruction::
                                RMAD_CALIBRATION_INSTRUCTION_UNKNOWN);
    return;
  }
  std::move(callback).Run(state_proto_.setup_calibration().instruction());
}

void ShimlessRmaService::StartCalibration(
    const std::vector<::rmad::CalibrationComponentStatus>& components,
    StartCalibrationCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kCheckCalibration) {
    LOG(ERROR) << "StartCalibration called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(CreateStateResultForInvalidRequest());
    return;
  }
  state_proto_.mutable_check_calibration()->clear_components();
  state_proto_.mutable_check_calibration()->mutable_components()->Reserve(
      components.size());
  for (auto& component : components) {
    rmad::CalibrationComponentStatus* proto_component =
        state_proto_.mutable_check_calibration()->add_components();
    proto_component->set_component(component.component());
    // rmad only cares if the status is set to skip or not.
    proto_component->set_status(component.status());
    // Progress is not relevant when sending to rmad.
    proto_component->set_progress(0.0);
  }
  TransitionNextStateGeneric(std::move(callback));
}

void ShimlessRmaService::RunCalibrationStep(
    RunCalibrationStepCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kSetupCalibration) {
    LOG(ERROR) << "RunCalibrationStep called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(CreateStateResultForInvalidRequest());
    return;
  }

  // Clear the previous calibration progress.
  last_calibration_progress_ = std::nullopt;
  last_calibration_overall_progress_ = std::nullopt;

  TransitionNextStateGeneric(std::move(callback));
}

void ShimlessRmaService::ContinueCalibration(
    ContinueCalibrationCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kRunCalibration) {
    LOG(ERROR) << "ContinueCalibration called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(CreateStateResultForInvalidRequest());
    return;
  }
  TransitionNextStateGeneric(std::move(callback));
}

void ShimlessRmaService::CalibrationComplete(
    CalibrationCompleteCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kRunCalibration) {
    LOG(ERROR) << "CalibrationComplete called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(CreateStateResultForInvalidRequest());
    return;
  }
  TransitionNextStateGeneric(std::move(callback));
}

void ShimlessRmaService::RetryProvisioning(RetryProvisioningCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kProvisionDevice) {
    LOG(ERROR) << "RetryProvisioning called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(CreateStateResultForInvalidRequest());
    return;
  }
  state_proto_.mutable_provision_device()->set_choice(
      rmad::ProvisionDeviceState::RMAD_PROVISION_CHOICE_RETRY);
  TransitionNextStateGeneric(std::move(callback));
}

void ShimlessRmaService::ProvisioningComplete(
    ProvisioningCompleteCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kProvisionDevice) {
    LOG(ERROR) << "ProvisioningComplete called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(CreateStateResultForInvalidRequest());
    return;
  }
  state_proto_.mutable_provision_device()->set_choice(
      rmad::ProvisionDeviceState::RMAD_PROVISION_CHOICE_CONTINUE);
  TransitionNextStateGeneric(std::move(callback));
}

void ShimlessRmaService::RetryFinalization(RetryFinalizationCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kFinalize) {
    LOG(ERROR) << "RetryFinalization called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(CreateStateResultForInvalidRequest());
    return;
  }
  state_proto_.mutable_finalize()->set_choice(
      rmad::FinalizeState::RMAD_FINALIZE_CHOICE_RETRY);
  TransitionNextStateGeneric(std::move(callback));
}

void ShimlessRmaService::FinalizationComplete(
    FinalizationCompleteCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kFinalize) {
    LOG(ERROR) << "FinalizationComplete called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(CreateStateResultForInvalidRequest());
    return;
  }
  state_proto_.mutable_finalize()->set_choice(
      rmad::FinalizeState::RMAD_FINALIZE_CHOICE_CONTINUE);
  TransitionNextStateGeneric(std::move(callback));
}

void ShimlessRmaService::WriteProtectManuallyEnabled(
    WriteProtectManuallyEnabledCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kWpEnablePhysical) {
    LOG(ERROR) << "WriteProtectManuallyEnabled called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(CreateStateResultForInvalidRequest());
    return;
  }
  TransitionNextStateGeneric(std::move(callback));
}

void ShimlessRmaService::GetLog(GetLogCallback callback) {
  RmadClient::Get()->GetLog(base::BindOnce(&ShimlessRmaService::OnGetLog,
                                           weak_ptr_factory_.GetWeakPtr(),
                                           std::move(callback)));
}

void ShimlessRmaService::SaveLog(SaveLogCallback callback) {
  if (diagnostics::DiagnosticsLogController::IsInitialized()) {
    task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(
            &diagnostics::DiagnosticsLogController::
                GenerateSessionStringOnBlockingPool,
            // base::Unretained safe here because ~DiagnosticsLogController is
            // called during shutdown of ash::Shell and will out-live
            // ShimlessRmaService.
            base::Unretained(diagnostics::DiagnosticsLogController::Get())),
        base::BindOnce(&ShimlessRmaService::OnDiagnosticsLogReady,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }

  OnDiagnosticsLogReady(std::move(callback), "");
}

void ShimlessRmaService::OnDiagnosticsLogReady(
    SaveLogCallback callback,
    const std::string& diagnostics_log_text) {
  RmadClient::Get()->SaveLog(
      diagnostics_log_text,
      base::BindOnce(&ShimlessRmaService::OnSaveLog,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ShimlessRmaService::OnGetLog(GetLogCallback callback,
                                  std::optional<rmad::GetLogReply> response) {
  if (!response) {
    LOG(ERROR) << "Failed to call rmad::GetLog";
    std::move(callback).Run("",
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
    return;
  }

  std::move(callback).Run(response->log(), response->error());
}

void ShimlessRmaService::OnSaveLog(SaveLogCallback callback,
                                   std::optional<rmad::SaveLogReply> response) {
  if (!response) {
    LOG(ERROR) << "Failed to call rmad::SaveLog";
    std::move(callback).Run(base::FilePath(""),
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
    return;
  }

  std::move(callback).Run(base::FilePath(response->save_path()),
                          response->error());
}

void ShimlessRmaService::GetPowerwashRequired(
    GetPowerwashRequiredCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kRepairComplete) {
    LOG(ERROR) << "GetPowerwashRequired called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(false);
    return;
  }

  std::move(callback).Run(state_proto_.repair_complete().powerwash_required());
}

void ShimlessRmaService::LaunchDiagnostics() {
  if (state_proto_.state_case() != rmad::RmadState::kRepairComplete) {
    LOG(ERROR) << "LaunchDiagnostics called from incorrect state "
               << state_proto_.state_case();
    return;
  }
  shimless_rma_delegate_->ShowDiagnosticsDialog();

  SendMetricOnLaunchDiagnostics();
}

void ShimlessRmaService::EndRma(
    rmad::RepairCompleteState::ShutdownMethod shutdown_method,
    EndRmaCallback callback) {
  DCHECK_NE(rmad::RepairCompleteState::RMAD_REPAIR_COMPLETE_UNKNOWN,
            shutdown_method);
  if (state_proto_.state_case() != rmad::RmadState::kRepairComplete) {
    LOG(ERROR) << "EndRma called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(CreateStateResultForInvalidRequest());
    return;
  }

  ForgetNewNetworkConnections(base::BindOnce(
      &ShimlessRmaService::EndRmaForgetNetworkResponse,
      weak_ptr_factory_.GetWeakPtr(), shutdown_method, std::move(callback)));
}

void ShimlessRmaService::EndRmaForgetNetworkResponse(
    rmad::RepairCompleteState::ShutdownMethod shutdown_method,
    EndRmaCallback callback) {
  state_proto_.mutable_repair_complete()->set_shutdown(shutdown_method);
  TransitionNextStateGeneric(std::move(callback));
}

////////////////////////////////
// Metrics
void ShimlessRmaService::SendMetricOnLaunchDiagnostics() {
  rmad::RecordBrowserActionMetricRequest request;
  request.set_diagnostics(true);
  request.set_os_update(false);

  RmadClient::Get()->RecordBrowserActionMetric(
      request, base::BindOnce(&ShimlessRmaService::OnMetricsReply,
                              weak_ptr_factory_.GetWeakPtr()));
}

void ShimlessRmaService::SendMetricOnUpdateOs() {
  rmad::RecordBrowserActionMetricRequest request;
  request.set_diagnostics(false);
  request.set_os_update(true);

  RmadClient::Get()->RecordBrowserActionMetric(
      request, base::BindOnce(&ShimlessRmaService::OnMetricsReply,
                              weak_ptr_factory_.GetWeakPtr()));
}

void ShimlessRmaService::OnMetricsReply(
    std::optional<rmad::RecordBrowserActionMetricReply> response) {
  if (!response) {
    LOG(ERROR) << "Failed to call rmad::RecordBrowserActionMetric";
    return;
  }

  if (response->error() != rmad::RmadErrorCode::RMAD_ERROR_OK) {
    LOG(ERROR) << "Failed to upload metrics";
  }
}

////////////////////////////////
// Observers
void ShimlessRmaService::Error(rmad::RmadErrorCode error) {
  if (error_observer_.is_bound()) {
    error_observer_->OnError(error);
  }
}

void ShimlessRmaService::OsUpdateProgress(update_engine::Operation operation,
                                          double progress,
                                          update_engine::ErrorCode error_code) {
  DCHECK(features::IsShimlessRMAOsUpdateEnabled());
  if (os_update_observer_.is_bound()) {
    os_update_observer_->OnOsUpdateProgressUpdated(operation, progress,
                                                   error_code);
  }
}

void ShimlessRmaService::CalibrationProgress(
    const rmad::CalibrationComponentStatus& component_status) {
  last_calibration_progress_ = component_status;
  if (calibration_observer_.is_bound()) {
    calibration_observer_->OnCalibrationUpdated(component_status);
  }
}

void ShimlessRmaService::CalibrationOverallProgress(
    rmad::CalibrationOverallStatus status) {
  last_calibration_overall_progress_ = status;
  if (calibration_observer_.is_bound()) {
    calibration_observer_->OnCalibrationStepComplete(status);
  }
}

void ShimlessRmaService::ProvisioningProgress(
    const rmad::ProvisionStatus& status) {
  if (status.status() ==
          rmad::ProvisionStatus::RMAD_PROVISION_STATUS_FAILED_BLOCKING ||
      status.status() ==
          rmad::ProvisionStatus::RMAD_PROVISION_STATUS_FAILED_NON_BLOCKING) {
    LOG(ERROR) << "Provisioning failed with error " << status.error();
  }

  last_provisioning_progress_ = status;
  if (provisioning_observer_.is_bound()) {
    provisioning_observer_->OnProvisioningUpdated(
        status.status(), status.progress(), status.error());
  }
}

void ShimlessRmaService::HardwareWriteProtectionState(bool enabled) {
  last_hardware_protection_state_ = enabled;
  if (hwwp_state_observer_.is_bound()) {
    hwwp_state_observer_->OnHardwareWriteProtectionStateChanged(enabled);
  }
}

void ShimlessRmaService::PowerCableState(bool plugged_in) {
  last_power_cable_state_ = plugged_in;
  if (power_cable_observer_.is_bound()) {
    power_cable_observer_->OnPowerCableStateChanged(plugged_in);
  }
}

void ShimlessRmaService::ExternalDiskState(bool detected) {
  last_external_disk_state_ = detected;
  for (auto& external_disk_state_observer : external_disk_state_observers_) {
    external_disk_state_observer->OnExternalDiskStateChanged(
        *last_external_disk_state_);
  }
}

void ShimlessRmaService::HardwareVerificationResult(
    const rmad::HardwareVerificationResult& result) {
  last_hardware_verification_result_ = result;
  for (auto& observer : hardware_verification_observers_) {
    observer->OnHardwareVerificationResult(result.is_compliant(),
                                           result.error_str());
  }
}

void ShimlessRmaService::FinalizationProgress(
    const rmad::FinalizeStatus& status) {
  if (status.status() ==
          rmad::FinalizeStatus::RMAD_FINALIZE_STATUS_FAILED_BLOCKING ||
      status.status() ==
          rmad::FinalizeStatus::RMAD_FINALIZE_STATUS_FAILED_NON_BLOCKING) {
    LOG(ERROR) << "Finalization failed with error " << status.error();
  }

  last_finalization_progress_ = status;
  if (finalization_observer_.is_bound()) {
    finalization_observer_->OnFinalizationUpdated(
        status.status(), status.progress(), status.error());
  }
}

void ShimlessRmaService::RoFirmwareUpdateProgress(
    rmad::UpdateRoFirmwareStatus status) {
  last_update_ro_firmware_progress_ = status;
  if (update_ro_firmware_observer_.is_bound()) {
    update_ro_firmware_observer_->OnUpdateRoFirmwareStatusChanged(status);
  }
}

void ShimlessRmaService::ObserveError(
    ::mojo::PendingRemote<mojom::ErrorObserver> observer) {
  error_observer_.Bind(std::move(observer));
}

void ShimlessRmaService::ObserveOsUpdateProgress(
    ::mojo::PendingRemote<mojom::OsUpdateObserver> observer) {
  DCHECK(features::IsShimlessRMAOsUpdateEnabled());
  os_update_observer_.Bind(std::move(observer));
}

void ShimlessRmaService::ObserveCalibrationProgress(
    ::mojo::PendingRemote<mojom::CalibrationObserver> observer) {
  if (calibration_observer_.is_bound()) {
    calibration_observer_.reset();
  }

  calibration_observer_.Bind(std::move(observer));
  if (last_calibration_progress_) {
    calibration_observer_->OnCalibrationUpdated(*last_calibration_progress_);
  }
  if (last_calibration_overall_progress_) {
    calibration_observer_->OnCalibrationStepComplete(
        *last_calibration_overall_progress_);
  }
}

void ShimlessRmaService::ObserveProvisioningProgress(
    ::mojo::PendingRemote<mojom::ProvisioningObserver> observer) {
  provisioning_observer_.Bind(std::move(observer));
  if (last_provisioning_progress_) {
    provisioning_observer_->OnProvisioningUpdated(
        last_provisioning_progress_->status(),
        last_provisioning_progress_->progress(),
        last_provisioning_progress_->error());
  }
}

void ShimlessRmaService::ObserveHardwareWriteProtectionState(
    ::mojo::PendingRemote<mojom::HardwareWriteProtectionStateObserver>
        observer) {
  hwwp_state_observer_.Bind(std::move(observer));
  if (last_hardware_protection_state_) {
    hwwp_state_observer_->OnHardwareWriteProtectionStateChanged(
        *last_hardware_protection_state_);
  }
}

void ShimlessRmaService::ObservePowerCableState(
    ::mojo::PendingRemote<mojom::PowerCableStateObserver> observer) {
  power_cable_observer_.Bind(std::move(observer));
  if (last_power_cable_state_) {
    power_cable_observer_->OnPowerCableStateChanged(*last_power_cable_state_);
  }
}

void ShimlessRmaService::ObserveExternalDiskState(
    ::mojo::PendingRemote<mojom::ExternalDiskStateObserver> observer) {
  external_disk_state_observers_.Add(std::move(observer));
  if (last_external_disk_state_) {
    for (auto& external_disk_state_observer : external_disk_state_observers_) {
      external_disk_state_observer->OnExternalDiskStateChanged(
          *last_external_disk_state_);
    }
  }
}

void ShimlessRmaService::ObserveHardwareVerificationStatus(
    ::mojo::PendingRemote<mojom::HardwareVerificationStatusObserver> observer) {
  hardware_verification_observers_.Add(std::move(observer));
  if (last_hardware_verification_result_) {
    for (auto& hardware_verification_observer :
         hardware_verification_observers_) {
      hardware_verification_observer->OnHardwareVerificationResult(
          last_hardware_verification_result_->is_compliant(),
          last_hardware_verification_result_->error_str());
    }
  }
}

void ShimlessRmaService::ObserveFinalizationStatus(
    ::mojo::PendingRemote<mojom::FinalizationObserver> observer) {
  finalization_observer_.Bind(std::move(observer));
  if (last_finalization_progress_) {
    finalization_observer_->OnFinalizationUpdated(
        last_finalization_progress_->status(),
        last_finalization_progress_->progress(),
        last_finalization_progress_->error());
  }
}

void ShimlessRmaService::ObserveRoFirmwareUpdateProgress(
    ::mojo::PendingRemote<mojom::UpdateRoFirmwareObserver> observer) {
  update_ro_firmware_observer_.Bind(std::move(observer));
  if (last_update_ro_firmware_progress_) {
    update_ro_firmware_observer_->OnUpdateRoFirmwareStatusChanged(
        *last_update_ro_firmware_progress_);
  }
}

////////////////////////////////
// Mojom binding.
void ShimlessRmaService::BindInterface(
    mojo::PendingReceiver<mojom::ShimlessRmaService> pending_receiver) {
  receiver_.Bind(std::move(pending_receiver));
}

////////////////////////////////
// RmadClient response handlers.
template <class Callback>
void ShimlessRmaService::TransitionNextStateGeneric(Callback callback) {
  RmadClient::Get()->TransitionNextState(
      state_proto_,
      base::BindOnce(&ShimlessRmaService::OnGetStateResponse<Callback>,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     kTransitNextState));
}

template <class Callback>
void ShimlessRmaService::OnGetStateResponse(
    Callback callback,
    StateResponseCalledFrom called_from,
    std::optional<rmad::GetStateReply> response) {
  if (!response) {
    LOG(ERROR) << "Failed to call rmadClient";
    critical_error_occurred_ = true;
    std::move(callback).Run(
        CreateStateResult(mojom::State::kUnknown,
                          /*can_exit=*/false,
                          /*can_go_back=*/false,
                          rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID));
    return;
  }
  // TODO(gavindodd): When platform and chrome release cycles are decoupled
  // there needs to be a way to detect an unexpected state and switch to update
  // Chrome screen.
  state_proto_ = response->state();
  can_abort_ = response->can_abort();
  can_go_back_ = response->can_go_back();
  mojo_state_ = RmadStateToMojo(state_proto_.state_case());
  if (response->error() != rmad::RMAD_ERROR_OK) {
    LOG(ERROR) << "rmadClient returned error " << response->error();
    if (response->error() == rmad::RMAD_ERROR_RMA_NOT_REQUIRED) {
      critical_error_occurred_ = true;
    }
    std::move(callback).Run(
        CreateStateResult(RmadStateToMojo(state_proto_.state_case()),
                          can_abort_, can_go_back_, response->error()));
    return;
  }

  // This is a special case we need to check to make sure if user has seen
  // the NetworkPage and clicks back button from the next page. The user should
  // be back to the NetworkPage. The reason why it needs special check is
  // because of state mismatch between shimless mojom and rmad. In this case,
  // the mojom kConfigureNetwork state doesn't match to any rmad state.
  if (called_from == kTransitPreviousState && user_has_seen_network_page_ &&
      state_proto_.state_case() == rmad::RmadState::kWelcome &&
      mojo_state_ == mojom::State::kWelcomeScreen) {
    user_has_seen_network_page_ = false;
    state_proto_.mutable_welcome()->set_choice(
        rmad::WelcomeState::RMAD_CHOICE_FINALIZE_REPAIR);

    mojo_state_ = mojom::State::kConfigureNetwork;
    std::move(callback).Run(
        CreateStateResult(mojom::State::kConfigureNetwork,
                          /*can_exit=*/true, /*can_go_back=*/true,
                          rmad::RmadErrorCode::RMAD_ERROR_OK));
    return;
  }

  std::move(callback).Run(
      CreateStateResult(RmadStateToMojo(state_proto_.state_case()), can_abort_,
                        can_go_back_, rmad::RmadErrorCode::RMAD_ERROR_OK));
}

void ShimlessRmaService::OnAbortRmaResponse(
    AbortRmaCallback callback,
    bool reboot,
    std::optional<rmad::AbortRmaReply> response) {
  const rmad::RmadErrorCode error_code =
      response ? response->error()
               : rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID;

  // Only reboot or exit to login if abort was successful (state will be
  // RMAD_ERROR_RMA_NOT_REQUIRED) or a critical error has occurred.
  const bool should_exit_rma = critical_error_occurred_ ||
                               error_code == rmad::RMAD_ERROR_RMA_NOT_REQUIRED;
  if (!should_exit_rma) {
    std::move(callback).Run(error_code);
    return;
  }

  ForgetNewNetworkConnections(base::BindOnce(
      &ShimlessRmaService::AbortRmaForgetNetworkResponse,
      weak_ptr_factory_.GetWeakPtr(), std::move(callback), reboot, response));
}

void ShimlessRmaService::AbortRmaForgetNetworkResponse(
    AbortRmaCallback callback,
    bool reboot,
    std::optional<rmad::AbortRmaReply> response) {
  // Send status before shutting down or restarting Chrome session.
  std::move(callback).Run(rmad::RMAD_ERROR_OK);

  // Either reboot the device or just restart the Chrome session.
  if (reboot) {
    VLOG(1) << "Rebooting...";
    chromeos::PowerManagerClient::Get()->RequestRestart(
        power_manager::REQUEST_RESTART_FOR_USER,
        critical_error_occurred_
            ? "Rebooting after user cancelled RMA due to critical error."
            : "Rebooting after user cancelled RMA.");
  } else {
    VLOG(1) << "Restarting Chrome to bypass RMA after cancel request.";
    shimless_rma_delegate_->ExitRmaThenRestartChrome();
  }
}

void ShimlessRmaService::OnOsUpdateStatusCallback(
    update_engine::Operation operation,
    double progress,
    bool rollback,
    bool powerwash,
    const std::string& version,
    int64_t update_size,
    update_engine::ErrorCode error_code) {
  DCHECK(features::IsShimlessRMAOsUpdateEnabled());
  if (check_os_callback_) {
    switch (operation) {
      // If IDLE is received when there is a callback it means no update is
      // available.
      case update_engine::Operation::DISABLED:
      case update_engine::Operation::ERROR:
      case update_engine::Operation::IDLE:
      case update_engine::Operation::REPORTING_ERROR_EVENT:
        std::move(check_os_callback_).Run("");
        break;
      case update_engine::Operation::UPDATE_AVAILABLE:
        std::move(check_os_callback_).Run(version);
        break;
      case update_engine::Operation::ATTEMPTING_ROLLBACK:
      case update_engine::Operation::CHECKING_FOR_UPDATE:
      case update_engine::Operation::DOWNLOADING:
      case update_engine::Operation::FINALIZING:
      case update_engine::Operation::NEED_PERMISSION_TO_UPDATE:
      case update_engine::Operation::UPDATED_NEED_REBOOT:
      case update_engine::Operation::VERIFYING:
      case update_engine::Operation::CLEANUP_PREVIOUS_UPDATE:
      case update_engine::Operation::UPDATED_BUT_DEFERRED:
        break;
      // Added to avoid lint error
      case update_engine::Operation::Operation_INT_MIN_SENTINEL_DO_NOT_USE_:
      case update_engine::Operation::Operation_INT_MAX_SENTINEL_DO_NOT_USE_:
        NOTREACHED();
    }
  }
  OsUpdateProgress(operation, progress, error_code);
}

void ShimlessRmaService::OsUpdateOrNextRmadStateCallback(
    TransitionStateCallback callback,
    const std::string& version) {
  if (version.empty()) {
    TransitionNextStateGeneric(std::move(callback));
  } else {
    mojo_state_ = mojom::State::kUpdateOs;
    std::move(callback).Run(
        CreateStateResult(mojom::State::kUpdateOs,
                          /*can_exit=*/true, /*can_go_back=*/true,
                          rmad::RmadErrorCode::RMAD_ERROR_OK));
  }
}

void ShimlessRmaService::SetCriticalErrorOccurredForTest(
    bool critical_error_occurred) {
  critical_error_occurred_ = critical_error_occurred;
}

////////////////////////////////
// Methods related to 3p diagnostics.
void ShimlessRmaService::Get3pDiagnosticsProvider(
    Get3pDiagnosticsProviderCallback callback) {
  ash::cros_healthd::ServiceConnection::GetInstance()
      ->GetProbeService()
      ->ProbeTelemetryInfo(
          {ash::cros_healthd::mojom::ProbeCategoryEnum::kSystem},
          base::BindOnce(&ShimlessRmaService::OnGetSystemInfoFor3pDiag,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ShimlessRmaService::OnGetSystemInfoFor3pDiag(
    Get3pDiagnosticsProviderCallback callback,
    ash::cros_healthd::mojom::TelemetryInfoPtr telemetry_info) {
  if (!telemetry_info->system_result ||
      !telemetry_info->system_result->is_system_info() ||
      !telemetry_info->system_result->get_system_info()->os_info->oem_name) {
    LOG(ERROR) << "Failed to get oem name from cros_healthd";
    std::move(callback).Run(std::nullopt);
    return;
  }

  const std::string& oem_name = telemetry_info->system_result->get_system_info()
                                    ->os_info->oem_name.value();
  if (shimless_rma_delegate_->IsChromeOSSystemExtensionProvider(oem_name)) {
    std::move(callback).Run(oem_name);
    return;
  }

  std::move(callback).Run(std::nullopt);
}

void ShimlessRmaService::GetInstallable3pDiagnosticsAppPath(
    GetInstallable3pDiagnosticsAppPathCallback callback) {
  RmadClient::Get()->ExtractExternalDiagnosticsApp(
      base::BindOnce(&ShimlessRmaService::OnExtractExternalDiagnosticsApp,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ShimlessRmaService::OnExtractExternalDiagnosticsApp(
    GetInstallable3pDiagnosticsAppPathCallback callback,
    std::optional<rmad::ExtractExternalDiagnosticsAppReply> response) {
  if (!response || response->error() != rmad::RmadErrorCode::RMAD_ERROR_OK) {
    LOG_IF(ERROR, !response)
        << "Failed to call rmad::ExtractExternalDiagnosticsApp";
    LOG_IF(ERROR,
           response &&
               response->error() !=
                   rmad::RmadErrorCode::RMAD_ERROR_DIAGNOSTICS_APP_NOT_FOUND)
        << "Unexpected result from rmad::ExtractExternalDiagnosticsApp: "
        << response->error();
    extracted_3p_diag_swbn_path_ = base::FilePath{};
    extracted_3p_diag_crx_path_ = base::FilePath{};
    std::move(callback).Run(std::nullopt);
    return;
  }

  extracted_3p_diag_swbn_path_ =
      base::FilePath{response->diagnostics_app_swbn_path()};
  extracted_3p_diag_crx_path_ =
      base::FilePath{response->diagnostics_app_crx_path()};
  std::move(callback).Run(
      base::FilePath{response->diagnostics_app_swbn_path()});
}

void ShimlessRmaService::InstallLastFound3pDiagnosticsApp(
    InstallLastFound3pDiagnosticsAppCallback callback) {
  if (extracted_3p_diag_swbn_path_.empty() ||
      extracted_3p_diag_swbn_path_.empty()) {
    LOG(ERROR) << "Should call GetInstallable3pDiagnosticsAppPath first";
    std::move(callback).Run(nullptr);
    return;
  }

  shimless_rma_delegate_->PrepareDiagnosticsAppBrowserContext(
      extracted_3p_diag_crx_path_, extracted_3p_diag_swbn_path_,
      base::BindOnce(&ShimlessRmaService::On3pDiagnosticsAppLoadForInstallation,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ShimlessRmaService::On3pDiagnosticsAppLoadForInstallation(
    InstallLastFound3pDiagnosticsAppCallback callback,
    base::expected<
        ShimlessRmaDelegate::PrepareDiagnosticsAppBrowserContextResult,
        std::string> result) {
  if (!result.has_value()) {
    LOG(ERROR) << "Failed to load 3p diag app: " << result.error();
    std::move(callback).Run(nullptr);
    return;
  }

  shimless_app_browser_context_ = result.value().context;
  shimless_3p_diag_iwa_id_ = result.value().iwa_id;
  shimless_3p_diag_app_name_ = result.value().name;

  auto app_info = ash::shimless_rma::mojom::Shimless3pDiagnosticsAppInfo::New();
  app_info->name = result.value().name;
  app_info->permission_message = result.value().permission_message;
  std::move(callback).Run(std::move(app_info));
}

void ShimlessRmaService::CompleteLast3pDiagnosticsInstallation(
    bool is_approved,
    CompleteLast3pDiagnosticsInstallationCallback callback) {
  if (!is_approved) {
    // Clean the cached app so it will be reloaded next time calling
    // `Show3pDiagnosticsApp`.
    shimless_app_browser_context_ = nullptr;
    shimless_3p_diag_iwa_id_ = std::nullopt;
    shimless_3p_diag_app_name_ = "";
    std::move(callback).Run();
    return;
  }

  RmadClient::Get()->InstallExtractedDiagnosticsApp(base::BindOnce(
      [](CompleteLast3pDiagnosticsInstallationCallback callback,
         std::optional<rmad::InstallExtractedDiagnosticsAppReply> response) {
        LOG_IF(ERROR, !response)
            << "Failed to call rmad::InstallExtractedDiagnosticsApp";
        LOG_IF(ERROR, response->error() != rmad::RmadErrorCode::RMAD_ERROR_OK)
            << "rmad::InstallExtractedDiagnosticsApp returned "
            << response->error();
        std::move(callback).Run();
      },
      std::move(callback)));
}

void ShimlessRmaService::Show3pDiagnosticsApp(
    Show3pDiagnosticsAppCallback callback) {
  if (!shimless_app_browser_context_) {
    RmadClient::Get()->GetInstalledDiagnosticsApp(
        base::BindOnce(&ShimlessRmaService::GetInstalledDiagnosticsApp,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }

  ExternalAppDialog::InitParams params;
  params.context = shimless_app_browser_context_;
  params.app_name = shimless_3p_diag_app_name_;
  params.content_url = GURL("isolated-app://" + shimless_3p_diag_iwa_id_->id());
  params.shimless_rma_delegate = shimless_rma_delegate_->GetWeakPtr();
  ExternalAppDialog::Show(params);
  std::move(callback).Run(
      ash::shimless_rma::mojom::Show3pDiagnosticsAppResult::kOk);
}

void ShimlessRmaService::GetInstalledDiagnosticsApp(
    Show3pDiagnosticsAppCallback callback,
    std::optional<rmad::GetInstalledDiagnosticsAppReply> response) {
  if (!response) {
    LOG(ERROR) << "Failed to call rmad::GetInstalledDiagnosticsApp";
    std::move(callback).Run(
        ash::shimless_rma::mojom::Show3pDiagnosticsAppResult::kFailedToLoad);
    return;
  }

  switch (response->error()) {
    case rmad::RmadErrorCode::RMAD_ERROR_DIAGNOSTICS_APP_NOT_FOUND:
      std::move(callback).Run(ash::shimless_rma::mojom::
                                  Show3pDiagnosticsAppResult::kAppNotInstalled);
      return;
    case rmad::RmadErrorCode::RMAD_ERROR_OK:
      shimless_rma_delegate_->PrepareDiagnosticsAppBrowserContext(
          base::FilePath{response->diagnostics_app_crx_path()},
          base::FilePath{response->diagnostics_app_swbn_path()},
          base::BindOnce(&ShimlessRmaService::On3pDiagnosticsAppLoadForShow,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
      return;
    default:
      LOG(ERROR) << "rmad::GetInstalledDiagnosticsApp returned "
                 << response->error();
      std::move(callback).Run(
          ash::shimless_rma::mojom::Show3pDiagnosticsAppResult::kFailedToLoad);
      return;
  }
}

void ShimlessRmaService::On3pDiagnosticsAppLoadForShow(
    Show3pDiagnosticsAppCallback callback,
    base::expected<
        ShimlessRmaDelegate::PrepareDiagnosticsAppBrowserContextResult,
        std::string> result) {
  if (!result.has_value()) {
    LOG(ERROR) << "Failed to load 3p diag app: " << result.error();
    std::move(callback).Run(
        ash::shimless_rma::mojom::Show3pDiagnosticsAppResult::kFailedToLoad);
    return;
  }

  shimless_app_browser_context_ = result.value().context;
  shimless_3p_diag_iwa_id_ = result.value().iwa_id;
  shimless_3p_diag_app_name_ = result.value().name;
  Show3pDiagnosticsApp(std::move(callback));
}

}  // namespace shimless_rma
}  // namespace ash
