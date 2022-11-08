// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_CAMERA_APP_UI_CAMERA_APP_HELPER_IMPL_H_
#define ASH_WEBUI_CAMERA_APP_UI_CAMERA_APP_HELPER_IMPL_H_

#include <vector>

#include "ash/public/cpp/screen_backlight.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/webui/camera_app_ui/camera_app_helper.mojom.h"
#include "ash/webui/camera_app_ui/camera_app_ui.h"
#include "ash/webui/camera_app_ui/camera_app_window_state_controller.h"
#include "ash/webui/camera_app_ui/document_scanner_service_client.h"
#include "chromeos/services/machine_learning/public/mojom/document_scanner.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/aura/window.h"
#include "ui/display/display_observer.h"
#include "ui/display/screen.h"

namespace ash {

class CameraAppHelperImpl : public TabletModeObserver,
                            public ScreenBacklightObserver,
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
  void SetCameraUsageMonitor(
      mojo::PendingRemote<CameraUsageOwnershipMonitor> usage_monitor,
      SetCameraUsageMonitorCallback callback) override;
  void GetWindowStateController(
      GetWindowStateControllerCallback callback) override;
  void SendNewCaptureBroadcast(bool is_video, const std::string& name) override;
  void MonitorFileDeletion(const std::string& name,
                           MonitorFileDeletionCallback callback) override;
  void GetDocumentScannerReadyState(
      GetDocumentScannerReadyStateCallback callback) override;
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

  // TabletModeObserver overrides;
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;

  // ScreenBacklightObserver overrides;
  void OnScreenBacklightStateChanged(
      ScreenBacklightState screen_backlight_state) override;

  // display::DisplayObserver overrides;
  void OnDisplayAdded(const display::Display& new_display) override;
  void OnDisplayRemoved(const display::Display& old_display) override;

  // For platform app, we set |camera_app_ui_| to nullptr and should not use
  // it. For SWA, since CameraAppUI owns CameraAppHelperImpl, it is safe to
  // assume that the |camera_app_ui_| is always valid during the whole lifetime
  // of CameraAppHelperImpl.
  CameraAppUI* camera_app_ui_;

  CameraResultCallback camera_result_callback_;

  SendBroadcastCallback send_broadcast_callback_;

  bool has_external_screen_;

  absl::optional<uint32_t> pending_intent_id_;

  aura::Window* window_;

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
};

}  // namespace ash

#endif  // ASH_WEBUI_CAMERA_APP_UI_CAMERA_APP_HELPER_IMPL_H_
