// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/crosapi_ash.h"

#include <memory>
#include <utility>

#include "ash/public/ash_interfaces.h"
#include "base/dcheck_is_on.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/notimplemented.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/crosapi/cert_provisioning_ash.h"
#include "chrome/browser/ash/crosapi/chaps_service_ash.h"
#include "chrome/browser/ash/crosapi/device_attributes_ash.h"
#include "chrome/browser/ash/crosapi/device_oauth2_token_service_ash.h"
#include "chrome/browser/ash/crosapi/document_scan_ash.h"
#include "chrome/browser/ash/crosapi/file_system_access_cloud_identifier_provider_ash.h"
#include "chrome/browser/ash/crosapi/file_system_provider_service_ash.h"
#include "chrome/browser/ash/crosapi/fullscreen_controller_ash.h"
#include "chrome/browser/ash/crosapi/keystore_service_ash.h"
#include "chrome/browser/ash/crosapi/kiosk_session_service_ash.h"
#include "chrome/browser/ash/crosapi/local_printer_ash.h"
#include "chrome/browser/ash/crosapi/login_ash.h"
#include "chrome/browser/ash/crosapi/login_state_ash.h"
#include "chrome/browser/ash/crosapi/media_ui_ash.h"
#include "chrome/browser/ash/crosapi/multi_capture_service_ash.h"
#include "chrome/browser/ash/crosapi/networking_attributes_ash.h"
#include "chrome/browser/ash/crosapi/parent_access_ash.h"
#include "chrome/browser/ash/crosapi/structured_metrics_service_ash.h"
#include "chrome/browser/ash/crosapi/vpn_service_ash.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/ash/printing/print_preview/print_preview_webcontents_adapter_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/remote_apps/remote_apps_manager_factory.h"
#include "chrome/browser/ash/video_conference/video_conference_manager_ash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chromeos/ash/components/account_manager/account_manager_factory.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/telemetry_extension/diagnostics/diagnostics_service_ash.h"
#include "chromeos/ash/components/telemetry_extension/events/telemetry_event_service_ash.h"
#include "chromeos/ash/components/telemetry_extension/management/telemetry_management_service_ash.h"
#include "chromeos/ash/components/telemetry_extension/routines/telemetry_diagnostic_routine_service_ash.h"
#include "chromeos/ash/components/telemetry_extension/telemetry/probe_service_ash.h"
#include "chromeos/components/cdm_factory_daemon/cdm_factory_daemon_proxy_ash.h"
#include "chromeos/components/in_session_auth/in_process_instances.h"
#include "chromeos/components/in_session_auth/in_session_auth.h"
#include "chromeos/components/sensors/ash/sensor_hal_dispatcher.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/crosapi/mojom/file_change_service_bridge.mojom.h"
#include "chromeos/crosapi/mojom/keystore_service.mojom.h"
#include "chromeos/crosapi/mojom/local_printer.mojom.h"
#include "chromeos/crosapi/mojom/magic_boost.mojom.h"
#include "chromeos/crosapi/mojom/mahi.mojom.h"
#include "chromeos/crosapi/mojom/multi_capture_service.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_diagnostic_routine_service.mojom.h"
#include "chromeos/services/chromebox_for_meetings/public/cpp/service_connection.h"
#include "chromeos/services/chromebox_for_meetings/public/mojom/cfm_service_manager.mojom.h"
#include "chromeos/services/machine_learning/public/cpp/service_connection.h"
#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "components/account_manager_core/chromeos/account_manager_mojo_service.h"
#include "components/trusted_vault/features.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/device_service.h"
#include "content/public/browser/media_session_service.h"
#include "content/public/browser/video_capture_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "printing/buildflags/buildflags.h"
#include "services/video_capture/public/mojom/video_capture_service.mojom.h"

namespace crosapi {
namespace {

// Assumptions:
// 1. TODO(crbug.com/40704278): Multi-Signin / Fast-User-Switching is disabled.
// 2. ash-chrome has 1 and only 1 "regular" `Profile`.
Profile* GetAshProfile() {
#if DCHECK_IS_ON()
  int num_regular_profiles = 0;
  for (const Profile* profile :
       g_browser_process->profile_manager()->GetLoadedProfiles()) {
    if (ash::ProfileHelper::IsUserProfile(profile)) {
      ++num_regular_profiles;
    }
  }
  DCHECK_EQ(1, num_regular_profiles);
#endif  // DCHECK_IS_ON()
  return ash::ProfileHelper::Get()->GetProfileByUser(
      user_manager::UserManager::Get()->GetActiveUser());
}

}  // namespace

CrosapiAsh::CrosapiAsh()
    : cert_provisioning_ash_(std::make_unique<CertProvisioningAsh>()),
      chaps_service_ash_(std::make_unique<ChapsServiceAsh>()),
      device_attributes_ash_(std::make_unique<DeviceAttributesAsh>()),
      device_oauth2_token_service_ash_(
          std::make_unique<DeviceOAuth2TokenServiceAsh>()),
      diagnostics_service_ash_(std::make_unique<ash::DiagnosticsServiceAsh>()),
      document_scan_ash_(std::make_unique<DocumentScanAsh>()),
      file_system_access_cloud_identifier_provider_ash_(
          std::make_unique<FileSystemAccessCloudIdentifierProviderAsh>()),
      file_system_provider_service_ash_(
          std::make_unique<FileSystemProviderServiceAsh>()),
      fullscreen_controller_ash_(std::make_unique<FullscreenControllerAsh>()),
      keystore_service_ash_(std::make_unique<KeystoreServiceAsh>()),
      kiosk_session_service_ash_(std::make_unique<KioskSessionServiceAsh>()),
      local_printer_ash_(std::make_unique<LocalPrinterAsh>()),
      login_ash_(std::make_unique<LoginAsh>()),
      login_state_ash_(std::make_unique<LoginStateAsh>()),
      media_ui_ash_(std::make_unique<MediaUIAsh>()),
      multi_capture_service_ash_(std::make_unique<MultiCaptureServiceAsh>()),
      networking_attributes_ash_(std::make_unique<NetworkingAttributesAsh>()),
      parent_access_ash_(std::make_unique<ParentAccessAsh>()),
      telemetry_diagnostic_routine_service_ash_(
          std::make_unique<ash::TelemetryDiagnosticsRoutineServiceAsh>()),
      telemetry_event_service_ash_(
          std::make_unique<ash::TelemetryEventServiceAsh>()),
      telemetry_management_service_ash_(
          std::make_unique<ash::TelemetryManagementServiceAsh>()),
      probe_service_ash_(std::make_unique<ash::ProbeServiceAsh>()),
      print_preview_webcontents_adapter_ash_(
          std::make_unique<ash::printing::PrintPreviewWebcontentsAdapterAsh>()),
      structured_metrics_service_ash_(
          std::make_unique<StructuredMetricsServiceAsh>()),
      video_conference_manager_ash_(
          std::make_unique<ash::VideoConferenceManagerAsh>()),
      vpn_service_ash_(std::make_unique<VpnServiceAsh>()) {
  receiver_set_.set_disconnect_handler(base::BindRepeating(
      &CrosapiAsh::OnDisconnected, weak_factory_.GetWeakPtr()));
}

CrosapiAsh::~CrosapiAsh() {
  // Invoke all disconnect handlers.
  auto handlers = std::move(disconnect_handler_map_);
  for (auto& entry : handlers) {
    std::move(entry.second).Run();
  }
}

void CrosapiAsh::BindAccountManager(
    mojo::PendingReceiver<mojom::AccountManager> receiver) {
  // Given `GetAshProfile()` assumptions, there is 1 and only 1
  // `AccountManagerMojoService` that can/should be contacted - the one attached
  // to the regular `Profile` in ash-chrome for the active `User`.
  crosapi::AccountManagerMojoService* const account_manager_mojo_service =
      g_browser_process->platform_part()
          ->GetAccountManagerFactory()
          ->GetAccountManagerMojoService(
              /*profile_path=*/GetAshProfile()->GetPath().value());
  account_manager_mojo_service->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindBrowserCdmFactory(mojo::GenericPendingReceiver receiver) {
  if (auto r = receiver.As<chromeos::cdm::mojom::BrowserCdmFactory>()) {
    chromeos::CdmFactoryDaemonProxyAsh::Create(std::move(r));
  }
}

void CrosapiAsh::BindCertProvisioning(
    mojo::PendingReceiver<mojom::CertProvisioning> receiver) {
  cert_provisioning_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindCfmServiceContext(
    mojo::PendingReceiver<chromeos::cfm::mojom::CfmServiceContext> receiver) {
  chromeos::cfm::ServiceConnection::GetInstance()->BindServiceContext(
      std::move(receiver));
}

void CrosapiAsh::BindChapsService(
    mojo::PendingReceiver<mojom::ChapsService> receiver) {
  chaps_service_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindCrosDisplayConfigController(
    mojo::PendingReceiver<mojom::CrosDisplayConfigController> receiver) {
  ash::BindCrosDisplayConfigController(std::move(receiver));
}

void CrosapiAsh::BindDeviceAttributes(
    mojo::PendingReceiver<mojom::DeviceAttributes> receiver) {
  device_attributes_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindDeviceOAuth2TokenService(
    mojo::PendingReceiver<mojom::DeviceOAuth2TokenService> receiver) {
  device_oauth2_token_service_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindDiagnosticsService(
    mojo::PendingReceiver<mojom::DiagnosticsService> receiver) {
  diagnostics_service_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindDocumentScan(
    mojo::PendingReceiver<mojom::DocumentScan> receiver) {
  document_scan_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindFileSystemAccessCloudIdentifierProvider(
    mojo::PendingReceiver<
        crosapi::mojom::FileSystemAccessCloudIdentifierProvider> receiver) {
  file_system_access_cloud_identifier_provider_ash_->BindReceiver(
      std::move(receiver));
}

void CrosapiAsh::BindFullscreenController(
    mojo::PendingReceiver<crosapi::mojom::FullscreenController> receiver) {
  fullscreen_controller_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindHidManager(
    mojo::PendingReceiver<device::mojom::HidManager> receiver) {
  content::GetDeviceService().BindHidManager(std::move(receiver));
}

void CrosapiAsh::BindInSessionAuth(
    mojo::PendingReceiver<chromeos::auth::mojom::InSessionAuth> receiver) {
  chromeos::auth::BindToInSessionAuthService(std::move(receiver));
}

void CrosapiAsh::BindKeystoreService(
    mojo::PendingReceiver<crosapi::mojom::KeystoreService> receiver) {
  keystore_service_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindKioskSessionService(
    mojo::PendingReceiver<mojom::KioskSessionService> receiver) {
  kiosk_session_service_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindLocalPrinter(
    mojo::PendingReceiver<crosapi::mojom::LocalPrinter> receiver) {
  local_printer_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindLogin(
    mojo::PendingReceiver<crosapi::mojom::Login> receiver) {
  login_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindLoginState(
    mojo::PendingReceiver<crosapi::mojom::LoginState> receiver) {
  login_state_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindMachineLearningService(
    mojo::PendingReceiver<
        chromeos::machine_learning::mojom::MachineLearningService> receiver) {
  chromeos::machine_learning::ServiceConnection::GetInstance()
      ->BindMachineLearningService(std::move(receiver));
}

void CrosapiAsh::BindMediaUI(mojo::PendingReceiver<mojom::MediaUI> receiver) {
  media_ui_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindMediaSessionAudioFocus(
    mojo::PendingReceiver<media_session::mojom::AudioFocusManager> receiver) {
  content::GetMediaSessionService().BindAudioFocusManager(std::move(receiver));
}

void CrosapiAsh::BindMediaSessionAudioFocusDebug(
    mojo::PendingReceiver<media_session::mojom::AudioFocusManagerDebug>
        receiver) {
  content::GetMediaSessionService().BindAudioFocusManagerDebug(
      std::move(receiver));
}

void CrosapiAsh::BindMediaSessionController(
    mojo::PendingReceiver<media_session::mojom::MediaControllerManager>
        receiver) {
  content::GetMediaSessionService().BindMediaControllerManager(
      std::move(receiver));
}

void CrosapiAsh::BindMultiCaptureService(
    mojo::PendingReceiver<mojom::MultiCaptureService> receiver) {
  multi_capture_service_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindNetworkChange(
    mojo::PendingReceiver<crosapi::mojom::NetworkChange> receiver) {
  NOTREACHED();
}

void CrosapiAsh::BindNetworkingAttributes(
    mojo::PendingReceiver<mojom::NetworkingAttributes> receiver) {
  networking_attributes_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindParentAccess(
    mojo::PendingReceiver<mojom::ParentAccess> receiver) {
  parent_access_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindPrintPreviewCrosDelegate(
    mojo::PendingReceiver<mojom::PrintPreviewCrosDelegate> receiver) {
  print_preview_webcontents_adapter_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindReceiver(
    mojo::PendingReceiver<mojom::Crosapi> pending_receiver,
    CrosapiId crosapi_id,
    base::OnceClosure disconnect_handler) {
  mojo::ReceiverId id =
      receiver_set_.Add(this, std::move(pending_receiver), crosapi_id);
  if (!disconnect_handler.is_null()) {
    disconnect_handler_map_.emplace(id, std::move(disconnect_handler));
  }
}

void CrosapiAsh::BindRemoteAppsLacrosBridge(
    mojo::PendingReceiver<chromeos::remote_apps::mojom::RemoteAppsLacrosBridge>
        receiver) {
  ash::RemoteAppsManager* remote_apps_manager =
      ash::RemoteAppsManagerFactory::GetForProfile(GetAshProfile());

  // RemoteApps are only available for managed guest sessions.
  if (!remote_apps_manager) {
    return;
  }
  remote_apps_manager->BindLacrosBridgeInterface(std::move(receiver));
}

void CrosapiAsh::BindSensorHalClient(
    mojo::PendingRemote<chromeos::sensors::mojom::SensorHalClient> remote) {
  chromeos::sensors::SensorHalDispatcher::GetInstance()->RegisterClient(
      std::move(remote));
}

void CrosapiAsh::BindStructuredMetricsService(
    mojo::PendingReceiver<crosapi::mojom::StructuredMetricsService> receiver) {
  structured_metrics_service_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindTelemetryDiagnosticRoutinesService(
    mojo::PendingReceiver<mojom::TelemetryDiagnosticRoutinesService> receiver) {
  telemetry_diagnostic_routine_service_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindTelemetryEventService(
    mojo::PendingReceiver<mojom::TelemetryEventService> receiver) {
  telemetry_event_service_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindTelemetryManagementService(
    mojo::PendingReceiver<mojom::TelemetryManagementService> receiver) {
  telemetry_management_service_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindTelemetryProbeService(
    mojo::PendingReceiver<mojom::TelemetryProbeService> receiver) {
  probe_service_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindVideoCaptureDeviceFactory(
    mojo::PendingReceiver<mojom::VideoCaptureDeviceFactory> receiver) {
  content::GetVideoCaptureService().BindVideoCaptureDeviceFactory(
      std::move(receiver));
}

void CrosapiAsh::BindVpnService(
    mojo::PendingReceiver<mojom::VpnService> receiver) {
  vpn_service_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindGuestOsSkForwarderFactory(
    mojo::PendingReceiver<mojom::GuestOsSkForwarderFactory> receiver) {
  NOTREACHED();
}

void CrosapiAsh::OnDisconnected() {
  auto it = disconnect_handler_map_.find(receiver_set_.current_receiver());
  if (it == disconnect_handler_map_.end()) {
    return;
  }

  base::OnceClosure callback = std::move(it->second);
  disconnect_handler_map_.erase(it);
  std::move(callback).Run();
}

}  // namespace crosapi
