// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_window_observer.h"

#include "ash/capture_mode/capture_mode_session.h"
#include "ash/home_screen/home_screen_controller.h"
#include "ash/home_screen/home_screen_delegate.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_finder.h"
#include "ash/shell.h"
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
  location_in_screen_ = location_in_screen;
  // Find the toplevel window under the mouse/touch position.
  aura::Window* window =
      GetTopmostWindowAtPoint(location_in_screen_, ignore_windows);
  if (window_ == window)
    return;

  // Don't capture wallpaper window.
  if (window && window->parent() &&
      window->parent()->id() == kShellWindowId_WallpaperContainer) {
    window = nullptr;
  }

  // Don't capture home screen window.
  if (window && window == Shell::Get()
                              ->home_screen_controller()
                              ->delegate()
                              ->GetHomeScreenWindow()) {
    window = nullptr;
  }

  // Stop observing the current selected window if there is one.
  StopObserving();
  if (window)
    StartObserving(window);
  RepaintCaptureRegion();
}

void CaptureWindowObserver::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  DCHECK_EQ(window, window_);
  RepaintCaptureRegion();
}

void CaptureWindowObserver::OnWindowVisibilityChanging(aura::Window* window,
                                                       bool visible) {
  DCHECK_EQ(window, window_);
  DCHECK(!visible);
  StopObserving();
  UpdateSelectedWindowAtPosition(location_in_screen_,
                                 /*ignore_windows=*/{window});
}

void CaptureWindowObserver::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(window, window_);
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
