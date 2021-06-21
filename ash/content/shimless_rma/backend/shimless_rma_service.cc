// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/content/shimless_rma/backend/shimless_rma_service.h"

#include "ash/content/shimless_rma/mojom/shimless_rma_mojom_traits.h"
#include "base/bind.h"
#include "chromeos/dbus/rmad/rmad_client.h"

namespace ash {
namespace shimless_rma {

namespace {

using StateTraits =
    mojo::EnumTraits<mojom::RmaState, rmad::RmadState::StateCase>;

using ErrorTraits = mojo::EnumTraits<mojom::RmadErrorCode, rmad::RmadErrorCode>;

}  // namespace

// TODO(gavindodd): Declare an observer class and register an instance when
// the mojom interface is created.

ShimlessRmaService::ShimlessRmaService() {}
ShimlessRmaService::~ShimlessRmaService() = default;

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
}

void ShimlessRmaService::GetCurrentChromeVersion(
    GetCurrentChromeVersionCallback callback) {
}

void ShimlessRmaService::CheckForChromeUpdates(
    CheckForChromeUpdatesCallback callback) {
}

void ShimlessRmaService::UpdateChrome(UpdateChromeCallback callback) {
}

void ShimlessRmaService::UpdateChromeSkipped(UpdateChromeCallback callback) {
}

void ShimlessRmaService::SetSameOwner(SetSameOwnerCallback callback) {
}

void ShimlessRmaService::SetDifferentOwner(SetDifferentOwnerCallback callback) {
}

void ShimlessRmaService::ChooseManuallyDisableWriteProtect(
    ChooseManuallyDisableWriteProtectCallback callback) {}

void ShimlessRmaService::ChooseRsuDisableWriteProtect(
    ChooseRsuDisableWriteProtectCallback callback) {
}

void ShimlessRmaService::SetRsuDisableWriteProtectCode(
    const std::string& code,
    SetRsuDisableWriteProtectCodeCallback callback) {
}

void ShimlessRmaService::GetComponentList(GetComponentListCallback callback) {
}

void ShimlessRmaService::SetComponentList(
    std::vector<mojom::ComponentPtr> component_list,
    SetComponentListCallback callback) {
}

void ShimlessRmaService::ReworkMainboard(ReworkMainboardCallback callback) {
}

void ShimlessRmaService::ReimageRequired(ReimageRequiredCallback callback) {
}

void ShimlessRmaService::ReimageSkipped(ReimageSkippedCallback callback) {
}

void ShimlessRmaService::ReimageFromDownload(
    ReimageFromDownloadCallback callback) {
}

void ShimlessRmaService::ReimageFromUsb(ReimageFromUsbCallback callback) {
}

void ShimlessRmaService::GetRegionList(GetRegionListCallback callback) {}
void ShimlessRmaService::GetSkuList(GetSkuListCallback callback) {}

void ShimlessRmaService::GetOriginalSerialNumber(
    GetOriginalSerialNumberCallback callback) {
}

void ShimlessRmaService::GetSerialNumber(GetSerialNumberCallback callback) {
}

void ShimlessRmaService::SetSerialNumber(const std::string& serial_number,
                                         SetSerialNumberCallback callback) {
}

void ShimlessRmaService::GetOriginalRegion(GetOriginalRegionCallback callback) {
}

void ShimlessRmaService::GetRegion(GetRegionCallback callback) {
}

void ShimlessRmaService::SetRegion(int8_t region_index,
                                   SetRegionCallback callback) {
}

void ShimlessRmaService::GetOriginalSku(GetOriginalSkuCallback callback) {
}

void ShimlessRmaService::GetSku(GetSkuCallback callback) {
}

void ShimlessRmaService::SetSku(int8_t sku_index, SetSkuCallback callback) {
}

void ShimlessRmaService::FinalizeAndReboot(CutoffBatteryCallback callback) {
}

void ShimlessRmaService::FinalizeAndShutdown(CutoffBatteryCallback callback) {
}

void ShimlessRmaService::CutoffBattery(CutoffBatteryCallback callback) {
}

// Observers

void ShimlessRmaService::ObserveError(
    ::mojo::PendingRemote<mojom::ErrorObserver> observer) {
}

void ShimlessRmaService::ObserveCalibrationProgress(
    ::mojo::PendingRemote<mojom::CalibrationObserver> observer) {
}

void ShimlessRmaService::ObserveProvisioningProgress(
    ::mojo::PendingRemote<mojom::ProvisioningObserver> observer) {
}

void ShimlessRmaService::ObserveHardwareWriteProtectionState(
    ::mojo::PendingRemote<mojom::HardwareWriteProtectionStateObserver>
        observer) {
}

void ShimlessRmaService::ObservePowerCableState(
    ::mojo::PendingRemote<mojom::PowerCableStateObserver> observer) {
}

void ShimlessRmaService::BindInterface(
    mojo::PendingReceiver<mojom::ShimlessRmaService> pending_receiver) {
  receiver_.Bind(std::move(pending_receiver));
}

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
    std::move(callback).Run(mojom::RmaState::kUnknown,
                            mojom::RmadErrorCode::kRequestInvalid);
    return;
  }
  const mojom::RmaState state =
      StateTraits::ToMojom(response->state().state_case());
  if (!mojom::IsKnownEnumValue(state)) {
    LOG(ERROR) << "rmadClient returned unknown state " << state;
    std::move(callback).Run(mojom::RmaState::kUnknown,
                            mojom::RmadErrorCode::kTransitionFailed);
  }
  state_proto_ = response->state();
  if (response->error() != rmad::RMAD_ERROR_OK) {
    LOG(ERROR) << "rmadClient returned error " << response->error();
    std::move(callback).Run(state, ErrorTraits::ToMojom(response->error()));
    return;
  }
  std::move(callback).Run(state, mojom::RmadErrorCode::kOk);
}

}  // namespace shimless_rma
}  // namespace ash
