// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/mahi/mahi_panel_drag_controller.h"

#include "ash/system/mahi/mahi_ui_controller.h"
#include "base/check_deref.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"

namespace ash {

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
      previous_location_in_root_ = event->root_location();
      event->SetHandled();
      break;
    case ui::ET_MOUSE_DRAGGED:
    case ui::ET_GESTURE_SCROLL_UPDATE: {
      if (!is_dragging_) {
        break;
      }
      gfx::Rect panel_widget_bounds =
          mahi_panel_widget->GetWindowBoundsInScreen();
      // Offset the widget bounds by the difference between the current and
      // previous drag location. Since we are only concerned with the offset, it
      // is ok that the drag locations are in root window coordinates rather
      // than screen coordinates.
      panel_widget_bounds.Offset(event->root_location() -
                                 previous_location_in_root_);
      mahi_panel_widget->SetBoundsConstrained(panel_widget_bounds);
      previous_location_in_root_ = event->root_location();
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
