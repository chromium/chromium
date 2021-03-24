// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/projector/projector_ui_controller.h"

#include "ash/projector/projector_controller_impl.h"
#include "ash/projector/ui/projector_bar_view.h"
#include "ash/public/cpp/toast_data.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/toast/toast_manager_impl.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
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
  MarkerController::Get()->SetEnabled(enabled);
}

}  // namespace

ProjectorUiController::ProjectorUiController(
    ProjectorControllerImpl* projector_controller)
    : projector_controller_(projector_controller) {
  auto* laser_pointer_controller = Shell::Get()->laser_pointer_controller();
  DCHECK(laser_pointer_controller);
  laser_pointer_controller_observation_.Observe(laser_pointer_controller);

  auto* marker_controller = MarkerController::Get();
  DCHECK(marker_controller);
  marker_controller_observation_.Observe(marker_controller);

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

  projector_bar_widget_->Close();
  projector_bar_view_ = nullptr;
  model_.SetBarEnabled(false);
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

void ProjectorUiController::OnTranscription(const std::string& transcription,
                                            bool is_final) {}

bool ProjectorUiController::IsToolbarVisible() const {
  return model_.bar_enabled();
}

// TODO(llin): Refactor this logic into ProjectorTool and ProjectorToolManager.
void ProjectorUiController::ResetTools() {
  // Reset laser pointer.
  EnableLaserPointer(false);
  // Reset marker.
  EnableMarker(false);
}

void ProjectorUiController::OnLaserPointerStateChanged(bool enabled) {
  // Disable marker if laser pointer is enabled;
  if (enabled)
    MarkerController::Get()->SetEnabled(false);

  if (projector_bar_view_)
    projector_bar_view_->OnLaserPointerStateChanged(enabled);
}

void ProjectorUiController::OnMarkerStateChanged(bool enabled) {
  // Disable laser pointer since marker if enabled;
  if (enabled)
    Shell::Get()->laser_pointer_controller()->SetEnabled(false);

  if (projector_bar_view_)
    projector_bar_view_->OnMarkerStateChanged(enabled);
}

void ProjectorUiController::OnProjectorSessionActiveStateChanged(bool active) {
  if (!active)
    MarkerController::Get()->Clear();
}

}  // namespace ash
