// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/unified_message_center_bubble.h"

#include <memory>

#include "ash/public/cpp/ash_features.h"
#include "ash/shell.h"
#include "ash/system/message_center/unified_message_center_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/system/unified/unified_system_tray_view.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "ui/message_center/message_center.h"

using message_center::MessageCenter;
using message_center::Notification;

#include <iostream>

namespace ash {

class UnifiedMessageCenterBubbleTest : public AshTestBase {
 public:
  UnifiedMessageCenterBubbleTest() = default;
  ~UnifiedMessageCenterBubbleTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
  }

 protected:
  std::string AddNotification() {
    std::string id = base::NumberToString(id_++);
    MessageCenter::Get()->AddNotification(std::make_unique<Notification>(
        message_center::NOTIFICATION_TYPE_BASE_FORMAT, id,
        base::UTF8ToUTF16("test title"), base::UTF8ToUTF16("test message"),
        gfx::Image(), base::string16() /* display_source */, GURL(),
        message_center::NotifierId(), message_center::RichNotificationData(),
        new message_center::NotificationDelegate()));
    return id;
  }

  void EnableMessageCenterRefactor() {
    scoped_feature_list_->InitAndEnableFeature(
        features::kUnifiedMessageCenterRefactor);
  }

  UnifiedMessageCenterBubble* GetMessageCenterBubble() {
    return GetPrimaryUnifiedSystemTray()->message_center_bubble();
  }

  UnifiedSystemTrayBubble* GetSystemTrayBubble() {
    return GetPrimaryUnifiedSystemTray()->bubble();
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
    return GetMessageCenterBubble()->message_center_view()->collapsed();
  }

  bool IsQuickSettingsCollapsed() {
    return !GetSystemTrayBubble()->controller_for_test()->IsExpanded();
  }

  // Helper functions for focus cycle testing.
  void DoTab() {
    ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
    generator.PressKey(ui::KeyboardCode::VKEY_TAB, ui::EventFlags::EF_NONE);
  }

  void DoShiftTab() {
    ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
    generator.PressKey(ui::KeyboardCode::VKEY_TAB,
                       ui::EventFlags::EF_SHIFT_DOWN);
  }

  void ToggleExpanded() {
    GetSystemTrayBubble()->controller_for_test()->ToggleExpanded();
  }

  void WaitForAnimation() {
    while (GetSystemTrayBubble()
               ->controller_for_test()
               ->animation_->is_animating())
      base::RunLoop().RunUntilIdle();
  }

  views::View* GetFirstMessageCenterFocusable() {
    return GetMessageCenterBubble()
        ->message_center_view()
        ->GetFirstFocusableChild();
  }

  views::View* GetLastMessageCenterFocusable() {
    return GetMessageCenterBubble()
        ->message_center_view()
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
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(UnifiedMessageCenterBubbleTest);
};

TEST_F(UnifiedMessageCenterBubbleTest, PositionedAboveSystemTray) {
  const int total_notifications = 5;
  EnableMessageCenterRefactor();
  GetPrimaryUnifiedSystemTray()->ShowBubble(true);
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
  EnableMessageCenterRefactor();
  GetPrimaryUnifiedSystemTray()->ShowBubble(true);
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

TEST_F(UnifiedMessageCenterBubbleTest, ReverseFocusCycle) {
  EnableMessageCenterRefactor();
  GetPrimaryUnifiedSystemTray()->ShowBubble(true);
  AddNotification();
  AddNotification();

  views::Widget* quick_settings_widget =
      GetSystemTrayBubble()->GetBubbleWidget();
  views::Widget* message_center_widget =
      GetMessageCenterBubble()->GetBubbleWidget();

  // First shift tab should focus the last element in the quick settings bubble.
  DoShiftTab();
  EXPECT_TRUE(quick_settings_widget->IsActive());
  EXPECT_FALSE(message_center_widget->IsActive());
  EXPECT_EQ(quick_settings_widget->GetFocusManager()->GetFocusedView(),
            GetLastQuickSettingsFocusable());

  // Keep shift tabbing until we reach the first focusable element in the quick
  // settings bubble.
  while (quick_settings_widget->GetFocusManager()->GetFocusedView() !=
         GetFirstQuickSettingsFocusable()) {
    DoShiftTab();
  }

  // Shift tab at the first element in the quick settings bubble should move
  // focus to the last element in the message center.
  DoShiftTab();
  EXPECT_TRUE(message_center_widget->IsActive());
  EXPECT_FALSE(quick_settings_widget->IsActive());
  EXPECT_EQ(message_center_widget->GetFocusManager()->GetFocusedView(),
            GetLastMessageCenterFocusable());

  // Keep shift tabbing until we reach the first focusable element in the
  // message center bubble.
  while (message_center_widget->GetFocusManager()->GetFocusedView() !=
         GetFirstMessageCenterFocusable()) {
    DoShiftTab();
  }

  // Shift tab at the first element in the message center bubble should move
  // focus to the last element in the quick settings bubble.
  DoShiftTab();
  EXPECT_TRUE(quick_settings_widget->IsActive());
  EXPECT_FALSE(message_center_widget->IsActive());
  EXPECT_EQ(quick_settings_widget->GetFocusManager()->GetFocusedView(),
            GetLastQuickSettingsFocusable());
}

TEST_F(UnifiedMessageCenterBubbleTest, CollapseState) {
  EnableMessageCenterRefactor();
  AddNotification();

  GetPrimaryUnifiedSystemTray()->ShowBubble(true);
  int small_display_height =
      GetSystemTrayBubble()->unified_view()->GetCollapsedSystemTrayHeight() +
      (2 * kMessageCenterCollapseThreshold);
  int large_display_height =
      GetSystemTrayBubble()->unified_view()->GetExpandedSystemTrayHeight() +
      (4 * kMessageCenterCollapseThreshold);
  GetPrimaryUnifiedSystemTray()->CloseBubble();

  // Message center should open in expanded state when screen height is
  // limited.
  UpdateDisplay(base::StringPrintf("1000x%d", small_display_height));
  GetPrimaryUnifiedSystemTray()->ShowBubble(true);
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
  GetPrimaryUnifiedSystemTray()->ShowBubble(true);
  EXPECT_FALSE(IsMessageCenterCollapsed());

  ToggleExpanded();
  WaitForAnimation();
  EXPECT_FALSE(IsMessageCenterCollapsed());

  ToggleExpanded();
  WaitForAnimation();
  EXPECT_FALSE(IsMessageCenterCollapsed());
}

TEST_F(UnifiedMessageCenterBubbleTest, FocusCycleWithNoNotifications) {
  EnableMessageCenterRefactor();
  GetPrimaryUnifiedSystemTray()->ShowBubble(true);

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

}  // namespace ash
