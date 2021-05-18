// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/projector/projector_ui_controller.h"

#include "ash/accessibility/magnifier/partial_magnification_controller.h"
#include "ash/projector/projector_controller_impl.h"
#include "ash/projector/ui/projector_bar_view.h"
#include "ash/public/cpp/toast_data.h"
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
constexpr base::TimeDelta kToastDuration =
    base::TimeDelta::FromMilliseconds(2500);

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
}

void EnableLaserPointer(bool enabled) {
  auto* laser_pointer_controller = Shell::Get()->laser_pointer_controller();
  DCHECK(laser_pointer_controller);
  Shell::Get()->laser_pointer_controller()->SetEnabled(enabled);
}

void EnableMarker(bool enabled) {
  auto* marker_controller = MarkerController::Get();
  DCHECK(marker_controller);
  marker_controller->SetEnabled(enabled);

  if (enabled) {
    marker_controller->ChangeColor(
        ProjectorBarView::kProjectorMarkerDefaultColor);
  }
}

void EnableMagnifier(bool enabled) {
  auto* magnifier_controller = Shell::Get()->partial_magnification_controller();
  DCHECK(magnifier_controller);
  magnifier_controller->SetEnabled(enabled);
  magnifier_controller->set_allow_mouse_following(enabled);
}

}  // namespace

// This class controls the interaction with the caption bubble. It keeps track
// of the lifetime and visibility state of the CaptionBubble.
class ProjectorUiController::CaptionBubbleController
    : public views::WidgetObserver {
 public:
  explicit CaptionBubbleController(ProjectorUiController* controller)
      : controller_(controller) {
    aura::Window* root_window = Shell::Get()->GetRootWindowForNewWindows();
    caption_bubble_model_ = std::make_unique<captions::CaptionBubbleModel>(
        root_window->GetBoundsInScreen(), base::NullCallback());

    auto* caption_bubble = new captions::CaptionBubble(
        base::NullCallback(), /* hide_on_inactivity= */ false);
    caption_bubble_widget_ = base::WrapUnique<views::Widget>(
        views::BubbleDialogDelegateView::CreateBubble(caption_bubble));
    caption_bubble->SetModel(caption_bubble_model_.get());
    caption_bubble_widget_->AddObserver(this);
    AddExcludedWindowToFastInkController(
        caption_bubble_widget_->GetNativeWindow());
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
  std::unique_ptr<captions::CaptionBubbleModel> caption_bubble_model_;
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

  auto* partial_magnification_controller =
      Shell::Get()->partial_magnification_controller();
  DCHECK(partial_magnification_controller);
  partial_magnification_observation_.Observe(partial_magnification_controller);

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
}

void ProjectorUiController::CloseToolbar() {
  if (!projector_bar_widget_)
    return;

  ResetTools();

  caption_bubble_->Close();
  projector_bar_widget_->Close();
  projector_bar_view_ = nullptr;
  model_.SetBarEnabled(false);
}

void ProjectorUiController::SetCaptionBubbleState(bool enabled) {
  if (enabled) {
    caption_bubble_->Open();
    return;
  }

  caption_bubble_->Close();
}

void ProjectorUiController::OnKeyIdeaMarked() {
  ShowToast(kMarkedKeyIdeaToastId, IDS_ASH_PROJECTOR_KEY_IDEA_MARKED,
            kToastDuration);
}

void ProjectorUiController::OnLaserPointerPressed() {
  auto* laser_pointer_controller = Shell::Get()->laser_pointer_controller();
  DCHECK(laser_pointer_controller);
  EnableLaserPointer(!laser_pointer_controller->is_enabled());
}

void ProjectorUiController::OnMarkerPressed() {
  auto* marker_controller = MarkerController::Get();
  DCHECK(marker_controller);
  EnableMarker(!marker_controller->is_enabled());
}

void ProjectorUiController::OnClearAllMarkersPressed() {
  auto* marker_controller = MarkerController::Get();
  DCHECK(marker_controller);
  marker_controller->Clear();
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
}

void ProjectorUiController::OnRecordingStateChanged(bool started) {
  projector_bar_view_->OnRecordingStateChanged(started);
  if (caption_bubble_->IsCaptionBubbleModelOpen())
    caption_bubble_->Close();
}

void ProjectorUiController::OnMagnifierButtonPressed(bool enabled) {
  EnableMagnifier(enabled);
}

void ProjectorUiController::OnChangeMarkerColorPressed(SkColor new_color) {
  auto* marker_controller = MarkerController::Get();
  DCHECK(marker_controller);
  marker_controller->ChangeColor(new_color);
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
