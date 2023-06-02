// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/unified_message_center_bubble.h"

#include <memory>

#include "ash/constants/ash_pref_names.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/notification_center/notification_center_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/system/unified/unified_system_tray_view.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "components/prefs/pref_service.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/message_center/message_center.h"

using message_center::MessageCenter;
using message_center::Notification;

namespace ash {

class UnifiedMessageCenterBubbleTest : public AshTestBase {
 public:
  UnifiedMessageCenterBubbleTest() {
    // `UnifiedMessageCenterBubble` is only used when the QS revamp is disabled.
    scoped_feature_list_.InitAndDisableFeature(features::kQsRevamp);
  }

  UnifiedMessageCenterBubbleTest(const UnifiedMessageCenterBubbleTest&) =
      delete;
  UnifiedMessageCenterBubbleTest& operator=(
      const UnifiedMessageCenterBubbleTest&) = delete;

  ~UnifiedMessageCenterBubbleTest() override = default;

 protected:
  std::string AddWebNotification() {
    std::string id = base::NumberToString(id_++);
    MessageCenter::Get()->AddNotification(std::make_unique<Notification>(
        message_center::NOTIFICATION_TYPE_SIMPLE, id, u"title", u"message",
        ui::ImageModel(), std::u16string(), GURL(),
        message_center::NotifierId(GURL(u"example.com"), u"webpagetitle"),
        message_center::RichNotificationData(), /*delegate=*/nullptr));
    return id;
  }

  std::string AddNotification() {
    std::string id = base::NumberToString(id_++);
    MessageCenter::Get()->AddNotification(std::make_unique<Notification>(
        message_center::NOTIFICATION_TYPE_SIMPLE, id, u"test title",
        u"test message", ui::ImageModel(), std::u16string(), GURL(),
        message_center::NotifierId(), message_center::RichNotificationData(),
        new message_center::NotificationDelegate()));
    return id;
  }

  void RemoveAllNotifications() {
    message_center::MessageCenter::Get()->RemoveAllNotifications(
        /*by_user=*/true, MessageCenter::RemoveType::ALL);
    GetMessageCenterBubble()
        ->notification_center_view()
        ->notification_list_view()
        ->ResetBounds();
  }

  UnifiedSystemTray* GetSecondaryUnifiedSystemTray() {
    return Shell::Get()
        ->GetRootWindowControllerWithDisplayId(GetSecondaryDisplay().id())
        ->shelf()
        ->status_area_widget()
        ->unified_system_tray();
  }

  UnifiedMessageCenterBubble* GetMessageCenterBubble() {
    return GetPrimaryUnifiedSystemTray()->message_center_bubble();
  }

  UnifiedMessageCenterBubble* GetSecondaryMessageCenterBubble() {
    return GetSecondaryUnifiedSystemTray()->message_center_bubble();
  }

  UnifiedSystemTrayBubble* GetSystemTrayBubble() {
    return GetPrimaryUnifiedSystemTray()->bubble();
  }

  UnifiedSystemTrayBubble* GetSecondarySystemTrayBubble() {
    return GetSecondaryUnifiedSystemTray()->bubble();
  }

  int MessageCenterSeparationHeight() {
    gfx::Rect message_bubble_bounds =
        GetMessageCenterBubble()->GetBubbleView()->GetBoundsInScreen();
    gfx::Rect tray_bounds =
        GetSystemTrayBubble()->GetBubbleView()->GetBoundsInScreen();

    return message_bubble_bounds.y() + message_bubble_bounds.height() -
           tray_bounds.y();
  }

  bool IsMessageCenterCollapsed() {
    return GetMessageCenterBubble()->notification_center_view()->collapsed();
  }

  bool IsQuickSettingsCollapsed() {
    return !GetSystemTrayBubble()
                ->unified_system_tray_controller()
                ->IsExpanded();
  }

  // Helper functions for focus cycle testing.
  void DoTab() { PressAndReleaseKey(ui::KeyboardCode::VKEY_TAB, ui::EF_NONE); }

  void DoShiftTab() {
    PressAndReleaseKey(ui::KeyboardCode::VKEY_TAB, ui::EF_SHIFT_DOWN);
  }

  void DoAltShiftN() {
    PressAndReleaseKey(ui::KeyboardCode::VKEY_N,
                       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN);
  }

  void DoEsc() { PressAndReleaseKey(ui::KeyboardCode::VKEY_ESCAPE); }

  void ToggleExpanded() {
    GetSystemTrayBubble()->unified_system_tray_controller()->ToggleExpanded();
  }

  void WaitForAnimation() {
    // Some animations do not complete without checking is_animating();
    do {
      base::RunLoop().RunUntilIdle();
    } while (
        GetSystemTrayBubble() &&
        GetSystemTrayBubble()->unified_system_tray_controller() &&
        GetSystemTrayBubble()->unified_system_tray_controller()->animation_ &&
        GetSystemTrayBubble()
            ->unified_system_tray_controller()
            ->animation_->is_animating());
  }

  views::View* GetFirstMessageCenterFocusable() {
    return GetMessageCenterBubble()
        ->notification_center_view()
        ->GetFirstFocusableChild();
  }

  views::View* GetLastMessageCenterFocusable() {
    return GetMessageCenterBubble()
        ->notification_center_view()
        ->GetLastFocusableChild();
  }

  views::View* GetFirstQuickSettingsFocusable() {
    return GetSystemTrayBubble()->unified_view()->GetFirstFocusableChild();
  }

  views::View* GetLastQuickSettingsFocusable() {
    return GetSystemTrayBubble()->unified_view()->GetLastFocusableChild();
  }

 private:
  int id_ = 0;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(UnifiedMessageCenterBubbleTest, PositionedAboveSystemTray) {
  const int total_notifications = 5;
  GetPrimaryUnifiedSystemTray()->ShowBubble();
  AddNotification();

  const int reference_separation = MessageCenterSeparationHeight();

  // The message center should be positioned a constant distance above
  // the tray as it grows in size.
  for (int i = 0; i < total_notifications; i++) {
    AddNotification();
    EXPECT_EQ(reference_separation, MessageCenterSeparationHeight());
  }

  // When the system tray is collapsing, message view should stay at a constant
  // height above it.
  for (double i = 1.0; i >= 0; i -= 0.1) {
    GetSystemTrayBubble()->unified_view()->SetExpandedAmount(i);
    EXPECT_EQ(reference_separation, MessageCenterSeparationHeight());
  }

  // When the system tray is collapsing, message view should stay at a constant
  // height above it.
  for (double i = 0.0; i <= 1.0; i += 0.1) {
    GetSystemTrayBubble()->unified_view()->SetExpandedAmount(i);
    EXPECT_EQ(reference_separation, MessageCenterSeparationHeight());
  }
}

TEST_F(UnifiedMessageCenterBubbleTest, FocusCycle) {
  GetPrimaryUnifiedSystemTray()->ShowBubble();
  AddNotification();
  AddNotification();

  views::Widget* quick_settings_widget =
      GetSystemTrayBubble()->GetBubbleWidget();
  views::Widget* message_center_widget =
      GetMessageCenterBubble()->GetBubbleWidget();

  // First tab should focus the first element in the quick settings bubble.
  DoTab();
  EXPECT_TRUE(quick_settings_widget->IsActive());
  EXPECT_FALSE(message_center_widget->IsActive());
  EXPECT_EQ(quick_settings_widget->GetFocusManager()->GetFocusedView(),
            GetFirstQuickSettingsFocusable());

  // Keep tabbing until we reach the last focusable element in the quick
  // settings bubble.
  while (quick_settings_widget->GetFocusManager()->GetFocusedView() !=
         GetLastQuickSettingsFocusable()) {
    DoTab();
  }

  // Tab at the last element in the quick settings bubble should move focus to
  // the first element in the message center.
  DoTab();
  EXPECT_TRUE(message_center_widget->IsActive());
  EXPECT_FALSE(quick_settings_widget->IsActive());
  EXPECT_EQ(message_center_widget->GetFocusManager()->GetFocusedView(),
            GetFirstMessageCenterFocusable());

  // Keep tabbing until we reach the last focusable element in the message
  // center bubble.
  while (message_center_widget->GetFocusManager()->GetFocusedView() !=
         GetLastMessageCenterFocusable()) {
    DoTab();
  }

  // Tab at the last element in the message center bubble should move focus to
  // the first element in the quick settings bubble.
  DoTab();
  EXPECT_TRUE(quick_settings_widget->IsActive());
  EXPECT_FALSE(message_center_widget->IsActive());
  EXPECT_EQ(quick_settings_widget->GetFocusManager()->GetFocusedView(),
            GetFirstQuickSettingsFocusable());
}

TEST_F(UnifiedMessageCenterBubbleTest, CollapseState) {
  AddNotification();
  AddNotification();

  GetPrimaryUnifiedSystemTray()->ShowBubble();
  int small_display_height =
      GetSystemTrayBubble()->unified_view()->GetCollapsedSystemTrayHeight() +
      (2 * kMessageCenterCollapseThreshold);
  int large_display_height =
      GetSystemTrayBubble()->unified_view()->GetExpandedSystemTrayHeight() +
      (4 * kMessageCenterCollapseThreshold);
  GetPrimaryUnifiedSystemTray()->CloseBubble();

  // Clear pref to test behavior when expanded pref is not set.
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  prefs->ClearPref(prefs::kSystemTrayExpanded);

  // Message center should open in expanded state when screen height is
  // limited.
  UpdateDisplay(base::StringPrintf("1000x%d", small_display_height));
  GetPrimaryUnifiedSystemTray()->ShowBubble();
  WaitForAnimation();
  EXPECT_TRUE(IsQuickSettingsCollapsed());
  EXPECT_FALSE(IsMessageCenterCollapsed());

  // Message center should be collapsed when quick settings is expanded
  // with limited screen height.
  ToggleExpanded();
  WaitForAnimation();
  EXPECT_TRUE(IsMessageCenterCollapsed());

  ToggleExpanded();
  WaitForAnimation();
  EXPECT_FALSE(IsMessageCenterCollapsed());

  GetPrimaryUnifiedSystemTray()->CloseBubble();

  UpdateDisplay(base::StringPrintf("1000x%d", large_display_height));
  GetPrimaryUnifiedSystemTray()->ShowBubble();
  EXPECT_FALSE(IsMessageCenterCollapsed());

  ToggleExpanded();
  WaitForAnimation();
  EXPECT_FALSE(IsMessageCenterCollapsed());

  ToggleExpanded();
  WaitForAnimation();
  EXPECT_FALSE(IsMessageCenterCollapsed());
}

TEST_F(UnifiedMessageCenterBubbleTest, FocusCycleWithNoNotifications) {
  GetPrimaryUnifiedSystemTray()->ShowBubble();

  views::Widget* quick_settings_widget =
      GetSystemTrayBubble()->GetBubbleWidget();
  views::Widget* message_center_widget =
      GetMessageCenterBubble()->GetBubbleWidget();

  // First tab should focus the first element in the quick settings bubble.
  DoTab();
  EXPECT_TRUE(quick_settings_widget->IsActive());
  EXPECT_FALSE(message_center_widget->IsActive());
  EXPECT_EQ(quick_settings_widget->GetFocusManager()->GetFocusedView(),
            GetFirstQuickSettingsFocusable());

  // Keep tabbing until we reach the last focusable element in the quick
  // settings bubble.
  while (quick_settings_widget->GetFocusManager()->GetFocusedView() !=
         GetLastQuickSettingsFocusable()) {
    DoTab();
  }

  // Tab at the last element in the quick settings bubble should move focus to
  // the first element in the quick settings bubble.
  DoTab();
  EXPECT_TRUE(quick_settings_widget->IsActive());
  EXPECT_FALSE(message_center_widget->IsActive());
  EXPECT_EQ(quick_settings_widget->GetFocusManager()->GetFocusedView(),
            GetFirstQuickSettingsFocusable());
}

TEST_F(UnifiedMessageCenterBubbleTest, BubbleBounds) {
  std::vector<std::string> displays = {"0+0-1200x800", "0+0-1280x1080",
                                       "0+0-1600x1440"};

  for (auto display : displays) {
    // Set display size where the message center is not collapsed.
    UpdateDisplay(display);

    // Ensure message center is not collapsed.
    GetPrimaryUnifiedSystemTray()->ShowBubble();
    ASSERT_FALSE(GetMessageCenterBubble()->IsMessageCenterCollapsed());

    // Add enough notifications so that the scroll bar is visible.
    while (!GetMessageCenterBubble()
                ->notification_center_view()
                ->IsScrollBarVisible())
      AddNotification();

    // The message center bubble should be positioned above the system tray
    // bubble.
    GetPrimaryUnifiedSystemTray()->ShowBubble();
    EXPECT_LT(GetMessageCenterBubble()->GetBoundsInScreen().bottom(),
              GetSystemTrayBubble()->GetBoundsInScreen().y());
    GetPrimaryUnifiedSystemTray()->CloseBubble();

    // Go into overview mode, check bounds again.
    EnterOverview();
    GetPrimaryUnifiedSystemTray()->ShowBubble();
    EXPECT_LT(GetMessageCenterBubble()->GetBoundsInScreen().bottom(),
              GetSystemTrayBubble()->GetBoundsInScreen().y());
    GetPrimaryUnifiedSystemTray()->CloseBubble();
    ExitOverview();

    // Go into tablet mode, check bounds again.
    Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
    GetPrimaryUnifiedSystemTray()->ShowBubble();
    EXPECT_LT(GetMessageCenterBubble()->GetBoundsInScreen().bottom(),
              GetSystemTrayBubble()->GetBoundsInScreen().y());
    GetPrimaryUnifiedSystemTray()->CloseBubble();

    // Go into overview mode inside tablet mode, check bounds again.
    EnterOverview();
    GetPrimaryUnifiedSystemTray()->ShowBubble();
    EXPECT_LT(GetMessageCenterBubble()->GetBoundsInScreen().bottom(),
              GetSystemTrayBubble()->GetBoundsInScreen().y());
    GetPrimaryUnifiedSystemTray()->CloseBubble();
  }
}

TEST_F(UnifiedMessageCenterBubbleTest, HandleAccelerators) {
  auto id = AddWebNotification();
  WaitForAnimation();

  // Open and focus message center.
  DoAltShiftN();
  WaitForAnimation();
  EXPECT_TRUE(GetMessageCenterBubble()->IsMessageCenterVisible());
  EXPECT_EQ(
      1u,
      message_center::MessageCenter::Get()->GetVisibleNotifications().size());

  views::Widget* quick_settings_widget =
      GetSystemTrayBubble()->GetBubbleWidget();
  views::Widget* message_center_widget =
      GetMessageCenterBubble()->GetBubbleWidget();
  EXPECT_FALSE(quick_settings_widget->IsActive());
  EXPECT_TRUE(message_center_widget->GetFocusManager()->GetFocusedView());

  RemoveAllNotifications();
  WaitForAnimation();
  EXPECT_EQ(
      0u,
      message_center::MessageCenter::Get()->GetVisibleNotifications().size());
  EXPECT_FALSE(quick_settings_widget->IsActive());
  EXPECT_FALSE(message_center_widget->GetFocusManager()->GetFocusedView());

  EXPECT_EQ(nullptr, GetFirstMessageCenterFocusable());
  EXPECT_EQ(nullptr,
            message_center_widget->GetFocusManager()->GetFocusedView());

  // Press Esc to close system tray.
  DoEsc();
  WaitForAnimation();
  EXPECT_EQ(nullptr,
            GetPrimaryUnifiedSystemTray()->GetFocusManager()->GetFocusedView());
}

class UnifiedMessageCenterBubbleMultiDisplayTest
    : public UnifiedMessageCenterBubbleTest,
      public testing::WithParamInterface<
          std::tuple</* Primary display height */ int,
                     /* Secondary display height */ int>> {
 public:
  UnifiedMessageCenterBubbleMultiDisplayTest() = default;
  UnifiedMessageCenterBubbleMultiDisplayTest(
      const UnifiedMessageCenterBubbleMultiDisplayTest&) = delete;
  UnifiedMessageCenterBubbleMultiDisplayTest& operator=(
      const UnifiedMessageCenterBubbleMultiDisplayTest&) = delete;
  ~UnifiedMessageCenterBubbleMultiDisplayTest() override = default;

 protected:
  int GetPrimaryDisplayHeight() { return std::get<0>(GetParam()); }
  int GetSecondaryDisplayHeight() { return std::get<1>(GetParam()); }
};

INSTANTIATE_TEST_SUITE_P(DisplayHeight,
                         UnifiedMessageCenterBubbleMultiDisplayTest,
                         testing::Values(
                             // Short primary display, tall secondary display
                             std::make_tuple(600, 1600),
                             // Tall primary display, short secondary display
                             std::make_tuple(1600, 600),
                             // Same primary and secondary display heights
                             std::make_tuple(600, 600)));

// Tests that the bounds of `UnifiedMessageCenterBubble` are constrained
// according to the dimensions of the display it is being shown on.
TEST_P(UnifiedMessageCenterBubbleMultiDisplayTest, BubbleBounds) {
  UpdateDisplay("800x" + base::NumberToString(GetPrimaryDisplayHeight()) +
                ",800x" + base::NumberToString(GetSecondaryDisplayHeight()));

  // Add a large number of notifications to overflow the scroll view in the
  // `UnifiedMessageCenterBubble`.
  for (int i = 0; i < 100; i++) {
    AddNotification();
  }

  // Show the primary display's `UnifiedMessageCenterBubble`.
  GetPrimaryUnifiedSystemTray()->ShowBubble();

  // The height of the primary display's `UnifiedMessageCenterBubble` should
  // not exceed the primary display's height.
  const int bubble1_height =
      GetMessageCenterBubble()->GetBoundsInScreen().height();
  EXPECT_LT(bubble1_height, GetPrimaryDisplayHeight());

  // The primary display's `UnifiedMessageCenterBubble` should be positioned
  // above the primary display's system tray bubble.
  EXPECT_LT(GetMessageCenterBubble()->GetBoundsInScreen().bottom(),
            GetSystemTrayBubble()->GetBoundsInScreen().y());

  // Show the secondary display's `UnifiedMessageCenterBubble`.
  GetSecondaryUnifiedSystemTray()->ShowBubble();

  // The height of the secondary display's `UnifiedMessageCenterBubble` should
  // not exceed the secondary display's height.
  const int bubble2_height =
      GetSecondaryMessageCenterBubble()->GetBoundsInScreen().height();
  EXPECT_LT(bubble2_height, GetSecondaryDisplayHeight());

  // The secondary display's `UnifiedMessageCenterBubble` should be positioned
  // above the secondary display's system tray bubble.
  EXPECT_LT(GetSecondaryMessageCenterBubble()->GetBoundsInScreen().bottom(),
            GetSecondarySystemTrayBubble()->GetBoundsInScreen().y());
}

}  // namespace ash
