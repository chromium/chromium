// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_CROSAPI_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_CROSAPI_ASH_H_

#include <map>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/crosapi/crosapi_id.h"
#include "chromeos/crosapi/mojom/cros_display_config.mojom.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/crosapi/mojom/magic_boost.mojom-forward.h"
#include "chromeos/crosapi/mojom/mahi.mojom-forward.h"
#include "chromeos/crosapi/mojom/print_preview_cros.mojom-forward.h"
#include "chromeos/crosapi/mojom/telemetry_diagnostic_routine_service.mojom.h"
#include "media/gpu/buildflags.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "printing/buildflags/buildflags.h"

namespace ash {
class DiagnosticsServiceAsh;
class ProbeServiceAsh;
class TelemetryDiagnosticsRoutineServiceAsh;
class TelemetryEventServiceAsh;
class TelemetryManagementServiceAsh;
class VideoConferenceManagerAsh;

namespace auth {
class InSessionAuth;
}  // namespace auth

namespace printing {
class PrintPreviewWebcontentsAdapterAsh;
}  // namespace printing

}  // namespace ash

namespace crosapi {

class CertProvisioningAsh;
class ChapsServiceAsh;
class DeviceAttributesAsh;
class DeviceOAuth2TokenServiceAsh;
class DocumentScanAsh;
class FileSystemAccessCloudIdentifierProviderAsh;
class FileSystemProviderServiceAsh;
class FullscreenControllerAsh;
class KeystoreServiceAsh;
class KioskSessionServiceAsh;
class LocalPrinterAsh;
class LoginAsh;
class LoginStateAsh;
class MediaUIAsh;
class MultiCaptureServiceAsh;
class NetworkingAttributesAsh;
class ParentAccessAsh;
class StructuredMetricsServiceAsh;
class VpnServiceAsh;

// Implementation of Crosapi in Ash. It provides a set of APIs that
// crosapi clients, such as lacros-chrome, can call into.
class CrosapiAsh : public mojom::Crosapi {
 public:
  CrosapiAsh();
  ~CrosapiAsh() override;

  // Binds the given receiver to this instance.
  // |disconnected_handler| is called on the connection lost.
  void BindReceiver(mojo::PendingReceiver<mojom::Crosapi> pending_receiver,
                    CrosapiId crosapi_id,
                    base::OnceClosure disconnect_handler);

  // crosapi::mojom::Crosapi:
  void BindAccountManager(
      mojo::PendingReceiver<mojom::AccountManager> receiver) override;
  void BindBrowserCdmFactory(mojo::GenericPendingReceiver receiver) override;
  void BindCertProvisioning(
      mojo::PendingReceiver<mojom::CertProvisioning> receiver) override;
  void BindCfmServiceContext(
      mojo::PendingReceiver<chromeos::cfm::mojom::CfmServiceContext> receiver)
      override;
  void BindChapsService(
      mojo::PendingReceiver<mojom::ChapsService> receiver) override;
  void BindCrosDisplayConfigController(
      mojo::PendingReceiver<mojom::CrosDisplayConfigController> receiver)
      override;
  void BindDeviceAttributes(
      mojo::PendingReceiver<mojom::DeviceAttributes> receiver) override;
  void BindDeviceOAuth2TokenService(
      mojo::PendingReceiver<mojom::DeviceOAuth2TokenService> receiver) override;
  void BindDiagnosticsService(
      mojo::PendingReceiver<mojom::DiagnosticsService> receiver) override;
  void BindDocumentScan(
      mojo::PendingReceiver<mojom::DocumentScan> receiver) override;
  void BindFileSystemAccessCloudIdentifierProvider(
      mojo::PendingReceiver<mojom::FileSystemAccessCloudIdentifierProvider>
          receiver) override;
  void BindFullscreenController(
      mojo::PendingReceiver<mojom::FullscreenController> receiver) override;
  void BindHidManager(
      mojo::PendingReceiver<device::mojom::HidManager> receiver) override;
  void BindInSessionAuth(
      mojo::PendingReceiver<chromeos::auth::mojom::InSessionAuth> receiver)
      override;
  void BindKeystoreService(
      mojo::PendingReceiver<mojom::KeystoreService> receiver) override;
  void BindKioskSessionService(
      mojo::PendingReceiver<mojom::KioskSessionService> receiver) override;
  void BindLocalPrinter(
      mojo::PendingReceiver<mojom::LocalPrinter> receiver) override;
  void BindLogin(mojo::PendingReceiver<mojom::Login> receiver) override;
  void BindLoginState(
      mojo::PendingReceiver<mojom::LoginState> receiver) override;
  void BindMachineLearningService(
      mojo::PendingReceiver<
          chromeos::machine_learning::mojom::MachineLearningService> receiver)
      override;
  void BindMediaUI(mojo::PendingReceiver<mojom::MediaUI> receiver) override;
  void BindMediaSessionAudioFocus(
      mojo::PendingReceiver<media_session::mojom::AudioFocusManager> receiver)
      override;
  void BindMediaSessionAudioFocusDebug(
      mojo::PendingReceiver<media_session::mojom::AudioFocusManagerDebug>
          receiver) override;
  void BindMediaSessionController(
      mojo::PendingReceiver<media_session::mojom::MediaControllerManager>
          receiver) override;
  void BindMultiCaptureService(
      mojo::PendingReceiver<mojom::MultiCaptureService> receiver) override;
  void BindNetworkChange(
      mojo::PendingReceiver<mojom::NetworkChange> receiver) override;
  void BindNetworkingAttributes(
      mojo::PendingReceiver<mojom::NetworkingAttributes> receiver) override;
  void BindParentAccess(
      mojo::PendingReceiver<mojom::ParentAccess> receiver) override;
  void BindPrintPreviewCrosDelegate(
      mojo::PendingReceiver<mojom::PrintPreviewCrosDelegate> receiver) override;
  void BindRemoteAppsLacrosBridge(
      mojo::PendingReceiver<
          chromeos::remote_apps::mojom::RemoteAppsLacrosBridge> receiver)
      override;
  void BindSensorHalClient(
      mojo::PendingRemote<chromeos::sensors::mojom::SensorHalClient> remote)
      override;
  void BindStructuredMetricsService(
      ::mojo::PendingReceiver<::crosapi::mojom::StructuredMetricsService>
          receiver) override;
  void BindTelemetryDiagnosticRoutinesService(
      mojo::PendingReceiver<mojom::TelemetryDiagnosticRoutinesService> receiver)
      override;
  void BindTelemetryEventService(
      mojo::PendingReceiver<mojom::TelemetryEventService> receiver) override;
  void BindTelemetryManagementService(
      mojo::PendingReceiver<mojom::TelemetryManagementService> receiver)
      override;
  void BindTelemetryProbeService(
      mojo::PendingReceiver<mojom::TelemetryProbeService> receiver) override;
  void BindVideoCaptureDeviceFactory(
      mojo::PendingReceiver<mojom::VideoCaptureDeviceFactory> receiver)
      override;
  void BindVpnService(
      mojo::PendingReceiver<mojom::VpnService> receiver) override;
  void BindGuestOsSkForwarderFactory(
      mojo::PendingReceiver<mojom::GuestOsSkForwarderFactory> receiver)
      override;

  CertProvisioningAsh* cert_provisioning_ash() {
    return cert_provisioning_ash_.get();
  }

  ChapsServiceAsh* chaps_service_ash() { return chaps_service_ash_.get(); }

  DeviceAttributesAsh* device_attributes_ash() {
    return device_attributes_ash_.get();
  }

  DocumentScanAsh* document_scan_ash() { return document_scan_ash_.get(); }

  FileSystemAccessCloudIdentifierProviderAsh*
  file_system_access_cloud_identifier_provider_ash() {
    return file_system_access_cloud_identifier_provider_ash_.get();
  }

  FileSystemProviderServiceAsh* file_system_provider_service_ash() {
    return file_system_provider_service_ash_.get();
  }

  FullscreenControllerAsh* fullscreen_controller_ash() {
    return fullscreen_controller_ash_.get();
  }

  KeystoreServiceAsh* keystore_service_ash() {
    return keystore_service_ash_.get();
  }

  KioskSessionServiceAsh* kiosk_session_service() {
    return kiosk_session_service_ash_.get();
  }

  LocalPrinterAsh* local_printer_ash() { return local_printer_ash_.get(); }

  LoginAsh* login_ash() { return login_ash_.get(); }

  LoginStateAsh* login_state_ash() { return login_state_ash_.get(); }

  MediaUIAsh* media_ui_ash() { return media_ui_ash_.get(); }

  MultiCaptureServiceAsh* multi_capture_service_ash() {
    return multi_capture_service_ash_.get();
  }

  NetworkingAttributesAsh* networking_attributes_ash() {
    return networking_attributes_ash_.get();
  }

  ParentAccessAsh* parent_access_ash() { return parent_access_ash_.get(); }

  ash::printing::PrintPreviewWebcontentsAdapterAsh*
  print_preview_webcontents_adapter_ash() {
    return print_preview_webcontents_adapter_ash_.get();
  }

  ash::ProbeServiceAsh* probe_service_ash() { return probe_service_ash_.get(); }

  StructuredMetricsServiceAsh* structured_metrics_service_ash() {
    return structured_metrics_service_ash_.get();
  }

  ash::VideoConferenceManagerAsh* video_conference_manager_ash() {
    return video_conference_manager_ash_.get();
  }

  VpnServiceAsh* vpn_service_ash() { return vpn_service_ash_.get(); }

 private:
  // Called when a connection is lost.
  void OnDisconnected();

  std::unique_ptr<CertProvisioningAsh> cert_provisioning_ash_;
  std::unique_ptr<ChapsServiceAsh> chaps_service_ash_;
  std::unique_ptr<DeviceAttributesAsh> device_attributes_ash_;
  std::unique_ptr<DeviceOAuth2TokenServiceAsh> device_oauth2_token_service_ash_;
  std::unique_ptr<ash::DiagnosticsServiceAsh> diagnostics_service_ash_;
  std::unique_ptr<DocumentScanAsh> document_scan_ash_;
  std::unique_ptr<FileSystemAccessCloudIdentifierProviderAsh>
      file_system_access_cloud_identifier_provider_ash_;
  std::unique_ptr<FileSystemProviderServiceAsh>
      file_system_provider_service_ash_;
  std::unique_ptr<FullscreenControllerAsh> fullscreen_controller_ash_;
  std::unique_ptr<KeystoreServiceAsh> keystore_service_ash_;
  std::unique_ptr<KioskSessionServiceAsh> kiosk_session_service_ash_;
  std::unique_ptr<LocalPrinterAsh> local_printer_ash_;
  std::unique_ptr<LoginAsh> login_ash_;
  std::unique_ptr<LoginStateAsh> login_state_ash_;
  std::unique_ptr<MediaUIAsh> media_ui_ash_;
  std::unique_ptr<MultiCaptureServiceAsh> multi_capture_service_ash_;
  std::unique_ptr<NetworkingAttributesAsh> networking_attributes_ash_;
  std::unique_ptr<ParentAccessAsh> parent_access_ash_;
  std::unique_ptr<ash::TelemetryDiagnosticsRoutineServiceAsh>
      telemetry_diagnostic_routine_service_ash_;
  std::unique_ptr<ash::TelemetryEventServiceAsh> telemetry_event_service_ash_;
  std::unique_ptr<ash::TelemetryManagementServiceAsh>
      telemetry_management_service_ash_;
  std::unique_ptr<ash::ProbeServiceAsh> probe_service_ash_;
  std::unique_ptr<ash::printing::PrintPreviewWebcontentsAdapterAsh>
      print_preview_webcontents_adapter_ash_;
  std::unique_ptr<StructuredMetricsServiceAsh> structured_metrics_service_ash_;
  std::unique_ptr<ash::VideoConferenceManagerAsh> video_conference_manager_ash_;
  std::unique_ptr<VpnServiceAsh> vpn_service_ash_;

  mojo::ReceiverSet<mojom::Crosapi, CrosapiId> receiver_set_;
  std::map<mojo::ReceiverId, base::OnceClosure> disconnect_handler_map_;

  base::WeakPtrFactory<CrosapiAsh> weak_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_CROSAPI_ASH_H_
