// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/base_capture_mode_session.h"
#include "ash/accessibility/accessibility_controller.h"
#include "ash/capture_mode/capture_mode_camera_controller.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ash/capture_mode/capture_mode_util.h"
#include "ash/display/mouse_cursor_event_filter.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_item.h"
#include "ui/aura/client/capture_client.h"
#include "ui/compositor/layer.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

BaseCaptureModeSession::BaseCaptureModeSession(
    CaptureModeController* controller,
    CaptureModeBehavior* active_behavior,
    SessionType type)
    : controller_(controller),
      active_behavior_(active_behavior),
      session_type_(type),
      current_root_(capture_mode_util::GetPreferredRootWindow()) {}

BaseCaptureModeSession::~BaseCaptureModeSession() = default;

void BaseCaptureModeSession::Initialize() {
  SetLayer(std::make_unique<ui::Layer>(ui::LAYER_TEXTURED));
  layer()->SetFillsBoundsOpaquely(false);

  InitInternal();

  // Selfie cam is not needed for the null session, but the recording will take
  // ownership if/when it starts.
  MaybeUpdateSelfieCamInSessionVisibility();

  if (active_behavior_->ShouldAutoSelectFirstCamera()) {
    controller_->camera_controller()->MaybeSelectFirstCamera();
  }

  Shell::Get()->AddShellObserver(this);
  active_behavior_->AttachToSession();
}

void BaseCaptureModeSession::Shutdown() {
  is_shutting_down_ = true;

  ShutdownInternal();

  if (!is_stopping_to_start_video_recording_) {
    // Kill the camera preview when the capture mode session ends without
    // starting any recording. Note that we need to kill the camera preview
    // before aborting the client initiated capture mode session that requires
    // the camera to avoid repareting the camera preview widget which will lead
    // to crash.
    if (!controller_->is_recording_in_progress()) {
      controller_->camera_controller()->SetShouldShowPreview(false);

      // Reset the camera selection if it was auto-selected in a client-
      // initiated (e.g. Projector or Game Dashboard) capture mode session if
      // this session is ending and no video recording is active. We need to do
      // this to avoid the camera selection settings of the next default capture
      // mode session being overridden by the client-initiated capture mode
      // session. We also need to do this only if no recording is currently
      // active since an active recording must have come from a previous
      // different session than `this` (see http://b/353883311).
      controller_->camera_controller()->MaybeRevertAutoCameraSelection();
    }

    if (controller_->type() == CaptureModeType::kVideo) {
      controller_->NotifyRecordingStartAborted();
    }

    // The session is about to end and recording won't start afterwards. The
    // active behavior may have overwritten some of the configs, we need to
    // restore them back to their original values so that future sessions are
    // not affected.
    active_behavior_->DetachFromSession();
  }

  Shell::Get()->RemoveShellObserver(this);
}

aura::Window* BaseCaptureModeSession::GetOnCaptureSurfaceWidgetParentWindow()
    const {
  auto* controller = CaptureModeController::Get();
  DCHECK(!controller->is_recording_in_progress());
  auto* menu_container =
      current_root_->GetChildById(kShellWindowId_MenuContainer);
  auto* unparented_container =
      current_root_->GetChildById(kShellWindowId_UnparentedContainer);

  switch (controller->source()) {
    case CaptureModeSource::kFullscreen:
      return menu_container;
    case CaptureModeSource::kRegion:
      return controller_->user_capture_region().IsEmpty() ? unparented_container
                                                          : menu_container;
    case CaptureModeSource::kWindow:
      aura::Window* selected_window = GetSelectedWindow();
      return selected_window ? selected_window : unparented_container;
  }
}

gfx::Rect BaseCaptureModeSession::GetCaptureSurfaceConfineBounds() const {
  auto* controller = CaptureModeController::Get();
  DCHECK(!controller->is_recording_in_progress());
  switch (controller->source()) {
    case CaptureModeSource::kFullscreen: {
      auto* parent = GetOnCaptureSurfaceWidgetParentWindow();
      DCHECK(parent);
      return display::Screen::GetScreen()
          ->GetDisplayNearestWindow(parent)
          .work_area();
    }
    case CaptureModeSource::kWindow: {
      auto* selected_window = GetSelectedWindow();
      return selected_window ? capture_mode_util::GetCaptureWindowConfineBounds(
                                   selected_window)
                             : gfx::Rect();
    }
    case CaptureModeSource::kRegion: {
      gfx::Rect capture_region = controller->user_capture_region();
      wm::ConvertRectToScreen(current_root_, &capture_region);
      return capture_region;
    }
  }
}

void BaseCaptureModeSession::OnRootWindowWillShutdown(
    aura::Window* root_window) {
  if (root_window == current_root_) {
    // There should always be a primary root window.
    DCHECK_NE(Shell::GetPrimaryRootWindow(), current_root_);
    MaybeChangeRoot(Shell::GetPrimaryRootWindow(),
                    /*root_window_will_shutdown=*/true);
  }
}

// static
aura::Window* BaseCaptureModeSession::GetParentContainer(aura::Window* root) {
  DCHECK(root);
  DCHECK(root->IsRootWindow());
  return root->GetChildById(kShellWindowId_MenuContainer);
}

void BaseCaptureModeSession::MaybeUpdateSelfieCamInSessionVisibility() {
  auto* camera_controller = controller_->camera_controller();
  CHECK(camera_controller);

  // Set the value to true for `SetShouldShowPreview` when the capture type is
  // `kVideo` with no video recording in progress.
  // Don't trigger `SetShouldShowPreview` if there's a video recording in
  // progress, since the capture type is restricted to `kImage` at this use case
  // and we don't want to affect the camera preview for the in_progress video
  // recording.
  if (!controller_->is_recording_in_progress()) {
    camera_controller->SetShouldShowPreview(controller_->type() ==
                                            CaptureModeType::kVideo);
    // The selfie camera may have already been visible from before, but had the
    // wrong parent and now needs to be updated (e.g. due to a change in the
    // capture type).
    camera_controller->MaybeReparentPreviewWidget();
  }
}

gfx::Rect BaseCaptureModeSession::GetSelectedWindowTargetBounds() const {
  auto* window = GetSelectedWindow();
  if (!window) {
    return gfx::Rect();
  }

  OverviewController* overview_controller = Shell::Get()->overview_controller();
  if (overview_controller->InOverviewSession()) {
    if (auto* item =
            overview_controller->overview_session()->GetOverviewItemForWindow(
                window)) {
      gfx::Rect target_bounds_in_root =
          gfx::ToRoundedRect(item->target_bounds());
      wm::ConvertRectFromScreen(window->GetRootWindow(),
                                &target_bounds_in_root);
      return target_bounds_in_root;
    }
  }

  return window->bounds();
}

}  // namespace ash
