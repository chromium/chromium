// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/projector/projector_ui_controller.h"

#include "ash/accessibility/caption_bubble_context_ash.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/projector/projector_annotation_tray.h"
#include "ash/projector/projector_controller_impl.h"
#include "ash/projector/projector_metrics.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/projector/annotator_tool.h"
#include "ash/public/cpp/projector/projector_annotator_controller.h"
#include "ash/public/cpp/projector/projector_client.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/toast/toast_manager_impl.h"
#include "base/functional/callback_helpers.h"
#include "components/live_caption/views/caption_bubble.h"
#include "components/live_caption/views/caption_bubble_model.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace ash {
namespace {

// A unique id to identify system notifications coming from this file.
constexpr char kProjectorNotifierId[] = "ash.projector_ui_controller";

// A unique id for system notifications reporting a generic failure.
constexpr char kProjectorErrorNotificationId[] = "projector_error_notification";

// A unique id for system notifications reporting a save failure.
constexpr char kProjectorSaveErrorNotificationId[] =
    "projector_save_error_notification";

ProjectorAnnotationTray* GetProjectorAnnotationTrayForRoot(aura::Window* root) {
  // It may happen that root is nullptr. This may happen in the event that
  // the annotation tray is hidden before the canvas finishes its
  // initialization.
  if (!root) {
    return nullptr;
  }

  DCHECK(root->IsRootWindow());

  // Recording can end when a display being fullscreen-captured gets removed, in
  // this case, we don't need to hide the button.
  if (root->is_destroying())
    return nullptr;

  // Can be null while shutting down.
  auto* root_window_controller = RootWindowController::ForWindow(root);
  if (!root_window_controller)
    return nullptr;

  auto* projector_annotation_tray =
      root_window_controller->GetStatusAreaWidget()
          ->projector_annotation_tray();
  DCHECK(projector_annotation_tray);
  return projector_annotation_tray;
}

void SetProjectorAnnotationTrayVisibility(aura::Window* root, bool visible) {
  if (auto* projector_annotation_tray = GetProjectorAnnotationTrayForRoot(root))
    projector_annotation_tray->SetVisiblePreferred(visible);
}

void ToggleAnnotatorCanvas() {
  auto* capture_mode_controller = CaptureModeController::Get();
  // TODO(b/200292852): This check should not be necessary, but because
  // several Projector unit tests that rely on mocking and don't test the real
  // code path, we can end up calling |ToggleRecordingOverlayEnabled()|
  // without ever starting a Projector recording session.
  // |CaptureModeController| asserts all invariants via DCHECKs, and those
  // tests would crash. Remove any unnecessary mocks and test the real thing
  // if possible.
  if (capture_mode_controller->is_recording_in_progress()) {
    capture_mode_controller->ToggleRecordingOverlayEnabled();
  }
}

// Shows a Projector-related notification to the user with the given parameters.
void ShowNotification(
    const std::string& notification_id,
    int title_id,
    int message_id,
    message_center::SystemNotificationWarningLevel warning_level =
        message_center::SystemNotificationWarningLevel::NORMAL,
    const message_center::RichNotificationData& optional_fields = {},
    scoped_refptr<message_center::NotificationDelegate> delegate = nullptr,
    const gfx::VectorIcon& notification_icon = kPaletteTrayIconProjectorIcon) {
  std::unique_ptr<message_center::Notification> notification =
      CreateSystemNotificationPtr(
          message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
          l10n_util::GetStringUTF16(title_id),
          l10n_util::GetStringUTF16(message_id),
          l10n_util::GetStringUTF16(IDS_ASH_PROJECTOR_DISPLAY_SOURCE), GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kProjectorNotifierId, NotificationCatalogName::kProjector),
          optional_fields, delegate, notification_icon, warning_level);

  // Remove the previous notification before showing the new one if there are
  // any.
  auto* message_center = message_center::MessageCenter::Get();
  message_center->RemoveNotification(notification_id,
                                     /*by_user=*/false);
  message_center->AddNotification(std::move(notification));
}

}  // namespace

// static
void ProjectorUiController::ShowFailureNotification(int message_id,
                                                    int title_id) {
  RecordCreationFlowError(message_id);
  ShowNotification(
      kProjectorErrorNotificationId, title_id, message_id,
      message_center::SystemNotificationWarningLevel::CRITICAL_WARNING);
}

// static
void ProjectorUiController::ShowSaveFailureNotification() {
  RecordCreationFlowError(IDS_ASH_PROJECTOR_SAVE_FAILURE_TEXT);
  ShowNotification(
      kProjectorSaveErrorNotificationId, IDS_ASH_PROJECTOR_SAVE_FAILURE_TITLE,
      IDS_ASH_PROJECTOR_SAVE_FAILURE_TEXT,
      message_center::SystemNotificationWarningLevel::CRITICAL_WARNING);
}

ProjectorUiController::ProjectorUiController(
    ProjectorControllerImpl* projector_controller) {
  projector_session_observation_.Observe(
      projector_controller->projector_session());
}

ProjectorUiController::~ProjectorUiController() = default;

void ProjectorUiController::ShowAnnotationTray(aura::Window* current_root) {
  current_root_ = current_root;

  // Show the tray icon.
  SetProjectorAnnotationTrayVisibility(current_root_, /*visible=*/true);
}

void ProjectorUiController::HideAnnotationTray() {
  ResetTools();
  // Hide the tray icon.
  if (auto* projector_annotation_tray =
          GetProjectorAnnotationTrayForRoot(current_root_)) {
    projector_annotation_tray->HideAnnotationTray();
  }

  canvas_initialized_state_.reset();
  current_root_ = nullptr;
}

void ProjectorUiController::EnableAnnotatorTool() {
  if (!annotator_enabled_) {
    ToggleAnnotatorCanvas();
    annotator_enabled_ = !annotator_enabled_;
    RecordToolbarMetrics(ProjectorToolbar::kMarkerTool);
  }
}

void ProjectorUiController::SetAnnotatorTool(const AnnotatorTool& tool) {
  ash::ProjectorAnnotatorController::Get()->SetTool(tool);
  RecordMarkerColorMetrics(GetMarkerColorForMetrics(tool.color));
}

void ProjectorUiController::ResetTools() {
  if (annotator_enabled_) {
    ToggleAnnotatorCanvas();
    annotator_enabled_ = false;
    ash::ProjectorAnnotatorController::Get()->Clear();
  }
}

void ProjectorUiController::OnCanvasInitialized(bool success) {
  canvas_initialized_state_ = success;
  UpdateTrayEnabledState();
}

bool ProjectorUiController::GetAnnotatorAvailability() {
  if (!canvas_initialized_state_) {
    return false;
  }
  return *canvas_initialized_state_;
}

void ProjectorUiController::ToggleAnnotationTray() {
  if (auto* projector_annotation_tray =
          GetProjectorAnnotationTrayForRoot(current_root_)) {
    projector_annotation_tray->ToggleAnnotator();
  }
}

void ProjectorUiController::OnRecordedWindowChangingRoot(
    aura::Window* new_root) {
  DCHECK_NE(new_root, current_root_);

  SetProjectorAnnotationTrayVisibility(current_root_, /*visible=*/false);
  SetProjectorAnnotationTrayVisibility(new_root, /*visible=*/true);
  current_root_ = new_root;
  if (GetAnnotatorAvailability())
    UpdateTrayEnabledState();
}

void ProjectorUiController::OnProjectorSessionActiveStateChanged(bool active) {
  if (!active)
    ResetTools();
}

ProjectorMarkerColor ProjectorUiController::GetMarkerColorForMetrics(
    SkColor color) {
  std::map<SkColor, ProjectorMarkerColor> marker_colors_map = {
      {kProjectorMagentaPenColor, ProjectorMarkerColor::kMagenta},
      {kProjectorBluePenColor, ProjectorMarkerColor::kBlue},
      {kProjectorRedPenColor, ProjectorMarkerColor::kRed},
      {kProjectorYellowPenColor, ProjectorMarkerColor::kYellow}};
  return marker_colors_map[color];
}

void ProjectorUiController::UpdateTrayEnabledState() {
  if (auto* projector_annotation_tray =
          GetProjectorAnnotationTrayForRoot(current_root_)) {
    projector_annotation_tray->SetTrayEnabled(GetAnnotatorAvailability());
  }
}
}  // namespace ash
