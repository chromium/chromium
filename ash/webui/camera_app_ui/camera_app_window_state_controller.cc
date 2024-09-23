// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/camera_app_ui/camera_app_window_state_controller.h"

#include <utility>

#include "base/containers/to_vector.h"
#include "ui/display/screen.h"

namespace ash {

namespace {

bool IsRestored(views::Widget* widget) {
  CHECK(widget);
  if (display::Screen::GetScreen()->InTabletMode()) {
    return !widget->IsMinimized();
  }
  return !widget->IsMinimized() && !widget->IsMaximized() &&
         !widget->IsFullscreen();
}

}  // namespace

CameraAppWindowStateController::CameraAppWindowStateController(
    views::Widget* widget)
    : widget_(widget) {
  window_states_ = GetCurrentWindowStates();
  if (widget_) {
    widget_->AddObserver(this);
  }
}

CameraAppWindowStateController::~CameraAppWindowStateController() {
  if (widget_) {
    widget_->RemoveObserver(this);
  }
}

void CameraAppWindowStateController::AddReceiver(
    mojo::PendingReceiver<camera_app::mojom::WindowStateController> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void CameraAppWindowStateController::AddMonitor(
    mojo::PendingRemote<camera_app::mojom::WindowStateMonitor> monitor,
    AddMonitorCallback callback) {
  auto remote =
      mojo::Remote<camera_app::mojom::WindowStateMonitor>(std::move(monitor));
  monitors_.push_back(std::move(remote));
  std::move(callback).Run(base::ToVector(window_states_));
}

void CameraAppWindowStateController::GetWindowState(
    GetWindowStateCallback callback) {
  std::move(callback).Run(base::ToVector(window_states_));
}

void CameraAppWindowStateController::Minimize(MinimizeCallback callback) {
  if (!widget_) {
    return;
  }
  if (widget_->IsMinimized()) {
    std::move(callback).Run();
    return;
  }
  minimize_callbacks_.push(std::move(callback));
  widget_->Minimize();
}

void CameraAppWindowStateController::Restore(RestoreCallback callback) {
  if (!widget_) {
    return;
  }
  if (IsRestored(widget_)) {
    std::move(callback).Run();
    return;
  }
  restore_callbacks_.push(std::move(callback));
  widget_->Restore();
}

void CameraAppWindowStateController::Maximize(MaximizeCallback callback) {
  if (!widget_) {
    return;
  }
  if (widget_->IsMaximized()) {
    std::move(callback).Run();
    return;
  }
  maximize_callbacks_.push(std::move(callback));
  widget_->Maximize();
}

void CameraAppWindowStateController::Fullscreen(FullscreenCallback callback) {
  if (!widget_) {
    return;
  }
  if (widget_->IsFullscreen()) {
    std::move(callback).Run();
    return;
  }
  fullscreen_callbacks_.push(std::move(callback));
  widget_->SetFullscreen(true);
}

void CameraAppWindowStateController::Focus(FocusCallback callback) {
  if (!widget_) {
    return;
  }
  if (widget_->IsActive()) {
    std::move(callback).Run();
    return;
  }
  focus_callbacks_.push(std::move(callback));
  widget_->Activate();
}

void CameraAppWindowStateController::OnWidgetVisibilityChanged(
    views::Widget* widget,
    bool visible) {
  OnWindowStateChanged();
}

void CameraAppWindowStateController::OnWidgetActivationChanged(
    views::Widget* widget,
    bool active) {
  while (!focus_callbacks_.empty()) {
    std::move(focus_callbacks_.front()).Run();
    focus_callbacks_.pop();
  }
  OnWindowFocusChanged(active);
}

void CameraAppWindowStateController::OnWidgetDestroying(views::Widget* widget) {
  widget_->RemoveObserver(this);
  widget_ = nullptr;
}

void CameraAppWindowStateController::OnWidgetBoundsChanged(
    views::Widget* widget,
    const gfx::Rect& new_bounds) {
  OnWindowStateChanged();
}

void CameraAppWindowStateController::OnWindowStateChanged() {
  CHECK(widget_);
  auto trigger_callbacks = [](std::queue<base::OnceClosure>* callbacks) {
    while (!callbacks->empty()) {
      std::move(callbacks->front()).Run();
      callbacks->pop();
    }
  };

  if (widget_->IsMinimized()) {
    trigger_callbacks(&minimize_callbacks_);
  }
  if (IsRestored(widget_)) {
    trigger_callbacks(&restore_callbacks_);
  }
  if (widget_->IsMaximized()) {
    trigger_callbacks(&maximize_callbacks_);
  }
  if (widget_->IsFullscreen()) {
    trigger_callbacks(&fullscreen_callbacks_);
  }

  auto prev_states = window_states_;
  window_states_ = GetCurrentWindowStates();
  if (prev_states != window_states_) {
    for (const auto& monitor : monitors_) {
      monitor->OnWindowStateChanged(base::ToVector(window_states_));
    }
  }
}

void CameraAppWindowStateController::OnWindowFocusChanged(bool is_focus) {
  for (const auto& monitor : monitors_) {
    monitor->OnWindowFocusChanged(is_focus);
  }
}

CameraAppWindowStateController::WindowStateTypeSet
CameraAppWindowStateController::GetCurrentWindowStates() const {
  if (!widget_) {
    return window_states_;
  }
  WindowStateTypeSet states;
  if (widget_->IsMinimized()) {
    states.Put(WindowStateType::kMinimized);
  }
  if (widget_->IsMaximized()) {
    states.Put(WindowStateType::kMaximized);
  }
  if (widget_->IsFullscreen()) {
    states.Put(WindowStateType::kFullscreen);
  }
  if (IsRestored(widget_)) {
    states.Put(WindowStateType::kRegular);
  }
  return states;
}

}  // namespace ash
