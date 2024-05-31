// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/mahi/mahi_panel_drag_controller.h"

#include "ash/system/mahi/mahi_ui_controller.h"
#include "base/check_deref.h"
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

MahiPanelDragController::MahiPanelDragController(
    MahiUiController* ui_controller)
    : ui_controller_(CHECK_DEREF(ui_controller)) {}

MahiPanelDragController::~MahiPanelDragController() = default;

void MahiPanelDragController::OnLocatedPanelEvent(ui::LocatedEvent* event) {
  views::Widget* mahi_panel_widget = ui_controller_->mahi_panel_widget();
  if (mahi_panel_widget == nullptr) {
    is_dragging_ = false;
    return;
  }

  switch (event->type()) {
    case ui::ET_MOUSE_PRESSED:
    case ui::ET_GESTURE_SCROLL_BEGIN:
      is_dragging_ = true;
      panel_widget_initial_bounds_ =
          mahi_panel_widget->GetWindowBoundsInScreen();
      start_dragging_event_location_ =
          event->target()->GetScreenLocation(*event);
      event->SetHandled();
      break;
    case ui::ET_MOUSE_DRAGGED:
    case ui::ET_GESTURE_SCROLL_UPDATE: {
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

      mahi_panel_widget->SetBounds(panel_widget_bounds);
      event->SetHandled();
      break;
    }
    case ui::ET_MOUSE_RELEASED:
    case ui::ET_GESTURE_SCROLL_END:
    case ui::ET_GESTURE_END:
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

}  // namespace ash
