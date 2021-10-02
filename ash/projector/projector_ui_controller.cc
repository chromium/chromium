// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/projector/projector_ui_controller.h"

#include "ash/accessibility/caption_bubble_context_ash.h"
#include "ash/accessibility/magnifier/partial_magnifier_controller.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/projector/projector_controller_impl.h"
#include "ash/projector/projector_metrics.h"
#include "ash/projector/ui/projector_bar_view.h"
#include "ash/public/cpp/toast_data.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/toast/toast_manager_impl.h"
#include "base/callback_helpers.h"
#include "components/live_caption/views/caption_bubble.h"
#include "components/live_caption/views/caption_bubble_model.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

constexpr char kMarkedKeyIdeaToastId[] = "projector_marked_key_idea";
constexpr base::TimeDelta kToastDuration = base::Milliseconds(2500);

void ShowToast(const std::string& id,
               int message_id,
               base::TimeDelta duration) {
  DCHECK(Shell::Get());
  DCHECK(Shell::Get()->toast_manager());

  ToastData toast(id, l10n_util::GetStringUTF16(message_id),
                  duration.InMilliseconds(),
                  l10n_util::GetStringUTF16(IDS_ASH_TOAST_DISMISS_BUTTON));
  Shell::Get()->toast_manager()->Show(toast);
}

void AddExcludedWindowToFastInkController(aura::Window* window) {
  DCHECK(window);
  Shell::Get()->laser_pointer_controller()->AddExcludedWindow(window);
  MarkerController::Get()->AddExcludedWindow(window);
  // TODO(b/200341176): Add excluded windows to RecordingOverlayView instead of
  // MarkerController.
}

void EnableLaserPointer(bool enabled) {
  auto* laser_pointer_controller = Shell::Get()->laser_pointer_controller();
  DCHECK(laser_pointer_controller);
  Shell::Get()->laser_pointer_controller()->SetEnabled(enabled);
}

void EnableMarker(bool enabled) {
  if (features::IsProjectorAnnotatorEnabled()) {
    auto* capture_mode_controller = CaptureModeController::Get();
    // TODO(b/200292852): This check should not be necessary, but because
    // several Projector unit tests that rely on mocking and don't test the real
    // code path, we can end up calling |ToggleRecordingOverlayEnabled()|
    // without ever starting a Projector recording session.
    // |CaptureModeController| asserts all invariants via DCHECKs, and those
    // tests would crash. Remove any unnecessary mocks and test the real thing
    // if possible.
    if (capture_mode_controller->is_recording_in_progress())
      capture_mode_controller->ToggleRecordingOverlayEnabled();
    return;
  }
  // TODO(b/200341176): Remove the older marker tools.
  auto* marker_controller = MarkerController::Get();
  DCHECK(marker_controller);
  marker_controller->SetEnabled(enabled);

  if (enabled) {
    marker_controller->ChangeColor(
        ProjectorBarView::kProjectorMarkerDefaultColor);
  }
}

void EnableMagnifier(bool enabled) {
  auto* magnifier_controller = Shell::Get()->partial_magnifier_controller();
  DCHECK(magnifier_controller);
  magnifier_controller->SetEnabled(enabled);
  magnifier_controller->set_allow_mouse_following(enabled);
}

ProjectorMarkerColor GetMarkerColor(SkColor color) {
  switch (color) {
    case SK_ColorBLACK:
      return ProjectorMarkerColor::kBlack;
    case SK_ColorWHITE:
      return ProjectorMarkerColor::kWhite;
    case SK_ColorBLUE:
      return ProjectorMarkerColor::kBlue;
    default:
      NOTREACHED();
      return ProjectorMarkerColor::kMaxValue;
  }
}

}  // namespace

// This class controls the interaction with the caption bubble. It keeps track
// of the lifetime and visibility state of the CaptionBubble.
class ProjectorUiController::CaptionBubbleController
    : public views::WidgetObserver {
 public:
  explicit CaptionBubbleController(ProjectorUiController* controller)
      : controller_(controller) {
    caption_bubble_context_ =
        std::make_unique<captions::CaptionBubbleContextAsh>();
    caption_bubble_model_ = std::make_unique<::captions::CaptionBubbleModel>(
        caption_bubble_context_.get());

    auto* caption_bubble = new ::captions::CaptionBubble(
        base::NullCallback(), /* hide_on_inactivity= */ false);
    caption_bubble_widget_ = base::WrapUnique<views::Widget>(
        views::BubbleDialogDelegateView::CreateBubble(caption_bubble));
    caption_bubble->SetModel(caption_bubble_model_.get());
    caption_bubble_widget_->AddObserver(this);
    AddExcludedWindowToFastInkController(
        caption_bubble_widget_->GetNativeWindow());
    // Use Picture-in-Picture (PIP) window management logic for caption bubble
    // so that
    // a) it avoids collision with system UI such as virtual keyboards, quick
    // settings etc.
    // b) it is draggable in tablet mode as well.
    caption_bubble_widget_->GetNativeWindow()->SetProperty(
        ash::kWindowPipTypeKey, true);
  }

  CaptionBubbleController(const CaptionBubbleController&) = delete;
  CaptionBubbleController& operator=(const CaptionBubbleController&) = delete;
  ~CaptionBubbleController() override {
    if (caption_bubble_widget_) {
      caption_bubble_widget_->RemoveObserver(this);
      caption_bubble_widget_->CloseNow();
    }
  }

  void Open() {
    caption_bubble_model_->Open();
    controller_->OnCaptionBubbleModelStateChanged(true);
  }

  void Close() {
    caption_bubble_model_->Close();
    controller_->OnCaptionBubbleModelStateChanged(false);
  }

  bool IsCaptionBubbleModelOpen() const {
    return !caption_bubble_model_->IsClosed();
  }

  void OnTranscription(const std::string& transcription, bool is_final) {
    if (caption_bubble_model_->IsClosed())
      return;

    caption_bubble_model_->SetPartialText(transcription);
    if (is_final)
      caption_bubble_model_->CommitPartialText();
  }

 private:
  // views::WidgetObserver:
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override {
    DCHECK_EQ(widget, caption_bubble_widget_.get());
    controller_->OnCaptionBubbleModelStateChanged(
        !caption_bubble_model_->IsClosed());
  }

  // Owns the instance of this class.
  ProjectorUiController* const controller_;

  views::UniqueWidgetPtr caption_bubble_widget_;
  std::unique_ptr<::captions::CaptionBubbleModel> caption_bubble_model_;
  std::unique_ptr<captions::CaptionBubbleContextAsh> caption_bubble_context_;
};

ProjectorUiController::ProjectorUiController(
    ProjectorControllerImpl* projector_controller)
    : projector_controller_(projector_controller) {
  auto* laser_pointer_controller = Shell::Get()->laser_pointer_controller();
  DCHECK(laser_pointer_controller);
  laser_pointer_controller_observation_.Observe(laser_pointer_controller);

  auto* marker_controller = MarkerController::Get();
  DCHECK(marker_controller);
  marker_controller_observation_.Observe(marker_controller);

  auto* partial_magnifier_controller =
      Shell::Get()->partial_magnifier_controller();
  DCHECK(partial_magnifier_controller);
  partial_magnification_observation_.Observe(partial_magnifier_controller);

  caption_bubble_ =
      std::make_unique<ProjectorUiController::CaptionBubbleController>(this);

  projector_session_observation_.Observe(
      projector_controller->projector_session());
}

ProjectorUiController::~ProjectorUiController() = default;

void ProjectorUiController::ShowToolbar() {
  if (!projector_bar_widget_) {
    // Create the toolbar.
    projector_bar_widget_ = ProjectorBarView::Create(projector_controller_);
    projector_bar_view_ = static_cast<ProjectorBarView*>(
        projector_bar_widget_->GetContentsView());
    AddExcludedWindowToFastInkController(
        projector_bar_widget_->GetNativeWindow());
  }

  projector_bar_widget_->ShowInactive();
  model_.SetBarEnabled(true);

  RecordToolbarMetrics(ProjectorToolbar::kToolbarOpened);
}

void ProjectorUiController::CloseToolbar() {
  if (!projector_bar_widget_)
    return;

  ResetTools();

  caption_bubble_->Close();
  projector_bar_widget_->Close();
  projector_bar_view_ = nullptr;
  model_.SetBarEnabled(false);

  RecordToolbarMetrics(ProjectorToolbar::kToolbarClosed);
}

void ProjectorUiController::SetCaptionBubbleState(bool enabled) {
  if (enabled) {
    caption_bubble_->Open();
  } else {
    caption_bubble_->Close();
  }
  RecordToolbarMetrics(enabled ? ProjectorToolbar::kStartClosedCaptions
                               : ProjectorToolbar::kStopClosedCaptions);
}

void ProjectorUiController::OnKeyIdeaMarked() {
  ShowToast(kMarkedKeyIdeaToastId, IDS_ASH_PROJECTOR_KEY_IDEA_MARKED,
            kToastDuration);
  RecordToolbarMetrics(ProjectorToolbar::kKeyIdea);
}

void ProjectorUiController::OnLaserPointerPressed() {
  auto* laser_pointer_controller = Shell::Get()->laser_pointer_controller();
  DCHECK(laser_pointer_controller);
  EnableLaserPointer(!laser_pointer_controller->is_enabled());
  RecordToolbarMetrics(ProjectorToolbar::kLaserPointer);
}

void ProjectorUiController::OnMarkerPressed() {
  auto* marker_controller = MarkerController::Get();
  DCHECK(marker_controller);
  EnableMarker(!marker_controller->is_enabled());
  RecordToolbarMetrics(ProjectorToolbar::kMarkerTool);
}

void ProjectorUiController::OnClearAllMarkersPressed() {
  auto* marker_controller = MarkerController::Get();
  DCHECK(marker_controller);
  marker_controller->Clear();
  RecordToolbarMetrics(ProjectorToolbar::kClearAllMarkers);
}

void ProjectorUiController::OnUndoPressed() {
  auto* marker_controller = MarkerController::Get();
  DCHECK(marker_controller);
  marker_controller->UndoLastStroke();
  RecordToolbarMetrics(ProjectorToolbar::kUndo);
}

void ProjectorUiController::OnCaptionBubbleModelStateChanged(bool opened) {
  projector_bar_view_->OnCaptionBubbleModelStateChanged(opened);
  projector_controller_->OnCaptionBubbleModelStateChanged(opened);
}

void ProjectorUiController::OnTranscription(const std::string& transcription,
                                            bool is_final) {
  caption_bubble_->OnTranscription(transcription, is_final);
}

void ProjectorUiController::OnSelfieCamPressed(bool enabled) {
  // If the selfie cam is visible, then the button for turning on the selfie cam
  // should be hidden in the projector bar view. The button for turning off the
  // selfie cam should show instead.
  if (projector_bar_view_)
    projector_bar_view_->OnSelfieCamStateChanged(enabled);
  RecordToolbarMetrics(enabled ? ProjectorToolbar::kStartSelfieCamera
                               : ProjectorToolbar::kStopSelfieCamera);
}

void ProjectorUiController::OnRecordingStateChanged(bool started) {
  projector_bar_view_->OnRecordingStateChanged(started);
  if (caption_bubble_->IsCaptionBubbleModelOpen())
    caption_bubble_->Close();
}

void ProjectorUiController::OnMagnifierButtonPressed(bool enabled) {
  EnableMagnifier(enabled);
  RecordToolbarMetrics(enabled ? ProjectorToolbar::kStartMagnifier
                               : ProjectorToolbar::kStopMagnifier);
}

void ProjectorUiController::OnChangeMarkerColorPressed(SkColor new_color) {
  auto* marker_controller = MarkerController::Get();
  DCHECK(marker_controller);
  marker_controller->ChangeColor(new_color);
  RecordMarkerColorMetrics(GetMarkerColor(new_color));
}

bool ProjectorUiController::IsToolbarVisible() const {
  return model_.bar_enabled();
}

bool ProjectorUiController::IsCaptionBubbleModelOpen() const {
  return caption_bubble_->IsCaptionBubbleModelOpen();
}

// TODO(llin): Refactor this logic into ProjectorTool and ProjectorToolManager.
void ProjectorUiController::ResetTools() {
  // Reset laser pointer.
  EnableLaserPointer(false);
  // Reset marker.
  EnableMarker(false);
  // Reset magnifier.
  EnableMagnifier(false);
}

void ProjectorUiController::OnLaserPointerStateChanged(bool enabled) {
  // If laser pointer is enabled, disable marker and magnifier.
  if (enabled) {
    EnableMarker(false);
    EnableMagnifier(false);
  }

  if (projector_bar_view_)
    projector_bar_view_->OnLaserPointerStateChanged(enabled);
}

void ProjectorUiController::OnMarkerStateChanged(bool enabled) {
  // If marker is enabled, disable laser pointer and magnifier.
  if (enabled) {
    EnableLaserPointer(false);
    EnableMagnifier(false);
  }

  if (projector_bar_view_)
    projector_bar_view_->OnMarkerStateChanged(enabled);
}

void ProjectorUiController::OnProjectorSessionActiveStateChanged(bool active) {
  if (!active)
    MarkerController::Get()->Clear();
}

void ProjectorUiController::OnPartialMagnificationStateChanged(bool enabled) {
  // If magnifier is enabled, disable laser pointer and marker.
  if (enabled) {
    EnableMarker(false);
    EnableLaserPointer(false);
  }

  if (projector_bar_view_)
    projector_bar_view_->OnMagnifierStateChanged(enabled);
}

}  // namespace ash
