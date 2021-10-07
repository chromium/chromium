// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_SHIMLESS_RMA_BACKEND_SHIMLESS_RMA_SERVICE_H_
#define ASH_WEBUI_SHIMLESS_RMA_BACKEND_SHIMLESS_RMA_SERVICE_H_

#include <string>

#include "ash/webui/shimless_rma/backend/version_updater.h"
#include "ash/webui/shimless_rma/mojom/shimless_rma.mojom.h"
#include "chromeos/dbus/rmad/rmad.pb.h"
#include "chromeos/dbus/rmad/rmad_client.h"
#include "chromeos/dbus/update_engine/update_engine.pb.h"
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
  void UpdateOsSkipped(UpdateOsSkippedCallback callback) override;

  void SetSameOwner(SetSameOwnerCallback callback) override;
  void SetDifferentOwner(SetDifferentOwnerCallback callback) override;

  void ChooseManuallyDisableWriteProtect(
      ChooseManuallyDisableWriteProtectCallback callback) override;
  void ChooseRsuDisableWriteProtect(
      ChooseRsuDisableWriteProtectCallback callback) override;
  void GetRsuDisableWriteProtectChallenge(
      GetRsuDisableWriteProtectChallengeCallback callback) override;
  void GetRsuDisableWriteProtectHwid(
      GetRsuDisableWriteProtectHwidCallback callback) override;
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

  void ShutdownForRestock(ShutdownForRestockCallback callback) override;
  void ContinueFinalizationAfterRestock(
      ContinueFinalizationAfterRestockCallback callback) override;

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

  void GetCalibrationComponentList(
      GetCalibrationComponentListCallback callback) override;
  void GetCalibrationSetupInstructions(
      GetCalibrationSetupInstructionsCallback callback) override;
  void StartCalibration(
      const std::vector<::rmad::CalibrationComponentStatus>& components,
      StartCalibrationCallback callback) override;
  void RunCalibrationStep(RunCalibrationStepCallback callback) override;
  void ContinueCalibration(ContinueCalibrationCallback callback) override;
  void CalibrationComplete(CalibrationCompleteCallback callback) override;

  void EndRmaAndReboot(EndRmaAndRebootCallback callback) override;
  void EndRmaAndShutdown(EndRmaAndShutdownCallback callback) override;
  void EndRmaAndCutoffBattery(EndRmaAndCutoffBatteryCallback callback) override;

  void ObserveError(
      ::mojo::PendingRemote<mojom::ErrorObserver> observer) override;
  void ObserveOsUpdateProgress(
      ::mojo::PendingRemote<mojom::OsUpdateObserver> observer) override;
  void ObserveCalibrationProgress(
      ::mojo::PendingRemote<mojom::CalibrationObserver> observer) override;
  void ObserveProvisioningProgress(
      ::mojo::PendingRemote<mojom::ProvisioningObserver> observer) override;
  void ObserveHardwareWriteProtectionState(
      ::mojo::PendingRemote<mojom::HardwareWriteProtectionStateObserver>
          observer) override;
  void ObservePowerCableState(
      ::mojo::PendingRemote<mojom::PowerCableStateObserver> observer) override;
  void ObserveFinalizationStatus(
      ::mojo::PendingRemote<mojom::FinalizationObserver> observer) override;

  void BindInterface(
      mojo::PendingReceiver<mojom::ShimlessRmaService> pending_receiver);

  // RmadClient::Observer interface.
  void Error(rmad::RmadErrorCode error) override;
  void CalibrationProgress(
      const rmad::CalibrationComponentStatus& component_status) override;
  void CalibrationOverallProgress(
      rmad::CalibrationOverallStatus status) override;
  void ProvisioningProgress(rmad::ProvisionDeviceState::ProvisioningStep step,
                            double progress) override;
  void HardwareWriteProtectionState(bool enabled) override;
  void PowerCableState(bool plugged_in) override;
  void HardwareVerificationResult(const rmad::HardwareVerificationResult&
                                      hardware_verification_result) override;

  void OsUpdateProgress(update_engine::Operation operation, double progress);

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

  void OnOsUpdateStatusCallback(update_engine::Operation operation,
                                double progress,
                                bool rollback,
                                bool powerwash,
                                const std::string& version,
                                int64_t update_size);

  rmad::RmadState state_proto_;
  bool can_abort_ = false;
  bool can_go_back_ = false;

  mojo::Remote<mojom::ErrorObserver> error_observer_;
  mojo::Remote<mojom::OsUpdateObserver> os_update_observer_;
  mojo::Remote<mojom::CalibrationObserver> calibration_observer_;
  mojo::Remote<mojom::ProvisioningObserver> provisioning_observer_;
  mojo::Remote<mojom::HardwareWriteProtectionStateObserver>
      hwwp_state_observer_;
  mojo::Remote<mojom::PowerCableStateObserver> power_cable_observer_;
  mojo::Remote<mojom::FinalizationObserver> finalization_observer_;
  mojo::Receiver<mojom::ShimlessRmaService> receiver_{this};

  mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>
      remote_cros_network_config_;

  VersionUpdater version_updater_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ShimlessRmaService> weak_ptr_factory_{this};
};
}  // namespace shimless_rma
}  // namespace ash

#endif  // ASH_WEBUI_SHIMLESS_RMA_BACKEND_SHIMLESS_RMA_SERVICE_H_
