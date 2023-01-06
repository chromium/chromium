// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy/privacy_indicators_controller.h"

#include <string>

#include "ash/constants/ash_constants.h"
#include "ash/system/message_center/ash_message_popup_collection.h"
#include "ash/system/message_center/unified_message_center_bubble.h"
#include "ash/system/notification_center/notification_center_view.h"
#include "ash/system/notification_center/notification_list_view.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "base/functional/bind.h"
#include "ui/events/test/event_generator.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/views/notification_view_base.h"
#include "ui/views/widget/widget_utils.h"

namespace ash {

namespace {

class TestDelegate : public PrivacyIndicatorsNotificationDelegate {
 public:
  TestDelegate()
      : PrivacyIndicatorsNotificationDelegate(
            base::BindRepeating(&TestDelegate::LaunchApp,
                                base::Unretained(this)),
            base::BindRepeating(&TestDelegate::LaunchAppSettings,
                                base::Unretained(this))) {}
  TestDelegate(const TestDelegate&) = delete;
  TestDelegate& operator=(const TestDelegate&) = delete;

  void LaunchApp() { launch_app_called_ = true; }
  void LaunchAppSettings() { launch_settings_called_ = true; }

  bool launch_app_called() { return launch_app_called_; }
  bool launch_settings_called() { return launch_settings_called_; }

 private:
  ~TestDelegate() override = default;

  bool launch_app_called_ = false;
  bool launch_settings_called_ = false;
};

}  // namespace

class PrivacyIndicatorsControllerTest : public AshTestBase {
 public:
  PrivacyIndicatorsControllerTest() = default;
  PrivacyIndicatorsControllerTest(const PrivacyIndicatorsControllerTest&) =
      delete;
  PrivacyIndicatorsControllerTest& operator=(
      const PrivacyIndicatorsControllerTest&) = delete;
  ~PrivacyIndicatorsControllerTest() override = default;

  // Get the notification view from message center associated with `id`.
  views::View* GetNotificationViewFromMessageCenter(const std::string& id) {
    return GetPrimaryUnifiedSystemTray()
        ->message_center_bubble()
        ->notification_center_view()
        ->notification_list_view()
        ->GetMessageViewForNotificationId(id);
  }

  // Get the popup notification view associated with `id`.
  views::View* GetPopupNotificationView(const std::string& id) {
    return GetPrimaryUnifiedSystemTray()
        ->GetMessagePopupCollection()
        ->GetMessageViewForNotificationId(id);
  }

  void ClickView(message_center::NotificationViewBase* view, int button_index) {
    auto* action_buttons = view->GetViewByID(
        message_center::NotificationViewBase::kActionButtonsRow);

    auto* button_view = action_buttons->children()[button_index];

    ui::test::EventGenerator generator(GetRootWindow(button_view->GetWidget()));
    gfx::Point cursor_location = button_view->GetBoundsInScreen().CenterPoint();
    generator.MoveMouseTo(cursor_location);
    generator.ClickLeftButton();
  }
};

TEST_F(PrivacyIndicatorsControllerTest, NotificationMetadata) {
  std::string app_id = "test_app_id";
  std::u16string app_name = u"test_app_name";
  std::string notification_id = GetPrivacyIndicatorsNotificationId(app_id);
  scoped_refptr<TestDelegate> delegate = base::MakeRefCounted<TestDelegate>();
  ash::ModifyPrivacyIndicatorsNotification(
      app_id, app_name, /*is_camera_used=*/true, /*is_microphone_used=*/true,
      delegate);

  auto* notification =
      message_center::MessageCenter::Get()->FindNotificationById(
          notification_id);

  // Notification message should contains app name.
  EXPECT_NE(std::string::npos, notification->message().find(app_name));
}

TEST_F(PrivacyIndicatorsControllerTest, NotificationClickButton) {
  std::string app_id = "test_app_id";
  std::string notification_id = GetPrivacyIndicatorsNotificationId(app_id);
  scoped_refptr<TestDelegate> delegate = base::MakeRefCounted<TestDelegate>();
  ash::ModifyPrivacyIndicatorsNotification(
      app_id, u"test_app_name", /*is_camera_used=*/true,
      /*is_microphone_used=*/true, delegate);

  // Privacy indicators notification should not be a popup. It is silently added
  // to the tray.
  EXPECT_FALSE(GetPopupNotificationView(notification_id));
  GetPrimaryUnifiedSystemTray()->ShowBubble();
  auto* notification_view = static_cast<message_center::NotificationViewBase*>(
      GetNotificationViewFromMessageCenter(notification_id));
  EXPECT_TRUE(notification_view);

  // Clicking the first button will trigger launching the app settings.
  EXPECT_FALSE(delegate->launch_settings_called());
  ClickView(notification_view, 0);
  EXPECT_TRUE(delegate->launch_settings_called());
}

}  // namespace ash
