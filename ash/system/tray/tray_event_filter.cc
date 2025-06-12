// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_event_filter.h"

#include "ash/bubble/bubble_event_filter.h"
#include "ash/shell.h"
#include "ash/system/notification_center/ash_message_popup_collection.h"
#include "ash/system/notification_center/notification_center_tray.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/unified/unified_system_tray.h"
#include "base/functional/bind.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

TrayEventFilter::TrayEventFilter(views::Widget* bubble_widget,
                                 TrayBubbleView* bubble_view,
                                 TrayBackgroundView* tray_button)
    : BubbleEventFilter(bubble_widget,
                        tray_button,
                        base::BindRepeating(
                            [](TrayBackgroundView* tray_button,
                               const ui::LocatedEvent& event) {
                              tray_button->ClickedOutsideBubble(event);
                            },
                            tray_button)),
      bubble_widget_(bubble_widget),
      bubble_view_(bubble_view),
      tray_button_(tray_button) {
  Shell::Get()->activation_client()->AddObserver(this);
}

TrayEventFilter::~TrayEventFilter() {
  Shell::Get()->activation_client()->RemoveObserver(this);
}

void TrayEventFilter::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() != ui::EventType::kGestureScrollBegin) {
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
    tray_button_->ClickedOutsideBubble(*event);
  }
}

bool TrayEventFilter::ShouldRunOnClickOutsideCallback(
    const ui::LocatedEvent& event) {
  if (!bubble_view_ || !tray_button_) {
    return false;
  }

  return BubbleEventFilter::ShouldRunOnClickOutsideCallback(event);
}

void TrayEventFilter::OnWindowActivated(ActivationReason reason,
                                        aura::Window* gained_active,
                                        aura::Window* lost_active) {
  if (!gained_active) {
    return;
  }

  // Check for the CloseBubble() lock.
  if (!TrayBackgroundView::ShouldCloseBubbleOnWindowActivated()) {
    return;
  }

  aura::Window* const bubble_window = bubble_widget_->GetNativeWindow();

  // Bail if `gained_active` is our own bubble, or on another display.
  if (bubble_window == gained_active ||
      bubble_window->GetRootWindow() != gained_active->GetRootWindow()) {
    return;
  }

  // Don't close the bubble if a transient child is gaining or losing
  // activation (i.e. b/303382616: the network info bubble is a transcient child
  // of the QS bubble and activating it should not close the QS bubble).
  if (::wm::HasTransientAncestor(gained_active, bubble_window) ||
      (lost_active && ::wm::HasTransientAncestor(lost_active, bubble_window))) {
    return;
  }

  // If the activated window is a popup notification, interacting with it
  // should not close the bubble.
  if (StatusAreaWidget::ForWindow(bubble_window)
          ->notification_center_tray()
          ->popup_collection()
          ->IsWidgetAPopupNotification(
              views::Widget::GetWidgetForNativeView(gained_active))) {
    return;
  }

  tray_button_->CloseBubble(TrayBackgroundView::CloseReason::kWindowActivation);
}

}  // namespace ash
