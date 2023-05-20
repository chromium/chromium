// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_event_filter.h"

#include "ash/constants/ash_features.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/message_center/ash_message_popup_collection.h"
#include "ash/system/message_center/unified_message_center_bubble.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/message_center/message_center.h"

using message_center::MessageCenter;
using message_center::Notification;

namespace ash {

namespace {

class TrayEventFilterTest : public AshTestBase,
                            public testing::WithParamInterface<bool> {
 public:
  TrayEventFilterTest() = default;

  TrayEventFilterTest(const TrayEventFilterTest&) = delete;
  TrayEventFilterTest& operator=(const TrayEventFilterTest&) = delete;

  ~TrayEventFilterTest() override = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatureState(features::kQsRevamp,
                                              /*enabled=*/IsQsRevampEnabed());
    AshTestBase::SetUp();
  }

  ui::MouseEvent outside_event() {
    const gfx::Rect tray_bounds = GetSystemTrayBoundsInScreen();
    const gfx::Point point = tray_bounds.bottom_right() + gfx::Vector2d(1, 1);
    const base::TimeTicks time = base::TimeTicks::Now();
    return ui::MouseEvent(ui::ET_MOUSE_PRESSED, point, point, time, 0, 0);
  }

  ui::MouseEvent inside_event() {
    const gfx::Rect tray_bounds = GetSystemTrayBoundsInScreen();
    const gfx::Point point = tray_bounds.origin();
    const base::TimeTicks time = base::TimeTicks::Now();
    return ui::MouseEvent(ui::ET_MOUSE_PRESSED, point, point, time, 0, 0);
  }

  ui::MouseEvent InsideMessageCenterEvent() {
    const gfx::Rect message_center_bounds = GetMessageCenterBoundsInScreen();
    const gfx::Point point = message_center_bounds.origin();
    const base::TimeTicks time = base::TimeTicks::Now();
    return ui::MouseEvent(ui::ET_MOUSE_PRESSED, point, point, time, 0, 0);
  }

 protected:
  bool IsQsRevampEnabed() { return GetParam(); }

  std::string AddNotification() {
    std::string notification_id = base::NumberToString(notification_id_++);
    MessageCenter::Get()->AddNotification(std::make_unique<Notification>(
        message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
        u"test title", u"test message", ui::ImageModel(),
        std::u16string() /* display_source */, GURL(),
        message_center::NotifierId(), message_center::RichNotificationData(),
        new message_center::NotificationDelegate()));
    return notification_id;
  }

  void ShowSystemTrayMainView() { GetPrimaryUnifiedSystemTray()->ShowBubble(); }

  bool IsBubbleShown() {
    return GetPrimaryUnifiedSystemTray()->IsBubbleShown();
  }

  bool IsMessageCenterBubbleShown() {
    return GetPrimaryUnifiedSystemTray()->IsMessageCenterBubbleShown();
  }

  gfx::Rect GetSystemTrayBoundsInScreen() {
    return GetPrimaryUnifiedSystemTray()->GetBubbleBoundsInScreen();
  }

  TrayEventFilter* GetTrayEventFilter() {
    return GetPrimaryUnifiedSystemTray()->tray_event_filter();
  }

  UnifiedSystemTray* GetPrimaryUnifiedSystemTray() {
    return GetPrimaryShelf()->GetStatusAreaWidget()->unified_system_tray();
  }

  UnifiedMessageCenterBubble* GetMessageCenterBubble() {
    return GetPrimaryUnifiedSystemTray()->message_center_bubble();
  }
  gfx::Rect GetMessageCenterBoundsInScreen() {
    return GetMessageCenterBubble()->GetBubbleView()->GetBoundsInScreen();
  }

 private:
  int notification_id_ = 0;
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(IsQsRevampEnabled,
                         TrayEventFilterTest,
                         testing::Bool());

TEST_P(TrayEventFilterTest, ClickingOutsideCloseBubble) {
  ShowSystemTrayMainView();
  EXPECT_TRUE(IsBubbleShown());

  // Clicking outside should close the bubble.
  ui::MouseEvent event = outside_event();
  GetTrayEventFilter()->OnMouseEvent(&event);
  EXPECT_FALSE(IsBubbleShown());
}

TEST_P(TrayEventFilterTest, ClickingInsideDoesNotCloseBubble) {
  ShowSystemTrayMainView();
  EXPECT_TRUE(IsBubbleShown());

  // Clicking inside should not close the bubble
  ui::MouseEvent event = inside_event();
  GetTrayEventFilter()->OnMouseEvent(&event);
  EXPECT_TRUE(IsBubbleShown());
}

TEST_P(TrayEventFilterTest, DraggingInsideDoesNotCloseBubble) {
  ShowSystemTrayMainView();
  EXPECT_TRUE(IsBubbleShown());

  // Dragging within the bubble should not close the bubble.
  const gfx::Rect tray_bounds = GetSystemTrayBoundsInScreen();
  const gfx::Point start = tray_bounds.origin();
  const gfx::Point end_inside = start + gfx::Vector2d(5, 5);
  GetEventGenerator()->GestureScrollSequence(start, end_inside,
                                             base::Milliseconds(100), 4);
  EXPECT_TRUE(IsBubbleShown());

  // Dragging from inside to outside of the bubble should not close the bubble.
  const gfx::Point start_inside = end_inside;
  const gfx::Point end_outside = start + gfx::Vector2d(-5, -5);
  GetEventGenerator()->GestureScrollSequence(start_inside, end_outside,
                                             base::Milliseconds(100), 4);
  EXPECT_TRUE(IsBubbleShown());
}

TEST_P(TrayEventFilterTest, ClickingOnMenuContainerDoesNotCloseBubble) {
  // Create a menu window and place it in the menu container window.
  std::unique_ptr<aura::Window> menu_window = CreateTestWindow();
  menu_window->set_owned_by_parent(false);
  Shell::GetPrimaryRootWindowController()
      ->GetContainer(kShellWindowId_MenuContainer)
      ->AddChild(menu_window.get());

  ShowSystemTrayMainView();
  EXPECT_TRUE(IsBubbleShown());

  // Clicking on MenuContainer should not close the bubble.
  ui::MouseEvent event = outside_event();
  ui::Event::DispatcherApi(&event).set_target(menu_window.get());
  GetTrayEventFilter()->OnMouseEvent(&event);
  EXPECT_TRUE(IsBubbleShown());
}

TEST_P(TrayEventFilterTest, ClickingOnPopupClosesBubble) {
  // Set up a popup window.
  auto popup_widget = std::make_unique<views::Widget>();
  views::Widget::InitParams popup_params;
  popup_params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  auto ash_message_center_popup_collection =
      std::make_unique<AshMessagePopupCollection>(GetPrimaryShelf());
  ash_message_center_popup_collection->ConfigureWidgetInitParamsForContainer(
      popup_widget.get(), &popup_params);
  popup_widget->Init(std::move(popup_params));

  std::unique_ptr<aura::Window> popup_window =
      CreateTestWindow(gfx::Rect(), aura::client::WINDOW_TYPE_POPUP);
  popup_window->set_owned_by_parent(false);
  popup_widget->GetNativeView()->AddChild(popup_window.get());
  popup_widget->GetNativeView()->SetProperty(aura::client::kZOrderingKey,
                                             ui::ZOrderLevel::kFloatingWindow);

  ShowSystemTrayMainView();
  EXPECT_TRUE(IsBubbleShown());

  // Clicking on the popup should close the bubble.
  ui::MouseEvent event = outside_event();
  ui::Event::DispatcherApi(&event).set_target(popup_window.get());
  GetTrayEventFilter()->OnMouseEvent(&event);
  EXPECT_FALSE(IsBubbleShown());
}

TEST_P(TrayEventFilterTest, ClickingOnKeyboardContainerDoesNotCloseBubble) {
  // Simulate the virtual keyboard being open. In production the virtual
  // keyboard container only exists while the keyboard is open.
  std::unique_ptr<aura::Window> keyboard_container =
      CreateTestWindow(gfx::Rect(), aura::client::WINDOW_TYPE_NORMAL,
                       kShellWindowId_VirtualKeyboardContainer);
  std::unique_ptr<aura::Window> keyboard_window = CreateTestWindow();
  keyboard_window->set_owned_by_parent(false);
  keyboard_container->AddChild(keyboard_window.get());

  ShowSystemTrayMainView();
  EXPECT_TRUE(IsBubbleShown());

  // Clicking on KeyboardContainer should not close the bubble.
  ui::MouseEvent event = outside_event();
  ui::Event::DispatcherApi(&event).set_target(keyboard_window.get());
  GetTrayEventFilter()->OnMouseEvent(&event);
  EXPECT_TRUE(IsBubbleShown());
}

TEST_P(TrayEventFilterTest, DraggingOnTrayClosesBubble) {
  ShowSystemTrayMainView();
  EXPECT_TRUE(IsBubbleShown());

  // Dragging on the tray background view should close the bubble.
  const gfx::Rect tray_bounds =
      GetPrimaryUnifiedSystemTray()->GetBoundsInScreen();
  const gfx::Point start = tray_bounds.CenterPoint();
  const gfx::Point end_inside = start + gfx::Vector2d(0, 10);
  GetEventGenerator()->GestureScrollSequence(start, end_inside,
                                             base::Milliseconds(100), 4);
  EXPECT_FALSE(IsBubbleShown());
}

using TrayEventFilterQsRevampDisabledTest = TrayEventFilterTest;

INSTANTIATE_TEST_SUITE_P(QsRevampDisabled,
                         TrayEventFilterQsRevampDisabledTest,
                         testing::Values(false));

TEST_P(TrayEventFilterQsRevampDisabledTest,
       MessageCenterAndSystemTrayStayOpenTogether) {
  AddNotification();

  ShowSystemTrayMainView();
  EXPECT_TRUE(GetMessageCenterBubble()->GetBubbleWidget()->IsVisible());
  EXPECT_TRUE(IsBubbleShown());

  // Clicking inside system tray should not close either bubble.
  ui::MouseEvent event = inside_event();
  GetTrayEventFilter()->OnMouseEvent(&event);
  EXPECT_TRUE(GetMessageCenterBubble()->GetBubbleWidget()->IsVisible());
  EXPECT_TRUE(IsBubbleShown());

  // Clicking inside the message center bubble should not close either bubble.
  event = InsideMessageCenterEvent();
  GetTrayEventFilter()->OnMouseEvent(&event);
  EXPECT_TRUE(GetMessageCenterBubble()->GetBubbleWidget()->IsVisible());
  EXPECT_TRUE(IsBubbleShown());
}

TEST_P(TrayEventFilterQsRevampDisabledTest,
       MessageCenterAndSystemTrayCloseTogether) {
  AddNotification();

  ShowSystemTrayMainView();
  EXPECT_TRUE(IsMessageCenterBubbleShown());
  EXPECT_TRUE(IsBubbleShown());

  // Clicking outside should close both bubbles.
  ui::MouseEvent event = outside_event();
  GetTrayEventFilter()->OnMouseEvent(&event);
  EXPECT_FALSE(IsMessageCenterBubbleShown());
  EXPECT_FALSE(IsBubbleShown());
}

}  // namespace
}  // namespace ash
