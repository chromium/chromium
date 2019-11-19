// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/unified_system_tray_test_api.h"

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/time/time_tray_item_view.h"
#include "ash/system/time/time_view.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "base/strings/string16.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget_utils.h"

namespace ash {

UnifiedSystemTrayTestApi::UnifiedSystemTrayTestApi(UnifiedSystemTray* tray)
    : tray_(tray) {}

UnifiedSystemTrayTestApi::~UnifiedSystemTrayTestApi() = default;

void UnifiedSystemTrayTestApi::DisableAnimations() {
  disable_animations_ = std::make_unique<ui::ScopedAnimationDurationScaleMode>(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
}

bool UnifiedSystemTrayTestApi::IsTrayBubbleOpen() {
  return tray_->IsBubbleShown();
}

void UnifiedSystemTrayTestApi::ShowBubble() {
  tray_->ShowBubble(false /* show_by_click */);
}

void UnifiedSystemTrayTestApi::CloseBubble() {
  tray_->CloseBubble();
}

void UnifiedSystemTrayTestApi::ShowAccessibilityDetailedView() {
  tray_->ShowBubble(false /* show_by_click */);
  tray_->bubble_->controller_->ShowAccessibilityDetailedView();
}

void UnifiedSystemTrayTestApi::ShowNetworkDetailedView() {
  tray_->ShowBubble(false /* show_by_click */);
  tray_->bubble_->controller_->ShowNetworkDetailedView(true /* force */);
}

bool UnifiedSystemTrayTestApi::IsBubbleViewVisible(int view_id,
                                                   bool open_tray) {
  if (open_tray)
    tray_->ShowBubble(false /* show_by_click */);
  views::View* view = GetBubbleView(view_id);
  return view && view->GetVisible();
}

void UnifiedSystemTrayTestApi::ClickBubbleView(int view_id) {
  views::View* view = GetBubbleView(view_id);
  if (view && view->GetVisible()) {
    gfx::Point cursor_location = view->GetLocalBounds().CenterPoint();
    views::View::ConvertPointToScreen(view, &cursor_location);

    ui::test::EventGenerator generator(GetRootWindow(view->GetWidget()));
    generator.MoveMouseTo(cursor_location);
    generator.ClickLeftButton();
  }
}

base::string16 UnifiedSystemTrayTestApi::GetBubbleViewTooltip(int view_id) {
  views::View* view = GetBubbleView(view_id);
  return view ? view->GetTooltipText(gfx::Point()) : base::string16();
}

bool UnifiedSystemTrayTestApi::Is24HourClock() {
  base::HourClockType type =
      tray_->time_view_->time_view()->GetHourTypeForTesting();
  return type == base::k24HourClock;
}

message_center::MessagePopupView*
UnifiedSystemTrayTestApi::GetPopupViewForNotificationID(
    const std::string& notification_id) {
  return tray_->GetPopupViewForNotificationID(notification_id);
}

views::View* UnifiedSystemTrayTestApi::GetBubbleView(int view_id) const {
  return tray_->bubble_->bubble_view_->GetViewByID(view_id);
}

// static
std::unique_ptr<SystemTrayTestApi> SystemTrayTestApi::Create() {
  UnifiedSystemTray* primary_tray = Shell::Get()
                                        ->GetPrimaryRootWindowController()
                                        ->GetStatusAreaWidget()
                                        ->unified_system_tray();
  return std::make_unique<UnifiedSystemTrayTestApi>(primary_tray);
}

}  // namespace ash
