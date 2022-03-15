// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_SHIMLESS_RMA_BACKEND_SHIMLESS_RMA_SERVICE_H_
#define ASH_WEBUI_SHIMLESS_RMA_BACKEND_SHIMLESS_RMA_SERVICE_H_

#include <memory>
#include <string>

#include "ash/webui/shimless_rma/backend/version_updater.h"
#include "ash/webui/shimless_rma/mojom/shimless_rma.mojom.h"
#include "chromeos/dbus/rmad/rmad.pb.h"
#include "chromeos/dbus/rmad/rmad_client.h"
#include "chromeos/dbus/update_engine/update_engine.pb.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
namespace shimless_rma {

class ShimlessRmaDelegate;

class ShimlessRmaService : public mojom::ShimlessRmaService,
                           public chromeos::RmadClient::Observer {
 public:
  ShimlessRmaService(
      std::unique_ptr<ShimlessRmaDelegate> shimless_rma_delegate);
  ShimlessRmaService(const ShimlessRmaService&) = delete;
  ShimlessRmaService& operator=(const ShimlessRmaService&) = delete;

  ~ShimlessRmaService() override;

  void GetCurrentState(GetCurrentStateCallback callback) override;
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

  void SetWipeDevice(bool wipe_device, SetWipeDeviceCallback) override;

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

  void WriteProtectManuallyDisabled(
      WriteProtectManuallyDisabledCallback callback) override;
  void GetWriteProtectManuallyDisabledInstructions(
      GetWriteProtectManuallyDisabledInstructionsCallback callback) override;

  void GetWriteProtectDisableCompleteAction(
      GetWriteProtectDisableCompleteActionCallback callback) override;
  void ConfirmManualWpDisableComplete(
      ConfirmManualWpDisableCompleteCallback callback) override;

  void GetComponentList(GetComponentListCallback callback) override;
  void SetComponentList(
      const std::vector<::rmad::ComponentsRepairState_ComponentRepairStatus>&
          component_list,
      SetComponentListCallback callback) override;
  void ReworkMainboard(ReworkMainboardCallback callback) override;

  void RoFirmwareUpdateComplete(
      RoFirmwareUpdateCompleteCallback callback) override;

  void ShutdownForRestock(ShutdownForRestockCallback callback) override;
  void ContinueFinalizationAfterRestock(
      ContinueFinalizationAfterRestockCallback callback) override;

  void GetRegionList(GetRegionListCallback callback) override;
  void GetSkuList(GetSkuListCallback callback) override;
  void GetWhiteLabelList(GetWhiteLabelListCallback callback) override;
  void GetOriginalSerialNumber(
      GetOriginalSerialNumberCallback callback) override;
  void GetOriginalRegion(GetOriginalRegionCallback callback) override;
  void GetOriginalSku(GetOriginalSkuCallback callback) override;
  void GetOriginalWhiteLabel(GetOriginalWhiteLabelCallback callback) override;
  void GetOriginalDramPartNumber(
      GetOriginalDramPartNumberCallback callback) override;
  void SetDeviceInformation(const std::string& serial_number,
                            int32_t region_index,
                            int32_t sku_index,
                            int32_t white_label_index,
                            const std::string& dram_part_number,
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

  void RetryProvisioning(RetryProvisioningCallback callback) override;
  void ProvisioningComplete(ProvisioningCompleteCallback callback) override;

  void RetryFinalization(RetryFinalizationCallback callback) override;
  void FinalizationComplete(FinalizationCompleteCallback callback) override;

  void WriteProtectManuallyEnabled(
      WriteProtectManuallyEnabledCallback callback) override;

  void GetLog(GetLogCallback callback) override;
  void LaunchDiagnostics() override;
  void EndRmaAndReboot(EndRmaAndRebootCallback callback) override;
  void EndRmaAndShutdown(EndRmaAndShutdownCallback callback) override;
  void EndRmaAndCutoffBattery(EndRmaAndCutoffBatteryCallback callback) override;

  void CriticalErrorExitToLogin(
      CriticalErrorExitToLoginCallback callback) override;
  void CriticalErrorReboot(CriticalErrorRebootCallback callback) override;

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
  void ObserveHardwareVerificationStatus(
      ::mojo::PendingRemote<mojom::HardwareVerificationStatusObserver> observer)
      override;
  void ObserveFinalizationStatus(
      ::mojo::PendingRemote<mojom::FinalizationObserver> observer) override;
  void ObserveRoFirmwareUpdateProgress(
      ::mojo::PendingRemote<mojom::UpdateRoFirmwareObserver> observer) override;

  void BindInterface(
      mojo::PendingReceiver<mojom::ShimlessRmaService> pending_receiver);

  // RmadClient::Observer interface.
  void Error(rmad::RmadErrorCode error) override;
  void CalibrationProgress(
      const rmad::CalibrationComponentStatus& component_status) override;
  void CalibrationOverallProgress(
      rmad::CalibrationOverallStatus status) override;
  void ProvisioningProgress(const rmad::ProvisionStatus& status) override;
  void HardwareWriteProtectionState(bool enabled) override;
  void PowerCableState(bool plugged_in) override;
  void HardwareVerificationResult(
      const rmad::HardwareVerificationResult& result) override;
  void FinalizationProgress(const rmad::FinalizeStatus& status) override;
  void RoFirmwareUpdateProgress(rmad::UpdateRoFirmwareStatus status) override;

  void OsUpdateProgress(update_engine::Operation operation,
                        double progress,
                        update_engine::ErrorCode error_code);

 private:
  using TransitionStateCallback =
      base::OnceCallback<void(mojom::State, bool, bool, rmad::RmadErrorCode)>;

  template <class Callback>
  void TransitionNextStateGeneric(Callback callback);
  template <class Callback>
  void OnGetStateResponse(Callback callback,
                          absl::optional<rmad::GetStateReply> response);
  void OnAbortRmaResponse(AbortRmaCallback callback,
                          bool reboot,
                          absl::optional<rmad::AbortRmaReply> response);
  void OnGetLog(GetLogCallback callback,
                absl::optional<rmad::GetLogReply> response);

  void OnOsUpdateStatusCallback(update_engine::Operation operation,
                                double progress,
                                bool rollback,
                                bool powerwash,
                                const std::string& version,
                                int64_t update_size,
                                update_engine::ErrorCode error_code);

  void OsUpdateOrNextRmadStateCallback(TransitionStateCallback callback,
                                       const std::string& version);

  rmad::RmadState state_proto_;
  bool can_abort_ = false;
  bool can_go_back_ = false;
  // Used to validate mojo only states such as kConfigureNetwork
  mojom::State mojo_state_;

  absl::optional<rmad::CalibrationComponentStatus> last_calibration_progress_;
  absl::optional<rmad::CalibrationOverallStatus>
      last_calibration_overall_progress_;
  absl::optional<rmad::ProvisionStatus> last_provisioning_progress_;
  absl::optional<bool> last_hardware_protection_state_;
  absl::optional<bool> last_power_cable_state_;
  absl::optional<rmad::HardwareVerificationResult>
      last_hardware_verification_result_;
  absl::optional<rmad::FinalizeStatus> last_finalization_progress_;
  absl::optional<rmad::UpdateRoFirmwareStatus>
      last_update_ro_firmware_progress_;

  mojo::Remote<mojom::ErrorObserver> error_observer_;
  mojo::Remote<mojom::OsUpdateObserver> os_update_observer_;
  mojo::Remote<mojom::CalibrationObserver> calibration_observer_;
  mojo::Remote<mojom::ProvisioningObserver> provisioning_observer_;
  mojo::Remote<mojom::HardwareWriteProtectionStateObserver>
      hwwp_state_observer_;
  mojo::Remote<mojom::PowerCableStateObserver> power_cable_observer_;
  // HardwareVerificationStatusObserver is used by landing and OS update pages.
  mojo::RemoteSet<mojom::HardwareVerificationStatusObserver>
      hardware_verification_observers_;
  mojo::Remote<mojom::FinalizationObserver> finalization_observer_;
  mojo::Remote<mojom::UpdateRoFirmwareObserver> update_ro_firmware_observer_;
  mojo::Receiver<mojom::ShimlessRmaService> receiver_{this};

  VersionUpdater version_updater_;
  base::OnceCallback<void(const std::string& version)> check_os_callback_;

  // Provides browser functionality from //chrome to the Shimless RMA UI.
  std::unique_ptr<ShimlessRmaDelegate> shimless_rma_delegate_;

  // When a critical error occurs this is set to true and never clears.
  // It is used to allow abort requests to reboot or exit to login, even if the
  // request fails.
  bool critical_error_occurred_ = false;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ShimlessRmaService> weak_ptr_factory_{this};
};
}  // namespace shimless_rma
}  // namespace ash

#endif  // ASH_WEBUI_SHIMLESS_RMA_BACKEND_SHIMLESS_RMA_SERVICE_H_
