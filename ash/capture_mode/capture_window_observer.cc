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
#include "ash/wm/desks/desks_util.h"
#include "ui/compositor/layer.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

CaptureWindowObserver::CaptureWindowObserver(
    CaptureModeSession* capture_mode_session)
    : capture_mode_session_(capture_mode_session) {
  Shell::Get()->activation_client()->AddObserver(this);
  DesksController::Get()->AddObserver(this);
}

CaptureWindowObserver::~CaptureWindowObserver() {
  DesksController::Get()->RemoveObserver(this);
  Shell::Get()->activation_client()->RemoveObserver(this);
  StopObserving();
}

void CaptureWindowObserver::UpdateSelectedWindowAtPosition(
    const gfx::Point& location_in_screen) {
  if (capture_mode_session_->IsInCountDownAnimation()) {
    return;
  }

  location_in_screen_ = location_in_screen;

  SetSelectedWindow(
      capture_mode_util::GetTopMostCapturableWindowAtPoint(location_in_screen),
      /*a11y_alert_again=*/false,
      /*bar_anchored_to_window=*/false);
  capture_mode_session_->UpdateCursor(location_in_screen, /*is_touch=*/false);
}

void CaptureWindowObserver::SetSelectedWindow(aura::Window* window,
                                              bool a11y_alert_again,
                                              bool bar_anchored_to_window) {
  if (window_ == window) {
    if (a11y_alert_again && window_) {
      capture_mode_session_->A11yAlertCaptureSource(/*trigger_now=*/true);
    }
    return;
  }

  if (window_ && bar_anchored_to_window_) {
    return;
  }

  bar_anchored_to_window_ = bar_anchored_to_window;

  // Don't capture wallpaper window.
  if (window && window->parent() &&
      window->parent()->GetId() == kShellWindowId_WallpaperContainer) {
    window = nullptr;
  }

  // Don't capture the shelf.
  if (window && window->parent() &&
      window->parent()->GetId() == kShellWindowId_ShelfContainer) {
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

  // The bounds of the capture bar should be updated accordingly if the bounds
  // of the selected window has been updated.
  if (bar_anchored_to_window_ &&
      capture_mode_session_->GetCaptureModeBarWidget()) {
    capture_mode_session_->RefreshBarWidgetBounds();
  }
}

void CaptureWindowObserver::OnWindowParentChanged(aura::Window* window,
                                                  aura::Window* parent) {
  if (!parent || !bar_anchored_to_window_) {
    return;
  }
  CHECK_EQ(window, window_);
  if (!desks_util::BelongsToActiveDesk(window)) {
    // Window has been sent to another desk, so we should stop recording.
    CaptureModeController::Get()->Stop();
    return;
  }
  // Move the capture mode widgets to the new root and repaint the capture
  // region when the window parent changes. E.g, `window_` is moved to another
  // display.
  capture_mode_session_->MaybeChangeRoot(window_->GetRootWindow(),
                                         /*root_window_will_shutdown=*/false);
  RepaintCaptureRegion();
}

void CaptureWindowObserver::OnWindowVisibilityChanging(aura::Window* window,
                                                       bool visible) {
  CHECK_EQ(window, window_);
  CHECK(!visible);
  StopObserving();

  if (bar_anchored_to_window_ ||
      capture_mode_session_->IsInCountDownAnimation()) {
    CaptureModeController::Get()->Stop();
    return;
  }

  UpdateSelectedWindowAtPosition(location_in_screen_);
}

void CaptureWindowObserver::OnWindowDestroying(aura::Window* window) {
  CHECK_EQ(window, window_);
  StopObserving();

  if (bar_anchored_to_window_ ||
      capture_mode_session_->IsInCountDownAnimation()) {
    CaptureModeController::Get()->Stop();
    return;
  }

  UpdateSelectedWindowAtPosition(location_in_screen_);
}

void CaptureWindowObserver::OnWindowActivated(ActivationReason reason,
                                              aura::Window* gained_active,
                                              aura::Window* lost_active) {
  // If another window is activated on top of the current selected window, we
  // may change the selected window to the activated window if it's under the
  // current event location. If there is no selected window at the moment, we
  // also want to check if new activated window should be focused.
  UpdateSelectedWindowAtPosition(location_in_screen_);
}

void CaptureWindowObserver::OnDeskActivationChanged(const Desk* activated,
                                                    const Desk* deactivated) {
  // When the desk switches and the window and bar are no longer visible, we
  // should stop the session.
  if (window_ && bar_anchored_to_window_ &&
      !desks_util::BelongsToActiveDesk(window_)) {
    CaptureModeController::Get()->Stop();
  }
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
