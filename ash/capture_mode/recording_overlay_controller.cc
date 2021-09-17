// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/recording_overlay_controller.h"

#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/public/cpp/capture_mode/recording_overlay_view.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer_type.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"

namespace ash {

RecordingOverlayController::RecordingOverlayController(
    aura::Window* window_being_recorded,
    const gfx::Rect& initial_bounds_in_parent) {
  DCHECK(window_being_recorded);
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.name = "RecordingOverlayWidget";
  // The overlay is added as a direct child of the window being recorded, and
  // stacked on top of all children. This is so that the overlay contents show
  // up in the recording above everything else.
  // TODO(crbug.com/1250768): Parent it differently when we start taking into
  // account the docked magnifier.
  params.child = true;
  params.parent = window_being_recorded;
  // The overlay hosts transparent contents so actual contents of the window
  // being recorded shows up underneath.
  params.layer_type = ui::LAYER_NOT_DRAWN;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.bounds = initial_bounds_in_parent;
  // The overlay window does not receive any events until it's shown and
  // enabled. See |Start()| below.
  params.activatable = views::Widget::InitParams::Activatable::kNo;
  params.accept_events = false;
  overlay_widget_->Init(std::move(params));
  recording_overlay_view_ = overlay_widget_->SetContentsView(
      CaptureModeController::Get()->CreateRecordingOverlayView());
  window_being_recorded->StackChildAtTop(overlay_widget_->GetNativeWindow());
}

void RecordingOverlayController::Toggle() {
  is_enabled_ = !is_enabled_;
  if (is_enabled_)
    Start();
  else
    Stop();
}

void RecordingOverlayController::SetBounds(const gfx::Rect& bounds_in_parent) {
  overlay_widget_->SetBounds(bounds_in_parent);
}

aura::Window* RecordingOverlayController::GetOverlayNativeWindow() {
  return overlay_widget_->GetNativeWindow();
}

void RecordingOverlayController::Start() {
  DCHECK(is_enabled_);

  overlay_widget_->GetNativeWindow()->SetEventTargetingPolicy(
      aura::EventTargetingPolicy::kTargetAndDescendants);
  overlay_widget_->Show();
}

void RecordingOverlayController::Stop() {
  DCHECK(!is_enabled_);

  overlay_widget_->GetNativeWindow()->SetEventTargetingPolicy(
      aura::EventTargetingPolicy::kNone);
  overlay_widget_->Hide();
}

}  // namespace ash
