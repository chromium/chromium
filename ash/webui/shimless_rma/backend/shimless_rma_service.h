// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_SHIMLESS_RMA_BACKEND_SHIMLESS_RMA_SERVICE_H_
#define ASH_WEBUI_SHIMLESS_RMA_BACKEND_SHIMLESS_RMA_SERVICE_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ash/webui/shimless_rma/backend/shimless_rma_delegate.h"
#include "ash/webui/shimless_rma/backend/version_updater.h"
#include "ash/webui/shimless_rma/mojom/shimless_rma.mojom.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/dbus/rmad/rmad.pb.h"
#include "chromeos/ash/components/dbus/rmad/rmad_client.h"
#include "chromeos/ash/components/dbus/update_engine/update_engine.pb.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace ash {
namespace shimless_rma {

class ShimlessRmaService : public mojom::ShimlessRmaService,
                           public RmadClient::Observer {
 public:
  ShimlessRmaService(
      std::unique_ptr<ShimlessRmaDelegate> shimless_rma_delegate);
  ShimlessRmaService(const ShimlessRmaService&) = delete;
  ShimlessRmaService& operator=(const ShimlessRmaService&) = delete;

  ~ShimlessRmaService() override;

  // mojom::ShimlessRmaService:
  void GetCurrentState(GetCurrentStateCallback callback) override;
  void TransitionPreviousState(
      TransitionPreviousStateCallback callback) override;
  void AbortRma(AbortRmaCallback callback) override;
  void BeginFinalization(BeginFinalizationCallback callback) override;
  void TrackConfiguredNetworks() override;
  void NetworkSelectionComplete(
      NetworkSelectionCompleteCallback callback) override;
  void GetCurrentOsVersion(GetCurrentOsVersionCallback callback) override;
  void CheckForOsUpdates(CheckForOsUpdatesCallback callback) override;
  void UpdateOs(UpdateOsCallback callback) override;
  void UpdateOsSkipped(UpdateOsSkippedCallback callback) override;
  void SetSameOwner(SetSameOwnerCallback callback) override;
  void SetDifferentOwner(SetDifferentOwnerCallback callback) override;
  void SetWipeDevice(bool wipe_device, SetWipeDeviceCallback) override;
  void SetManuallyDisableWriteProtect(
      SetManuallyDisableWriteProtectCallback callback) override;
  void SetRsuDisableWriteProtect(
      SetRsuDisableWriteProtectCallback callback) override;
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
  void GetCustomLabelList(GetCustomLabelListCallback callback) override;
  void GetSkuDescriptionList(GetSkuDescriptionListCallback callback) override;
  void GetOriginalSerialNumber(
      GetOriginalSerialNumberCallback callback) override;
  void GetOriginalRegion(GetOriginalRegionCallback callback) override;
  void GetOriginalSku(GetOriginalSkuCallback callback) override;
  void GetOriginalCustomLabel(GetOriginalCustomLabelCallback callback) override;
  void GetOriginalDramPartNumber(
      GetOriginalDramPartNumberCallback callback) override;
  void GetOriginalFeatureLevel(
      GetOriginalFeatureLevelCallback callback) override;
  void SetDeviceInformation(const std::string& serial_number,
                            int32_t region_index,
                            int32_t sku_index,
                            int32_t custom_label_index,
                            const std::string& dram_part_number,
                            bool is_chassis_branded,
                            int32_t hw_compliance_version,
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
  void SaveLog(SaveLogCallback callback) override;
  void GetPowerwashRequired(GetPowerwashRequiredCallback callback) override;
  void LaunchDiagnostics() override;
  void EndRma(rmad::RepairCompleteState::ShutdownMethod shutdown_method,
              EndRmaCallback callback) override;
  void CriticalErrorExitToLogin(
      CriticalErrorExitToLoginCallback callback) override;
  void CriticalErrorReboot(CriticalErrorRebootCallback callback) override;
  void ShutDownAfterHardwareError() override;
  void Get3pDiagnosticsProvider(
      Get3pDiagnosticsProviderCallback callback) override;
  void GetInstallable3pDiagnosticsAppPath(
      GetInstallable3pDiagnosticsAppPathCallback callback) override;
  void InstallLastFound3pDiagnosticsApp(
      InstallLastFound3pDiagnosticsAppCallback callback) override;
  void CompleteLast3pDiagnosticsInstallation(
      bool is_approved,
      CompleteLast3pDiagnosticsInstallationCallback callback) override;
  void Show3pDiagnosticsApp(Show3pDiagnosticsAppCallback callback) override;
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
  void ObserveExternalDiskState(
      ::mojo::PendingRemote<mojom::ExternalDiskStateObserver> observer)
      override;
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
  void ExternalDiskState(bool detected) override;
  void HardwareVerificationResult(
      const rmad::HardwareVerificationResult& result) override;
  void FinalizationProgress(const rmad::FinalizeStatus& status) override;
  void RoFirmwareUpdateProgress(rmad::UpdateRoFirmwareStatus status) override;

  void OsUpdateProgress(update_engine::Operation operation,
                        double progress,
                        update_engine::ErrorCode error_code);

  VersionUpdater* GetVersionUpdaterForTesting();

  // Sends a metric to the platform side when the Diagnostics app is launched.
  void SendMetricOnLaunchDiagnostics();

  // Sends a metric to the platform side when an OS update is requested.
  void SendMetricOnUpdateOs();

  // Sets the critical_error_occurred_ variable, so that the tests can call
  // functions that request reboots after an error.
  void SetCriticalErrorOccurredForTest(bool critical_error_occurred);

 private:
  using TransitionStateCallback =
      base::OnceCallback<void(mojom::StateResultPtr)>;

  mojom::StateResultPtr CreateStateResult(mojom::State,
                                          bool can_exit,
                                          bool can_go_back,
                                          rmad::RmadErrorCode);
  mojom::StateResultPtr CreateStateResultForInvalidRequest();

  enum StateResponseCalledFrom {
    kTransitPreviousState = 0,
    kGetCurrentState,
    kTransitNextState,
  };

  template <class Callback>
  void TransitionNextStateGeneric(Callback callback);
  template <class Callback>
  void OnGetStateResponse(Callback callback,
                          StateResponseCalledFrom called_from,
                          std::optional<rmad::GetStateReply> response);
  void OnAbortRmaResponse(AbortRmaCallback callback,
                          bool reboot,
                          std::optional<rmad::AbortRmaReply> response);
  void AbortRmaForgetNetworkResponse(
      AbortRmaCallback callback,
      bool reboot,
      std::optional<rmad::AbortRmaReply> response);
  void EndRmaForgetNetworkResponse(
      rmad::RepairCompleteState::ShutdownMethod shutdown_method,
      EndRmaCallback callback);
  void OnGetLog(GetLogCallback callback,
                std::optional<rmad::GetLogReply> response);
  void OnSaveLog(SaveLogCallback callback,
                 std::optional<rmad::SaveLogReply> response);
  void OnDiagnosticsLogReady(SaveLogCallback callback,
                             const std::string& diagnostics_log_text);

  void OnOsUpdateStatusCallback(update_engine::Operation operation,
                                double progress,
                                bool rollback,
                                bool powerwash,
                                const std::string& version,
                                int64_t update_size,
                                update_engine::ErrorCode error_code);

  void OsUpdateOrNextRmadStateCallback(TransitionStateCallback callback,
                                       const std::string& version);

  // Indicate if user has seen the NetworkPage. It helps to check if user
  // will skip the NetworkPage when clicking back button.
  bool user_has_seen_network_page_ = false;

  // Saves existing configured networks to `existing_saved_network_guids_`.
  void OnTrackConfiguredNetworks(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
          networks);

  // Fetches the list of configured networks on RMA completion/exit.
  void ForgetNewNetworkConnections(base::OnceClosure end_rma_callback);

  // Compares the saved and current list of configured networks and attempts to
  // drop any new network configurations.
  void OnForgetNewNetworkConnections(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
          networks);

  // Confirms if the network was dropped. Invokes `end_rma_callback_` once all
  // the expected networks are dropped.
  void OnForgetNetwork(const std::string& guid, bool success);

  // Handles responses from the platform to diagnostics requests.
  void OnMetricsReply(
      std::optional<rmad::RecordBrowserActionMetricReply> response);

  // Handles the response when the RSU QR code is generated.
  void OnQrCodeGenerated(
      GetRsuDisableWriteProtectChallengeQrCodeCallback callback,
      const std::string& qr_code_image);

  // Handles the response from cros_healthd for 3p diag provider.
  void OnGetSystemInfoFor3pDiag(
      Get3pDiagnosticsProviderCallback callback,
      ash::cros_healthd::mojom::TelemetryInfoPtr telemetry_info);

  // Handles the response from rmad when the 3p diag app is extracted.
  void OnExtractExternalDiagnosticsApp(
      GetInstallable3pDiagnosticsAppPathCallback callback,
      std::optional<rmad::ExtractExternalDiagnosticsAppReply> response);

  // Handles the response from rmad for getting the installed 3p diag app.
  void GetInstalledDiagnosticsApp(
      Show3pDiagnosticsAppCallback callback,
      std::optional<rmad::GetInstalledDiagnosticsAppReply> response);

  // Handles the response when the 3p diag app is loaded for show.
  void On3pDiagnosticsAppLoadForShow(
      Show3pDiagnosticsAppCallback callback,
      base::expected<
          ShimlessRmaDelegate::PrepareDiagnosticsAppBrowserContextResult,
          std::string> result);

  // Handles the response when a new 3p diag app is loaded for a pending
  // installation.
  void On3pDiagnosticsAppLoadForInstallation(
      InstallLastFound3pDiagnosticsAppCallback callback,
      base::expected<
          ShimlessRmaDelegate::PrepareDiagnosticsAppBrowserContextResult,
          std::string> result);

  // Remote for sending requests to the CrosNetworkConfig service.
  mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>
      remote_cros_network_config_;

  // The GUIDs of the saved network configurations prior to starting RMA. Needed
  // to track network connections added during RMA. This only gets created if
  // the user gets to the `kConfigureNetwork` state.
  std::optional<base::flat_set<std::string>> existing_saved_network_guids_;

  // The set of guids for networks to be removed from the device before RMA
  // exit. After each response from ForgetNetwork(), the corresponding guid is
  // removed from the set.
  base::flat_set<std::string> pending_network_guids_to_forget_;

  // The callback invoked once to end RMA when all the expected networks have
  // been forgotten.
  base::OnceClosure end_rma_callback_;

  rmad::RmadState state_proto_;
  bool can_abort_ = false;
  bool can_go_back_ = false;
  // Used to validate mojo only states such as kConfigureNetwork
  mojom::State mojo_state_;

  // These variables are used to save the most recent values of the
  // corresponding variables, in case if the rmad client has already sent them,
  // but the front end observer isn't connected yet. The value will be passed to
  // the front end observer when it connects.
  std::optional<rmad::CalibrationComponentStatus> last_calibration_progress_;
  std::optional<rmad::CalibrationOverallStatus>
      last_calibration_overall_progress_;
  std::optional<rmad::ProvisionStatus> last_provisioning_progress_;
  std::optional<bool> last_hardware_protection_state_;
  std::optional<bool> last_power_cable_state_;
  std::optional<bool> last_external_disk_state_;
  std::optional<rmad::HardwareVerificationResult>
      last_hardware_verification_result_;
  std::optional<rmad::FinalizeStatus> last_finalization_progress_;
  std::optional<rmad::UpdateRoFirmwareStatus> last_update_ro_firmware_progress_;

  mojo::Remote<mojom::ErrorObserver> error_observer_;
  mojo::Remote<mojom::OsUpdateObserver> os_update_observer_;
  mojo::Remote<mojom::CalibrationObserver> calibration_observer_;
  mojo::Remote<mojom::ProvisioningObserver> provisioning_observer_;
  mojo::Remote<mojom::HardwareWriteProtectionStateObserver>
      hwwp_state_observer_;
  mojo::Remote<mojom::PowerCableStateObserver> power_cable_observer_;
  // ExternalDiskStateObserver is used to detect external disks for saving logs
  // and installing firmware.
  mojo::RemoteSet<mojom::ExternalDiskStateObserver>
      external_disk_state_observers_;
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

  // Paths of the last extracted 3p diag app files.
  base::FilePath extracted_3p_diag_swbn_path_;
  base::FilePath extracted_3p_diag_crx_path_;
  // The browser context for showing the 3p diagnostics app.
  raw_ptr<content::BrowserContext> shimless_app_browser_context_ = nullptr;
  // The 3p diagnostics app info.
  std::optional<web_package::SignedWebBundleId> shimless_3p_diag_iwa_id_;
  std::string shimless_3p_diag_app_name_;

  // Task runner for tasks posted by the Shimless service. Used to ensure
  // posted tasks are handled while this service is in scope to stop
  // heap-use-after-free error.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ShimlessRmaService> weak_ptr_factory_{this};
};
}  // namespace shimless_rma
}  // namespace ash

#endif  // ASH_WEBUI_SHIMLESS_RMA_BACKEND_SHIMLESS_RMA_SERVICE_H_
