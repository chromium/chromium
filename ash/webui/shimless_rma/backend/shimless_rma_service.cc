// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/shimless_rma/backend/shimless_rma_service.h"

#include "ash/webui/shimless_rma/mojom/shimless_rma.mojom.h"
#include "ash/webui/shimless_rma/mojom/shimless_rma_mojom_traits.h"
#include "base/bind.h"
#include "chromeos/dbus/rmad/rmad.pb.h"
#include "chromeos/dbus/rmad/rmad_client.h"

namespace ash {
namespace shimless_rma {

namespace {

class RmadObserver : chromeos::RmadClient::Observer {
 public:
  void Error(rmad::RmadErrorCode error) override {}

  // Called when calibration progress is updated.
  void CalibrationProgress(
      rmad::CalibrateComponentsState::CalibrationComponent component,
      double progress) override {}

  // Called when provisioning progress is updated.
  void ProvisioningProgress(rmad::ProvisionDeviceState::ProvisioningStep step,
                            double progress) override {}

  // Called when hardware write protection state changes.
  void HardwareWriteProtectionState(bool enabled) override {}

  // Called when power cable is plugged in or removed.
  void PowerCableState(bool plugged_in) override {}
};

}  // namespace

ShimlessRmaService::ShimlessRmaService() {
  // TODO(gavindodd): Is there a guarantee that rmad client exists at this time?
  chromeos::RmadClient::Get()->AddObserver(this);
}

ShimlessRmaService::~ShimlessRmaService() {
  // TODO(gavindodd): Is there a guarantee that rmad client exists at this time?
  chromeos::RmadClient::Get()->RemoveObserver(this);
}

void ShimlessRmaService::GetCurrentState(GetCurrentStateCallback callback) {
  chromeos::RmadClient::Get()->GetCurrentState(base::BindOnce(
      &ShimlessRmaService::OnGetStateResponse<GetCurrentStateCallback>,
      weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

// TODO(crbug.com/1218180): For development only. Remove when all state
// specific functions implemented.
void ShimlessRmaService::GetNextState(GetNextStateCallback callback) {
  GetNextStateGeneric(std::move(callback));
}

void ShimlessRmaService::GetPrevState(GetPrevStateCallback callback) {
  chromeos::RmadClient::Get()->TransitionPreviousState(base::BindOnce(
      &ShimlessRmaService::OnGetStateResponse<GetPrevStateCallback>,
      weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ShimlessRmaService::AbortRma(AbortRmaCallback callback) {
  chromeos::RmadClient::Get()->AbortRma(
      base::BindOnce(&ShimlessRmaService::OnAbortRmaResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ShimlessRmaService::CheckForNetworkConnection(
    CheckForNetworkConnectionCallback callback) {
  // TODO(joonbug): actually check for network
  GetNextStateGeneric(std::move(callback));
}

void ShimlessRmaService::GetCurrentChromeVersion(
    GetCurrentChromeVersionCallback callback) {
  // TODO(gavindodd): Get actual Chrome version.
  std::move(callback).Run("Chrome OS 0.0.0.0");
}

void ShimlessRmaService::CheckForChromeUpdates(
    CheckForChromeUpdatesCallback callback) {
  // TODO(gavindodd): Get actual Chrome update available value.
  std::move(callback).Run(false);
}

void ShimlessRmaService::UpdateChrome(UpdateChromeCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kUpdateChrome) {
    LOG(ERROR) << "UpdateChrome called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(state_proto_.state_case(),
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
    return;
  }
  state_proto_.mutable_update_chrome()->set_update(
      rmad::UpdateChromeState::RMAD_UPDATE_STATE_UPDATE);
  GetNextStateGeneric(std::move(callback));
}

void ShimlessRmaService::UpdateChromeSkipped(UpdateChromeCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kUpdateChrome) {
    LOG(ERROR) << "UpdateChrome called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(state_proto_.state_case(),
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
    return;
  }
  state_proto_.mutable_update_chrome()->set_update(
      rmad::UpdateChromeState::RMAD_UPDATE_STATE_SKIP);
  GetNextStateGeneric(std::move(callback));
}

void ShimlessRmaService::SetSameOwner(SetSameOwnerCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kDeviceDestination) {
    LOG(ERROR) << "SetSameOwner called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(state_proto_.state_case(),
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
    return;
  }
  state_proto_.mutable_device_destination()->set_destination(
      rmad::DeviceDestinationState::RMAD_DESTINATION_SAME);
  GetNextStateGeneric(std::move(callback));
}

void ShimlessRmaService::SetDifferentOwner(SetDifferentOwnerCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kDeviceDestination) {
    LOG(ERROR) << "SetDifferentOwner called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(state_proto_.state_case(),
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
    return;
  }
  state_proto_.mutable_device_destination()->set_destination(
      rmad::DeviceDestinationState::RMAD_DESTINATION_DIFFERENT);
  GetNextStateGeneric(std::move(callback));
}

void ShimlessRmaService::ChooseManuallyDisableWriteProtect(
    ChooseManuallyDisableWriteProtectCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kWpDisableMethod) {
    LOG(ERROR)
        << "ChooseManuallyDisableWriteProtect called from incorrect state "
        << state_proto_.state_case();
    std::move(callback).Run(state_proto_.state_case(),
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
    return;
  }
  state_proto_.mutable_wp_disable_method()->set_disable_method(
      rmad::WriteProtectDisableMethodState::RMAD_WP_DISABLE_PHYSICAL);
  GetNextStateGeneric(std::move(callback));
}

void ShimlessRmaService::ChooseRsuDisableWriteProtect(
    ChooseRsuDisableWriteProtectCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kWpDisableMethod) {
    LOG(ERROR) << "ChooseRsuDisableWriteProtect called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(state_proto_.state_case(),
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
    return;
  }
  state_proto_.mutable_wp_disable_method()->set_disable_method(
      rmad::WriteProtectDisableMethodState::RMAD_WP_DISABLE_RSU);
  GetNextStateGeneric(std::move(callback));
}

void ShimlessRmaService::SetRsuDisableWriteProtectCode(
    const std::string& code,
    SetRsuDisableWriteProtectCodeCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kWpDisableRsu) {
    LOG(ERROR) << "SetRsuDisableWriteProtectCode called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(state_proto_.state_case(),
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
    return;
  }
  state_proto_.mutable_wp_disable_rsu()->set_unlock_code(code);
  GetNextStateGeneric(std::move(callback));
}

void ShimlessRmaService::GetComponentList(GetComponentListCallback callback) {
  std::vector<::rmad::ComponentRepairState> components;
  if (state_proto_.state_case() != rmad::RmadState::kComponentsRepair) {
    LOG(ERROR) << "GetComponentList called from incorrect state "
               << state_proto_.state_case();
  } else {
    components.reserve(state_proto_.components_repair().components_size());
    for (auto component : state_proto_.components_repair().components()) {
      components.push_back(component);
    }
  }
  std::move(callback).Run(std::move(components));
}

void ShimlessRmaService::SetComponentList(
    const std::vector<::rmad::ComponentRepairState>& component_list,
    SetComponentListCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kComponentsRepair) {
    LOG(ERROR) << "SetComponentList called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(state_proto_.state_case(),
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
    return;
  }
  state_proto_.mutable_components_repair()->clear_components();
  state_proto_.mutable_components_repair()->mutable_components()->Reserve(
      component_list.size());
  for (auto& component : component_list) {
    rmad::ComponentRepairState* proto_component =
        state_proto_.mutable_components_repair()->add_components();
    proto_component->set_name(component.name());
    proto_component->set_repair_state(component.repair_state());
  }
  GetNextStateGeneric(std::move(callback));
}

void ShimlessRmaService::ReworkMainboard(ReworkMainboardCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kComponentsRepair) {
    LOG(ERROR) << "ReworkMainboard called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(state_proto_.state_case(),
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
    return;
  }
  // Create a new proto so that the full component list is retained while
  // transitioning to the next state.
  // This is not strictly necessary, but as the UX should never display
  // 'mainboard' it reduces the chance of error.
  rmad::RmadState state;
  state.set_allocated_components_repair(new rmad::ComponentsRepairState());
  rmad::ComponentRepairState* component =
      state.mutable_components_repair()->add_components();
  component->set_name(
      rmad::ComponentRepairState::RMAD_COMPONENT_MAINBOARD_REWORK);
  component->set_repair_state(rmad::ComponentRepairState::RMAD_REPAIR_REPLACED);

  chromeos::RmadClient::Get()->TransitionNextState(
      state,
      base::BindOnce(
          &ShimlessRmaService::OnGetStateResponse<ReworkMainboardCallback>,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
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
    std::move(callback).Run(state_proto_.state_case(),
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
    return;
  }
  // TODO(gavindodd): Is it better to just rely on rmad to enforce this?
  if (!state_proto_.update_ro_firmware().optional()) {
    LOG(ERROR) << "ReimageSkipped called when reimage required.";
    std::move(callback).Run(state_proto_.state_case(),
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
    return;
  }
  state_proto_.mutable_update_ro_firmware()->set_update(
      rmad::UpdateRoFirmwareState::RMAD_UPDATE_SKIP);
  GetNextStateGeneric(std::move(callback));
}

void ShimlessRmaService::ReimageFromDownload(
    ReimageFromDownloadCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kUpdateRoFirmware) {
    LOG(ERROR) << "ReimageFromDownload called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(state_proto_.state_case(),
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
    return;
  }
  state_proto_.mutable_update_ro_firmware()->set_update(
      rmad::UpdateRoFirmwareState::RMAD_UPDATE_FIRMWARE_DOWNLOAD);
  GetNextStateGeneric(std::move(callback));
}

void ShimlessRmaService::ReimageFromUsb(ReimageFromUsbCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kUpdateRoFirmware) {
    LOG(ERROR) << "ReimageFromUsb called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(state_proto_.state_case(),
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
    return;
  }
  state_proto_.mutable_update_ro_firmware()->set_update(
      rmad::UpdateRoFirmwareState::RMAD_UPDATE_FIRMWARE_RECOVERY_UTILITY);
  GetNextStateGeneric(std::move(callback));
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
    std::move(callback).Run(state_proto_.state_case(),
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
    return;
  }
  state_proto_.mutable_update_device_info()->set_serial_number(serial_number);
  state_proto_.mutable_update_device_info()->set_region_index(region_index);
  state_proto_.mutable_update_device_info()->set_sku_index(sku_index);
  GetNextStateGeneric(std::move(callback));
}

void ShimlessRmaService::FinalizeAndReboot(FinalizeAndRebootCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kFinalize) {
    LOG(ERROR) << "FinalizeAndReboot called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(state_proto_.state_case(),
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
    return;
  }
  state_proto_.mutable_finalize()->set_shutdown(
      rmad::FinalizeState::RMAD_FINALIZE_REBOOT);
  GetNextStateGeneric(std::move(callback));
}

void ShimlessRmaService::FinalizeAndShutdown(
    FinalizeAndShutdownCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kFinalize) {
    LOG(ERROR) << "FinalizeAndShutdown called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(state_proto_.state_case(),
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
    return;
  }
  state_proto_.mutable_finalize()->set_shutdown(
      rmad::FinalizeState::RMAD_FINALIZE_SHUTDOWN);
  GetNextStateGeneric(std::move(callback));
}

void ShimlessRmaService::CutoffBattery(CutoffBatteryCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kFinalize) {
    LOG(ERROR) << "CutoffBattery called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(state_proto_.state_case(),
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
    return;
  }
  state_proto_.mutable_finalize()->set_shutdown(
      rmad::FinalizeState::RMAD_FINALIZE_BATERY_CUTOFF);
  GetNextStateGeneric(std::move(callback));
}

////////////////////////////////
// Observers
void ShimlessRmaService::Error(rmad::RmadErrorCode error) {
  if (error_observer_.is_bound()) {
    error_observer_->OnError(error);
  }
}

void ShimlessRmaService::CalibrationProgress(
    rmad::CalibrateComponentsState::CalibrationComponent component,
    double progress) {
  if (calibration_observer_.is_bound()) {
    calibration_observer_->OnCalibrationUpdated(component, progress);
  }
}

void ShimlessRmaService::ProvisioningProgress(
    rmad::ProvisionDeviceState::ProvisioningStep step,
    double progress) {
  if (provisioning_observer_.is_bound()) {
    provisioning_observer_->OnProvisioningUpdated(step, progress);
  }
}

void ShimlessRmaService::HardwareWriteProtectionState(bool enabled) {
  if (hwwp_state_observer_.is_bound()) {
    hwwp_state_observer_->OnHardwareWriteProtectionStateChanged(enabled);
  }
}

void ShimlessRmaService::PowerCableState(bool plugged_in) {
  if (power_cable_observer_.is_bound()) {
    power_cable_observer_->OnPowerCableStateChanged(plugged_in);
  }
}

void ShimlessRmaService::ObserveError(
    ::mojo::PendingRemote<mojom::ErrorObserver> observer) {
  error_observer_.Bind(std::move(observer));
}

void ShimlessRmaService::ObserveCalibrationProgress(
    ::mojo::PendingRemote<mojom::CalibrationObserver> observer) {
  calibration_observer_.Bind(std::move(observer));
}

void ShimlessRmaService::ObserveProvisioningProgress(
    ::mojo::PendingRemote<mojom::ProvisioningObserver> observer) {
  provisioning_observer_.Bind(std::move(observer));
}

void ShimlessRmaService::ObserveHardwareWriteProtectionState(
    ::mojo::PendingRemote<mojom::HardwareWriteProtectionStateObserver>
        observer) {
  hwwp_state_observer_.Bind(std::move(observer));
}

void ShimlessRmaService::ObservePowerCableState(
    ::mojo::PendingRemote<mojom::PowerCableStateObserver> observer) {
  power_cable_observer_.Bind(std::move(observer));
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
void ShimlessRmaService::GetNextStateGeneric(Callback callback) {
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
    std::move(callback).Run(rmad::RmadState::STATE_NOT_SET,
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
    return;
  }
  // TODO(gavindodd): When platform and chrome release cycles are decoupled
  // there needs to be a way to detect an unexpected state and switch to update
  // Chrome screen.
  state_proto_ = response->state();
  if (response->error() != rmad::RMAD_ERROR_OK) {
    LOG(ERROR) << "rmadClient returned error " << response->error();
    std::move(callback).Run(state_proto_.state_case(), response->error());
    return;
  }
  std::move(callback).Run(state_proto_.state_case(),
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

}  // namespace shimless_rma
}  // namespace ash
