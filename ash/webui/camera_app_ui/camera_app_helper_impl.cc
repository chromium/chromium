// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/camera_app_ui/camera_app_helper_impl.h"

#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/window_properties.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/typed_macros.h"
#include "chromeos/ash/components/mojo_service_manager/connection.h"
#include "components/onc/onc_constants.h"
#include "content/public/browser/web_contents.h"
#include "media/capture/video/chromeos/camera_sw_privacy_switch_state_observer.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "net/base/url_util.h"
#include "third_party/cros_system_api/mojo/service_constants.h"
#include "ui/aura/window.h"
#include "ui/display/screen.h"
#include "ui/display/tablet_state.h"

namespace ash {

namespace {

using chromeos::machine_learning::mojom::Rotation;

camera_app::mojom::ScreenState ToMojoScreenState(ScreenBacklightState s) {
  switch (s) {
    case ScreenBacklightState::ON:
      return camera_app::mojom::ScreenState::kOn;
    case ScreenBacklightState::OFF:
      return camera_app::mojom::ScreenState::kOff;
    case ScreenBacklightState::OFF_AUTO:
      return camera_app::mojom::ScreenState::kOffAuto;
    default:
      NOTREACHED();
  }
}

camera_app::mojom::LidState ToMojoLidState(cros::mojom::LidState state) {
  switch (state) {
    case cros::mojom::LidState::kOpen:
      return camera_app::mojom::LidState::kOpen;
    case cros::mojom::LidState::kClosed:
      return camera_app::mojom::LidState::kClosed;
    case cros::mojom::LidState::kNotPresent:
      return camera_app::mojom::LidState::kNotPresent;
    default:
      NOTREACHED() << "Unexpected Lid type: " << static_cast<int>(state);
  }
}

camera_app::mojom::FileMonitorResult ToMojoFileMonitorResult(
    CameraAppUIDelegate::FileMonitorResult result) {
  switch (result) {
    case CameraAppUIDelegate::FileMonitorResult::kDeleted:
      return camera_app::mojom::FileMonitorResult::kDeleted;
    case CameraAppUIDelegate::FileMonitorResult::kCanceled:
      return camera_app::mojom::FileMonitorResult::kCanceled;
    case CameraAppUIDelegate::FileMonitorResult::kError:
      return camera_app::mojom::FileMonitorResult::kError;
    default:
      NOTREACHED();
  }
}

camera_app::mojom::StorageMonitorStatus ToMojoStorageMonitorStatus(
    CameraAppUIDelegate::StorageMonitorStatus status) {
  switch (status) {
    case CameraAppUIDelegate::StorageMonitorStatus::kNormal:
      return camera_app::mojom::StorageMonitorStatus::kNormal;
    case CameraAppUIDelegate::StorageMonitorStatus::kLow:
      return camera_app::mojom::StorageMonitorStatus::kLow;
    case CameraAppUIDelegate::StorageMonitorStatus::kCriticallyLow:
      return camera_app::mojom::StorageMonitorStatus::kCriticallyLow;
    case CameraAppUIDelegate::StorageMonitorStatus::kCanceled:
      return camera_app::mojom::StorageMonitorStatus::kCanceled;
    case CameraAppUIDelegate::StorageMonitorStatus::kError:
      return camera_app::mojom::StorageMonitorStatus::kError;
  }
}

std::string FromMojoSecurityType(
    camera_app::mojom::WifiSecurityType security_type) {
  switch (security_type) {
    case camera_app::mojom::WifiSecurityType::kNone:
      return "";
    case camera_app::mojom::WifiSecurityType::kEap:
      return onc::wifi::kWPA_EAP;
    case camera_app::mojom::WifiSecurityType::kWep:
      return onc::wifi::kWEP_PSK;
    case camera_app::mojom::WifiSecurityType::kWpa:
      return onc::wifi::kWPA_PSK;
    default:
      NOTREACHED() << "Unexpected security type: "
                   << static_cast<int>(security_type);
  }
}

std::string FromMojoEapMethod(camera_app::mojom::WifiEapMethod eap_method) {
  switch (eap_method) {
    case camera_app::mojom::WifiEapMethod::kEapTls:
      return onc::eap::kEAP_TLS;
    case camera_app::mojom::WifiEapMethod::kEapTtls:
      return onc::eap::kEAP_TTLS;
    case camera_app::mojom::WifiEapMethod::kLeap:
      return onc::eap::kLEAP;
    case camera_app::mojom::WifiEapMethod::kPeap:
      return onc::eap::kPEAP;
    default:
      NOTREACHED() << "Unexpected EAP method: " << static_cast<int>(eap_method);
  }
}

std::string FromMojoEapPhase2Method(
    camera_app::mojom::WifiEapPhase2Method eap_phase2_method) {
  switch (eap_phase2_method) {
    case camera_app::mojom::WifiEapPhase2Method::kAutomatic:
      return onc::eap::kAutomatic;
    case camera_app::mojom::WifiEapPhase2Method::kChap:
      return onc::eap::kCHAP;
    case camera_app::mojom::WifiEapPhase2Method::kGtc:
      return onc::eap::kGTC;
    case camera_app::mojom::WifiEapPhase2Method::kMd5:
      return onc::eap::kMD5;
    case camera_app::mojom::WifiEapPhase2Method::kMschap:
      return onc::eap::kMSCHAP;
    case camera_app::mojom::WifiEapPhase2Method::kMschapv2:
      return onc::eap::kMSCHAPv2;
    case camera_app::mojom::WifiEapPhase2Method::kPap:
      return onc::eap::kPAP;
    default:
      NOTREACHED() << "Unexpected EAP Phase2 method: "
                   << static_cast<int>(eap_phase2_method);
  }
}

CameraAppUIDelegate::WifiConfig FromMojoWifiConfig(
    camera_app::mojom::WifiConfigPtr mojo_wifi_config) {
  CameraAppUIDelegate::WifiConfig config;
  config.ssid = mojo_wifi_config->ssid;
  config.security = FromMojoSecurityType(mojo_wifi_config->security);
  config.password = mojo_wifi_config->password;
  config.eap_method = mojo_wifi_config->eap_method.has_value()
                          ? std::make_optional<std::string>(FromMojoEapMethod(
                                mojo_wifi_config->eap_method.value()))
                          : std::nullopt;
  config.eap_phase2_method =
      mojo_wifi_config->eap_phase2_method.has_value()
          ? std::make_optional<std::string>(FromMojoEapPhase2Method(
                mojo_wifi_config->eap_phase2_method.value()))
          : std::nullopt;
  config.eap_identity = mojo_wifi_config->eap_identity;
  config.eap_anonymous_identity = mojo_wifi_config->eap_anonymous_identity;
  return config;
}

bool HasExternalScreen() {
  for (const auto& display : display::Screen::GetScreen()->GetAllDisplays()) {
    if (!display.IsInternal()) {
      return true;
    }
  }
  return false;
}

std::optional<uint32_t> ParseIntentIdFromUrl(const GURL& url) {
  std::string id_str;
  if (!net::GetValueForKeyInQuery(url, "intentId", &id_str)) {
    return std::nullopt;
  }

  uint32_t intent_id;
  if (!base::StringToUint(id_str, &intent_id)) {
    return std::nullopt;
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
      pending_intent_id_(std::nullopt),
      window_(window),
      document_scanner_service_(DocumentScannerServiceClient::Create()) {
  DCHECK(camera_app_ui);
  DCHECK(window);
  window->SetProperty(kCanConsumeSystemKeysKey, true);
  ScreenBacklight::Get()->AddObserver(this);
  ash::SessionManagerClient::Get()->AddObserver(this);
  sw_privacy_switch_state_observer_ =
      std::make_unique<media::CrosCameraSWPrivacySwitchStateObserver>(
          base::BindRepeating(
              &CameraAppHelperImpl::OnSWPrivacySwitchStateChanged,
              weak_factory_.GetWeakPtr()));
}

CameraAppHelperImpl::~CameraAppHelperImpl() {
  ash::SessionManagerClient::Get()->RemoveObserver(this);
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
    pending_intent_id_ = std::nullopt;
  }
  camera_result_callback_.Run(intent_id, action, data, std::move(callback));
}

void CameraAppHelperImpl::IsTabletMode(IsTabletModeCallback callback) {
  std::move(callback).Run(display::Screen::GetScreen()->InTabletMode());
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
  std::move(callback).Run(display::Screen::GetScreen()->InTabletMode());
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
  if (has_external_screen_ == HasExternalScreen()) {
    return;
  }
  has_external_screen_ = !has_external_screen_;

  if (external_screen_monitor_.is_bound()) {
    external_screen_monitor_->Update(has_external_screen_);
  }
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
    ConvertToDocumentCallback callback,
    bool success,
    const std::vector<uint8_t>& processed_jpeg_image) {
  if (!success) {
    LOG(ERROR) << "Failed to convert to document";
    std::move(callback).Run({});
    return;
  }
  std::move(callback).Run(processed_jpeg_image);
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

void CameraAppHelperImpl::IsDocumentScannerSupported(
    IsDocumentScannerSupportedCallback callback) {
  std::move(callback).Run(document_scanner_service_ != nullptr);
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
  base::span(memory.mapping).copy_from(jpeg_data);

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
  base::span(memory.mapping).copy_from(jpeg_data);

  // Since |this| owns |document_scanner_service|, and the callback will be
  // posted to other sequence with weak pointer of |document_scanner_service|.
  // Therefore, it is safe to use |base::Unretained(this)| here.
  document_scanner_service_->DoPostProcessing(
      std::move(memory.region), corners, rotation,
      base::BindOnce(&CameraAppHelperImpl::OnConvertedToDocument,
                     base::Unretained(this), std::move(callback)));
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

  camera_app_ui_->delegate()->StartStorageMonitor(
      base::BindRepeating(&CameraAppHelperImpl::OnStorageStatusUpdated,
                          weak_factory_.GetWeakPtr()));
}

void CameraAppHelperImpl::StopStorageMonitor() {
  camera_app_ui_->delegate()->StopStorageMonitor();
  if (!storage_callback_.is_null()) {
    std::move(storage_callback_)
        .Run(camera_app::mojom::StorageMonitorStatus::kCanceled);
  }
  if (storage_monitor_.is_bound()) {
    storage_monitor_.reset();
  }
}

void CameraAppHelperImpl::OnDisplayTabletStateChanged(
    display::TabletState state) {
  switch (state) {
    case display::TabletState::kEnteringTabletMode:
    case display::TabletState::kExitingTabletMode:
      break;
    case display::TabletState::kInClamshellMode:
      if (tablet_mode_monitor_.is_bound()) {
        tablet_mode_monitor_->Update(false);
      }
      break;
    case display::TabletState::kInTabletMode:
      if (tablet_mode_monitor_.is_bound()) {
        tablet_mode_monitor_->Update(true);
      }
      break;
  }
}

void CameraAppHelperImpl::OnScreenBacklightStateChanged(
    ScreenBacklightState screen_backlight_state) {
  if (screen_state_monitor_.is_bound()) {
    screen_state_monitor_->Update(ToMojoScreenState(screen_backlight_state));
  }
}

void CameraAppHelperImpl::OnDisplayAdded(const display::Display& new_display) {
  CheckExternalScreenState();
}

void CameraAppHelperImpl::OnDisplaysRemoved(
    const display::Displays& removed_displays) {
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

void CameraAppHelperImpl::OpenWifiDialog(
    camera_app::mojom::WifiConfigPtr wifi_config) {
  camera_app_ui_->delegate()->OpenWifiDialog(
      FromMojoWifiConfig(std::move(wifi_config)));
}

void CameraAppHelperImpl::SetLidStateMonitor(
    mojo::PendingRemote<LidStateMonitor> monitor,
    SetLidStateMonitorCallback callback) {
  CHECK(ash::mojo_service_manager::IsServiceManagerBound());
  lid_callback_ = std::move(callback);
  lid_state_monitor_ = mojo::Remote<LidStateMonitor>(std::move(monitor));
  ash::mojo_service_manager::GetServiceManagerProxy()->Request(
      /*service_name=*/chromeos::mojo_services::kCrosSystemEventMonitor,
      std::nullopt, monitor_.BindNewPipeAndPassReceiver().PassPipe());
  monitor_->AddLidObserver(lid_observer_receiver_.BindNewPipeAndPassRemote());
}

void CameraAppHelperImpl::SetSWPrivacySwitchMonitor(
    mojo::PendingRemote<SWPrivacySwitchMonitor> monitor,
    SetSWPrivacySwitchMonitorCallback callback) {
  sw_privacy_switch_monitor_ =
      mojo::Remote<SWPrivacySwitchMonitor>(std::move(monitor));
  std::move(callback).Run(is_sw_privacy_switch_on_);
}

void CameraAppHelperImpl::OnLidStateChanged(cros::mojom::LidState state) {
  auto lid_state = ToMojoLidState(state);
  if (!lid_callback_.is_null()) {
    std::move(lid_callback_).Run(lid_state);
  } else if (lid_state_monitor_.is_bound()) {
    lid_state_monitor_->Update(lid_state);
  }
}

void CameraAppHelperImpl::OnSWPrivacySwitchStateChanged(
    cros::mojom::CameraPrivacySwitchState state) {
  is_sw_privacy_switch_on_ = state == cros::mojom::CameraPrivacySwitchState::ON;
  if (sw_privacy_switch_monitor_.is_bound()) {
    sw_privacy_switch_monitor_->Update(is_sw_privacy_switch_on_);
  }
}

void CameraAppHelperImpl::GetEventsSender(GetEventsSenderCallback callback) {
  if (!events_sender_) {
    auto system_language = camera_app_ui_->delegate()->GetSystemLanguage();
    events_sender_ = std::make_unique<CameraAppEventsSender>(system_language);
  }
  std::move(callback).Run(events_sender_->CreateConnection());
}

void CameraAppHelperImpl::SetScreenLockedMonitor(
    mojo::PendingRemote<ScreenLockedMonitor> monitor,
    SetScreenLockedMonitorCallback callback) {
  screen_locked_monitor_ =
      mojo::Remote<ScreenLockedMonitor>(std::move(monitor));
  std::move(callback).Run(ash::SessionManagerClient::Get()->IsScreenLocked());
}

void CameraAppHelperImpl::ScreenLockedStateUpdated() {
  if (!screen_locked_monitor_.is_bound()) {
    return;
  }
  screen_locked_monitor_->Update(
      ash::SessionManagerClient::Get()->IsScreenLocked());
}

void CameraAppHelperImpl::RenderPdfAsJpeg(const std::vector<uint8_t>& pdf_data,
                                          RenderPdfAsJpegCallback callback) {
  camera_app_ui_->delegate()->RenderPdfAsJpeg(pdf_data, std::move(callback));
}

void CameraAppHelperImpl::PerformOcr(mojo_base::BigBuffer jpeg_data,
                                     PerformOcrCallback callback) {
  camera_app_ui_->delegate()->PerformOcr(jpeg_data, std::move(callback));
}

void CameraAppHelperImpl::PerformOcrInline(
    const std::vector<uint8_t>& jpeg_data,
    PerformOcrCallback callback) {
  camera_app_ui_->delegate()->PerformOcr(jpeg_data, std::move(callback));
}

void CameraAppHelperImpl::CreatePdfBuilder(
    mojo::PendingReceiver<camera_app::mojom::PdfBuilder> receiver) {
  return camera_app_ui_->delegate()->CreatePdfBuilder(std::move(receiver));
}

}  // namespace ash
