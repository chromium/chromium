// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/notification_tray.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ash/system/message_center/ash_popup_alignment_delegate.h"
#include "ash/system/message_center/message_center_bubble.h"
#include "ash/system/message_center/message_center_ui_controller.h"
#include "ash/system/message_center/message_center_view.h"
#include "ash/system/screen_layout_observer.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_item.h"
#include "ash/system/tray/tray_container.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/display_manager_utilities.h"
#include "ui/display/screen.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification_list.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/views/message_popup_collection.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

NotificationTray* GetTray() {
  return StatusAreaWidgetTestHelper::GetStatusAreaWidget()->notification_tray();
}

NotificationTray* GetSecondaryTray() {
  StatusAreaWidget* status_area_widget =
      StatusAreaWidgetTestHelper::GetSecondaryStatusAreaWidget();
  if (status_area_widget)
    return status_area_widget->notification_tray();
  return NULL;
}

message_center::MessageCenter* GetMessageCenter() {
  return GetTray()->message_center();
}

SystemTray* GetSystemTray() {
  return StatusAreaWidgetTestHelper::GetStatusAreaWidget()->system_tray();
}

// Trivial item implementation for testing PopupAndSystemTray test case.
class TestItem : public SystemTrayItem {
 public:
  TestItem()
      : SystemTrayItem(GetSystemTray(), SystemTrayItemUmaType::UMA_TEST) {}

  views::View* CreateDefaultView(LoginStatus status) override {
    views::View* default_view = new views::View;
    default_view->SetLayoutManager(std::make_unique<views::FillLayout>());
    default_view->AddChildView(new views::Label(base::UTF8ToUTF16("Default")));
    return default_view;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestItem);
};

}  // namespace

class NotificationTrayTest : public AshTestBase {
 public:
  NotificationTrayTest() = default;
  ~NotificationTrayTest() override = default;

 protected:
  void AddNotification(const std::string& id) {
    std::unique_ptr<message_center::Notification> notification;
    notification.reset(new message_center::Notification(
        message_center::NOTIFICATION_TYPE_SIMPLE, id,
        base::ASCIIToUTF16("Test Web Notification"),
        base::ASCIIToUTF16("Notification message body."), gfx::Image(),
        base::ASCIIToUTF16("www.test.org"), GURL(),
        message_center::NotifierId(), message_center::RichNotificationData(),
        NULL /* delegate */));
    GetMessageCenter()->AddNotification(std::move(notification));
  }

  void UpdateNotification(const std::string& old_id,
                          const std::string& new_id) {
    std::unique_ptr<message_center::Notification> notification;
    notification.reset(new message_center::Notification(
        message_center::NOTIFICATION_TYPE_SIMPLE, new_id,
        base::ASCIIToUTF16("Updated Web Notification"),
        base::ASCIIToUTF16("Updated message body."), gfx::Image(),
        base::ASCIIToUTF16("www.test.org"), GURL(),
        message_center::NotifierId(), message_center::RichNotificationData(),
        NULL /* delegate */));
    GetMessageCenter()->UpdateNotification(old_id, std::move(notification));
  }

  void RemoveNotification(const std::string& id) {
    GetMessageCenter()->RemoveNotification(id, false);
  }

  views::Widget* GetWidget() { return GetTray()->GetWidget(); }

  int GetPopupWorkAreaBottom() {
    return GetPopupWorkAreaBottomForTray(GetTray());
  }

  int GetPopupWorkAreaBottomForTray(NotificationTray* tray) {
    return tray->popup_alignment_delegate_->GetWorkArea().bottom();
  }

  bool IsPopupVisible() { return GetTray()->IsPopupVisible(); }

  static std::unique_ptr<views::Widget> CreateTestWidget() {
    return AshTestBase::CreateTestWidget(
        nullptr, kShellWindowId_DefaultContainer, gfx::Rect(1, 2, 3, 4));
  }

  static void UpdateAutoHideStateNow() {
    GetPrimaryShelf()->shelf_layout_manager()->UpdateAutoHideStateNow();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NotificationTrayTest);
};

TEST_F(NotificationTrayTest, Notifications) {
  // TODO(tetsui): Remove the test after UnifiedSystemTray launch.
  // https://crbug.com/847104
  if (features::IsSystemTrayUnifiedEnabled())
    return;

  // TODO(mukai): move this test case to ui/message_center.
  ASSERT_TRUE(GetWidget());

  // Add a notification.
  AddNotification("test_id1");
  EXPECT_EQ(1u, GetMessageCenter()->NotificationCount());
  EXPECT_TRUE(GetMessageCenter()->FindVisibleNotificationById("test_id1"));
  AddNotification("test_id2");
  AddNotification("test_id2");
  EXPECT_EQ(2u, GetMessageCenter()->NotificationCount());
  EXPECT_TRUE(GetMessageCenter()->FindVisibleNotificationById("test_id2"));

  // Ensure that updating a notification does not affect the count.
  UpdateNotification("test_id2", "test_id3");
  UpdateNotification("test_id3", "test_id3");
  EXPECT_EQ(2u, GetMessageCenter()->NotificationCount());
  EXPECT_FALSE(GetMessageCenter()->FindVisibleNotificationById("test_id2"));

  // Ensure that Removing the first notification removes it from the tray.
  RemoveNotification("test_id1");
  EXPECT_FALSE(GetMessageCenter()->FindVisibleNotificationById("test_id1"));
  EXPECT_EQ(1u, GetMessageCenter()->NotificationCount());

  // Remove the remianing notification.
  RemoveNotification("test_id3");
  EXPECT_EQ(0u, GetMessageCenter()->NotificationCount());
  EXPECT_FALSE(GetMessageCenter()->FindVisibleNotificationById("test_id3"));
}

TEST_F(NotificationTrayTest, NotificationPopupBubble) {
  // TODO(tetsui): Remove the test after UnifiedSystemTray launch.
  // https://crbug.com/847104
  if (features::IsSystemTrayUnifiedEnabled())
    return;

  // TODO(mukai): move this test case to ui/message_center.
  ASSERT_TRUE(GetWidget());

  // Adding a notification should show the popup bubble.
  AddNotification("test_id1");
  EXPECT_TRUE(GetTray()->IsPopupVisible());

  // Updating a notification should not hide the popup bubble.
  AddNotification("test_id2");
  UpdateNotification("test_id2", "test_id3");
  EXPECT_TRUE(GetTray()->IsPopupVisible());

  // Removing the first notification should not hide the popup bubble.
  RemoveNotification("test_id1");
  EXPECT_TRUE(GetTray()->IsPopupVisible());

  // Removing the visible notification should hide the popup bubble.
  RemoveNotification("test_id3");
  EXPECT_FALSE(GetTray()->IsPopupVisible());

  // Now test that we can show multiple popups and then show the message center.
  AddNotification("test_id4");
  AddNotification("test_id5");
  EXPECT_TRUE(GetTray()->IsPopupVisible());

  GetTray()->message_center_ui_controller_->ShowMessageCenterBubble(
      false /* show_by_click */);
  GetTray()->message_center_ui_controller_->HideMessageCenterBubble();

  EXPECT_FALSE(GetTray()->IsPopupVisible());
}

using message_center::NotificationList;

TEST_F(NotificationTrayTest, ManyMessageCenterNotifications) {
  // TODO(tetsui): Remove the test after UnifiedSystemTray launch.
  // https://crbug.com/847104
  if (features::IsSystemTrayUnifiedEnabled())
    return;

  // Add the max visible notifications +1, ensure the correct visible number.
  size_t notifications_to_add = MessageCenterView::kMaxVisibleNotifications + 1;
  for (size_t i = 0; i < notifications_to_add; ++i) {
    std::string id = base::StringPrintf("test_id%d", static_cast<int>(i));
    AddNotification(id);
  }
  bool shown =
      GetTray()->message_center_ui_controller_->ShowMessageCenterBubble(
          false /* show_by_click */);
  EXPECT_TRUE(shown);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(GetTray()->message_center_bubble() != NULL);
  EXPECT_EQ(notifications_to_add, GetMessageCenter()->NotificationCount());
  EXPECT_EQ(
      MessageCenterView::kMaxVisibleNotifications,
      GetTray()->GetMessageCenterBubbleForTest()->NumMessageViewsForTest());
}

TEST_F(NotificationTrayTest, ManyPopupNotifications) {
  // TODO(tetsui): Remove the test after UnifiedSystemTray launch.
  // https://crbug.com/847104
  if (features::IsSystemTrayUnifiedEnabled())
    return;

  // Add the max visible popup notifications +1, ensure the correct num visible.
  size_t notifications_to_add =
      message_center::kMaxVisiblePopupNotifications + 1;
  for (size_t i = 0; i < notifications_to_add; ++i) {
    std::string id = base::StringPrintf("test_id%d", static_cast<int>(i));
    AddNotification(id);
  }
  GetTray()->ShowPopups();
  EXPECT_TRUE(GetTray()->IsPopupVisible());
  EXPECT_EQ(notifications_to_add, GetMessageCenter()->NotificationCount());
  NotificationList::PopupNotifications popups =
      GetMessageCenter()->GetPopupNotifications();
  EXPECT_EQ(message_center::kMaxVisiblePopupNotifications, popups.size());
}

// Verifies if the notification appears on both displays when extended mode.
TEST_F(NotificationTrayTest, PopupShownOnBothDisplays) {
  // TODO(tetsui): Remove the test after UnifiedSystemTray launch.
  // https://crbug.com/847104
  if (features::IsSystemTrayUnifiedEnabled())
    return;

  Shell::Get()->screen_layout_observer()->set_show_notifications_for_testing(
      true);

  const int64_t first_display_id =
      display::Screen::GetScreen()->GetPrimaryDisplay().id();
  const int64_t second_display_id = first_display_id + 1;
  display::ManagedDisplayInfo first_display_info =
      display::CreateDisplayInfo(first_display_id, gfx::Rect(1, 1, 500, 500));
  display::ManagedDisplayInfo second_display_info =
      display::CreateDisplayInfo(second_display_id, gfx::Rect(2, 2, 500, 500));
  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.emplace_back(first_display_info);
  display_info_list.emplace_back(second_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);

  // OnNativeDisplaysChanged() creates the display notifications, so popup is
  // visible.
  EXPECT_TRUE(GetTray()->IsPopupVisible());
  NotificationTray* secondary_tray = GetSecondaryTray();
  ASSERT_TRUE(secondary_tray);
  EXPECT_TRUE(secondary_tray->IsPopupVisible());

  // Transition to mirroring and then back to extended display, which recreates
  // root window controller and shelf with having notifications. This code
  // verifies it doesn't cause crash and popups are still visible. See
  // http://crbug.com/263664

  // Turn on mirror mode.
  display_manager()->SetMirrorMode(display::MirrorMode::kNormal, base::nullopt);
  EXPECT_TRUE(GetTray()->IsPopupVisible());
  EXPECT_FALSE(GetSecondaryTray());

  // Disconnect a display to end mirror mode.
  display_info_list.erase(display_info_list.end() - 1);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_TRUE(GetTray()->IsPopupVisible());
  EXPECT_FALSE(GetSecondaryTray());

  // Restore mirror mode.
  display_info_list.emplace_back(second_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_TRUE(GetTray()->IsPopupVisible());
  EXPECT_FALSE(GetSecondaryTray());

  // Turn off mirror mode.
  display_manager()->SetMirrorMode(display::MirrorMode::kOff, base::nullopt);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  secondary_tray = GetSecondaryTray();
  ASSERT_TRUE(secondary_tray);
  EXPECT_TRUE(secondary_tray->IsPopupVisible());
}

// PopupAndSystemTray may fail in platforms other than ChromeOS because the
// RootWindow's bound can be bigger than display::Display's work area so that
// openingsystem tray doesn't affect at all the work area of popups.
TEST_F(NotificationTrayTest, PopupAndSystemTray) {
  // TODO(tetsui): Remove the test after UnifiedSystemTray launch.
  // https://crbug.com/847104
  if (features::IsSystemTrayUnifiedEnabled())
    return;

  GetSystemTray()->AddTrayItem(std::make_unique<TestItem>());

  AddNotification("test_id");
  EXPECT_TRUE(GetTray()->IsPopupVisible());
  int bottom = GetPopupWorkAreaBottom();

  // System tray is created, the popup's work area should be narrowed but still
  // visible.
  GetSystemTray()->ShowDefaultView(BUBBLE_CREATE_NEW,
                                   false /* show_by_click */);
  EXPECT_TRUE(GetTray()->IsPopupVisible());
  int bottom_with_tray = GetPopupWorkAreaBottom();
  EXPECT_GT(bottom, bottom_with_tray);
}

TEST_F(NotificationTrayTest, PopupAndAutoHideShelf) {
  // TODO(tetsui): Remove the test after UnifiedSystemTray launch.
  // https://crbug.com/847104
  if (features::IsSystemTrayUnifiedEnabled())
    return;

  AddNotification("test_id");
  EXPECT_TRUE(GetTray()->IsPopupVisible());
  int bottom = GetPopupWorkAreaBottom();

  // Shelf's auto-hide state won't be HIDDEN unless window exists.
  std::unique_ptr<views::Widget> widget(CreateTestWidget());
  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);

  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
  int bottom_auto_hidden = GetPopupWorkAreaBottom();
  EXPECT_LT(bottom, bottom_auto_hidden);

  // Close the window, which shows the shelf.
  widget.reset();
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  int bottom_auto_shown = GetPopupWorkAreaBottom();
  EXPECT_EQ(bottom, bottom_auto_shown);

  // Create the system tray during auto-hide.
  widget = CreateTestWidget();
  GetSystemTray()->AddTrayItem(std::make_unique<TestItem>());
  GetSystemTray()->ShowDefaultView(BUBBLE_CREATE_NEW,
                                   false /* show_by_click */);
  UpdateAutoHideStateNow();

  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  EXPECT_TRUE(GetTray()->IsPopupVisible());
  int bottom_with_tray = GetPopupWorkAreaBottom();
  EXPECT_GT(bottom_auto_shown, bottom_with_tray);
}

TEST_F(NotificationTrayTest, PopupAndFullscreen) {
  // TODO(tetsui): Remove the test after UnifiedSystemTray launch.
  // https://crbug.com/847104
  if (features::IsSystemTrayUnifiedEnabled())
    return;

  AddNotification("test_id");
  EXPECT_TRUE(IsPopupVisible());
  int bottom = GetPopupWorkAreaBottom();

  // Checks the work area for normal auto-hidden state.
  std::unique_ptr<views::Widget> widget(CreateTestWidget());
  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
  int bottom_auto_hidden = GetPopupWorkAreaBottom();
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);

  // Put |widget| into fullscreen without forcing the shelf to hide. Currently,
  // this is used by immersive fullscreen and forces the shelf to be auto
  // hidden.
  wm::GetWindowState(widget->GetNativeWindow())
      ->SetHideShelfWhenFullscreen(false);
  widget->SetFullscreen(true);
  base::RunLoop().RunUntilIdle();

  // The work area for auto-hidden status of fullscreen is a bit larger
  // since it doesn't even have the 3-pixel width.
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
  int bottom_fullscreen_hidden = GetPopupWorkAreaBottom();
  EXPECT_EQ(bottom_auto_hidden, bottom_fullscreen_hidden);

  // Move the mouse cursor at the bottom, which shows the shelf.
  ui::test::EventGenerator* generator = GetEventGenerator();
  gfx::Point bottom_right =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds().bottom_right();
  bottom_right.Offset(-1, -1);
  generator->MoveMouseTo(bottom_right);
  shelf->UpdateVisibilityState();
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  EXPECT_EQ(bottom, GetPopupWorkAreaBottom());

  generator->MoveMouseTo(
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds().CenterPoint());
  shelf->UpdateVisibilityState();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
  EXPECT_EQ(bottom_auto_hidden, GetPopupWorkAreaBottom());
}

TEST_F(NotificationTrayTest, PopupAndSystemTrayMultiDisplay) {
  // TODO(tetsui): Remove the test after UnifiedSystemTray launch.
  // https://crbug.com/847104
  if (features::IsSystemTrayUnifiedEnabled())
    return;

  UpdateDisplay("800x600,600x400");

  AddNotification("test_id");
  int bottom = GetPopupWorkAreaBottom();
  int bottom_second = GetPopupWorkAreaBottomForTray(GetSecondaryTray());

  // System tray is created on the primary display. The popups in the secondary
  // tray aren't affected.
  GetSystemTray()->ShowDefaultView(BUBBLE_CREATE_NEW,
                                   false /* show_by_click */);
  EXPECT_GT(bottom, GetPopupWorkAreaBottom());
  EXPECT_EQ(bottom_second, GetPopupWorkAreaBottomForTray(GetSecondaryTray()));
}

TEST_F(NotificationTrayTest, VisibleSmallIcon) {
  // TODO(tetsui): Remove the test after UnifiedSystemTray launch.
  // https://crbug.com/847104
  if (features::IsSystemTrayUnifiedEnabled())
    return;

  EXPECT_EQ(0u, GetTray()->visible_small_icons_.size());
  EXPECT_EQ(3, GetTray()->tray_container()->child_count());
  std::unique_ptr<message_center::Notification> notification =
      std::make_unique<message_center::Notification>(
          message_center::NOTIFICATION_TYPE_SIMPLE, "test",
          base::ASCIIToUTF16("Test System Notification"),
          base::ASCIIToUTF16("Notification message body."), gfx::Image(),
          base::ASCIIToUTF16("system"), GURL(),
          message_center::NotifierId(
              message_center::NotifierId::NotifierType::SYSTEM_COMPONENT,
              "test"),
          message_center::RichNotificationData(), nullptr /* delegate */);
  notification->set_small_image(gfx::test::CreateImage(18, 18));
  GetMessageCenter()->AddNotification(std::move(notification));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, GetTray()->visible_small_icons_.size());
  EXPECT_EQ(4, GetTray()->tray_container()->child_count());
}

TEST_F(NotificationTrayTest, QuietModeIcon) {
  // TODO(tetsui): Remove the test after UnifiedSystemTray launch.
  // https://crbug.com/847104
  if (features::IsSystemTrayUnifiedEnabled())
    return;

  NotificationTray::DisableAnimationsForTest(true);

  AddNotification("test");
  base::RunLoop().RunUntilIdle();

  // There is a notification, so no bell & quiet mode icons are shown.
  EXPECT_FALSE(GetTray()->bell_icon_->visible());
  EXPECT_FALSE(GetTray()->quiet_mode_icon_->visible());

  GetMessageCenter()->SetQuietMode(true);
  base::RunLoop().RunUntilIdle();

  // If there is a notification, setting quiet mode shouldn't change tray icons.
  EXPECT_FALSE(GetTray()->bell_icon_->visible());
  EXPECT_FALSE(GetTray()->quiet_mode_icon_->visible());

  GetMessageCenter()->SetQuietMode(false);
  GetMessageCenter()->RemoveAllNotifications(
      false /* by_user */, message_center::MessageCenter::RemoveType::ALL);
  base::RunLoop().RunUntilIdle();

  // If there is no notification, bell icon should be shown.
  EXPECT_TRUE(GetTray()->bell_icon_->visible());
  EXPECT_FALSE(GetTray()->quiet_mode_icon_->visible());

  GetMessageCenter()->SetQuietMode(true);
  base::RunLoop().RunUntilIdle();

  // If there is no notification and quiet mode is set, it should show quiet
  // mode icon.
  EXPECT_FALSE(GetTray()->bell_icon_->visible());
  EXPECT_TRUE(GetTray()->quiet_mode_icon_->visible());

  NotificationTray::DisableAnimationsForTest(false);
}

// Makes sure that the system tray bubble closes when another window is
// activated, and does not crash regardless of the initial activation state.
TEST_F(NotificationTrayTest, CloseOnActivation) {
  // TODO(tetsui): Remove the test after UnifiedSystemTray launch.
  // https://crbug.com/847104
  if (features::IsSystemTrayUnifiedEnabled())
    return;

  NotificationTray* tray = GetTray();

  // Show the notification bubble.
  tray->ShowBubble(false /* show_by_click */);
  EXPECT_FALSE(tray->GetBubbleView()->GetWidget()->IsActive());

  // Test 1: no crash when there's no active window to begin with.
  EXPECT_FALSE(wm::GetActiveWindow());

  // Showing a new window and activating it will close the system bubble.
  std::unique_ptr<views::Widget> widget(CreateTestWidget());
  EXPECT_TRUE(widget->IsActive());
  EXPECT_FALSE(tray->message_center_bubble());

  // Wait for bubble to actually close.
  base::RunLoop().RunUntilIdle();

  // Show a second widget.
  std::unique_ptr<views::Widget> second_widget(CreateTestWidget());
  EXPECT_TRUE(second_widget->IsActive());

  // Re-show the system bubble.
  tray->ShowBubble(false /* show_by_click */);
  EXPECT_FALSE(tray->GetBubbleView()->GetWidget()->IsActive());

  // Test 2: also no crash when there is a previously active window.
  EXPECT_TRUE(wm::GetActiveWindow());

  // Re-activate the first widget. The system bubble should hide again.
  widget->Activate();
  EXPECT_FALSE(tray->message_center_bubble());
}

}  // namespace ash
