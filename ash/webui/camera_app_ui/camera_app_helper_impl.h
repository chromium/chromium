// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_CAMERA_APP_UI_CAMERA_APP_HELPER_IMPL_H_
#define ASH_WEBUI_CAMERA_APP_UI_CAMERA_APP_HELPER_IMPL_H_

#include <optional>
#include <vector>

#include "ash/public/cpp/screen_backlight.h"
#include "ash/webui/camera_app_ui/camera_app_helper.mojom.h"
#include "ash/webui/camera_app_ui/camera_app_ui.h"
#include "ash/webui/camera_app_ui/camera_app_window_state_controller.h"
#include "ash/webui/camera_app_ui/document_scanner_service_client.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/services/machine_learning/public/mojom/document_scanner.mojom.h"
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
                            public display::DisplayObserver,
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

  CameraAppHelperImpl(CameraAppUI* camera_app_ui,
                      CameraResultCallback camera_result_callback,
                      SendBroadcastCallback send_broadcast_callback,
                      aura::Window* window,
                      HoldingSpaceClient* holding_space_client);

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
  void NotifyTote(const camera_app::mojom::ToteMetricFormat format,
                  const std::string& name) override;
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
                         camera_app::mojom::DocumentOutputFormat output_format,
                         ConvertToDocumentCallback callback) override;
  void ConvertToPdf(const std::vector<std::vector<uint8_t>>& jpegs_data,
                    ConvertToPdfCallback callback) override;
  void MaybeTriggerSurvey() override;
  void StartStorageMonitor(mojo::PendingRemote<StorageMonitor> monitor,
                           StartStorageMonitorCallback callback) override;
  void StopStorageMonitor() override;
  void OpenStorageManagement() override;

 private:
  void CheckExternalScreenState();

  void OnScannedDocumentCorners(ScanDocumentCornersCallback callback,
                                bool success,
                                const std::vector<gfx::PointF>& corners);
  void OnConvertedToDocument(
      camera_app::mojom::DocumentOutputFormat output_format,
      ConvertToDocumentCallback callback,
      bool success,
      const std::vector<uint8_t>& processed_jpeg_data);

  // callback for storage monitor status update
  void OnStorageStatusUpdated(CameraAppUIDelegate::StorageMonitorStatus status);

  // ScreenBacklightObserver overrides;
  void OnScreenBacklightStateChanged(
      ScreenBacklightState screen_backlight_state) override;

  // display::DisplayObserver overrides;
  void OnDisplayAdded(const display::Display& new_display) override;
  void OnDisplayRemoved(const display::Display& old_display) override;
  void OnDisplayTabletStateChanged(display::TabletState state) override;

  // For platform app, we set |camera_app_ui_| to nullptr and should not use
  // it. For SWA, since CameraAppUI owns CameraAppHelperImpl, it is safe to
  // assume that the |camera_app_ui_| is always valid during the whole lifetime
  // of CameraAppHelperImpl.
  raw_ptr<CameraAppUI, ExperimentalAsh> camera_app_ui_;

  CameraResultCallback camera_result_callback_;

  SendBroadcastCallback send_broadcast_callback_;

  bool has_external_screen_;

  std::optional<uint32_t> pending_intent_id_;

  raw_ptr<aura::Window, ExperimentalAsh> window_;

  mojo::Remote<TabletModeMonitor> tablet_mode_monitor_;
  mojo::Remote<ScreenStateMonitor> screen_state_monitor_;
  mojo::Remote<ExternalScreenMonitor> external_screen_monitor_;
  mojo::Remote<StorageMonitor> storage_monitor_;
  StartStorageMonitorCallback storage_callback_;

  mojo::Receiver<camera_app::mojom::CameraAppHelper> receiver_{this};

  std::unique_ptr<CameraAppWindowStateController> window_state_controller_;

  display::ScopedDisplayObserver display_observer_{this};

  // Client to connect to document detection service.
  std::unique_ptr<DocumentScannerServiceClient> document_scanner_service_;

  raw_ptr<HoldingSpaceClient> const holding_space_client_;

  base::WeakPtrFactory<CameraAppHelperImpl> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_WEBUI_CAMERA_APP_UI_CAMERA_APP_HELPER_IMPL_H_
