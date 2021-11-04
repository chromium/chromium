// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/shimless_rma/backend/shimless_rma_service.h"

#include "ash/public/cpp/network_config_service.h"
#include "ash/webui/shimless_rma/backend/version_updater.h"
#include "ash/webui/shimless_rma/mojom/shimless_rma.mojom.h"
#include "ash/webui/shimless_rma/mojom/shimless_rma_mojom_traits.h"
#include "base/bind.h"
#include "base/logging.h"
#include "chromeos/dbus/rmad/rmad.pb.h"
#include "chromeos/dbus/rmad/rmad_client.h"
#include "chromeos/dbus/util/version_loader.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "components/qr_code_generator/qr_code_generator.h"

using chromeos::network_config::mojom::ConnectionStateType;
using chromeos::network_config::mojom::FilterType;
using chromeos::network_config::mojom::NetworkFilter;
using chromeos::network_config::mojom::NetworkStatePropertiesPtr;
using chromeos::network_config::mojom::NetworkType;

namespace ash {
namespace shimless_rma {

namespace {

mojom::RmaState RmadStateToMojo(rmad::RmadState::StateCase rmadState) {
  return mojo::EnumTraits<ash::shimless_rma::mojom::RmaState,
                          rmad::RmadState::StateCase>::ToMojom(rmadState);
}

// Returns whether the device is connected to an unmetered network.
// Metered networks are excluded for RMA to avoid any cost to the owner who
// does not have control of the device during RMA.
bool HaveAllowedNetworkConnection() {
  chromeos::NetworkStateHandler* network_state_handler =
      chromeos::NetworkHandler::Get()->network_state_handler();
  const chromeos::NetworkState* network =
      network_state_handler->DefaultNetwork();
  // TODO(gavindodd): Confirm that metered networks should be excluded.
  const bool metered = network_state_handler->default_network_is_metered();
  // Return true if connected to an unmetered network.
  return network && network->IsConnectedState() && !metered;
}

}  // namespace

ShimlessRmaService::ShimlessRmaService() {
  chromeos::RmadClient::Get()->AddObserver(this);
  GetNetworkConfigService(
      remote_cros_network_config_.BindNewPipeAndPassReceiver());
  version_updater_.SetStatusCallback(
      base::BindRepeating(&ShimlessRmaService::OnOsUpdateStatusCallback,
                          weak_ptr_factory_.GetWeakPtr()));
  // Check if an OS update is available to minimize delays if needed later.
  if (HaveAllowedNetworkConnection()) {
    version_updater_.CheckOsUpdateAvailable();
  }
}

ShimlessRmaService::~ShimlessRmaService() {
  chromeos::RmadClient::Get()->RemoveObserver(this);
}

void ShimlessRmaService::GetCurrentState(GetCurrentStateCallback callback) {
  chromeos::RmadClient::Get()->GetCurrentState(base::BindOnce(
      &ShimlessRmaService::OnGetStateResponse<GetCurrentStateCallback>,
      weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

// TODO(gavindodd): Handle transition back from wifi connect and os update pages
void ShimlessRmaService::TransitionPreviousState(
    TransitionPreviousStateCallback callback) {
  chromeos::RmadClient::Get()->TransitionPreviousState(base::BindOnce(
      &ShimlessRmaService::OnGetStateResponse<TransitionPreviousStateCallback>,
      weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ShimlessRmaService::AbortRma(AbortRmaCallback callback) {
  chromeos::RmadClient::Get()->AbortRma(
      base::BindOnce(&ShimlessRmaService::OnAbortRmaResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ShimlessRmaService::BeginFinalization(BeginFinalizationCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kWelcome ||
      mojo_state_ != mojom::RmaState::kWelcomeScreen) {
    LOG(ERROR) << "FinalizeRepair called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(RmadStateToMojo(state_proto_.state_case()),
                            can_abort_, can_go_back_,
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
    return;
  }
  state_proto_.mutable_welcome()->set_choice(
      rmad::WelcomeState::RMAD_CHOICE_FINALIZE_REPAIR);

  if (!HaveAllowedNetworkConnection()) {
    remote_cros_network_config_->GetNetworkStateList(
        NetworkFilter::New(FilterType::kVisible, NetworkType::kAll,
                           /*limit=*/0),
        base::BindOnce(&ShimlessRmaService::OnNetworkListResponse,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  } else {
    check_os_callback_ =
        base::BindOnce(&ShimlessRmaService::OsUpdateOrNextRmadStateCallback,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback));
    version_updater_.CheckOsUpdateAvailable();
  }
}

void ShimlessRmaService::NetworkSelectionComplete(
    NetworkSelectionCompleteCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kWelcome ||
      mojo_state_ != mojom::RmaState::kConfigureNetwork) {
    LOG(ERROR) << "NetworkSelectionComplete called from incorrect state "
               << state_proto_.state_case() << " / " << mojo_state_;
    std::move(callback).Run(RmadStateToMojo(state_proto_.state_case()),
                            can_abort_, can_go_back_,
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
    return;
  }
  if (HaveAllowedNetworkConnection()) {
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
  // TODO(gavindodd): Decide whether to use full or short Chrome version.
  std::move(callback).Run(chromeos::version_loader::GetVersion(
      chromeos::version_loader::VERSION_FULL));
}

void ShimlessRmaService::CheckForOsUpdates(CheckForOsUpdatesCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kWelcome ||
      mojo_state_ != mojom::RmaState::kUpdateOs) {
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
  if (state_proto_.state_case() != rmad::RmadState::kWelcome ||
      mojo_state_ != mojom::RmaState::kUpdateOs) {
    LOG(ERROR) << "UpdateOs called from incorrect state "
               << state_proto_.state_case() << " / " << mojo_state_;
    std::move(callback).Run(false);
    return;
  }
  std::move(callback).Run(version_updater_.UpdateOs());
}

void ShimlessRmaService::UpdateOsSkipped(UpdateOsSkippedCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kWelcome ||
      mojo_state_ != mojom::RmaState::kUpdateOs) {
    LOG(ERROR) << "UpdateOsSkipped called from incorrect state "
               << state_proto_.state_case() << " / " << mojo_state_;
    std::move(callback).Run(RmadStateToMojo(state_proto_.state_case()),
                            can_abort_, can_go_back_,
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
    return;
  }
  if (!version_updater_.IsIdle()) {
    LOG(ERROR) << "UpdateOsSkipped called while UpdateEngine active";
    // Override the rmad state (kWelcome) with the mojo sub-state for OS
    // updates.
    std::move(callback).Run(mojom::RmaState::kUpdateOs, can_abort_,
                            can_go_back_,
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
    return;
  }
  TransitionNextStateGeneric(std::move(callback));
}

void ShimlessRmaService::SetSameOwner(SetSameOwnerCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kDeviceDestination) {
    LOG(ERROR) << "SetSameOwner called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(RmadStateToMojo(state_proto_.state_case()),
                            can_abort_, can_go_back_,
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
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
    std::move(callback).Run(RmadStateToMojo(state_proto_.state_case()),
                            can_abort_, can_go_back_,
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
    return;
  }
  state_proto_.mutable_device_destination()->set_destination(
      rmad::DeviceDestinationState::RMAD_DESTINATION_DIFFERENT);
  TransitionNextStateGeneric(std::move(callback));
}

void ShimlessRmaService::ChooseManuallyDisableWriteProtect(
    ChooseManuallyDisableWriteProtectCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kWpDisableMethod) {
    LOG(ERROR)
        << "ChooseManuallyDisableWriteProtect called from incorrect state "
        << state_proto_.state_case();
    std::move(callback).Run(RmadStateToMojo(state_proto_.state_case()),
                            can_abort_, can_go_back_,
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
    return;
  }
  state_proto_.mutable_wp_disable_method()->set_disable_method(
      rmad::WriteProtectDisableMethodState::RMAD_WP_DISABLE_PHYSICAL);
  TransitionNextStateGeneric(std::move(callback));
}

void ShimlessRmaService::ChooseRsuDisableWriteProtect(
    ChooseRsuDisableWriteProtectCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kWpDisableMethod) {
    LOG(ERROR) << "ChooseRsuDisableWriteProtect called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(RmadStateToMojo(state_proto_.state_case()),
                            can_abort_, can_go_back_,
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
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
    std::move(callback).Run(nullptr);
    return;
  }
  QRCodeGenerator qr_generator;
  absl::optional<QRCodeGenerator::GeneratedCode> qr_data =
      qr_generator.Generate(base::as_bytes(base::make_span(
          state_proto_.wp_disable_rsu().challenge_url().data(),
          state_proto_.wp_disable_rsu().challenge_url().size())));
  if (!qr_data || qr_data->data.data() == nullptr ||
      qr_data->data.size() == 0) {
    std::move(callback).Run(nullptr);
    return;
  }

  // Data returned from QRCodeGenerator consist of bytes that represents
  // tiles. Least significant bit of each byte is set if the tile should be
  // filled. Other bit positions indicate QR Code structure and are not required
  // for rendering. Convert this data to 0 or 1 values for simpler UI side
  // rendering.
  for (uint8_t& qr_data_byte : qr_data->data) {
    qr_data_byte &= 1;
  }

  mojom::QrCodePtr qr_code = mojom::QrCode::New();
  qr_code->size = qr_data->qr_size;
  qr_code->data.assign(qr_data->data.begin(), qr_data->data.end());
  std::move(callback).Run(std::move(qr_code));
}

void ShimlessRmaService::SetRsuDisableWriteProtectCode(
    const std::string& code,
    SetRsuDisableWriteProtectCodeCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kWpDisableRsu) {
    LOG(ERROR) << "SetRsuDisableWriteProtectCode called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(RmadStateToMojo(state_proto_.state_case()),
                            can_abort_, can_go_back_,
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
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
    std::move(callback).Run(RmadStateToMojo(state_proto_.state_case()),
                            can_abort_, can_go_back_,
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
    return;
  }
  TransitionNextStateGeneric(std::move(callback));
}

void ShimlessRmaService::GetWriteProtectDisableCompleteState(
    GetWriteProtectDisableCompleteStateCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kWpDisableComplete) {
    LOG(ERROR) << "ConfirmManualWpDisableComplete called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(mojom::WriteProtectDisableCompleteState::kUnknown);
    return;
  }
  // TODO(gavindodd): Replace this with the rmad Action enum using traits when
  // implemented
  mojom::WriteProtectDisableCompleteState state =
      mojom::WriteProtectDisableCompleteState::kCompleteAssembleDevice;

  if (state_proto_.wp_disable_complete().keep_device_open()) {
    state = mojom::WriteProtectDisableCompleteState::kCompleteKeepDeviceOpen;
  } else if (state_proto_.wp_disable_complete().wp_disable_skipped()) {
    state = mojom::WriteProtectDisableCompleteState::kSkippedAssembleDevice;
  }
  std::move(callback).Run(state);
}

void ShimlessRmaService::ConfirmManualWpDisableComplete(
    ConfirmManualWpDisableCompleteCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kWpDisableComplete) {
    LOG(ERROR) << "ConfirmManualWpDisableComplete called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(RmadStateToMojo(state_proto_.state_case()),
                            can_abort_, can_go_back_,
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
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
    std::move(callback).Run(RmadStateToMojo(state_proto_.state_case()),
                            can_abort_, can_go_back_,
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
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
  }
  TransitionNextStateGeneric(std::move(callback));
}

void ShimlessRmaService::ReworkMainboard(ReworkMainboardCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kComponentsRepair) {
    LOG(ERROR) << "ReworkMainboard called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(RmadStateToMojo(state_proto_.state_case()),
                            can_abort_, can_go_back_,
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
    return;
  }
  state_proto_.mutable_components_repair()->set_mainboard_rework(true);
  TransitionNextStateGeneric(std::move(callback));
}

void ShimlessRmaService::ReimageRequired(ReimageRequiredCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kUpdateRoFirmware) {
    LOG(ERROR) << "ReimageRequired called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(true);
    return;
  }
  std::move(callback).Run(!state_proto_.update_ro_firmware().optional());
}

void ShimlessRmaService::ReimageSkipped(ReimageSkippedCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kUpdateRoFirmware) {
    LOG(ERROR) << "ReimageSkipped called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(RmadStateToMojo(state_proto_.state_case()),
                            can_abort_, can_go_back_,
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
    return;
  }
  // TODO(gavindodd): Is it better to just rely on rmad to enforce this?
  if (!state_proto_.update_ro_firmware().optional()) {
    LOG(ERROR) << "ReimageSkipped called when reimage required.";
    std::move(callback).Run(RmadStateToMojo(state_proto_.state_case()),
                            can_abort_, can_go_back_,
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
    return;
  }
  state_proto_.mutable_update_ro_firmware()->set_update(
      rmad::UpdateRoFirmwareState::RMAD_UPDATE_SKIP);
  TransitionNextStateGeneric(std::move(callback));
}

void ShimlessRmaService::ReimageFromDownload(
    ReimageFromDownloadCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kUpdateRoFirmware) {
    LOG(ERROR) << "ReimageFromDownload called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(RmadStateToMojo(state_proto_.state_case()),
                            can_abort_, can_go_back_,
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
    return;
  }
  state_proto_.mutable_update_ro_firmware()->set_update(
      rmad::UpdateRoFirmwareState::RMAD_UPDATE_FIRMWARE_DOWNLOAD);
  TransitionNextStateGeneric(std::move(callback));
}

void ShimlessRmaService::ReimageFromUsb(ReimageFromUsbCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kUpdateRoFirmware) {
    LOG(ERROR) << "ReimageFromUsb called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(RmadStateToMojo(state_proto_.state_case()),
                            can_abort_, can_go_back_,
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
    return;
  }
  state_proto_.mutable_update_ro_firmware()->set_update(
      rmad::UpdateRoFirmwareState::RMAD_UPDATE_FIRMWARE_RECOVERY_UTILITY);
  TransitionNextStateGeneric(std::move(callback));
}

void ShimlessRmaService::ShutdownForRestock(
    ShutdownForRestockCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kRestock) {
    LOG(ERROR) << "ShutdownForRestock called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(RmadStateToMojo(state_proto_.state_case()),
                            can_abort_, can_go_back_,
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
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
    std::move(callback).Run(RmadStateToMojo(state_proto_.state_case()),
                            can_abort_, can_go_back_,
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
    return;
  }
  state_proto_.mutable_restock()->set_choice(
      rmad::RestockState::RMAD_RESTOCK_CONTINUE_RMA);
  TransitionNextStateGeneric(std::move(callback));
}

void ShimlessRmaService::GetRegionList(GetRegionListCallback callback) {}
void ShimlessRmaService::GetSkuList(GetSkuListCallback callback) {}

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

void ShimlessRmaService::SetDeviceInformation(
    const std::string& serial_number,
    uint8_t region_index,
    uint8_t sku_index,
    SetDeviceInformationCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kUpdateDeviceInfo) {
    LOG(ERROR) << "SetDeviceInformation called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(RmadStateToMojo(state_proto_.state_case()),
                            can_abort_, can_go_back_,
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
    return;
  }
  state_proto_.mutable_update_device_info()->set_serial_number(serial_number);
  state_proto_.mutable_update_device_info()->set_region_index(region_index);
  state_proto_.mutable_update_device_info()->set_sku_index(sku_index);
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
    std::move(callback).Run(RmadStateToMojo(state_proto_.state_case()),
                            can_abort_, can_go_back_,
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
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
    std::move(callback).Run(RmadStateToMojo(state_proto_.state_case()),
                            can_abort_, can_go_back_,
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
    return;
  }
  TransitionNextStateGeneric(std::move(callback));
}

void ShimlessRmaService::ContinueCalibration(
    ContinueCalibrationCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kRunCalibration) {
    LOG(ERROR) << "ContinueCalibration called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(RmadStateToMojo(state_proto_.state_case()),
                            can_abort_, can_go_back_,
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
    return;
  }
  TransitionNextStateGeneric(std::move(callback));
}

void ShimlessRmaService::CalibrationComplete(
    CalibrationCompleteCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kRunCalibration) {
    LOG(ERROR) << "CalibrationComplete called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(RmadStateToMojo(state_proto_.state_case()),
                            can_abort_, can_go_back_,
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
    return;
  }
  TransitionNextStateGeneric(std::move(callback));
}

void ShimlessRmaService::ProvisioningComplete(
    ProvisioningCompleteCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kProvisionDevice) {
    LOG(ERROR) << "ProvisioningComplete called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(RmadStateToMojo(state_proto_.state_case()),
                            can_abort_, can_go_back_,
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
    return;
  }
  TransitionNextStateGeneric(std::move(callback));
}

void ShimlessRmaService::FinalizationComplete(
    FinalizationCompleteCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kFinalize) {
    LOG(ERROR) << "FinalizationComplete called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(RmadStateToMojo(state_proto_.state_case()),
                            can_abort_, can_go_back_,
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
    return;
  }
  TransitionNextStateGeneric(std::move(callback));
}

void ShimlessRmaService::WriteProtectManuallyEnabled(
    WriteProtectManuallyEnabledCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kWpEnablePhysical) {
    LOG(ERROR) << "WriteProtectManuallyEnabled called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(RmadStateToMojo(state_proto_.state_case()),
                            can_abort_, can_go_back_,
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
    return;
  }
  TransitionNextStateGeneric(std::move(callback));
}

void ShimlessRmaService::GetLog(GetLogCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kRepairComplete) {
    LOG(ERROR) << "GetLog called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run("");
    return;
  }
  chromeos::RmadClient::Get()->GetLog(
      base::BindOnce(&ShimlessRmaService::OnGetLog,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ShimlessRmaService::OnGetLog(GetLogCallback callback,
                                  absl::optional<std::string> log) {
  if (!log) {
    std::move(callback).Run("");
    return;
  }
  std::move(callback).Run(*log);
}

void ShimlessRmaService::EndRmaAndReboot(EndRmaAndRebootCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kRepairComplete) {
    LOG(ERROR) << "EndRmaAndReboot called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(RmadStateToMojo(state_proto_.state_case()),
                            can_abort_, can_go_back_,
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
    return;
  }
  state_proto_.mutable_repair_complete()->set_shutdown(
      rmad::RepairCompleteState::RMAD_REPAIR_COMPLETE_REBOOT);
  TransitionNextStateGeneric(std::move(callback));
}

void ShimlessRmaService::EndRmaAndShutdown(EndRmaAndShutdownCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kRepairComplete) {
    LOG(ERROR) << "EndRmaAndShutdown called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(RmadStateToMojo(state_proto_.state_case()),
                            can_abort_, can_go_back_,
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
    return;
  }
  state_proto_.mutable_repair_complete()->set_shutdown(
      rmad::RepairCompleteState::RMAD_REPAIR_COMPLETE_SHUTDOWN);
  TransitionNextStateGeneric(std::move(callback));
}

void ShimlessRmaService::EndRmaAndCutoffBattery(
    EndRmaAndCutoffBatteryCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kRepairComplete) {
    LOG(ERROR) << "EndRmaAndCutoffBattery called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(RmadStateToMojo(state_proto_.state_case()),
                            can_abort_, can_go_back_,
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
    return;
  }
  state_proto_.mutable_repair_complete()->set_shutdown(
      rmad::RepairCompleteState::RMAD_REPAIR_COMPLETE_BATTERY_CUTOFF);
  TransitionNextStateGeneric(std::move(callback));
}

////////////////////////////////
// Observers
void ShimlessRmaService::Error(rmad::RmadErrorCode error) {
  if (error_observer_.is_bound()) {
    error_observer_->OnError(error);
  }
}

void ShimlessRmaService::OsUpdateProgress(update_engine::Operation operation,
                                          double progress) {
  if (os_update_observer_.is_bound()) {
    os_update_observer_->OnOsUpdateProgressUpdated(operation, progress);
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
  last_provisioning_progress_ = status;
  if (provisioning_observer_.is_bound()) {
    provisioning_observer_->OnProvisioningUpdated(status.status(),
                                                  status.progress());
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
  last_finalization_progress_ = status;
  if (finalization_observer_.is_bound()) {
    finalization_observer_->OnFinalizationUpdated(status.status(),
                                                  status.progress());
  }
}

void ShimlessRmaService::ObserveError(
    ::mojo::PendingRemote<mojom::ErrorObserver> observer) {
  error_observer_.Bind(std::move(observer));
}

void ShimlessRmaService::ObserveOsUpdateProgress(
    ::mojo::PendingRemote<mojom::OsUpdateObserver> observer) {
  os_update_observer_.Bind(std::move(observer));
}

void ShimlessRmaService::ObserveCalibrationProgress(
    ::mojo::PendingRemote<mojom::CalibrationObserver> observer) {
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
        last_provisioning_progress_->progress());
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

void ShimlessRmaService::ObserveHardwareVerificationStatus(
    ::mojo::PendingRemote<mojom::HardwareVerificationStatusObserver> observer) {
  hardware_verification_observers_.Add(std::move(observer));
  if (last_hardware_verification_result_) {
    for (auto& observer : hardware_verification_observers_) {
      observer->OnHardwareVerificationResult(
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
        last_finalization_progress_->progress());
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
  chromeos::RmadClient::Get()->TransitionNextState(
      state_proto_,
      base::BindOnce(&ShimlessRmaService::OnGetStateResponse<Callback>,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

template <class Callback>
void ShimlessRmaService::OnGetStateResponse(
    Callback callback,
    absl::optional<rmad::GetStateReply> response) {
  if (!response) {
    LOG(ERROR) << "Failed to call rmadClient";
    // TODO(gavindodd): This needs better handling. Maybe display an error and
    // force a chrome update?
    std::move(callback).Run(mojom::RmaState::kUnknown, false, false,
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
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
    std::move(callback).Run(RmadStateToMojo(state_proto_.state_case()),
                            can_abort_, can_go_back_, response->error());
    return;
  }
  std::move(callback).Run(RmadStateToMojo(state_proto_.state_case()),
                          can_abort_, can_go_back_,
                          rmad::RmadErrorCode::RMAD_ERROR_OK);
}

void ShimlessRmaService::OnAbortRmaResponse(
    AbortRmaCallback callback,
    absl::optional<rmad::AbortRmaReply> response) {
  if (!response) {
    LOG(ERROR) << "Failed to call rmad::AbortRma";
    std::move(callback).Run(rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
    return;
  }
  std::move(callback).Run(response->error());
}

void ShimlessRmaService::OnNetworkListResponse(
    BeginFinalizationCallback callback,
    std::vector<NetworkStatePropertiesPtr> response) {
  for (const NetworkStatePropertiesPtr& network : response) {
    if (network->connection_state == ConnectionStateType::kOnline) {
      switch (network->type) {
        case NetworkType::kWiFi:
        case NetworkType::kEthernet:
          mojo_state_ = mojom::RmaState::kUpdateOs;
          std::move(callback).Run(mojom::RmaState::kUpdateOs, can_abort_,
                                  can_go_back_,
                                  rmad::RmadErrorCode::RMAD_ERROR_OK);
          return;
        case NetworkType::kAll:  // filter-only type
        case NetworkType::kCellular:
        case NetworkType::kMobile:
        case NetworkType::kTether:
        case NetworkType::kVPN:
        case NetworkType::kWireless:  // filter-only type
          continue;
      }
    }
  }
  mojo_state_ = mojom::RmaState::kConfigureNetwork;
  std::move(callback).Run(mojom::RmaState::kConfigureNetwork, can_abort_,
                          can_go_back_, rmad::RmadErrorCode::RMAD_ERROR_OK);
}

void ShimlessRmaService::OnOsUpdateStatusCallback(
    update_engine::Operation operation,
    double progress,
    bool rollback,
    bool powerwash,
    const std::string& version,
    int64_t update_size) {
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
        break;
      // Added to avoid lint error
      case update_engine::Operation::Operation_INT_MIN_SENTINEL_DO_NOT_USE_:
      case update_engine::Operation::Operation_INT_MAX_SENTINEL_DO_NOT_USE_:
        NOTREACHED();
        break;
    }
  }
  // TODO(gavindodd): Pass errors and any other needed data.
  OsUpdateProgress(operation, progress);
}

void ShimlessRmaService::OsUpdateOrNextRmadStateCallback(
    TransitionStateCallback callback,
    const std::string& version) {
  if (version.empty()) {
    TransitionNextStateGeneric(std::move(callback));
  } else {
    mojo_state_ = mojom::RmaState::kUpdateOs;
    std::move(callback).Run(mojom::RmaState::kUpdateOs, can_abort_,
                            can_go_back_, rmad::RmadErrorCode::RMAD_ERROR_OK);
  }
}

}  // namespace shimless_rma
}  // namespace ash
