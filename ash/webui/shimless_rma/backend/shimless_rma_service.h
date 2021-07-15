// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_SHIMLESS_RMA_BACKEND_SHIMLESS_RMA_SERVICE_H_
#define ASH_WEBUI_SHIMLESS_RMA_BACKEND_SHIMLESS_RMA_SERVICE_H_

#include <string>

#include "ash/webui/shimless_rma/mojom/shimless_rma.mojom.h"
#include "chromeos/dbus/rmad/rmad.pb.h"
#include "chromeos/dbus/rmad/rmad_client.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
namespace shimless_rma {

class ShimlessRmaService : public mojom::ShimlessRmaService,
                           public chromeos::RmadClient::Observer {
 public:
  ShimlessRmaService();
  ShimlessRmaService(const ShimlessRmaService&) = delete;
  ShimlessRmaService& operator=(const ShimlessRmaService&) = delete;

  ~ShimlessRmaService() override;

  void GetCurrentState(GetCurrentStateCallback callback) override;
  void GetNextState(GetNextStateCallback callback) override;
  void GetPrevState(GetPrevStateCallback callback) override;

  void AbortRma(AbortRmaCallback callback) override;

  void CheckForNetworkConnection(
      CheckForNetworkConnectionCallback callback) override;
  void GetCurrentChromeVersion(
      GetCurrentChromeVersionCallback callback) override;
  void CheckForChromeUpdates(CheckForChromeUpdatesCallback callback) override;
  void UpdateChrome(UpdateChromeCallback callback) override;
  void UpdateChromeSkipped(UpdateChromeCallback callback) override;

  void SetSameOwner(SetSameOwnerCallback callback) override;
  void SetDifferentOwner(SetDifferentOwnerCallback callback) override;

  void ChooseManuallyDisableWriteProtect(
      ChooseManuallyDisableWriteProtectCallback callback) override;
  void ChooseRsuDisableWriteProtect(
      ChooseRsuDisableWriteProtectCallback callback) override;
  // TODO(gavindodd): GetRsuDisableChallengeCode()
  void SetRsuDisableWriteProtectCode(
      const std::string& code,
      SetRsuDisableWriteProtectCodeCallback callback) override;

  void GetComponentList(GetComponentListCallback callback) override;
  void SetComponentList(
      const std::vector<::rmad::ComponentRepairState>& component_list,
      SetComponentListCallback callback) override;
  void ReworkMainboard(ReworkMainboardCallback callback) override;

  void ReimageRequired(ReimageRequiredCallback callback) override;
  void ReimageSkipped(ReimageSkippedCallback callback) override;
  void ReimageFromDownload(ReimageFromDownloadCallback callback) override;
  void ReimageFromUsb(ReimageFromUsbCallback callback) override;
  void GetRegionList(GetRegionListCallback callback) override;

  void GetSkuList(GetSkuListCallback callback) override;
  void GetOriginalSerialNumber(
      GetOriginalSerialNumberCallback callback) override;
  void GetOriginalRegion(GetOriginalRegionCallback callback) override;
  void GetOriginalSku(GetOriginalSkuCallback callback) override;
  void SetDeviceInformation(const std::string& serial_number,
                            uint8_t region_index,
                            uint8_t sku_index,
                            SetDeviceInformationCallback callback) override;

  void FinalizeAndReboot(FinalizeAndRebootCallback callback) override;
  void FinalizeAndShutdown(FinalizeAndShutdownCallback callback) override;
  void CutoffBattery(CutoffBatteryCallback callback) override;

  void ObserveError(
      ::mojo::PendingRemote<mojom::ErrorObserver> observer) override;
  void ObserveCalibrationProgress(
      ::mojo::PendingRemote<mojom::CalibrationObserver> observer) override;
  void ObserveProvisioningProgress(
      ::mojo::PendingRemote<mojom::ProvisioningObserver> observer) override;
  void ObserveHardwareWriteProtectionState(
      ::mojo::PendingRemote<mojom::HardwareWriteProtectionStateObserver>
          observer) override;
  void ObservePowerCableState(
      ::mojo::PendingRemote<mojom::PowerCableStateObserver> observer) override;

  void BindInterface(
      mojo::PendingReceiver<mojom::ShimlessRmaService> pending_receiver);

  // RmadClient::Observer interface.
  void Error(rmad::RmadErrorCode error) override;
  void CalibrationProgress(
      rmad::CalibrateComponentsState::CalibrationComponent component,
      double progress) override;
  void ProvisioningProgress(rmad::ProvisionDeviceState::ProvisioningStep step,
                            double progress) override;
  void HardwareWriteProtectionState(bool enabled) override;
  void PowerCableState(bool plugged_in) override;

 private:
  template <class Callback>
  void GetNextStateGeneric(Callback callback);
  template <class Callback>
  void OnGetStateResponse(Callback callback,
                          absl::optional<rmad::GetStateReply> response);
  void OnAbortRmaResponse(AbortRmaCallback callback,
                          absl::optional<rmad::AbortRmaReply> response);

  rmad::RmadState state_proto_;

  mojo::Remote<mojom::ErrorObserver> error_observer_;
  mojo::Remote<mojom::CalibrationObserver> calibration_observer_;
  mojo::Remote<mojom::ProvisioningObserver> provisioning_observer_;
  mojo::Remote<mojom::HardwareWriteProtectionStateObserver>
      hwwp_state_observer_;
  mojo::Remote<mojom::PowerCableStateObserver> power_cable_observer_;
  mojo::Receiver<mojom::ShimlessRmaService> receiver_{this};

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ShimlessRmaService> weak_ptr_factory_{this};
};
}  // namespace shimless_rma
}  // namespace ash

#endif  // ASH_WEBUI_SHIMLESS_RMA_BACKEND_SHIMLESS_RMA_SERVICE_H_
