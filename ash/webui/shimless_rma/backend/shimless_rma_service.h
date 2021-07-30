// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_SHIMLESS_RMA_BACKEND_SHIMLESS_RMA_SERVICE_H_
#define ASH_WEBUI_SHIMLESS_RMA_BACKEND_SHIMLESS_RMA_SERVICE_H_

#include <string>

#include "ash/webui/shimless_rma/mojom/shimless_rma.mojom.h"
#include "chromeos/dbus/rmad/rmad.pb.h"
#include "chromeos/dbus/rmad/rmad_client.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "chromeos/services/network_config/public/mojom/network_types.mojom-forward.h"
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
  void TransitionNextState(TransitionNextStateCallback callback) override;
  void TransitionPreviousState(
      TransitionPreviousStateCallback callback) override;

  void AbortRma(AbortRmaCallback callback) override;

  void BeginFinalization(BeginFinalizationCallback callback) override;

  void NetworkSelectionComplete(
      NetworkSelectionCompleteCallback callback) override;

  void GetCurrentOsVersion(GetCurrentOsVersionCallback callback) override;
  void CheckForOsUpdates(CheckForOsUpdatesCallback callback) override;
  void UpdateOs(UpdateOsCallback callback) override;
  void UpdateOsSkipped(UpdateOsCallback callback) override;

  void SetSameOwner(SetSameOwnerCallback callback) override;
  void SetDifferentOwner(SetDifferentOwnerCallback callback) override;

  void ChooseManuallyDisableWriteProtect(
      ChooseManuallyDisableWriteProtectCallback callback) override;
  void ChooseRsuDisableWriteProtect(
      ChooseRsuDisableWriteProtectCallback callback) override;
  void GetRsuDisableWriteProtectChallenge(
      GetRsuDisableWriteProtectChallengeCallback callback) override;
  void GetRsuDisableWriteProtectChallengeQrCode(
      GetRsuDisableWriteProtectChallengeQrCodeCallback callback) override;
  void SetRsuDisableWriteProtectCode(
      const std::string& code,
      SetRsuDisableWriteProtectCodeCallback callback) override;

  void GetComponentList(GetComponentListCallback callback) override;
  void SetComponentList(
      const std::vector<::rmad::ComponentsRepairState_ComponentRepairStatus>&
          component_list,
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
      rmad::CheckCalibrationState::CalibrationStatus::Component component,
      double progress) override;
  void ProvisioningProgress(rmad::ProvisionDeviceState::ProvisioningStep step,
                            double progress) override;
  void HardwareWriteProtectionState(bool enabled) override;
  void PowerCableState(bool plugged_in) override;

 private:
  template <class Callback>
  void TransitionNextStateGeneric(Callback callback);
  template <class Callback>
  void OnGetStateResponse(Callback callback,
                          absl::optional<rmad::GetStateReply> response);
  void OnAbortRmaResponse(AbortRmaCallback callback,
                          absl::optional<rmad::AbortRmaReply> response);
  void OnNetworkListResponse(
      BeginFinalizationCallback callback,
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
          response);

  rmad::RmadState state_proto_;

  mojo::Remote<mojom::ErrorObserver> error_observer_;
  mojo::Remote<mojom::CalibrationObserver> calibration_observer_;
  mojo::Remote<mojom::ProvisioningObserver> provisioning_observer_;
  mojo::Remote<mojom::HardwareWriteProtectionStateObserver>
      hwwp_state_observer_;
  mojo::Remote<mojom::PowerCableStateObserver> power_cable_observer_;
  mojo::Receiver<mojom::ShimlessRmaService> receiver_{this};

  mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>
      remote_cros_network_config_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ShimlessRmaService> weak_ptr_factory_{this};
};
}  // namespace shimless_rma
}  // namespace ash

#endif  // ASH_WEBUI_SHIMLESS_RMA_BACKEND_SHIMLESS_RMA_SERVICE_H_
