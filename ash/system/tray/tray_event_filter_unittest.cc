// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_event_filter.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/message_center/unified_message_center_bubble.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/message_center/message_center.h"

using message_center::MessageCenter;
using message_center::Notification;

namespace ash {

namespace {

class TrayEventFilterTest : public AshTestBase {
 public:
  TrayEventFilterTest() = default;
  ~TrayEventFilterTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
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
  std::string AddNotification() {
    std::string notification_id = base::NumberToString(notification_id_++);
    MessageCenter::Get()->AddNotification(std::make_unique<Notification>(
        message_center::NOTIFICATION_TYPE_BASE_FORMAT, notification_id,
        base::UTF8ToUTF16("test title"), base::UTF8ToUTF16("test message"),
        gfx::Image(), base::string16() /* display_source */, GURL(),
        message_center::NotifierId(), message_center::RichNotificationData(),
        new message_center::NotificationDelegate()));
    return notification_id;
  }

  void EnableMessageCenterRefactor() {
    scoped_feature_list_->InitAndEnableFeature(
        features::kUnifiedMessageCenterRefactor);
  }

  void ShowSystemTrayMainView() {
    GetPrimaryUnifiedSystemTray()->ShowBubble(false /* show_by_click */);
  }

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
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
  DISALLOW_COPY_AND_ASSIGN(TrayEventFilterTest);
};

TEST_F(TrayEventFilterTest, ClickingOutsideCloseBubble) {
  ShowSystemTrayMainView();
  EXPECT_TRUE(IsBubbleShown());

  // Clicking outside should close the bubble.
  ui::MouseEvent event = outside_event();
  GetTrayEventFilter()->OnMouseEvent(&event);
  EXPECT_FALSE(IsBubbleShown());
}

TEST_F(TrayEventFilterTest, ClickingInsideDoesNotCloseBubble) {
  ShowSystemTrayMainView();
  EXPECT_TRUE(IsBubbleShown());

  // Clicking inside should not close the bubble
  ui::MouseEvent event = inside_event();
  GetTrayEventFilter()->OnMouseEvent(&event);
  EXPECT_TRUE(IsBubbleShown());
}

TEST_F(TrayEventFilterTest, ClickingOnMenuContainerDoesNotCloseBubble) {
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

TEST_F(TrayEventFilterTest, ClickingOnPopupDoesNotCloseBubble) {
  // Set up a popup window.
  std::unique_ptr<views::Widget> popup_widget =
      CreateTestWidget(nullptr, kShellWindowId_StatusContainer, gfx::Rect());
  std::unique_ptr<aura::Window> popup_window =
      CreateTestWindow(gfx::Rect(), aura::client::WINDOW_TYPE_POPUP);
  popup_window->set_owned_by_parent(false);
  popup_widget->GetNativeView()->AddChild(popup_window.get());
  popup_widget->GetNativeView()->SetProperty(aura::client::kZOrderingKey,
                                             ui::ZOrderLevel::kFloatingWindow);

  ShowSystemTrayMainView();
  EXPECT_TRUE(IsBubbleShown());

  // Clicking on StatusContainer should not close the bubble.
  ui::MouseEvent event = outside_event();
  ui::Event::DispatcherApi(&event).set_target(popup_window.get());
  GetTrayEventFilter()->OnMouseEvent(&event);
  EXPECT_TRUE(IsBubbleShown());
}

TEST_F(TrayEventFilterTest, ClickingOnKeyboardContainerDoesNotCloseBubble) {
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

TEST_F(TrayEventFilterTest, MessageCenterAndSystemTrayStayOpenTogether) {
  EnableMessageCenterRefactor();
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

TEST_F(TrayEventFilterTest, MessageCenterAndSystemTrayCloseTogether) {
  EnableMessageCenterRefactor();
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
