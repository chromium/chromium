// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/system_panel_view.h"

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// The percentage of the panel which will be used as the buffer zone around the
// screen edges.
constexpr float kBufferRatio = 2.0 / 3.0;

}  // namespace

// -----------------------------------------------------------------------------
// SystemPanelView::DragController:
void SystemPanelView::DragController::OnLocatedPanelEvent(
    views::Widget* const widget,
    ui::LocatedEvent* event) {
  if (!widget) {
    is_dragging_ = false;
    base::debug::DumpWithoutCrashing();
    return;
  }

  switch (event->type()) {
    case ui::EventType::kMousePressed:
    case ui::EventType::kGestureScrollBegin:
      is_dragging_ = true;
      panel_widget_initial_bounds_ = widget->GetWindowBoundsInScreen();
      start_dragging_event_location_ =
          event->target()->GetScreenLocation(*event);
      event->SetHandled();
      break;
    case ui::EventType::kMouseDragged:
    case ui::EventType::kGestureScrollUpdate: {
      if (!is_dragging_) {
        break;
      }

      gfx::Point event_location = event->target()->GetScreenLocation(*event);
      gfx::Rect panel_widget_bounds = panel_widget_initial_bounds_;
      panel_widget_bounds.Offset(event_location -
                                 start_dragging_event_location_);

      // Establishes a buffer zone around the screen edges equal to
      // `kBufferRatio` of the height or width of the panel. This will prevent
      // the panel from being dragged too far that less than 1-`kBufferRatio` of
      // it remains visible on the screen.
      auto buff_width = panel_widget_bounds.width() * kBufferRatio;
      auto buff_height = panel_widget_bounds.height() * kBufferRatio;
      auto screen_bounds = display::Screen::GetScreen()
                               ->GetDisplayNearestPoint(event_location)
                               .bounds();
      screen_bounds.SetByBounds(
          screen_bounds.x() - buff_width, screen_bounds.y() - buff_height,
          screen_bounds.width() + screen_bounds.x() + buff_width,
          screen_bounds.y() + screen_bounds.height() + buff_height);
      panel_widget_bounds.AdjustToFit(screen_bounds);

      widget->SetBounds(panel_widget_bounds);
      event->SetHandled();
      break;
    }
    case ui::EventType::kMouseReleased:
    case ui::EventType::kGestureScrollEnd:
    case ui::EventType::kGestureEnd:
      if (!is_dragging_) {
        break;
      }
      is_dragging_ = false;
      event->SetHandled();
      break;
    default:
      break;
  }
}

// -----------------------------------------------------------------------------
// SystemPanelView:

SystemPanelView::SystemPanelView() = default;

SystemPanelView::~SystemPanelView() = default;

void SystemPanelView::OnMouseEvent(ui::MouseEvent* event) {
  HandleDragEventIfNeeded(event);
}

void SystemPanelView::OnGestureEvent(ui::GestureEvent* event) {
  HandleDragEventIfNeeded(event);
}

void SystemPanelView::HandleDragEventIfNeeded(ui::LocatedEvent* event) {
  // Checks whether the event is part of a drag sequence and handles it if
  // needed. Note that we only handle drag events for repositioning the panel
  // here. Other drag behavior, e.g. for text selection, is handled by the
  // panel's child views.
  drag_controller_.OnLocatedPanelEvent(GetWidget(), event);
}

BEGIN_METADATA(SystemPanelView)
END_METADATA

}  // namespace ash
