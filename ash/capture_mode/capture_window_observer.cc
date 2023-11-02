// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_window_observer.h"

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/capture_mode/capture_mode_camera_controller.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_session.h"
#include "ash/capture_mode/capture_mode_util.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ui/compositor/layer.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

CaptureWindowObserver::CaptureWindowObserver(
    CaptureModeSession* capture_mode_session)
    : capture_mode_session_(capture_mode_session) {
  Shell::Get()->activation_client()->AddObserver(this);
}

CaptureWindowObserver::~CaptureWindowObserver() {
  auto* shell = Shell::Get();
  shell->activation_client()->RemoveObserver(this);
  StopObserving();
}

void CaptureWindowObserver::UpdateSelectedWindowAtPosition(
    const gfx::Point& location_in_screen,
    const std::set<aura::Window*>& ignore_windows) {
  if (capture_mode_session_->IsInCountDownAnimation())
    return;
  location_in_screen_ = location_in_screen;

  SetSelectedWindow(
      capture_mode_util::GetTopMostCapturableWindowAtPoint(location_in_screen));
  capture_mode_session_->UpdateCursor(location_in_screen, /*is_touch=*/false);
}

void CaptureWindowObserver::SetSelectedWindow(aura::Window* window) {
  if (window_ == window)
    return;

  // Don't capture wallpaper window.
  if (window && window->parent() &&
      window->parent()->GetId() == kShellWindowId_WallpaperContainer) {
    window = nullptr;
  }

  // Don't capture home screen window.
  if (window &&
      window == Shell::Get()->app_list_controller()->GetHomeScreenWindow()) {
    window = nullptr;
  }

  // Stop observing the current selected window if there is one.
  StopObserving();
  if (window) {
    StartObserving(window);
    capture_mode_session_->A11yAlertCaptureSource(/*trigger_now=*/true);
  }
  RepaintCaptureRegion();

  auto* controller = CaptureModeController::Get();
  if (!controller->is_recording_in_progress())
    controller->camera_controller()->MaybeReparentPreviewWidget();
  capture_mode_session_->MaybeUpdateCaptureUisOpacity(
      display::Screen::GetScreen()->GetCursorScreenPoint());
}

void CaptureWindowObserver::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  DCHECK_EQ(window, window_);
  RepaintCaptureRegion();

  // The bounds of camera preview should be updated accordingly if the bounds of
  // the selected window has been updated.
  auto* controller = CaptureModeController::Get();
  if (!controller->is_recording_in_progress())
    controller->camera_controller()->MaybeUpdatePreviewWidget();
}

void CaptureWindowObserver::OnWindowVisibilityChanging(aura::Window* window,
                                                       bool visible) {
  DCHECK_EQ(window, window_);
  DCHECK(!visible);
  if (capture_mode_session_->IsInCountDownAnimation()) {
    CaptureModeController::Get()->Stop();
    return;
  }

  StopObserving();
  UpdateSelectedWindowAtPosition(location_in_screen_,
                                 /*ignore_windows=*/{window});
}

void CaptureWindowObserver::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(window, window_);
  if (capture_mode_session_->IsInCountDownAnimation()) {
    CaptureModeController::Get()->Stop();
    return;
  }

  StopObserving();
  UpdateSelectedWindowAtPosition(location_in_screen_,
                                 /*ignore_windows=*/{window});
}

void CaptureWindowObserver::OnWindowActivated(ActivationReason reason,
                                              aura::Window* gained_active,
                                              aura::Window* lost_active) {
  // If another window is activated on top of the current selected window, we
  // may change the selected window to the activated window if it's under the
  // current event location. If there is no selected window at the moment, we
  // also want to check if new activated window should be focused.
  UpdateSelectedWindowAtPosition(location_in_screen_, /*ignore_windows=*/{});
}

void CaptureWindowObserver::StartObserving(aura::Window* window) {
  window_ = window;
  window_->AddObserver(this);
}

void CaptureWindowObserver::StopObserving() {
  if (window_) {
    window_->RemoveObserver(this);
    window_ = nullptr;
  }
}

void CaptureWindowObserver::RepaintCaptureRegion() {
  ui::Layer* layer = capture_mode_session_->layer();
  layer->SchedulePaint(layer->bounds());
}

}  // namespace ash
