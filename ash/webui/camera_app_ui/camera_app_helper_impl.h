// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_CAMERA_APP_UI_CAMERA_APP_HELPER_IMPL_H_
#define ASH_WEBUI_CAMERA_APP_UI_CAMERA_APP_HELPER_IMPL_H_

#include <optional>
#include <vector>

#include "ash/public/cpp/screen_backlight.h"
#include "ash/webui/camera_app_ui/camera_app_events_sender.h"
#include "ash/webui/camera_app_ui/camera_app_helper.mojom.h"
#include "ash/webui/camera_app_ui/camera_app_ui.h"
#include "ash/webui/camera_app_ui/camera_app_window_state_controller.h"
#include "ash/webui/camera_app_ui/document_scanner_service_client.h"
#include "ash/webui/camera_app_ui/pdf_builder.mojom.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/services/machine_learning/public/mojom/document_scanner.mojom.h"
#include "media/capture/video/chromeos/camera_sw_privacy_switch_state_observer.h"
#include "media/capture/video/chromeos/mojom/system_event_monitor.mojom.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/aura/window.h"
#include "ui/display/display_observer.h"
#include "ui/display/screen.h"

namespace display {
enum class TabletState;
}  // namespace display

namespace ash {

class CameraAppHelperImpl : public ScreenBacklightObserver,
                            public SessionManagerClient::Observer,
                            public display::DisplayObserver,
                            public cros::mojom::CrosLidObserver,
                            public camera_app::mojom::CameraAppHelper {
 public:
  using CameraResultCallback =
      base::RepeatingCallback<void(uint32_t,
                                   arc::mojom::CameraIntentAction,
                                   const std::vector<uint8_t>&,
                                   HandleCameraResultCallback)>;
  using SendBroadcastCallback =
      base::RepeatingCallback<void(bool, std::string)>;
  using TabletModeMonitor = camera_app::mojom::TabletModeMonitor;
  using ScreenStateMonitor = camera_app::mojom::ScreenStateMonitor;
  using ExternalScreenMonitor = camera_app::mojom::ExternalScreenMonitor;
  using CameraUsageOwnershipMonitor =
      camera_app::mojom::CameraUsageOwnershipMonitor;
  using StorageMonitor = camera_app::mojom::StorageMonitor;
  using LidStateMonitor = camera_app::mojom::LidStateMonitor;
  using ScreenLockedMonitor = camera_app::mojom::ScreenLockedMonitor;
  using SWPrivacySwitchMonitor = camera_app::mojom::SWPrivacySwitchMonitor;

  CameraAppHelperImpl(CameraAppUI* camera_app_ui,
                      CameraResultCallback camera_result_callback,
                      SendBroadcastCallback send_broadcast_callback,
                      aura::Window* window);

  CameraAppHelperImpl(const CameraAppHelperImpl&) = delete;
  CameraAppHelperImpl& operator=(const CameraAppHelperImpl&) = delete;

  ~CameraAppHelperImpl() override;
  void Bind(mojo::PendingReceiver<camera_app::mojom::CameraAppHelper> receiver);

  // camera_app::mojom::CameraAppHelper implementations.
  void HandleCameraResult(uint32_t intent_id,
                          arc::mojom::CameraIntentAction action,
                          const std::vector<uint8_t>& data,
                          HandleCameraResultCallback callback) override;
  void IsTabletMode(IsTabletModeCallback callback) override;
  void StartPerfEventTrace(const std::string& event) override;
  void StopPerfEventTrace(const std::string& event) override;
  void SetTabletMonitor(mojo::PendingRemote<TabletModeMonitor> monitor,
                        SetTabletMonitorCallback callback) override;
  void SetScreenStateMonitor(mojo::PendingRemote<ScreenStateMonitor> monitor,
                             SetScreenStateMonitorCallback callback) override;
  void IsMetricsAndCrashReportingEnabled(
      IsMetricsAndCrashReportingEnabledCallback callback) override;
  void SetExternalScreenMonitor(
      mojo::PendingRemote<ExternalScreenMonitor> monitor,
      SetExternalScreenMonitorCallback callback) override;
  void OpenFileInGallery(const std::string& name) override;
  void OpenFeedbackDialog(const std::string& placeholder) override;
  void OpenUrlInBrowser(const GURL& url) override;
  void GetWindowStateController(
      GetWindowStateControllerCallback callback) override;
  void SendNewCaptureBroadcast(bool is_video, const std::string& name) override;
  void MonitorFileDeletion(const std::string& name,
                           MonitorFileDeletionCallback callback) override;
  void IsDocumentScannerSupported(
      IsDocumentScannerSupportedCallback callback) override;
  void CheckDocumentModeReadiness(
      CheckDocumentModeReadinessCallback callback) override;
  void ScanDocumentCorners(const std::vector<uint8_t>& jpeg_data,
                           ScanDocumentCornersCallback callback) override;
  void ConvertToDocument(const std::vector<uint8_t>& jpeg_data,
                         const std::vector<gfx::PointF>& corners,
                         chromeos::machine_learning::mojom::Rotation rotation,
                         ConvertToDocumentCallback callback) override;
  void MaybeTriggerSurvey() override;
  void StartStorageMonitor(mojo::PendingRemote<StorageMonitor> monitor,
                           StartStorageMonitorCallback callback) override;
  void StopStorageMonitor() override;
  void OpenStorageManagement() override;
  void OpenWifiDialog(camera_app::mojom::WifiConfigPtr wifi_config) override;
  void SetLidStateMonitor(mojo::PendingRemote<LidStateMonitor> monitor,
                          SetLidStateMonitorCallback callback) override;
  void SetSWPrivacySwitchMonitor(
      mojo::PendingRemote<SWPrivacySwitchMonitor> monitor,
      SetSWPrivacySwitchMonitorCallback callback) override;
  void GetEventsSender(GetEventsSenderCallback callback) override;
  void SetScreenLockedMonitor(mojo::PendingRemote<ScreenLockedMonitor> monitor,
                              SetScreenLockedMonitorCallback callback) override;
  void RenderPdfAsJpeg(const std::vector<uint8_t>& pdf_data,
                       RenderPdfAsJpegCallback callback) override;
  void PerformOcr(mojo_base::BigBuffer jpeg_data,
                  PerformOcrCallback callback) override;
  void PerformOcrInline(const std::vector<uint8_t>& jpeg_data,
                        PerformOcrCallback callback) override;
  void CreatePdfBuilder(
      mojo::PendingReceiver<camera_app::mojom::PdfBuilder> receiver) override;

 private:
  void CheckExternalScreenState();

  void OnScannedDocumentCorners(ScanDocumentCornersCallback callback,
                                bool success,
                                const std::vector<gfx::PointF>& corners);
  void OnConvertedToDocument(
      ConvertToDocumentCallback callback,
      bool success,
      const std::vector<uint8_t>& processed_jpeg_data);

  // callback for storage monitor status update
  void OnStorageStatusUpdated(CameraAppUIDelegate::StorageMonitorStatus status);

  // ScreenBacklightObserver overrides;
  void OnScreenBacklightStateChanged(
      ScreenBacklightState screen_backlight_state) override;

  // ash::SessionManagerClient::Observer overrides;
  void ScreenLockedStateUpdated() override;

  // display::DisplayObserver overrides;
  void OnDisplayAdded(const display::Display& new_display) override;
  void OnDisplaysRemoved(const display::Displays& removed_displays) override;
  void OnDisplayTabletStateChanged(display::TabletState state) override;

  void OnLidStateChanged(cros::mojom::LidState state) override;

  void OnSWPrivacySwitchStateChanged(
      cros::mojom::CameraPrivacySwitchState state);

  // For platform app, we set |camera_app_ui_| to nullptr and should not use
  // it. For SWA, since CameraAppUI owns CameraAppHelperImpl, it is safe to
  // assume that the |camera_app_ui_| is always valid during the whole lifetime
  // of CameraAppHelperImpl.
  raw_ptr<CameraAppUI> camera_app_ui_;

  CameraResultCallback camera_result_callback_;

  SendBroadcastCallback send_broadcast_callback_;

  bool has_external_screen_;

  bool is_sw_privacy_switch_on_ = false;

  std::optional<uint32_t> pending_intent_id_;

  raw_ptr<aura::Window> window_;

  mojo::Remote<TabletModeMonitor> tablet_mode_monitor_;
  mojo::Remote<ScreenStateMonitor> screen_state_monitor_;
  mojo::Remote<ExternalScreenMonitor> external_screen_monitor_;
  mojo::Remote<LidStateMonitor> lid_state_monitor_;
  mojo::Remote<SWPrivacySwitchMonitor> sw_privacy_switch_monitor_;
  SetLidStateMonitorCallback lid_callback_;
  mojo::Remote<StorageMonitor> storage_monitor_;
  StartStorageMonitorCallback storage_callback_;

  std::unique_ptr<CameraAppWindowStateController> window_state_controller_;

  std::unique_ptr<CameraAppEventsSender> events_sender_;

  display::ScopedDisplayObserver display_observer_{this};

  // Client to connect to document detection service.
  std::unique_ptr<DocumentScannerServiceClient> document_scanner_service_;

  std::unique_ptr<media::CrosCameraSWPrivacySwitchStateObserver>
      sw_privacy_switch_state_observer_;

  mojo::Remote<cros::mojom::CrosSystemEventMonitor> monitor_;

  mojo::Receiver<cros::mojom::CrosLidObserver> lid_observer_receiver_{this};

  mojo::Remote<ScreenLockedMonitor> screen_locked_monitor_;

  mojo::Receiver<camera_app::mojom::CameraAppHelper> receiver_{this};

  base::WeakPtrFactory<CameraAppHelperImpl> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_WEBUI_CAMERA_APP_UI_CAMERA_APP_HELPER_IMPL_H_
