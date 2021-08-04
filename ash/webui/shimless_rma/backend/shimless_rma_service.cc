// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/shimless_rma/backend/shimless_rma_service.h"

#include "ash/public/cpp/network_config_service.h"
#include "ash/webui/shimless_rma/mojom/shimless_rma.mojom.h"
#include "ash/webui/shimless_rma/mojom/shimless_rma_mojom_traits.h"
#include "base/bind.h"
#include "chromeos/dbus/rmad/rmad.pb.h"
#include "chromeos/dbus/rmad/rmad_client.h"
#include "chromeos/dbus/util/version_loader.h"
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

class RmadObserver : chromeos::RmadClient::Observer {
 public:
  void Error(rmad::RmadErrorCode error) override {}

  // Called when calibration progress is updated.
  void CalibrationProgress(
      rmad::CheckCalibrationState::CalibrationStatus::Component component,
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
  GetNetworkConfigService(
      remote_cros_network_config_.BindNewPipeAndPassReceiver());
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
void ShimlessRmaService::TransitionNextState(
    TransitionNextStateCallback callback) {
  TransitionNextStateGeneric(std::move(callback));
}

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
  if (state_proto_.state_case() != rmad::RmadState::kWelcome) {
    LOG(ERROR) << "FinalizeRepair called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(RmadStateToMojo(state_proto_.state_case()),
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
    return;
  }
  state_proto_.mutable_welcome()->set_choice(
      rmad::WelcomeState::RMAD_CHOICE_FINALIZE_REPAIR);

  remote_cros_network_config_->GetNetworkStateList(
      NetworkFilter::New(FilterType::kVisible, NetworkType::kAll,
                         /*limit=*/0),
      base::BindOnce(&ShimlessRmaService::OnNetworkListResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ShimlessRmaService::NetworkSelectionComplete(
    NetworkSelectionCompleteCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kWelcome) {
    LOG(ERROR) << "NetworkSelectionComplete called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(RmadStateToMojo(state_proto_.state_case()),
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
    return;
  }
  std::move(callback).Run(mojom::RmaState::kUpdateOs,
                          rmad::RmadErrorCode::RMAD_ERROR_OK);
}

void ShimlessRmaService::GetCurrentOsVersion(
    GetCurrentOsVersionCallback callback) {
  // TODO(gavindodd): Decide whether to use full or short Chrome version.
  std::move(callback).Run(chromeos::version_loader::GetVersion(
      chromeos::version_loader::VERSION_FULL));
}

void ShimlessRmaService::CheckForOsUpdates(CheckForOsUpdatesCallback callback) {
  // TODO(gavindodd): Get actual Chrome update available value.
  std::move(callback).Run(false);
}

void ShimlessRmaService::UpdateOs(UpdateOsCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kWelcome) {
    LOG(ERROR) << "UpdateOs called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(RmadStateToMojo(state_proto_.state_case()),
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
    return;
  }
  // TODO(gavindodd): Trigger a chrome update.
  TransitionNextStateGeneric(std::move(callback));
}

void ShimlessRmaService::UpdateOsSkipped(UpdateOsCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kWelcome) {
    LOG(ERROR) << "UpdateOs called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(RmadStateToMojo(state_proto_.state_case()),
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

void ShimlessRmaService::GetRsuDisableWriteProtectChallengeQrCode(
    GetRsuDisableWriteProtectChallengeQrCodeCallback callback) {
  // TODO(gavindodd): Get URL from proto when available.
  std::string challenge_url_string = "https://www.google.com";
  QRCodeGenerator qr_generator;
  absl::optional<QRCodeGenerator::GeneratedCode> qr_data =
      qr_generator.Generate(base::as_bytes(base::make_span(
          challenge_url_string.data(), challenge_url_string.size())));
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
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
    return;
  }
  state_proto_.mutable_wp_disable_rsu()->set_unlock_code(code);
  TransitionNextStateGeneric(std::move(callback));
}

void ShimlessRmaService::GetComponentList(GetComponentListCallback callback) {
  std::vector<::rmad::ComponentsRepairState_ComponentRepairStatus> components;
  if (state_proto_.state_case() != rmad::RmadState::kComponentsRepair) {
    LOG(ERROR) << "GetComponentList called from incorrect state "
               << state_proto_.state_case();
  } else {
    components.reserve(
        state_proto_.components_repair().component_repair_size());
    for (auto component : state_proto_.components_repair().component_repair()) {
      int component_id = component.component();
      LOG(ERROR) << "Component: " << component_id;
      components.push_back(component);
    }
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
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
    return;
  }
  state_proto_.mutable_components_repair()->clear_component_repair();
  state_proto_.mutable_components_repair()->mutable_component_repair()->Reserve(
      component_list.size());
  for (auto& component : component_list) {
    rmad::ComponentsRepairState_ComponentRepairStatus* proto_component =
        state_proto_.mutable_components_repair()->add_component_repair();
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
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
    return;
  }
  // TODO(gavindodd): set mainboard_rework flag when new rmad.proto is in
  // third_party
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
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
    return;
  }
  // TODO(gavindodd): Is it better to just rely on rmad to enforce this?
  if (!state_proto_.update_ro_firmware().optional()) {
    LOG(ERROR) << "ReimageSkipped called when reimage required.";
    std::move(callback).Run(RmadStateToMojo(state_proto_.state_case()),
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
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
    return;
  }
  state_proto_.mutable_update_ro_firmware()->set_update(
      rmad::UpdateRoFirmwareState::RMAD_UPDATE_FIRMWARE_RECOVERY_UTILITY);
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
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
    return;
  }
  state_proto_.mutable_update_device_info()->set_serial_number(serial_number);
  state_proto_.mutable_update_device_info()->set_region_index(region_index);
  state_proto_.mutable_update_device_info()->set_sku_index(sku_index);
  TransitionNextStateGeneric(std::move(callback));
}

void ShimlessRmaService::FinalizeAndReboot(FinalizeAndRebootCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kFinalize) {
    LOG(ERROR) << "FinalizeAndReboot called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(RmadStateToMojo(state_proto_.state_case()),
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
    return;
  }
  state_proto_.mutable_finalize()->set_shutdown(
      rmad::FinalizeState::RMAD_FINALIZE_REBOOT);
  TransitionNextStateGeneric(std::move(callback));
}

void ShimlessRmaService::FinalizeAndShutdown(
    FinalizeAndShutdownCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kFinalize) {
    LOG(ERROR) << "FinalizeAndShutdown called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(RmadStateToMojo(state_proto_.state_case()),
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
    return;
  }
  state_proto_.mutable_finalize()->set_shutdown(
      rmad::FinalizeState::RMAD_FINALIZE_SHUTDOWN);
  TransitionNextStateGeneric(std::move(callback));
}

void ShimlessRmaService::CutoffBattery(CutoffBatteryCallback callback) {
  if (state_proto_.state_case() != rmad::RmadState::kFinalize) {
    LOG(ERROR) << "CutoffBattery called from incorrect state "
               << state_proto_.state_case();
    std::move(callback).Run(RmadStateToMojo(state_proto_.state_case()),
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
    return;
  }
  state_proto_.mutable_finalize()->set_shutdown(
      rmad::FinalizeState::RMAD_FINALIZE_BATERY_CUTOFF);
  TransitionNextStateGeneric(std::move(callback));
}

////////////////////////////////
// Observers
void ShimlessRmaService::Error(rmad::RmadErrorCode error) {
  if (error_observer_.is_bound()) {
    error_observer_->OnError(error);
  }
}

void ShimlessRmaService::CalibrationProgress(
    rmad::CheckCalibrationState::CalibrationStatus::Component component,
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
    std::move(callback).Run(mojom::RmaState::kUnknown,
                            rmad::RmadErrorCode::RMAD_ERROR_REQUEST_INVALID);
    return;
  }
  // TODO(gavindodd): When platform and chrome release cycles are decoupled
  // there needs to be a way to detect an unexpected state and switch to update
  // Chrome screen.
  state_proto_ = response->state();
  if (response->error() != rmad::RMAD_ERROR_OK) {
    LOG(ERROR) << "rmadClient returned error " << response->error();
    std::move(callback).Run(RmadStateToMojo(state_proto_.state_case()),
                            response->error());
    return;
  }
  std::move(callback).Run(RmadStateToMojo(state_proto_.state_case()),
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
          std::move(callback).Run(mojom::RmaState::kUpdateOs,
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

  std::move(callback).Run(mojom::RmaState::kConfigureNetwork,
                          rmad::RmadErrorCode::RMAD_ERROR_OK);
}
}  // namespace shimless_rma
}  // namespace ash
