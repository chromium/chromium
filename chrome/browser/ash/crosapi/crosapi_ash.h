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
class TelemetryManagementServiceAsh;

namespace auth {
class InSessionAuth;
}  // namespace auth

namespace printing {
class PrintPreviewWebcontentsAdapterAsh;
}  // namespace printing

}  // namespace ash

namespace crosapi {

class DocumentScanAsh;
class KeystoreServiceAsh;
class LocalPrinterAsh;
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
  void BindCfmServiceContext(
      mojo::PendingReceiver<chromeos::cfm::mojom::CfmServiceContext> receiver)
      override;
  void BindCrosDisplayConfigController(
      mojo::PendingReceiver<mojom::CrosDisplayConfigController> receiver)
      override;
  void BindDiagnosticsService(
      mojo::PendingReceiver<mojom::DiagnosticsService> receiver) override;
  void BindDocumentScan(
      mojo::PendingReceiver<mojom::DocumentScan> receiver) override;
  void BindHidManager(
      mojo::PendingReceiver<device::mojom::HidManager> receiver) override;
  void BindInSessionAuth(
      mojo::PendingReceiver<chromeos::auth::mojom::InSessionAuth> receiver)
      override;
  void BindKeystoreService(
      mojo::PendingReceiver<mojom::KeystoreService> receiver) override;
  void BindLocalPrinter(
      mojo::PendingReceiver<mojom::LocalPrinter> receiver) override;
  void BindMachineLearningService(
      mojo::PendingReceiver<
          chromeos::machine_learning::mojom::MachineLearningService> receiver)
      override;
  void BindMediaSessionAudioFocus(
      mojo::PendingReceiver<media_session::mojom::AudioFocusManager> receiver)
      override;
  void BindMediaSessionAudioFocusDebug(
      mojo::PendingReceiver<media_session::mojom::AudioFocusManagerDebug>
          receiver) override;
  void BindMediaSessionController(
      mojo::PendingReceiver<media_session::mojom::MediaControllerManager>
          receiver) override;
  void BindNetworkChange(
      mojo::PendingReceiver<mojom::NetworkChange> receiver) override;
  void BindRemoteAppsLacrosBridge(
      mojo::PendingReceiver<
          chromeos::remote_apps::mojom::RemoteAppsLacrosBridge> receiver)
      override;
  void BindSensorHalClient(
      mojo::PendingRemote<chromeos::sensors::mojom::SensorHalClient> remote)
      override;
  void BindTelemetryDiagnosticRoutinesService(
      mojo::PendingReceiver<mojom::TelemetryDiagnosticRoutinesService> receiver)
      override;
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

  DocumentScanAsh* document_scan_ash() { return document_scan_ash_.get(); }

  KeystoreServiceAsh* keystore_service_ash() {
    return keystore_service_ash_.get();
  }

  LocalPrinterAsh* local_printer_ash() { return local_printer_ash_.get(); }

  ash::printing::PrintPreviewWebcontentsAdapterAsh*
  print_preview_webcontents_adapter_ash() {
    return print_preview_webcontents_adapter_ash_.get();
  }

  ash::ProbeServiceAsh* probe_service_ash() { return probe_service_ash_.get(); }

  VpnServiceAsh* vpn_service_ash() { return vpn_service_ash_.get(); }

 private:
  // Called when a connection is lost.
  void OnDisconnected();

  std::unique_ptr<ash::DiagnosticsServiceAsh> diagnostics_service_ash_;
  std::unique_ptr<DocumentScanAsh> document_scan_ash_;
  std::unique_ptr<KeystoreServiceAsh> keystore_service_ash_;
  std::unique_ptr<LocalPrinterAsh> local_printer_ash_;
  std::unique_ptr<ash::TelemetryDiagnosticsRoutineServiceAsh>
      telemetry_diagnostic_routine_service_ash_;
  std::unique_ptr<ash::TelemetryManagementServiceAsh>
      telemetry_management_service_ash_;
  std::unique_ptr<ash::ProbeServiceAsh> probe_service_ash_;
  std::unique_ptr<ash::printing::PrintPreviewWebcontentsAdapterAsh>
      print_preview_webcontents_adapter_ash_;
  std::unique_ptr<VpnServiceAsh> vpn_service_ash_;

  mojo::ReceiverSet<mojom::Crosapi, CrosapiId> receiver_set_;
  std::map<mojo::ReceiverId, base::OnceClosure> disconnect_handler_map_;

  base::WeakPtrFactory<CrosapiAsh> weak_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_CROSAPI_ASH_H_
