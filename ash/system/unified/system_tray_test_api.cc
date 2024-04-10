// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/system_tray_test_api.h"

#include <string>

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/accessibility/select_to_speak/select_to_speak_tray.h"
#include "ash/system/accessibility/unified_accessibility_detailed_view_controller.h"
#include "ash/system/notification_center/notification_center_tray.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/time/time_tray_item_view.h"
#include "ash/system/time/time_view.h"
#include "ash/system/tray/tray_detailed_view.h"
#include "ash/system/tray/tray_toggle_button.h"
#include "ash/system/unified/power_button.h"
#include "ash/system/unified/quick_settings_footer.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget_utils.h"

namespace {
ash::NotificationCenterTray* GetNotificationTray() {
  return ash::Shell::Get()
      ->GetPrimaryRootWindowController()
      ->GetStatusAreaWidget()
      ->notification_center_tray();
}

ash::UnifiedSystemTray* GetTray() {
  return ash::Shell::Get()
      ->GetPrimaryRootWindowController()
      ->GetStatusAreaWidget()
      ->unified_system_tray();
}

}  // namespace

namespace ash {

SystemTrayTestApi::SystemTrayTestApi() = default;

SystemTrayTestApi::~SystemTrayTestApi() = default;

bool SystemTrayTestApi::IsTrayBubbleOpen() {
  return GetTray()->IsBubbleShown();
}

void SystemTrayTestApi::ShowBubble() {
  GetTray()->ShowBubble();
}

void SystemTrayTestApi::CloseBubble() {
  GetTray()->CloseBubble();
}

void SystemTrayTestApi::ShowAccessibilityDetailedView() {
  GetTray()->ShowBubble();
  GetTray()->bubble_->controller_->ShowAccessibilityDetailedView();
}

void SystemTrayTestApi::ShowNetworkDetailedView() {
  GetTray()->ShowBubble();
  GetTray()->bubble_->controller_->ShowNetworkDetailedView();
}

AccessibilityDetailedView* SystemTrayTestApi::GetAccessibilityDetailedView() {
  auto* unified_system_tray_controller = GetTray()->bubble_->controller_.get();
  DCHECK(unified_system_tray_controller->IsDetailedViewShown());
  return static_cast<UnifiedAccessibilityDetailedViewController*>(
             unified_system_tray_controller->detailed_view_controller())
      ->accessibility_detailed_view_for_testing();
}

bool SystemTrayTestApi::IsBubbleViewVisible(int view_id, bool open_tray) {
  if (open_tray)
    GetTray()->ShowBubble();
  views::View* view = GetMainBubbleView()->GetViewByID(view_id);
  return view && view->GetVisible();
}

bool SystemTrayTestApi::IsToggleOn(int view_id) {
  auto* view =
      static_cast<TrayToggleButton*>(GetMainBubbleView()->GetViewByID(view_id));
  DCHECK(view);
  return view->GetIsOn();
}

void SystemTrayTestApi::ScrollToShowView(views::ScrollView* scroll_view,
                                         int view_id) {
  views::View* view = GetMainBubbleView()->GetViewByID(view_id);
  DCHECK(view && scroll_view->Contains(view));

  gfx::Point view_center = view->GetBoundsInScreen().CenterPoint();
  gfx::Rect scroll_bounds = scroll_view->GetBoundsInScreen();

  if (scroll_bounds.Contains(view_center.x(), view_center.y()))
    return;

  scroll_view->ScrollToPosition(scroll_view->vertical_scroll_bar(),
                                view_center.y() - scroll_bounds.y());
}

void SystemTrayTestApi::ClickBubbleView(int view_id) {
  views::View* view = GetMainBubbleView()->GetViewByID(view_id);
  if (view && view->GetVisible()) {
    gfx::Point cursor_location = view->GetLocalBounds().CenterPoint();
    views::View::ConvertPointToScreen(view, &cursor_location);

    ui::test::EventGenerator generator(GetRootWindow(view->GetWidget()));
    generator.MoveMouseTo(cursor_location);
    generator.ClickLeftButton();
  }
}

views::View* SystemTrayTestApi::GetMainBubbleView() {
  return GetTray()->bubble()->GetBubbleView();
}

std::u16string SystemTrayTestApi::GetBubbleViewTooltip(int view_id) {
  views::View* view = GetMainBubbleView()->GetViewByID(view_id);
  return view ? view->GetTooltipText(gfx::Point()) : std::u16string();
}

std::u16string SystemTrayTestApi::GetShutdownButtonTooltip() {
  // The power button view that has ID `VIEW_ID_QS_POWER_BUTTON` is not the view
  // that has the tooltip; what we're looking for is actually the child
  // `ash::IconButton` view.
  auto* icon_button = GetTray()
                          ->bubble()
                          ->quick_settings_view()
                          ->footer_for_testing()
                          ->power_button_for_testing()
                          ->button_content_for_testing();
  return icon_button ? icon_button->GetTooltipText(gfx::Point())
                     : std::u16string();
}

std::u16string SystemTrayTestApi::GetBubbleViewText(int view_id) {
  views::View* view = GetMainBubbleView()->GetViewByID(view_id);
  return view ? static_cast<views::Label*>(view)->GetText() : std::u16string();
}

bool SystemTrayTestApi::Is24HourClock() {
  base::HourClockType type =
      GetTray()->time_view_->time_view()->GetHourTypeForTesting();
  return type == base::k24HourClock;
}

void SystemTrayTestApi::TapSelectToSpeakTray() {
  StatusAreaWidget* status_area_widget =
      RootWindowController::ForWindow(GetTray()->GetWidget()->GetNativeWindow())
          ->GetStatusAreaWidget();
  ui::test::EventGenerator generator(GetRootWindow(status_area_widget));
  generator.MoveMouseTo(status_area_widget->select_to_speak_tray()
                            ->GetBoundsInScreen()
                            .CenterPoint());
  generator.ClickLeftButton();
}

message_center::MessagePopupView*
SystemTrayTestApi::GetPopupViewForNotificationID(
    const std::string& notification_id) {
  return GetNotificationTray()
      ->popup_collection()
      ->GetPopupViewForNotificationID(notification_id);
}

// static
std::unique_ptr<SystemTrayTestApi> SystemTrayTestApi::Create() {
  return std::make_unique<SystemTrayTestApi>();
}

}  // namespace ash
