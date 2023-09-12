// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_event_filter.h"

#include "ash/capture_mode/capture_mode_util.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/tray_background_view_catalog.h"
#include "ash/public/cpp/shell_window_ids.h"
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
#include "ash/wm/container_finder.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ui/aura/window.h"
#include "ui/display/screen.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/widget/widget.h"

namespace ash {

TrayEventFilter::TrayEventFilter(views::Widget* bubble_widget,
                                 TrayBubbleView* bubble_view,
                                 TrayBackgroundView* tray_button)
    : bubble_widget_(bubble_widget),
      bubble_view_(bubble_view),
      tray_button_(tray_button) {
  Shell::Get()->AddPreTargetHandler(this);
}

TrayEventFilter::~TrayEventFilter() {
  Shell::Get()->RemovePreTargetHandler(this);
}

void TrayEventFilter::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() == ui::ET_MOUSE_PRESSED) {
    ProcessPressedEvent(*event);
  }
}

void TrayEventFilter::OnTouchEvent(ui::TouchEvent* event) {
  if (event->type() == ui::ET_TOUCH_PRESSED) {
    ProcessPressedEvent(*event);
  }
}

void TrayEventFilter::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_SCROLL_BEGIN) {
    ProcessPressedEvent(*event);
  }
}

void TrayEventFilter::ProcessPressedEvent(const ui::LocatedEvent& event) {
  if (!bubble_widget_ || !bubble_view_ || !tray_button_) {
    return;
  }

  // Users in a capture session may be trying to capture tray bubble(s).
  if (capture_mode_util::IsCaptureModeActive()) {
    return;
  }

  // The hit target window for the virtual keyboard isn't the same as its
  // views::Widget.
  aura::Window* target = static_cast<aura::Window*>(event.target());
  const aura::Window* container =
      target ? GetContainerForWindow(target) : nullptr;
  // TODO(https://crbug.com/1208083): Replace some of this logic with
  // bubble_utils::ShouldCloseBubbleForEvent().
  if (target && container) {
    const int container_id = container->GetId();
    // Don't process events that occurred inside an embedded menu, for example
    // the right-click menu in a popup notification.
    if (container_id == kShellWindowId_MenuContainer) {
      return;
    }
    // Don't process events that occurred inside a virtual keyboard.
    if (container_id == kShellWindowId_VirtualKeyboardContainer) {
      return;
    }
  }

  // Check the boundary for all bubbles, and do not handle the event if it
  // happens inside of any of those bubbles.
  const gfx::Point screen_location =
      event.target() ? event.target()->GetScreenLocation(event)
                     : event.root_location();

  gfx::Rect bounds = bubble_widget_->GetWindowBoundsInScreen();
  bounds.Inset(bubble_view_->GetBorderInsets());

  int64_t display_id = display::Screen::GetScreen()
                           ->GetDisplayNearestPoint(screen_location)
                           .id();
  StatusAreaWidget* status_area =
      Shell::GetRootWindowControllerWithDisplayId(display_id)
          ->shelf()
          ->GetStatusAreaWidget();

  // When Quick Settings bubble is opened and the date tray is
  // clicked, the bubble should not be closed since it will transition
  // to show calendar.
  if (tray_button_->catalog_name() ==
          TrayBackgroundViewCatalogName::kUnifiedSystem &&
      status_area->date_tray()->GetBoundsInScreen().Contains(screen_location)) {
    return;
  }

  UnifiedSystemTray* unified_system_tray = status_area->unified_system_tray();

  // The system tray and message center are separate bubbles but they need to
  // stay open together. We need to make sure to check if a click falls with in
  // both their bounds and not close them both in this case.
  if (!features::IsQsRevampEnabled() &&
      tray_button_->catalog_name() ==
          TrayBackgroundViewCatalogName::kUnifiedSystem) {
    auto* system_tray_bubble = unified_system_tray->bubble();
    TrayBubbleBase* message_center_bubble =
        unified_system_tray->message_center_bubble();
    CHECK(message_center_bubble);
    if (bubble_widget_ == system_tray_bubble->GetBubbleWidget()) {
      bounds.Union(
          message_center_bubble->GetBubbleWidget()->GetWindowBoundsInScreen());
    } else {
      CHECK_EQ(bubble_widget_, message_center_bubble->GetBubbleWidget());
      bounds.Union(system_tray_bubble->GetBoundsInScreen());
    }
  }

  // If the bubble is anchored to the shelf corner, the notification
  // popup will be shown on top of that bubble. In that case, we
  // should not filter out the events happening on the popup
  // notification.
  if (features::IsNotifierCollisionEnabled() &&
      bubble_view_->IsAnchoredToShelfCorner() &&
      unified_system_tray->GetMessagePopupCollection()
          ->popup_collection_bounds()
          .Contains(screen_location)) {
    return;
  }

  if (bounds.Contains(screen_location)) {
    return;
  }

  // Maybe close the parent tray if the user drags on it. Otherwise, let the
  // tray logic handle the event and determine show/hide behavior if the
  // user clicks on the parent tray.
  bounds = tray_button_->GetBoundsInScreen();
  if (tray_button_->GetVisible() && bounds.Contains(screen_location) &&
      event.type() != ui::ET_GESTURE_SCROLL_BEGIN) {
    return;
  }

  tray_button_->ClickedOutsideBubble();
}

}  // namespace ash
