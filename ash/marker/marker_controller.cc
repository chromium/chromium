// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/marker/marker_controller.h"

#include "ash/highlighter/highlighter_view.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {
MarkerController* g_instance = nullptr;
}

MarkerController::MarkerController() {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;

  Shell::Get()->AddPreTargetHandler(this);
}

MarkerController::~MarkerController() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;

  Shell::Get()->RemovePreTargetHandler(this);
}

// static
MarkerController* MarkerController::Get() {
  return g_instance;
}

void MarkerController::AddObserver(MarkerObserver* observer) {
  observers_.AddObserver(observer);
}

void MarkerController::RemoveObserver(MarkerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void MarkerController::Clear() {
  DestroyMarkerView();
}

void MarkerController::UndoLastStroke() {
  if (GetMarkerView())
    GetMarkerView()->UndoLastStroke();
}

void MarkerController::ChangeColor(SkColor new_color) {
  marker_color_ = new_color;

  if (GetMarkerView())
    GetMarkerView()->ChangeColor(new_color);
}

void MarkerController::SetEnabled(bool enabled) {
  if (enabled == is_enabled())
    return;

  FastInkPointerController::SetEnabled(enabled);
  NotifyStateChanged(enabled);
}

void MarkerController::DestroyMarkerView() {
  marker_view_widget_.reset();
  marker_view_ = nullptr;
}

HighlighterView* MarkerController::GetMarkerView() {
  return marker_view_;
}

void MarkerController::NotifyStateChanged(bool enabled) {
  for (MarkerObserver& observer : observers_)
    observer.OnMarkerStateChanged(enabled);
}

views::View* MarkerController::GetPointerView() const {
  return marker_view_;
}

void MarkerController::CreatePointerView(base::TimeDelta presentation_delay,
                                         aura::Window* root_window) {
  marker_view_widget_ = HighlighterView::Create(
      presentation_delay,
      Shell::GetContainer(root_window, kShellWindowId_OverlayContainer));
  marker_view_ =
      static_cast<HighlighterView*>(marker_view_widget_->GetContentsView());
  marker_view_->ChangeColor(marker_color_);
}

void MarkerController::UpdatePointerView(ui::TouchEvent* event) {
  if (event->type() == ui::ET_TOUCH_RELEASED) {
    GetMarkerView()->AddGap();
    return;
  }

  GetMarkerView()->AddNewPoint(event->root_location_f(), event->time_stamp());
}

void MarkerController::UpdatePointerView(ui::MouseEvent* event) {
  if (!event->IsOnlyLeftMouseButton()) {
    return;
  }

  if (event->type() == ui::ET_MOUSE_RELEASED) {
    // Adds gap between strokes.
    GetMarkerView()->AddGap();
    return;
  }

  GetMarkerView()->AddNewPoint(event->root_location_f(), event->time_stamp());
}

void MarkerController::DestroyPointerView() {
  DestroyMarkerView();
}

bool MarkerController::CanStartNewGesture(ui::LocatedEvent* event) {
  // To preserve strokes, only start a new gesture when the pointer view is not
  // available.
  return !GetPointerView();
}

bool MarkerController::ShouldProcessEvent(ui::LocatedEvent* event) {
  if (IsPointerInExcludedWindows(event))
    return false;

  // Disable on mouse move event.
  if (event->type() == ui::ET_MOUSE_MOVED)
    return false;

  // Enable on mouse drag event.
  if (event->type() == ui::ET_MOUSE_DRAGGED)
    return true;

  return FastInkPointerController::ShouldProcessEvent(event);
}

}  // namespace ash
