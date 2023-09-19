// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_event_filter.h"

#include "ash/bubble/bubble_event_filter.h"
#include "ash/bubble/bubble_utils.h"
#include "ash/capture_mode/capture_mode_util.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/tray_background_view_catalog.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/message_center/ash_message_popup_collection.h"
#include "ash/system/message_center/unified_message_center_bubble.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/tray_bubble_base.h"
#include "ash/system/unified/date_tray.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/functional/bind.h"
#include "ui/aura/window.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"

namespace ash {

TrayEventFilter::TrayEventFilter(views::Widget* bubble_widget,
                                 TrayBubbleView* bubble_view,
                                 TrayBackgroundView* tray_button)
    : BubbleEventFilter(bubble_widget,
                        tray_button,
                        base::BindRepeating(
                            [](TrayBackgroundView* tray_button) {
                              tray_button->ClickedOutsideBubble();
                            },
                            tray_button)),
      bubble_widget_(bubble_widget),
      bubble_view_(bubble_view),
      tray_button_(tray_button) {}

TrayEventFilter::~TrayEventFilter() = default;

void TrayEventFilter::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() != ui::ET_GESTURE_SCROLL_BEGIN) {
    return;
  }

  const gfx::Point event_location =
      event->target() ? event->target()->GetScreenLocation(*event)
                      : event->root_location();
  // If user is dragging on the tray button or
  // `ShouldRunOnClickOutsideCallback()` is satisfied, we should close the
  // bubble.
  if ((tray_button_->GetVisible() &&
       tray_button_->GetBoundsInScreen().Contains(event_location)) ||
      ShouldRunOnClickOutsideCallback(*event)) {
    tray_button_->ClickedOutsideBubble();
  }
}

bool TrayEventFilter::ShouldRunOnClickOutsideCallback(
    const ui::LocatedEvent& event) {
  if (!bubble_view_ || !tray_button_) {
    return false;
  }

  if (!BubbleEventFilter::ShouldRunOnClickOutsideCallback(event)) {
    return false;
  }

  const gfx::Point event_location =
      event.target() ? event.target()->GetScreenLocation(event)
                     : event.root_location();
  int64_t display_id =
      display::Screen::GetScreen()->GetDisplayNearestPoint(event_location).id();
  StatusAreaWidget* status_area =
      Shell::GetRootWindowControllerWithDisplayId(display_id)
          ->shelf()
          ->GetStatusAreaWidget();

  // When Quick Settings bubble is opened and the date tray is clicked, the
  // bubble should not be closed since it will transition to show calendar.
  if (tray_button_->catalog_name() ==
          TrayBackgroundViewCatalogName::kUnifiedSystem &&
      status_area->date_tray()->GetBoundsInScreen().Contains(event_location)) {
    return false;
  }

  return true;
}

}  // namespace ash
