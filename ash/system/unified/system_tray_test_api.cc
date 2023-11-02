// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/system_tray_test_api.h"

#include <string>

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/accessibility/select_to_speak/select_to_speak_tray.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/time/time_tray_item_view.h"
#include "ash/system/time/time_view.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget_utils.h"

namespace {
ash::UnifiedSystemTray* GetTray() {
  return ash::Shell::Get()
      ->GetPrimaryRootWindowController()
      ->GetStatusAreaWidget()
      ->unified_system_tray();
}

views::View* GetBubbleView(int view_id) {
  return GetTray()->bubble()->GetBubbleView()->GetViewByID(view_id);
}

}  // namespace

namespace ash {

SystemTrayTestApi::SystemTrayTestApi() = default;

SystemTrayTestApi::~SystemTrayTestApi() = default;

bool SystemTrayTestApi::IsTrayBubbleOpen() {
  return GetTray()->IsBubbleShown();
}

bool SystemTrayTestApi::IsTrayBubbleExpanded() {
  return GetTray()->bubble_->controller_->IsExpanded();
}

void SystemTrayTestApi::ShowBubble() {
  GetTray()->ShowBubble();
}

void SystemTrayTestApi::CloseBubble() {
  GetTray()->CloseBubble();
}

void SystemTrayTestApi::CollapseBubble() {
  GetTray()->EnsureQuickSettingsCollapsed(true /*animate*/);
}

void SystemTrayTestApi::ExpandBubble() {
  GetTray()->EnsureBubbleExpanded();
}

void SystemTrayTestApi::ShowAccessibilityDetailedView() {
  GetTray()->ShowBubble();
  GetTray()->bubble_->controller_->ShowAccessibilityDetailedView();
}

void SystemTrayTestApi::ShowNetworkDetailedView() {
  GetTray()->ShowBubble();
  GetTray()->bubble_->controller_->ShowNetworkDetailedView(true /* force */);
}

bool SystemTrayTestApi::IsBubbleViewVisible(int view_id, bool open_tray) {
  if (open_tray)
    GetTray()->ShowBubble();
  views::View* view = GetBubbleView(view_id);
  return view && view->GetVisible();
}

void SystemTrayTestApi::ScrollToShowView(int view_id) {
  views::View* view = GetBubbleView(view_id);
  DCHECK(view);

  views::View* contents = view->parent();
  DCHECK(contents);

  views::ScrollView* scroll_view =
      views::ScrollView::GetScrollViewForContents(contents);
  DCHECK(scroll_view);

  gfx::Point view_center = view->GetBoundsInScreen().CenterPoint();
  gfx::Rect scroll_bounds = scroll_view->GetBoundsInScreen();

  if (scroll_bounds.Contains(view_center.x(), view_center.y()))
    return;

  scroll_view->ScrollToPosition(scroll_view->vertical_scroll_bar(),
                                view_center.y() - scroll_bounds.y());
}

void SystemTrayTestApi::ClickBubbleView(int view_id) {
  views::View* view = GetBubbleView(view_id);
  if (view && view->GetVisible()) {
    gfx::Point cursor_location = view->GetLocalBounds().CenterPoint();
    views::View::ConvertPointToScreen(view, &cursor_location);

    ui::test::EventGenerator generator(GetRootWindow(view->GetWidget()));
    generator.MoveMouseTo(cursor_location);
    generator.ClickLeftButton();
  }
}

std::u16string SystemTrayTestApi::GetBubbleViewTooltip(int view_id) {
  views::View* view = GetBubbleView(view_id);
  return view ? view->GetTooltipText(gfx::Point()) : std::u16string();
}

std::u16string SystemTrayTestApi::GetBubbleViewText(int view_id) {
  views::View* view = GetBubbleView(view_id);
  return view ? static_cast<views::Label*>(view)->GetText() : std::u16string();
}

bool SystemTrayTestApi::Is24HourClock() {
  base::HourClockType type =
      GetTray()->time_view_->time_view()->GetHourTypeForTesting();
  return type == base::k24HourClock;
}

void SystemTrayTestApi::TapSelectToSpeakTray() {
  // The Select-to-Speak tray doesn't actually use the event, so construct
  // a bare bones event to perform the action.
  ui::TouchEvent event(ui::ET_TOUCH_PRESSED, gfx::Point(),
                       base::TimeTicks::Now(),
                       ui::PointerDetails(ui::EventPointerType::kTouch), 0);
  StatusAreaWidget* status_area_widget =
      RootWindowController::ForWindow(GetTray()->GetWidget()->GetNativeWindow())
          ->GetStatusAreaWidget();
  status_area_widget->select_to_speak_tray()->PerformAction(event);
}

message_center::MessagePopupView*
SystemTrayTestApi::GetPopupViewForNotificationID(
    const std::string& notification_id) {
  return GetTray()->GetPopupViewForNotificationID(notification_id);
}

// static
std::unique_ptr<SystemTrayTestApi> SystemTrayTestApi::Create() {
  return std::make_unique<SystemTrayTestApi>();
}

}  // namespace ash
