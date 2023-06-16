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

TrayEventFilter::TrayEventFilter() = default;

TrayEventFilter::~TrayEventFilter() {
  DCHECK(bubbles_.empty());
}

void TrayEventFilter::AddBubble(TrayBubbleBase* bubble) {
  bool was_empty = bubbles_.empty();
  bubbles_.insert(bubble);
  if (was_empty && !bubbles_.empty()) {
    Shell::Get()->AddPreTargetHandler(this);
  }
}

void TrayEventFilter::RemoveBubble(TrayBubbleBase* bubble) {
  bubbles_.erase(bubble);
  if (bubbles_.empty()) {
    Shell::Get()->RemovePreTargetHandler(this);
  }
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
  // Users in a capture session may be trying to capture tray bubble(s).
  if (capture_mode_util::IsCaptureModeActive()) {
    return;
  }

  // The hit target window for the virtual keyboard isn't the same as its
  // views::Widget.
  aura::Window* target = static_cast<aura::Window*>(event.target());
  const views::Widget* target_widget =
      views::Widget::GetTopLevelWidgetForNativeView(target);
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
    // Don't process events that occurred inside a popup notification
    // from message center.
    if (container_id == kShellWindowId_ShelfContainer &&
        target->GetType() == aura::client::WINDOW_TYPE_POPUP &&
        target_widget->GetName() ==
            AshMessagePopupCollection::kMessagePopupWidgetName) {
      return;
    }
    // Don't process events that occurred inside a virtual keyboard.
    if (container_id == kShellWindowId_VirtualKeyboardContainer) {
      return;
    }
  }

  std::set<TrayBackgroundView*> trays;
  // Check the boundary for all bubbles, and do not handle the event if it
  // happens inside of any of those bubbles.
  const gfx::Point screen_location =
      event.target() ? event.target()->GetScreenLocation(event)
                     : event.root_location();
  for (const TrayBubbleBase* bubble : bubbles_) {
    const views::Widget* bubble_widget = bubble->GetBubbleWidget();
    if (!bubble_widget) {
      continue;
    }

    gfx::Rect bounds = bubble_widget->GetWindowBoundsInScreen();
    bounds.Inset(bubble->GetBubbleView()->GetBorderInsets());
    // System tray can be dragged to show the bubble if it is in tablet mode.
    // During the drag, the bubble's logical bounds can extend outside of the
    // work area, but its visual bounds are only within the work area. Restrict
    // |bounds| so that events located outside the bubble's visual bounds are
    // treated as outside of the bubble.
    int bubble_container_id =
        GetContainerForWindow(bubble_widget->GetNativeWindow())->GetId();
    if (Shell::Get()->tablet_mode_controller()->InTabletMode() &&
        bubble_container_id == kShellWindowId_SettingBubbleContainer) {
      bounds.Intersect(bubble_widget->GetWorkAreaBoundsInScreen());
    }

    TrayBackgroundView* tray = bubble->GetTray();

    if (bubble_container_id == kShellWindowId_SettingBubbleContainer) {
      int64_t display_id = display::Screen::GetScreen()
                               ->GetDisplayNearestPoint(screen_location)
                               .id();
      StatusAreaWidget* status_area =
          Shell::GetRootWindowControllerWithDisplayId(display_id)
              ->shelf()
              ->GetStatusAreaWidget();

      // When Quick Settings bubble is opened and the date tray is clicked, the
      // bubble should not be closed since it will transition to show calendar.
      if (tray->catalog_name() ==
              TrayBackgroundViewCatalogName::kUnifiedSystem &&
          status_area->date_tray()->GetBoundsInScreen().Contains(
              screen_location)) {
        continue;
      }

      // The system tray and message center are separate bubbles but they need
      // to stay open together. We need to make sure to check if a click falls
      // with in both their bounds and not close them both in this case.
      UnifiedSystemTray* unified_system_tray =
          status_area->unified_system_tray();
      TrayBubbleBase* system_tray_bubble = unified_system_tray->bubble();
      if (unified_system_tray->IsBubbleShown() &&
          system_tray_bubble != bubble) {
        bounds.Union(
            system_tray_bubble->GetBubbleWidget()->GetWindowBoundsInScreen());
      } else if (unified_system_tray->IsMessageCenterBubbleShown()) {
        TrayBubbleBase* message_center_bubble =
            unified_system_tray->message_center_bubble();
        bounds.Union(message_center_bubble->GetBubbleWidget()
                         ->GetWindowBoundsInScreen());
      }

      // If the bubble is anchored to the shelf corner, the notification popup
      // will be shown on top of that bubble. In that case, we should not filter
      // out the events happening on the popup notification.
      if (features::IsQsRevampEnabled() &&
          bubble->GetBubbleView()->IsAnchoredToShelfCorner() &&
          unified_system_tray->GetMessagePopupCollection()
              ->popup_collection_bounds()
              .Contains(screen_location)) {
        continue;
      }
    }

    if (bounds.Contains(screen_location)) {
      continue;
    }

    if (tray) {
      // Maybe close the parent tray if the user drags on it. Otherwise, let the
      // tray logic handle the event and determine show/hide behavior if the
      // user clicks on the parent tray.
      bounds = tray->GetBoundsInScreen();
      if (tray->GetVisible() && bounds.Contains(screen_location) &&
          event.type() != ui::ET_GESTURE_SCROLL_BEGIN) {
        continue;
      }
    }

    trays.insert(tray);
  }

  // Close all bubbles other than the one that the user clicked on.
  for (TrayBackgroundView* tray_background_view : trays) {
    tray_background_view->ClickedOutsideBubble();
  }
}

}  // namespace ash
