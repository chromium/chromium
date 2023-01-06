// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/camera_app_ui/camera_app_helper_impl.h"

#include <utility>

#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/public/cpp/window_properties.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/typed_macros.h"
#include "chromeos/utils/pdf_conversion.h"
#include "content/public/browser/web_contents.h"
#include "net/base/url_util.h"
#include "ui/aura/window.h"

namespace ash {
namespace {

using camera_app::mojom::DocumentOutputFormat;
using chromeos::machine_learning::mojom::Rotation;

camera_app::mojom::ScreenState ToMojoScreenState(ScreenBacklightState s) {
  switch (s) {
    case ScreenBacklightState::ON:
      return camera_app::mojom::ScreenState::ON;
    case ScreenBacklightState::OFF:
      return camera_app::mojom::ScreenState::OFF;
    case ScreenBacklightState::OFF_AUTO:
      return camera_app::mojom::ScreenState::OFF_AUTO;
    default:
      NOTREACHED();
  }
}

camera_app::mojom::FileMonitorResult ToMojoFileMonitorResult(
    CameraAppUIDelegate::FileMonitorResult result) {
  switch (result) {
    case CameraAppUIDelegate::FileMonitorResult::DELETED:
      return camera_app::mojom::FileMonitorResult::DELETED;
    case CameraAppUIDelegate::FileMonitorResult::CANCELED:
      return camera_app::mojom::FileMonitorResult::CANCELED;
    case CameraAppUIDelegate::FileMonitorResult::ERROR:
      return camera_app::mojom::FileMonitorResult::ERROR;
    default:
      NOTREACHED();
  }
}

camera_app::mojom::StorageMonitorStatus ToMojoStorageMonitorStatus(
    CameraAppUIDelegate::StorageMonitorStatus status) {
  switch (status) {
    case CameraAppUIDelegate::StorageMonitorStatus::NORMAL:
      return camera_app::mojom::StorageMonitorStatus::NORMAL;
    case CameraAppUIDelegate::StorageMonitorStatus::LOW:
      return camera_app::mojom::StorageMonitorStatus::LOW;
    case CameraAppUIDelegate::StorageMonitorStatus::CRITICALLY_LOW:
      return camera_app::mojom::StorageMonitorStatus::CRITICALLY_LOW;
    case CameraAppUIDelegate::StorageMonitorStatus::CANCELED:
      return camera_app::mojom::StorageMonitorStatus::CANCELED;
    case CameraAppUIDelegate::StorageMonitorStatus::ERROR:
      return camera_app::mojom::StorageMonitorStatus::ERROR;
  }
}

bool HasExternalScreen() {
  for (const auto& display : display::Screen::GetScreen()->GetAllDisplays()) {
    if (!display.IsInternal()) {
      return true;
    }
  }
  return false;
}

absl::optional<uint32_t> ParseIntentIdFromUrl(const GURL& url) {
  std::string id_str;
  if (!net::GetValueForKeyInQuery(url, "intentId", &id_str)) {
    return absl::nullopt;
  }

  uint32_t intent_id;
  if (!base::StringToUint(id_str, &intent_id)) {
    return absl::nullopt;
  }
  return intent_id;
}

bool IsValidCorners(const std::vector<gfx::PointF>& corners) {
  if (corners.size() != 4) {
    return false;
  }
  for (auto& corner : corners) {
    if (corner.x() < 0.f || corner.x() > 1.f || corner.y() < 0.f ||
        corner.y() > 1.f) {
      return false;
    }
  }
  return true;
}

}  // namespace

CameraAppHelperImpl::CameraAppHelperImpl(
    CameraAppUI* camera_app_ui,
    CameraResultCallback camera_result_callback,
    SendBroadcastCallback send_broadcast_callback,
    aura::Window* window)
    : camera_app_ui_(camera_app_ui),
      camera_result_callback_(std::move(camera_result_callback)),
      send_broadcast_callback_(std::move(send_broadcast_callback)),
      has_external_screen_(HasExternalScreen()),
      pending_intent_id_(absl::nullopt),
      window_(window),
      document_scanner_service_(DocumentScannerServiceClient::Create()) {
  DCHECK(camera_app_ui);
  DCHECK(window);
  window->SetProperty(kCanConsumeSystemKeysKey, true);
  TabletMode::Get()->AddObserver(this);
  ScreenBacklight::Get()->AddObserver(this);
}

CameraAppHelperImpl::~CameraAppHelperImpl() {
  TabletMode::Get()->RemoveObserver(this);
  ScreenBacklight::Get()->RemoveObserver(this);

  if (pending_intent_id_.has_value()) {
    camera_result_callback_.Run(*pending_intent_id_,
                                arc::mojom::CameraIntentAction::CANCEL, {},
                                base::DoNothing());
  }
}

void CameraAppHelperImpl::Bind(
    mojo::PendingReceiver<camera_app::mojom::CameraAppHelper> receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
  pending_intent_id_ = ParseIntentIdFromUrl(camera_app_ui_->url());
}

void CameraAppHelperImpl::HandleCameraResult(
    uint32_t intent_id,
    arc::mojom::CameraIntentAction action,
    const std::vector<uint8_t>& data,
    HandleCameraResultCallback callback) {
  if (pending_intent_id_.has_value() && *pending_intent_id_ == intent_id &&
      (action == arc::mojom::CameraIntentAction::FINISH ||
       action == arc::mojom::CameraIntentAction::CANCEL)) {
    pending_intent_id_ = absl::nullopt;
  }
  camera_result_callback_.Run(intent_id, action, data, std::move(callback));
}

void CameraAppHelperImpl::IsTabletMode(IsTabletModeCallback callback) {
  std::move(callback).Run(TabletMode::Get()->InTabletMode());
}

void CameraAppHelperImpl::StartPerfEventTrace(const std::string& event) {
  TRACE_EVENT_BEGIN("camera", nullptr, [&](perfetto::EventContext ctx) {
    ctx.event()->set_name(event);
  });
}

void CameraAppHelperImpl::StopPerfEventTrace(const std::string& event) {
  TRACE_EVENT_END("camera");
}

void CameraAppHelperImpl::SetTabletMonitor(
    mojo::PendingRemote<TabletModeMonitor> monitor,
    SetTabletMonitorCallback callback) {
  tablet_mode_monitor_ = mojo::Remote<TabletModeMonitor>(std::move(monitor));
  std::move(callback).Run(TabletMode::Get()->InTabletMode());
}

void CameraAppHelperImpl::SetScreenStateMonitor(
    mojo::PendingRemote<ScreenStateMonitor> monitor,
    SetScreenStateMonitorCallback callback) {
  screen_state_monitor_ = mojo::Remote<ScreenStateMonitor>(std::move(monitor));
  auto&& mojo_state =
      ToMojoScreenState(ScreenBacklight::Get()->GetScreenBacklightState());
  std::move(callback).Run(mojo_state);
}

void CameraAppHelperImpl::IsMetricsAndCrashReportingEnabled(
    IsMetricsAndCrashReportingEnabledCallback callback) {
  std::move(callback).Run(
      camera_app_ui_->delegate()->IsMetricsAndCrashReportingEnabled());
}

void CameraAppHelperImpl::SetExternalScreenMonitor(
    mojo::PendingRemote<ExternalScreenMonitor> monitor,
    SetExternalScreenMonitorCallback callback) {
  external_screen_monitor_ =
      mojo::Remote<ExternalScreenMonitor>(std::move(monitor));
  std::move(callback).Run(has_external_screen_);
}

void CameraAppHelperImpl::CheckExternalScreenState() {
  if (has_external_screen_ == HasExternalScreen())
    return;
  has_external_screen_ = !has_external_screen_;

  if (external_screen_monitor_.is_bound())
    external_screen_monitor_->Update(has_external_screen_);
}

void CameraAppHelperImpl::OnScannedDocumentCorners(
    ScanDocumentCornersCallback callback,
    bool success,
    const std::vector<gfx::PointF>& corners) {
  if (success) {
    std::move(callback).Run(corners);
  } else {
    LOG(ERROR) << "Failed to scan document corners";
    std::move(callback).Run({});
  }
}

void CameraAppHelperImpl::OnConvertedToDocument(
    DocumentOutputFormat output_format,
    ConvertToDocumentCallback callback,
    bool success,
    const std::vector<uint8_t>& processed_jpeg_image) {
  if (!success) {
    LOG(ERROR) << "Failed to convert to document";
    std::move(callback).Run({});
    return;
  }

  switch (output_format) {
    case DocumentOutputFormat::JPEG:
      std::move(callback).Run(processed_jpeg_image);
      return;
    case DocumentOutputFormat::PDF: {
      std::vector<uint8_t> pdf_data;
      if (!chromeos::ConvertJpgImagesToPdf({processed_jpeg_image}, &pdf_data)) {
        LOG(ERROR) << "Failed to convert jpeg image to PDF format";
        std::move(callback).Run({});
        return;
      }
      std::move(callback).Run(std::move(pdf_data));
      return;
    }
    default:
      NOTREACHED() << "Unsupported output format: " << output_format;
  }
}

void CameraAppHelperImpl::OpenFileInGallery(const std::string& name) {
  camera_app_ui_->delegate()->OpenFileInGallery(name);
}

void CameraAppHelperImpl::OpenFeedbackDialog(const std::string& placeholder) {
  camera_app_ui_->delegate()->OpenFeedbackDialog(placeholder);
}

void CameraAppHelperImpl::OpenUrlInBrowser(const GURL& url) {
  NewWindowDelegate::GetPrimary()->OpenUrl(
      url, NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      NewWindowDelegate::Disposition::kNewForegroundTab);
}

void CameraAppHelperImpl::SetCameraUsageMonitor(
    mojo::PendingRemote<CameraUsageOwnershipMonitor> usage_monitor,
    SetCameraUsageMonitorCallback callback) {
  camera_app_ui_->app_window_manager()->SetCameraUsageMonitor(
      window_, std::move(usage_monitor), std::move(callback));
}

void CameraAppHelperImpl::GetWindowStateController(
    GetWindowStateControllerCallback callback) {
  if (!window_state_controller_) {
    window_state_controller_ = std::make_unique<CameraAppWindowStateController>(
        views::Widget::GetWidgetForNativeWindow(window_));
  }
  mojo::PendingRemote<camera_app::mojom::WindowStateController>
      controller_remote;
  window_state_controller_->AddReceiver(
      controller_remote.InitWithNewPipeAndPassReceiver());
  std::move(callback).Run(std::move(controller_remote));
}

void CameraAppHelperImpl::SendNewCaptureBroadcast(bool is_video,
                                                  const std::string& name) {
  auto file_path = camera_app_ui_->delegate()->GetFilePathInArcByName(name);
  if (file_path.empty()) {
    LOG(ERROR) << "Drop the broadcast request due to invalid file path in ARC "
                  "generated by the file name: "
               << name;
    return;
  }
  send_broadcast_callback_.Run(is_video, file_path);
}

void CameraAppHelperImpl::MonitorFileDeletion(
    const std::string& name,
    MonitorFileDeletionCallback callback) {
  camera_app_ui_->delegate()->MonitorFileDeletion(
      name, base::BindOnce(
                [](MonitorFileDeletionCallback callback,
                   CameraAppUIDelegate::FileMonitorResult result) {
                  std::move(callback).Run(ToMojoFileMonitorResult(result));
                },
                std::move(callback)));
}

void CameraAppHelperImpl::GetDocumentScannerReadyState(
    GetDocumentScannerReadyStateCallback callback) {
  if (document_scanner_service_ == nullptr) {
    std::move(callback).Run(
        camera_app::mojom::DocumentScannerReadyState::NOT_SUPPORTED);
    return;
  }
  if (document_scanner_service_->IsLoaded()) {
    std::move(callback).Run(
        camera_app::mojom::DocumentScannerReadyState::SUPPORTED_AND_READY);
    return;
  }
  std::move(callback).Run(
      camera_app::mojom::DocumentScannerReadyState::SUPPORTED_BUT_NOT_READY);
}

void CameraAppHelperImpl::CheckDocumentModeReadiness(
    CheckDocumentModeReadinessCallback callback) {
  if (document_scanner_service_ == nullptr) {
    std::move(callback).Run(false);
    return;
  }
  document_scanner_service_->CheckDocumentModeReadiness(std::move(callback));
}

void CameraAppHelperImpl::ScanDocumentCorners(
    const std::vector<uint8_t>& jpeg_data,
    ScanDocumentCornersCallback callback) {
  DCHECK(document_scanner_service_);
  base::MappedReadOnlyRegion memory =
      base::ReadOnlySharedMemoryRegion::Create(jpeg_data.size());
  if (!memory.IsValid()) {
    LOG(ERROR) << "Failed to map memory";
    std::move(callback).Run({});
    return;
  }
  memcpy(memory.mapping.memory(), jpeg_data.data(), jpeg_data.size());

  // Since |this| owns |document_scanner_service|, and the callback will be
  // posted to other sequence with weak pointer of |document_scanner_service|.
  // Therefore, it is safe to use |base::Unretained(this)| here.
  document_scanner_service_->DetectCornersFromJPEGImage(
      std::move(memory.region),
      base::BindOnce(&CameraAppHelperImpl::OnScannedDocumentCorners,
                     base::Unretained(this), std::move(callback)));
}

void CameraAppHelperImpl::ConvertToDocument(
    const std::vector<uint8_t>& jpeg_data,
    const std::vector<gfx::PointF>& corners,
    Rotation rotation,
    DocumentOutputFormat output_format,
    ConvertToDocumentCallback callback) {
  DCHECK(document_scanner_service_);
  if (!IsValidCorners(corners)) {
    LOG(ERROR) << "Failed to convert to document due to invalid corners";
    std::move(callback).Run({});
    return;
  }

  base::MappedReadOnlyRegion memory =
      base::ReadOnlySharedMemoryRegion::Create(jpeg_data.size());
  if (!memory.IsValid()) {
    LOG(ERROR) << "Failed to map memory";
    std::move(callback).Run({});
    return;
  }
  memcpy(memory.mapping.memory(), jpeg_data.data(), jpeg_data.size());

  // Since |this| owns |document_scanner_service|, and the callback will be
  // posted to other sequence with weak pointer of |document_scanner_service|.
  // Therefore, it is safe to use |base::Unretained(this)| here.
  document_scanner_service_->DoPostProcessing(
      std::move(memory.region), corners, rotation,
      base::BindOnce(&CameraAppHelperImpl::OnConvertedToDocument,
                     base::Unretained(this), output_format,
                     std::move(callback)));
}

void CameraAppHelperImpl::ConvertToPdf(
    const std::vector<std::vector<uint8_t>>& jpegs_data,
    ConvertToPdfCallback callback) {
  std::vector<uint8_t> pdf_data;
  if (!chromeos::ConvertJpgImagesToPdf(jpegs_data, &pdf_data)) {
    LOG(ERROR) << "Failed to convert jpeg image to PDF format";
    std::move(callback).Run({});
    return;
  }
  std::move(callback).Run(std::move(pdf_data));
}

void CameraAppHelperImpl::MaybeTriggerSurvey() {
  camera_app_ui_->delegate()->MaybeTriggerSurvey();
}

void CameraAppHelperImpl::StartStorageMonitor(
    mojo::PendingRemote<StorageMonitor> monitor,
    StartStorageMonitorCallback callback) {
  // If there is an existing callback from previous call, cancel it first.
  if (storage_monitor_.is_bound()) {
    StopStorageMonitor();
  }

  storage_monitor_ = mojo::Remote<StorageMonitor>(std::move(monitor));
  storage_callback_ = std::move(callback);

  camera_app_ui_->delegate()->StartStorageMonitor(base::BindRepeating(
      &CameraAppHelperImpl::OnStorageStatusUpdated, base::Unretained(this)));
}

void CameraAppHelperImpl::StopStorageMonitor() {
  camera_app_ui_->delegate()->StopStorageMonitor();
  if (!storage_callback_.is_null()) {
    std::move(storage_callback_)
        .Run(camera_app::mojom::StorageMonitorStatus::CANCELED);
  }
  if (storage_monitor_.is_bound()) {
    storage_monitor_.reset();
  }
}

void CameraAppHelperImpl::OnTabletModeStarted() {
  if (tablet_mode_monitor_.is_bound())
    tablet_mode_monitor_->Update(true);
}

void CameraAppHelperImpl::OnTabletModeEnded() {
  if (tablet_mode_monitor_.is_bound())
    tablet_mode_monitor_->Update(false);
}

void CameraAppHelperImpl::OnScreenBacklightStateChanged(
    ScreenBacklightState screen_backlight_state) {
  if (screen_state_monitor_.is_bound())
    screen_state_monitor_->Update(ToMojoScreenState(screen_backlight_state));
}

void CameraAppHelperImpl::OnDisplayAdded(const display::Display& new_display) {
  CheckExternalScreenState();
}

void CameraAppHelperImpl::OnDisplayRemoved(
    const display::Display& old_display) {
  CheckExternalScreenState();
}

void CameraAppHelperImpl::OnStorageStatusUpdated(
    CameraAppUIDelegate::StorageMonitorStatus status) {
  auto mojo_status = ToMojoStorageMonitorStatus(status);
  // Send initial status back, otherwise update through monitor.
  if (!storage_callback_.is_null()) {
    std::move(storage_callback_).Run(mojo_status);
  } else if (storage_monitor_.is_bound()) {
    storage_monitor_->Update(mojo_status);
  }
}

void CameraAppHelperImpl::OpenStorageManagement() {
  camera_app_ui_->delegate()->OpenStorageManagement();
}

}  // namespace ash
