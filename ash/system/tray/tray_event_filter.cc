// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_event_filter.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/tray_bubble_base.h"
#include "ash/wm/container_finder.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ui/aura/window.h"
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
  if (was_empty && !bubbles_.empty())
    Shell::Get()->AddPreTargetHandler(this);
}

void TrayEventFilter::RemoveBubble(TrayBubbleBase* bubble) {
  bubbles_.erase(bubble);
  if (bubbles_.empty())
    Shell::Get()->RemovePreTargetHandler(this);
}

void TrayEventFilter::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() == ui::ET_MOUSE_PRESSED)
    ProcessPressedEvent(*event);
}

void TrayEventFilter::OnTouchEvent(ui::TouchEvent* event) {
  if (event->type() == ui::ET_TOUCH_PRESSED)
    ProcessPressedEvent(*event);
}

void TrayEventFilter::ProcessPressedEvent(const ui::LocatedEvent& event) {
  // The hit target window for the virtual keyboard isn't the same as its
  // views::Widget.
  aura::Window* target = static_cast<aura::Window*>(event.target());
  const views::Widget* target_widget =
      views::Widget::GetTopLevelWidgetForNativeView(target);
  const aura::Window* container =
      target ? wm::GetContainerForWindow(target) : nullptr;
  if (target && container) {
    const int container_id = container->id();
    // Don't process events that occurred inside an embedded menu, for example
    // the right-click menu in a popup notification.
    if (container_id == kShellWindowId_MenuContainer)
      return;
    // Don't process events that occurred inside a popup notification
    // from message center.
    if (container_id == kShellWindowId_StatusContainer &&
        target->type() == aura::client::WINDOW_TYPE_POPUP && target_widget &&
        target_widget->IsAlwaysOnTop()) {
      return;
    }
    // Don't process events that occurred inside a virtual keyboard.
    if (container_id == kShellWindowId_VirtualKeyboardContainer)
      return;
  }

  std::set<TrayBackgroundView*> trays;
  // Check the boundary for all bubbles, and do not handle the event if it
  // happens inside of any of those bubbles.
  const gfx::Point screen_location =
      event.target() ? event.target()->GetScreenLocation(event)
                     : event.root_location();
  for (const TrayBubbleBase* bubble : bubbles_) {
    const views::Widget* bubble_widget = bubble->GetBubbleWidget();
    if (!bubble_widget)
      continue;

    gfx::Rect bounds = bubble_widget->GetWindowBoundsInScreen();
    bounds.Inset(bubble->GetBubbleView()->GetBorderInsets());
    // System tray can be dragged to show the bubble if it is in tablet mode.
    // During the drag, the bubble's logical bounds can extend outside of the
    // work area, but its visual bounds are only within the work area. Restrict
    // |bounds| so that events located outside the bubble's visual bounds are
    // treated as outside of the bubble.
    int bubble_container_id =
        wm::GetContainerForWindow(bubble_widget->GetNativeWindow())->id();
    if (Shell::Get()
            ->tablet_mode_controller()
            ->IsTabletModeWindowManagerEnabled() &&
        bubble_container_id == kShellWindowId_SettingBubbleContainer) {
      bounds.Intersect(bubble_widget->GetWorkAreaBoundsInScreen());
    }
    if (bounds.Contains(screen_location))
      continue;
    if (bubble->GetTray()) {
      // If the user clicks on the parent tray, don't process the event here,
      // let the tray logic handle the event and determine show/hide behavior.
      bounds = bubble->GetTray()->GetBoundsInScreen();
      if (bounds.Contains(screen_location))
        continue;
    }
    trays.insert(bubble->GetTray());
  }

  // Close all bubbles other than the one that the user clicked on.
  for (TrayBackgroundView* tray_background_view : trays)
    tray_background_view->ClickedOutsideBubble();
}

}  // namespace ash
